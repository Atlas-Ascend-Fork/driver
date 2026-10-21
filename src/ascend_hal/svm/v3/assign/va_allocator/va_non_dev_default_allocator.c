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
#include "mga.h"
#include "va_gap.h"
#include "va_reserve.h"
#include "va_non_dev_default_allocator.h"

#define VA_NON_DEV_DEFAULT_VA_START (0x300000000000ULL + (16ULL * SVM_BYTES_PER_TB))

#define VA_NON_DEV_DEFAULT_RESERVE_SIZE (1ULL * SVM_BYTES_PER_TB)

#define VA_NON_DEV_DEFAULT_EXPAND_FACTOR 128
#define VA_NON_DEV_DEFAULT_SHRINK_FACTOR 8

static void *g_non_dev_default_mga_inst = NULL;

static int va_non_dev_default_va_expand(void *mga_inst, u64 *size, u64 *va)
{
    u64 fixed_va = 0;
    u32 flag = 0;
    int ret;

    flag |= SVM_VA_RESERVE_FLAG_WITH_MASTER;
    flag |= SVM_VA_RESERVE_FLAG_WITH_CUSTOM_CP;
    flag |= SVM_VA_RESERVE_FLAG_WITH_HCCP;

    /* Not open device */
    if (!va_reserve_has_dev()) {
        fixed_va = VA_NON_DEV_DEFAULT_VA_START + mga_get_total_size(mga_inst);
    }

    ret = svm_reserve_va(fixed_va, *size, flag, va);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Reserve va failed. (ret=%d; size=%llu)\n", ret, *size);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int va_non_dev_default_va_shrink(void *mga_inst, u64 va, u64 size)
{
    SVM_UNUSED(mga_inst);
    SVM_UNUSED(size);

    return svm_release_va(va);
}

int _va_non_dev_default_allocator_init(void)
{
    void *mga_inst = NULL;
    struct mga_attr attr;

    if (g_non_dev_default_mga_inst != NULL) {
        return DRV_ERROR_NONE;
    }

    attr.max_align_size = SVM_MGA_MAX_GRAN;
    attr.expand_gran = VA_NON_DEV_DEFAULT_RESERVE_SIZE;
    attr.expand_thres = VA_NON_DEV_DEFAULT_RESERVE_SIZE * VA_NON_DEV_DEFAULT_EXPAND_FACTOR;
    attr.shrink_thres = VA_NON_DEV_DEFAULT_RESERVE_SIZE * VA_NON_DEV_DEFAULT_SHRINK_FACTOR;
    attr.expand = va_non_dev_default_va_expand;
    attr.shrink = va_non_dev_default_va_shrink;

    mga_inst = mga_inst_create(&attr);
    if (mga_inst == NULL) {
        svm_err("Create mga inst failed.\n");
        return DRV_ERROR_INNER_ERR;
    }

    g_non_dev_default_mga_inst = mga_inst;
    return DRV_ERROR_NONE;
}

static int va_non_dev_default_allocator_init(void)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret;

    (void)pthread_mutex_lock(&mutex);
    ret = _va_non_dev_default_allocator_init();
    (void)pthread_mutex_unlock(&mutex);

    return ret;
}

void va_non_dev_default_allocator_uninit(void)
{
    if (g_non_dev_default_mga_inst != NULL) {
        mga_inst_destroy(g_non_dev_default_mga_inst);
        g_non_dev_default_mga_inst = NULL;
    }
}

int va_non_dev_default_alloc(u64 align, u64 size, u64 *va)
{
    if (g_non_dev_default_mga_inst == NULL) {
        int ret = va_non_dev_default_allocator_init();
        if (ret != DRV_ERROR_NONE) {
            svm_err("Init failed.\n");
            return ret;
        }
    }

    return mga_va_alloc(g_non_dev_default_mga_inst, align, size, va);
}

int va_non_dev_default_free(u64 va, u64 size, u64 align)
{
    if (g_non_dev_default_mga_inst == NULL) {
        svm_err("Hasn't inited.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    return mga_va_free(g_non_dev_default_mga_inst, va, size, align);
}
