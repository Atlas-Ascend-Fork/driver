/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef EMU_ST
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "securec.h"
#include "queue.h"
#include "que_compiler.h"
#include "que_ctx.h"
#include "que_chan.h"
#include "semaphore.h"
#include "que_ub_msg.h"
#include "que_comm_thread.h"

#define QUEUE_POLL_NAME "poll_thread"
#define QUEUE_POLL_D2D_NAME "poll_d2d_thread"
static pthread_t g_queue_thread_handle[MAX_DEVICE][QUEUE_THREAD_BUTT] = {{0}};
static int g_poll_thread_stat[MAX_DEVICE];
static int g_poll_d2d_thread_stat[MAX_DEVICE];
static int g_recycle_thread_stat[MAX_DEVICE];
static struct que_query_alive_msg *g_que_list[MAX_DEVICE] = {NULL};

static void *que_poll_thread(void *data)
{
    (void)prctl(PR_SET_NAME, QUEUE_POLL_NAME);
    unsigned int devid = (unsigned int)(uintptr_t)data;

    while (g_poll_thread_stat[devid]) {
        que_ctx_poll(devid, TRANS_D2H_H2D);
    }
    return NULL;
}

#define QUEUE_WAIT_F2NF_NAME "wait_f2nf_thread"
void *que_wait_f2nf_thread(void *data)
{
    (void)prctl(PR_SET_NAME, QUEUE_WAIT_F2NF_NAME);
    unsigned int devid = (unsigned int)(uintptr_t)data;
    que_ctx_wait_f2nf(devid);
    return NULL;
}

#define QUEUE_THREAD_SLEEP_INTERVAL (5000000) // 5000ms
#define QUEUE_RECYCLE_POLL_NAME "que_chan_recycle_thread"
static void *que_recycle_thread(void *data)
{
    (void)prctl(PR_SET_NAME, QUEUE_RECYCLE_POLL_NAME);
    unsigned int devid = (unsigned int)(uintptr_t)data;

    g_que_list[devid] = (struct que_query_alive_msg *)calloc(1, sizeof(struct que_query_alive_msg));
    if (g_que_list[devid] == NULL) {
        QUEUE_LOG_ERR("que recycle thread create fail. (devid=%u)\n", devid);
        return NULL;
    }
    while (g_recycle_thread_stat[devid]) {
        que_ctx_chan_recycle(devid, g_que_list[devid]);
        (void)usleep(QUEUE_THREAD_SLEEP_INTERVAL);
    }
    free(g_que_list[devid]);
    g_que_list[devid] = NULL;
    return NULL;
}

static void *que_poll_d2d_thread(void *data)
{
    (void)prctl(PR_SET_NAME, QUEUE_POLL_D2D_NAME);
    unsigned int devid = (unsigned int)(uintptr_t)data;

    while (g_poll_d2d_thread_stat[devid]) {
        que_ctx_poll(devid, TRANS_D2D);
    }
    return NULL;
}

int que_create_poll_d2d_thread(unsigned int devid)
{
    pthread_t poll_thread;
    pthread_attr_t attr;
    int ret;
    (void)pthread_attr_init(&attr);
    g_poll_d2d_thread_stat[devid] = 1;
    ret = pthread_create(&poll_thread, &attr, que_poll_d2d_thread, (void *)(unsigned long)devid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        g_poll_d2d_thread_stat[devid] = 0;
        QUEUE_LOG_ERR("que ub pool d2d thread create fail. (ret=%d; devid=%u)\n", ret, devid);
    } else {
        g_queue_thread_handle[devid][QUEUE_POLL_D2D] = poll_thread;
    }

    (void)pthread_attr_destroy(&attr);
    return ret;
}

void que_cancle_poll_d2d_thread(unsigned int devid)
{
    int ret;
    pthread_t curr_thread = g_queue_thread_handle[devid][QUEUE_POLL_D2D];
    if (curr_thread != 0) {
        g_poll_d2d_thread_stat[devid] = 0;
        ret = pthread_join(curr_thread, NULL);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("que poll d2d thread join fail. (ret=%d)\n", ret);
        }
        g_queue_thread_handle[devid][QUEUE_POLL_D2D] = 0;
    }
}

int que_create_poll_thread(unsigned int devid)
{
    pthread_t poll_thread;
    pthread_attr_t attr;
    int ret;

    (void)pthread_attr_init(&attr);
    g_poll_thread_stat[devid] = 1;
    ret = pthread_create(&poll_thread, &attr, que_poll_thread, (void *)(unsigned long)devid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        g_poll_thread_stat[devid] = 0;
        QUEUE_LOG_ERR("que ub pool thread create fail. (ret=%d; devid=%u)\n", ret, devid);
    } else {
        g_queue_thread_handle[devid][QUEUE_POLL] = poll_thread;
    }

    (void)pthread_attr_destroy(&attr);
    return ret;
}

static void que_cancle_poll_thread(unsigned int devid)
{
    int ret;
    pthread_t curr_thread = g_queue_thread_handle[devid][QUEUE_POLL];
    if (curr_thread != 0) {
        g_poll_thread_stat[devid] = 0;
        ret = pthread_join(curr_thread, NULL);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("que poll thread join fail. (ret=%d)\n", ret);
        }
        g_queue_thread_handle[devid][QUEUE_POLL] = 0;
    }
}

int que_create_wait_f2nf_thread(unsigned int devid)
{
    pthread_t wait_f2nf_thread;
    pthread_attr_t attr;
    int ret;

    (void)pthread_attr_init(&attr);

    ret = pthread_create(&wait_f2nf_thread, &attr, que_wait_f2nf_thread, (void *)(unsigned long)devid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ub wait f2nf thread create fail. (ret=%d; devid=%u)\n", ret, devid);
    } else {
        g_queue_thread_handle[devid][QUEUE_WAIT_F2NF] = wait_f2nf_thread;
    }

    (void)pthread_attr_destroy(&attr);
    return ret;
}

static void que_cancle_wait_f2nf_thread(unsigned int devid)
{
    int ret;
    pthread_t curr_thread = g_queue_thread_handle[devid][QUEUE_WAIT_F2NF];

    if (curr_thread != 0) {
        ret = que_ctx_bulid_f2nf_event(devid);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("que wake up f2nf thread fail. (ret=%d)\n", ret);
            return;
        }
        ret = pthread_join(curr_thread, NULL);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("que wait f2nf thread join fail. (ret=%d)\n", ret);
        }
        g_queue_thread_handle[devid][QUEUE_WAIT_F2NF] = 0;
    }
}

int que_create_recycle_thread(unsigned int devid)
{
    pthread_t poll_thread;
    pthread_attr_t attr;
    int ret;

    (void)pthread_attr_init(&attr);
    g_recycle_thread_stat[devid] = 1;
    ret = pthread_create(&poll_thread, &attr, que_recycle_thread, (void *)(unsigned long)devid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        g_recycle_thread_stat[devid] = 0;
        QUEUE_LOG_ERR("que ub pool thread create fail. (ret=%d; devid=%u)\n", ret, devid);
    } else {
        g_queue_thread_handle[devid][QUEUE_RECYCLE] = poll_thread;
    }

    (void)pthread_attr_destroy(&attr);
    return ret;
}

static void que_cancle_recycle_thread(unsigned int devid)
{
    int ret;
    pthread_t curr_thread = g_queue_thread_handle[devid][QUEUE_RECYCLE];
    if (curr_thread != 0) {
        g_recycle_thread_stat[devid] = 0;
        ret = pthread_join(curr_thread, NULL);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("que recycle thread join fail. (ret=%d)\n", ret);
        }
        g_queue_thread_handle[devid][QUEUE_RECYCLE] = 0;
    }
}

void que_thread_cancle(unsigned int devid)
{
    que_cancle_wait_f2nf_thread(devid);
    que_cancle_poll_thread(devid);
    que_cancle_recycle_thread(devid);
}
#else

void que_comm_thread_emu_test(void)
{}

#endif
