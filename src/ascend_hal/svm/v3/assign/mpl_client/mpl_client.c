/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ascend_hal.h"

#include "svm_pub.h"
#include "svm_log.h"
#include "svm_user_adapt.h"
#include "svm_umc_client.h"
#include "svm_sub_event_type_uk_msg.h"
#include "svm_dbi.h"
#include "svm_apbi.h"
#include "mpl_uk_msg.h"
#include "mpl_client.h"
#include "mpl.h"

static int svm_mpl_umc_msg_send(u32 flag, struct svm_umc_msg_head *head, struct svm_umc_msg *msg)
{
    if ((flag & SVM_MPL_FLAG_NO_REMOTE_OPS) != 0) {
        return svm_umc_h2d_send_async(head, msg);
    }

    return svm_umc_h2d_send(head, msg);
}

static int svm_mpl_populate_remote(u32 devid, u64 va, u64 size, u32 flag, bool is_no_pin)
{
    struct svm_umc_msg_head head;
    struct svm_mpl_populate_msg pop_msg = {.size = size, .flag = flag, .va = va};
    struct svm_umc_msg msg = {.msg_in = (char *)(uintptr_t)&pop_msg,
                              .msg_in_len = sizeof(struct svm_mpl_populate_msg),
                              .msg_out = (char *)(uintptr_t)&pop_msg,
                              .msg_out_len = sizeof(struct svm_mpl_populate_msg)};
    struct svm_apbi apbi;
    u32 event_id = is_no_pin ? SVM_MPL_POPULATE_NO_PIN_EVENT : SVM_MPL_POPULATE_EVENT;
    int ret;

    ret = svm_apbi_query(devid, DEVDRV_PROCESS_CP1, &apbi);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_umc_msg_head_pack(devid, apbi.tgid, apbi.grp_id, event_id, &head);
    ret = svm_mpl_umc_msg_send(flag, &head, &msg);
    if (ret != DRV_ERROR_NONE) {
        if (ret == DRV_ERROR_NO_PROCESS) {
            (void)svm_apbi_clear(devid, DEVDRV_PROCESS_CP1);
        }
        svm_err_if((ret != DRV_ERROR_OUT_OF_MEMORY), "Mpl populate msg failed. (devid=%u; devpid=%d; ret=%d)\n", devid,
                   apbi.tgid, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int svm_mpl_depopulate_remote(u32 devid, u64 va, u64 size, u32 flag, bool is_no_pin)
{
    struct svm_umc_msg_head head;
    struct svm_mpl_depopulate_msg depop_msg = {.size = size, .va = va, .is_busy = 0, .flag = flag};
    struct svm_umc_msg msg = {.msg_in = (char *)(uintptr_t)&depop_msg,
                              .msg_in_len = sizeof(struct svm_mpl_depopulate_msg),
                              .msg_out = (char *)(uintptr_t)&depop_msg,
                              .msg_out_len = sizeof(struct svm_mpl_depopulate_msg)};
    struct svm_apbi apbi;
    u32 event_id = is_no_pin ? SVM_MPL_DEPOPULATE_NO_UNPIN_EVENT : SVM_MPL_DEPOPULATE_EVENT;
    int ret;

    ret = svm_apbi_query(devid, DEVDRV_PROCESS_CP1, &apbi);
    if (ret != DRV_ERROR_NONE) {
        /* process exit, no need msg to depopulate, return ok derictly */
        return (ret == DRV_ERROR_NO_PROCESS) ? DRV_ERROR_NONE : ret;
    }

    svm_umc_msg_head_pack(devid, apbi.tgid, apbi.grp_id, event_id, &head);
    ret = svm_mpl_umc_msg_send(flag, &head, &msg);
    if (ret != DRV_ERROR_NONE) {
        if (ret == DRV_ERROR_NO_PROCESS) {
            (void)svm_apbi_clear(devid, DEVDRV_PROCESS_CP1);
        }
        svm_warn("Mpl depopulate msg failed. (devid=%u; devpid=%d; ret=%d)\n", devid, apbi.tgid, ret);
        return (ret == DRV_ERROR_NO_PROCESS) ? DRV_ERROR_NONE : ret;
    }

    return ((depop_msg.is_busy == 1) ? DRV_ERROR_BUSY : DRV_ERROR_NONE);
}

static int _svm_mpl_client_populate(u32 devid, u64 va, u64 size, u32 flag, bool is_no_pin)
{
    if (devid == svm_get_host_devid()) {
        return svm_mpl_populate(devid, va, size, flag);
    } else {
        return svm_mpl_populate_remote(devid, va, size, flag, is_no_pin);
    }
}

int svm_mpl_client_populate(u32 devid, u64 va, u64 size, u32 flag)
{
    return _svm_mpl_client_populate(devid, va, size, flag, false);
}

/* host mem not support repair */
int svm_mpl_client_populate_no_pin(u32 devid, u64 va, u64 size, u32 flag)
{
    return svm_mpl_populate_remote(devid, va, size, flag | SVM_MPL_FLAG_NO_PIN, true);
}

static int _svm_mpl_client_depopulate(u32 devid, u64 va, u64 size, u32 flag, bool is_no_unpin)
{
    if (devid == svm_get_host_devid()) {
        return svm_mpl_depopulate(devid, va, size, flag);
    } else {
        return svm_mpl_depopulate_remote(devid, va, size, flag, is_no_unpin);
    }
}

int svm_mpl_client_depopulate(u32 devid, u64 va, u64 size, u32 flag)
{
    return _svm_mpl_client_depopulate(devid, va, size, flag, false);
}

/* host mem not support repair */
int svm_mpl_client_depopulate_no_unpin(u32 devid, u64 va, u64 size, u32 flag)
{
    return svm_mpl_depopulate_remote(devid, va, size, flag | SVM_MPL_FLAG_NO_PIN, true);
}
