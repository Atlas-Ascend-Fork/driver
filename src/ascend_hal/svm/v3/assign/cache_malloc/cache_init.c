/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ascend_hal_error.h"
#include "ascend_hal.h"

#include "svm_pub.h"
#include "svm_log.h"
#include "svm_init_pri.h"
#include "svm_criu.h"
#include "svm_sys_cmd.h"
#include "svm_dbi.h"
#include "va_allocator.h"
#include "cache_malloc.h"
#include "cache_allocator.h"
#include "cache_recycle_seg.h"

static u32 g_host_cache_flag[] = {0, SVM_CACHE_MALLOC_FLAG_MASTER_UVA};
static u32 g_dev_cache_flag[] = {
    0,
    SVM_CACHE_MALLOC_FLAG_PA_P2P,
    SVM_CACHE_MALLOC_FLAG_PA_HPAGE,
    SVM_CACHE_MALLOC_FLAG_PA_HPAGE | SVM_CACHE_MALLOC_FLAG_PA_P2P,
};
static int g_cache_init_flag[SVM_MAX_DEV_NUM] = {0};

static void cache_get_support_flag(u32 devid, u32 **flag, u32 *num)
{
    if (devid == svm_get_host_devid()) {
        *flag = g_host_cache_flag;
        *num = sizeof(g_host_cache_flag) / sizeof(g_host_cache_flag[0]);
        if (!svm_is_support_pcie_th()) {
            *num -= 1;
        }
    } else {
        *flag = g_dev_cache_flag;
        *num = sizeof(g_dev_cache_flag) / sizeof(g_dev_cache_flag[0]);
    }
}

static int cache_create(u32 devid)
{
    u32 *flag = NULL;
    u32 i, j, num;
    int ret;

    cache_get_support_flag(devid, &flag, &num);

    for (i = 0; i < num; i++) {
        ret = cache_allocator_create(devid, flag[i]);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Cache allocator create failed. (i=%u; ret=%d; devid=%u; flag=0x%x)\n", i, ret, devid, flag[i]);
            goto destroy_cache;
        }
    }

    return DRV_ERROR_NONE;
destroy_cache:
    for (j = 0; j < i; j++) {
        cache_allocator_destroy(devid, flag[j]);
    }
    return ret;
}

/* call after malloc_mng recycle_by_dev */
static void cache_destroy(u32 devid)
{
    u64 shrinked_size;
    u32 *flag = NULL;
    u32 i, num;

    cache_get_support_flag(devid, &flag, &num);

    for (i = 0; i < num; i++) {
        svm_cache_shrink(devid, flag[i], &shrinked_size);
        cache_allocator_destroy(devid, flag[i]);
    }
}

static int cache_dev_init(u32 devid)
{
    int ret = DRV_ERROR_NONE;

    if (g_cache_init_flag[devid] == 0) {
        ret = cache_create(devid);
        if (ret == DRV_ERROR_NONE) {
            g_cache_init_flag[devid] = 1;
        }
    }

    return ret;
}

static int cache_dev_uninit(u32 devid)
{
    if (g_cache_init_flag[devid] == 1) {
        g_cache_init_flag[devid] = 0;
        cache_destroy(devid);
    }

    return 0;
}

int cache_restore(u32 devid, u64 start, u64 size, u32 flag);
static int cache_criu_restore_single_range(u32 devid, u64 start, u64 size, u32 flag, void *priv)
{
    SVM_UNUSED(priv);
    return cache_restore(devid, start, size, flag);
}

static int cache_criu_reset(u32 devid, void *data)
{
    SVM_UNUSED(data);
    cache_recycle_seg_clear_by_dev(devid);
    return DRV_ERROR_NONE;
}

static int cache_criu_restore(u32 devid, void *data)
{
    return svm_cache_for_each_range(devid, cache_criu_restore_single_range, data);
}

static const struct svm_criu_ops g_cache_malloc_criu_ops = {
    .name = "cache_malloc",
    .reset = cache_criu_reset,
    .restore = cache_criu_restore,
};

static void __attribute__((constructor(SVM_INIT_PRI_LOW))) cache_init(void)
{
    int ret;

    ret = svm_register_ioctl_dev_init_post_handle(cache_dev_init);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev init post handle failed.\n");
    }

    ret = svm_register_ioctl_dev_uninit_pre_handle(cache_dev_uninit);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev uninit pre handle failed.\n");
    }

    ret = svm_criu_register_ops(&g_cache_malloc_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed.\n");
    }
}
