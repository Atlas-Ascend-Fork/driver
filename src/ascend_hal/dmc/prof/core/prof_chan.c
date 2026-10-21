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
#include <pthread.h>
#include <stdlib.h>

#include "prof_common.h"
#include "prof_chan_list.h"
#include "prof_adapt.h"
#include "prof_chan.h"

enum prof_channel_state {
    CHANNEL_DISABLE,
    CHANNEL_STARTING,
    CHANNEL_ENABLE,
    CHANNEL_FLUSH,
    CHANNEL_STOPPING
};

struct prof_chan_mng {
    pthread_mutex_t state_mutex;
    uint32_t channel_state;

    uint32_t mode;
    uint32_t remote_pid;
    bool event_flag;
    bool support_host_sample;
    struct prof_chan_ops *ops;
    char *priv;
};

static struct prof_chan_mng *g_prof_chan[DEV_NUM] = {NULL};

STATIC drvError_t prof_chan_mng_init(uint32_t dev_id)
{
    struct prof_chan_mng *mng_list = NULL;
    int i;

    mng_list = (struct prof_chan_mng *)calloc(PROF_CHANNEL_NUM_MAX, sizeof(*mng_list));
    if (mng_list == NULL) {
        PROF_ERR("Failed to alloc chan_mng_list. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    for (i = 0; i < PROF_CHANNEL_NUM_MAX; i++) {
        (void)pthread_mutex_init(&mng_list[i].state_mutex, NULL);
        mng_list[i].channel_state = (uint32_t)CHANNEL_DISABLE;
    }

    g_prof_chan[dev_id] = mng_list;
    return DRV_ERROR_NONE;
}

STATIC struct prof_chan_mng *prof_chan_get_mng(uint32_t dev_id, uint32_t chan_id)
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    static int init_flag[DEV_NUM] = {0};
    drvError_t ret;

    if (init_flag[dev_id] != 0) {
        return &g_prof_chan[dev_id][chan_id];
    }

    (void)pthread_mutex_lock(&init_mutex);
    if (init_flag[dev_id] == 0) {
        ret = prof_chan_mng_init(dev_id);
        if (ret != DRV_ERROR_NONE) {
            (void)pthread_mutex_unlock(&init_mutex);
            PROF_ERR("Failed to init chan_mng_list. (dev_id=%u, ret=%d)\n", dev_id, (int)ret);
            return NULL;
        }

        ATOMIC_SET(&init_flag[dev_id], 1);
    }
    (void)pthread_mutex_unlock(&init_mutex);

    return &g_prof_chan[dev_id][chan_id];
}

STATIC drvError_t prof_chan_init(struct prof_chan_mng *chan_mng, uint32_t dev_id, uint32_t chan_id, bool event_flag)
{
    uint32_t chan_mode, remote_pid;
    drvError_t ret;

    ret = prof_get_chan_attr(dev_id, chan_id, &chan_mode, &remote_pid);
    if (ret != DRV_ERROR_NONE) {
        (void)prof_update_chan_list(dev_id);
        ret = prof_get_chan_attr(dev_id, chan_id, &chan_mode, &remote_pid);
        if (ret != DRV_ERROR_NONE) {
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

#ifdef DRV_HOST
    if (prof_support_host_sample(dev_id, chan_id)) {
        PROF_INFO("support host sample. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        chan_mng->support_host_sample = true;
    }
#endif
    /* host sample mode device handle */
    if (event_flag && chan_mng->support_host_sample) {
        if (!prof_support_host_sample(dev_id, chan_id)) {
            PROF_ERR("Device does not support host sample. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
            return DRV_ERROR_INNER_ERR;
        }
    }

    chan_mng->mode = chan_mode;
    chan_mng->remote_pid = remote_pid;
    chan_mng->event_flag = event_flag;

    ret = prof_adapt_get_chan_ops(dev_id, chan_mode, &chan_mng->ops, chan_mng->support_host_sample);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (chan_mng->ops == NULL) {
        PROF_ERR("Ops is NULL. (dev_id=%u, chan_id=%u, support_host_sample=%d)\n", dev_id, chan_id,
                 (int)chan_mng->support_host_sample);
        return DRV_ERROR_INNER_ERR;
    }

    ret = chan_mng->ops->init(dev_id, chan_id, event_flag, &chan_mng->priv);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC void prof_chan_uninit(struct prof_chan_mng *chan_mng)
{
    chan_mng->ops->uninit(&chan_mng->priv);
    chan_mng->ops = NULL;
}

drvError_t prof_chan_start(uint32_t dev_id, uint32_t chan_id, struct prof_user_start_para *para, bool event_flag)
{
    struct prof_chan_mng *chan_mng = NULL;
    drvError_t ret;

    chan_mng = prof_chan_get_mng(dev_id, chan_id);
    if (chan_mng == NULL) {
        PROF_ERR("Failed to get chan_mng. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    (void)pthread_mutex_lock(&chan_mng->state_mutex);
    if (chan_mng->channel_state != (uint32_t)CHANNEL_DISABLE) {
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        PROF_WARN("The channel has been started. (dev_id=%u, chan_id=%u, state=%u)\n", dev_id, chan_id,
                  chan_mng->channel_state);
        return DRV_ERROR_STATUS_FAIL;
    }

    chan_mng->support_host_sample = (bool)para->support_host_sample;
    ret = prof_chan_init(chan_mng, dev_id, chan_id, event_flag);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        return ret;
    }

    para->remote_pid = chan_mng->remote_pid;

    chan_mng->channel_state = (uint32_t)CHANNEL_STARTING;
    (void)pthread_mutex_unlock(&chan_mng->state_mutex);

    ret = chan_mng->ops->start(dev_id, chan_id, para, chan_mng->priv);

    (void)pthread_mutex_lock(&chan_mng->state_mutex);
    if (ret != DRV_ERROR_NONE) {
        prof_chan_uninit(chan_mng);
        chan_mng->channel_state = (uint32_t)CHANNEL_DISABLE;
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        PROF_ERR("Failed to start. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    chan_mng->channel_state = (uint32_t)CHANNEL_ENABLE;
    (void)pthread_mutex_unlock(&chan_mng->state_mutex);

    return DRV_ERROR_NONE;
}

drvError_t prof_chan_stop(uint32_t dev_id, uint32_t chan_id, struct prof_user_stop_para *para, bool event_flag)
{
    struct prof_chan_mng *chan_mng = NULL;
    drvError_t ret;

    chan_mng = prof_chan_get_mng(dev_id, chan_id);
    if (chan_mng == NULL) {
        PROF_ERR("Failed to get chan_mng. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    (void)pthread_mutex_lock(&chan_mng->state_mutex);
    if (((para->host_sample_release_flag == PROF_STOP_STAGE_DEFAULT) ||
         (para->host_sample_release_flag == PROF_STOP_STAGE_PAUSE) ||
         (para->host_sample_release_flag == PROF_STOP_STAGE_PAUSE_AND_RELEASE)) &&
        (chan_mng->channel_state != (uint32_t)CHANNEL_ENABLE)) {
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        PROF_WARN("The channel is disabled or busy. (dev_id=%u, chan_id=%u, state=%u)\n", dev_id, chan_id,
                  chan_mng->channel_state);
        return DRV_ERROR_STATUS_FAIL;
    }

    if ((para->host_sample_release_flag == PROF_STOP_STAGE_RELEASE) &&
        (chan_mng->channel_state != (uint32_t)CHANNEL_STOPPING)) {
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        PROF_WARN("The channel is disabled or busy. (dev_id=%u, chan_id=%u, state=%u)\n", dev_id, chan_id,
                  chan_mng->channel_state);
        return DRV_ERROR_STATUS_FAIL;
    }

    if (chan_mng->event_flag != event_flag) {
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        return DRV_ERROR_NOT_SUPPORT;
    }

    para->remote_pid = chan_mng->remote_pid;

    chan_mng->channel_state = (uint32_t)CHANNEL_STOPPING;
    (void)pthread_mutex_unlock(&chan_mng->state_mutex);

    ret = chan_mng->ops->stop(dev_id, chan_id, para, chan_mng->priv);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Stop the channel unsuccessfully. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
    }
    para->h2d_ret = ret;

    if (!para->support_host_sample || (para->host_sample_release_flag == PROF_STOP_STAGE_RELEASE) ||
        (para->host_sample_release_flag == PROF_STOP_STAGE_PAUSE_AND_RELEASE)) {
        (void)pthread_mutex_lock(&chan_mng->state_mutex);
        prof_chan_uninit(chan_mng);
        chan_mng->channel_state = (uint32_t)CHANNEL_DISABLE;
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_chan_flush_check(uint32_t dev_id, uint32_t chan_id, uint32_t chan_state)
{
    if (chan_state == (uint32_t)CHANNEL_DISABLE) {
        PROF_ERR("The channel is disabled. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return (drvError_t)PROF_STOPPED_ALREADY;
    }

    if (chan_state != (uint32_t)CHANNEL_ENABLE) {
        PROF_WARN("The channel is busy. (dev_id=%u, chan_id=%u, state=%u)\n", dev_id, chan_id, chan_state);
        return DRV_ERROR_STATUS_FAIL;
    }

    return DRV_ERROR_NONE;
}

drvError_t prof_chan_flush(uint32_t dev_id, uint32_t chan_id, uint32_t *data_len)
{
    struct prof_chan_mng *chan_mng = NULL;
    drvError_t ret;

    chan_mng = prof_chan_get_mng(dev_id, chan_id);
    if (chan_mng == NULL) {
        PROF_ERR("Failed to get chan_mng. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    (void)pthread_mutex_lock(&chan_mng->state_mutex);
    ret = prof_chan_flush_check(dev_id, chan_id, chan_mng->channel_state);
    if (ret != PROF_OK) {
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        return ret;
    }

    chan_mng->channel_state = (uint32_t)CHANNEL_FLUSH;
    (void)pthread_mutex_unlock(&chan_mng->state_mutex);

    ret = chan_mng->ops->flush(dev_id, chan_id, data_len, chan_mng->priv);

    (void)pthread_mutex_lock(&chan_mng->state_mutex);
    chan_mng->channel_state = (uint32_t)CHANNEL_ENABLE;
    (void)pthread_mutex_unlock(&chan_mng->state_mutex);

    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            PROF_ERR("Failed to flush. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        }
        return ret;
    }

    return DRV_ERROR_NONE;
}

int prof_chan_read(uint32_t dev_id, uint32_t chan_id, char *out_buf, uint32_t buf_size)
{
    prof_user_read_para_t read_para = {0};
    struct prof_chan_mng *chan_mng = NULL;
    int ret;

    chan_mng = prof_chan_get_mng(dev_id, chan_id);
    if (chan_mng == NULL) {
        PROF_ERR("Failed to get chan_mng. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return PROF_ERROR;
    }

    (void)pthread_mutex_lock(&chan_mng->state_mutex);
    if (chan_mng->channel_state == (uint32_t)CHANNEL_DISABLE) {
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        PROF_WARN("The channel is disabled. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return PROF_STOPPED_ALREADY;
    }

    read_para.out_buf = out_buf;
    read_para.buf_size = buf_size;
    read_para.write_rptr_flag = !((chan_mng->channel_state == (uint32_t)CHANNEL_STOPPING) ||
                                  (chan_mng->channel_state == (uint32_t)CHANNEL_STARTING));
    ret = chan_mng->ops->read(dev_id, chan_id, &read_para, chan_mng->priv);
    (void)pthread_mutex_unlock(&chan_mng->state_mutex);

    return ret;
}

drvError_t prof_chan_query(uint32_t dev_id, uint32_t chan_id, uint32_t *avail_len)
{
    struct prof_chan_mng *chan_mng = NULL;
    drvError_t ret;

    chan_mng = prof_chan_get_mng(dev_id, chan_id);
    if (chan_mng == NULL) {
        PROF_ERR("Failed to get chan_mng. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    (void)pthread_mutex_lock(&chan_mng->state_mutex);
    if (chan_mng->channel_state == (uint32_t)CHANNEL_DISABLE) {
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        PROF_WARN("The channel is disabled. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_STATUS_FAIL;
    }

    ret = chan_mng->ops->query(dev_id, chan_id, avail_len, chan_mng->priv);
    (void)pthread_mutex_unlock(&chan_mng->state_mutex);

    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            PROF_ERR("Failed to query. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        }
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t prof_chan_report(uint32_t dev_id, uint32_t chan_id, void *data, uint32_t data_len, bool hal_flag)
{
    struct prof_chan_mng *chan_mng = NULL;
    drvError_t ret;

    chan_mng = prof_chan_get_mng(dev_id, chan_id);
    if (chan_mng == NULL) {
        PROF_ERR("Failed to get chan_mng. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    (void)pthread_mutex_lock(&chan_mng->state_mutex);
    if (chan_mng->channel_state == (uint32_t)CHANNEL_DISABLE) {
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        PROF_WARN("The channel is disabled. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_STATUS_FAIL;
    }

    if (hal_flag && (chan_mng->mode != PROF_CHAN_MODE_USER)) {
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (chan_mng->support_host_sample && hal_flag) {
        (void)pthread_mutex_unlock(&chan_mng->state_mutex);
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = chan_mng->ops->report(dev_id, chan_id, data, data_len, chan_mng->priv);
    (void)pthread_mutex_unlock(&chan_mng->state_mutex);

    return ret;
}

#else
int prof_chan_ut_test(void)
{
    return 0;
}
#endif
