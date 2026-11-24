/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
/* Validated 2410mlnx54 layout/command contracts: mlx5/{mlx5.h,verbs.c,srq.c}
 * and libibverbs/cmd_srq.c. No CPU access to the NPU WQE or DBR addresses. */
#include <stdlib.h>
#include <mlx5.h>
#include "adp_mlx_internal.h"

struct mlx_srq_nda {
    struct mlx5_srq mlx; /* Full stock layout, also used by standard QP/DV. */
    bool registered;     /* A failed create may own hardware but no table slot. */
};

static void release_srq(struct mlx_srq_nda *object, struct mlx_resource *r, bool events_initialized)
{
    struct mlx5_srq *s = &object->mlx;
    if (s->db)
        mlx_free(r->parent, r, s->db, MLX5DV_RES_TYPE_DBR);
    if (s->buf.buf)
        mlx_free(r->parent, r, s->buf.buf, MLX5DV_RES_TYPE_SRQ);
    if (events_initialized) {
        pthread_cond_destroy(&s->vsrq.srq.cond);
        pthread_mutex_destroy(&s->vsrq.srq.mutex);
    }
    mlx5_spinlock_destroy(&s->lock);
    free(s->free_wqe_bitmap);
    free(s->wrid);
    free(object);
}

struct ibv_srq *mlx_create_srq_nda(struct ibv_pd *pd, struct ibv_srq_init_attr *attr)
{
    /* mlx_create_srq checked public inputs and created this parent for its
     * MLX_SRQ resource. The private layout follows the 2410 header contract. */
    struct mlx5_parent_domain *parent = to_mparent_domain(pd);
    struct mlx_resource *r = parent->pd_context;
    struct mlx5_context *ctx = to_mctx(pd->context);
    /* Existing libmlx5/Host page granule, matching the validated 4 KiB
     * NPU platform. V1 callbacks do not expose an NPU page-size query. */
    int page = to_mdev(pd->context->device)->page_size;
    if (page < (int)ADP_MLX_SRQ_STRIDE || (page & (page - 1))) {
        errno = EINVAL;
        return NULL;
    }
    /* libmlx5 checks presence, not the value, of MLX5_SRQ_SIGNATURE. */
    if (getenv("MLX5_SRQ_SIGNATURE")) {
        errno = EOPNOTSUPP;
        return NULL;
    }
    unsigned int entries = 1;
    while (entries < attr->attr.max_wr + 1)
        entries <<= 1;
    if (ctx->max_srq_recv_wr <= 0 || ctx->max_rq_desc_sz < (int)sizeof(struct mlx5_wqe_data_seg) ||
        attr->attr.max_sge > (unsigned int)ctx->max_rq_desc_sz / sizeof(struct mlx5_wqe_data_seg) ||
        ctx->max_recv_wr < (int)ADP_MLX_SRQ_STRIDE) {
        errno = EINVAL;
        return NULL;
    }
    /* Match mlx5_create_srq / mlx5_alloc_srq_buf in both pinned versions:
     * one next segment followed by the requested data segments, rounded to a
     * power-of-two WQE of at least 32 bytes. max_recv_wr is the stock stride
     * bound here (despite its name). Check before any NPU allocation. */
    uint64_t needed = sizeof(struct mlx5_wqe_srq_next_seg) +
                      (uint64_t)attr->attr.max_sge * sizeof(struct mlx5_wqe_data_seg);
    uint32_t stride = ADP_MLX_SRQ_STRIDE;
    unsigned int shift = 5;
    while (stride < needed) {
        stride <<= 1; /* The descriptor bound above limits needed to <= 2^31. */
        ++shift;
    }
    if (stride > (unsigned int)ctx->max_recv_wr) {
        errno = EINVAL;
        return NULL;
    }
    unsigned int max_sge = (stride - sizeof(struct mlx5_wqe_srq_next_seg)) / sizeof(struct mlx5_wqe_data_seg);
    /* mlx5_ib derives the UMEM length from max_wr/max_sge, not buf.length.
     * Grow the actual queue depth, keeping the requested WQE format, so
     * entries * stride is page aligned even for a small SRQ. Both values
     * are powers of two; once the queue covers one page it is aligned.
     * All added entries are real linked WQEs and are reported to the caller. */
    unsigned int min_entries = stride < (unsigned int)page ? (unsigned int)page / stride : 1;
    if (entries < min_entries)
        entries = min_entries;
    /* Check the final depth once, after page-size expansion. */
    if (entries - 1 > UINT16_MAX || entries - 1 > (unsigned int)ctx->max_srq_recv_wr) {
        errno = EINVAL;
        return NULL;
    }
    if (entries > SIZE_MAX / stride) {
        errno = EOVERFLOW;
        return NULL;
    }
    size_t bytes = (size_t)entries * stride;
    struct mlx_srq_nda *object = calloc(1, sizeof(*object));
    if (!object)
        return NULL;
    struct mlx5_srq *s = &object->mlx;
    int error = mlx5_spinlock_init(&s->lock, 1);
    if (error) {
        free(object);
        errno = error;
        return NULL;
    }
    bool events_initialized = false;
    /* Reserve a bucket before hardware creation: no fallible allocation is
     * needed when the kernel returns an SRQN in a previously empty bucket. */
    struct mlx5_srq **bucket = calloc(MLX5_SRQ_TABLE_MASK + 1, sizeof(*bucket));
    s->wrid = calloc(entries, sizeof(*s->wrid));
    if (!bucket || !s->wrid)
        goto fail;
    s->vsrq.srq_type = IBV_SRQT_BASIC;
    s->vsrq.srq.context = pd->context;
    s->vsrq.srq.pd = pd;
    s->vsrq.srq.srq_context = attr->srq_context;
    /* calloc provided events_completed, head, counter, signature/TM fields. */
    s->max = entries;
    s->max_gs = max_sge;
    s->wqe_shift = shift;
    s->tail = entries - 1;
    /* Host metadata added by 2410; never write the NPU WQE through this map. */
    s->nwqes = entries;
    s->free_wqe_bitmap = bitmap_alloc0(entries);
    if (!s->free_wqe_bitmap)
        goto fail;
    s->waitq_head = s->waitq_tail = -1; /* NPU owns recycling; no ODP waitq. */
    r->srq_entries = entries;
    r->srq_stride = stride;
    /* DV queue export requires WQE alignment as well as device-page alignment. */
    size_t alignment = stride > (unsigned int)page ? stride : (unsigned int)page;
    s->buf = (struct mlx5_buf){.length = bytes,
                               .type = MLX5_ALLOC_TYPE_CUSTOM,
                               .resource_type = MLX5DV_RES_TYPE_SRQ,
                               .req_alignment = alignment,
                               .mparent_domain = parent};
    s->buf.buf = mlx_alloc(pd, r, bytes, alignment, MLX5DV_RES_TYPE_SRQ);
    if (!s->buf.buf)
        goto fail;
    s->db = mlx_alloc(pd, r, page, page, MLX5DV_RES_TYPE_DBR);
    if (!s->db)
        goto fail;
    s->custom_db = true;

    struct mlx5_create_srq cmd = {0};
    struct mlx5_create_srq_resp response = {0};
    cmd.buf_addr = (uintptr_t)s->buf.buf;
    cmd.db_addr = (uintptr_t)s->db;
    struct ibv_srq_init_attr actual = *attr;
    actual.attr.max_wr = entries - 1;
    actual.attr.max_sge = max_sge; /* Kernel and Host must describe the same WQE. */
    pthread_mutex_lock(&ctx->srq_table_mutex);
    /* This command helper initializes the public mutex/cond even on a kernel
     * error. Do not repeat the public ibv_create_srq wrapper's initialization. */
    error = ibv_cmd_create_srq(pd, &s->vsrq.srq, &actual, &cmd.ibv_cmd, sizeof(cmd), &response.ibv_resp,
                               sizeof(response));
    events_initialized = true;
    if (error) {
        pthread_mutex_unlock(&ctx->srq_table_mutex);
        errno = error;
        goto fail;
    }
    s->srqn = response.srqn;
    s->rsc.rsn = response.srqn;
    s->rsc.type = MLX5_RSC_TYPE_SRQ;
    atomic_fetch_add(&parent->mpd.refcount, 1);
    /* From here on the outer rollback owner MUST retain this pointer even if
     * this function returns NULL. Hardware destruction can itself fail. */
    r->out.srq.srq = &s->vsrq.srq;
    unsigned int index = s->srqn >> MLX5_SRQ_TABLE_SHIFT;
    unsigned int slot = s->srqn & MLX5_SRQ_TABLE_MASK;
    if (actual.attr.max_wr != entries - 1 || actual.attr.max_sge != max_sge || index >= MLX5_SRQ_TABLE_SIZE) {
        error = EPROTO;
    } else if (ctx->srq_table[index].refcnt && (!ctx->srq_table[index].table || ctx->srq_table[index].table[slot])) {
        /* A just-destroyed SRQN can be reused before its old table removal.
         * Never overwrite another object's slot; roll back and allow retry. */
        error = EBUSY;
    } else {
        if (!ctx->srq_table[index].refcnt) {
            ctx->srq_table[index].table = bucket;
            bucket = NULL;
        }
        ++ctx->srq_table[index].refcnt;
        ctx->srq_table[index].table[slot] = s;
        object->registered = true;
    }
    pthread_mutex_unlock(&ctx->srq_table_mutex);
    free(bucket);
    if (error) {
        errno = error;
        return NULL;
    }
    attr->attr = actual.attr;
    return &s->vsrq.srq;

fail:
    error = errno ? errno : ENOMEM;
    free(bucket);
    release_srq(object, r, events_initialized);
    errno = error;
    return NULL;
}

int mlx_destroy_srq_nda(struct ibv_srq *srq)
{
    struct mlx5_srq *s = to_msrq(srq);
    struct mlx_srq_nda *object = container_of(s, struct mlx_srq_nda, mlx);
    struct mlx5_context *ctx = to_mctx(srq->context);
    struct mlx5_parent_domain *parent = to_mparent_domain(srq->pd);
    struct mlx_resource *r = parent->pd_context;
    /* No locks held while the command waits for outstanding async events.
     * On failure the hardware, table entry and callback code stay alive. */
    int error = ibv_cmd_destroy_srq(srq);
    if (error)
        return mlx_error(error);
    if (object->registered) {
        unsigned int index = s->srqn >> MLX5_SRQ_TABLE_SHIFT;
        unsigned int slot = s->srqn & MLX5_SRQ_TABLE_MASK;
        pthread_mutex_lock(&ctx->srq_table_mutex);
        /* A native libmlx5 SRQ creator may reuse this SRQN after the kernel
         * destroy but before event acknowledgement finishes. Its store adds
         * another bucket reference. Drop ours without erasing its new slot. */
        if (ctx->srq_table[index].table[slot] == s)
            ctx->srq_table[index].table[slot] = NULL;
        if (!--ctx->srq_table[index].refcnt) {
            free(ctx->srq_table[index].table);
            ctx->srq_table[index].table = NULL;
        }
        pthread_mutex_unlock(&ctx->srq_table_mutex);
    }
    release_srq(object, r, true);
    atomic_fetch_sub(&parent->mpd.refcount, 1);
    return 0;
}
