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
#include "svm_user_adapt.h"
#include "mga.h"
#include "va_reserve.h"
#include "va_dev_default_allocator.h"

#define VA_DEV_DEFAULT_HOST_VA_START 0x100000000000ULL
#define VA_DEV_DEFAULT_HOST_VA_MAX 0x180000000000ULL

#ifdef EMU_ST /* Simulation ST is required and cannot be deleted. */
#define VA_DEV_DEFAULT_DEV_RESERVE_SIZE (16ULL * SVM_BYTES_PER_GB)
#define VA_DEV_DEFAULT_HOST_RESERVE_SIZE (128ULL * SVM_BYTES_PER_GB)
#else
#define VA_DEV_DEFAULT_DEV_RESERVE_SIZE (256ULL * SVM_BYTES_PER_GB)
#define VA_DEV_DEFAULT_HOST_RESERVE_SIZE (2ULL * SVM_BYTES_PER_TB)
#endif

#define VA_DEV_DEFAULT_EXPAND_FACTOR 2

static void *g_dev_default_mga_inst[SVM_MAX_DEV_NUM];

static u64 va_get_dev_default_reserve_size(u32 devid)
{
    return (devid < SVM_MAX_AGENT_NUM) ? VA_DEV_DEFAULT_DEV_RESERVE_SIZE : VA_DEV_DEFAULT_HOST_RESERVE_SIZE;
}

u64 va_get_dev_default_alloc_max_size(u32 devid)
{
    return va_get_dev_default_reserve_size(devid);
}

static int _va_dev_default_va_expand(void *mga_inst, u64 fixed_va, u64 *size, u64 *va)
{
    u32 flag = 0;
    int ret;

    SVM_UNUSED(mga_inst);

    flag |= SVM_VA_RESERVE_FLAG_WITH_MASTER;
    flag |= SVM_VA_RESERVE_FLAG_WITH_CUSTOM_CP;
    flag |= SVM_VA_RESERVE_FLAG_WITH_HCCP;

    ret = svm_reserve_va(fixed_va, *size, flag, va);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int va_dev_default_dev_va_expand(void *mga_inst, u64 *size, u64 *va)
{
    int ret;

    SVM_UNUSED(mga_inst);

    ret = _va_dev_default_va_expand(mga_inst, 0, size, va);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Reserve va failed. (ret=%d; size=%llu)\n", ret, *size);
    }
    return ret;
}

static int va_dev_default_host_va_expand_with_retry(void *mga_inst, u64 *size, u64 *va)
{
    u64 start_va = VA_DEV_DEFAULT_HOST_VA_START + mga_get_total_size(mga_inst); /* Factor is 2, va won't conflict. */
    u64 fixed_va, step;
    int ret;

    step = SVM_BYTES_PER_TB;
    for (fixed_va = start_va; fixed_va + *size <= VA_DEV_DEFAULT_HOST_VA_MAX; fixed_va += step) {
        ret = _va_dev_default_va_expand(mga_inst, fixed_va, size, va);
        if (ret == DRV_ERROR_NONE) {
            svm_run_info("Reserve va success. (va=0x%llx; size=%llu)\n", *va, *size);
            return DRV_ERROR_NONE;
        }
    }

    step = SVM_BYTES_PER_GB;
    for (fixed_va = start_va; fixed_va + *size <= VA_DEV_DEFAULT_HOST_VA_MAX; fixed_va += step) {
        ret = _va_dev_default_va_expand(mga_inst, fixed_va, size, va);
        if (ret == DRV_ERROR_NONE) {
            svm_run_info("Reserve va success. (va=0x%llx; size=%llu)\n", *va, *size);
            return DRV_ERROR_NONE;
        }
    }

    svm_err("Reserve va failed after retries. (start_va=0x%llx; max_va=0x%llx; size=%llu)\n", start_va,
            VA_DEV_DEFAULT_HOST_VA_MAX, *size);
    return DRV_ERROR_OUT_OF_MEMORY;
}

static int va_dev_default_host_va_expand(void *mga_inst, u64 *size, u64 *va)
{
    int ret;

    /* Not open device, only alloc host mem. */
    if (!va_reserve_has_dev()) {
        return va_dev_default_host_va_expand_with_retry(mga_inst, size, va);
    }

    ret = _va_dev_default_va_expand(mga_inst, 0, size, va);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Reserve va failed. (ret=%d; size=%llu)\n", ret, *size);
    }
    return ret;
}

static int va_dev_default_va_shrink(void *mga_inst, u64 va, u64 size)
{
    SVM_UNUSED(mga_inst);
    SVM_UNUSED(size);

    return svm_release_va(va);
}

static void *va_dev_default_mga_inst_create(u32 devid)
{
    void *mga_inst = NULL;
    struct mga_attr attr;

    attr.max_align_size = (devid < SVM_MAX_AGENT_NUM) ? SVM_MGA_MAX_GRAN : (2ULL * SVM_BYTES_PER_MB);
    attr.expand_gran = va_get_dev_default_reserve_size(devid);
    attr.expand_thres = va_get_dev_default_reserve_size(devid) * VA_DEV_DEFAULT_EXPAND_FACTOR;
    attr.shrink_thres = va_get_dev_default_reserve_size(devid);
    attr.expand = (devid < SVM_MAX_AGENT_NUM) ? va_dev_default_dev_va_expand : va_dev_default_host_va_expand;
    attr.shrink = va_dev_default_va_shrink;

    mga_inst = mga_inst_create(&attr);
    if (mga_inst == NULL) {
        svm_err("Create mga inst failed. (devid=%u)\n", devid);
    }

    return mga_inst;
}

static void va_dev_default_mga_inst_destroy(void *mga_inst)
{
    mga_inst_destroy(mga_inst);
}

int va_dev_default_allocator_dev_init(u32 devid)
{
    void *mga_inst = NULL;

    if (g_dev_default_mga_inst[devid] != NULL) {
        return DRV_ERROR_NONE;
    }

    mga_inst = va_dev_default_mga_inst_create(devid);
    if (mga_inst == NULL) {
        return DRV_ERROR_INNER_ERR;
    }

    g_dev_default_mga_inst[devid] = mga_inst;
    return DRV_ERROR_NONE;
}

void va_dev_default_allocator_dev_uninit(u32 devid)
{
    if (g_dev_default_mga_inst[devid] != NULL) {
        va_dev_default_mga_inst_destroy(g_dev_default_mga_inst[devid]);
        g_dev_default_mga_inst[devid] = NULL;
    }
}

int va_dev_default_alloc(u32 devid, u64 align, u64 size, u64 *va)
{
    void *mga_inst = NULL;

    if (devid >= SVM_MAX_DEV_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (size > va_get_dev_default_alloc_max_size(devid)) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    mga_inst = g_dev_default_mga_inst[devid];
    if (mga_inst == NULL) {
        svm_err("Hasn't inited. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return mga_va_alloc(mga_inst, align, size, va);
}

int va_dev_default_free(u32 devid, u64 va, u64 size, u64 align)
{
    void *mga_inst = NULL;

    if (devid >= SVM_MAX_DEV_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    mga_inst = g_dev_default_mga_inst[devid];
    if (mga_inst == NULL) {
        svm_warn("Hasn't inited. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return mga_va_free(mga_inst, va, size, align);
}
