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
#include <sys/prctl.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include "drv_user_common.h"

#include "prof_adapt.h"
#include "prof_buff.h"
#include "prof_adapt_user.h"

static struct prof_user_kernel_ops g_user_kernel_ops = {NULL};

void prof_user_regiser_kernel_ops(struct prof_user_kernel_ops *ops)
{
    g_user_kernel_ops.chan_register = ops->chan_register;
    g_user_kernel_ops.chan_query = ops->chan_query;
    g_user_kernel_ops.chan_writer = ops->chan_writer;
}

STATIC struct prof_user_kernel_ops *prof_user_get_kernel_ops(void)
{
    return &g_user_kernel_ops;
}

struct prof_user_chan_node {
    struct list_head list_node;
    uint32_t dev_id;
    uint32_t chan_id;
    uint32_t sample_buff_size;
    struct prof_sample_ops ops;
    struct prof_sample_ops ops_ex;
    /* The timer callback is processed asynchronously,
     * so it is necessary to ensure that the semaphore memory is always available */
    sem_t sem;
};
static LIST_HEAD(g_prof_user_chan_list);
static pthread_mutex_t g_chan_list_mutex = PTHREAD_MUTEX_INITIALIZER;

STATIC drvError_t prof_user_chan_add(uint32_t dev_id, uint32_t chan_id, struct prof_sample_register_para *para,
                                     bool support_host_sample)
{
    struct prof_user_chan_node *node = NULL;
    struct list_head *pos = NULL, *n = NULL;
    struct prof_sample_ops *ops;

    (void)pthread_mutex_lock(&g_chan_list_mutex);
    list_for_each_safe(pos, n, &g_prof_user_chan_list)
    {
        node = list_entry(pos, struct prof_user_chan_node, list_node);
        if ((node->dev_id == dev_id) && (node->chan_id == chan_id)) {
            if (support_host_sample) {
                ops = &node->ops_ex;
                node->sample_buff_size = 512 * 1024; /* 512 1024: 512K*/
            } else {
                ops = &node->ops;
            }
            ops->start_func = para->ops.start_func;
            ops->sample_func = para->ops.sample_func;
            ops->flush_func = para->ops.flush_func;
            ops->stop_func = para->ops.stop_func;
            (void)pthread_mutex_unlock(&g_chan_list_mutex);
            return DRV_ERROR_NONE;
        }
    }

    node = (struct prof_user_chan_node *)malloc(sizeof(*node));
    if (node == NULL) {
        (void)pthread_mutex_unlock(&g_chan_list_mutex);
        PROF_ERR("Failed to alloc chan_node. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_MALLOC_FAIL;
    }
    node->dev_id = dev_id;
    node->chan_id = chan_id;
    node->sample_buff_size = 0;
    if (support_host_sample) {
        ops = &node->ops_ex;
        node->sample_buff_size = 512 * 1024; /* 512 1024: 512K*/
    } else {
        ops = &node->ops;
    }
    ops->start_func = para->ops.start_func;
    ops->sample_func = para->ops.sample_func;
    ops->flush_func = para->ops.flush_func;
    ops->stop_func = para->ops.stop_func;
    drv_user_list_add_tail(&node->list_node, &g_prof_user_chan_list);

    (void)pthread_mutex_unlock(&g_chan_list_mutex);

    return DRV_ERROR_NONE;
}

STATIC void prof_user_chan_del(uint32_t dev_id, uint32_t chan_id)
{
    struct prof_user_chan_node *node = NULL, *chan_node = NULL;
    struct list_head *pos = NULL, *n = NULL;

    (void)pthread_mutex_lock(&g_chan_list_mutex);
    list_for_each_safe(pos, n, &g_prof_user_chan_list)
    {
        node = list_entry(pos, struct prof_user_chan_node, list_node);
        if ((node->dev_id == dev_id) && (node->chan_id == chan_id)) {
            drv_user_list_del(&node->list_node);
            chan_node = node;
            break;
        }
    }
    (void)pthread_mutex_unlock(&g_chan_list_mutex);

    if (chan_node != NULL) {
        free(chan_node);
    }
}

drvError_t prof_user_chan_get(uint32_t dev_id, uint32_t chan_id, struct prof_user_chan_priv *chan_priv, sem_t **sem,
                              bool support_host_sample)
{
    struct prof_user_chan_node *node = NULL;
    struct list_head *pos = NULL, *n = NULL;

    (void)pthread_mutex_lock(&g_chan_list_mutex);
    list_for_each_safe(pos, n, &g_prof_user_chan_list)
    {
        node = list_entry(pos, struct prof_user_chan_node, list_node);
        if ((node->dev_id == dev_id) && (node->chan_id == chan_id)) {
            if (support_host_sample) {
                chan_priv->ops.start_func = node->ops_ex.start_func;
                chan_priv->ops.sample_func = node->ops_ex.sample_func;
                chan_priv->ops.flush_func = node->ops_ex.flush_func;
                chan_priv->ops.stop_func = node->ops_ex.stop_func;
                chan_priv->sample_buff_size = node->sample_buff_size;
            } else {
                chan_priv->ops.start_func = node->ops.start_func;
                chan_priv->ops.sample_func = node->ops.sample_func;
                chan_priv->ops.flush_func = node->ops.flush_func;
                chan_priv->ops.stop_func = node->ops.stop_func;
                chan_priv->sample_buff_size = 0;
            }

            *sem = &node->sem;
            chan_priv->support_host_sample = support_host_sample;

            (void)pthread_mutex_unlock(&g_chan_list_mutex);
            return DRV_ERROR_NONE;
        }
    }
    (void)pthread_mutex_unlock(&g_chan_list_mutex);

    return DRV_ERROR_NOT_EXIST;
}

STATIC bool prof_user_is_need_kernel_reg(uint32_t chan_id)
{
    uint32_t support_chan_list[] = {CHANNEL_AICPU, CHANNEL_CUS_AICPU, CHANNEL_ADPROF};
    uint32_t i;

    for (i = 0; i < sizeof(support_chan_list) / sizeof(uint32_t); i++) {
        if (support_chan_list[i] == chan_id) {
            return true;
        }
    }

    return false;
}

drvError_t prof_user_register_channel(uint32_t dev_id, uint32_t chan_id, struct prof_sample_register_para *para,
                                      bool support_host_sample)
{
    struct prof_user_kernel_ops *kernel_ops = NULL;
    drvError_t ret;

    ret = prof_user_chan_add(dev_id, chan_id, para, support_host_sample);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to register channel to user. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    if (support_host_sample) {
        return DRV_ERROR_NONE;
    }

    if (prof_user_is_need_kernel_reg(chan_id) == false) {
        return DRV_ERROR_NONE;
    }

    kernel_ops = prof_user_get_kernel_ops();
    if (kernel_ops->chan_register != NULL) {
        ret = kernel_ops->chan_register(dev_id, chan_id);
        if (ret != DRV_ERROR_NONE) {
            prof_user_chan_del(dev_id, chan_id);
            PROF_ERR("Failed to register channel to kernel. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id,
                     (int)ret);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_user_chan_init(uint32_t dev_id, uint32_t chan_id, bool event_flag, char **priv)
{
    struct prof_user_chan_priv *chan_priv = NULL;
    drvError_t ret;

    chan_priv = (struct prof_user_chan_priv *)malloc(sizeof(*chan_priv));
    if (chan_priv == NULL) {
        PROF_ERR("Failed to alloc chan_priv. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    ret = prof_user_chan_get(dev_id, chan_id, chan_priv, &chan_priv->sem, false);
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

    /* Only local mode requires local buff */
    if (chan_priv->sample_mode == PROF_SAMPLE_LOCAL_MODE) {
        ret = prof_buff_init(chan_id, &chan_priv->buff);
        if (ret != DRV_ERROR_NONE) {
            free(chan_priv);
            PROF_ERR("Failed to init buff. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
            return ret;
        }
    } else {
        chan_priv->buff = NULL;
    }

    *priv = (char *)chan_priv;
    return DRV_ERROR_NONE;
}

void prof_user_chan_uninit(char **priv)
{
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)(*priv);

    if (chan_priv->sample_mode == PROF_SAMPLE_LOCAL_MODE) {
        prof_buff_uninit(&chan_priv->buff);
    }

    free(chan_priv);
    *priv = NULL;
}

drvError_t prof_user_sample_start_check(struct prof_user_chan_priv *chan_priv, struct prof_user_start_para *para)
{
    if (para->sample_period == 0) {
        return DRV_ERROR_NONE;
    }

    if ((para->sample_period < PROF_PERIOD_MIN) || (para->sample_period > PROF_PERIOD_MAX)) {
        PROF_ERR("Invalid sample period. (dev_id=%u, chan_id=%u, sample_period=%ums, min=%ums, max=%ums)\n",
                 chan_priv->dev_id, chan_priv->chan_id, para->sample_period, PROF_PERIOD_MIN, PROF_PERIOD_MAX);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (chan_priv->ops.sample_func == NULL) {
        PROF_ERR("No sample_func. (dev_id=%u, chan_id=%u, sample_period=%ums)\n", chan_priv->dev_id, chan_priv->chan_id,
                 para->sample_period);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t prof_user_sample_pre_start(struct prof_user_chan_priv *chan_priv, struct prof_user_start_para *para)
{
    struct prof_sample_start_para start_para = {0};
    int ret;

    if (chan_priv->ops.start_func != NULL) {
        start_para.dev_id = chan_priv->dev_id;
        start_para.user_data = para->user_data;
        start_para.user_data_len = para->user_data_size;
        start_para.out_data = (void *)para->addrdata; // pre_start reuse out_data as in_data.
        start_para.out_data_max_len = PROF_START_OUTDATA_SIZE_MAX;
        start_para.is_support_host_move = true;
        start_para.phease = PROF_HOST_SAMPLE_PHEASE1;

        ret = chan_priv->ops.start_func(&start_para);
        if (ret != PROF_OK) {
            PROF_ERR("Failed to invoke pre start func. (dev_id=%u, chan_id=%u, ret=%d)\n", chan_priv->dev_id,
                     chan_priv->chan_id, ret);
            return DRV_ERROR_INVALID_HANDLE;
        }
        if (start_para.out_data_len > PROF_START_OUTDATA_SIZE_MAX) {
            PROF_ERR("out_data_len exceeds the limit. (dev_id=%u, chan_id=%u, out_data_len=%u, limit=%u)\n",
                     chan_priv->dev_id, chan_priv->chan_id, start_para.out_data_len, PROF_START_OUTDATA_SIZE_MAX);
            return DRV_ERROR_INVALID_VALUE;
        }
        para->addr_data_len = start_para.out_data_len;
    }

    return DRV_ERROR_NONE;
}

drvError_t prof_user_sample_start(struct prof_user_chan_priv *chan_priv, struct prof_user_start_para *para,
                                  struct prof_start_event_out_msg *outdata, bool support_host_sample)
{
    struct prof_sample_start_para start_para = {0};
    int ret;

    if (chan_priv->ops.start_func != NULL) {
        start_para.dev_id = chan_priv->dev_id;
        start_para.user_data = para->user_data;
        start_para.user_data_len = para->user_data_size;
        if (support_host_sample) {
            start_para.sub_chan_id = outdata->sub_chan_id;
            start_para.out_data_len = outdata->outdata_len;
            start_para.out_data = outdata->outdata;
            start_para.out_data_max_len = PROF_START_OUTDATA_SIZE_MAX;
            start_para.is_support_host_move = support_host_sample;
            start_para.phease = PROF_HOST_SAMPLE_PHEASE2;
        }
        ret = chan_priv->ops.start_func(&start_para);
        if (ret != PROF_OK) {
            PROF_ERR("Failed to invoke start func. (dev_id=%u, chan_id=%u, ret=%d)\n", chan_priv->dev_id,
                     chan_priv->chan_id, ret);
            return DRV_ERROR_INVALID_HANDLE;
        }
    }

    return DRV_ERROR_NONE;
}

void prof_user_sample_stop(struct prof_user_chan_priv *chan_priv, uint32_t release_flag)
{
    struct prof_sample_stop_para stop_para = {0};
    int ret;

    if (chan_priv->ops.stop_func != NULL) {
        stop_para.dev_id = chan_priv->dev_id;
        stop_para.release_flag = release_flag;
        ret = chan_priv->ops.stop_func(&stop_para);
        if (ret != PROF_OK) {
            PROF_ERR("Failed to invoke stop func. (dev_id=%u, chan_id=%u, ret=%d)\n", chan_priv->dev_id,
                     chan_priv->chan_id, ret);
        }
    }
}

STATIC void *prof_user_sample_thread(void *arg)
{
    (void)prctl(PR_SET_NAME, "prof_sample");
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)arg;
    struct prof_adapt_core_notifier *notifier = prof_adapt_get_notifier();
    struct prof_sample_para para = {0};
    uint32_t dev_id = chan_priv->dev_id;
    uint32_t chan_id = chan_priv->chan_id;
    uint32_t buff_len = 100 * 1024; /* 100 * 1024 Byte */
    uint8_t *buff;
    int ret;

    PROF_INFO("Sample thread start. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
    if (chan_priv->support_host_sample) {
        buff_len = chan_priv->sample_buff_size;
        if (buff_len == 0) {
            PROF_ERR("Invalid buff len. (dev_id=%u, chan_id=%u, buff_len=%u, min=%u)\n", dev_id, chan_id, buff_len, 1U);
            return NULL;
        }
    }

    buff = (uint8_t *)malloc(buff_len);
    if (buff == NULL) {
        PROF_ERR("Failed to malloc buff mem. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return NULL;
    }

    para.sample_flag = SAMPLE_DATA_WITH_HEADER;
    while (chan_priv->sample_thread_flag) {
        (void)sem_wait(chan_priv->sem);

        (void)CAS(&chan_priv->sample_status, PROF_SAMPLE_FLUSH_START, PROF_SAMPLE_FLUSH_STARTING);
        para.dev_id = dev_id;
        para.target_pid = getpid();
        para.buff = buff;
        para.buff_len = buff_len;
        para.report_len = 0;
        ret = chan_priv->ops.sample_func(&para);
        if (ret == DRV_ERROR_CALL_NO_RETRY) {
            (void)CAS(&chan_priv->sample_status, PROF_SAMPLE_FLUSH_STARTING, PROF_SAMPLE_FLUSH_FINISH);
            break;
        }

        if ((ret != 0) || (para.report_len > buff_len)) {
            (void)CAS(&chan_priv->sample_status, PROF_SAMPLE_FLUSH_STARTING, PROF_SAMPLE_FLUSH_FINISH);
            continue;
        }

        if (para.report_len != 0) {
            (void)notifier->chan_report(dev_id, chan_id, para.buff, para.report_len, false);
        }

        para.sample_flag = SAMPLE_DATA_ONLY;
        (void)CAS(&chan_priv->sample_status, PROF_SAMPLE_FLUSH_STARTING, PROF_SAMPLE_FLUSH_FINISH);
    }

    free(buff);

    PROF_INFO("Sample thread exit. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
    return NULL;
}

drvError_t prof_user_sample_thread_enable(struct prof_user_chan_priv *chan_priv)
{
    pthread_attr_t attr;
    int ret;

    chan_priv->sample_thread_flag = 1;

    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    ret = pthread_create(&chan_priv->sample_thread, &attr, prof_user_sample_thread, chan_priv);
    if (ret != 0) {
        chan_priv->sample_thread_flag = 0;
        PROF_ERR("Failed to create the sample thread. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    (void)pthread_attr_destroy(&attr);

    return DRV_ERROR_NONE;
}

void prof_user_sample_thread_disable(struct prof_user_chan_priv *chan_priv)
{
    /* Non-periodic sampling */
    if (chan_priv->sample_thread_flag == 0) {
        return;
    }

    chan_priv->sample_thread_flag = 0;
    (void)sem_post(chan_priv->sem);

    (void)pthread_join(chan_priv->sample_thread, NULL);
}

STATIC void prof_user_sample_timer_handle(union sigval v)
{
    sem_t *sem = (sem_t *)v.sival_ptr;
    (void)sem_post(sem);
}

drvError_t prof_user_sample_timer_init(struct prof_user_chan_priv *chan_priv, uint32_t sample_period)
{
    struct sigevent sig_ev = {0};
    struct itimerspec ts;
    int ret;

    sig_ev.sigev_value.sival_ptr = chan_priv->sem;
    sig_ev.sigev_notify = SIGEV_THREAD;
    sig_ev.sigev_notify_function = prof_user_sample_timer_handle;
    ret = timer_create(CLOCK_MONOTONIC, &sig_ev, &chan_priv->timer_fd);
    if (ret != 0) {
        PROF_ERR("Failed to create timer. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    ts.it_interval.tv_sec = sample_period / 1000;              /* 1000ms = 1s */
    ts.it_interval.tv_nsec = (sample_period % 1000) * 1000000; /* 1000ms = 1s 1000000ns = 1ms */
    ts.it_value.tv_sec = ts.it_interval.tv_sec;
    ts.it_value.tv_nsec = ts.it_interval.tv_nsec;
    ret = timer_settime(chan_priv->timer_fd, 0, &ts, NULL);
    if (ret != 0) {
        (void)timer_delete(chan_priv->timer_fd);
        PROF_ERR("Failed to set timer. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

void prof_user_sample_timer_uninit(struct prof_user_chan_priv *chan_priv)
{
    int ret;

    /* Non-periodic sampling */
    if (chan_priv->timer_fd == NULL) {
        return;
    }

    ret = timer_delete(chan_priv->timer_fd);
    if (ret != 0) {
        PROF_ERR("Failed to delete timer. (ret=%d)\n", ret);
    }
    chan_priv->timer_fd = NULL;
}

STATIC drvError_t prof_user_chan_start(uint32_t dev_id, uint32_t chan_id, struct prof_user_start_para *para, char *priv)
{
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)priv;
    drvError_t ret;
    struct prof_start_event_out_msg start_out_msg = {0};

    ret = prof_user_sample_start_check(chan_priv, para);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to check para. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    ret = prof_user_sample_start(chan_priv, para, &start_out_msg, false);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to start user sample. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    /* Non-periodic sampling */
    if (para->sample_period == 0) {
        return DRV_ERROR_NONE;
    }

    ret = prof_user_sample_thread_enable(chan_priv);
    if (ret != DRV_ERROR_NONE) {
        prof_user_sample_stop(chan_priv, PROF_STOP_STAGE_DEFAULT);
        PROF_ERR("Failed to enable thread. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    ret = prof_user_sample_timer_init(chan_priv, para->sample_period);
    if (ret != DRV_ERROR_NONE) {
        prof_user_sample_thread_disable(chan_priv);
        prof_user_sample_stop(chan_priv, PROF_STOP_STAGE_DEFAULT);
        PROF_ERR("Failed to init timer. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t prof_user_chan_stop(uint32_t dev_id, uint32_t chan_id, struct prof_user_stop_para *para, char *priv)
{
    (void)para;
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)priv;

    prof_user_sample_timer_uninit(chan_priv);
    prof_user_sample_thread_disable(chan_priv);
    prof_user_sample_stop(chan_priv, para->host_sample_release_flag);

    if (chan_priv->sample_mode == PROF_SAMPLE_LOCAL_MODE) {
        prof_buff_wait_read_empty(chan_priv->buff, dev_id, chan_id);
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_user_chan_flush(uint32_t dev_id, uint32_t chan_id, uint32_t *data_len, char *priv)
{
    (void)dev_id;
    (void)chan_id;
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)priv;

    if (chan_priv->sample_mode == PROF_SAMPLE_LOCAL_MODE) {
        *data_len = prof_buff_get_data_len(chan_priv->buff);
        return DRV_ERROR_NONE;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }
}

int prof_user_chan_read(uint32_t dev_id, uint32_t chan_id, prof_user_read_para_t *read_para, char *priv)
{
    (void)dev_id;
    (void)chan_id;
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)priv;

    if (chan_priv->sample_mode == PROF_SAMPLE_LOCAL_MODE) {
        return prof_buff_read(chan_priv->buff, read_para->out_buf, read_para->buf_size);
    } else {
        return 0;
    }
}

STATIC drvError_t prof_user_chan_query(uint32_t dev_id, uint32_t chan_id, uint32_t *avail_len, char *priv)
{
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)priv;
    struct prof_user_kernel_ops *kernel_ops = NULL;
    drvError_t ret;

    if (chan_priv->sample_mode == PROF_SAMPLE_LOCAL_MODE) {
        *avail_len = prof_buff_get_avail_len(chan_priv->buff);
        return DRV_ERROR_NONE;
    }

    kernel_ops = prof_user_get_kernel_ops();
    if (kernel_ops->chan_query != NULL) {
        ret = kernel_ops->chan_query(dev_id, chan_id, avail_len);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("Failed to query avail_len from kernel. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id,
                     (int)ret);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

drvError_t prof_user_chan_report(uint32_t dev_id, uint32_t chan_id, void *data, uint32_t data_len, char *priv)
{
    struct prof_user_chan_priv *chan_priv = (struct prof_user_chan_priv *)priv;
    struct prof_adapt_core_notifier *notifier = NULL;
    struct prof_user_kernel_ops *kernel_ops = NULL;
    drvError_t ret;

    if (chan_priv->sample_mode == PROF_SAMPLE_LOCAL_MODE) {
        ret = prof_buff_write(chan_priv->buff, data, data_len);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("Failed to write data to local buff. (dev_id=%u, chan_id=%u, data_len=%u, ret=%d)\n", dev_id,
                     chan_id, data_len, (int)ret);
            return ret;
        }

        notifier = prof_adapt_get_notifier();
        notifier->poll_report(dev_id, chan_id);
        return DRV_ERROR_NONE;
    }

    kernel_ops = prof_user_get_kernel_ops();
    if (kernel_ops->chan_writer != NULL) {
        ret = kernel_ops->chan_writer(dev_id, chan_id, data, data_len);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("Failed to write data to kernel. (dev_id=%u, chan_id=%u, data_len=%u, ret=%d)\n", dev_id, chan_id,
                     data_len, (int)ret);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

static struct prof_chan_ops prof_user_chan_ops = {
    .init = prof_user_chan_init,
    .uninit = prof_user_chan_uninit,
    .start = prof_user_chan_start,
    .stop = prof_user_chan_stop,
    .flush = prof_user_chan_flush,
    .read = prof_user_chan_read,
    .query = prof_user_chan_query,
    .report = prof_user_chan_report,
};

drvError_t prof_user_get_chan_ops(struct prof_chan_ops **ops)
{
    *ops = &prof_user_chan_ops;
    return DRV_ERROR_NONE;
}

#else
int prof_adapt_user_ut_test(void)
{
    return 0;
}
#endif
