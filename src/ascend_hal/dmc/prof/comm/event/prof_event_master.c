/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef UT_TEST
#include <string.h>
#include <stdlib.h>

#include "securec.h"
#include "ascend_hal.h"
#include "prof_event_master.h"

STATIC drvError_t prof_event_fill_start_msg(struct prof_start_event_msg *start_msg, uint32_t dev_id, uint32_t chan_id,
                                            struct prof_user_start_para *para)
{
    int ret;

    start_msg->chan_id = chan_id;
    start_msg->sample_period = para->sample_period;
    if (para->user_data_size != 0) {
        ret = memcpy_s(start_msg->data, PROF_EVENT_DATA_SIZE_MAX, para->user_data, para->user_data_size);
        if (ret != EOK) {
            PROF_ERR("Failed to memcpy_s user_data. (dev_id=%u, chan_id=%u, user_data_len=%u, ret=%d)\n", dev_id,
                     chan_id, para->user_data_size, ret);
            return DRV_ERROR_MEMORY_OPT_FAIL;
        }
        start_msg->data_len = para->user_data_size;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_event_fill_host_sample_start_msg(struct prof_host_sample_start_event_msg *start_msg,
                                                        uint32_t dev_id, uint32_t chan_id,
                                                        struct prof_user_start_para *para)
{
    int ret;

    start_msg->chan_id = chan_id;
    start_msg->sample_period = para->sample_period;
    if (para->addr_data_len != 0) {
        ret = memcpy_s(start_msg->addrdata, PROF_USER_ADDR_DATA_LEN, para->addrdata, para->addr_data_len);
        if (ret != 0) {
            PROF_ERR("Failed to memcpy_s addr_data. (dev_id=%u, chan_id=%u, addr_data_len=%u, ret=%d)\n", dev_id,
                     chan_id, para->addr_data_len, ret);
            return DRV_ERROR_MEMORY_OPT_FAIL;
        }
        start_msg->addr_data_len = para->addr_data_len;
    }

    if (para->user_data_size != 0) {
        ret = memcpy_s(start_msg->data, PROF_USER_DATA_LEN, para->user_data, para->user_data_size);
        if (ret != 0) {
            PROF_ERR("Failed to memcpy_s user_data. (dev_id=%u, chan_id=%u, user_data_len=%u, ret=%d)\n", dev_id,
                     chan_id, para->user_data_size, ret);
            return DRV_ERROR_MEMORY_OPT_FAIL;
        }
        start_msg->data_len = para->user_data_size;
    }

    return DRV_ERROR_NONE;
}

drvError_t prof_event_start(uint32_t dev_id, uint32_t chan_id, struct prof_user_start_para *para)
{
    struct prof_start_event_msg start_msg = {0};
    struct prof_event_para event_para = {0};
    drvError_t drv_ret;

    drv_ret = prof_event_fill_start_msg(&start_msg, dev_id, chan_id, para);
    if (drv_ret != DRV_ERROR_NONE) {
        return drv_ret;
    }

    prof_event_event_para_comm_pack(&event_para, dev_id, EVENT_DRV_MSG, DRV_SUBEVENT_PROF_START_MSG, para->remote_pid);
    prof_event_event_para_msg_send_pack(&event_para, (char *)&start_msg, sizeof(struct prof_start_event_msg));
    prof_event_event_para_msg_recv_pack(&event_para, NULL, EVENT_PROC_RSP_LEN);

    drv_ret = prof_event_submit_event_sync(&event_para);
    if (drv_ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to sync prof event. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)drv_ret);
    }

    return drv_ret;
}

drvError_t prof_event_stop(uint32_t dev_id, uint32_t chan_id, struct prof_user_stop_para *para)
{
    struct prof_stop_event_msg stop_msg = {0};
    struct prof_event_para event_para = {0};
    int ret;

    stop_msg.chan_id = chan_id;

    prof_event_event_para_comm_pack(&event_para, dev_id, EVENT_DRV_MSG, DRV_SUBEVENT_PROF_STOP_MSG, para->remote_pid);
    prof_event_event_para_msg_send_pack(&event_para, (char *)&stop_msg, sizeof(struct prof_stop_event_msg));
    prof_event_event_para_msg_recv_pack(&event_para, (char *)para->report, para->report_len);

    ret = prof_event_submit_event_sync(&event_para);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to sync prof event. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    PROF_INFO("Recv reply ok. (dev_id=%u, chan_id=%u, remote_pid=%u)\n", dev_id, chan_id, para->remote_pid);
    return ret;
}

drvError_t prof_user_host_sample_event_stop(uint32_t dev_id, uint32_t chan_id, struct prof_user_stop_para *para,
                                            uint32_t release_flag)
{
    struct prof_host_sample_stop_event_msg stop_msg = {0};
    struct prof_event_para event_para = {0};
    drvError_t ret;

    stop_msg.chan_id = chan_id;
    stop_msg.release_flag = release_flag;

    prof_event_event_para_comm_pack(&event_para, dev_id, EVENT_DRV_MSG_EX, DRV_SUBEVENT_PROF_HOST_SAMPLE_STOP_MSG,
                                    para->remote_pid);
    prof_event_event_para_msg_send_pack(&event_para, (char *)&stop_msg, sizeof(struct prof_host_sample_stop_event_msg));
    prof_event_event_para_msg_recv_pack(&event_para, (char *)para->report, para->report_len);

    ret = prof_event_submit_event_sync(&event_para);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to sync prof event. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    PROF_INFO("Recv stop reply msg ok. (dev_id=%u, chan_id=%u, remote_pid=%u, release_flag=%d)\n", dev_id, chan_id,
              para->remote_pid, (int)release_flag);
    return ret;
}

drvError_t prof_user_host_sample_event_start(uint32_t dev_id, uint32_t chan_id, struct prof_user_start_para *para,
                                             struct prof_start_event_out_msg *outdata)
{
    struct prof_host_sample_start_event_msg start_msg = {0};
    struct prof_event_para event_para = {0};
    drvError_t drv_ret;
    struct prof_user_stop_para stop_para = {0};
    stop_para.remote_pid = para->remote_pid;

    drv_ret = prof_event_fill_host_sample_start_msg(&start_msg, dev_id, chan_id, para);
    if (drv_ret != DRV_ERROR_NONE) {
        return drv_ret;
    }

    prof_event_event_para_comm_pack(&event_para, dev_id, EVENT_DRV_MSG_EX, DRV_SUBEVENT_PROF_HOST_SAMPLE_START_MSG,
                                    para->remote_pid);
    prof_event_event_para_msg_send_pack(&event_para, (char *)&start_msg,
                                        sizeof(struct prof_host_sample_start_event_msg));
    prof_event_event_para_msg_recv_pack(&event_para, (char *)outdata, sizeof(struct prof_start_event_out_msg));

    drv_ret = prof_event_submit_event_sync(&event_para);
    if (drv_ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to sync prof event. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)drv_ret);
        return drv_ret;
    }

    if (outdata->outdata_len > PROF_START_OUTDATA_SIZE_MAX) {
        (void)prof_user_host_sample_event_stop(dev_id, chan_id, &stop_para, PROF_STOP_STAGE_PAUSE_AND_RELEASE);
        PROF_ERR("Out data len is out of range. (dev_id=%u, chan_id=%u, outdata_len=%d, max=%u)\n", dev_id, chan_id,
                 outdata->outdata_len, PROF_START_OUTDATA_SIZE_MAX);
        return DRV_ERROR_PARA_ERROR;
    }

    return (drvError_t)0;
}

#else
int prof_event_master_ut_test(void)
{
    return 0;
}
#endif
