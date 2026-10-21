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

#include "ascend_hal_error.h"

#include "svm_log.h"
#include "va_allocator.h"
#include "mpl_client.h"
#include "va_allocator.h"
#include "normal_malloc.h"

/* TODO: Move it to a separate file later for declaration. */
bool svm_is_dev_closing(u32 devid);

struct svm_normal_ops *normal_ops = NULL;

void svm_normal_set_ops(struct svm_normal_ops *ops)
{
    normal_ops = ops;
}

static void normal_ops_post_alloc(u32 devid, u64 va, u64 size, u32 flag)
{
    if ((normal_ops != NULL) && (normal_ops->post_malloc != NULL)) {
        normal_ops->post_malloc(devid, va, size, flag);
    }
}

static void normal_ops_pre_free(u32 devid, u64 va, u64 size, u32 flag)
{
    if ((normal_ops != NULL) && (normal_ops->pre_free != NULL)) {
        normal_ops->pre_free(devid, va, size, flag);
    }
}

static inline u32 normal_malloc_flag_to_va_allocator_flag(u32 flag)
{
    u32 va_allocator_flag = 0;

    va_allocator_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_VA_WITH_MASTER) != 0) ? SVM_VA_ALLOCATOR_FLAG_WITH_MASTER : 0;
    va_allocator_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_DEV_CP_ONLY) != 0) ? SVM_VA_ALLOCATOR_FLAG_DEV_CP_ONLY : 0;
    va_allocator_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_SPACIFIED_VA) != 0) ? SVM_VA_ALLOCATOR_FLAG_SPACIFIED_ADDR : 0;
    va_allocator_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_MASTER_UVA) != 0) ? SVM_VA_ALLOCATOR_FLAG_MASTER_UVA : 0;
    va_allocator_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_VA_WITHOUT_POPULATE) != 0) ?
                             SVM_VA_ALLOCATOR_FLAG_WITHOUT_POPULATE :
                             0;

    return va_allocator_flag;
}

static int normal_va_alloc(u32 devid, u32 flag, u64 align, u64 *va, u64 size)
{
    u32 va_allocator_flag = normal_malloc_flag_to_va_allocator_flag(flag);
    int ret;

    ret = svm_alloc_va(*va, size, align, devid, va_allocator_flag, va);
    if (ret != DRV_ERROR_NONE) {
        svm_no_err_if(ret == DRV_ERROR_OUT_OF_MEMORY, "Alloc va completed. (ret=%d; flag=%x; size=0x%llx)\n", ret, flag,
                      size);
    }

    return ret;
}

static void normal_va_free(u32 devid, u32 flag, u64 align, u64 va, u64 size)
{
    u32 va_allocator_flag = normal_malloc_flag_to_va_allocator_flag(flag);
    (void)svm_free_va(va, size, align, devid, va_allocator_flag);
}

static u32 normal_malloc_flag_to_mpl_flag(u32 flag)
{
    u32 mpl_flag = 0;

    mpl_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_PA_HPAGE) != 0) ? SVM_MPL_FLAG_HPAGE : 0;
    mpl_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_PA_GPAGE) != 0) ? SVM_MPL_FLAG_GPAGE : 0;
    mpl_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_PA_CONTINUOUS) != 0) ? SVM_MPL_FLAG_CONTIGUOUS : 0;
    mpl_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_MASTER_UVA) != 0) ? SVM_MPL_FLAG_CONTIGUOUS_PER_2M : 0;
    mpl_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_PA_P2P) != 0) ? SVM_MPL_FLAG_P2P : 0;
    mpl_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_PG_NC) != 0) ? SVM_MPL_FLAG_PG_NC : 0;
    mpl_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_PG_RDONLY) != 0) ? SVM_MPL_FLAG_PG_RDONLY : 0;
    mpl_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_DEV_CP_ONLY) != 0) ? SVM_MPL_FLAG_DEV_CP_ONLY : 0;
    mpl_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_VA_WITHOUT_POPULATE) != 0) ? SVM_MPL_FLAG_NO_REMOTE_OPS : 0;
    mpl_flag |= ((flag & SVM_NORMAL_MALLOC_FLAG_DEV_PRIVATE) != 0) ? SVM_MPL_FLAG_DEV_PRIVATE : 0;

    if ((flag & SVM_NORMAL_MALLOC_FLAG_FIXED_NUMA) != 0) {
        mpl_flag |= SVM_MPL_FLAG_FIXED_NUMA;
        mpl_flag_set_numa_id(&mpl_flag, normal_malloc_flag_get_numa_id(flag));
    }

    return mpl_flag;
}

static int normal_mem_populate(u32 devid, u32 flag, u64 va, u64 size)
{
    u32 mpl_flag = normal_malloc_flag_to_mpl_flag(flag);
    int ret;

    ret = svm_mpl_client_populate(devid, va, size, mpl_flag);
    if (ret != DRV_ERROR_NONE) {
        svm_no_err_if((ret == DRV_ERROR_OUT_OF_MEMORY),
                      "Mpl client populate completed. (ret=%d; mpl_flag=%u; va=0x%llx; size=%llu)\n", ret, mpl_flag, va,
                      size);
    }

    return ret;
}

static int normal_mem_depopulate(u32 devid, u64 va, u64 size, u32 flag)
{
    u32 mpl_flag = normal_malloc_flag_to_mpl_flag(flag);
    int ret;

    ret = svm_mpl_client_depopulate(devid, va, size, mpl_flag);
    if (ret != DRV_ERROR_NONE) {
        svm_warn("Mpl client depopulate completed. (ret=%d; devid=%u; va=0x%llx; size=%llu)\n", ret, devid, va, size);
    }

    /* We deserve forcefully destroy the handle resource when closing the device. */
    return (ret == DRV_ERROR_BUSY) ? (svm_is_dev_closing(devid) ? DRV_ERROR_NONE : ret) : ret;
}

int svm_normal_malloc(u32 devid, u32 flag, u64 align, u64 *va, u64 size)
{
    int ret;
    u64 tmp_va = *va;

    if ((flag & SVM_NORMAL_MALLOC_FLAG_POPULATE_ONLY) == 0) {
        ret = normal_va_alloc(devid, flag, align, &tmp_va, size);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }

    if ((flag & SVM_NORMAL_MALLOC_FLAG_VA_ONLY) == 0) {
        ret = normal_mem_populate(devid, flag, tmp_va, size);
        if (ret != DRV_ERROR_NONE) {
            if ((flag & SVM_NORMAL_MALLOC_FLAG_POPULATE_ONLY) == 0) {
                normal_va_free(devid, flag, align, tmp_va, size);
            }
            return ret;
        }
    }
    normal_ops_post_alloc(devid, tmp_va, size, flag);
    *va = tmp_va;

    return DRV_ERROR_NONE;
}

int svm_normal_free(u32 devid, u32 flag, u64 align, u64 va, u64 size)
{
    int ret;

    normal_ops_pre_free(devid, va, size, flag);

    if ((flag & SVM_NORMAL_MALLOC_FLAG_VA_ONLY) == 0) {
        ret = normal_mem_depopulate(devid, va, size, flag);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }

    normal_va_free(devid, flag, align, va, size);
    return DRV_ERROR_NONE;
}
