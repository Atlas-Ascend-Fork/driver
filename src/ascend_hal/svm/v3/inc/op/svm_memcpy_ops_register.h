/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SVM_MEMCPY_OPS_REGISTER_H
#define SVM_MEMCPY_OPS_REGISTER_H

#include "ascend_hal_define.h"

#include "svm_pub.h"
#include "svm_memcpy.h"

struct svm_copy_ops {
    int (*sync_copy)(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info);
    int (*async_copy_submit)(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info, int *id);
    int (*async_copy_wait)(u32 devid, int id);
    int (*dma_desc_convert)(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info,
                            struct DMA_ADDR *dma_desc);
    int (*dma_desc_submit)(u32 devid, struct DMA_ADDR *dma_desc, int flag);
    int (*dma_desc_wait)(u32 devid, struct DMA_ADDR *dma_desc);
    int (*dma_desc_destroy)(u32 devid, struct DMA_ADDR *dma_desc);
    int (*dma_desc_convert_2d)(u32 devid, struct svm_copy_va_2d_info *src_info, struct svm_copy_va_2d_info *dst_info,
                               u64 fixed_size, struct DMA_ADDR *dma_desc);
    int (*sync_copy_2d)(u32 devid, struct svm_copy_va_2d_info *src_info, struct svm_copy_va_2d_info *dst_info);
    int (*sync_copy_batch)(u64 src[], u64 dst[], u64 size[], u64 count, u32 src_devid, u32 dst_devid);
    /* Two-level async interface for pipeline: alloc channel once, reuse across chunks */
    int (*async_chan_alloc)(u32 devid, int *chan_id);
    void (*async_chan_free)(u32 devid, int chan_id);
    int (*async_copy_submit_reuse)(u32 devid, int chan_id, struct svm_copy_va_info *src_info,
                                   struct svm_copy_va_info *dst_info);
    int (*async_copy_wait_one)(u32 devid, int chan_id);
};

void svm_copy_ops_register(u32 devid, struct svm_copy_ops *ops);

#endif
