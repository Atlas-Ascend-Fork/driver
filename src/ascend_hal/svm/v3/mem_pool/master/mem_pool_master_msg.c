/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include "svm_umc_client.h"
#include "svm_sub_event_type_uk_msg.h"
#include "svm_dbi.h"
#include "svm_apbi.h"
#include "mem_pool_uk_msg.h"
#include "mem_pool_client.h"

static int _mem_pool_msg_send_remote(u32 devid, u32 event_id, void *msg_data, u32 msg_len, const char *err_log_fmt, ...)
{
    struct svm_umc_msg_head head;
    struct svm_umc_msg msg = {
        .msg_in = (char *)msg_data, .msg_in_len = msg_len, .msg_out = (char *)msg_data, .msg_out_len = msg_len};
    struct svm_apbi apbi;
    int ret;

    ret = svm_apbi_query(devid, DEVDRV_PROCESS_CP1, &apbi);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_umc_msg_head_pack(devid, apbi.tgid, apbi.grp_id, event_id, &head);

    ret = svm_umc_h2d_send(&head, &msg);
    if (ret != DRV_ERROR_NONE) {
        if (ret == DRV_ERROR_NO_PROCESS) {
            (void)svm_apbi_clear(devid, DEVDRV_PROCESS_CP1);
        }

        svm_soma_err("%s failed. (devid=%u; devpid=%d; ret=%d)\n", err_log_fmt, devid, apbi.tgid, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int mem_pool_client_create(u32 devid, u64 pool_id, u64 va, u64 size, u64 global_va)
{
    struct mem_pool_create_msg msg = {.pool_id = pool_id, .va = va, .size = size, .global_va = global_va};

    return _mem_pool_msg_send_remote(devid, SVM_SOMA_POOL_CREATE_EVENT, &msg, sizeof(msg), "Pool create msg");
}

int mem_pool_client_destory(u32 devid, u64 pool_id, u64 *va)
{
    struct mem_pool_destroy_msg msg = {.pool_id = pool_id};
    int ret;

    ret = _mem_pool_msg_send_remote(devid, SVM_SOMA_POOL_DESTROY_EVENT, &msg, sizeof(msg), "Pool destroy msg");
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    *va = msg.va;
    return DRV_ERROR_NONE;
}

int mem_pool_client_get_attr(u32 devid, u64 pool_id, u64 attr, u64 *value)
{
    struct mem_pool_get_attr_msg msg = {.pool_id = pool_id, .attr = attr, .value = 0};
    int ret;

    ret = _mem_pool_msg_send_remote(devid, SVM_SOMA_POOL_GET_ATTR_EVENT, &msg, sizeof(msg), "Pool get attr msg");
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    *value = msg.value;
    return DRV_ERROR_NONE;
}

int mem_pool_client_set_attr(u32 devid, u64 pool_id, u64 attr, u64 value)
{
    struct mem_pool_set_attr_msg msg = {.pool_id = pool_id, .attr = attr, .value = value};

    return _mem_pool_msg_send_remote(devid, SVM_SOMA_POOL_SET_ATTR_EVENT, &msg, sizeof(msg), "Pool set attr msg");
}

int mem_pool_client_mem_free(u32 devid, u64 pool_id, u64 size, u64 va)
{
    struct mem_pool_mem_free_msg msg = {.pool_id = pool_id, .va = va, .size = size};

    return _mem_pool_msg_send_remote(devid, SVM_SOMA_POOL_SYNC_FREE_EVENT, &msg, sizeof(msg), "Pool memFree msg");
}
