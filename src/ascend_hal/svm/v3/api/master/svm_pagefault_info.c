/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <string.h>

#include "ascend_hal.h"
#include "malloc_mng.h"
#include "svm_pub.h"
#include "svm_flag.h"
#include "svm_dbi.h"
#include "svm_log.h"
#include "svm_criu.h"
#include "svm_sys_cmd.h"
#include "svm_pagefault_info.h"

struct svm_pagefault_info_slot {
    pthread_rwlock_t rwlock;
    struct svmPagefaultInfo info;
    bool has_record;
    bool dev_opened; /* Whether this device has been opened via dev_init_post_handle */
};

/* Per-device array indexed by devid. SVM_MAX_AGENT_NUM = 65 (65 device) */
static struct svm_pagefault_info_slot g_pagefault_info_records[SVM_MAX_AGENT_NUM];

static bool svm_pagefault_info_check_is_shared(unsigned long long flag)
{
    if (svm_flag_cap_is_support_ipc_close(flag)) {
        return true; /* IPC shared via halShmemOpenHandle */
    }
    if (svm_flag_cap_is_support_vmm_ipc_unmap(flag)) {
        return true; /* VMM import via halMemImportFromShareableHandleV2 */
    }
    return false; /* halMemAlloc / VMM local */
}

static uint32_t svm_pagefault_info_get_side(uint32_t devid)
{
    if (devid == svm_get_host_devid()) {
        return MEM_HOST_SIDE;
    }
    return MEM_DEV_SIDE;
}

void _svm_pagefault_info_record_access(unsigned int devid, unsigned long long va, struct svm_prop *prop)
{
    struct svm_pagefault_info_slot *slot = &g_pagefault_info_records[devid];
    struct svmPagefaultInfo *info = &slot->info;

    (void)pthread_rwlock_wrlock(&slot->rwlock);

    (void)memset_s(info, sizeof(*info), 0, sizeof(*info));
    info->fault_va = va;

    if (prop != NULL) {
        info->valid_range_start = prop->start;
        info->valid_range_size = prop->size;
        info->valid_range_side = svm_pagefault_info_get_side(prop->devid);
        info->valid_range_devid = (info->valid_range_side == MEM_HOST_SIDE) ? 0 : prop->devid;
        info->valid_range_is_shared = (int)svm_pagefault_info_check_is_shared(prop->flag);
    }

    slot->has_record = true;

    (void)pthread_rwlock_unlock(&slot->rwlock);
}

static bool svm_pagefault_info_has_record(u32 devid)
{
    bool has_record;

    (void)pthread_rwlock_rdlock(&g_pagefault_info_records[devid].rwlock);
    has_record = g_pagefault_info_records[devid].has_record;
    (void)pthread_rwlock_unlock(&g_pagefault_info_records[devid].rwlock);

    return has_record;
}

void svm_pagefault_info_record_access(unsigned int devid, unsigned long long va)
{
    struct svm_prop left_prop;
    struct svm_prop right_prop;
    bool has_left = false;
    bool has_right = false;
    int ret;

    if (devid >= SVM_MAX_AGENT_NUM) {
        return;
    }

    if (svm_pagefault_info_has_record(devid)) {
        return;
    }

    ret = svm_get_nearby_prop(va, &left_prop, &has_left, &right_prop, &has_right);
    if (ret != DRV_ERROR_NONE) {
        svm_warn("Get nearby prop not success. (va=0x%llx; ret=%d)\n", va, ret);
        has_left = false;
    }

    _svm_pagefault_info_record_access(devid, va, (has_left ? &left_prop : NULL));

    svm_run_info_ratelimited("Pagefault info recorded. (va=0x%llx; devid=%u; valid_start=0x%llx; "
                             "valid_size=0x%llx; is_shared=%u)\n",
                             va, devid, has_left ? left_prop.start : 0ULL, has_left ? left_prop.size : 0ULL,
                             has_left ? (unsigned int)svm_pagefault_info_check_is_shared(left_prop.flag) : 0U);
}

static void svm_pagefault_info_clear_internal(u32 devid)
{
    struct svm_pagefault_info_slot *slot = &g_pagefault_info_records[devid];

    (void)pthread_rwlock_wrlock(&slot->rwlock);
    (void)memset_s(&slot->info, sizeof(slot->info), 0, sizeof(slot->info));
    slot->has_record = false;
    slot->dev_opened = false;
    (void)pthread_rwlock_unlock(&slot->rwlock);
}

static int mem_ctrl_pagefault_info_query_para_check(void *param_value, size_t param_value_size, void *out_value,
                                                    size_t *out_size_ret)
{
    uint32_t devid;

    if ((param_value == NULL) || (param_value_size != sizeof(uint32_t)) || (out_value == NULL) ||
        (out_size_ret == NULL)) {
        svm_err("Param is invalid. (param_value=%p; param_value_size=%llu; out_value=%p; out_size_ret=%p)\n",
                param_value, (unsigned long long)param_value_size, out_value, out_size_ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    devid = *(uint32_t *)param_value;
    if (devid >= SVM_MAX_AGENT_NUM) {
        svm_err("Device id out of range. (devid=%u; range=[0, %u))\n", devid, (u32)SVM_MAX_AGENT_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (*out_size_ret < sizeof(struct svmPagefaultInfo)) {
        svm_err("Out size mismatch. (out_size=%llu; expected=%llu)\n", (unsigned long long)*out_size_ret,
                (unsigned long long)sizeof(struct svmPagefaultInfo));
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

int mem_ctrl_pagefault_info_query(void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret)
{
    struct svm_pagefault_info_slot *slot = NULL;
    uint32_t devid;
    int ret;

    ret = mem_ctrl_pagefault_info_query_para_check(param_value, param_value_size, out_value, out_size_ret);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    devid = *(uint32_t *)param_value;

    slot = &g_pagefault_info_records[devid];
    (void)pthread_rwlock_rdlock(&slot->rwlock);
    if (!slot->dev_opened) {
        (void)pthread_rwlock_unlock(&slot->rwlock);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (!slot->has_record) {
        (void)pthread_rwlock_unlock(&slot->rwlock);
        return DRV_ERROR_NOT_EXIST;
    }
    *(struct svmPagefaultInfo *)out_value = slot->info;
    (void)pthread_rwlock_unlock(&slot->rwlock);

    *out_size_ret = sizeof(struct svmPagefaultInfo);

    return DRV_ERROR_NONE;
}

int mem_ctrl_pagefault_info_clear(void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret)
{
    struct svm_pagefault_info_slot *slot = NULL;
    uint32_t devid;

    if ((param_value == NULL) || (param_value_size != sizeof(uint32_t)) || (out_value != NULL) ||
        (out_size_ret != NULL)) {
        svm_err("Param is invalid. (param_value=%p; param_value_size=%llu; out_value=%p; "
                "out_size_ret=%p)\n",
                param_value, (unsigned long long)param_value_size, out_value, out_size_ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    devid = *(uint32_t *)param_value;
    if (devid >= SVM_MAX_AGENT_NUM) {
        svm_err("Device id out of range. (devid=%u; range=[0, %u))\n", devid, (u32)SVM_MAX_AGENT_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    slot = &g_pagefault_info_records[devid];
    (void)pthread_rwlock_wrlock(&slot->rwlock);
    if (!slot->dev_opened) {
        (void)pthread_rwlock_unlock(&slot->rwlock);
        return DRV_ERROR_INVALID_DEVICE;
    }
    (void)memset_s(&slot->info, sizeof(slot->info), 0, sizeof(slot->info));
    slot->has_record = false;
    (void)pthread_rwlock_unlock(&slot->rwlock);

    return DRV_ERROR_NONE;
}

static int svm_pagefault_info_dev_init_post(u32 devid)
{
    struct svm_pagefault_info_slot *slot = NULL;

    if (devid >= SVM_MAX_AGENT_NUM) {
        return DRV_ERROR_NONE;
    }

    slot = &g_pagefault_info_records[devid];
    (void)pthread_rwlock_wrlock(&slot->rwlock);
    slot->dev_opened = true;
    (void)pthread_rwlock_unlock(&slot->rwlock);

    return DRV_ERROR_NONE;
}

static int svm_pagefault_info_dev_uninit_pre(u32 devid)
{
    if (devid >= SVM_MAX_AGENT_NUM) {
        return DRV_ERROR_NONE;
    }

    svm_pagefault_info_clear_internal(devid);

    return DRV_ERROR_NONE;
}

static int svm_pagefault_info_criu_reset(u32 devid, void *data)
{
    (void)data;

    if (devid >= SVM_MAX_AGENT_NUM) {
        return DRV_ERROR_NONE;
    }

    svm_pagefault_info_clear_internal(devid);

    return DRV_ERROR_NONE;
}

static const struct svm_criu_ops g_pagefault_info_criu_ops = {
    .name = "pagefault_info",
    .reset = svm_pagefault_info_criu_reset,
};

static int __attribute__((constructor)) svm_pagefault_info_init(void)
{
    uint32_t i;

    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        (void)pthread_rwlock_init(&g_pagefault_info_records[i].rwlock, NULL);
        g_pagefault_info_records[i].has_record = false;
        g_pagefault_info_records[i].dev_opened = false;
    }

    (void)svm_register_ioctl_dev_init_post_handle(svm_pagefault_info_dev_init_post);
    (void)svm_register_ioctl_dev_uninit_pre_handle(svm_pagefault_info_dev_uninit_pre);
    (void)svm_criu_register_ops(&g_pagefault_info_criu_ops);

    return 0;
}
