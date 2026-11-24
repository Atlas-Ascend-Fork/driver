/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "adp_mlx_internal.h"
#include <stdlib.h>

static struct ibv_td *alloc_td(struct ibv_context *context)
{
    struct ibv_td_init_attr attr = {0};
    return ibv_alloc_td(context, &attr);
}

static int query_device(struct ibv_context *context, struct ibv_device_attr_extend *attr)
{
    /* mlx_query_device validates pointers and clears public output;
     * supported() supplies its own output and receives a checked context. */
    if (!context->device)
        return mlx_error(EINVAL);
    if (!mlx5dv_is_supported(context->device))
        return mlx_error(EOPNOTSUPP);

    struct ibv_device_attr_ex device = {0};
    int ret = ibv_query_device_ex(context, NULL, &device);
    if (ret)
        return ret;

    /* Match XSC's V1 output convention: the legacy 32-bit device flags,
     * not device_cap_flags_ex. Query success alone does not establish NDR. */
    attr->resv[0] = device.orig_attr.device_cap_flags;
    /* Provider ownership is checked above; do not filter by vendor/part
     * IDs. This advertises the NDA path, not verified NPU DMA capability. */
    attr->ext_cap = IBV_EXTEND_DEV_NDA;
    return 0;
}

static bool supported(struct ibv_context *context)
{
    struct ibv_device_attr_extend attr;
    /* Query success already establishes mlx5 ownership and NDA support. */
    return query_device(context, &attr) == 0;
}

static struct ibv_pd *alloc_parent(struct mlx_resource *r, struct ibv_pd *base)
{
    struct ibv_parent_domain_init_attr attr = {
        .pd = base,
        .td = r->td,
        .comp_mask = IBV_PARENT_DOMAIN_INIT_ATTR_ALLOCATORS | IBV_PARENT_DOMAIN_INIT_ATTR_PD_CONTEXT,
        .alloc = mlx_alloc,
        .free = mlx_free,
        .pd_context = r,
    };
    r->parent = ibv_alloc_parent_domain(r->context, &attr);
    return r->parent;
}

static struct ibv_qp *create_qp(struct mlx_resource *r, struct ibv_qp_init_attr *attr)
{
    /* libmlx5 enables WQE signatures on mere presence of this variable,
     * including value "0". The NPU WQE writer does not generate signatures. */
    if (getenv("MLX5_QP_SIGNATURE")) {
        errno = EOPNOTSUPP;
        return NULL;
    }
    struct ibv_qp_init_attr_ex ex = {
        .qp_context = attr->qp_context,
        .send_cq = attr->send_cq,
        .recv_cq = attr->recv_cq,
        .srq = attr->srq,
        .cap = attr->cap,
        .qp_type = attr->qp_type,
        .sq_sig_all = attr->sq_sig_all,
        .comp_mask = IBV_QP_INIT_ATTR_PD,
        .pd = r->parent,
    };
    struct mlx5dv_qp_init_attr dv = {
        .comp_mask = MLX5DV_QP_INIT_ATTR_MASK_QP_CREATE_FLAGS,
        .create_flags = MLX5DV_QP_CREATE_DISABLE_SCATTER_TO_CQE,
    };
    struct ibv_qp *qp = mlx5dv_create_qp(r->context, &ex, &dv);
    if (qp)
        attr->cap = ex.cap;
    return qp;
}

static int export_cq(struct ibv_cq *cq, struct mlx5dv_cq *dv)
{
    struct mlx5dv_obj obj = {.cq = {.in = cq, .out = dv}};
    return mlx5dv_init_obj(&obj, MLX5DV_OBJ_CQ);
}

static int export_qp(struct ibv_qp *qp, struct mlx5dv_qp *dv)
{
    struct mlx5dv_obj obj = {.qp = {.in = qp, .out = dv}};
    return mlx5dv_init_obj(&obj, MLX5DV_OBJ_QP);
}

static int export_srq(struct ibv_srq *srq, struct mlx5dv_srq *dv)
{
    struct mlx5dv_obj obj = {.srq = {.in = srq, .out = dv}};
    return mlx5dv_init_obj(&obj, MLX5DV_OBJ_SRQ);
}

const struct mlx_backend mlx_real_backend = {
    .supported = supported,
    .query_device = query_device,
    .alloc_parent = alloc_parent,
    .alloc_pd = ibv_alloc_pd,
    .alloc_td = alloc_td,
    .dealloc_pd = ibv_dealloc_pd,
    .dealloc_td = ibv_dealloc_td,
    .create_srq = mlx_create_srq_nda,
    .export_srq = export_srq,
    .destroy_srq = mlx_destroy_srq_nda,
    .create_cq = mlx_create_cq_nda,
    .create_qp = create_qp,
    .export_cq = export_cq,
    .export_qp = export_qp,
    .destroy_cq = ibv_destroy_cq,
    .destroy_qp = ibv_destroy_qp,
};
