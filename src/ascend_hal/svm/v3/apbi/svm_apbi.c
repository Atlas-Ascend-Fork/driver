/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <limits.h>

#include "ascend_hal.h"

#include "svm_pub.h"
#include "svm_log.h"
#include "svm_init_pri.h"
#include "svm_criu.h"
#include "svm_user_adapt.h"
#include "svm_sys_cmd.h"
#include "svm_event_grp_id.h"
#include "svm_dbi.h"
#include "svm_apbi.h"

#define APBI_INVALID_GRP_ID UINT_MAX

struct svm_apbi g_apbi[SVM_MAX_DEV_NUM][DEVDRV_PROCESS_CPTYPE_MAX];

static int apbi_query(u32 devid, int task_type, struct svm_apbi *apbi)
{
    if (g_apbi[devid][task_type].tgid == 0) {
        return DRV_ERROR_NO_PROCESS;
    }

    *apbi = g_apbi[devid][task_type];
    return DRV_ERROR_NONE;
}

static void _apbi_clear(u32 devid, int task_type)
{
    g_apbi[devid][task_type].grp_id = APBI_INVALID_GRP_ID;
    g_apbi[devid][task_type].tgid = 0;
}

static int apbi_clear(u32 devid)
{
    int task_type;

    for (task_type = 0; task_type < DEVDRV_PROCESS_CPTYPE_MAX; task_type++) {
        _apbi_clear(devid, task_type);
    }

    return DRV_ERROR_NONE;
}

static int apbi_update_tgid(u32 devid, int task_type)
{
    struct halQueryDevpidInfo info = {.proc_type = task_type, .hostpid = svm_getpid(), .devid = devid, .vfid = 0};
    int ret;

    ret = halQueryDevpid(info, &g_apbi[devid][task_type].tgid);
    if (ret != DRV_ERROR_NONE) {
        g_apbi[devid][task_type].tgid = 0;
        /* The log cannot be modified, because in the failure mode library. */
        svm_err_if(ret != DRV_ERROR_NO_PROCESS, "Call halQueryDevpid failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    if ((task_type != DEVDRV_PROCESS_CP1) &&
        (g_apbi[devid][task_type].tgid == g_apbi[devid][DEVDRV_PROCESS_CP1].tgid)) {
        svm_warn("Same as cp tgid. (devid=%u; task_type=%d; tpid=%d)\n", devid, task_type,
                 g_apbi[devid][task_type].tgid);
        _apbi_clear(devid, task_type);
        return DRV_ERROR_NO_PROCESS;
    }

    return DRV_ERROR_NONE;
}

static int apbi_update_grp_id(u32 devid, int task_type)
{
    int tgid = g_apbi[devid][task_type].tgid;
    int ret;

    ret = svm_event_get_remote_grp_id(devid, tgid, &g_apbi[devid][task_type].grp_id);
    if (ret != DRV_ERROR_NONE) {
        g_apbi[devid][task_type].grp_id = APBI_INVALID_GRP_ID;
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int apbi_update(u32 devid, int task_type)
{
    int ret;

    /* Init tgid firstly, others apbi will use tgid. */
    ret = apbi_update_tgid(devid, task_type);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = apbi_update_grp_id(devid, task_type);
    if (ret != DRV_ERROR_NONE) {
        _apbi_clear(devid, task_type);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int apbi_update_cp1(u32 devid)
{
    int ret;

    if (devid == svm_get_host_devid()) {
        return DRV_ERROR_NONE;
    }

    ret = apbi_update(devid, DEVDRV_PROCESS_CP1);
    if (ret != 0) {
        svm_err("Update cp1 basic info failed. (ret=%d)\n", ret);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return DRV_ERROR_NONE;
}

static void apbi_clear_all(void)
{
    u32 devid;

    for (devid = 0; devid < SVM_MAX_DEV_NUM; devid++) {
        (void)apbi_clear(devid);
    }
}

static int svm_apbi_criu_reset(u32 devid, void *data)
{
    SVM_UNUSED(data);
    return apbi_clear(devid);
}

static const struct svm_criu_ops g_apbi_criu_ops = {
    .name = "apbi",
    .reset = svm_apbi_criu_reset,
};

static void __attribute__((constructor(SVM_INIT_PRI_FISRT))) apbi_init(void)
{
    int ret;

    apbi_clear_all();

    ret = svm_register_ioctl_dev_init_post_handle(apbi_update_cp1);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev init post handle failed.\n");
    }

    ret = svm_register_ioctl_dev_uninit_pre_handle(apbi_clear);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev uninit pre handle failed.\n");
    }

    ret = svm_criu_register_ops(&g_apbi_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed.\n");
    }
}

int svm_apbi_update(u32 devid, int task_type)
{
    if ((devid >= SVM_MAX_DEV_NUM) || (task_type >= DEVDRV_PROCESS_CPTYPE_MAX) || (task_type < 0)) {
        svm_err("Invalid para. (devid=%u; task_type=%d)\n", devid, task_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return apbi_update(devid, task_type);
}

int svm_apbi_query(u32 devid, int task_type, struct svm_apbi *apbi)
{
    if ((devid >= SVM_MAX_DEV_NUM) || (task_type >= DEVDRV_PROCESS_CPTYPE_MAX) || (task_type < 0)) {
        svm_err("Invalid para. (devid=%u; task_type=%d)\n", devid, task_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return apbi_query(devid, task_type, apbi);
}

void svm_apbi_clear(u32 devid, int task_type)
{
    if ((devid >= SVM_MAX_DEV_NUM) || (task_type >= DEVDRV_PROCESS_CPTYPE_MAX) || (task_type < 0)) {
        return;
    }

    _apbi_clear(devid, task_type);
}
