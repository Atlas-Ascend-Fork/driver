/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SVM_MEMCPY_H
#define SVM_MEMCPY_H

#include "ascend_hal_define.h"

#include "svm_pub.h"
#include "svm_dbi.h"

struct svm_copy_va_info {
    u64 va;
    u64 size;
    u32 devid;
    int host_tgid;
    bool is_share;
};

struct svm_copy_va_2d_info {
    u64 va;
    u64 pitch;
    u64 width;
    u64 height;
    u32 devid;
};

static inline void svm_copy_va_info_pack(u64 va, u64 size, u32 devid, struct svm_copy_va_info *info)
{
    info->va = va;
    info->size = size;
    info->devid = devid;
    info->host_tgid = 0;
    info->is_share = false;
}

static inline enum svm_cpy_dir copy_dir_get_by_devid(u32 src_devid, u32 dst_devid)
{
    u32 host_devid = svm_get_host_devid();

    if ((src_devid == host_devid) && (dst_devid != host_devid)) {
        return SVM_H2D_CPY;
    } else if ((dst_devid == host_devid) && (src_devid != host_devid)) {
        return SVM_D2H_CPY;
    } else if ((dst_devid != host_devid) && (src_devid != host_devid)) {
        return SVM_D2D_CPY;
    } else {
        return SVM_H2H_CPY;
    }
}

int svm_sync_copy(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info);

int svm_async_copy_submit(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info, u64 *handle);
int svm_async_copy_wait(u64 handle);

/* Two-level async interface for pipeline: alloc channel once, reuse across chunks */
int svm_async_chan_alloc(u32 devid, int *chan_id);
void svm_async_chan_free(u32 devid, int chan_id);
int svm_async_copy_submit_reuse(u32 devid, int chan_id, struct svm_copy_va_info *src_info,
                                struct svm_copy_va_info *dst_info);
int svm_async_copy_wait_one(u32 devid, int chan_id);

int svm_dma_desc_convert(struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info,
                         struct DMA_ADDR *dma_desc);
int svm_dma_desc_submit(struct DMA_ADDR *dma_desc, int flag);
int svm_dma_desc_wait(struct DMA_ADDR *dma_desc);
int svm_dma_desc_destroy(struct DMA_ADDR *dma_desc);

int svm_dma_desc_convert_2d(struct svm_copy_va_2d_info *src_info, struct svm_copy_va_2d_info *dst_info, u64 fixed_size,
                            struct DMA_ADDR *dma_desc);

int svm_sync_copy_2d(struct svm_copy_va_2d_info *src_info, struct svm_copy_va_2d_info *dst_info);

int svm_sync_copy_batch(u64 dst[], u64 src[], u64 size[], u64 count, u64 src_devid, u64 dst_devid);

#endif
