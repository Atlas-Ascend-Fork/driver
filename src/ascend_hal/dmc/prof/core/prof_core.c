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

#include "prof_common.h"
#include "prof_chan_list.h"
#include "prof_chan.h"
#include "prof_poll.h"
#include "prof_adapt.h"
#include "prof_communication.h"
#include "prof_core.h"

drvError_t prof_core_register_channel(uint32_t dev_id, uint32_t chan_id, struct prof_sample_register_para *para,
                                      bool support_host_sample)
{
    drvError_t ret;

    /* Handling concurrency and reentrancy */
    ret = prof_add_local_channel(dev_id, chan_id, support_host_sample);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = prof_adapt_register_channel(dev_id, chan_id, para, support_host_sample);
    if (ret != DRV_ERROR_NONE) {
        prof_del_local_channel(dev_id, chan_id, support_host_sample);
        PROF_ERR("Failed to register channel to adapt. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t prof_core_get_channels(uint32_t dev_id, struct channel_list *channels)
{
    drvError_t ret;

    ret = prof_update_chan_list(dev_id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    prof_get_chan_list(dev_id, channels);

    return DRV_ERROR_NONE;
}

int prof_core_poll_channels(struct prof_poll_info *out_buf, uint32_t num, int timeout)
{
    return prof_poll_read(out_buf, num, timeout);
}

drvError_t prof_core_chan_start(uint32_t dev_id, uint32_t chan_id, struct prof_start_para *start_para)
{
    struct prof_user_start_para para = {0};
    drvError_t ret;

    para.user_data = start_para->user_data;
    para.user_data_size = start_para->user_data_size;
    para.sample_period = start_para->sample_period;
    para.support_host_sample = false;
    ret = prof_chan_start(dev_id, chan_id, &para, false);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    prof_poll_chan_start(dev_id, chan_id);

    return DRV_ERROR_NONE;
}

drvError_t prof_core_chan_stop(uint32_t dev_id, uint32_t chan_id)
{
    struct prof_user_stop_para para = {0};
    drvError_t ret;

    para.support_host_sample = false;
    para.host_sample_release_flag = PROF_STOP_STAGE_DEFAULT;
    ret = prof_chan_stop(dev_id, chan_id, &para, false);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    prof_poll_chan_stop(dev_id, chan_id);

    return para.h2d_ret;
}

drvError_t prof_core_chan_flush(uint32_t dev_id, uint32_t chan_id, uint32_t *data_len)
{
    return prof_chan_flush(dev_id, chan_id, data_len);
}

int prof_core_chan_read(uint32_t dev_id, uint32_t chan_id, char *out_buf, uint32_t buf_size)
{
    return prof_chan_read(dev_id, chan_id, out_buf, buf_size);
}

drvError_t prof_core_chan_query(uint32_t dev_id, uint32_t chan_id, uint32_t *avail_len)
{
    return prof_chan_query(dev_id, chan_id, avail_len);
}

drvError_t prof_core_chan_report(uint32_t dev_id, uint32_t chan_id, void *data, uint32_t data_len)
{
    return prof_chan_report(dev_id, chan_id, data, data_len, true);
}

static void __attribute__((constructor)) prof_core_callback_register(void)
{
    struct prof_adapt_core_notifier adapt_notifier = {
        .poll_report = prof_poll_report,
        .chan_report = prof_chan_report,
    };

    struct prof_comm_core_notifier comm_notifier = {
        .chan_start = prof_chan_start,
        .chan_stop = prof_chan_stop,
        .chan_report = prof_chan_report,
    };

    prof_adapt_register_notifier(&adapt_notifier);
    prof_comm_register_notifier(&comm_notifier);
}

#else
int prof_core_ut_test(void)
{
    return 0;
}
#endif
