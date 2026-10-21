/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <util/compiler.h>
#include <linux/errno.h>
#include "user_log.h"
#include "hns_roce_u_sec.h"
#include "hns_roce_lite_stdio.h"
#include "hns_roce_lite.h"
#include "securec.h"

#ifndef DEFINE_HNS_LLT
#define STATIC static
#else
#define STATIC
#endif

struct rdma_lite_context *hns_roce_lite_alloc_context(u8 phy_id, struct dev_cap_info *cap)
{
    struct hns_roce_lite_context *ctx = NULL;
    int ret;
    int i;

    HNS_ROCE_U_NULL_POINT_RETURN_NULL(cap);

    if (cap->port_num == 0) {
        roce_err("context port_num is 0, invalid.\n");
        return NULL;
    }

    ctx = (struct hns_roce_lite_context *)calloc(1, sizeof(struct hns_roce_lite_context));
    if (ctx == NULL) {
        roce_err("calloc ctx failed");
        return NULL;
    }

    ctx->num_qps = cap->num_qps;
    ctx->qp_table_shift = cap->qp_table_shift;
    ctx->qp_table_mask = cap->qp_table_mask;
    ctx->port_num = cap->port_num;
    ctx->page_size = cap->page_size;
    ctx->max_qp_wr = cap->max_qp_wr;
    ctx->max_sge = cap->max_sge;

    ret = drvDeviceGetIndexByPhyId(phy_id, &ctx->dev_id);
    if (ret) {
        roce_err("hns_roce_lite_alloc_context drvDeviceGetIndexByPhyId get dev_id failed, phy_id %d\n", phy_id);
        goto out;
    }

    pthread_mutex_init(&ctx->qp_table_mutex, NULL);
    pthread_mutex_init(&ctx->mutex, NULL);
    list_head_init(&ctx->db_list);

    for (i = 0; i < HNS_ROCE_LITE_QP_TABLE_BITS; ++i) {
        ctx->qp_table[i].refcnt = 0;
    }

    for (i = 0; i < BITMAP_WORD_NUM; ++i) {
        ctx->mem_bitmap_list[i] = 0;
    }
    for (i = 0; i < BITMAP_WORD_SIZE; ++i) {
        ctx->mem_pool_list[i] = NULL;
    }

    return &ctx->lite_ctx;

out:
    free(ctx);
    ctx = NULL;
    return NULL;
}

void hns_roce_lite_free_context(struct rdma_lite_context *lite_ctx)
{
    struct hns_roce_lite_context *ctx = NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_VOID(lite_ctx);

    ctx = to_hr_lite_ctx(lite_ctx);

    pthread_mutex_destroy(&ctx->mutex);
    pthread_mutex_destroy(&ctx->qp_table_mutex);

    free(ctx);
}

STATIC int bitmap_set(unsigned int *bitmap_list, unsigned int list_size, unsigned int index)
{
    unsigned int word_index = index / BITMAP_WORD_LEN;
    unsigned int bit_index = index % BITMAP_WORD_LEN;

    if (word_index >= list_size) {
        roce_err("set bitmap failed, word_index[%u] >= list_size[%u]", word_index, list_size);
        return -EINVAL;
    }

    if (((bitmap_list[word_index] >> bit_index) & 0x1) != 0) {
        roce_err("set bitmap failed, word_index[%u] bit_index[%u] already allocated", word_index, bit_index);
        return -EIO;
    }

    bitmap_list[word_index] = bitmap_list[word_index] | (0x1 << bit_index);
    return 0;
}

static inline void bitmap_free(unsigned int *bitmap_list, unsigned int list_size, unsigned int index)
{
    unsigned int word_index = index / BITMAP_WORD_LEN;
    unsigned int bit_index = index % BITMAP_WORD_LEN;

    if (word_index >= list_size) {
        roce_err("free bitmap failed, word_index[%u] >= list_size[%u]", word_index, list_size);
        return;
    }

    bitmap_list[word_index] = bitmap_list[word_index] & (~(1 << bit_index));
}

int hns_roce_lite_init_mem_pool(struct rdma_lite_context *lite_ctx, struct rdma_lite_mem_attr *lite_mem_attr)
{
    struct rdma_lite_device_mem_attr *mem_data = NULL;
    struct rdma_lite_mem_pool *mem_pool = NULL;
    struct hns_roce_lite_context *ctx = NULL;
    int ret;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_ctx);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_mem_attr);

    ctx = to_hr_lite_ctx(lite_ctx);
    if (ctx->page_size == PAGE_ALIGN_4KB) {
        roce_err("init param err, ctx->page_size[%u] invalid", ctx->page_size);
        return -EINVAL;
    }

    mem_data = &lite_mem_attr->mem_data;
    ret = bitmap_set(ctx->mem_bitmap_list, BITMAP_WORD_NUM, mem_data->mem_idx);
    if (ret != 0) {
        return ret;
    }

    mem_pool = (struct rdma_lite_mem_pool *)calloc(1, sizeof(struct rdma_lite_mem_pool));
    if (mem_pool == NULL) {
        roce_err("alloc mem_pool failed");
        ret = -ENOMEM;
        goto err_calloc;
    }

    mem_pool->host_buf.dva = mem_data->va;
    mem_pool->host_buf.length = mem_data->mem_size;
    mem_pool->host_buf.hva = hns_roce_lite_mmap_host_va(mem_data->va, mem_data->mem_size, ctx);
    if (mem_pool->host_buf.hva == NULL) {
        roce_err("mmap mem_pool failed");
        ret = -ENOMEM;
        goto err_alloc_buf;
    }

    mem_pool->host_buf.mem_idx = mem_data->mem_idx;
    mem_pool->offset = 0;
    mem_pool->used_size = 0;
    ctx->mem_pool_list[mem_data->mem_idx] = mem_pool;

    return 0;

err_alloc_buf:
    free(mem_pool);
err_calloc:
    bitmap_free(ctx->mem_bitmap_list, BITMAP_WORD_NUM, mem_data->mem_idx);

    return ret;
}

int hns_roce_lite_deinit_mem_pool(struct rdma_lite_context *lite_ctx, u32 mem_idx)
{
    struct rdma_lite_mem_pool *mem_pool = NULL;
    struct hns_roce_lite_context *ctx = NULL;
    int ret;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_ctx);

    ctx = to_hr_lite_ctx(lite_ctx);
    if (ctx->page_size == PAGE_ALIGN_4KB) {
        roce_err("deinit param err, ctx->page_size[%u] invalid", ctx->page_size);
        return -EINVAL;
    }

    if (mem_idx >= BITMAP_WORD_SIZE) {
        roce_err("deinit param err, mem_idx[%u] >= %u", mem_idx, BITMAP_WORD_SIZE);
        return -EINVAL;
    }

    mem_pool = ctx->mem_pool_list[mem_idx];
    if (mem_pool == NULL || mem_pool->used_size > 0) {
        roce_err("deinit param err, mem_idx[%u] mem_pool[%pK] is NULL or used_size > 0", mem_idx, mem_pool);
        return -EINVAL;
    }

    ret = hns_roce_lite_unmmap_host_va(mem_pool->host_buf.dva, ctx);
    if (ret != 0) {
        return ret;
    }

    free(mem_pool);
    mem_pool = NULL;
    ctx->mem_pool_list[mem_idx] = NULL;
    bitmap_free(ctx->mem_bitmap_list, BITMAP_WORD_NUM, mem_idx);

    return 0;
}

STATIC int hns_roce_lite_init_cq(struct hns_roce_lite_cq *cq, struct rdma_lite_context *lite_ctx,
                                 struct rdma_lite_cq_attr *lite_cq_attr)
{
    struct hns_roce_lite_context *ctx = NULL;
    int ret, ret_temp;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_ctx);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(cq);

    cq->depth = lite_cq_attr->device_cq_attr.depth;
    cq->flags = lite_cq_attr->device_cq_attr.flags;

    if ((cq->flags & HNS_ROCE_LITE_CQ_FLAG_RECORD_DB) == 0) {
        roce_err("Lite cq only supports record db! cq_flag 0x%x", cq->flags);
        return -EINVAL;
    }

    cq->cqe_size = lite_cq_attr->device_cq_attr.cqe_size;
    cq->cqn = lite_cq_attr->device_cq_attr.cqn;
    list_head_init(&cq->list_sq);
    list_head_init(&cq->list_rq);

    ctx = to_hr_lite_ctx(lite_ctx);
    cq->cq_buf.mem_idx = lite_cq_attr->mem_idx;
    ret = hns_roce_lite_mmap_hva(&lite_cq_attr->device_cq_attr.cq_buf, &cq->cq_buf, ctx);
    if (ret != 0) {
        roce_err("%s hns_roce_lite_mmap_hva cq->cq_buf.hva failed", __func__);
        return ret;
    }

    cq->swdb_buf.mem_idx = lite_cq_attr->mem_idx;
    ret = hns_roce_lite_mmap_hdb(&lite_cq_attr->device_cq_attr.swdb_buf, &cq->swdb_buf, ctx);
    if (ret != 0) {
        roce_err("%s hns_roce_lite_mmap_hdb cq->swdb_buf.hva failed", __func__);
        goto unregister_cq_buf;
    }

    cq->cons_index = 0;

    ret = pthread_spin_init(&cq->lock, PTHREAD_PROCESS_PRIVATE);
    if (ret) {
        roce_err("hns_roce_lite_init_cq spin lock init failed, ret = %d", ret);
        goto unregister_cq_swdb;
    }

    return 0;

unregister_cq_swdb:
    ret_temp = hns_roce_lite_unmmap_hdb(&cq->swdb_buf, ctx);
    if (ret_temp) {
        roce_err("hns_roce_lite_unmmap_hdb failed, ret = %d", ret_temp);
    }
unregister_cq_buf:
    ret_temp = hns_roce_lite_unmmap_hva(&cq->cq_buf, ctx);
    if (ret_temp) {
        roce_err("hns_roce_lite_unmmap_hva failed, ret = %d", ret_temp);
    }
    return ret;
}

struct rdma_lite_cq *hns_roce_lite_create_cq(struct rdma_lite_context *lite_ctx, struct rdma_lite_cq_attr *lite_cq_attr)
{
    struct hns_roce_lite_cq *cq = NULL;
    int ret;

    HNS_ROCE_U_NULL_POINT_RETURN_NULL(lite_ctx);
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(lite_cq_attr);

    cq = calloc(1, sizeof(struct hns_roce_lite_cq));
    if (cq == NULL) {
        roce_err("hns_roce_lite_init_cq calloc cq failed");
        return NULL;
    }

    ret = hns_roce_lite_init_cq(cq, lite_ctx, lite_cq_attr);
    if (ret) {
        roce_err("hns_roce_lite_init_cq failed, ret [%d], expect 0", ret);
        goto err;
    }

    return &cq->lite_cq;

err:
    free(cq);
    cq = NULL;
    return NULL;
}

STATIC void hns_roce_lite_deinit_cq(struct hns_roce_lite_cq *cq, struct hns_roce_lite_context *ctx)
{
    int ret;

    pthread_spin_destroy(&cq->lock);

    ret = hns_roce_lite_unmmap_hdb(&cq->swdb_buf, ctx);
    if (ret) {
        roce_err("hns_roce_lite_unmmap_hdb failed, ret = %d", ret);
    }

    ret = hns_roce_lite_unmmap_hva(&cq->cq_buf, ctx);
    if (ret) {
        roce_err("hns_roce_lite_unmmap_hva failed, ret = %d", ret);
    }
}

int hns_roce_lite_destroy_cq(struct rdma_lite_cq *lite_cq)
{
    struct hns_roce_lite_context *ctx = NULL;
    struct hns_roce_lite_cq *cq = NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_cq);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_cq->ctx);

    ctx = to_hr_lite_ctx(lite_cq->ctx);
    cq = to_hr_lite_cq(lite_cq);

    hns_roce_lite_deinit_cq(cq, ctx);

    free(cq);
    cq = NULL;
    return 0;
}

STATIC int hns_roce_lite_alloc_qp_wrid_init(struct hns_roce_lite_qp *qp)
{
    HNS_ROCE_U_PARA_CHECK_RETURN_INT(qp->sq.wrid_len);
    HNS_ROCE_U_PARA_CHECK_RETURN_INT(qp->rq.wrid_len);

    qp->sq.wrid = (unsigned long *)calloc(1, qp->sq.wrid_len);
    if (qp->sq.wrid == NULL) {
        roce_err("lite sq wrid calloc failed, wrid_len is %d", qp->sq.wrid_len);
        goto out;
    }

    qp->rq.wrid = (unsigned long *)calloc(1, qp->rq.wrid_len);
    if (qp->rq.wrid == NULL) {
        roce_err("lite rq wrid calloc failed, wrid_len is %d", qp->rq.wrid_len);
        goto calloc_rq_wrid_err;
    }

    return 0;

calloc_rq_wrid_err:
    free(qp->sq.wrid);
    qp->sq.wrid = NULL;
out:
    return -ENOMEM;
}

STATIC int hns_roce_lite_create_qp_init(struct hns_roce_lite_context *context, struct hns_roce_lite_qp *qp,
                                        struct rdma_lite_qp_attr *attr)
{
    int ret;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(context);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(qp);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(attr);

    hns_roce_set_lite_qp_attr(qp, attr);
    hns_roce_init_lite_qp_indices(qp);

    ret = hns_roce_lite_alloc_qp_wrid_init(qp);
    if (ret != 0) {
        roce_err("%s hns_roce_lite_alloc_qp_wrid_init failed, ret %d", __func__, ret);
        return ret;
    }

    qp->buf.mem_idx = attr->mem_idx;
    ret = hns_roce_lite_mmap_hva(&attr->device_qp_attr.qp_buf, &qp->buf, context);
    if (ret != 0) {
        roce_err("%s hns_roce_lite_mmap_hva qp->buf.hva failed", __func__);
        goto alloc_wr_id_err;
    }

    qp->sdb_buf.mem_idx = attr->mem_idx;
    ret = hns_roce_lite_mmap_hdb(&attr->device_qp_attr.sq.db_buf, &qp->sdb_buf, context);
    if (ret != 0) {
        roce_err("%s hns_roce_lite_mmap_hdb qp->sdb_buf.hva failed", __func__);
        goto mmap_sdb_buf_err;
    }

    qp->rdb_buf.mem_idx = attr->mem_idx;
    ret = hns_roce_lite_mmap_hdb(&attr->device_qp_attr.rq.db_buf, &qp->rdb_buf, context);
    if (ret != 0) {
        roce_err("%s hns_roce_lite_mmap_hdb qp->rdb_buf.hva failed", __func__);
        goto mmap_rdb_buf_err;
    }

    ret = hns_roce_lite_qp_lock_init(qp);
    if (ret) {
        roce_err("lite_qp lock init failed!, ret:%d", ret);
        goto init_qp_lock_err;
    }

    qp->lite_qp.qp_num = attr->device_qp_attr.qpn;
    (void)pthread_mutex_lock(&context->qp_table_mutex);
    ret = hns_roce_store_lite_qp(context, qp->lite_qp.qp_num, qp);
    if (ret) {
        roce_err("hns_roce_store_lite_qp failed!, ret [%d], expect 0", ret);
        goto store_lite_qp_err;
    }
    (void)pthread_mutex_unlock(&context->qp_table_mutex);

    return 0;

store_lite_qp_err:
    (void)pthread_mutex_unlock(&context->qp_table_mutex);
    hns_roce_lite_qp_lock_uninit(qp);

init_qp_lock_err:
    ret = hns_roce_lite_unmmap_hdb(&qp->rdb_buf, context);
    if (ret) {
        roce_err("hns_roce_lite_unmmap_hdb failed, ret = %d", ret);
    }
    qp->rdb_buf.hva = NULL;

mmap_rdb_buf_err:
    ret = hns_roce_lite_unmmap_hdb(&qp->sdb_buf, context);
    if (ret) {
        roce_err("hns_roce_lite_unmmap_hdb failed, ret = %d", ret);
    }
    qp->sdb_buf.hva = NULL;

mmap_sdb_buf_err:
    ret = hns_roce_lite_unmmap_hva(&qp->buf, context);
    if (ret) {
        roce_err("hns_roce_lite_unmmap_hva failed, ret = %d", ret);
    }
    qp->buf.hva = NULL;

alloc_wr_id_err:
    free(qp->rq.wrid);
    qp->rq.wrid = NULL;
    free(qp->sq.wrid);
    qp->sq.wrid = NULL;

    return -ENOMEM;
}

struct rdma_lite_qp *hns_roce_lite_create_qp(struct rdma_lite_context *lite_ctx, struct rdma_lite_qp_attr *lite_qp_attr)
{
    struct hns_roce_lite_context *context = NULL;
    struct hns_roce_lite_qp *qp = NULL;
    int ret;

    HNS_ROCE_U_NULL_POINT_RETURN_NULL(lite_qp_attr);
    HNS_ROCE_U_NULL_POINT_RETURN_NULL(lite_ctx);

    if (hns_roce_verify_lite_qp(lite_qp_attr)) {
        roce_err("hns_roce_verify_lite_qp failed!");
        return NULL;
    }

    qp = calloc(1, sizeof(struct hns_roce_lite_qp));
    if (qp == NULL) {
        roce_err("calloc lite_qp failed!");
        return NULL;
    }

    context = to_hr_lite_ctx(lite_ctx);
    ret = hns_roce_lite_create_qp_init(context, qp, lite_qp_attr);
    if (ret) {
        roce_err("lite_qp init failed, ret [%d], expect 0", ret);
        goto err_qp_init;
    }

    return &qp->lite_qp;

err_qp_init:
    free(qp);
    qp = NULL;
    return NULL;
}

int hns_roce_lite_destroy_qp(struct rdma_lite_qp *lite_qp)
{
    struct hns_roce_lite_context *ctx = NULL;
    struct hns_roce_lite_qp *qp = NULL;
    int ret;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_qp);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_qp->ctx);
    qp = to_hr_lite_qp(lite_qp);
    ctx = to_hr_lite_ctx(lite_qp->ctx);

    (void)pthread_mutex_lock(&ctx->qp_table_mutex);
    hns_roce_clear_lite_qp(ctx, lite_qp->qp_num);
    (void)pthread_mutex_unlock(&ctx->qp_table_mutex);

    ret = hns_roce_lite_unmmap_hdb(&qp->rdb_buf, ctx);
    if (ret) {
        roce_err("hns_roce_lite_unmmap_hdb failed, ret = %d", ret);
    }

    ret = hns_roce_lite_unmmap_hdb(&qp->sdb_buf, ctx);
    if (ret) {
        roce_err("hns_roce_lite_unmmap_hdb failed, ret = %d", ret);
    }

    ret = hns_roce_lite_unmmap_hva(&qp->buf, ctx);
    if (ret) {
        roce_err("hns_roce_lite_unmmap_hva failed, ret = %d", ret);
    }

    free(qp->rq.wrid);
    qp->rq.wrid = NULL;

    free(qp->sq.wrid);
    qp->sq.wrid = NULL;

    hns_roce_lite_qp_lock_uninit(qp);

    free(qp);
    qp = NULL;

    return 0;
}

STATIC struct hns_roce_lite_cqe *get_cqe(struct hns_roce_lite_cq *cq, u32 entry)
{
    return (struct hns_roce_lite_cqe *)((char *)cq->cq_buf.hva + (u32)(entry * cq->cqe_size));
}

STATIC void *get_sw_cqe(struct hns_roce_lite_cq *cq, u32 n)
{
    struct hns_roce_lite_cqe *cqe = get_cqe(cq, n & (cq->depth - 1));

    return ((roce_get_bit(cqe->byte_4, CQE_BYTE_4_OWNER_S) != 0) != (((n & (cq->depth)) != 0))) ? cqe : NULL;
}

STATIC struct hns_roce_lite_cqe *next_cqe_sw(struct hns_roce_lite_cq *cq)
{
    return get_sw_cqe(cq, cq->cons_index);
}

STATIC struct hns_roce_lite_qp *hns_roce_lite_find_qp(struct hns_roce_lite_context *ctx, u32 qpn)
{
    u32 tind = (qpn & (ctx->num_qps - 1)) >> (u32)ctx->qp_table_shift;

    if (tind < HNS_ROCE_LITE_QP_TABLE_SIZE) {
        if (ctx->qp_table[tind].refcnt) {
            return ctx->qp_table[tind].table[qpn & (u32)(ctx->qp_table_mask)];
        }
    }

    return NULL;
}

STATIC void hns_roce_lite_poll_one_set_wc(struct rdma_lite_wc *lite_wc, u32 qpn, struct hns_roce_lite_qp **cur_qp,
                                          int is_send, struct hns_roce_lite_cqe *cqe)
{
    struct hns_roce_lite_wq *lite_wq = NULL;
    u16 wqe_ctr;

    lite_wc->qp_num = qpn & 0xffffff;

    if (is_send) {
        lite_wq = &(*cur_qp)->sq;
        /*
         * if sq_signal_bits is 1, the tail pointer first update to
         * the wqe corresponding the current cqe
         */
        if ((*cur_qp)->sq_signal_bits) {
            wqe_ctr = (u16)(roce_get_field(cqe->byte_4, CQE_BYTE_4_WQE_IDX_M, CQE_BYTE_4_WQE_IDX_S));
            /*
             * lite_wq->tail will plus a positive number every time,
             * when lite_wq->tail exceeds 32b, it is 0 and acc
             */
            lite_wq->tail += (u32)(wqe_ctr - (u16)lite_wq->tail) & (lite_wq->wqe_cnt - 1);
        }
        /* write the wr_id of lite_wq into the lite_wc */
        lite_wc->wr_id = lite_wq->wrid[lite_wq->tail & (lite_wq->wqe_cnt - 1)];
        ++lite_wq->tail;
    } else {
        lite_wq = &(*cur_qp)->rq;
        lite_wc->wr_id = lite_wq->wrid[lite_wq->tail & (lite_wq->wqe_cnt - 1)];
        ++lite_wq->tail;
    }
}

static void hns_roce_lite_handle_error_cqe(struct hns_roce_lite_cqe *cqe, struct rdma_lite_wc *lite_wc)
{
    struct cqe_wc_status lite_cqe_wc_array[] = {
        {HNS_ROCE_V2_CQE_LOCAL_LENGTH_ERR, RDMA_LITE_WC_LOC_LEN_ERR},
        {HNS_ROCE_V2_CQE_LOCAL_QP_OP_ERR, RDMA_LITE_WC_LOC_QP_OP_ERR},
        {HNS_ROCE_V2_CQE_LOCAL_PROT_ERR, RDMA_LITE_WC_LOC_PROT_ERR},
        {HNS_ROCE_V2_CQE_WR_FLUSH_ERR, RDMA_LITE_WC_WR_FLUSH_ERR},
        {HNS_ROCE_V2_CQE_MEM_MANAGERENT_OP_ERR, RDMA_LITE_WC_MW_BIND_ERR},
        {HNS_ROCE_V2_CQE_BAD_RESP_ERR, RDMA_LITE_WC_BAD_RESP_ERR},
        {HNS_ROCE_V2_CQE_LOCAL_ACCESS_ERR, RDMA_LITE_WC_LOC_ACCESS_ERR},
        {HNS_ROCE_V2_CQE_REMOTE_INVAL_REQ_ERR, RDMA_LITE_WC_REM_INV_REQ_ERR},
        {HNS_ROCE_V2_CQE_REMOTE_ACCESS_ERR, RDMA_LITE_WC_REM_ACCESS_ERR},
        {HNS_ROCE_V2_CQE_REMOTE_OP_ERR, RDMA_LITE_WC_REM_OP_ERR},
        {HNS_ROCE_V2_CQE_TRANSPORT_RETRY_EXC_ERR, RDMA_LITE_WC_RETRY_EXC_ERR},
        {HNS_ROCE_V2_CQE_RNR_RETRY_EXC_ERR, RDMA_LITE_WC_RNR_RETRY_EXC_ERR},
        {HNS_ROCE_V2_CQE_REMOTE_ABORTED_ERR, RDMA_LITE_WC_REM_ABORT_ERR},
    };
    u32 status = roce_get_field(cqe->byte_4, CQE_BYTE_4_STATUS_M, CQE_BYTE_4_STATUS_S);
    int array_num = sizeof(lite_cqe_wc_array) / sizeof(struct cqe_wc_status);
    u32 cqe_status = status & HNS_ROCE_LITE_CQE_STATUS_MASK;
    int i;

    for (i = 0; i < array_num; i++) {
        if (cqe_status == lite_cqe_wc_array[i].cqe_status) {
            lite_wc->status = lite_cqe_wc_array[i].wc_status;
            break;
        }
    }

    if (i == array_num) {
        lite_wc->status = RDMA_LITE_WC_GENERAL_ERR;
    }

    if (lite_wc->status != RDMA_LITE_WC_WR_FLUSH_ERR) {
        roce_err("error cqe status: 0x%x", cqe_status);
    }
}

STATIC void hns_roce_lite_mark_recv_opcode(struct hns_roce_lite_cqe *cqe, struct rdma_lite_wc *lite_wc,
                                           struct rdma_lite_wc_ext *ext)
{
    u32 opcode;

    lite_wc->byte_len = le32toh(cqe->byte_cnt);
    opcode = roce_get_field(cqe->byte_4, CQE_BYTE_4_OPCODE_M, CQE_BYTE_4_OPCODE_S) & HNS_ROCE_LITE_CQE_OPCODE_MASK;

    switch (opcode) {
        case HNS_ROCE_RECV_OP_RDMA_WRITE_IMM:
            lite_wc->opcode = RDMA_LITE_WC_RECV_RDMA_WITH_IMM;
            lite_wc->wc_flags = RDMA_LITE_WC_WITH_IMM;
            ext->imm_data = le32toh(cqe->immtdata);
            break;
        case HNS_ROCE_RECV_OP_SEND:
            lite_wc->opcode = RDMA_LITE_WC_RECV;
            lite_wc->wc_flags = 0;
            break;
        case HNS_ROCE_RECV_OP_SEND_WITH_IMM:
            lite_wc->opcode = RDMA_LITE_WC_RECV;
            lite_wc->wc_flags = RDMA_LITE_WC_WITH_IMM;
            ext->imm_data = le32toh(cqe->immtdata);
            break;
        case HNS_ROCE_RECV_OP_SEND_WITH_INV:
            lite_wc->opcode = RDMA_LITE_WC_RECV;
            lite_wc->wc_flags = RDMA_LITE_WC_WITH_INV;
            ext->invalidated_rkey = le32toh(cqe->rkey);
            break;
        default:
            lite_wc->status = RDMA_LITE_WC_GENERAL_ERR;
            roce_err("unknown opcode: 0x%08x", opcode);
            break;
    }
}

STATIC void hns_roce_lite_mark_send_opcode(int is_send, struct hns_roce_lite_cqe *cqe, struct rdma_lite_wc *lite_wc)
{
    u32 opcode;

    if (is_send) {
        /* Get opcode and flag before update the tail point for send */
        opcode = roce_get_field(cqe->byte_4, CQE_BYTE_4_OPCODE_M, CQE_BYTE_4_OPCODE_S) & HNS_ROCE_LITE_CQE_OPCODE_MASK;
        lite_wc->wc_flags = 0;
        switch (opcode) {
            case HNS_ROCE_SQ_OP_SEND:
                lite_wc->opcode = RDMA_LITE_WC_SEND;
                break;
            case HNS_ROCE_SQ_OP_SEND_WITH_IMM:
                lite_wc->opcode = RDMA_LITE_WC_SEND;
                lite_wc->wc_flags = RDMA_LITE_WC_WITH_IMM;
                break;
            case HNS_ROCE_SQ_OP_SEND_WITH_INV:
                lite_wc->opcode = RDMA_LITE_WC_SEND;
                break;
            case HNS_ROCE_SQ_OP_RDMA_READ:
                lite_wc->opcode = RDMA_LITE_WC_RDMA_READ;
                lite_wc->byte_len = le32toh(cqe->byte_cnt);
                break;
            case HNS_ROCE_SQ_OP_RDMA_WRITE:
                lite_wc->opcode = RDMA_LITE_WC_RDMA_WRITE;
                break;
            case HNS_ROCE_SQ_OP_RDMA_WRITE_WITH_IMM:
                lite_wc->opcode = RDMA_LITE_WC_RDMA_WRITE;
                lite_wc->wc_flags = RDMA_LITE_WC_WITH_IMM;
                break;
            case HNS_ROCE_SQ_OP_LOCAL_INV:
                lite_wc->opcode = RDMA_LITE_WC_LOCAL_INV;
                lite_wc->wc_flags = RDMA_LITE_WC_WITH_INV;
                break;
            case HNS_ROCE_SQ_OP_ATOMIC_COMP_AND_SWAP:
                lite_wc->opcode = RDMA_LITE_WC_COMP_SWAP;
                lite_wc->byte_len = BYTE_LEN;
                break;
            case HNS_ROCE_SQ_OP_ATOMIC_MASK_COMP_AND_SWAP:
                lite_wc->opcode = (enum rdma_lite_wc_opcode)HNS_ROCE_WC_MASK_COMP_SWAP;
                lite_wc->byte_len = BYTE_LEN;
                break;
            case HNS_ROCE_SQ_OP_ATOMIC_FETCH_AND_ADD:
                lite_wc->opcode = RDMA_LITE_WC_FETCH_ADD;
                lite_wc->byte_len = BYTE_LEN;
                break;
            case HNS_ROCE_SQ_OP_ATOMIC_MASK_FETCH_AND_ADD:
                lite_wc->opcode = (enum rdma_lite_wc_opcode)HNS_ROCE_WC_MASK_FETCH_ADD;
                lite_wc->byte_len = BYTE_LEN;
                break;
            case HNS_ROCE_SQ_OP_BIND_MW:
                lite_wc->opcode = RDMA_LITE_WC_BIND_MW;
                break;
            case HNS_ROCE_SQ_OP_NOP:
                lite_wc->opcode = (enum rdma_lite_wc_opcode)HNS_ROCE_WC_NOP;
                break;
            case HNS_ROCE_SQ_OP_REDUCE_WRITE:
                lite_wc->opcode = (enum rdma_lite_wc_opcode)HNS_ROCE_WC_REDUCE_WRITE;
                break;
            case HNS_ROCE_SQ_OP_REDUCE_WRITE_NOTIFY:
                lite_wc->opcode = (enum rdma_lite_wc_opcode)HNS_ROCE_WC_REDUCE_WRITE_NOTIFY;
                break;
            case HNS_ROCE_SQ_OP_WRITE_NOTIFY:
                lite_wc->opcode = (enum rdma_lite_wc_opcode)HNS_ROCE_WC_WRITE_NOTIFY;
                break;
            case HNS_ROCE_SQ_OP_ATOMIC_WRITE:
                lite_wc->opcode = (enum rdma_lite_wc_opcode)HNS_ROCE_WC_ATOMIC_WRITE;
                break;
            default:
                lite_wc->status = RDMA_LITE_WC_GENERAL_ERR;
                roce_err("unknown opcode: 0x%08x", opcode);
                break;
        }
    }
}

STATIC void hns_roce_lite_poll_one_mark_opcode(int is_send, struct hns_roce_lite_cqe *cqe, struct rdma_lite_wc *lite_wc,
                                               struct rdma_lite_wc_ext *ext)
{
    if (is_send != 0 || ext == NULL) {
        hns_roce_lite_mark_send_opcode(is_send, cqe, lite_wc);
        return;
    }

    hns_roce_lite_mark_recv_opcode(cqe, lite_wc, ext);
    return;
}

STATIC void dump_err_cqe(const struct hns_roce_lite_cq *cq, const u32 *cqe, u32 wc_status)
{
#define CQE_ELEMENT_EIGHT 8
    int index;

    if (wc_status != RDMA_LITE_WC_WR_FLUSH_ERR) {
        for (index = 0; index < CQE_ELEMENT_EIGHT; index++) {
            roce_err("CQ(0x%lx) CQE(0x%x) INDEX(0x%08x): 0x%08x", cq->cqn, cq->cons_index, index, *(cqe + index));
        }
    }
}

STATIC int hns_roce_lite_poll_one(struct hns_roce_lite_cq *cq, struct hns_roce_lite_qp **cur_qp,
                                  struct rdma_lite_wc *lite_wc, struct rdma_lite_wc_ext *ext)
{
    struct hns_roce_lite_cqe *cqe = NULL;
    int is_send;
    u32 qpn;

    /* According to CI, find the relative cqe */
    cqe = next_cqe_sw(cq);
    if (cqe == NULL) {
        /* This is normal, don't need record log. */
        return CQ_EMPTY;
    }

    /* Get the next cqe, CI will be added gradually */
    ++cq->cons_index;

    udma_from_device_barrier();

    qpn = roce_get_field(cqe->byte_16, CQE_BYTE_16_LCL_QPN_M, CQE_BYTE_16_LCL_QPN_S);

    is_send = (roce_get_bit(cqe->byte_4, CQE_BYTE_4_S_R_S) == HNS_ROCE_LITE_CQE_IS_SQ);

    /* if qp is zero, it will not get the correct qpn */
    if (*cur_qp == NULL || (qpn & HNS_ROCE_LITE_CQE_QPN_MASK) != (*cur_qp)->lite_qp.qp_num) {
        *cur_qp = hns_roce_lite_find_qp(to_hr_lite_ctx(cq->lite_cq.ctx), qpn & 0xffffff);
        if (*cur_qp == NULL) {
            roce_err("can't find qp!");
            return CQ_POLL_ERR;
        }
    }

    hns_roce_lite_poll_one_set_wc(lite_wc, qpn, cur_qp, is_send, cqe);

    /*
     * HW maintains lite_wc status, set the err type and directly return, after
     * generated the incorrect CQE
     */
    if (roce_get_field(cqe->byte_4, CQE_BYTE_4_STATUS_M, CQE_BYTE_4_STATUS_S) != HNS_ROCE_LITE_CQE_SUCCESS) {
        hns_roce_lite_handle_error_cqe(cqe, lite_wc);
        dump_err_cqe(cq, (u32 *)cqe, lite_wc->status);
        (*cur_qp)->lite_qp.qp_state = RDMA_LITE_QPS_ERR;
        return CQ_OK;
    }

    lite_wc->status = RDMA_LITE_WC_SUCCESS;

    /*
     * According to the opcode type of cqe, mark the opcode and other
     * information of lite_wc
     */
    hns_roce_lite_poll_one_mark_opcode(is_send, cqe, lite_wc, ext);

    return CQ_OK;
}

int hns_roce_lite_poll_cq(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc *lite_wc)
{
    struct hns_roce_lite_cq *cq = NULL;
    struct hns_roce_lite_qp *qp = NULL;
    int err = CQ_OK;
    int npolled;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_cq);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_wc);

    cq = to_hr_lite_cq(lite_cq);
    if ((cq->flags & HNS_ROCE_LITE_CQ_FLAG_RECORD_DB) == 0) {
        roce_err("Lite cq only supports record db! cq_flag 0x%x", cq->flags);
        return -EINVAL;
    }

    pthread_spin_lock(&cq->lock);

    for (npolled = 0; npolled < num_entries; ++npolled) {
        err = hns_roce_lite_poll_one(cq, &qp, lite_wc + npolled, NULL);
        if (err != CQ_OK) {
            break;
        }
    }

    if (npolled != 0 || err == CQ_POLL_ERR) {
        *(u32 *)cq->swdb_buf.hva = cq->cons_index & RECORD_DB_CI_MASK;
    }

    pthread_spin_unlock(&cq->lock);

    return (err == CQ_POLL_ERR) ? err : npolled;
}

int hns_roce_lite_poll_cq_v2(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc_v2 *lite_wc)
{
    struct rdma_lite_wc_v2 *lite_wc_tmp = NULL;
    struct rdma_lite_wc_ext *ext = NULL;
    struct hns_roce_lite_cq *cq = NULL;
    struct hns_roce_lite_qp *qp = NULL;
    struct rdma_lite_wc *wc = NULL;
    int err = CQ_OK;
    int npolled;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_cq);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_wc);

    cq = to_hr_lite_cq(lite_cq);
    if ((cq->flags & HNS_ROCE_LITE_CQ_FLAG_RECORD_DB) == 0) {
        roce_err("Lite cq only supports record db! cq_flag 0x%x", cq->flags);
        return -EINVAL;
    }

    pthread_spin_lock(&cq->lock);

    for (npolled = 0; npolled < num_entries; ++npolled) {
        lite_wc_tmp = lite_wc + npolled;
        wc = &lite_wc_tmp->wc;
        ext = &lite_wc_tmp->ext;
        err = hns_roce_lite_poll_one(cq, &qp, wc, ext);
        if (err != CQ_OK) {
            break;
        }
        ext->version = LITE_WC_EXT_VERSION;
    }

    if (npolled != 0 || err == CQ_POLL_ERR) {
        *(u32 *)cq->swdb_buf.hva = cq->cons_index & RECORD_DB_CI_MASK;
    }

    pthread_spin_unlock(&cq->lock);

    return (err == CQ_POLL_ERR) ? err : npolled;
}

STATIC int check_qp_send_recv(struct hns_roce_lite_qp *qp)
{
    struct rdma_lite_qp *lite_qp = &qp->lite_qp;

    if (unlikely(lite_qp->qp_type != RDMA_LITE_QPT_RC)) {
        roce_err("unsupported qp type, qp_type = %d.", lite_qp->qp_type);
        return -EINVAL;
    }

    if (unlikely(lite_qp->qp_state == RDMA_LITE_QPS_ERR)) {
        roce_err("unsupported qp state, state = %d", lite_qp->qp_state);
        return -EINVAL;
    }
    return 0;
}

STATIC int hns_roce_lite_wq_overflow(struct hns_roce_lite_wq *wq, u32 nreq, struct hns_roce_lite_cq *cq)
{
    u32 cur;

    cur = wq->head - wq->tail;
    if (cur + nreq < wq->max_post) {
        return 0;
    }

    pthread_spin_lock(&cq->lock);
    cur = wq->head - wq->tail;
    pthread_spin_unlock(&cq->lock);

    return (cur + nreq) >= wq->max_post;
}

STATIC int hns_roce_lite_check_send_wr(struct hns_roce_lite_qp *qp, struct rdma_lite_send_wr *lite_wr, u32 nreq)
{
    if (hns_roce_lite_wq_overflow(&qp->sq, nreq, to_hr_lite_cq(qp->lite_qp.send_cq))) {
        roce_warn("send lite_wq overflow! sq head:%u tail:%u nreq:%u max_post:%d", qp->sq.head, qp->sq.tail, nreq,
                  qp->sq.max_post);
        return -ENOMEM;
    }

    if ((u32)lite_wr->num_sge > qp->sq.max_gs) {
        roce_err("send num_sge(%d) in lite_wr is bigger than max_gs(%d)", lite_wr->num_sge, qp->sq.max_gs);
        return -EINVAL;
    }

    return 0;
}

STATIC void *get_send_lite_wqe(struct hns_roce_lite_qp *qp, u32 n)
{
    return (void *)((char *)qp->buf.hva + (u32)(qp->sq.offset + (n << (u32)(qp->sq.wqe_shift))));
}

STATIC void *get_recv_lite_wqe(struct hns_roce_lite_qp *qp, u32 n)
{
    return (void *)((char *)qp->buf.hva + (u32)(qp->rq.offset + (n << (u32)(qp->rq.wqe_shift))));
}

STATIC __le32 get_immtdata(enum rdma_lite_wr_opcode opcode, const struct rdma_lite_send_wr *lite_wr)
{
    switch (opcode) {
        case RDMA_LITE_WR_SEND_WITH_IMM:
        case RDMA_LITE_WR_RDMA_WRITE_WITH_IMM:
            return htole32(be32toh(lite_wr->imm_data));
        default:
            return 0;
    }
}

STATIC void set_reduce_op(struct hns_roce_lite_rc_sq_wqe *rc_sq_wqe, const struct rdma_lite_post_send_attr *attr)
{
    roce_set_field(rc_sq_wqe->byte_20, RC_SQ_WQE_BYTE_20_REDUCE_TYPE_M, RC_SQ_WQE_BYTE_20_REDUCE_TYPE_S,
                   attr->reduce_type & INLINE_REDUCE_TYPE_MASK);
    roce_set_field(rc_sq_wqe->byte_20, RC_SQ_WQE_BYTE_20_REDUCE_OP_M, RC_SQ_WQE_BYTE_20_REDUCE_OP_S,
                   attr->reduce_op & INLINE_REDUCE_OP_MASK);
}

STATIC int check_rc_opcode(struct hns_roce_lite_rc_sq_wqe *rc_sq_wqe, const struct rdma_lite_send_wr *lite_wr,
                           struct rdma_lite_post_send_attr *attr)
{
    u32 opcode = lite_wr->opcode;
    int ret = 0;

    rc_sq_wqe->immtdata = get_immtdata(opcode, lite_wr);

    switch (opcode) {
        case RDMA_LITE_WR_REDUCE_WRITE:
        case RDMA_LITE_WR_REDUCE_WRITE_NOTIFY:
            set_reduce_op(rc_sq_wqe, attr);
            rc_sq_wqe->va = htole64(lite_wr->remote_addr);
            rc_sq_wqe->rkey = htole32(lite_wr->rkey);
            break;
        case RDMA_LITE_WR_RDMA_READ:
        case RDMA_LITE_WR_RDMA_WRITE:
        case RDMA_LITE_WR_RDMA_WRITE_WITH_IMM:
        case RDMA_LITE_WR_ATOMIC_WRITE:
        case RDMA_LITE_WR_WRITE_WITH_NOTIFY:
            rc_sq_wqe->va = htole64(lite_wr->remote_addr);
            rc_sq_wqe->rkey = htole32(lite_wr->rkey);
            break;
        case RDMA_LITE_WR_SEND:
        case RDMA_LITE_WR_SEND_WITH_IMM:
        case RDMA_LITE_WR_NOP:
            break;
        default:
            ret = -EINVAL;
            break;
    }

    roce_set_field(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_OPCODE_M, RC_SQ_WQE_BYTE_4_OPCODE_S, to_hr_lite_opcode(opcode));
    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_CQE_S,
                 (lite_wr->send_flags & RDMA_LITE_SEND_SIGNALED) != 0 ? 1 : 0);
    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_FENCE_S,
                 (lite_wr->send_flags & RDMA_LITE_SEND_FENCE) != 0 ? 1 : 0);
    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_SE_S,
                 (lite_wr->send_flags & RDMA_LITE_SEND_SOLICITED) != 0 ? 1 : 0);

    return ret;
}

STATIC void set_data_seg(struct hns_roce_lite_wqe_data_seg *dseg, struct rdma_lite_sge *sg)
{
    dseg->lkey = htole32(sg->lkey);
    dseg->addr = htole64(sg->addr);
    dseg->len = htole32(sg->length);
}

STATIC void set_sge(struct hns_roce_lite_wqe_data_seg *dseg, struct hns_roce_lite_qp *qp,
                    struct rdma_lite_send_wr *lite_wr, struct hns_roce_lite_sge_info *sge_info)
{
    struct hns_roce_lite_wqe_data_seg *temp_dseg = dseg;
    int i;

    sge_info->valid_num = 0;
    sge_info->total_len = 0;

    for (i = 0; i < lite_wr->num_sge; i++) {
        if (likely(lite_wr->sg_list[i].length != 0)) {
            sge_info->total_len += lite_wr->sg_list[i].length;
            sge_info->valid_num++;

            if ((((lite_wr->send_flags & RDMA_LITE_SEND_INLINE) != 0) &&
                 (lite_wr->opcode != RDMA_LITE_WR_ATOMIC_FETCH_AND_ADD) &&
                 (lite_wr->opcode != RDMA_LITE_WR_ATOMIC_CMP_AND_SWP)) ||
                (lite_wr->opcode == RDMA_LITE_WR_ATOMIC_WRITE)) {
                continue;
            }

            /* No inner sge in UD wqe */
            if (sge_info->valid_num <= HNS_ROCE_SGE_IN_WQE && qp->lite_qp.qp_type != RDMA_LITE_QPT_UD) {
                set_data_seg(temp_dseg, lite_wr->sg_list + i);
                temp_dseg++;
            }
        }
    }
}

STATIC int set_atom_write_seg(struct hns_roce_lite_wqe_data_seg *dseg, struct hns_roce_lite_rc_sq_wqe *rc_sq_wqe,
                              struct rdma_lite_send_wr *lite_wr, struct hns_roce_lite_sge_info *sge_info)
{
    // strict maximum support atomic write data to 4B, default capability supports 8B
    if (sge_info->total_len > sizeof(uint32_t)) {
        roce_err("total_len:%u exceeds %zu, atomic write is not allowed", sge_info->total_len, sizeof(uint32_t));
        return -EINVAL;
    }

    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_INLINE_S, 1);
    roce_set_bit(rc_sq_wqe->byte_20, RC_SQ_WQE_BYTE_20_INL_TYPE_S, 0);

    // atomic write data use 4B in dseg->len field(byte_36)
    dseg->len = htole32(be32toh(lite_wr->imm_data));
    return 0;
}

STATIC int hns_roce_lite_set_rc_wqe(void *wqe, struct hns_roce_lite_qp *qp, struct rdma_lite_send_wr *lite_wr,
                                    struct rdma_lite_post_send_attr *attr, u32 nreq,
                                    struct hns_roce_lite_sge_info *sge_info)
{
    struct hns_roce_lite_rc_sq_wqe *send_wqe = wqe;
    struct hns_roce_lite_rc_sq_wqe *rc_sq_wqe;
    struct hns_roce_lite_wqe_data_seg *dseg;
    u32 owner_bit;
    u32 wqe_length;
    int ret;

    // rc wqe固定64字节，最多2个sge
    if (lite_wr->num_sge > RC_MAX_SGE_NUME) {
        roce_err("num_sge[%d] > max sge num[%d].\n", lite_wr->num_sge, RC_MAX_SGE_NUME);
        return -EINVAL;
    }

    wqe_length = sizeof(struct hns_roce_lite_rc_sq_wqe) + sizeof(struct hns_roce_lite_wqe_data_seg) * lite_wr->num_sge;
    rc_sq_wqe = (struct hns_roce_lite_rc_sq_wqe *)calloc(1, wqe_length);
    if (rc_sq_wqe == NULL) {
        roce_err("calloc rc sq wqe failed.\n");
        return -ENOMEM;
    }

    ret = check_rc_opcode(rc_sq_wqe, lite_wr, attr);
    if (ret) {
        roce_err("unsupported opcode, opcode = %d.\n", lite_wr->opcode);
        goto out;
    }

    roce_set_field(rc_sq_wqe->byte_20, RC_SQ_WQE_BYTE_20_MSG_START_SGE_IDX_M, RC_SQ_WQE_BYTE_20_MSG_START_SGE_IDX_S,
                   sge_info->start_idx & (qp->sge.sge_cnt - 1));

    dseg = (struct hns_roce_lite_wqe_data_seg *)((char *)rc_sq_wqe + sizeof(struct hns_roce_lite_rc_sq_wqe));
    set_sge(dseg, qp, lite_wr, sge_info);
    rc_sq_wqe->msg_len = htole32(sge_info->total_len);

    if (lite_wr->opcode == RDMA_LITE_WR_ATOMIC_WRITE) {
        ret = set_atom_write_seg(dseg, rc_sq_wqe, lite_wr, sge_info);
        if (ret != 0) {
            roce_err("set_atom_write_seg failed, ret=%d qpn=%u\n", ret, qp->lite_qp.qp_num);
            goto out;
        }
    }

    roce_set_field(rc_sq_wqe->byte_16, RC_SQ_WQE_BYTE_16_SGE_NUM_M, RC_SQ_WQE_BYTE_16_SGE_NUM_S, sge_info->valid_num);
    owner_bit = ((qp->sq.head + nreq) >> qp->sq.shift) & 0x1;
    roce_set_bit(rc_sq_wqe->byte_4, RC_SQ_WQE_BYTE_4_OWNER_S, owner_bit);

    (void)memcpy_s(send_wqe, wqe_length, rc_sq_wqe, wqe_length);

    if (qp->flags & HNS_ROCE_QP_CAP_OWNER_DB) {
        udma_to_device_barrier();
    }

    roce_set_bit(send_wqe->byte_4, RC_SQ_WQE_BYTE_4_OWNER_S, ~owner_bit);
out:
    free(rc_sq_wqe);
    rc_sq_wqe = NULL;
    return ret;
}

STATIC int hns_roce_lite_set_wqe(void *wqe, struct hns_roce_lite_qp *qp, struct rdma_lite_send_wr *lite_wr,
                                 struct rdma_lite_post_send_attr *attr, u32 nreq,
                                 struct hns_roce_lite_sge_info *sge_info)
{
    int ret = 0;

    switch (qp->lite_qp.qp_type) {
        case RDMA_LITE_QPT_RC:
            ret = hns_roce_lite_set_rc_wqe(wqe, qp, lite_wr, attr, nreq, sge_info);
            break;
        default:
            roce_err("Not supported qp type %d", qp->lite_qp.qp_type);
            return -EINVAL;
    }

    return ret;
}

STATIC void hns_roce_lite_get_rsp(struct hns_roce_lite_qp *qp, struct rdma_lite_post_send_resp *resp)
{
    u32 pi = qp->sq.head & ((qp->sq.wqe_cnt << 1) - 1);

    if (qp->gdr_enabled == HNS_ROCE_QP_AI_MODE_OP || qp->gdr_enabled == HNS_ROCE_QP_AI_MODE_GDR_ASYN ||
        qp->gdr_enabled == HNS_ROCE_QP_AI_MODE_OP_EXT) {
        resp->db.lite_db_info = ((unsigned long)qp->sl << DB_SL_OFFSET) | ((unsigned long)pi << DB_PI_OFFSET) |
                                (0 << DB_QPN_OFFSET) | qp->lite_qp.qp_num;
    }
}

int hns_roce_lite_post_send(struct rdma_lite_qp *lite_qp, struct rdma_lite_send_wr *lite_wr,
                            struct rdma_lite_send_wr **bad_wr, struct rdma_lite_post_send_attr *attr,
                            struct rdma_lite_post_send_resp *resp)
{
    struct rdma_lite_send_wr *lite_wr_temp = NULL;
    struct hns_roce_lite_sge_info sge_info = {0};
    struct hns_roce_lite_qp *qp = NULL;
    void *wqe = NULL;
    u32 wqe_idx = 0;
    u32 nreq = 0;
    int ret;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_qp);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_wr);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(attr);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(resp);

    qp = to_hr_lite_qp(lite_qp);
    lite_wr_temp = lite_wr;

    ret = check_qp_send_recv(qp);
    if (ret) {
        *bad_wr = lite_wr_temp;
        return ret;
    }

    pthread_spin_lock(&qp->sq.lock);
    sge_info.start_idx = qp->next_sge; /* start index of extend sge */

    for (nreq = 0; lite_wr_temp != NULL; ++nreq, lite_wr_temp = lite_wr_temp->next) {
        ret = hns_roce_lite_check_send_wr(qp, lite_wr_temp, nreq);
        if (ret) {
            *bad_wr = lite_wr_temp;
            goto out;
        }

        wqe_idx = (qp->sq.head + (u32)nreq) & (qp->sq.wqe_cnt - 1);
        wqe = get_send_lite_wqe(qp, wqe_idx);
        qp->sq.wrid[wqe_idx] = lite_wr_temp->wr_id;

        ret = hns_roce_lite_set_wqe(wqe, qp, lite_wr_temp, attr, nreq, &sge_info);
        if (ret) {
            *bad_wr = lite_wr_temp;
            goto out;
        }
    }

out:
    qp->sq.head += nreq;
    qp->next_sge = sge_info.start_idx;

    if (qp->flags & HNS_ROCE_QP_CAP_SQ_RECORD_DB) {
        *(u32 *)qp->sdb_buf.hva = qp->sq.head & 0xffff;
    }

    pthread_spin_unlock(&qp->sq.lock);

    hns_roce_lite_get_rsp(qp, resp);

    return ret;
}

STATIC void hns_roce_lite_post_recv_out(int nreq, struct hns_roce_lite_qp *qp)
{
    if (nreq == 0) {
        return;
    }

    qp->rq.head += (u32)nreq;
    udma_to_device_barrier();
    *(u32 *)qp->rdb_buf.hva = qp->rq.head & 0xffff;
}

STATIC void hns_roce_lite_post_recv_fill_dseg(struct hns_roce_lite_qp *qp, struct rdma_lite_recv_wr *lite_wr,
                                              struct hns_roce_lite_wqe_data_seg *dseg)
{
    struct hns_roce_lite_wqe_data_seg *temp_dseg = dseg;
    int i;

    for (i = 0; i < lite_wr->num_sge; i++) {
        if (likely(lite_wr->sg_list[i].length != 0)) {
            set_data_seg(temp_dseg, lite_wr->sg_list + i);
            temp_dseg++;
        }
    }

    if (i < (int)qp->rq.max_gs) {
        temp_dseg->len = 0;
        temp_dseg->lkey = htole32(HNS_ROCE_LITE_INVALID_SGE_KEY);
        temp_dseg->addr = 0;
    }
}

STATIC int hns_roce_lite_check_recv_wr(struct hns_roce_lite_qp *qp, struct rdma_lite_recv_wr *lite_wr, u32 nreq)
{
    if (hns_roce_lite_wq_overflow(&qp->rq, nreq, to_hr_lite_cq(qp->lite_qp.recv_cq))) {
        roce_warn("recv lite_wq overflow! rq head:%u tail:%u nreq:%u max_post:%d", qp->rq.head, qp->rq.tail, nreq,
                  qp->rq.max_post);
        return -ENOMEM;
    }

    if ((u32)lite_wr->num_sge > qp->rq.max_gs) {
        roce_err("recv num_sge(%d) in lite_wr is bigger than max_gs(%u)", lite_wr->num_sge, qp->rq.max_gs);
        return -EINVAL;
    }

    return 0;
}

int hns_roce_lite_post_recv(struct rdma_lite_qp *lite_qp, struct rdma_lite_recv_wr *lite_wr,
                            struct rdma_lite_recv_wr **bad_wr)
{
    struct hns_roce_lite_wqe_data_seg *dseg = NULL;
    struct rdma_lite_recv_wr *lite_wr_temp = NULL;
    struct hns_roce_lite_qp *qp = NULL;
    void *wqe = NULL;
    u32 wqe_idx = 0;
    int nreq = 0;
    int ret = 0;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_qp);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_wr);
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(bad_wr);

    qp = to_hr_lite_qp(lite_qp);
    lite_wr_temp = lite_wr;

    ret = check_qp_send_recv(qp);
    if (ret) {
        *bad_wr = lite_wr_temp;
        return ret;
    }

    pthread_spin_lock(&qp->rq.lock);

    for (nreq = 0; lite_wr_temp != NULL; ++nreq, lite_wr_temp = lite_wr_temp->next) {
        ret = hns_roce_lite_check_recv_wr(qp, lite_wr_temp, (u32)nreq);
        if (ret) {
            *bad_wr = lite_wr_temp;
            goto out;
        }

        wqe_idx = (qp->rq.head + (u32)nreq) & (qp->rq.wqe_cnt - 1);
        wqe = get_recv_lite_wqe(qp, wqe_idx);
        qp->rq.wrid[wqe_idx] = lite_wr_temp->wr_id;

        dseg = (struct hns_roce_lite_wqe_data_seg *)wqe;

        hns_roce_lite_post_recv_fill_dseg(qp, lite_wr_temp, dseg);
    }

out:
    hns_roce_lite_post_recv_out(nreq, qp);

    pthread_spin_unlock(&qp->rq.lock);

    return ret;
}

int hns_roce_lite_set_qp_sl(struct rdma_lite_qp *lite_qp, int sl)
{
    struct hns_roce_lite_qp *qp = NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_qp);

    qp = to_hr_lite_qp(lite_qp);
    qp->sl = sl;
    return 0;
}

int hns_roce_lite_clean_qp(struct rdma_lite_qp *lite_qp)
{
    struct hns_roce_lite_qp *qp = NULL;

    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_qp);

    qp = to_hr_lite_qp(lite_qp);

    pthread_spin_lock(&qp->sq.lock);
    pthread_spin_lock(&qp->rq.lock);
    hns_roce_init_lite_qp_indices(qp);
    pthread_spin_unlock(&qp->rq.lock);
    pthread_spin_unlock(&qp->sq.lock);
    return 0;
}

int hns_roce_lite_restore_snapshot(struct rdma_lite_context *lite_ctx)
{
    HNS_ROCE_U_NULL_POINT_RETURN_ERR(lite_ctx);
    lite_ctx->restore_flag = true;
    return 0;
}

struct rdma_lite_ops g_hns_roce_lite_ops = {
    .rdma_lite_alloc_context = hns_roce_lite_alloc_context,
    .rdma_lite_free_context = hns_roce_lite_free_context,
    .rdma_lite_init_mem_pool = hns_roce_lite_init_mem_pool,
    .rdma_lite_deinit_mem_pool = hns_roce_lite_deinit_mem_pool,
    .rdma_lite_create_cq = hns_roce_lite_create_cq,
    .rdma_lite_destroy_cq = hns_roce_lite_destroy_cq,
    .rdma_lite_create_qp = hns_roce_lite_create_qp,
    .rdma_lite_destroy_qp = hns_roce_lite_destroy_qp,
    .rdma_lite_poll_cq = hns_roce_lite_poll_cq,
    .rdma_lite_poll_cq_v2 = hns_roce_lite_poll_cq_v2,
    .rdma_lite_post_send = hns_roce_lite_post_send,
    .rdma_lite_post_recv = hns_roce_lite_post_recv,
    .rdma_lite_set_qp_sl = hns_roce_lite_set_qp_sl,
    .rdma_lite_clean_qp = hns_roce_lite_clean_qp,
    .rdma_lite_restore_snapshot = hns_roce_lite_restore_snapshot,
};
