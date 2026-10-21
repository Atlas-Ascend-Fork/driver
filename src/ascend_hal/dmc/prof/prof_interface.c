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
#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"

#include "prof_common.h"
#include "prof_core.h"
#include "prof_adapt.h"

/******************************part1: prof channel list management interfaces******************************/
STATIC bool prof_is_support_register(uint32_t chan_id)
{
    uint32_t support_chan_list[] = {CHANNEL_NPU_MODULE_MEM, CHANNEL_AICPU, CHANNEL_CUS_AICPU, CHANNEL_ADPROF};
    uint64_t i;

    if (prof_is_v4_extra_channel(chan_id)) {
        return true;
    }

    for (i = 0; i < sizeof(support_chan_list) / sizeof(uint32_t); i++) {
        if (support_chan_list[i] == chan_id) {
            return true;
        }
    }

    return false;
}

STATIC bool prof_is_support_host_sample_register(uint32_t chan_id)
{
    if (!prof_is_support_host_sample()) {
        (void)chan_id;
        return false;
    }

#ifdef DRV_HOST
    uint32_t support_chan_list[] = {CHANNEL_STARS_SOC_LOG_BUFFER, CHANNEL_FFTS_PROFILE_BUFFER_TASK, CHANNEL_AICPU,
                                    CHANNEL_CUS_AICPU, CHANNEL_ADPROF};
#else
    uint32_t support_chan_list[] = {CHANNEL_AICPU, CHANNEL_CUS_AICPU, CHANNEL_ADPROF};
#endif

    uint64_t i;

    for (i = 0; i < sizeof(support_chan_list) / sizeof(uint32_t); i++) {
        if (support_chan_list[i] == chan_id) {
            return true;
        }
    }
    return false;
}

STATIC int prof_sample_register_inner(unsigned int dev_id, unsigned int chan_id, struct prof_sample_register_para *para,
                                      bool support_host_sample)
{
    drvError_t ret;

    ret = prof_core_register_channel(dev_id, chan_id, para, support_host_sample);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to register channel. (dev_id=%u, chan_id=%u, ret=%d, support_host_sample=%d)\n", dev_id,
                 chan_id, (int)ret, (int)support_host_sample);
        return (int)ret;
    }
    PROF_INFO("Register channel successfully. (dev_id=%u, chan_id=%u, support_host_sample=%d)\n", dev_id, chan_id,
              (int)support_host_sample);
    return 0;
}

int halProfSampleRegister(unsigned int dev_id, unsigned int chan_id, struct prof_sample_register_para *para)
{
    if ((dev_id >= DEV_NUM) || (chan_id >= PROF_CHANNEL_NUM_MAX) || (para == NULL)) {
        PROF_ERR("Invalid para. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    if (prof_is_support_register(chan_id) == false) {
        return (int)DRV_ERROR_NOT_SUPPORT;
    }

    return prof_sample_register_inner(dev_id, chan_id, para, false);
}

int halProfSampleRegisterEx(unsigned int dev_id, unsigned int chan_id, struct prof_sample_register_para *para)
{
    if ((dev_id >= DEV_NUM) || (chan_id >= PROF_CHANNEL_NUM_MAX) || (para == NULL)) {
        PROF_ERR("Invalid para. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    if (prof_is_support_host_sample_register(chan_id) == false) {
        return (int)DRV_ERROR_NOT_SUPPORT;
    }

    if ((para->ops.start_func == NULL) || (para->ops.stop_func == NULL)) {
        PROF_ERR("Invalid ops. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_INVALID_VALUE;
    }

#ifdef DRV_HOST
    if (para->ops.sample_func == NULL) {
        PROF_ERR("Invalid ops. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_INVALID_VALUE;
    }
#endif

    return prof_sample_register_inner(dev_id, chan_id, para, true);
}

int prof_drv_get_channels(unsigned int device_id, channel_list_t *channels)
{
    drvError_t ret;

    if ((device_id >= DEV_NUM) || (channels == NULL)) {
        PROF_ERR("Invalid para. (dev_id=%u)\n", device_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    ret = prof_core_get_channels(device_id, channels);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to get channels. (dev_id=%u, ret=%d)\n", device_id, (int)ret);
        return (int)ret;
    }

    PROF_INFO("Get channels successfully. (dev_id=%u)\n", device_id);
    return (int)DRV_ERROR_NONE;
}

int prof_channel_poll(struct prof_poll_info *out_buf, int num, int timeout)
{
    if ((out_buf == NULL) || (num > PROF_CHANNEL_NUM_MAX) || (num <= 0)) {
        PROF_ERR("Invalid para. (num=%d)\n", num);
        return PROF_ERROR;
    }

    return prof_core_poll_channels(out_buf, (uint32_t)num, timeout);
}

/******************************part2: prof channel operation interfaces******************************/
/* Specific error code: the error code returned from the kernel (-10, -11) */
int prof_drv_start(unsigned int device_id, unsigned int channel_id, struct prof_start_para *start_para)
{
    drvError_t ret;

    if ((device_id >= DEV_NUM) || (channel_id >= PROF_CHANNEL_NUM_MAX) || (start_para == NULL)) {
        PROF_ERR("Invalid para. (dev_id=%u, chan_id=%u)\n", device_id, channel_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    if ((start_para->user_data == NULL) && (start_para->user_data_size != 0)) {
        PROF_ERR("Invalid para. (dev_id=%u, chan_id=%u, data_size=%u)\n", device_id, channel_id,
                 start_para->user_data_size);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    ret = prof_core_chan_start(device_id, channel_id, start_para);
    if (ret != DRV_ERROR_NONE) {
        /* Logs cannot be recorded in special scenarios */
        return (int)ret;
    }

    PROF_INFO("Channel start successfully. (dev_id=%u, chan_id=%u)\n", device_id, channel_id);
    return (int)DRV_ERROR_NONE;
}

int prof_stop(unsigned int device_id, unsigned int channel_id)
{
    drvError_t ret;

    if ((device_id >= DEV_NUM) || (channel_id >= PROF_CHANNEL_NUM_MAX)) {
        PROF_ERR("Invalid para. (dev_id=%u, chan_id=%u)\n", device_id, channel_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    ret = prof_core_chan_stop(device_id, channel_id);
    if (ret != DRV_ERROR_NONE) {
        /* Logs cannot be recorded in special scenarios */
        return (int)ret;
    }

    PROF_INFO("Channel stop successfully. (dev_id=%u, chan_id=%u)\n", device_id, channel_id);
    return (int)DRV_ERROR_NONE;
}

/* Specific error code: the error code returned from the user or kernel (-4), DRV_ERROR_NOT_SUPPORT */
int halProfDataFlush(unsigned int device_id, unsigned int channel_id, unsigned int *data_len)
{
    drvError_t ret;

    if ((device_id >= DEV_NUM) || (channel_id >= PROF_CHANNEL_NUM_MAX) || (data_len == NULL)) {
        PROF_ERR("Invalid para. (dev_id=%u, chan_id=%u)\n", device_id, channel_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    ret = prof_core_chan_flush(device_id, channel_id, data_len);
    if (ret != PROF_OK) {
        /* Logs cannot be recorded in special scenarios */
        return (int)ret;
    }

    PROF_INFO("Channel flush successfully. (devid=%u, chan_id=%u, data_len=%u)\n", device_id, channel_id, *data_len);
    return (int)DRV_ERROR_NONE;
}

/* Specific error code: the error code returned from the user or kernel (-4) */
int prof_channel_read(unsigned int device_id, unsigned int channel_id, char *out_buf, unsigned int buf_size)
{
    if ((device_id >= DEV_NUM) || (channel_id >= PROF_CHANNEL_NUM_MAX) || (out_buf == NULL)) {
        PROF_ERR("Invalid para. (dev_id=%u, chan_id=%u)\n", device_id, channel_id);
        return PROF_ERROR;
    }

    return prof_core_chan_read(device_id, channel_id, out_buf, buf_size);
}

int halProfQueryAvailBufLen(unsigned int dev_id, unsigned int chan_id, unsigned int *buff_avail_len)
{
    drvError_t ret;

    if ((dev_id >= DEV_NUM) || (chan_id >= PROF_CHANNEL_NUM_MAX) || (buff_avail_len == NULL)) {
        PROF_ERR("Invalid para. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    ret = prof_core_chan_query(dev_id, chan_id, buff_avail_len);
    if (ret != DRV_ERROR_NONE) {
        /* Logs cannot be recorded in special scenarios */
        return (int)ret;
    }

    PROF_DEBUG("Channel query successfully. (dev_id=%u, chan_id=%u, avail_len=%u)\n", dev_id, chan_id, *buff_avail_len);
    return (int)DRV_ERROR_NONE;
}

int halProfSampleDataReport(unsigned int dev_id, unsigned int chan_id, unsigned int sub_chan_id,
                            struct prof_data_report_para *para)
{
    (void)sub_chan_id;
    drvError_t ret;

    if ((dev_id >= DEV_NUM) || (chan_id >= PROF_CHANNEL_NUM_MAX) || (para == NULL)) {
        PROF_ERR("Invalid para. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    if ((para->data == NULL) || (para->data_len == 0)) {
        PROF_ERR("Invalid para. (dev_id=%u, chan_id=%u, data_len=%u)\n", dev_id, chan_id, para->data_len);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    ret = prof_core_chan_report(dev_id, chan_id, para->data, para->data_len);
    if (ret != DRV_ERROR_NONE) {
        /* Logs cannot be recorded in special scenarios */
        return (int)ret;
    }

    return (int)DRV_ERROR_NONE;
}

#else
int prof_interface_ut_test(void)
{
    return 0;
}
#endif
