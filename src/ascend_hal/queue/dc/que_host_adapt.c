/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "que_compiler.h"
#include "queue.h"
#include "queue_interface.h"
#include "queue_agent.h"
#include "dms_user_interface.h"
#include "queue_feature.h"
#include "que_platform.h"

/* ============================================================================
 * queue_interface.c - Host specific implementations
 * ============================================================================ */

bool g_enable_local_queue = false;
bool g_default_client_queue = true;

unsigned int que_get_unified_devid_platform(unsigned int dev_id)
{
    drvError_t ret;
    (void)dev_id;
    unsigned int host_id;
    ret = halGetHostID(&host_id);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("drv update host_id failed. (ret=%d)\n", ret);
        return 0;
    }
    return host_id;
}

void que_check_convert_to_inter_dev_queue_platform(unsigned int dev_id, int *type)
{
    if ((*type == LOCAL_QUEUE) && (dev_id == halGetHostDevid())) {
        *type = INTER_DEV_QUEUE;
    }
}

int queue_query_support_inter_dev_que_platform(QueueQueryOutputPara *out_put)
{
#ifndef EMU_ST
    drvError_t ret;
    int64_t val;
    QueQuerySupportInterDevQue *out_buff = (QueQuerySupportInterDevQue *)out_put->outBuff;

    if ((out_put->outLen < sizeof(QueQuerySupportInterDevQue))) {
        QUEUE_LOG_ERR("para is invalid. (out_len=%u)\n", out_put->outLen);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dms_get_connect_type(&val);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("dms_get_connect_type failed. (ret=%d)\n", ret);
        return ret;
    }

    if (val == HOST_DEVICE_CONNECT_TYPE_UB) {
        out_buff->value = 1; /* 1:support */
    } else {
        out_buff->value = 0; /* 0:not support */
    }
#endif
    return DRV_ERROR_NONE;
}

void que_get_inter_dev_local_devid_platform(struct shareQueInfo *que_info, unsigned int dev_id,
                                            unsigned int *local_dev_id)
{
    que_info->peerDevId = dev_id;
    *local_dev_id = halGetHostDevid();
}

bool queue_inter_devid_invalid_platform(unsigned int dev_id)
{
    return (dev_id != halGetHostDevid());
}

bool queue_is_client_queue_deploy(void)
{
    return true;
}

/* ============================================================================
 * queue.c - Host specific implementations
 * ============================================================================ */
/* Device only function - empty implementation for Host */
void queue_send_wake_up_rsp_platform(struct queue_manages *que_manage, int rsp_ret)
{
    (void)que_manage;
    (void)rsp_ret;
}

unsigned int queue_get_submit_devid_platform(unsigned int submit_devid, unsigned int dst_engine, unsigned int dst_devid)
{
    (void)dst_engine;
    (void)dst_devid;
    return submit_devid;
}

drvError_t queue_get_event_src_udevid_platform(unsigned int dev_id, unsigned int dst_engine, unsigned int *src_udevid,
                                               int *src_location)
{
    drvError_t ret;
    (void)dst_engine;
    *src_location = EVENT_SRC_LOCATION_HOST;
    ret = uda_get_udevid_by_devid_ex(dev_id, src_udevid);
    return ret;
}

unsigned int queue_get_grp_dst_engine_platform(unsigned int dev_id, unsigned int group_id)
{
    (void)dev_id;
    (void)group_id;
    return CCPU_HOST;
}
