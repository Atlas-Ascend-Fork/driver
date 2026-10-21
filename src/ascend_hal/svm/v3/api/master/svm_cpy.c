/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <securec.h>

#include "ascend_hal_define.h"
#include "ascend_hal.h"
#include "pbl_uda_user.h"
#include "dpa_apm.h"

#include "svm_pub.h"
#include "svm_addr_desc.h"
#include "svm_log.h"
#include "va_allocator.h"
#include "va_mng.h"
#include "svm_user_adapt.h"
#include "malloc_mng.h"
#include "svm_va_diag.h"
#include "svm_register_to_master.h"
#include "svm_memcpy.h"
#include "svm_memcpy_local_client.h"
#include "svm_dbi.h"
#include "svm_ipc.h"
#include "svm_vmm.h"
#include "svm_cpy_host_pool.h"

#define SVM_D2D_PIPE_CHUNK_SIZE (2ULL * SVM_BYTES_PER_MB) /* 2MB */
#define SVM_D2D_PIPE_DEPTH 2U                             /* pipeline depth */
#define SVM_D2D_PIPE_MIN_SIZE SVM_D2D_PIPE_CHUNK_SIZE     /* pipeline enable threshold */
#define SVM_D2D_PIPE_INVALID_CHAN (-1)                    /* invalid channel id sentinel */

struct svm_d2d_pipe_geom {
    u64 chunk_size;
    u32 total_chunks;
    u64 last_chunk_size;
};

struct svm_d2d_pipe_buf {
    u64 va;
};

struct svm_d2d_pipe_ctx {
    u64 src_va;
    u64 dst_va;
    u64 total_size;
    u32 src_devid;
    u32 dst_devid;
    struct svm_d2d_pipe_geom geom;
    struct svm_d2d_pipe_buf host_buf[SVM_D2D_PIPE_DEPTH]; /* per-slot bounce buffer, rotated by slot */
    int d2h_chan;                                         /* pre-allocated D2H channel on src_devid */
    int h2d_chan;                                         /* pre-allocated H2D channel on dst_devid */
};

enum svm_copy_shared_type {
    SVM_COPY_SHARED_NONE = 0,
    SVM_COPY_SHARED_IPC,
    SVM_COPY_SHARED_VMM,
};

struct svm_copy_shared_info {
    enum svm_copy_shared_type type;
    struct svm_copy_va_info map_info;
    struct svm_copy_va_info real_info;
};

static int svm_mem_sync_copy_h2d_by_pool(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    struct svm_cpy_host_pool_slot slot = {0};
    struct svm_copy_va_info host_svm_info;
    int ret;

    ret = svm_cpy_host_pool_slot_get(src_info->size, &slot);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_memcpy_s((void *)(uintptr_t)slot.va, src_info->size, (void *)(uintptr_t)src_info->va, src_info->size);
    if (ret != 0) {
        svm_err("Memcpy by host svm pool failed. (dst=0x%llx; src=0x%llx; size=%llu; ret=%d)\n", slot.va, src_info->va,
                src_info->size, ret);
        ret = DRV_ERROR_INVALID_VALUE;
        goto out;
    }

    svm_copy_va_info_pack(slot.va, src_info->size, svm_get_host_devid(), &host_svm_info);
    ret = svm_sync_copy(&host_svm_info, dst_info);

out:
    svm_cpy_host_pool_slot_put(&slot);
    return ret;
}

static int svm_mem_sync_copy_d2h_by_pool(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    struct svm_cpy_host_pool_slot slot = {0};
    struct svm_copy_va_info host_svm_info;
    int ret;

    ret = svm_cpy_host_pool_slot_get(dst_info->size, &slot);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_copy_va_info_pack(slot.va, dst_info->size, svm_get_host_devid(), &host_svm_info);
    ret = svm_sync_copy(src_info, &host_svm_info);
    if (ret != DRV_ERROR_NONE) {
        goto out;
    }

    ret = svm_memcpy_s((void *)(uintptr_t)dst_info->va, dst_info->size, (void *)(uintptr_t)slot.va, dst_info->size);
    if (ret != 0) {
        svm_err("Memcpy by host svm pool failed. (dst=0x%llx; src=0x%llx; size=%llu; ret=%d)\n", dst_info->va, slot.va,
                dst_info->size, ret);
        ret = DRV_ERROR_INVALID_VALUE;
    }

out:
    svm_cpy_host_pool_slot_put(&slot);
    return ret;
}

static int svm_copy_check_addr_prop_consistency(struct svm_prop *tar_prop, u64 start, u64 end)
{
    struct svm_prop prop;
    u64 va;
    int ret;

    for (va = start; va < end; va += prop.size) {
        ret = svm_get_prop(va, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Invalid addr. (ret=%d; va=0x%llx)\n", ret, va);
            return DRV_ERROR_PARA_ERROR; /* Same to OBP. */
        }

        if (((prop.flag & ~SVM_FLAG_ATTR_PA_P2P) != (tar_prop->flag & ~SVM_FLAG_ATTR_PA_P2P)) ||
            (prop.devid != tar_prop->devid)) {
            svm_err("Addr's prop is not consistent.\n");
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    return DRV_ERROR_NONE;
}

static int svm_copy_check_addr_prop_cap(u64 va, u64 size, u64 cap_mask)
{
    struct svm_prop prop;
    int va_type;
    int ret;

    ret = svm_get_va_type(va, size, &va_type);
    if (ret != 0) {
        svm_err("Invalid addr. (va=0x%llx; size=0x%llx)\n", va, size);
        return ret;
    }

    if (va_type == VA_TYPE_SVM) {
        ret = svm_get_prop(va, &prop);
        if (ret != DRV_ERROR_NONE) {
            /* The log cannot be modified, because in the failure mode library. */
            svm_err("Invalid addr. (ret=%d; va=0x%llx)\n", ret, va);
            return ret;
        }

        if ((prop.flag & cap_mask) == 0) {
            svm_err("Va not support cur cap. (va=0x%llx; prop.flag=0x%llx)\n", va, prop.flag);
            return DRV_ERROR_PARA_ERROR;
        }

        if ((va + size) > (prop.start + prop.size)) {
            ret = svm_copy_check_addr_prop_consistency(&prop, prop.start + prop.size, va + size);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
        }
    }

    return DRV_ERROR_NONE;
}

/* Should check va prop consistency firstly. */
static int svm_set_copy_va_info(u64 va, u64 size, struct svm_copy_va_info *info)
{
    struct svm_prop prop;
    int ret;

    info->va = va;
    info->size = size;
    info->host_tgid = 0;
    info->is_share = false;

    if (svm_va_is_in_range(va, size)) {
        ret = svm_get_prop(va, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Invalid addr. (ret=%d; va=0x%llx)\n", ret, va);
            return ret;
        }

        info->devid = prop.devid;
    } else {
        info->devid = svm_get_host_devid();
    }

    return DRV_ERROR_NONE;
}

static bool svm_copy_shared_info_is_valid(const struct svm_copy_shared_info *info)
{
    return (info->type != SVM_COPY_SHARED_NONE);
}

static bool svm_copy_need_try_resolve_shared_info(u32 devid)
{
    /* tmp for pcie, ub adapt later */
    return ((devid != svm_get_host_devid()) && (svm_get_device_connect_type(devid) == HOST_DEVICE_CONNECT_TYPE_PCIE));
}

static int svm_copy_real_info_pack(const struct svm_global_va *src_info, struct svm_copy_va_info *info)
{
    u32 host_tgid, proc_type;
    int ret;

    if ((src_info->server_id != SVM_INVALID_SERVER_ID) && (src_info->server_id != svm_get_cur_server_id())) {
        svm_info("Cross server shared memcpy is not supported. (server_id=%u; cur_server_id=%u)\n", src_info->server_id,
                 svm_get_cur_server_id());
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = uda_get_devid_by_udevid_ex(src_info->udevid, &info->devid);
    if (ret != 0) {
        svm_err("Get devid by udevid failed. (ret=%d; udevid=%u)\n", ret, src_info->udevid);
        return ret;
    }

    if (info->devid == svm_get_host_devid()) {
        host_tgid = (u32)src_info->tgid;
    } else {
        ret = halQueryMasterTgidByDeviceSlave(info->devid, src_info->tgid, &host_tgid, &proc_type);
        if (ret != 0) {
            svm_err("Get master tgid by device slave failed. (ret=%d; devid=%u; slave_tgid=%d)\n", ret, info->devid,
                    src_info->tgid);
            return ret;
        }
    }

    info->host_tgid = (int)host_tgid;
    info->va = src_info->va;
    info->size = src_info->size;
    info->is_share = true;
    return DRV_ERROR_NONE;
}

static int svm_copy_try_query_shared_info(struct svm_copy_va_info *info, struct svm_copy_shared_info *shared)
{
    struct svm_global_va src_info;
    enum svm_copy_shared_type type;
    int ret;

    shared->type = SVM_COPY_SHARED_NONE;
    shared->map_info = *info;
    shared->real_info = *info;

    ret = svm_ipc_query_src_info(info->va, info->size, &src_info);
    if (ret == DRV_ERROR_NONE) {
        type = SVM_COPY_SHARED_IPC;
    } else {
        ret = vmm_query_ipc_src_info(info->va, info->size, &src_info);
        if (ret != DRV_ERROR_NONE) {
            return DRV_ERROR_NONE; /* Non shared addr, just return ok to continue copy. */
        }
        type = SVM_COPY_SHARED_VMM;
    }

    ret = svm_copy_real_info_pack(&src_info, &shared->real_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    shared->type = type;

    return DRV_ERROR_NONE;
}

static int svm_copy_try_resolve_shared_info(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info,
                                            struct svm_copy_va_info *real_src, struct svm_copy_va_info *real_dst,
                                            bool *resolved)
{
    struct svm_copy_shared_info src_shared, dst_shared;
    bool src_is_shared, dst_is_shared;
    int ret;

    ret = svm_copy_try_query_shared_info(src_info, &src_shared);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_copy_try_query_shared_info(dst_info, &dst_shared);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    src_is_shared = svm_copy_shared_info_is_valid(&src_shared);
    dst_is_shared = svm_copy_shared_info_is_valid(&dst_shared);
    if (!src_is_shared && !dst_is_shared) {
        *resolved = false;
        return DRV_ERROR_NONE;
    }

    if (src_is_shared && dst_is_shared) {
        svm_info("Shared src and dst are not supported. (src=0x%llx; dst=0x%llx)\n", src_info->va, dst_info->va);
        return DRV_ERROR_NOT_SUPPORT;
    }

    *real_src = src_is_shared ? src_shared.real_info : *src_info;
    *real_dst = dst_is_shared ? dst_shared.real_info : *dst_info;
    real_src->is_share = src_is_shared;
    real_dst->is_share = dst_is_shared;
    *resolved = true;
    return DRV_ERROR_NONE;
}

static int svm_mem_sync_copy_local_resolved(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    int cur_tgid = (int)drvDeviceGetBareTgid();

    if (((src_info->host_tgid != 0) && (src_info->host_tgid != cur_tgid)) ||
        ((dst_info->host_tgid != 0) && (dst_info->host_tgid != cur_tgid))) {
        svm_info(
            "Resolved local copy tgid not match current process. (src_host_tgid=%d; dst_host_tgid=%d; cur_tgid=%d)\n",
            src_info->host_tgid, dst_info->host_tgid, cur_tgid);
        return DRV_ERROR_NOT_SUPPORT; /* todo: later adapt */
    }

    return svm_memcpy_local_client(dst_info->devid, dst_info->va, dst_info->size, src_info->va, src_info->size);
}

/* Should check va prop consistency firstly. */
static int svm_set_copy_va_2d_info(u64 va, u64 pitch, u64 width, u64 height, struct svm_copy_va_2d_info *info)
{
    struct svm_prop prop;
    int ret;

    info->va = va;
    info->pitch = pitch;
    info->width = width;
    info->height = height;

    if (svm_va_is_in_range(va, pitch * height)) {
        ret = svm_get_prop(va, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Invalid addr. (ret=%d; va=0x%llx)\n", ret, va);
            return ret;
        }

        info->devid = prop.devid;
    } else {
        info->devid = svm_get_host_devid();
    }

    return DRV_ERROR_NONE;
}
#ifdef CFG_FEATURE_ASYNC_COPY
static int svm_async_copy_para_check(u64 dst, u64 dst_max, u64 src, u64 size, u64 *handle)
{
    int ret;

    if (src == 0) {
        svm_err("src is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dst == 0) {
        svm_err("dst is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dst_max == 0) {
        svm_err("dst_max is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (size == 0) {
        svm_err("cnt is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dst_max < size) {
        svm_err("Dst_max less than size. (dst_max=%llu; size=%llu)\n", dst_max, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle == NULL) {
        svm_err("handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_copy_check_addr_prop_cap(dst, size, SVM_FLAG_CAP_ASYNC_COPY_SUBMIT);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return svm_copy_check_addr_prop_cap(src, size, SVM_FLAG_CAP_ASYNC_COPY_SUBMIT);
}
#endif
DVresult halMemCpyAsync(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count, uint64_t *copy_fd)
{
#ifdef CFG_FEATURE_ASYNC_COPY
    struct svm_copy_va_info src_info, dst_info;
    struct svm_copy_va_info real_src, real_dst;
    bool resolved = false;
    enum svm_cpy_dir dir;
    int ret;

    ret = svm_async_copy_para_check(dst, dest_max, src, byte_count, (u64 *)copy_fd);
    if (ret != 0) {
        return ret;
    }

    ret = svm_set_copy_va_info((u64)src, (u64)byte_count, &src_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_set_copy_va_info((u64)dst, (u64)byte_count, &dst_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_copy_try_resolve_shared_info(&src_info, &dst_info, &real_src, &real_dst, &resolved);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (resolved) {
        svm_info("Async memcpy does not support shared addr. (src=0x%llx; dst=0x%llx; size=%llu)\n", src_info.va,
                 dst_info.va, src_info.size);
        return DRV_ERROR_NOT_SUPPORT;
    }

    dir = copy_dir_get_by_devid(src_info.devid, dst_info.devid);
    if (dir == SVM_H2H_CPY) {
        svm_err("Invalid input, no support H2H async memcpy.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    return svm_async_copy_submit(&src_info, &dst_info, (u64 *)copy_fd);
#else
    SVM_UNUSED(dst);
    SVM_UNUSED(dest_max);
    SVM_UNUSED(src);
    SVM_UNUSED(byte_count);
    SVM_UNUSED(copy_fd);

    return DRV_ERROR_NOT_SUPPORT;
#endif
}

DVresult halMemCpyAsyncWaitFinish(uint64_t copy_fd)
{
#ifdef CFG_FEATURE_ASYNC_COPY
    return svm_async_copy_wait((u64)copy_fd);
#else
    SVM_UNUSED(copy_fd);

    return DRV_ERROR_NOT_SUPPORT;
#endif
}

static int svm_mem_sync_copy_h2h(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    return svm_memcpy_local_client(dst_info->devid, dst_info->va, dst_info->size, src_info->va, src_info->size);
}

static int svm_mem_sync_copy_h2d(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    int ret;

    if (svm_va_is_in_range(src_info->va, src_info->size) == false) {
        ret = svm_mem_sync_copy_h2d_by_pool(src_info, dst_info);
        if ((ret != DRV_ERROR_BUSY) && (ret != DRV_ERROR_NOT_SUPPORT)) {
            return ret;
        }
    }

    return svm_sync_copy(src_info, dst_info);
}

static int svm_mem_sync_copy_d2h(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    int ret;

    if (svm_va_is_in_range(dst_info->va, dst_info->size) == false) {
        ret = svm_mem_sync_copy_d2h_by_pool(src_info, dst_info);
        if ((ret != DRV_ERROR_BUSY) && (ret != DRV_ERROR_NOT_SUPPORT)) {
            return ret;
        }
    }

    return svm_sync_copy(src_info, dst_info);
}

static int svm_mem_sync_copy_same_d2d(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    return svm_memcpy_local_client(dst_info->devid, dst_info->va, dst_info->size, src_info->va, src_info->size);
}

#define SVM_D2D_SYNC_CHUNK_SIZE (16ULL * SVM_BYTES_PER_MB) /* 16MB per-chunk for serial d2d */

static int svm_sync_copy_diff_d2d(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    u64 chunk_size = SVM_D2D_SYNC_CHUNK_SIZE;
    struct svm_copy_va_info host_info;
    u64 remaining = src_info->size;
    u64 offset = 0;
    void *host_va = NULL;
    int ret;

    ret = halMemAlloc(&host_va, chunk_size, MEM_HOST | ((u64)DEVMM_MODULE_ID << MEM_MODULE_ID_BIT));
    if (ret != DRV_ERROR_NONE) {
        svm_warn("Alloc d2d serial host buf not success. (ret=%d; size=%llu)\n", ret, chunk_size);
        return ret;
    }

    host_info.devid = svm_get_host_devid();
    host_info.va = (u64)(uintptr_t)host_va;
    host_info.host_tgid = 0;
    host_info.is_share = false;

    while (remaining > 0) {
        u64 sz = (remaining < chunk_size) ? remaining : chunk_size;
        struct svm_copy_va_info src_chunk;
        struct svm_copy_va_info dst_chunk;

        svm_copy_va_info_pack(src_info->va + offset, sz, src_info->devid, &src_chunk);
        svm_copy_va_info_pack(dst_info->va + offset, sz, dst_info->devid, &dst_chunk);
        host_info.size = sz;

        ret = svm_mem_sync_copy_d2h(&src_chunk, &host_info);
        if (ret != DRV_ERROR_NONE) {
            svm_err("D2H serial chunk failed. (ret=%d; offset=%llu; size=%llu)\n", ret, offset, sz);
            goto free_host_va;
        }

        ret = svm_mem_sync_copy_h2d(&host_info, &dst_chunk);
        if (ret != DRV_ERROR_NONE) {
            svm_err("H2D serial chunk failed. (ret=%d; offset=%llu; size=%llu)\n", ret, offset, sz);
            goto free_host_va;
        }

        offset += sz;
        remaining -= sz;
    }

free_host_va:
    (void)halMemFree(host_va);
    return ret;
}

static u64 svm_d2d_pipe_chunk_size(const struct svm_d2d_pipe_geom *geom, u32 chunk_idx)
{
    return (chunk_idx == geom->total_chunks - 1) ? geom->last_chunk_size : geom->chunk_size;
}

static int svm_d2d_pipe_alloc_host_buf(struct svm_d2d_pipe_ctx *ctx)
{
    void *slot_ptr = NULL;
    u32 i, j;
    int ret;

    for (i = 0; i < SVM_D2D_PIPE_DEPTH; i++) {
        slot_ptr = NULL;
        ret = halMemAlloc(&slot_ptr, ctx->geom.chunk_size, MEM_HOST | ((u64)DEVMM_MODULE_ID << MEM_MODULE_ID_BIT));
        if (ret != DRV_ERROR_NONE) {
            svm_warn("Alloc d2d pipe host buf slot not success. (ret=%d; slot=%u; size=%llu)\n", ret, i,
                     ctx->geom.chunk_size);
            for (j = 0; j < i; j++) {
                (void)halMemFree((void *)(uintptr_t)ctx->host_buf[j].va);
                ctx->host_buf[j].va = 0;
            }
            return ret;
        }
        ctx->host_buf[i].va = (u64)(uintptr_t)slot_ptr;
    }
    return DRV_ERROR_NONE;
}

static void svm_d2d_pipe_free_host_buf(struct svm_d2d_pipe_ctx *ctx)
{
    u32 i;

    for (i = 0; i < SVM_D2D_PIPE_DEPTH; i++) {
        if (ctx->host_buf[i].va != 0) {
            (void)halMemFree((void *)(uintptr_t)ctx->host_buf[i].va);
            ctx->host_buf[i].va = 0;
        }
    }
}

static int svm_d2d_pipe_init(struct svm_d2d_pipe_ctx *ctx, struct svm_copy_va_info *src_info,
                             struct svm_copy_va_info *dst_info)
{
    int ret;

    ctx->src_va = src_info->va;
    ctx->dst_va = dst_info->va;
    ctx->total_size = src_info->size;
    ctx->src_devid = src_info->devid;
    ctx->dst_devid = dst_info->devid;
    ctx->geom.chunk_size = SVM_D2D_PIPE_CHUNK_SIZE;
    ctx->geom.total_chunks = (u32)((ctx->total_size + ctx->geom.chunk_size - 1) / ctx->geom.chunk_size);
    ctx->geom.last_chunk_size = ctx->total_size - (u64)(ctx->geom.total_chunks - 1) * ctx->geom.chunk_size;
    ctx->d2h_chan = SVM_D2D_PIPE_INVALID_CHAN;
    ctx->h2d_chan = SVM_D2D_PIPE_INVALID_CHAN;

    ret = svm_d2d_pipe_alloc_host_buf(ctx);
    if (ret != DRV_ERROR_NONE) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    /* Allocate D2H channel on src_devid, then H2D on dst_devid. Each device pool
     * holds at most 1 channel (no same-pool deadlock). Reverse-direction AB-BA
     * is mitigated by svm_urma_chan_try_alloc 5s timeout -> serial fallback. */
    ret = svm_async_chan_alloc(ctx->src_devid, &ctx->d2h_chan);
    if (ret != DRV_ERROR_NONE) {
        svm_warn("Alloc d2h chan not success. (ret=%d; devid=%u)\n", ret, ctx->src_devid);
        ctx->d2h_chan = SVM_D2D_PIPE_INVALID_CHAN;
        svm_d2d_pipe_free_host_buf(ctx);
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = svm_async_chan_alloc(ctx->dst_devid, &ctx->h2d_chan);
    if (ret != DRV_ERROR_NONE) {
        svm_warn("Alloc h2d chan not success. (ret=%d; devid=%u)\n", ret, ctx->dst_devid);
        svm_async_chan_free(ctx->src_devid, ctx->d2h_chan);
        ctx->d2h_chan = SVM_D2D_PIPE_INVALID_CHAN;
        ctx->h2d_chan = SVM_D2D_PIPE_INVALID_CHAN;
        svm_d2d_pipe_free_host_buf(ctx);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

static void svm_d2d_pipe_fini(struct svm_d2d_pipe_ctx *ctx)
{
    if (ctx->d2h_chan != SVM_D2D_PIPE_INVALID_CHAN) {
        svm_async_chan_free(ctx->src_devid, ctx->d2h_chan);
        ctx->d2h_chan = SVM_D2D_PIPE_INVALID_CHAN;
    }
    if (ctx->h2d_chan != SVM_D2D_PIPE_INVALID_CHAN) {
        svm_async_chan_free(ctx->dst_devid, ctx->h2d_chan);
        ctx->h2d_chan = SVM_D2D_PIPE_INVALID_CHAN;
    }

    svm_d2d_pipe_free_host_buf(ctx);
}

static int svm_d2d_pipe_submit_d2h(struct svm_d2d_pipe_ctx *ctx, u32 chunk_idx, u32 slot)
{
    struct svm_copy_va_info src;
    struct svm_copy_va_info host;

    src.va = ctx->src_va + (u64)chunk_idx * ctx->geom.chunk_size;
    src.size = svm_d2d_pipe_chunk_size(&ctx->geom, chunk_idx);
    src.devid = ctx->src_devid;
    src.host_tgid = 0;
    src.is_share = false;

    host.va = ctx->host_buf[slot].va;
    host.size = src.size;
    host.devid = svm_get_host_devid();
    host.host_tgid = 0;
    host.is_share = false;

    return svm_async_copy_submit_reuse(ctx->src_devid, ctx->d2h_chan, &src, &host);
}

static int svm_d2d_pipe_submit_h2d(struct svm_d2d_pipe_ctx *ctx, u32 chunk_idx, u32 slot)
{
    struct svm_copy_va_info host;
    struct svm_copy_va_info dst;

    host.va = ctx->host_buf[slot].va;
    host.size = svm_d2d_pipe_chunk_size(&ctx->geom, chunk_idx);
    host.devid = svm_get_host_devid();
    host.host_tgid = 0;
    host.is_share = false;

    dst.va = ctx->dst_va + (u64)chunk_idx * ctx->geom.chunk_size;
    dst.size = host.size;
    dst.devid = ctx->dst_devid;
    dst.host_tgid = 0;
    dst.is_share = false;

    return svm_async_copy_submit_reuse(ctx->dst_devid, ctx->h2d_chan, &host, &dst);
}

static int svm_d2d_pipe_wait_d2h(struct svm_d2d_pipe_ctx *ctx)
{
    return svm_async_copy_wait_one(ctx->src_devid, ctx->d2h_chan);
}

static int svm_d2d_pipe_wait_h2d(struct svm_d2d_pipe_ctx *ctx)
{
    return svm_async_copy_wait_one(ctx->dst_devid, ctx->h2d_chan);
}

/* Phase 2: Fill D2H — submit fill chunks without waiting. */
static int svm_d2d_pipe_fill_d2h(struct svm_d2d_pipe_ctx *ctx, u32 fill)
{
    u32 chunk;
    int ret;

    for (chunk = 0; chunk < fill; chunk++) {
        ret = svm_d2d_pipe_submit_d2h(ctx, chunk, chunk);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }
    return DRV_ERROR_NONE;
}

/* Phase 3: Drain D2H one by one and fill H2D for the same chunk. */
static int svm_d2d_pipe_drain_d2h_fill_h2d(struct svm_d2d_pipe_ctx *ctx, u32 fill)
{
    u32 chunk;
    int ret;

    for (chunk = 0; chunk < fill; chunk++) {
        ret = svm_d2d_pipe_wait_d2h(ctx);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
        ret = svm_d2d_pipe_submit_h2d(ctx, chunk, chunk);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }
    return DRV_ERROR_NONE;
}

/* Phase 4: Steady state — recycle buf[slot%DEPTH] for D2H(slot+DEPTH). */
static int svm_d2d_pipe_steady(struct svm_d2d_pipe_ctx *ctx)
{
    u32 slot;
    int ret;

    for (slot = 0; (slot + SVM_D2D_PIPE_DEPTH) < ctx->geom.total_chunks; slot++) {
        u32 next = slot + SVM_D2D_PIPE_DEPTH;
        u32 s = slot % SVM_D2D_PIPE_DEPTH;

        ret = svm_d2d_pipe_wait_h2d(ctx); /* release buf[s] */
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
        ret = svm_d2d_pipe_submit_d2h(ctx, next, s);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
        ret = svm_d2d_pipe_wait_d2h(ctx); /* 1 H2D in flight on dst_dev meanwhile */
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
        ret = svm_d2d_pipe_submit_h2d(ctx, next, s);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }
    return DRV_ERROR_NONE;
}

/* Phase 5: Drain remaining in-flight H2D. */
static int svm_d2d_pipe_drain_h2d(struct svm_d2d_pipe_ctx *ctx, u32 fill)
{
    u32 chunk;
    int ret;

    for (chunk = 0; chunk < fill; chunk++) {
        ret = svm_d2d_pipe_wait_h2d(ctx);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }
    return DRV_ERROR_NONE;
}

static int svm_d2d_pipe_run(struct svm_d2d_pipe_ctx *ctx)
{
    u32 fill = (ctx->geom.total_chunks < SVM_D2D_PIPE_DEPTH) ? ctx->geom.total_chunks : SVM_D2D_PIPE_DEPTH;
    int ret;

    /* Phase 2: Fill D2H. */
    ret = svm_d2d_pipe_fill_d2h(ctx, fill);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    /* Phase 3: Drain D2H + fill H2D. */
    ret = svm_d2d_pipe_drain_d2h_fill_h2d(ctx, fill);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    /* Phase 4: Steady state. */
    ret = svm_d2d_pipe_steady(ctx);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    /* Phase 5: Drain remaining H2D. */
    return svm_d2d_pipe_drain_h2d(ctx, fill);
}

static int svm_d2d_pipe_copy(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    struct svm_d2d_pipe_ctx ctx = {0};
    int ret;

    ret = svm_d2d_pipe_init(&ctx, src_info, dst_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_d2d_pipe_run(&ctx);
    svm_d2d_pipe_fini(&ctx);
    return ret;
}

static int svm_mem_sync_copy_diff_d2d(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    u32 hd_connect_type = svm_get_device_connect_type(src_info->devid);
    if ((hd_connect_type == HOST_DEVICE_CONNECT_TYPE_PCIE) || (hd_connect_type == HOST_DEVICE_CONNECT_TYPE_HCCS)) {
        return svm_sync_copy(src_info, dst_info);
    } else if (hd_connect_type == HOST_DEVICE_CONNECT_TYPE_UB) {
        if (src_info->size > SVM_D2D_PIPE_MIN_SIZE) {
            int ret = svm_d2d_pipe_copy(src_info, dst_info);
            if (ret != DRV_ERROR_NOT_SUPPORT) {
                return ret;
            }
            /* tseg lookup or resource alloc failed, fallback to serial path */
        }
        return svm_sync_copy_diff_d2d(src_info, dst_info);
    } else {
        svm_err("Unknown hd connect type. (devid=%u; connect_type=%u)\n", src_info->devid, hd_connect_type);
        return DRV_ERROR_INNER_ERR;
    }
}

static int svm_mem_sync_copy_d2d(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    if (src_info->devid == dst_info->devid) {
        return svm_mem_sync_copy_same_d2d(src_info, dst_info);
    } else {
        return svm_mem_sync_copy_diff_d2d(src_info, dst_info);
    }
}

static int (*const g_sync_copy[SVM_MAX_CPY_DIR])(struct svm_copy_va_info *src_info,
                                                 struct svm_copy_va_info *dst_info) = {
    [SVM_H2H_CPY] = svm_mem_sync_copy_h2h,
    [SVM_H2D_CPY] = svm_mem_sync_copy_h2d,
    [SVM_D2H_CPY] = svm_mem_sync_copy_d2h,
    [SVM_D2D_CPY] = svm_mem_sync_copy_d2d};

int svm_mem_sync_copy(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_info->devid, dst_info->devid);
    if (g_sync_copy[dir] == NULL) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_sync_copy[dir](src_info, dst_info);
}

static int svm_memcpy_sync(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    struct svm_copy_va_info real_src, real_dst;
    enum svm_cpy_dir dir;
    bool resolved = false;
    u32 exec_devid;
    int ret;

    dir = copy_dir_get_by_devid(src_info->devid, dst_info->devid);
    if ((dir == SVM_H2H_CPY) || ((dir == SVM_D2D_CPY) && (src_info->devid == dst_info->devid))) {
        return svm_mem_sync_copy(src_info, dst_info);
    }

    exec_devid = (dir == SVM_H2D_CPY) ? dst_info->devid : src_info->devid;
    if (svm_copy_need_try_resolve_shared_info(exec_devid)) {
        ret = svm_copy_try_resolve_shared_info(src_info, dst_info, &real_src, &real_dst, &resolved);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }

        if (resolved) {
            dir = copy_dir_get_by_devid(real_src.devid, real_dst.devid);
            if ((dir == SVM_H2H_CPY) || ((dir == SVM_D2D_CPY) && (real_src.devid == real_dst.devid))) {
                return svm_mem_sync_copy_local_resolved(&real_src, &real_dst);
            }
            *src_info = real_src;
            *dst_info = real_dst;
        }
    }

    return svm_mem_sync_copy(src_info, dst_info);
}

DVresult drvMemcpyInner(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count);
DVresult drvMemcpyInner(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count)
{
    struct svm_copy_va_info src_info, dst_info;
    int ret;

    if ((dst == 0) || (src == 0) || (dest_max == 0) || (dest_max < byte_count)) {
        svm_err("Invalid argument. (dst=0x%llx; src=0x%llx; byte_count=%lu; dest_max=%lu)\n", dst, src, byte_count,
                dest_max);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_debug("Memcpy. (dst=0x%llx; src=0x%llx; count=%llu)\n", dst, src, (u64)byte_count);

    /* Similar to memcpy_s, (byte_count == 0) need to return 0 */
    if (byte_count == 0) {
        return DRV_ERROR_NONE;
    }

    ret = svm_copy_check_addr_prop_cap(dst, byte_count, SVM_FLAG_CAP_SYNC_COPY);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_copy_check_addr_prop_cap(src, byte_count, SVM_FLAG_CAP_SYNC_COPY);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_set_copy_va_info((u64)src, (u64)byte_count, &src_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_set_copy_va_info((u64)dst, (u64)byte_count, &dst_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return svm_memcpy_sync(&src_info, &dst_info);
}

DVresult drvMemcpy(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count)
{
    int ret;

    ret = drvMemcpyInner(dst, dest_max, src, byte_count);
    if (ret != DRV_ERROR_NONE) {
        /*
         * Both src and dst addresses are diagnosed on failure.
         * drvMemcpyInner checks dst first (via svm_copy_check_addr_prop_cap),
         * then src. The inner function returns a generic error code, so the
         * exact failed side is not distinguishable here. Tags "drvMemcpySrcAddr"
         * and "drvMemcpyDstAddr" are used to identify each dump.
         */
        svm_diag_dump_va_prop((u64)src, (u64)byte_count, "drvMemcpySrcAddr");
        svm_diag_dump_va_prop((u64)dst, (u64)dest_max, "drvMemcpyDstAddr");
    }

    return ret;
}

static int svm_mem_cpy_para_check(u64 dst, u64 dst_size, u64 src, u64 count, struct memcpy_info *info)
{
    int ret;

    if (dst == 0) {
        svm_err("Dst is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (src == 0) {
        svm_err("Src is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dst_size == 0) {
        svm_err("Dst size is zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dst_size < count) {
        svm_err("Dst size larger than cpy size. (dst_size=%lu; count=%lu)\n", dst_size, count);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* for halMemcpy */
    if (info != NULL) {
        if (info->dir != DRV_MEMCPY_DEVICE_TO_HOST) {
            svm_info("Invalid dir type. (dir_type=%u)\n", info->dir);
            return DRV_ERROR_NOT_SUPPORT;
        }

        if (info->devid >= SVM_MAX_AGENT_NUM) {
            svm_err("Invalid devid. (devid=%u)\n", info->devid);
            return DRV_ERROR_INVALID_VALUE;
        }

        if (!svm_va_is_in_range(src, count) && !va_is_in_sp_range(src, count)) {
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    ret = svm_copy_check_addr_prop_cap(dst, count, SVM_FLAG_CAP_SYNC_COPY_EX);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return svm_copy_check_addr_prop_cap(src, count, SVM_FLAG_CAP_SYNC_COPY_EX);
}

static drvError_t halMemcpyInner(void *dst, size_t dst_size, void *src, size_t count, struct memcpy_info *info)
{
    struct svm_copy_va_info src_info, dst_info;
    enum svm_cpy_dir dir;
    drvError_t ret;

    svm_debug("Memcpy. (dst=0x%llx; src=0x%llx; count=%llu)\n", (u64)(uintptr_t)dst, (u64)(uintptr_t)src, (u64)count);

    /* Similar to memcpy_s, (count == 0) need to return 0 */
    if (count == 0) {
        return DRV_ERROR_NONE;
    }

    ret = svm_mem_cpy_para_check((u64)dst, (u64)dst_size, (u64)src, (u64)count, info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (va_is_in_sp_range((u64)src, (u64)count)) {
        svm_copy_va_info_pack((u64)src, (u64)count, info->devid, &src_info);
    } else {
        ret = svm_set_copy_va_info((u64)src, (u64)count, &src_info);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }

    ret = svm_set_copy_va_info((u64)dst, (u64)count, &dst_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (info == NULL) {
        return svm_mem_sync_copy(&src_info, &dst_info);
    } else {
        /* D2H copy sp addr. */
        dir = copy_dir_get_by_devid(src_info.devid, dst_info.devid);
        if (dir != (enum svm_cpy_dir)info->dir) {
            svm_err("Invalid addr dir. (dir=%d; info.dir=%d)\n", dir, info->dir);
            return DRV_ERROR_INVALID_VALUE;
        }

        return svm_mem_sync_copy_d2h(&src_info, &dst_info);
    }
}

drvError_t halMemcpy(void *dst, size_t dst_size, void *src, size_t count, struct memcpy_info *info)
{
    drvError_t ret;

    ret = halMemcpyInner(dst, dst_size, src, count, info);
    if (ret != DRV_ERROR_NONE) {
        /*
         * Diagnose both src and dst addresses for failure analysis.
         * The inner function (halMemcpyInner -> svm_mem_cpy_para_check)
         * validates dst first (via svm_copy_check_addr_prop_cap), then
         * src. Multiple paths can fail (param check, addr prop, mem alloc,
         * sync copy), making it impossible to pinpoint the exact failed
         * side without changing the inner interface. Both addresses are
         * dumped here, with "SrcAddr"/"DstAddr" tags to distinguish them.
         */
        svm_diag_dump_va_prop((u64)src, (u64)count, "halMemcpySrcAddr");
        svm_diag_dump_va_prop((u64)dst, (u64)dst_size, "halMemcpyDstAddr");
    }

    return ret;
}

static drvError_t svm_dma_desc_convert_para_check(u64 src, u64 dst, u64 size, struct DMA_ADDR *dma_desc)
{
    int ret;

    if (src == 0) {
        svm_err("src is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dst == 0) {
        svm_err("dst is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (size == 0) {
        svm_err("size is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dma_desc == NULL) {
        svm_err("DmaAddr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dma_desc->offsetAddr.devid >= SVM_MAX_AGENT_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", dma_desc->offsetAddr.devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_copy_check_addr_prop_cap(dst, size, SVM_FLAG_CAP_DMA_DESC_CONVERT);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return svm_copy_check_addr_prop_cap(src, size, SVM_FLAG_CAP_DMA_DESC_CONVERT);
}

static int svm_dma_desc_dir_check(u32 exec_devid, u32 src_devid, u32 dst_devid)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_devid, dst_devid);
    int ret = DRV_ERROR_PARA_ERROR;

    if (dir == SVM_H2H_CPY) {
        svm_err("Invalid input, no support H2H convert.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if ((dir == SVM_D2D_CPY) && (src_devid == dst_devid)) {
        svm_err("Invalid input, Src and dst are same device. (devid=%d)\n", src_devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (dir == SVM_H2D_CPY) {
        ret = (exec_devid != dst_devid) ? DRV_ERROR_INVALID_VALUE : 0;
    } else if (dir == SVM_D2H_CPY) {
        ret = (exec_devid != src_devid) ? DRV_ERROR_INVALID_VALUE : 0;
    } else if (dir == SVM_D2D_CPY) {
        ret = ((exec_devid != src_devid) && (exec_devid != dst_devid)) ? DRV_ERROR_INVALID_VALUE : 0;
    }
    if (ret != DRV_ERROR_NONE) {
        svm_err("Exec_devid does not match the direction. (exec_devid=%u; src_devid=%u; dst_devid=%u; dir=%d)\n",
                exec_devid, src_devid, dst_devid, dir);
        return ret;
    }

    return DRV_ERROR_NONE;
}

DVresult drvMemConvertAddr(DVdeviceptr p_src, DVdeviceptr p_dst, UINT32 len, struct DMA_ADDR *dma_addr)
{
    struct svm_copy_va_info src_info, dst_info;
    struct svm_copy_va_info real_src, real_dst;
    bool resolved = false;
    int ret;

    ret = svm_dma_desc_convert_para_check(p_src, p_dst, len, dma_addr);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_set_copy_va_info((u64)p_src, (u64)len, &src_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_set_copy_va_info((u64)p_dst, (u64)len, &dst_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (svm_copy_need_try_resolve_shared_info(dma_addr->offsetAddr.devid)) {
        ret = svm_copy_try_resolve_shared_info(&src_info, &dst_info, &real_src, &real_dst, &resolved);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }

        if (resolved) {
            src_info = real_src;
            dst_info = real_dst;
        }
    }

    ret = svm_dma_desc_dir_check(dma_addr->offsetAddr.devid, src_info.devid, dst_info.devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_dma_desc_convert(&src_info, &dst_info, dma_addr);

    svm_debug("Convert. (p_src=0x%llx; p_dst=0x%llx; len=%u; dma_addr=0x%llx)\n", p_src, p_dst, len,
              (u64)(uintptr_t)dma_addr);

    return ret;
}

static drvError_t svm_dma_desc_para_check(struct DMA_ADDR *dma_desc[], uint32_t num)
{
    uint32_t i;

    if (dma_desc == NULL) {
        svm_err("Ptr[] is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (num == 0) {
        svm_err("num is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < num; i++) {
        if (dma_desc[i] == NULL) {
            svm_err("Ptr is NULL.\n");
            return DRV_ERROR_INVALID_VALUE;
        }

        /* host agent not support convert */
        if (dma_desc[i]->virt_id >= SVM_MAX_DEV_AGENT_NUM) {
            svm_err("Virt_id is out of range. (virt_id=%u; range=0-%u)\n", dma_desc[i]->virt_id,
                    (UINT32)SVM_MAX_DEV_AGENT_NUM);
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    return DRV_ERROR_NONE;
}

DVresult drvMemDestroyAddr(struct DMA_ADDR *ptr)
{
    int ret;

    svm_debug("Destroy. (ptr=0x%llx)\n", (u64)(uintptr_t)ptr);

    ret = svm_dma_desc_para_check(&ptr, 1);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return svm_dma_desc_destroy(ptr);
}

DVresult halMemDestroyAddrBatch(struct DMA_ADDR *ptr[], uint32_t num)
{
    int ret = DRV_ERROR_NONE;
    u32 i;

    svm_debug("MemDestroyAddrBatch enter. (num=%u)\n", num);

    ret = svm_dma_desc_para_check(ptr, num);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    for (i = 0; i < num; i++) {
        ret = drvMemDestroyAddr(ptr[i]);
        if (ret != DRV_ERROR_NONE) {
            break;
        }
    }

    return ret;
}

DVresult halMemcpySumbit(struct DMA_ADDR *dma_addr, int flag)
{
#ifdef CFG_FEATURE_DMA_DESC_SUBMIT
    int ret;

    ret = svm_dma_desc_para_check(&dma_addr, 1);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if ((flag >= (int)MEMCPY_SUMBIT_MAX_TYPE) || (flag < (int)MEMCPY_SUMBIT_SYNC)) {
        svm_err("Invalid flag. (flag=%d)\n", flag);
        return DRV_ERROR_INVALID_VALUE;
    }

    return svm_dma_desc_submit(dma_addr, flag);
#else
    SVM_UNUSED(dma_addr);
    SVM_UNUSED(flag);

    return DRV_ERROR_NOT_SUPPORT;
#endif
}

DVresult halMemcpyWait(struct DMA_ADDR *dma_addr)
{
#ifdef CFG_FEATURE_DMA_DESC_SUBMIT
    int ret;

    ret = svm_dma_desc_para_check(&dma_addr, 1);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return svm_dma_desc_wait(dma_addr);
#else
    SVM_UNUSED(dma_addr);

    return DRV_ERROR_NOT_SUPPORT;
#endif
}

static drvError_t svm_2d_sync_copy(struct drvMem2D *sync_2d)
{
    struct svm_copy_va_2d_info src_info, dst_info;
    enum svm_cpy_dir dir;
    int ret;

    svm_debug(
        "2D sync copy. (src=0x%llx; dst=0x%llx; spitch=%llu; dpitch=%llu; width=%llu; height=%llu; direction=%u)\n",
        sync_2d->src, sync_2d->dst, sync_2d->spitch, sync_2d->dpitch, sync_2d->width, sync_2d->height,
        sync_2d->direction);
    ret = svm_copy_check_addr_prop_cap((u64)(uintptr_t)sync_2d->src,
                                       sync_2d->spitch * (sync_2d->height - 1) + sync_2d->width,
                                       SVM_FLAG_CAP_SYNC_COPY_2D);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_copy_check_addr_prop_cap((u64)(uintptr_t)sync_2d->dst,
                                       sync_2d->dpitch * (sync_2d->height - 1) + sync_2d->width,
                                       SVM_FLAG_CAP_SYNC_COPY_2D);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_set_copy_va_2d_info((u64)(uintptr_t)sync_2d->src, sync_2d->spitch, sync_2d->width, sync_2d->height,
                                  &src_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_set_copy_va_2d_info((u64)(uintptr_t)sync_2d->dst, sync_2d->dpitch, sync_2d->width, sync_2d->height,
                                  &dst_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    dir = copy_dir_get_by_devid(src_info.devid, dst_info.devid);
    if ((dir == SVM_H2H_CPY) || (dir == SVM_D2D_CPY)) {
        svm_info("Not support h2h, d2d copy.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (dir != sync_2d->direction) {
        svm_err("Act cpy dir is diff from para dir. (act_dir=%u; para_dir=%u; src=0x%llx; dst=0x%llx)\n", dir,
                sync_2d->direction, sync_2d->src, sync_2d->dst);
        return DRV_ERROR_PARA_ERROR;
    }

    return svm_sync_copy_2d(&src_info, &dst_info);
}

static drvError_t svm_2d_convert(struct drvMem2DAsync *convert_2d)
{
    struct drvMem2D *info_2d = &convert_2d->copy2dInfo;
    struct svm_copy_va_2d_info src_info, dst_info;
    enum svm_cpy_dir dir;
    int ret;

    svm_debug("2D convert. (src=0x%llx; dst=0x%llx; spitch=%llu; dpitch=%llu; width=%llu; height=%llu; direction=%u)\n",
              info_2d->src, info_2d->dst, info_2d->spitch, info_2d->dpitch, info_2d->width, info_2d->height,
              info_2d->direction);
    if (convert_2d->dmaAddr == NULL) {
        svm_err("dmaAddr is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (info_2d->fixed_size >= (info_2d->width * info_2d->height)) {
        svm_err("Fixed_size should be smaller than width*height. (fixed_size=%llu; width=%llu; height=%llu)\n",
                info_2d->fixed_size, info_2d->width, info_2d->height);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (convert_2d->dmaAddr->offsetAddr.devid >= SVM_MAX_DEV_AGENT_NUM) {
        svm_err("Devid is out of range. (devid=%u; range=0-%u)\n", convert_2d->dmaAddr->offsetAddr.devid,
                (SVM_MAX_DEV_AGENT_NUM - 1));
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_copy_check_addr_prop_cap((u64)(uintptr_t)info_2d->src,
                                       info_2d->spitch * (info_2d->height - 1) + info_2d->width,
                                       SVM_FLAG_CAP_DMA_DESC_CONVERT_2D);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_copy_check_addr_prop_cap((u64)(uintptr_t)info_2d->dst,
                                       info_2d->dpitch * (info_2d->height - 1) + info_2d->width,
                                       SVM_FLAG_CAP_DMA_DESC_CONVERT_2D);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_set_copy_va_2d_info((u64)(uintptr_t)info_2d->src, info_2d->spitch, info_2d->width, info_2d->height,
                                  &src_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_set_copy_va_2d_info((u64)(uintptr_t)info_2d->dst, info_2d->dpitch, info_2d->width, info_2d->height,
                                  &dst_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    dir = copy_dir_get_by_devid(src_info.devid, dst_info.devid);
    if ((dir == SVM_H2H_CPY) || (dir == SVM_D2D_CPY)) {
        svm_info("Not support h2h, d2d copy.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }
    if (dir != info_2d->direction) {
        svm_err("Act cpy dir is diff from para dir. (act_dir=%u; para_dir=%u; src=0x%llx; dst=0x%llx)\n", dir,
                info_2d->direction, info_2d->src, info_2d->dst);
        return DRV_ERROR_PARA_ERROR;
    }

    if (((dir == SVM_H2D_CPY) && (convert_2d->dmaAddr->offsetAddr.devid != dst_info.devid)) ||
        ((dir == SVM_D2H_CPY) && (convert_2d->dmaAddr->offsetAddr.devid != src_info.devid))) {
        svm_err("Invalid devid. (dir=%u; devid=%u; src_devid=%u; dst_devid=%u)\n", dir,
                convert_2d->dmaAddr->offsetAddr.devid, src_info.devid, dst_info.devid);
        return DRV_ERROR_PARA_ERROR;
    }

    return svm_dma_desc_convert_2d(&src_info, &dst_info, info_2d->fixed_size, convert_2d->dmaAddr);
}

#define SVM_2D_HEIGHT_MAX (5ULL * SVM_BYTES_PER_MB)
static int svm_memcpy_2d_para_check(struct MEMCPY2D *p_copy)
{
    struct drvMem2D *handle_2d = NULL;

    if (p_copy == NULL) {
        svm_err("p_copy is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    handle_2d = &p_copy->copy2d;
    if (handle_2d == NULL) {
        svm_err("Input para is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle_2d->dst == 0) {
        svm_err("Dst is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle_2d->src == 0) {
        svm_err("Src is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle_2d->width == 0) {
        svm_err("Width is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle_2d->height == 0) {
        svm_err("Height is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle_2d->dpitch < handle_2d->width) {
        svm_err("Dpitch should be larger than width. (dpitch=%llu; width=%llu)\n", handle_2d->dpitch, handle_2d->width);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle_2d->spitch < handle_2d->width) {
        svm_err("Spitch should be larger than width. (spitch=%llu; width=%llu)\n", handle_2d->spitch, handle_2d->width);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle_2d->direction > DRV_MEMCPY_DEVICE_TO_DEVICE) {
        svm_info("Direction is invalid. (direction=%u)\n", handle_2d->direction);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((handle_2d->direction == DRV_MEMCPY_HOST_TO_HOST) || (handle_2d->direction == DRV_MEMCPY_DEVICE_TO_DEVICE)) {
        svm_info("Not support h2h & d2d copy.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (handle_2d->height > SVM_2D_HEIGHT_MAX) {
        svm_err("Out of max height. (height=%llu; max=%llu)\n", handle_2d->height, SVM_2D_HEIGHT_MAX);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((handle_2d->height != 1) && (((handle_2d->src + handle_2d->spitch) < handle_2d->src) ||
                                     ((handle_2d->dst + handle_2d->dpitch) < handle_2d->dst))) {
        svm_err("Pitch is invalid. (src=0x%llx; dst=0x%llx; spitch=%llu; dpitch=%llu)\n", handle_2d->src,
                handle_2d->dst, handle_2d->spitch, handle_2d->dpitch);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t halMemcpy2DInner(struct MEMCPY2D *p_copy);
drvError_t halMemcpy2DInner(struct MEMCPY2D *p_copy)
{
    int ret;

    ret = svm_memcpy_2d_para_check(p_copy);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (p_copy->type == DEVMM_MEMCPY2D_SYNC) {
        ret = svm_2d_sync_copy(&p_copy->copy2d);
    } else if (p_copy->type == DEVMM_MEMCPY2D_ASYNC_CONVERT) {
        ret = svm_2d_convert(&p_copy->copy2dAsync);
    } else if (p_copy->type == DEVMM_MEMCPY2D_ASYNC_DESTROY) {
        ret = drvMemDestroyAddr(p_copy->copy2dAsync.dmaAddr);
    } else {
        svm_err("Input parameter type is invalid. (type=%u)\n", p_copy->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return ret;
}

drvError_t halMemcpy2D(struct MEMCPY2D *p_copy)
{
    return halMemcpy2DInner(p_copy);
}

static int svm_cpy_batch_para_prop_check(u64 dst[], u64 src[], u64 size[], u64 count)
{
    int ret;
    u64 i;

    for (i = 0; i < count; i++) {
        if ((src[i] == 0ULL) || (dst[i] == 0ULL) || (size[i] == 0ULL)) {
            svm_err("Va or size is zero. (index=%u; src=0x%llx; dst=0x%llx; size=%llu)\n", i, src[i], dst[i], size[i]);
            return DRV_ERROR_INVALID_VALUE;
        }

        if ((svm_va_is_in_range(src[i], size[i]) == false) && (svm_va_is_in_range(dst[i], size[i]) == false)) {
            svm_debug("Memcpy batch not support h2h. (dst=0x%llx; src=0x%llx; index=%u)\n", dst[i], src[i], i);
            return DRV_ERROR_NOT_SUPPORT;
        }

        ret = svm_copy_check_addr_prop_cap(dst[i], size[i], SVM_FLAG_CAP_SYNC_COPY_BATCH);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }

        ret = svm_copy_check_addr_prop_cap(src[i], size[i], SVM_FLAG_CAP_SYNC_COPY_BATCH);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

#define SVM_CPY_BATCH_MAX_COUNT (4ULL * SVM_BYTES_PER_KB)
static int svm_cpy_batch_para_check(u64 dst[], u64 src[], u64 size[], u64 count)
{
    if ((dst == NULL) || (src == NULL) || (size == NULL) || (count == 0)) {
        svm_err("Memcpy batch para check failed. (dst_is_null=%d; src_is_null=%d; size_is_null=%d; count=%lu)\n",
                (dst == NULL), (src == NULL), (size == NULL), count);
        return DRV_ERROR_PARA_ERROR;
    }

    if (count > SVM_CPY_BATCH_MAX_COUNT) {
        svm_info("Batch cpy addr count over limit. (count=%lu; max=%lu)\n", count,
                 (unsigned long)SVM_CPY_BATCH_MAX_COUNT);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return svm_cpy_batch_para_prop_check(dst, src, size, count);
}

static int get_copy_va_devid(u64 va, u64 size, u32 *devid)
{
    struct svm_prop prop;
    int ret;

    if (svm_va_is_in_range(va, size)) {
        ret = svm_get_prop(va, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Invalid addr. (ret=%d; va=0x%llx)\n", ret, va);
            return ret;
        }

        *devid = prop.devid;
    } else {
        *devid = svm_get_host_devid();
    }

    return DRV_ERROR_NONE;
}

static int check_batch_copy_va_devid(u64 va, u64 size, u32 devid)
{
    u32 tmp_devid;
    int ret;

    ret = get_copy_va_devid(va, size, &tmp_devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (devid != tmp_devid) {
        svm_err("Cpy devid different. (cur_addr=0x%llx; cur_devid=%u; target_devid=%u)\n", va, tmp_devid, devid);
        return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

static int svm_get_batch_copy_devid(u64 dst[], u64 src[], u64 size[], u64 count, u32 *src_devid, u32 *dst_devid)
{
    int ret;
    u64 i;

    ret = get_copy_va_devid(src[0], size[0], src_devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = get_copy_va_devid(dst[0], size[0], dst_devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    for (i = 1; i < count; i++) {
        ret = check_batch_copy_va_devid(src[i], size[i], *src_devid);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }

        ret = check_batch_copy_va_devid(dst[i], size[i], *dst_devid);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }

    return ret;
}

DVresult halMemcpyBatch(uint64_t dst[], uint64_t src[], size_t size[], size_t count)
{
    u32 src_devid, dst_devid;
    int ret;

    svm_debug("Sync copy batch. (count=%lu)\n", count);
    ret = svm_cpy_batch_para_check((u64 *)dst, (u64 *)src, (u64 *)size, (u64)count);
    if (ret != DRV_ERROR_NONE) {
        return (DVresult)ret;
    }

    ret = svm_get_batch_copy_devid((u64 *)dst, (u64 *)src, (u64 *)size, (u64)count, &src_devid, &dst_devid);
    if (ret != DRV_ERROR_NONE) {
        return (DVresult)ret;
    }

    return (DVresult)svm_sync_copy_batch((u64 *)dst, (u64 *)src, (u64 *)size, (u64)count, src_devid, dst_devid);
}
