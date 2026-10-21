/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>

#include "ascend_hal.h"

#include "svm_log.h"
#include "va_gap.h"
#include "va_reserve.h"

#define VA_DEV_DCACHE_RESERVE_SIZE (5ULL * SVM_BYTES_PER_GB)

static u64 g_dache_start;
static u64 g_dache_size;

int va_dev_dcache_allocator_init(void)
{
    if (g_dache_start == 0) {
        u32 flag = 0;
        int ret;

        g_dache_size = VA_DEV_DCACHE_RESERVE_SIZE;
        ret = svm_reserve_va(0, g_dache_size, flag, &g_dache_start);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Reserve va failed. (ret=%d; size=0x%llx)\n", ret, g_dache_size);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

void va_dev_dcache_allocator_uninit(void)
{
    if (g_dache_start != 0) {
        int ret = svm_release_va(g_dache_start);
        if (ret != DRV_ERROR_NONE) {
            svm_warn("Release va failed. (ret=%d; va=0x%llx)\n", ret, g_dache_start);
        }
        g_dache_start = 0;
    }
}

void svm_get_dcache_va_range(u64 *va, u64 *size)
{
    *va = g_dache_start;
    *size = g_dache_size;
}

bool svm_is_in_dcache_va_range(u64 va, u64 size)
{
    return (svm_check_va_range(va, size, g_dache_start, g_dache_start + g_dache_size) == 0);
}

int va_dev_dcache_alloc(u64 va, u64 size)
{
    if (!svm_is_in_dcache_va_range(va, size)) {
        svm_err("Invalid para. (va=0x%llx size=0x%llx)\n", va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

int va_dev_dcache_free(u64 va, u64 size)
{
    if (!svm_is_in_dcache_va_range(va, size)) {
        svm_err("Invalid para. (va=0x%llx size=0x%llx)\n", va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}
