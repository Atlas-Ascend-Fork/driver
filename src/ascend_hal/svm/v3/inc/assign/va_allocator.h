/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef VA_ALLOCATOR_H
#define VA_ALLOCATOR_H
#include <stdbool.h>
#include <stdint.h>

#include "svm_pub.h"
#include "va_mng.h"

/*
    va_allocator: virtual address allocator
    svm_alloc_va: for self owner allocator, allocate from address_range[devid] first, the alloc granularity is 2M
    svm_alloc_specified_va: for self or user owner allocator
*/

#define NON_DEV_DEFAULT_ALLOC_MAX_SIZE (128ULL * SVM_BYTES_PER_GB)

#define SVM_VA_ALLOCATOR_FLAG_WITH_MASTER (1U << 0U)
#define SVM_VA_ALLOCATOR_FLAG_DEV_CP_ONLY (1U << 8U)
#define SVM_VA_ALLOCATOR_FLAG_SPACIFIED_ADDR (1U << 9U)
#define SVM_VA_ALLOCATOR_FLAG_MASTER_UVA (1U << 10U)
#define SVM_VA_ALLOCATOR_FLAG_WITHOUT_POPULATE (1U << 11U)

static inline u64 svm_get_non_dev_align_size(u64 size)
{
    return (size > NON_DEV_DEFAULT_ALLOC_MAX_SIZE) ? svm_align_up(size, SVM_VA_RESERVE_ALIGN) : size;
}

int svm_alloc_va(u64 va, u64 size, u64 align, u32 devid, u32 flag, u64 *start);
int svm_free_va(u64 va, u64 size, u64 align, u32 devid, u32 flag);

bool svm_va_is_in_range(u64 va, u64 size);
bool svm_is_valid_range(u64 va, u64 size);
/* use svm_get_va_type to replace svm_va_is_in_range+svm_is_valid_range for better performance */
int svm_get_va_type(u64 va, u64 size, int *va_type);

int svm_va_reserve_add_task(u32 devid, int task_type);

int svm_register_va_reserve_post_handle(int (*fn)(u64 va, u64 size));
int svm_register_va_release_pre_handle(int (*fn)(u64 va, u64 size));
/* Traverse all shared reserved VAs. */
int va_reserve_for_each_node(int (*func)(u64 va, u64 size, void *priv), void *priv);

/* Acquire an existing token for the given VA range by searching the reserve range. */
void *svm_acquire_token(void *seg_mng, uint64_t start, uint64_t size);

void svm_get_dcache_va_range(u64 *va, u64 *size);
bool svm_is_in_dcache_va_range(u64 va, u64 size);

bool svm_is_in_linear_mem_va_range(u64 va, u64 size);

void svm_enable_pcie_th(void);
bool svm_is_support_pcie_th(void);
bool svm_is_pcie_th_allocator_enable(void);

void svm_va_set_gap(u64 gap);

void svm_va_allocator_uninit(void);

#endif
