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

#include "svm_log.h"
#include "svm_pub.h"
#include "gen_allocator.h"
#include "va_gap.h"
#include "va_reserve.h"

#define DEV_CP_ONLY_GRAN_SIZE (4ULL * SVM_BYTES_PER_KB)
#define DEV_CP_ONLY_RESERVE_SIZE (2ULL * SVM_BYTES_PER_GB)

struct va_dev_cp_only_allocator {
    u64 va;
    u64 size;
    void *ga_inst;
    bool valid;
};

static struct va_dev_cp_only_allocator g_dev_cp_only_allocator = {0};

static struct va_dev_cp_only_allocator *va_get_dev_cp_only_allocator(void)
{
    return &g_dev_cp_only_allocator;
}

static int va_dev_cp_only_allocator_init_ga_inst(struct va_dev_cp_only_allocator *allocator)
{
    struct svm_ga_attr attr;
    void *ga_inst = NULL;

    svm_ga_attr_pack(DEV_CP_ONLY_GRAN_SIZE, &attr);
    ga_inst = svm_ga_inst_create(&attr);
    if (ga_inst == NULL) {
        svm_debug("Create ga_inst completed. (ga_inst_is_null=%d)\n", (ga_inst == NULL));
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    allocator->ga_inst = ga_inst;

    return 0;
}

static void va_dev_cp_only_allocator_uninit_ga_inst(struct va_dev_cp_only_allocator *allocator)
{
    if (allocator->ga_inst != NULL) {
        svm_ga_inst_destroy(allocator->ga_inst);
        allocator->ga_inst = NULL;
    }
}

static int va_dev_cp_only_allocator_init_va(struct va_dev_cp_only_allocator *allocator)
{
    u64 va, size;
    u32 flag = SVM_VA_RESERVE_FLAG_PRIVATE;
    int ret;

    size = DEV_CP_ONLY_RESERVE_SIZE;
    ret = svm_reserve_va(0, size, flag, &va);
    if (ret != 0) {
        svm_err("Reserve va failed. (size=0x%llx)\n", size);
        return ret;
    }

    ret = svm_ga_add_range(allocator->ga_inst, va, size);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Ga inst add range failed. (ret=%d; start=0x%llx; size=0x%llx)\n", ret, va, size);
        svm_release_va(va);
        return ret;
    }

    allocator->va = va;
    allocator->size = size;

    return ret;
}

static void va_dev_cp_only_allocator_uninit_va(struct va_dev_cp_only_allocator *allocator)
{
    if (allocator->va != 0) {
        int ret = svm_release_va(allocator->va);
        if (ret != 0) {
            svm_warn("Release va failed. (va=0x%llx)\n", allocator->va);
        }
        allocator->va = 0;
        allocator->size = 0;
    }
}

static int va_dev_cp_only_allocator_init_locked(struct va_dev_cp_only_allocator *allocator)
{
    int ret;

    if (allocator->valid == true) {
        return 0;
    }

    ret = va_dev_cp_only_allocator_init_ga_inst(allocator);
    if (ret != 0) {
        return ret;
    }

    ret = va_dev_cp_only_allocator_init_va(allocator);
    if (ret != 0) {
        va_dev_cp_only_allocator_uninit_ga_inst(allocator);
        return ret;
    }

    allocator->valid = true;

    return 0;
}

static int va_dev_cp_only_allocator_init(struct va_dev_cp_only_allocator *allocator)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret;

    (void)pthread_mutex_lock(&mutex);
    ret = va_dev_cp_only_allocator_init_locked(allocator);
    (void)pthread_mutex_unlock(&mutex);

    return ret;
}

void va_dev_cp_only_allocator_uninit(void)
{
    struct va_dev_cp_only_allocator *allocator = va_get_dev_cp_only_allocator();

    va_dev_cp_only_allocator_uninit_va(allocator);
    va_dev_cp_only_allocator_uninit_ga_inst(allocator);
    allocator->valid = false;
}

int va_dev_cp_only_alloc(u32 devid, u64 align, u64 size, u64 *va)
{
    struct va_dev_cp_only_allocator *allocator = va_get_dev_cp_only_allocator();
    int ret;

    if (!SVM_IS_ALIGNED(align, DEV_CP_ONLY_GRAN_SIZE)) {
        svm_err("Align size is invalid. (align=%llu)\n", align);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (devid >= SVM_MAX_DEV_AGENT_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (allocator->valid == false) {
        ret = va_dev_cp_only_allocator_init(allocator);
        if (ret != 0) {
            return ret;
        }
    }

    return svm_ga_alloc(allocator->ga_inst, 0, va, size);
}

int va_dev_cp_only_free(u32 devid, u64 va, u64 size)
{
    struct va_dev_cp_only_allocator *allocator = va_get_dev_cp_only_allocator();

    if (devid >= SVM_MAX_DEV_AGENT_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (allocator->ga_inst == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }

    return svm_ga_free(allocator->ga_inst, va, size);
}
