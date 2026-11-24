/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "adp_mlx_internal.h"

static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t reap_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mlx_resource *resources;
static int reap_resources(struct ibv_context *context, const struct mlx_resource *dependency);

static bool zero_words(const uint64_t *words, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        if (words[i])
            return false;
    return true;
}

static bool power2(uint32_t value)
{
    return value && !(value & (value - 1));
}

static bool valid_ops(const struct ibv_extend_ops *ops, bool qp)
{
    return ops && ops->alloc && ops->free && ops->memcpy_s && (!qp || (ops->db_mmap && ops->db_unmap));
}

static void add_resource(struct mlx_resource *r)
{
    pthread_mutex_lock(&registry_lock);
    r->next = resources;
    resources = r;
    pthread_mutex_unlock(&registry_lock);
}

static struct mlx_resource *new_resource(struct ibv_context *context, struct ibv_extend_ops *ops, enum mlx_kind kind)
{
    const struct mlx_backend *b = &mlx_real_backend;
    if (!b->supported(context)) {
        errno = EOPNOTSUPP;
        return NULL;
    }
    struct mlx_resource *r = calloc(1, sizeof(*r));
    if (r) {
        r->context = context;
        r->kind = kind;
        r->ops = *ops;
        r->backend = b;
        r->creating = r->busy = true;
        add_resource(r);
    }
    return r;
}

static void publish_resource(struct mlx_resource *r)
{
    pthread_mutex_lock(&registry_lock);
    r->creating = r->busy = false;
    pthread_mutex_unlock(&registry_lock);
}

static void remove_resource(struct mlx_resource *r)
{
    struct mlx_resource **link = &resources;
    while (*link && *link != r)
        link = &(*link)->next;
    if (*link)
        *link = r->next;
}

static struct mlx_resource *find_cq(struct ibv_cq *cq, struct ibv_context *context)
{
    for (struct mlx_resource *r = resources; r; r = r->next)
        if (r->kind == MLX_CQ && r->context == context && !r->creating && !r->closing && r->out.cq.cq == cq)
            return r;
    return NULL;
}

static struct mlx_resource *find_srq(struct ibv_srq *srq, const struct mlx_resource *qp)
{
    for (struct mlx_resource *r = resources; r; r = r->next)
        if (r->kind == MLX_SRQ && r->context == qp->context && r->app_pd == qp->app_pd && !r->creating && !r->closing &&
            r->out.srq.srq == srq)
            return r;
    return NULL;
}

/* All dependencies are pinned together before the first hardware allocation. */
static int pin_queues(struct mlx_resource *qp, const struct ibv_qp_init_attr *attr)
{
    pthread_mutex_lock(&registry_lock);
    struct mlx_resource *s = find_cq(attr->send_cq, qp->context);
    struct mlx_resource *r = find_cq(attr->recv_cq, qp->context);
    struct mlx_resource *shared = attr->srq ? find_srq(attr->srq, qp) : NULL;
    int error = 0;
    if (!s || !r || (attr->srq && !shared))
        error = EINVAL;
    else if (s->refs == UINT32_MAX || r->refs == UINT32_MAX || (shared && shared->refs == UINT32_MAX))
        error = EOVERFLOW;
    else {
        ++s->refs;
        if (s != r)
            ++r->refs;
        if (shared)
            ++shared->refs;
        qp->shared_rq = shared;
        qp->send_cq = s;
        qp->recv_cq = r;
    }
    pthread_mutex_unlock(&registry_lock);
    return error ? mlx_error(error) : 0;
}

static void unpin_queues(struct mlx_resource *r)
{
    pthread_mutex_lock(&registry_lock);
    if (r->send_cq)
        --r->send_cq->refs;
    if (r->recv_cq && r->recv_cq != r->send_cq)
        --r->recv_cq->refs;
    if (r->shared_rq)
        --r->shared_rq->refs;
    r->shared_rq = r->send_cq = r->recv_cq = NULL;
    pthread_mutex_unlock(&registry_lock);
}

static int cleanup(struct mlx_resource *r)
{
    int ret;
    /* The NPU must already be quiescent. Preserve every resource on failure
     * until its hardware owner has been successfully destroyed. */
    if (r->db_mapping) {
        struct doorbell_map_desc desc = r->db_desc;
        ret = r->ops.db_unmap(r->db_mapping, &desc);
        if (ret)
            return mlx_error(EIO);
        r->db_mapping = NULL;
        r->out.qp.sq_info.db_hw_va = (struct iov_addr_desc){0};
    }
    if (r->kind == MLX_QP && r->out.qp.qp) {
        ret = r->backend->destroy_qp(r->out.qp.qp);
        if (ret)
            return mlx_error(ret);
        r->out.qp.qp = NULL;
        memset(&r->out.qp.sq_info, 0, sizeof(r->out.qp.sq_info));
        memset(&r->out.qp.rq_info, 0, sizeof(r->out.qp.rq_info));
        r->out.qp.resv[0] = 0;
    }
    if (r->kind == MLX_QP)
        unpin_queues(r);
    if (r->kind == MLX_SRQ && r->out.srq.srq) {
        ret = r->backend->destroy_srq(r->out.srq.srq);
        if (ret)
            return mlx_error(ret);
        memset(&r->out.srq, 0, sizeof(r->out.srq));
    }
    if (r->kind == MLX_CQ && r->out.cq.cq) {
        ret = r->backend->destroy_cq(r->out.cq.cq);
        if (ret)
            return mlx_error(ret);
        r->out.cq.cq = NULL;
        memset(&r->out.cq.cq_info, 0, sizeof(r->out.cq.cq_info));
        r->out.cq.resv[0] = 0;
    }
    if (r->parent) {
        ret = r->backend->dealloc_pd(r->parent);
        if (ret)
            return mlx_error(ret);
        r->parent = NULL;
    }
    if (r->td) {
        ret = r->backend->dealloc_td(r->td);
        if (ret)
            return mlx_error(ret);
        r->td = NULL;
    }
    if (r->base_pd) {
        ret = r->backend->dealloc_pd(r->base_pd);
        if (ret)
            return mlx_error(ret);
        r->base_pd = NULL;
    }
    mlx_release_memory(r);
    return 0;
}

static void failed_create(struct mlx_resource *r, int error)
{
    int ret = cleanup(r);
    if (ret == EBUSY || ret == EAGAIN || ret == EINTR)
        ret = cleanup(r);
    pthread_mutex_lock(&registry_lock);
    if (ret) {
        r->orphan = r->closing = true;
        r->creating = r->busy = false;
    } else {
        remove_resource(r);
    }
    pthread_mutex_unlock(&registry_lock);
    if (ret) {
        syslog(LOG_ERR, "adp_mlx: rollback pending (%d); will retry on dependency destruction or close", ret);
    } else {
        free(r);
    }
    errno = error ? error : EIO;
}

static int export_queue(struct mlx_resource *r, struct queue_info *q, void *buf, uint32_t count, uint32_t stride,
                        uint64_t type)
{
    if (!power2(count) || !power2(stride) || ((uintptr_t)buf & (stride - 1)) || count > SIZE_MAX / stride ||
        !mlx_contains(r, buf, (size_t)count * stride, type))
        return mlx_error(EPROTO);
    q->qbuf.base = (uintptr_t)buf;
    q->qbuf.entry_cnt = count;
    q->qbuf.entry_size = stride;
    return 0;
}

static bool qp_queue_page_aligned(const void *buf, uint32_t count, uint32_t stride)
{
    return !((uintptr_t)buf % ADP_MLX_NPU_PAGE_SIZE) && !((uint64_t)count * stride % ADP_MLX_NPU_PAGE_SIZE);
}

/* NPU-specific constraints. export_queue() owns the common power-of-two,
 * stride alignment, overflow and allocation-range checks for each queue. */
static int validate_qp_npu_layout(struct mlx_resource *r, const struct mlx5dv_qp *dv)
{
    if (dv->sq.stride != ADP_MLX_WQEBB_SIZE || !qp_queue_page_aligned(dv->sq.buf, dv->sq.wqe_cnt, dv->sq.stride))
        return mlx_error(EPROTO);
    if (dv->rq.wqe_cnt && !qp_queue_page_aligned(dv->rq.buf, dv->rq.wqe_cnt, dv->rq.stride))
        return mlx_error(EPROTO);
    /* The exported DBR must belong to a whole page owned by this resource. */
    if ((uintptr_t)dv->dbrec % ADP_MLX_NPU_PAGE_SIZE ||
        !mlx_contains(r, dv->dbrec, ADP_MLX_NPU_PAGE_SIZE, MLX5DV_RES_TYPE_DBR))
        return mlx_error(EPROTO);
    return 0;
}

static int validate_qp_caps(const struct ibv_qp_init_attr *requested, const struct ibv_qp_init_attr *actual,
                            const struct mlx5dv_qp *dv)
{
    const struct ibv_qp_cap *cap = &actual->cap;
    if (!cap->max_send_wr || cap->max_send_wr > dv->sq.wqe_cnt)
        return mlx_error(EPROTO);
    if (cap->max_inline_data < requested->cap.max_inline_data)
        return mlx_error(EPROTO);
    if (cap->max_recv_wr > dv->rq.wqe_cnt || (actual->srq && dv->rq.wqe_cnt))
        return mlx_error(EPROTO);
    /* A requested private RQ must retain at least the requested capacity. */
    if (!actual->srq && requested->cap.max_recv_wr &&
        (cap->max_recv_wr < requested->cap.max_recv_wr || cap->max_recv_sge < requested->cap.max_recv_sge))
        return mlx_error(EPROTO);
    /* A positive SGE count must fit the RQ stride; this also requires at
     * least one data segment, without a separate minimum-stride check. */
    if (dv->rq.wqe_cnt && (!cap->max_recv_sge || cap->max_recv_sge > dv->rq.stride / sizeof(struct mlx5_wqe_data_seg)))
        return mlx_error(EPROTO);
    return 0;
}

static int map_doorbell(struct mlx_resource *r, const struct mlx5dv_qp *dv)
{
    long page = sysconf(_SC_PAGESIZE);
    uintptr_t reg = (uintptr_t)dv->bf.reg;
    /* bf.size is the slot toggle stride, not the MMIO store width. */
    if (page <= 0 || ((unsigned long)page & ((unsigned long)page - 1)) || !reg || !power2(dv->bf.size) ||
        dv->bf.size < 8 || (reg & 7))
        return mlx_error(EPROTO);
    size_t offset = reg & ((uintptr_t)page - 1);
    size_t span = (size_t)dv->bf.size + 8;
    if (span > SIZE_MAX - offset || offset + span > SIZE_MAX - ((size_t)page - 1))
        return mlx_error(EOVERFLOW);
    size_t length = (offset + span + page - 1) & ~((size_t)page - 1);
    uintptr_t start = reg - offset;
    if (start > UINTPTR_MAX - length)
        return mlx_error(EOVERFLOW);
    r->db_desc = (struct doorbell_map_desc){.hva = start, .size = length, .type = DB_MAP_MODE_HOST_VA};
    /* Give callbacks a copy; they cannot mutate the descriptor used by unmap. */
    struct doorbell_map_desc desc = r->db_desc;
    r->db_mapping = r->ops.db_mmap(&desc);
    if (!r->db_mapping || r->db_mapping == (void *)(uintptr_t)UINTPTR_MAX) {
        r->db_mapping = NULL;
        return mlx_error(EIO);
    }
    uintptr_t mapped = (uintptr_t)r->db_mapping;
    if (mapped > UINTPTR_MAX - length || ((mapped + offset) & 7))
        return mlx_error(EPROTO);
    r->out.qp.sq_info.db_hw_va = (struct iov_addr_desc){.iov_base = (void *)(mapped + offset), .iov_len = span};
    r->out.qp.resv[5] = dv->bf.size;
    return 0;
}

struct ibv_cq_extend *mlx_create_cq(struct ibv_context *context, struct ibv_cq_init_attr_extend *attr)
{
    /* Validate provider-specific inputs here; libibverbs validates comp_mask.
     * The internal CQ constructor uses our own parent domain. */
    if (!context || !attr || attr->attr.cqe <= 0 || attr->attr.cqe >= (1 << 24) || !valid_ops(attr->ops, false) ||
        (attr->attr.parent_domain && attr->attr.parent_domain->context != context)) {
        errno = EINVAL;
        return NULL;
    }
    if (attr->type != QU_BUF_DMA_MODE_DEFAULT || attr->cq_cap_flag || attr->attr.channel ||
        (attr->attr.flags & ~IBV_CREATE_CQ_ATTR_IGNORE_OVERRUN) || (attr->attr.wc_flags & ~IBV_WC_STANDARD_FLAGS) ||
        !zero_words(attr->resv, 8)) {
        errno = EOPNOTSUPP;
        return NULL;
    }
    struct mlx_resource *r = new_resource(context, attr->ops, MLX_CQ);
    if (!r)
        return NULL;
    struct ibv_cq_init_attr_ex ca = attr->attr;
    struct mlx5dv_cq dv = {0};
    struct ibv_pd *base = attr->attr.parent_domain;
    if (!base) {
        r->base_pd = r->backend->alloc_pd(context);
        base = r->base_pd;
    }
    if (!base || !r->backend->alloc_parent(r, base))
        goto fail;
    r->out.cq.cq = r->backend->create_cq(r, &ca);
    if (!r->out.cq.cq)
        goto fail;
    int ret = r->backend->export_cq(r->out.cq.cq, &dv);
    if (ret) {
        mlx_error(ret);
        goto fail;
    }
    if (dv.cqe_size != ADP_MLX_CQE_SIZE || ((uintptr_t)dv.dbrec & 3) ||
        !mlx_contains(r, dv.dbrec, 8, MLX5DV_RES_TYPE_DBR) || r->out.cq.cq->cqe <= 0 ||
        (uint32_t)r->out.cq.cq->cqe >= dv.cqe_cnt) {
        errno = EPROTO;
        goto fail;
    }
    if (export_queue(r, &r->out.cq.cq_info, dv.buf, dv.cqe_cnt, dv.cqe_size, MLX5DV_RES_TYPE_CQ))
        goto fail;
    r->out.cq.cq_info.dbr_ci_va = (struct iov_addr_desc){.iov_base = dv.dbrec, .iov_len = 4};
    r->out.cq.resv[0] = ADP_MLX_META_MAGIC;
    r->out.cq.resv[1] = ADP_MLX_NDA_ABI;
    r->out.cq.resv[3] = dv.cqn;
    r->out.cq.resv[4] = r->out.cq.cq->cqe;
    publish_resource(r);
    return &r->out.cq;
fail:
    failed_create(r, errno);
    return NULL;
}

struct ibv_srq_extend *mlx_create_srq(struct ibv_context *context, struct ibv_srq_init_attr_extend *attr)
{
    if (!context || !attr || !attr->pd || attr->pd->context != context || !attr->attr.attr.max_wr ||
        attr->attr.attr.max_wr > UINT16_MAX || !valid_ops(attr->ops, false)) {
        errno = EINVAL;
        return NULL;
    }
    if (attr->type != QU_BUF_DMA_MODE_DEFAULT || (attr->comp_mask & ~(IBV_SRQ_INIT_ATTR_TYPE | IBV_SRQ_INIT_ATTR_PD)) ||
        attr->srq_cap_flag || attr->attr.attr.srq_limit || !zero_words(attr->resv, 8)) {
        errno = EOPNOTSUPP;
        return NULL;
    }
    struct mlx_resource *r = new_resource(context, attr->ops, MLX_SRQ);
    if (!r)
        return NULL;
    r->app_pd = attr->pd;
    struct ibv_srq_init_attr sa = attr->attr;
    struct mlx5dv_srq dv = {.comp_mask = MLX5DV_SRQ_MASK_SRQN};
    if (!r->backend->alloc_parent(r, r->app_pd))
        goto fail;
    /* A backend may return NULL after creating hardware and publishing a
     * rollback-owned SRQ in r. Preserve it for failed_create/reap. */
    struct ibv_srq *created = r->backend->create_srq(r->parent, &sa);
    if (!created)
        goto fail;
    r->out.srq.srq = created;
    int ret = r->backend->export_srq(r->out.srq.srq, &dv);
    if (ret) {
        mlx_error(ret);
        goto fail;
    }
    if (!(dv.comp_mask & MLX5DV_SRQ_MASK_SRQN) || dv.head || dv.tail > UINT16_MAX || dv.tail != sa.attr.max_wr ||
        dv.tail < attr->attr.attr.max_wr || dv.stride < ADP_MLX_SRQ_STRIDE ||
        sa.attr.max_sge != (dv.stride - sizeof(struct mlx5_wqe_srq_next_seg)) / sizeof(struct mlx5_wqe_data_seg) ||
        ((uintptr_t)dv.dbrec & 3) || !mlx_contains(r, dv.dbrec, sizeof(*dv.dbrec), MLX5DV_RES_TYPE_DBR)) {
        errno = EPROTO;
        goto fail;
    }
    if (export_queue(r, &r->out.srq.srq_info, dv.buf, dv.tail + 1, dv.stride, MLX5DV_RES_TYPE_SRQ))
        goto fail;
    r->out.srq.srq_info.dbr_pi_va = (struct iov_addr_desc){.iov_base = dv.dbrec, .iov_len = sizeof(*dv.dbrec)};
    r->out.srq.resv[0] = ADP_MLX_META_MAGIC;
    r->out.srq.resv[1] = ADP_MLX_NDA_ABI;
    r->out.srq.resv[3] = dv.srqn;
    r->out.srq.resv[4] = dv.head;
    r->out.srq.resv[5] = dv.tail;
    r->out.srq.resv[6] = sa.attr.max_wr;
    attr->attr.attr = sa.attr;
    publish_resource(r);
    return &r->out.srq;
fail:
    failed_create(r, errno);
    return NULL;
}

struct ibv_qp_extend *mlx_create_qp(struct ibv_context *context, struct ibv_qp_init_attr_extend *attr)
{
    /* pin_queues() validates both CQ handles and an optional SRQ together,
     * including NULL, foreign ownership and resources being destroyed. */
    if (!context || !attr || !attr->pd || attr->pd->context != context || !attr->attr.cap.max_send_wr ||
        !attr->attr.cap.max_send_sge || !valid_ops(attr->ops, true)) {
        errno = EINVAL;
        return NULL;
    }
    if (attr->type != QU_BUF_DMA_MODE_DEFAULT || attr->qp_cap_flag || attr->attr.qp_type != IBV_QPT_RC ||
        attr->verdor_priv_info.value || !zero_words(attr->resv, 7)) {
        errno = EOPNOTSUPP;
        return NULL;
    }
    if (!attr->attr.srq && attr->attr.cap.max_recv_wr && !attr->attr.cap.max_recv_sge) {
        errno = EINVAL;
        return NULL;
    }
    struct mlx_resource *r = new_resource(context, attr->ops, MLX_QP);
    if (!r)
        return NULL;
    /* libmlx5 validates SGE/inline capacities and sizes the queues. Inline
     * capacity does not force inline sends; the NPU writer selects each WQE. */
    struct ibv_qp_init_attr qa = attr->attr;
    struct mlx5dv_qp dv = {0};
    r->app_pd = attr->pd;
    if (qa.srq) {
        /* A bound SRQ supplies receive WQEs. No private RQ is created. */
        qa.cap.max_recv_wr = qa.cap.max_recv_sge = 0;
    }
    /* mlx5_ib registers the actual SQ + RQ length, not allocation padding.
     * libmlx5 rounds each queue to a power of two. Every send WR needs at
     * least one 64-byte WQEBB; a conservative 64-WR floor therefore makes
     * the SQ a whole number of 4 KiB pages without duplicating libmlx5's
     * SGE/opcode-dependent send-WQE calculation. With signatures disabled,
     * private RQ WQEs contain max_recv_sge 16-byte data segments, rounded
     * to a power of two. Cap the stride estimate at one page: larger WQEs
     * already span whole pages, and libmlx5 must validate their size.
     * Leave absent/shared RQs absent; never lower the requested SGE count.
     * libmlx5 still validates the enlarged capacities against device limits. */
    const uint32_t sq_page_entries = ADP_MLX_NPU_PAGE_SIZE / ADP_MLX_WQEBB_SIZE;
    uint32_t rq_page_stride = sizeof(struct mlx5_wqe_data_seg);
    while (rq_page_stride < ADP_MLX_NPU_PAGE_SIZE &&
           rq_page_stride / sizeof(struct mlx5_wqe_data_seg) < qa.cap.max_recv_sge)
        rq_page_stride <<= 1;
    const uint32_t rq_page_entries = ADP_MLX_NPU_PAGE_SIZE / rq_page_stride;
    if (qa.cap.max_send_wr < sq_page_entries)
        qa.cap.max_send_wr = sq_page_entries;
    if (qa.cap.max_recv_wr && qa.cap.max_recv_wr < rq_page_entries)
        qa.cap.max_recv_wr = rq_page_entries;
    if (pin_queues(r, &qa))
        goto fail;
    r->td = r->backend->alloc_td(context);
    if (!r->td || !r->backend->alloc_parent(r, attr->pd))
        goto fail;
    r->out.qp.qp = r->backend->create_qp(r, &qa);
    if (!r->out.qp.qp)
        goto fail;
    int ret = r->backend->export_qp(r->out.qp.qp, &dv);
    if (ret) {
        mlx_error(ret);
        goto fail;
    }
    if (validate_qp_npu_layout(r, &dv) || validate_qp_caps(&attr->attr, &qa, &dv))
        goto fail;
    if (export_queue(r, &r->out.qp.sq_info, dv.sq.buf, dv.sq.wqe_cnt, dv.sq.stride, MLX5DV_RES_TYPE_QP))
        goto fail;
    r->out.qp.sq_info.dbr_pi_va = (struct iov_addr_desc){.iov_base = dv.dbrec + 1, .iov_len = 4};
    if (dv.rq.wqe_cnt) {
        if (export_queue(r, &r->out.qp.rq_info, dv.rq.buf, dv.rq.wqe_cnt, dv.rq.stride, MLX5DV_RES_TYPE_QP))
            goto fail;
        r->out.qp.rq_info.dbr_pi_va = (struct iov_addr_desc){.iov_base = dv.dbrec, .iov_len = 4};
    }
    if (map_doorbell(r, &dv))
        goto fail;
    r->out.qp.resv[0] = ADP_MLX_META_MAGIC;
    r->out.qp.resv[1] = ADP_MLX_NDA_ABI;
    r->out.qp.resv[3] = qa.cap.max_send_wr;
    r->out.qp.resv[4] = qa.cap.max_recv_wr;
    attr->attr.cap = qa.cap;
    publish_resource(r);
    return &r->out.qp;
fail:
    failed_create(r, errno);
    return NULL;
}

static int destroy(void *out, enum mlx_kind kind)
{
    pthread_mutex_lock(&registry_lock);
    struct mlx_resource *r;
    for (r = resources; r; r = r->next)
        if (r->kind == kind && (void *)&r->out == out && !r->orphan)
            break;
    int error = !r ? EINVAL : r->busy ? EBUSY : 0;
    if (!error && r->refs) {
        /* Reserve the public object while the lock is dropped. Other QPs
         * may bind or unpin, so refs must be read again under this lock. */
        r->busy = true;
        pthread_mutex_unlock(&registry_lock);
        error = reap_resources(r->context, r);
        pthread_mutex_lock(&registry_lock);
        if (!error && r->refs)
            error = EBUSY;
        r->busy = false;
    }
    if (!error)
        r->busy = r->closing = true;
    pthread_mutex_unlock(&registry_lock);
    if (error)
        return mlx_error(error);
    int ret = cleanup(r);
    pthread_mutex_lock(&registry_lock);
    if (!ret)
        remove_resource(r);
    else
        r->busy = false;
    pthread_mutex_unlock(&registry_lock);
    if (!ret)
        free(r);
    return ret;
}

int mlx_destroy_qp(struct ibv_qp_extend *qp)
{
    return destroy(qp, MLX_QP);
}

int mlx_destroy_cq(struct ibv_cq_extend *cq)
{
    return destroy(cq, MLX_CQ);
}

int mlx_destroy_srq(struct ibv_srq_extend *srq)
{
    return destroy(srq, MLX_SRQ);
}

int mlx_query_device(struct ibv_context *context, struct ibv_device_attr_extend *attr)
{
    if (!context || !attr)
        return mlx_error(EINVAL);
    memset(attr, 0, sizeof(*attr));
    const struct mlx_backend *b = &mlx_real_backend;
    /* Do not turn a device query error into a successful zero-capability reply. */
    int ret = b->query_device(context, attr);
    if (ret)
        memset(attr, 0, sizeof(*attr));
    return ret;
}

/* With a dependency, only recover failed QPs that still own its reference.
 * The registry lock protects both the list and the QP reference pointers. */
static bool matches_orphan(const struct mlx_resource *r, struct ibv_context *context,
                           const struct mlx_resource *dependency)
{
    return r->orphan && r->context == context &&
           (!dependency || r->send_cq == dependency || r->recv_cq == dependency || r->shared_rq == dependency);
}

static int reap_resources(struct ibv_context *context, const struct mlx_resource *dependency)
{
    if (!context)
        return mlx_error(EINVAL);
    /* A concurrent/reentrant reap must not report success while another
     * reaper owns a detached snapshot of still-live hardware objects. */
    if (pthread_mutex_trylock(&reap_lock))
        return mlx_error(EBUSY);
    /* Detach a snapshot. No registry lock is held across driver or user callbacks. */
    struct mlx_resource *todo = NULL;
    int error = 0;
    pthread_mutex_lock(&registry_lock);
    struct mlx_resource **link = &resources;
    while (*link) {
        struct mlx_resource *r = *link;
        if (matches_orphan(r, context, dependency) && !r->busy) {
            *link = r->next;
            r->next = todo;
            todo = r;
        } else {
            link = &r->next;
        }
    }
    pthread_mutex_unlock(&registry_lock);
    while (todo) {
        struct mlx_resource *r = todo;
        todo = r->next;
        int ret = cleanup(r);
        if (ret) {
            if (!error)
                error = ret;
            add_resource(r);
        } else {
            free(r);
        }
    }
    pthread_mutex_lock(&registry_lock);
    for (struct mlx_resource *r = resources; r && !error; r = r->next)
        if (matches_orphan(r, context, dependency))
            error = EBUSY;
    pthread_mutex_unlock(&registry_lock);
    pthread_mutex_unlock(&reap_lock);
    return error ? mlx_error(error) : 0;
}

int adp_mlx_reap_resources(struct ibv_context *context)
{
    return reap_resources(context, NULL);
}

int mlx_close_resources(struct ibv_context *context)
{
    int ret = reap_resources(context, NULL);
    if (ret)
        return ret;
    /* Check while also excluding a detached orphan snapshot. Every wrapper
     * of this base context observes the same ownership set. This is not a
     * license to race ibv_close_device against new operations. */
    if (pthread_mutex_trylock(&reap_lock))
        return mlx_error(EBUSY);
    pthread_mutex_lock(&registry_lock);
    for (struct mlx_resource *r = resources; r; r = r->next)
        if (r->context == context) {
            ret = EBUSY;
            break;
        }
    pthread_mutex_unlock(&registry_lock);
    pthread_mutex_unlock(&reap_lock);
    return ret ? mlx_error(ret) : 0;
}
