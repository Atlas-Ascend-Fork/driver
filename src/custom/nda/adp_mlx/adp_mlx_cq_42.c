/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
/* Validated 2410mlnx54 private ABI.
 * Field/command contracts: upstream providers/mlx5/{verbs.c,mlx5.h}.
 * Never call mlx5_alloc_cq_buf: it dereferences device-only CQ memory. */
#include <stdint.h>
#include <stdlib.h>
#include <mlx5.h>
#include "adp_mlx_internal.h"

struct ibv_cq *mlx_create_cq_nda(struct mlx_resource *r, struct ibv_cq_init_attr_ex *attr)
{
    /* mlx_create_cq checked the attributes and installed our parent with
     * mlx_alloc/mlx_free and pd_context.
     * This internal entry is not a public cross-version verbs API. */
    struct mlx5_context *context = to_mctx(r->context);
    struct mlx5_parent_domain *parent = to_mparent_domain(r->parent);
    int page = to_mdev(r->context->device)->page_size;
    if (page < (int)ADP_MLX_CQE_SIZE || (page & (page - 1))) {
        errno = EINVAL;
        return NULL;
    }
    /* The common CQ command carries a 16-bit UAR index. Reject before
     * allocating queue memory rather than silently truncating the index. */
    if (context->nc_uar && context->nc_uar->page_id > UINT16_MAX) {
        errno = EOPNOTSUPP;
        return NULL;
    }
    struct mlx5_cq *cq = calloc(1, sizeof(*cq));
    if (!cq)
        return NULL;
    int error = mlx5_spinlock_init(&cq->lock, 1);
    if (error) {
        free(cq);
        errno = error;
        return NULL;
    }
    /* Install event state before publishing the user_handle to the kernel.
     * No completion channel is supported by this NPU-owned polling CQ. */
    verbs_init_cq(&cq->verbs_cq.cq, r->context, NULL, attr->cq_context);
    /* mlx5_ib registers round_up_pow2(actual.cqe + 1) * cqe_size bytes.
     * Allocation padding alone cannot enlarge that range. Make the hardware
     * ring at least one 4 KiB NPU page (64 CQEs), then submit entries - 1
     * below so the command, CQ mask and exported slot count stay consistent. */
    unsigned int entries = ADP_MLX_NPU_PAGE_SIZE / ADP_MLX_CQE_SIZE;
    while (entries < (unsigned int)attr->cqe + 1)
        entries <<= 1;
    size_t bytes = ((size_t)entries * ADP_MLX_CQE_SIZE + page - 1) & ~((size_t)page - 1);
    cq->parent_domain = r->parent;
    cq->buf_a = (struct mlx5_buf){
        .length = bytes,
        .type = MLX5_ALLOC_TYPE_CUSTOM,
        .resource_type = MLX5DV_RES_TYPE_CQ,
        .req_alignment = page,
        .mparent_domain = parent,
    };
    cq->active_buf = &cq->buf_a;
    cq->buf_a.buf = mlx_alloc(r->parent, r, bytes, page, MLX5DV_RES_TYPE_CQ);
    if (!cq->buf_a.buf)
        goto fail;
    /* Own the whole registered DBR page; only its first eight bytes are used.
     * mlx_alloc initializes via ops.memcpy_s, never CPU stores to these VAs. */
    cq->dbrec = mlx_alloc(r->parent, r, page, page, MLX5DV_RES_TYPE_DBR);
    if (!cq->dbrec)
        goto fail;
    cq->custom_db = true;
    cq->cqe_sz = ADP_MLX_CQE_SIZE;
    cq->flags = MLX5_CQ_FLAGS_DV_OWNED;

    struct mlx5_create_cq_ex cmd = {0};
    struct mlx5_create_cq_ex_resp resp = {0};
    cmd.drv_payload.buf_addr = (uintptr_t)cq->buf_a.buf;
    cmd.drv_payload.db_addr = (uintptr_t)cq->dbrec;
    cmd.drv_payload.cqe_size = ADP_MLX_CQE_SIZE;
    if (context->nc_uar) {
        cmd.drv_payload.flags = MLX5_IB_CREATE_CQ_FLAGS_UAR_PAGE_INDEX;
        cmd.drv_payload.uar_page_index = context->nc_uar->page_id;
    }
    struct ibv_cq_init_attr_ex actual = *attr;
    actual.cqe = entries - 1;
    actual.comp_mask |= IBV_CQ_INIT_ATTR_MASK_PD;
    actual.parent_domain = r->parent;
    error = ibv_cmd_create_cq_ex(r->context, &actual, &cq->verbs_cq, &cmd.ibv_cmd, sizeof(cmd), &resp.ibv_resp,
                                 sizeof(resp), CREATE_CQ_CMD_FLAGS_TS_IGNORED_EX);
    if (error) {
        errno = error;
        goto fail;
    }
    /* Pair with unmodified mlx5_destroy_cq: custom DBR/buf free callbacks,
     * then a single decrement of this parent reference. No fallible work
     * remains here after successful kernel object creation. */
    atomic_fetch_add(&parent->mpd.refcount, 1);
    cq->cqn = resp.drv_payload.cqn;
    return &cq->verbs_cq.cq;

fail:
    error = errno ? errno : EIO;
    if (cq->dbrec)
        mlx_free(r->parent, r, cq->dbrec, MLX5DV_RES_TYPE_DBR);
    if (cq->buf_a.buf)
        mlx_free(r->parent, r, cq->buf_a.buf, MLX5DV_RES_TYPE_CQ);
    pthread_cond_destroy(&cq->verbs_cq.cq.cond);
    pthread_mutex_destroy(&cq->verbs_cq.cq.mutex);
    mlx5_spinlock_destroy(&cq->lock);
    free(cq);
    errno = error;
    return NULL;
}
