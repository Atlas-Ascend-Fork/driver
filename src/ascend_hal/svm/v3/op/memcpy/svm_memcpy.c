/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <errno.h>
#include <securec.h>

#include "ascend_hal_define.h"

#include "svm_pub.h"
#include "svm_log.h"
#include "va_allocator.h"
#include "svm_user_adapt.h"
#include "malloc_mng.h"
#include "memcpy_uk_msg.h"
#include "svm_memcpy_local.h"
#include "svm_umc_client.h"
#include "svm_sub_event_type_uk_msg.h"
#include "svm_memcpy_ops_register.h"
#include "svm_memcpy_local_client.h"
#include "svm_memcpy.h"

#define SVM_ASYNC_COPY_HANDLE_DEVID_BIT 32ULL
#define SVM_ASYNC_COPY_HANDLE_ID_BIT 0ULL

#define SVM_ASYNC_COPY_HANDLE_DEVID_WIDTH 32ULL
#define SVM_ASYNC_COPY_HANDLE_ID_WIDTH 32ULL

static struct svm_copy_ops *g_copy_ops[SVM_MAX_DEV_NUM] = {NULL};

void svm_copy_ops_register(u32 devid, struct svm_copy_ops *ops)
{
    if (devid < SVM_MAX_DEV_NUM) {
        g_copy_ops[devid] = ops;
    }
}

static u64 svm_async_copy_handle_pack(u32 devid, int id)
{
    return (((u64)devid << SVM_ASYNC_COPY_HANDLE_DEVID_BIT) | (u64)id);
}

static void svm_async_copy_handle_parse(u64 handle, u32 *devid, int *id)
{
    *devid = (u32)((handle >> SVM_ASYNC_COPY_HANDLE_DEVID_BIT) & (u32)((1UL << SVM_ASYNC_COPY_HANDLE_DEVID_WIDTH) - 1));
    *id = (int)((handle >> SVM_ASYNC_COPY_HANDLE_ID_BIT) & (u32)((1UL << SVM_ASYNC_COPY_HANDLE_ID_WIDTH) - 1));
}

int svm_async_copy_submit(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info, u64 *handle)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_info->devid, dst_info->devid);
    u32 devid = (dir == SVM_H2D_CPY) ? dst_info->devid : src_info->devid;
    int id, ret;

    if ((g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->async_copy_submit == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = g_copy_ops[devid]->async_copy_submit(devid, src_info, dst_info, &id);
    if (ret == DRV_ERROR_NONE) {
        *handle = svm_async_copy_handle_pack(devid, id);
    }

    return ret;
}

int svm_async_copy_wait(u64 handle)
{
    u32 devid;
    int id;

    svm_async_copy_handle_parse(handle, &devid, &id);

    if ((g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->async_copy_wait == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->async_copy_wait(devid, id);
}

int svm_async_chan_alloc(u32 devid, int *chan_id)
{
    if ((devid >= SVM_MAX_DEV_NUM) || (g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->async_chan_alloc == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->async_chan_alloc(devid, chan_id);
}

void svm_async_chan_free(u32 devid, int chan_id)
{
    if ((devid >= SVM_MAX_DEV_NUM) || (g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->async_chan_free == NULL)) {
        return;
    }

    g_copy_ops[devid]->async_chan_free(devid, chan_id);
}

int svm_async_copy_submit_reuse(u32 devid, int chan_id, struct svm_copy_va_info *src_info,
                                struct svm_copy_va_info *dst_info)
{
    if ((devid >= SVM_MAX_DEV_NUM) || (g_copy_ops[devid] == NULL) ||
        (g_copy_ops[devid]->async_copy_submit_reuse == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->async_copy_submit_reuse(devid, chan_id, src_info, dst_info);
}

int svm_async_copy_wait_one(u32 devid, int chan_id)
{
    if ((devid >= SVM_MAX_DEV_NUM) || (g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->async_copy_wait_one == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->async_copy_wait_one(devid, chan_id);
}

int svm_sync_copy(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_info->devid, dst_info->devid);
    u32 devid = (dir == SVM_H2D_CPY) ? dst_info->devid : src_info->devid;

    if (src_info->is_share || dst_info->is_share) {
        /* Shared va may resolve to a device not opened by current process. Route sync ioctl through host devid. */
        devid = svm_get_host_devid();
    }

    if ((g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->sync_copy == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->sync_copy(devid, src_info, dst_info);
}

int svm_sync_copy_2d(struct svm_copy_va_2d_info *src_info, struct svm_copy_va_2d_info *dst_info)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_info->devid, dst_info->devid);
    u32 devid = (dir == SVM_H2D_CPY) ? dst_info->devid : src_info->devid;

    if ((g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->sync_copy_2d == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->sync_copy_2d(devid, src_info, dst_info);
}

int svm_sync_copy_batch(u64 dst[], u64 src[], u64 size[], u64 count, u64 src_devid, u64 dst_devid)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid((u32)src_devid, (u32)dst_devid);
    u32 devid;

    if ((dir == SVM_H2H_CPY) || (dir == SVM_D2D_CPY)) {
        svm_info("Batch cpy not support h2h, d2d cpy.(dir=%u)\n", dir);
        return DRV_ERROR_NOT_SUPPORT;
    }

    devid = (dir == SVM_H2D_CPY) ? (u32)dst_devid : (u32)src_devid;
    if ((g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->sync_copy_batch == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->sync_copy_batch(src, dst, size, count, (u32)src_devid, (u32)dst_devid);
}

int svm_dma_desc_convert(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info,
                         struct DMA_ADDR *dma_desc)
{
    u32 devid = dma_desc->offsetAddr.devid;

    if ((g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->dma_desc_convert == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->dma_desc_convert(devid, src_info, dst_info, dma_desc);
}

int svm_dma_desc_destroy(struct DMA_ADDR *dma_desc)
{
    u32 devid = dma_desc->virt_id;

    if ((g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->dma_desc_destroy == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->dma_desc_destroy(devid, dma_desc);
}

int svm_dma_desc_submit(struct DMA_ADDR *dma_desc, int flag)
{
    u32 devid = dma_desc->virt_id;

    if ((g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->dma_desc_submit == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->dma_desc_submit(devid, dma_desc, flag);
}

int svm_dma_desc_wait(struct DMA_ADDR *dma_desc)
{
    u32 devid = dma_desc->virt_id;

    if ((g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->dma_desc_wait == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->dma_desc_wait(devid, dma_desc);
}

int svm_dma_desc_convert_2d(struct svm_copy_va_2d_info *src_info, struct svm_copy_va_2d_info *dst_info, u64 fixed_size,
                            struct DMA_ADDR *dma_desc)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_info->devid, dst_info->devid);
    u32 devid = (dir == SVM_H2D_CPY) ? dst_info->devid : src_info->devid;

    if ((g_copy_ops[devid] == NULL) || (g_copy_ops[devid]->dma_desc_convert_2d == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_copy_ops[devid]->dma_desc_convert_2d(devid, src_info, dst_info, fixed_size, dma_desc);
}
