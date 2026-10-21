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
#include <stdlib.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include "drv_user_common.h"

#include "prof_adapt.h"
#include "prof_buff.h"
#include "prof_adapt_user.h"
#include "prof_event_master.h"

STATIC drvError_t prof_host_sample_start_para_check(uint32_t dev_id, uint32_t chan_id,
                                                    struct prof_user_start_para *para,
                                                    struct prof_user_chan_priv *chan_priv)
{
    if ((chan_id == CHANNEL_STARS_SOC_LOG_BUFFER) || (chan_id == CHANNEL_FFTS_PROFILE_BUFFER_TASK)) {
        para->sample_period = PROF_HOST_SAMPLE_PERIOD_DEFAULT;
    }

    if (para->sample_period == 0) {
        PROF_ERR("Host sample mode only supports period sample. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((para->sample_period < PROF_HOST_SAMPLE_PERIOD_MIN) || (para->sample_period > PROF_HOST_SAMPLE_PERIOD_MAX)) {
        PROF_ERR("Invalid sample period. (dev_id=%u, chan_id=%u, sample_period=%ums, min=%ums, max=%ums)\n", dev_id,
                 chan_id, para->sample_period, PROF_HOST_SAMPLE_PERIOD_MIN, PROF_HOST_SAMPLE_PERIOD_MAX);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (chan_priv->ops.sample_func == NULL) {
        PROF_ERR("No sample func. (dev_id=%u, chan_id=%u, sample_period=%ums)\n", dev_id, chan_id, para->sample_period);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_user_chan_host_sample_start(uint32_t dev_id, uint32_t chan_id, struct prof_user_start_para *para,
                                                   char *priv)
{
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)priv;
    drvError_t ret;
    struct prof_start_event_out_msg start_out_msg = {0, 0, {0}};
    struct prof_user_stop_para stop_para = {0};
    stop_para.remote_pid = para->remote_pid;
    char addr_data[PROF_START_OUTDATA_SIZE_MAX] = {0};

    ret = prof_host_sample_start_para_check(dev_id, chan_id, para, chan_priv);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    para->addrdata = addr_data;
    ret = prof_user_sample_pre_start(chan_priv, para);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to pre start user sample. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    ret = prof_user_host_sample_event_start(dev_id, chan_id, para, &start_out_msg);
    if (ret != DRV_ERROR_NONE) {
        prof_user_sample_stop(chan_priv, PROF_STOP_STAGE_DEFAULT); // revert prof_user_sample_pre_start
        PROF_ERR("Failed to send start event to device. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    ret = prof_user_sample_start(chan_priv, para, &start_out_msg, true);
    if (ret != DRV_ERROR_NONE) {
        (void)prof_user_host_sample_event_stop(dev_id, chan_id, &stop_para, PROF_STOP_STAGE_PAUSE_AND_RELEASE);
        prof_user_sample_stop(chan_priv, PROF_STOP_STAGE_DEFAULT); // revert prof_user_sample_pre_start
        PROF_ERR("Failed to start user sample. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    ret = prof_user_sample_thread_enable(chan_priv);
    if (ret != DRV_ERROR_NONE) {
        (void)prof_user_host_sample_event_stop(dev_id, chan_id, &stop_para, PROF_STOP_STAGE_PAUSE_AND_RELEASE);
        prof_user_sample_stop(chan_priv, PROF_STOP_STAGE_DEFAULT);
        PROF_ERR("Failed to enable thread. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    ret = prof_user_sample_timer_init(chan_priv, para->sample_period);
    if (ret != DRV_ERROR_NONE) {
        prof_user_sample_thread_disable(chan_priv);
        (void)prof_user_host_sample_event_stop(
            dev_id, chan_id, &stop_para, PROF_STOP_STAGE_PAUSE_AND_RELEASE); /* first device stop then host stop */
        prof_user_sample_stop(chan_priv, PROF_STOP_STAGE_DEFAULT);
        PROF_ERR("Failed to init timer. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    PROF_INFO("Host sample start success. (dev_id=%u, chan_id=%u, sub_chan_id=%u, outdata_len=%u)\n", dev_id, chan_id,
              start_out_msg.sub_chan_id, start_out_msg.outdata_len);
    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_user_chan_host_sample_init(uint32_t dev_id, uint32_t chan_id, bool event_flag, char **priv)
{
    struct prof_user_chan_priv *chan_priv = NULL;
    drvError_t ret;

    chan_priv = (struct prof_user_chan_priv *)malloc(sizeof(*chan_priv));
    if (chan_priv == NULL) {
        PROF_ERR("Failed to alloc chan_priv. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    ret = prof_user_chan_get(dev_id, chan_id, chan_priv, &chan_priv->sem, true);
    if (ret != DRV_ERROR_NONE) {
        free(chan_priv);
        return DRV_ERROR_NOT_SUPPORT;
    }

    chan_priv->dev_id = dev_id;
    chan_priv->chan_id = chan_id;

    chan_priv->sample_mode = event_flag ? PROF_SAMPLE_REMOTER_MODE : PROF_SAMPLE_LOCAL_MODE;
    chan_priv->sample_thread_flag = 0;
    chan_priv->timer_fd = NULL;
    chan_priv->sample_status = PROF_SAMPLE_NORMAL;
    (void)sem_init(chan_priv->sem, 0, 0);

    ret = prof_buff_init(chan_id, &chan_priv->buff);
    if (ret != DRV_ERROR_NONE) {
        free(chan_priv);
        PROF_ERR("Failed to init buff. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return ret;
    }

    *priv = (char *)chan_priv;
    PROF_INFO("Host sample init success. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_user_chan_proactive_sample(uint32_t dev_id, uint32_t chan_id, char *priv)
{
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)priv;
    uint32_t wait_num = 0;
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = 1000000; /* 1000000: sleep 1ms */

    if (CAS(&chan_priv->sample_status, PROF_SAMPLE_NORMAL, PROF_SAMPLE_FLUSH_START)) {
        (void)sem_post(chan_priv->sem);
        while (!CAS(&chan_priv->sample_status, PROF_SAMPLE_FLUSH_FINISH, PROF_SAMPLE_NORMAL) &&
               (wait_num < 1000)) { /* 1000 */
            nanosleep(&ts, NULL);
            wait_num++;
        }
        if (wait_num >= 1000) { /* 1000 */
            PROF_ERR("wait sample timeout. (dev_id=%u, chan_id=%u, sample_flag=%u)\n", dev_id, chan_id,
                     chan_priv->sample_status);
            return DRV_ERROR_WAIT_TIMEOUT;
        }
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_user_chan_host_sample_stop(uint32_t dev_id, uint32_t chan_id, struct prof_user_stop_para *para,
                                                  char *priv)
{
    int ret, result = 0;
    (void)para;
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)priv;

    ret = prof_user_host_sample_event_stop(dev_id, chan_id, para, PROF_STOP_STAGE_PAUSE);
    if (ret != 0) {
        PROF_ERR("Failed to send stop msg. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, ret);
        result |= ret;
    }
    prof_user_sample_timer_uninit(chan_priv);
    ret = prof_user_chan_proactive_sample(dev_id, chan_id, priv);
    if (ret != 0) {
        PROF_ERR("Failed to sample. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, ret);
        result |= ret;
    }
    prof_user_sample_thread_disable(chan_priv);
    ret = prof_user_host_sample_event_stop(dev_id, chan_id, para, PROF_STOP_STAGE_RELEASE);
    if (ret != 0) {
        PROF_ERR("Failed to send stop msg. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, ret);
        result |= ret;
    }
    prof_user_sample_stop(chan_priv, PROF_STOP_STAGE_DEFAULT);

    prof_buff_wait_read_empty(chan_priv->buff, dev_id, chan_id);
    return (result == 0) ? (DRV_ERROR_NONE) : (DRV_ERROR_INNER_ERR);
}

STATIC drvError_t prof_user_chan_host_sample_flush(uint32_t dev_id, uint32_t chan_id, uint32_t *data_len, char *priv)
{
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)priv;
    int ret;

    ret = prof_user_chan_proactive_sample(dev_id, chan_id, priv);
    if (ret != 0) {
        return ret;
    }

    if (chan_priv->sample_mode == PROF_SAMPLE_LOCAL_MODE) {
        *data_len = prof_buff_get_data_len(chan_priv->buff);
        return DRV_ERROR_NONE;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }
}

STATIC drvError_t prof_user_chan_host_sample_query(uint32_t dev_id, uint32_t chan_id, uint32_t *avail_len, char *priv)
{
    (void)dev_id;
    (void)chan_id;
    (void)avail_len;
    (void)priv;
    return DRV_ERROR_NOT_SUPPORT;
}

static struct prof_chan_ops prof_user_host_sample_chan_ops = {
    .init = prof_user_chan_host_sample_init,
    .uninit = prof_user_chan_uninit,
    .start = prof_user_chan_host_sample_start,
    .stop = prof_user_chan_host_sample_stop,
    .flush = prof_user_chan_host_sample_flush,
    .read = prof_user_chan_read,
    .query = prof_user_chan_host_sample_query,
    .report = prof_user_chan_report,
};

drvError_t prof_user_get_host_sample_chan_ops(struct prof_chan_ops **ops)
{
    *ops = &prof_user_host_sample_chan_ops;
    return DRV_ERROR_NONE;
}

#else
int prof_adapt_host_sample_ut_test(void)
{
    return 0;
}
#endif
