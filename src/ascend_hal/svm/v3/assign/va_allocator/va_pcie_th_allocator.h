/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef VA_PCIE_TH_ALLOCATOR_H
#define VA_PCIE_TH_ALLOCATOR_H

#include "svm_pub.h"

int va_pcie_th_alloc(u64 align, u64 size, u64 *va);
int va_pcie_th_free(u64 va, u64 size, u64 align);

int va_pcie_th_allocator_init(void);
void va_pcie_th_allocator_uninit(void);

#endif
