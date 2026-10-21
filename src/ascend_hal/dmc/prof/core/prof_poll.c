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
#include <pthread.h>
#include <semaphore.h>

#include "prof_common.h"
#include "prof_poll.h"

struct prof_poll_mng {
    pthread_mutex_t queue_mutex;
    prof_poll_info_t queue[PROF_POLL_DEPTH];
    uint32_t head;
    uint32_t tail;

    uint8_t poll_flag[DEV_NUM][PROF_CHANNEL_NUM_MAX]; /* Handling concurrency through atomic operations */
    int fd_num;                                       /* Handling concurrency through atomic operations */

    sem_t sem;
};

static struct prof_poll_mng *g_prof_poll = NULL;

STATIC drvError_t prof_poll_mng_init(void)
{
    struct prof_poll_mng *poll_mng = NULL;

    poll_mng = (struct prof_poll_mng *)calloc(1, sizeof(*poll_mng));
    if (poll_mng == NULL) {
        PROF_ERR("Failed to alloc poll_mng.\n");
        return DRV_ERROR_MALLOC_FAIL;
    }

    (void)pthread_mutex_init(&poll_mng->queue_mutex, NULL);
    (void)sem_init(&poll_mng->sem, 0, 0);

    g_prof_poll = poll_mng;
    return DRV_ERROR_NONE;
}

STATIC struct prof_poll_mng *prof_poll_get_mng(void)
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    static int init_flag = 0;
    drvError_t ret;

    if (init_flag != 0) {
        return g_prof_poll;
    }

    (void)pthread_mutex_lock(&init_mutex);
    if (init_flag == 0) {
        ret = prof_poll_mng_init();
        if (ret != DRV_ERROR_NONE) {
            (void)pthread_mutex_unlock(&init_mutex);
            PROF_ERR("Failed to init poll_mng. (ret=%d)\n", (int)ret);
            return NULL;
        }

        ATOMIC_SET(&init_flag, 1);
    }
    (void)pthread_mutex_unlock(&init_mutex);

    return g_prof_poll;
}

STATIC void prof_poll_enqueue(struct prof_poll_mng *poll_mng, uint32_t dev_id, uint32_t chan_id)
{
    poll_mng->queue[poll_mng->tail].device_id = dev_id;
    poll_mng->queue[poll_mng->tail].channel_id = chan_id;

    poll_mng->tail = (poll_mng->tail + 1U) % PROF_POLL_DEPTH;
}

STATIC drvError_t prof_poll_batch_dequeue(struct prof_poll_mng *poll_mng, uint8_t *out_buf, uint32_t max_num,
                                          uint32_t num)
{
    int ret;

    ret = memcpy_s(out_buf, max_num * sizeof(prof_poll_info_t), (uint8_t *)&poll_mng->queue[poll_mng->head],
                   num * sizeof(prof_poll_info_t));
    if (ret != 0) {
        PROF_ERR("Failed to copy poll_info. (ret=%d, max_num=%u, num=%u)\n", ret, max_num, num);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    poll_mng->head = (poll_mng->head + num) % PROF_POLL_DEPTH;
    return DRV_ERROR_NONE;
}

STATIC bool prof_poll_enable_flag(struct prof_poll_mng *poll_mng, uint32_t dev_id, uint32_t chan_id)
{
    return CAS(&poll_mng->poll_flag[dev_id][chan_id], (uint8_t)POLL_INVALID, (uint8_t)POLL_VALID);
}

STATIC void prof_poll_disable_flag(struct prof_poll_mng *poll_mng, uint32_t dev_id, uint32_t chan_id)
{
    ATOMIC_SET(&poll_mng->poll_flag[dev_id][chan_id], (uint8_t)POLL_INVALID);
}

STATIC void prof_poll_batch_disable_flag(struct prof_poll_mng *poll_mng, struct prof_poll_info *poll_info,
                                         uint32_t poll_num)
{
    uint32_t i;

    for (i = 0; i < poll_num; i++) {
        prof_poll_disable_flag(poll_mng, poll_info[i].device_id, poll_info[i].channel_id);
    }
}

void prof_poll_chan_start(uint32_t dev_id, uint32_t chan_id)
{
    struct prof_poll_mng *poll_mng = prof_poll_get_mng();

    if (poll_mng == NULL) {
        PROF_ERR("Failed to get poll_mng. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return;
    }

    (void)ATOMIC_INC(&poll_mng->fd_num);
}

void prof_poll_chan_stop(uint32_t dev_id, uint32_t chan_id)
{
    struct prof_poll_mng *poll_mng = prof_poll_get_mng();

    if ((dev_id >= DEV_NUM) || (chan_id >= PROF_CHANNEL_NUM_MAX)) {
        PROF_ERR("Invalid para.(dev_id=%u,chan_id=%u)\n", dev_id, chan_id);
        return;
    }

    if (poll_mng == NULL) {
        PROF_ERR("Failed to get poll_mng. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return;
    }

    prof_poll_disable_flag(poll_mng, dev_id, chan_id);

    if (ATOMIC_DEC(&poll_mng->fd_num) <= 0) {
        (void)sem_post(&poll_mng->sem);
    }
}

void prof_poll_report(uint32_t dev_id, uint32_t chan_id)
{
    struct prof_poll_mng *poll_mng = prof_poll_get_mng();

    if ((dev_id >= DEV_NUM) || (chan_id >= PROF_CHANNEL_NUM_MAX)) {
        PROF_ERR("Invalid para. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return;
    }

    if (poll_mng == NULL) {
        PROF_ERR("Failed to get poll_mng. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return;
    }

    if (prof_poll_enable_flag(poll_mng, dev_id, chan_id)) {
        (void)pthread_mutex_lock(&poll_mng->queue_mutex);
        prof_poll_enqueue(poll_mng, dev_id, chan_id);
        (void)pthread_mutex_unlock(&poll_mng->queue_mutex);

        (void)sem_post(&poll_mng->sem);
    }
}

STATIC int prof_poll_copy(struct prof_poll_mng *poll_mng, struct prof_poll_info *out_buf, uint32_t max_num,
                          uint32_t *poll_num)
{
    uint32_t num, polled_num;
    drvError_t ret;

    /* Scenario 1: the queue is empty */
    if (poll_mng->head == poll_mng->tail) {
        *poll_num = 0;
        return PROF_OK;
    }

    /* Scenario 2: head < tail, read once */
    if (poll_mng->head < poll_mng->tail) {
        num = poll_mng->tail - poll_mng->head;
        num = (num > max_num) ? max_num : num;
        ret = prof_poll_batch_dequeue(poll_mng, (uint8_t *)out_buf, max_num, num);
        if (ret != DRV_ERROR_NONE) {
            return PROF_ERROR;
        }

        *poll_num = num;
        return PROF_OK;
    }

    /* Scenario 3: head > tail, segmented reading */
    num = PROF_POLL_DEPTH - poll_mng->head;
    num = (num > max_num) ? max_num : num;
    ret = prof_poll_batch_dequeue(poll_mng, (uint8_t *)out_buf, max_num, num);
    if (ret != DRV_ERROR_NONE) {
        return PROF_ERROR;
    }

    if ((num >= max_num) || (poll_mng->tail == 0)) {
        *poll_num = num;
        return PROF_OK;
    }

    polled_num = num;
    num = poll_mng->tail;
    num = ((num + polled_num) > max_num) ? (max_num - polled_num) : num;
    ret = prof_poll_batch_dequeue(poll_mng, ((uint8_t *)out_buf + polled_num * sizeof(prof_poll_info_t)),
                                  (max_num - polled_num), num);
    if (ret != DRV_ERROR_NONE) {
        /* Should return success and the copied channels */
        *poll_num = polled_num;
        return PROF_OK;
    }

    *poll_num = polled_num + num;
    return PROF_OK;
}

int prof_poll_read(struct prof_poll_info *out_buf, uint32_t max_num, int timeout)
{
    struct prof_poll_mng *poll_mng = prof_poll_get_mng();
    struct timespec ts;
    uint32_t poll_num = 0;
    int ret;

    if (poll_mng == NULL) {
        PROF_ERR("Failed to get poll_mng.\n");
        return PROF_ERROR;
    }

    if (poll_mng->fd_num <= 0) {
        return PROF_STOPPED_ALREADY;
    }

    ret = clock_gettime(CLOCK_REALTIME, &ts);
    if (ret != 0) {
        PROF_ERR("Failed to invoke function [clock_gettime]. (ret=%d, errno=%d)\n", ret, errno);
        return PROF_ERROR;
    }

    ts.tv_sec += ((timeout <= 0) || (timeout > 5)) ? 5 : timeout; /* 5s */
    ret = sem_timedwait(&poll_mng->sem, &ts);
    if (ret != 0) {
        if ((errno == ETIMEDOUT) || (errno == EINTR)) {
            PROF_DEBUG("Function [sem_timedwait] invoked timed out or interrupted. (ret=%d, result_code=%d)\n", ret,
                       errno);
        } else {
            PROF_ERR("Failed to invoke function [sem_timedwait]. (ret=%d, errno=%d)\n", ret, errno);
            return PROF_ERROR;
        }
    } else {
        (void)pthread_mutex_lock(&poll_mng->queue_mutex);
        ret = prof_poll_copy(poll_mng, out_buf, max_num, &poll_num);
        (void)pthread_mutex_unlock(&poll_mng->queue_mutex);
        if (ret != PROF_OK) {
            PROF_ERR("Failed to copy poll channels. (ret=%d)\n", ret);
            return PROF_ERROR;
        }
        prof_poll_batch_disable_flag(poll_mng, out_buf, poll_num);
    }

    return (int)poll_num;
}

#else
int prof_poll_ut_test(void)
{
    return 0;
}
#endif
