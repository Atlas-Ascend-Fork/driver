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
#include "prof_adapt.h"
#include "prof_chan_list.h"

struct prof_chan_attr {
    bool online;
    uint32_t mode;
    uint32_t remote_pid;
};

struct prof_chan_list_mng {
    pthread_mutex_t mutex;
    struct prof_chan_attr chan_attr[PROF_CHANNEL_NUM_MAX];
    struct prof_chan_attr chan_attr_ex[PROF_CHANNEL_NUM_MAX];
};

static struct prof_chan_list_mng *g_prof_chan_list[DEV_NUM] = {NULL};

STATIC drvError_t prof_chan_list_mng_init(uint32_t dev_id)
{
    struct prof_chan_list_mng *list_mng = NULL;
    int i;

    list_mng = (struct prof_chan_list_mng *)calloc(1, sizeof(*list_mng));
    if (list_mng == NULL) {
        PROF_ERR("Failed to alloc chan_list_mng. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    (void)pthread_mutex_init(&list_mng->mutex, NULL);
    for (i = 0; i < PROF_CHANNEL_NUM_MAX; i++) {
        list_mng->chan_attr[i].online = false;
        list_mng->chan_attr_ex[i].online = false;
    }

    g_prof_chan_list[dev_id] = list_mng;
    return DRV_ERROR_NONE;
}

STATIC struct prof_chan_list_mng *prof_chan_list_get_mng(uint32_t dev_id)
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    static int init_flag[DEV_NUM] = {0};
    drvError_t ret;

    if (init_flag[dev_id] != 0) {
        return g_prof_chan_list[dev_id];
    }

    (void)pthread_mutex_lock(&init_mutex);
    if (init_flag[dev_id] == 0) {
        ret = prof_chan_list_mng_init(dev_id);
        if (ret != DRV_ERROR_NONE) {
            (void)pthread_mutex_unlock(&init_mutex);
            PROF_ERR("Failed to init chan_list_mng. (dev_id=%u, ret=%d)\n", dev_id, (int)ret);
            return NULL;
        }

        ATOMIC_SET(&init_flag[dev_id], 1);
    }
    (void)pthread_mutex_unlock(&init_mutex);

    return g_prof_chan_list[dev_id];
}

STATIC void prof_chan_list_add(struct prof_chan_attr *chan_attr, uint32_t mode, uint32_t remote_pid)
{
    chan_attr->mode = mode;
    chan_attr->remote_pid = remote_pid;
    chan_attr->online = true;
}

STATIC void prof_chan_list_del(struct prof_chan_attr *chan_attr)
{
    chan_attr->online = false;
}

STATIC drvError_t prof_chan_list_get(struct prof_chan_attr *chan_attr, uint32_t *mode, uint32_t *remote_pid)
{
    if (chan_attr->online) {
        *mode = chan_attr->mode;
        *remote_pid = chan_attr->remote_pid;
        return DRV_ERROR_NONE;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }
}

drvError_t prof_add_local_channel(uint32_t dev_id, uint32_t chan_id, bool support_host_sample)
{
    struct prof_chan_list_mng *list_mng = prof_chan_list_get_mng(dev_id);
    struct prof_chan_attr *chan_attr;

    if (list_mng == NULL) {
        PROF_ERR("Failed to get chan_list_mng. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    (void)pthread_mutex_lock(&list_mng->mutex);
    if (support_host_sample) {
        chan_attr = &list_mng->chan_attr_ex[chan_id];
    } else {
        chan_attr = &list_mng->chan_attr[chan_id];
    }
    if (chan_attr->online) {
        (void)pthread_mutex_unlock(&list_mng->mutex);
        PROF_ERR("Repeatedly add local channel. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_REPEATED_INIT;
    }

    prof_chan_list_add(chan_attr, (uint32_t)PROF_CHAN_MODE_USER, 0);
    (void)pthread_mutex_unlock(&list_mng->mutex);

    return DRV_ERROR_NONE;
}

void prof_del_local_channel(uint32_t dev_id, uint32_t chan_id, bool support_host_sample)
{
    struct prof_chan_list_mng *list_mng = prof_chan_list_get_mng(dev_id);
    struct prof_chan_attr *chan_attr;

    /* Should be called after prof_add_local_channel, so there is no need to check if list_mng is NULL */

    (void)pthread_mutex_lock(&list_mng->mutex);
    if (support_host_sample) {
        chan_attr = &list_mng->chan_attr_ex[chan_id];
    } else {
        chan_attr = &list_mng->chan_attr[chan_id];
    }
    prof_chan_list_del(chan_attr);
    (void)pthread_mutex_unlock(&list_mng->mutex);
}

STATIC void prof_clear_nonlocal_channels(struct prof_chan_list_mng *list_mng)
{
    struct prof_chan_attr *chan_attr = NULL;
    int i;

    for (i = 0; i < PROF_CHANNEL_NUM_MAX; i++) {
        chan_attr = &list_mng->chan_attr[i];
        if (chan_attr->online == false) {
            continue;
        }

        if (chan_attr->mode == PROF_CHAN_MODE_USER) {
            continue;
        }

        prof_chan_list_del(chan_attr);
    }
}

STATIC void prof_add_nonlocal_channels(struct prof_chan_list_mng *list_mng, struct prof_channel_list *channels)
{
    uint32_t chan_id, remote_pid, i;

    for (i = 0; i < channels->channel_num; i++) {
        if (channels->channel[i].channel_id >= PROF_CHANNEL_NUM_MAX) {
            PROF_WARN("Invalid channel_id. (channel_id=%hu, max=%u)\n", channels->channel[i].channel_id,
                      PROF_CHANNEL_NUM_MAX);
            continue;
        }
        chan_id = channels->channel[i].channel_id;
        remote_pid = channels->channel[i].remote_pid;

        prof_chan_list_add(&list_mng->chan_attr[chan_id], (uint32_t)PROF_CHAN_MODE_KERNEL, remote_pid);
    }
}

drvError_t prof_update_chan_list(uint32_t dev_id)
{
    struct prof_chan_list_mng *list_mng = prof_chan_list_get_mng(dev_id);
    struct prof_channel_list *channels = NULL;
    drvError_t ret;

    if (list_mng == NULL) {
        PROF_ERR("Failed to get chan_list_mng. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    channels = (struct prof_channel_list *)malloc(sizeof(*channels));
    if (channels == NULL) {
        PROF_ERR("Failed to alloc channels. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    ret = prof_adapt_get_channels(dev_id, channels);
    if (ret != DRV_ERROR_NONE) {
        free(channels);
        PROF_ERR("Failed to get channels from adapt. (dev_id=%u, ret=%d)\n", dev_id, (int)ret);
        return ret;
    }

    (void)pthread_mutex_lock(&list_mng->mutex);
    /* step 1: clear non-local channels */
    prof_clear_nonlocal_channels(list_mng);

    /* step 2: add non-local channels */
    prof_add_nonlocal_channels(list_mng, channels);
    (void)pthread_mutex_unlock(&list_mng->mutex);

    free(channels);

    return DRV_ERROR_NONE;
}

void prof_get_chan_list(uint32_t dev_id, struct channel_list *channels)
{
    struct prof_chan_list_mng *list_mng = prof_chan_list_get_mng(dev_id);
    int i;

    /* Should be called after prof_update_chan_list, so there is no need to check if list_mng is NULL */

    channels->channel_num = 0;
    (void)pthread_mutex_lock(&list_mng->mutex);
    for (i = 0; i < PROF_CHANNEL_NUM_MAX; i++) {
        if (list_mng->chan_attr[i].online) {
            channels->channel[channels->channel_num].channel_id = (uint32_t)i;
            channels->channel_num++;
        }
    }
    (void)pthread_mutex_unlock(&list_mng->mutex);
}

drvError_t prof_get_chan_attr(uint32_t dev_id, uint32_t chan_id, uint32_t *mode, uint32_t *remote_pid)
{
    struct prof_chan_list_mng *list_mng = prof_chan_list_get_mng(dev_id);
    drvError_t ret;

    if (list_mng == NULL) {
        PROF_ERR("Failed to get chan_list_mng. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    (void)pthread_mutex_lock(&list_mng->mutex);
    ret = prof_chan_list_get(&list_mng->chan_attr[chan_id], mode, remote_pid);
    (void)pthread_mutex_unlock(&list_mng->mutex);

    return ret;
}

bool prof_support_host_sample(uint32_t dev_id, uint32_t chan_id)
{
    struct prof_chan_list_mng *list_mng = prof_chan_list_get_mng(dev_id);

    if (list_mng == NULL) {
        PROF_ERR("Failed to get chan_list_mng. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return false;
    }

    if (list_mng->chan_attr_ex[chan_id].online) {
        return true;
    }
    return false;
}

#else
int prof_chan_list_ut_test(void)
{
    return 0;
}
#endif
