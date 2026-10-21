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
#include <stdbool.h>
#include <stdint.h>

#include "ascend_hal_error.h"

#include "svm_pub.h"
#include "svm_log.h"
#include "svm_criu.h"

#define SVM_CRIU_OPS_MAX_NUM 32U

enum svm_criu_action {
    SVM_CRIU_ACTION_BACKUP = 0,
    SVM_CRIU_ACTION_RESET,
    SVM_CRIU_ACTION_RESTORE,
};

enum svm_criu_phase {
    SVM_CRIU_PHASE_IDLE = 0,
    SVM_CRIU_PHASE_RESET,
    SVM_CRIU_PHASE_DEVICE_REOPEN,
    SVM_CRIU_PHASE_RESTORE,
};

static pthread_rwlock_t g_svm_criu_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static const struct svm_criu_ops *g_svm_criu_ops[SVM_CRIU_OPS_MAX_NUM] = {NULL};
static enum svm_criu_phase g_svm_criu_phase[SVM_MAX_DEV_NUM] = {SVM_CRIU_PHASE_IDLE};

static bool svm_criu_devid_is_out_of_range(u32 devid)
{
    return (devid >= SVM_MAX_DEV_NUM);
}

static void svm_criu_set_phase(u32 devid, enum svm_criu_phase phase)
{
    if (!svm_criu_devid_is_out_of_range(devid)) {
        g_svm_criu_phase[devid] = phase;
    }
}

static bool svm_criu_is_phase(u32 devid, enum svm_criu_phase phase)
{
    if (svm_criu_devid_is_out_of_range(devid)) {
        return false;
    }

    return (g_svm_criu_phase[devid] == phase);
}

static const char *svm_criu_get_ops_name(const struct svm_criu_ops *ops)
{
    return ((ops == NULL) || (ops->name == NULL)) ? "unknown" : ops->name;
}

static bool svm_criu_ops_is_invalid(const struct svm_criu_ops *ops)
{
    return (ops == NULL) || ((ops->backup == NULL) && (ops->reset == NULL) && (ops->restore == NULL));
}

static const char *svm_criu_action_to_str(enum svm_criu_action action)
{
    switch (action) {
        case SVM_CRIU_ACTION_BACKUP:
            return "backup";
        case SVM_CRIU_ACTION_RESET:
            return "reset";
        case SVM_CRIU_ACTION_RESTORE:
            return "restore";
        default:
            return "unknown";
    }
}

static int svm_criu_call_ops(const struct svm_criu_ops *ops, u32 devid, enum svm_criu_action action, void *data)
{
    switch (action) {
        case SVM_CRIU_ACTION_BACKUP:
            return (ops->backup == NULL) ? DRV_ERROR_NONE : ops->backup(devid, data);
        case SVM_CRIU_ACTION_RESET:
            return (ops->reset == NULL) ? DRV_ERROR_NONE : ops->reset(devid, data);
        case SVM_CRIU_ACTION_RESTORE:
            return (ops->restore == NULL) ? DRV_ERROR_NONE : ops->restore(devid, data);
        default:
            return DRV_ERROR_INVALID_VALUE;
    }
}

static int svm_criu_collect_ops(const struct svm_criu_ops *ops_array[], u32 *ops_num_out)
{
    u32 i;
    u32 ops_num = 0;

    (void)pthread_rwlock_rdlock(&g_svm_criu_rwlock);
    for (i = 0; i < SVM_CRIU_OPS_MAX_NUM; ++i) {
        if (g_svm_criu_ops[i] != NULL) {
            ops_array[ops_num++] = g_svm_criu_ops[i];
        }
    }
    (void)pthread_rwlock_unlock(&g_svm_criu_rwlock);

    *ops_num_out = ops_num;
    return DRV_ERROR_NONE;
}

static int svm_criu_do_action(u32 devid, enum svm_criu_action action, void *data)
{
    const struct svm_criu_ops *ops_array[SVM_CRIU_OPS_MAX_NUM] = {NULL};
    u32 ops_num = 0;
    u32 i;
    int ret;

    ret = svm_criu_collect_ops(ops_array, &ops_num);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (action == SVM_CRIU_ACTION_RESET) {
        for (i = ops_num; i > 0; --i) {
            ret = svm_criu_call_ops(ops_array[i - 1], devid, action, data);
            if (ret != DRV_ERROR_NONE) {
                svm_err("CRIU action failed. (action=%s; devid=%u; module=%s; ret=%d)\n",
                        svm_criu_action_to_str(action), devid, svm_criu_get_ops_name(ops_array[i - 1]), ret);
                return ret;
            }
            svm_info("CRIU action success. (action=%s; devid=%u; module=%s)\n", svm_criu_action_to_str(action), devid,
                     svm_criu_get_ops_name(ops_array[i - 1]));
        }
    } else {
        for (i = 0; i < ops_num; ++i) {
            ret = svm_criu_call_ops(ops_array[i], devid, action, data);
            if (ret != DRV_ERROR_NONE) {
                svm_err("CRIU action failed. (action=%s; devid=%u; module=%s; ret=%d)\n",
                        svm_criu_action_to_str(action), devid, svm_criu_get_ops_name(ops_array[i]), ret);
                return ret;
            }
            svm_info("CRIU action success. (action=%s; devid=%u; module=%s)\n", svm_criu_action_to_str(action), devid,
                     svm_criu_get_ops_name(ops_array[i]));
        }
    }

    return DRV_ERROR_NONE;
}
static int _svm_criu_register_ops(const struct svm_criu_ops *ops)
{
    u32 i;

    for (i = 0; i < SVM_CRIU_OPS_MAX_NUM; ++i) {
        if (g_svm_criu_ops[i] == ops) {
            svm_err("Repeat register CRIU ops. (module=%s)\n", svm_criu_get_ops_name(ops));
            return DRV_ERROR_REPEATED_INIT;
        }
    }

    for (i = 0; i < SVM_CRIU_OPS_MAX_NUM; ++i) {
        if (g_svm_criu_ops[i] == NULL) {
            g_svm_criu_ops[i] = ops;
            return DRV_ERROR_NONE;
        }
    }

    svm_err("No slot for CRIU ops. (module=%s; max_num=%u)\n", svm_criu_get_ops_name(ops), SVM_CRIU_OPS_MAX_NUM);
    return DRV_ERROR_OUT_OF_MEMORY;
}

static int _svm_criu_unregister_ops(const struct svm_criu_ops *ops)
{
    u32 i;

    for (i = 0; i < SVM_CRIU_OPS_MAX_NUM; ++i) {
        if (g_svm_criu_ops[i] == ops) {
            g_svm_criu_ops[i] = NULL;
            return DRV_ERROR_NONE;
        }
    }

    svm_err("No such CRIU ops. (module=%s)\n", svm_criu_get_ops_name(ops));
    return DRV_ERROR_INVALID_VALUE;
}

int svm_criu_register_ops(const struct svm_criu_ops *ops)
{
    int ret;

    if (svm_criu_ops_is_invalid(ops)) {
        svm_err("Invalid CRIU ops. (module=%s)\n", svm_criu_get_ops_name(ops));
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_rwlock_wrlock(&g_svm_criu_rwlock);
    ret = _svm_criu_register_ops(ops);
    (void)pthread_rwlock_unlock(&g_svm_criu_rwlock);

    return ret;
}

int svm_criu_unregister_ops(const struct svm_criu_ops *ops)
{
    int ret;

    if (ops == NULL) {
        svm_err("Invalid CRIU ops. (module=unknown)\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_rwlock_wrlock(&g_svm_criu_rwlock);
    ret = _svm_criu_unregister_ops(ops);
    (void)pthread_rwlock_unlock(&g_svm_criu_rwlock);

    return ret;
}

int svm_criu_backup(u32 devid, void *data)
{
    if (svm_criu_devid_is_out_of_range(devid)) {
        svm_err("Invalid CRIU devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return svm_criu_do_action(devid, SVM_CRIU_ACTION_BACKUP, data);
}

int svm_criu_reset(u32 devid)
{
    int ret;

    if (svm_criu_devid_is_out_of_range(devid)) {
        svm_err("Invalid CRIU devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_criu_set_phase(devid, SVM_CRIU_PHASE_RESET);
    ret = svm_criu_do_action(devid, SVM_CRIU_ACTION_RESET, NULL);
    svm_criu_set_phase(devid, (ret == DRV_ERROR_NONE) ? SVM_CRIU_PHASE_DEVICE_REOPEN : SVM_CRIU_PHASE_IDLE);

    return ret;
}

int svm_criu_restore(u32 devid, void *data)
{
    int ret;

    if (svm_criu_devid_is_out_of_range(devid)) {
        svm_err("Invalid CRIU devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_criu_set_phase(devid, SVM_CRIU_PHASE_RESTORE);
    ret = svm_criu_do_action(devid, SVM_CRIU_ACTION_RESTORE, data);
    svm_criu_set_phase(devid, SVM_CRIU_PHASE_IDLE);

    return ret;
}

bool svm_criu_is_resetting(u32 devid)
{
    return svm_criu_is_phase(devid, SVM_CRIU_PHASE_RESET);
}

bool svm_criu_is_device_reopen(u32 devid)
{
    return svm_criu_is_phase(devid, SVM_CRIU_PHASE_DEVICE_REOPEN);
}
