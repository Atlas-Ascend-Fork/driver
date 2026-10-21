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

#define VA_DEV_LINEAR_MEM_RESERVE_SIZE (16ULL * SVM_BYTES_PER_TB)

static u64 g_linear_start;
static u64 g_linear_size;

int va_dev_linear_mem_enable(void)
{
    if (g_linear_start == 0) {
        u32 flag = 0;
        int ret;

        flag |= SVM_VA_RESERVE_FLAG_PRIVATE;

        g_linear_size = VA_DEV_LINEAR_MEM_RESERVE_SIZE;
        ret = svm_reserve_va(0, g_linear_size, flag, &g_linear_start);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Reserve va failed. (ret=%d; size=0x%llx)\n", ret, g_linear_size);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

void va_dev_linear_mem_disable(void)
{
    if (g_linear_start != 0) {
        int ret = svm_release_va(g_linear_start);
        if (ret != DRV_ERROR_NONE) {
            svm_warn("Release va failed. (ret=%d; va=0x%llx)\n", ret, g_linear_start);
        }
        g_linear_start = 0;
    }
}

bool svm_is_in_linear_mem_va_range(u64 va, u64 size)
{
    return (svm_check_va_range(va, size, g_linear_start, g_linear_start + g_linear_size) == 0);
}
