/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SVM_REGISTER_DMA_H
#define SVM_REGISTER_DMA_H

#include <stdbool.h>
#include "svm_pub.h"

int svm_dma_register(u64 va, u64 size, u32 devid);
int svm_dma_unregister(u64 va, u32 devid);
bool svm_va_can_register_dma(u64 va, u64 size, u32 devid);
int svm_register_dma_lookup(u64 *va, u64 *size, u32 devid);

static inline bool svm_is_dma_registered(u64 va, u64 size, u32 devid)
{
    return (svm_register_dma_lookup(&va, &size, devid) == DRV_ERROR_NONE);
}

#endif /* SVM_REGISTER_DMA_H */
