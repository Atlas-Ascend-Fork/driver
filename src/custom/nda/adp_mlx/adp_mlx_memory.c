/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <endian.h>
#include <stdlib.h>
#include <string.h>
#include "adp_mlx_internal.h"

int mlx_error(int error)
{
    if (error < 0)
        error = errno ? errno : EIO;
    errno = error;
    return error;
}

_Static_assert(sizeof(struct mlx5_wqe_srq_next_seg) + sizeof(struct mlx5_wqe_data_seg) == ADP_MLX_SRQ_STRIDE,
               "minimum SRQ WQE layout");

static void fill_srq_links(unsigned char *staging, size_t offset, size_t chunk, uint32_t entries, uint32_t stride)
{
    /* A power-of-two stride >= 32 either divides the 4096-byte staging block
     * or spans whole blocks. Only blocks containing a WQE start get a link;
     * the next field fits entirely in that block. Padding is never linked. */
    size_t first = offset / stride + (offset % stride != 0);
    for (size_t entry = first; entry < entries && entry * stride < offset + chunk; ++entry) {
        uint16_t index = htobe16((uint16_t)((entry + 1) % entries));
        size_t pos = entry * stride - offset;
        memcpy(staging + pos + offsetof(struct mlx5_wqe_srq_next_seg, next_wqe_index), &index, sizeof(index));
    }
}

static int initialize_memory(struct mlx_resource *r, uintptr_t addr, size_t size, uint64_t type)
{
    unsigned char staging[4096] = {0};
    size_t offset;
    if (type == MLX5DV_RES_TYPE_CQ) {
        if (size % ADP_MLX_CQE_SIZE)
            return mlx_error(EINVAL);
        for (offset = ADP_MLX_CQE_SIZE - 1; offset < sizeof(staging); offset += ADP_MLX_CQE_SIZE)
            staging[offset] = 0xf0; /* MLX5_CQE_INVALID, owner = 0. */
    }
    if (type == MLX5DV_RES_TYPE_SRQ &&
        (!r->srq_entries || r->srq_entries > (uint32_t)UINT16_MAX + 1 || r->srq_stride < ADP_MLX_SRQ_STRIDE ||
         (r->srq_stride & (r->srq_stride - 1)) || size % r->srq_stride || r->srq_entries > size / r->srq_stride))
        return mlx_error(EINVAL);
    for (offset = 0; offset < size;) {
        size_t chunk = size - offset;
        if (chunk > sizeof(staging))
            chunk = sizeof(staging);
        if (type == MLX5DV_RES_TYPE_SRQ) {
            memset(staging, 0, chunk);
            fill_srq_links(staging, offset, chunk, r->srq_entries, r->srq_stride);
        }
        int ret = r->ops.memcpy_s((void *)(addr + offset), size - offset, staging, chunk, MEMCPY_DIR_HOST_TO_DEVICE);
        if (ret)
            return mlx_error(EIO);
        offset += chunk;
    }
    return 0;
}

void *mlx_alloc(struct ibv_pd *pd, void *opaque, size_t size, size_t alignment, uint64_t type)
{
    struct mlx_resource *r = opaque;
    struct mlx_allocation *a;
    (void)pd;
    static const uint64_t resource_types[] = {
        [MLX_QP] = MLX5DV_RES_TYPE_QP,
        [MLX_CQ] = MLX5DV_RES_TYPE_CQ,
        [MLX_SRQ] = MLX5DV_RES_TYPE_SRQ,
    };
    if (type != MLX5DV_RES_TYPE_DBR && type != resource_types[r->kind]) {
        errno = EOPNOTSUPP;
        return NULL;
    }
    if (!size || !alignment || (alignment & (alignment - 1)) || size > SIZE_MAX - (alignment - 1)) {
        errno = EINVAL;
        return NULL;
    }
    if (r->kind == MLX_QP) {
        /* Queue geometry must already cover whole pages. Padding only the
         * allocation cannot change the kernel's SQ/RQ registration length. */
        if (type == MLX5DV_RES_TYPE_QP && size % ADP_MLX_NPU_PAGE_SIZE) {
            errno = EINVAL;
            return NULL;
        }
        /* libmlx5 requests only 8 bytes for a QP DBR, while mlx5_ib pins its
         * containing Host page. Own and initialize that entire 4 KiB page. */
        if (type == MLX5DV_RES_TYPE_DBR) {
            if (size > SIZE_MAX - (ADP_MLX_NPU_PAGE_SIZE - 1)) {
                errno = EOVERFLOW;
                return NULL;
            }
            size = (size + ADP_MLX_NPU_PAGE_SIZE - 1) & ~(size_t)(ADP_MLX_NPU_PAGE_SIZE - 1);
        }
        if (alignment < ADP_MLX_NPU_PAGE_SIZE)
            alignment = ADP_MLX_NPU_PAGE_SIZE;
        if (size > SIZE_MAX - (alignment - 1)) {
            errno = EOVERFLOW;
            return NULL;
        }
    }
    a = calloc(1, sizeof(*a));
    if (!a)
        return NULL;
    a->raw = r->ops.alloc(size + alignment - 1);
    if (!a->raw) {
        free(a);
        errno = ENOMEM;
        return NULL;
    }
    uintptr_t raw = (uintptr_t)a->raw;
    if (raw > UINTPTR_MAX - (size + alignment - 1)) {
        r->ops.free(a->raw);
        free(a);
        errno = EOVERFLOW;
        return NULL;
    }
    a->addr = (raw + alignment - 1) & ~(uintptr_t)(alignment - 1);
    a->size = size;
    a->type = type;
    int ret = initialize_memory(r, a->addr, size, type);
    if (ret) {
        r->ops.free(a->raw);
        free(a);
        errno = ret;
        return NULL;
    }
    a->next = r->allocations;
    r->allocations = a;
    return (void *)a->addr;
}

void mlx_free(struct ibv_pd *pd, void *opaque, void *ptr, uint64_t type)
{
    struct mlx_resource *r = opaque;
    struct mlx_allocation **link = &r->allocations;
    (void)pd;
    while (*link) {
        struct mlx_allocation *a = *link;
        if (a->addr == (uintptr_t)ptr && a->type == type) {
            *link = a->next;
            r->ops.free(a->raw);
            free(a);
            return;
        }
        link = &a->next;
    }
}

bool mlx_contains(const struct mlx_resource *r, const void *ptr, size_t size, uint64_t type)
{
    uintptr_t p = (uintptr_t)ptr;
    for (const struct mlx_allocation *a = r->allocations; a; a = a->next) {
        if (a->type == type && p >= a->addr && p - a->addr <= a->size && size <= a->size - (p - a->addr))
            return true;
    }
    return false;
}

void mlx_release_memory(struct mlx_resource *r)
{
    /* Only after hardware objects and their parent domain have gone away. */
    while (r->allocations)
        mlx_free(NULL, r, (void *)r->allocations->addr, r->allocations->type);
}
