/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

#include "atomic_lock.h"
#include "atomic_ref.h"
#include "securec.h"
#include "ascend_hal.h"
#include "drv_buff_common.h"
#include "drv_buff_unibuff.h"
#include "drv_buff_mbuf.h"
#include "esched_user_interface.h"
#include "pbl_uda_user.h"
#include "queue_user_manage.h"
#include "queue_interface.h"
#include "queue.h"

#define MAX_STR_LEN 128
#define LOG_TIME_INTERVAL 100        /* 100 s */
#define MAX_ENQUE_TIME (38 * 100000) /* 100ms */
#define QUEUE_DEPT_RES_SIZE 2
#define MAX_DEQUEUE_TRY_TIMES 3
#define ENQUE_EVENT_TIMEOUT (38 * 1000000) /* enque timeout 1s */

typedef struct enque_timestamps {
    uint64 total_start_timestamp; /* enque */
    uint64 overwrite_start_timstamp;
    uint64 overwrite_end_timestamp;
    uint64 submit_start_timestamp;
    uint64 kern_start_timestamp;
    uint64 kern_submit_timstamp;
    uint64 kern_end_timestamp;
    uint64 submit_end_timestamp;
    uint64 total_end_timestamp;
} enque_timestamps;

THREAD pid_t g_cur_pid = 0;

enum queue_delay_action {
    QUEUE_DELAY_ENTRY,
    QUEUE_DELAY_OUT,
    QUEUE_DELAY_ACTION_MAX
};

struct queue_delay_trace {
    unsigned int time_threshold;
    unsigned int cost_time;
    unsigned int start_time;
    unsigned int pid;
    unsigned int rsv[4];
};

static inline unsigned int queue_get_tail(struct queue_manages *que_mng, unsigned int qid)
{
    unsigned int depth = queue_get_local_depth(qid);
    unsigned int tail = atomic_value_get(&que_mng->tail_status);
    return tail % depth;
}

static inline bool queue_enque_set_tail(struct queue_manages *que_mng, unsigned int depth, unsigned int qid)
{
    (void)qid;
    if (depth == 0) {
        return false;
    }
    unsigned int tail = (queue_get_orig_tail(que_mng) + 1) % depth;
    return atomic_value_set(&que_mng->tail_status, tail);
}

static inline unsigned int get_queue_size(unsigned int head, unsigned int tail, unsigned int depth)
{
    return (head <= tail) ? (tail - head) : (tail + depth - head);
}

static inline unsigned long long queue_get_head_value(struct queue_manages *que_manage)
{
    return (que_manage->queue_head.head_value);
}

static inline bool is_queue_full(struct queue_manages *que_manage, unsigned int qid)
{
    unsigned int depth = queue_get_local_depth(qid);
    if ((depth < MIN_VALID_QUEUE_DEPTH) || (depth > MAX_QUEUE_DEPTH)) {
        return true;
    }

    return (que_manage->queue_head.head_info.head == ((queue_get_orig_tail(que_manage) + 1) % depth)) ? true : false;
}

static inline unsigned int get_que_size_by_mng(struct queue_manages *que_manage, unsigned int qid)
{
    unsigned int head, tail, size;

    tail = queue_get_orig_tail(que_manage);
    head = que_manage->queue_head.head_info.head;
    size = get_queue_size(head, tail, queue_get_local_depth(qid));

    return size % MAX_QUEUE_DEPTH;
}

static bool queue_device_invalid(unsigned int dev_id)
{
    return (dev_id >= MAX_DEVICE);
}

static inline bool is_queue_empty(struct queue_manages *que_manage)
{
    return (que_manage->queue_head.head_info.head == queue_get_orig_tail(que_manage)) ? true : false;
}

static inline unsigned long long queue_head_inc(union atomic_queue_head cur_head_value, unsigned int depth)
{
    union atomic_queue_head new_head_value;
    unsigned int new_head = cur_head_value.head_info.head + 1;

    new_head_value.head_info.head = ((new_head >= depth) ? (0) : (new_head));
    new_head_value.head_info.index = cur_head_value.head_info.index + 1;

    return new_head_value.head_value;
}

static drvError_t get_queue_manage_by_qid(unsigned int devid, unsigned int qid, struct queue_manages **queue_manage)
{
    struct queue_manages *local_manage = NULL;

    if (queue_device_invalid(devid)) {
        QUEUE_LOG_ERR("para is error. (dev_id=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (qid >= MAX_SURPORT_QUEUE_NUM) {
        QUEUE_LOG_ERR("para is error. (qid=%u)\n", qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    local_manage = queue_get_local_mng(qid);
    if (local_manage == NULL) {
        QUEUE_LOG_WARN("Queue local manage is null. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    *queue_manage = local_manage;
    return DRV_ERROR_NONE;
}

drvError_t queue_init_local(unsigned int dev_id)
{
    static pthread_mutex_t g_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
    static THREAD int g_queue_init_status[MAX_DEVICE] = {0};
    static THREAD bool flag = false;
    drvError_t ret;

    (void)pthread_mutex_lock(&g_queue_mutex);
    if (g_queue_init_status[dev_id] == QUEUE_INITED) {
        (void)pthread_mutex_unlock(&g_queue_mutex);
        return DRV_ERROR_REPEATED_INIT;
    }

    if (flag == false) {
        ret = queue_dc_init();
        if (ret != 0) {
            (void)pthread_mutex_unlock(&g_queue_mutex);
            return ret;
        }

        g_cur_pid = GETPID();
        flag = true;
    }

    g_queue_init_status[dev_id] = QUEUE_INITED;
    (void)pthread_mutex_unlock(&g_queue_mutex);

    QUEUE_RUN_LOG_INFO("init queue success. (dev_id=%u)\n", dev_id);

    return DRV_ERROR_NONE;
}

static drvError_t queue_create_para_check(unsigned int dev_id, const QueueAttr *que_attr, unsigned int *qid)
{
    (void)qid;
    unsigned long len;
    (void)dev_id;
    if ((que_attr->depth < MIN_VALID_QUEUE_DEPTH) || (que_attr->depth > MAX_QUEUE_DEPTH) ||
        (que_attr->workMode > QUEUE_MODE_PULL)) { // work_mod 0 means : default PUSH
        QUEUE_LOG_ERR("Input para error. (depth=%u, work_mode=%u, flowctrl_flag=%d, drop_time=%ums).\n",
                      que_attr->depth, que_attr->workMode, que_attr->flowCtrlFlag, que_attr->flowCtrlDropTime);
        return DRV_ERROR_INVALID_VALUE;
    }

    len = strnlen(que_attr->name, MAX_STR_LEN);
    if (len >= MAX_STR_LEN) {
        QUEUE_LOG_ERR("Input para error. (name_len=%lu)\n", len);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static drvError_t queue_init_mng_info(struct queue_manages *que_mng, const QueueAttr *que_attr, unsigned int qid,
                                      unsigned int dev_id)
{
    int ret;
    struct timeval create_time;
    init_atomic_lock(&que_mng->merge_atomic_lock);
    ret = strncpy_s(que_mng->name, MAX_STR_LEN, que_attr->name, strnlen(que_attr->name, MAX_STR_LEN));
    if (ret != 0) {
        QUEUE_LOG_ERR("memcpy queue name failed. (qid=%u; name=%s; ret=%d)\n", qid, que_attr->name, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    que_mng->dev_id = dev_id;
    que_mng->id = qid;
    que_mng->queue_head.head_value = 0;
    que_get_time(&create_time);
    que_mng->create_time = (unsigned long)(create_time.tv_usec + create_time.tv_sec * USEC_PER_SEC);
    que_mng->creator_pid = getpid();
    atomic_value_init(&que_mng->tail_status);
    que_mng->enque_cas = 0;
    que_mng->deque_cas = 0;
    que_mng->fctl_flag = que_attr->flowCtrlFlag;
    que_mng->drop_time = que_attr->flowCtrlDropTime;
    que_mng->over_write = que_attr->overWriteFlag;
    que_mng->work_mode = (int)((que_attr->workMode != (unsigned int)QUEUE_MODE_PULL) ? QUEUE_MODE_PUSH :
                                                                                       QUEUE_MODE_PULL);
    que_mng->merge_idx = INVALID_QUEUE_MERGE_IDX;
    que_mng->remote_devid = QUEUE_INVALID_VALUE;
    que_mng->phy_remote_devid = QUEUE_INVALID_VALUE;
    que_mng->remote_qid = QUEUE_INVALID_VALUE;
    que_mng->remote_devpid = QUE_INTER_DEV_INVALID_VALUE;
    que_mng->remote_grpid = QUE_INTER_DEV_INVALID_VALUE;
    que_mng->valid = QUEUE_CREATED;
    que_mng->inter_dev_state = QUEUE_STATE_DISABLED;

    return DRV_ERROR_NONE;
}

static drvError_t queue_create_local(unsigned int dev_id, const QueueAttr *que_attr, unsigned int *qid)
{
    struct queue_local_info local_info;
    drvError_t ret;

    ret = queue_create_para_check(dev_id, que_attr, qid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = queue_create(que_attr->depth, qid, &local_info);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_create failed. (device_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = queue_init_mng_info(local_info.que_mng, que_attr, (*qid), dev_id);
    if (ret != DRV_ERROR_NONE) {
        queue_delete(*qid, (void *)local_info.entity);
        QUEUE_LOG_ERR("init manage info failed. (device_id=%u; queue_id=%u; depth=%u; ret=%d)\n", dev_id, *qid,
                      que_attr->depth, ret);
        return ret;
    }

    queue_set_local_info(*qid, &local_info);

    QUEUE_RUN_LOG_INFO("create queue success. (device_id=%u; qid=%u; depth=%u; name=%s; mode=%u; freq=%llu)\n", dev_id,
                       *qid, que_attr->depth, que_attr->name, que_attr->workMode, esched_get_sys_freq());
    return DRV_ERROR_NONE;
}

static drvError_t queue_grant_para_check(unsigned int dev_id, unsigned int qid, int pid)
{
    (void)dev_id;
    if (pid <= 0) {
        QUEUE_LOG_ERR("pid not exist. (qid=%u, pid=%d)\n", qid, pid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_grant_local(unsigned int dev_id, unsigned int qid, int pid, QueueShareAttr attr)
{
    struct queue_manages *que_manage = NULL;
    drvError_t ret;

    ret = queue_grant_para_check(dev_id, qid, pid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    que_manage = queue_get_local_mng(qid);
    if (que_manage == NULL) {
        QUEUE_LOG_ERR("Queue local manage is null. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_NOT_EXIST;
    }
    if (que_manage->valid != QUEUE_CREATED) {
        QUEUE_LOG_ERR("queue is not created. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }
    QUEUE_RUN_LOG_INFO("queue grant res. (dev_id=%u; qid=%d; pid=%u; manage=%u; read=%u; write=%u; ret=%d)\n", dev_id,
                       qid, pid, attr.manage, attr.read, attr.write, ret);

    return ret;
}

drvError_t queue_attach_local(unsigned int dev_id, unsigned int qid, int time_out)
{
    struct queue_manages *que_manage = NULL;
    struct queue_local_info local_info;
    drvError_t ret;

    que_manage = queue_get_local_mng(qid);
    if (que_manage != NULL) {
        return DRV_ERROR_NONE;
    }

    ret = queue_attach(qid, g_cur_pid, time_out, &local_info);
    if (ret != 0) {
        QUEUE_LOG_ERR("Queue attach failed.(qid=%u; timeout=%dms; ret=%d)\n", qid, time_out, ret);
        return ret;
    }

    queue_set_local_info(qid, &local_info);
    QUEUE_RUN_LOG_INFO("queue attach success. (qid=%u, dev=%u, timeout=%dms)\n", qid, dev_id, time_out);

    return DRV_ERROR_NONE;
}

/***********************************************************************************************************
Function: When deleting a queue, release the mbuf memory.
************************************************************************************************************/
static drvError_t de_queue_inner(struct queue_manages *manage, unsigned int qid)
{
    union atomic_queue_head cur_head, new_head;
    unsigned int head, tail, depth;
    queue_entity_node *que_entity = NULL;
    void *buff = NULL;
    drvError_t ret;
    uint32_t blk_id;

    queue_get_entity_and_depth(qid, &que_entity, &depth);
    if (que_entity == NULL) {
        QUEUE_LOG_ERR("queue entity is NULL. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    cur_head.head_value = queue_get_head_value(manage);
    head = ((cur_head.head_info.head) % queue_get_local_depth(qid));
    tail = queue_get_tail(manage, qid);
    while (head != tail) {
        Mbuf *mbuf = NULL;
        buff = que_entity[head].node;
        blk_id = que_entity[head].node_desp.blk_id;
        new_head.head_value = queue_head_inc(cur_head, depth);
        if (CAS(&manage->queue_head.head_value, cur_head.head_value, new_head.head_value) == true) {
            ret = create_priv_mbuf_for_queue(&mbuf, buff, blk_id);
            if (ret == DRV_ERROR_NONE) {
                (void)mbuf_free_for_queue(mbuf, MBUF_FREE_BY_QUEUE_DS);
            } else {
                QUEUE_LOG_WARN("dequeue Mbuf is illegal. (qid=%u; mbuf=%p; head=%u)\n", manage->id, buff, head);
            }
        }

        cur_head.head_value = new_head.head_value;
        head = ((cur_head.head_info.head) % queue_get_local_depth(qid));
    }

    return DRV_ERROR_NONE;
}

STATIC void queue_wake_up_wait_event(struct queue_manages *que_manage, int rsp_ret)
{
    queue_send_wake_up_rsp_platform(que_manage, rsp_ret);
}

void queue_detach_invalid_queue(void)
{
    struct queue_manages *que_manage = NULL;
    unsigned int i;

    for (i = 0; i < MAX_SURPORT_QUEUE_NUM; i++) {
        if (queue_is_valid(i) == false) {
            continue;
        }
        que_manage = queue_get_local_mng(i);
        if (que_manage == NULL) {
            continue;
        }
        if (que_manage->valid != QUEUE_UNCREATED) {
            continue;
        }
        /* Prevent concurrency with the main thread destroy */
        if (queue_local_is_attached_and_set_detached(i) == false) {
            continue;
        }
        queue_put(i);
    }
}

static drvError_t queue_destroy_res(struct queue_manages *que_manage, unsigned int qid)
{
    atomic_value_set_invalid(&que_manage->tail_status);
    if (is_queue_empty(que_manage) == false) {
        drvError_t ret = de_queue_inner(que_manage, qid);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("queue free Mbuf failed. (qid=%u; ret=%d)\n", que_manage->id, (int)ret);
            return DRV_ERROR_INNER_ERR;
        }
    }

    que_manage->creator_pid = 0;
    if (queue_is_merge_idx_valid(que_manage->merge_idx)) {
        (void)queue_merge_uninit(que_manage, qid);
    }

    return DRV_ERROR_NONE;
}

static drvError_t queue_destroy_local(unsigned int dev_id, unsigned int qid)
{
    struct queue_manages *que_manage = NULL;
    drvError_t ret;

    if (!queue_is_init()) {
        QUEUE_LOG_ERR("queue not init. (qid=%u)\n", qid);
        return DRV_ERROR_UNINIT;
    }

    que_manage = (struct queue_manages *)queue_get_que_addr(qid);
    if (que_manage->valid != QUEUE_CREATED) {
        return DRV_ERROR_NONE;
    }

    if (que_manage->inter_dev_state == QUEUE_STATE_IMPORTED) {
        QUEUE_LOG_ERR("queue inter dev state err. (qid=%u)\n", qid);
        return DRV_ERROR_PARA_ERROR;
    }

    queue_wake_up_wait_event(que_manage, QUEUE_IS_DESTROY_MAGIC);

    if (CAS(&que_manage->valid, QUEUE_CREATED, QUEUE_DESTROYING) == false) {
        QUEUE_LOG_INFO("repeated destroy. (dev_id=%u; queue_id=%u)\n", dev_id, qid);
        return DRV_ERROR_NONE;
    }

    if (queue_local_is_attached_and_set_detached(qid) == false) {
        QUEUE_LOG_ERR("repeated detach. (dev_id=%u, qid=%u)\n", dev_id, qid);
        que_manage->valid = QUEUE_CREATED;
        return DRV_ERROR_INNER_ERR;
    }

    ret = queue_destroy_res(que_manage, qid);
    if (ret != DRV_ERROR_NONE) {
        (void)queue_local_is_detached_and_se_attached(qid);
        que_manage->valid = QUEUE_CREATED;
        return ret;
    }

    que_manage->valid = QUEUE_UNCREATED;
    queue_put(qid);
    QUEUE_RUN_LOG_INFO("destroy success. (dev_id=%u; queue_id=%u)\n", dev_id, qid);

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_reset_res(struct queue_manages *que_manage, unsigned int dev_id, unsigned int qid)
{
    drvError_t ret;

    if (is_queue_empty(que_manage) == false) {
        ret = de_queue_inner(que_manage, qid);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("queue free mbuf failed. (dev_id=%u; qid=%u; ret=%d)\n", dev_id, que_manage->id, (int)ret);
            return DRV_ERROR_INNER_ERR;
        }
    }

    return DRV_ERROR_NONE;
}

drvError_t queue_reset_local(unsigned int dev_id, unsigned int qid)
{
    struct queue_manages *que_manage = NULL;
    drvError_t ret;

    if (queue_device_invalid(dev_id) || (qid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("para is error. (dev_id=%u; qid=%u)\n", dev_id, qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!queue_is_init()) {
        QUEUE_LOG_ERR("queue not init. (dev_id=%u; qid=%u)\n", dev_id, qid);
        return DRV_ERROR_UNINIT;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (dev_id=%u; qid=%u)\n", dev_id, qid);
        return DRV_ERROR_NOT_EXIST;
    }

    que_manage = (struct queue_manages *)queue_get_que_addr(qid);
    if (que_manage->valid != QUEUE_CREATED) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue is not created. (dev_id=%u; qid=%u; valid=%d)\n", dev_id, qid, que_manage->valid);
        return DRV_ERROR_NOT_EXIST;
    }

#ifndef EMU_ST
    if (CAS(&que_manage->enque_cas, 0, 1) == false) {
        queue_put(qid);
        QUEUE_LOG_WARN("queue is try to enque. (dev_id=%u; qid=%u)\n", dev_id, qid);
        return DRV_ERROR_BUSY;
    }
    if (CAS(&que_manage->deque_cas, 0, 1) == false) {
        (void)CAS(&que_manage->enque_cas, 1, 0);
        queue_put(qid);
        QUEUE_LOG_WARN("queue is try to deque. (dev_id=%u; qid=%u)\n", dev_id, qid);
        return DRV_ERROR_BUSY;
    }
#endif
    queue_wake_up_wait_event(que_manage, QUEUE_IS_CLEAR_MAGIC);

    ret = queue_reset_res(que_manage, dev_id, qid);
    if (ret == DRV_ERROR_NONE) {
        que_manage->empty_flag = 1;
        ATOMIC_SET(&que_manage->full_flag, 0);
        QUEUE_RUN_LOG_INFO("queue buff reset success. (dev_id=%u; qid=%u)\n", dev_id, qid);
    }
    if (CAS(&que_manage->enque_cas, 1, 0) == false) {
        QUEUE_LOG_ERR("enque write cas failed. (dev_id=%u; qid=%u; enque_cas=%d)\n", dev_id, qid,
                      que_manage->enque_cas);
    }
    if (CAS(&que_manage->deque_cas, 1, 0) == false) {
        QUEUE_LOG_ERR("enque write cas failed. (dev_id=%u; qid=%u; enque_cas=%d)\n", dev_id, qid,
                      que_manage->enque_cas);
    }
    queue_put(qid);
    return ret;
}

drvError_t queue_query_alive(unsigned int devid, unsigned int qid)
{
    struct queue_manages *que_manage = NULL;
    drvError_t ret;

    ret = get_queue_manage_by_qid(devid, qid, &que_manage);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (!queue_get(qid)) {
        return DRV_ERROR_NOT_EXIST;
    }

    if (que_manage->valid != QUEUE_CREATED) {
        queue_put(qid);
        return DRV_ERROR_NOT_EXIST;
    }
    queue_put(qid);
    return DRV_ERROR_NONE;
}

static drvError_t submit_queue_event(unsigned int devid, unsigned int qid, struct sub_info *sub_event)
{
    struct event_summary event = {0};
    struct QueueEventMsg msg;
    unsigned int submit_devid = devid;

    /* No one subscribes and does not send events */
    if ((sub_event->pid == 0) || (sub_event->eventid == 0)) {
        return DRV_ERROR_NO_PROCESS;
    }

    msg.src_location = sub_event->src_location;
    msg.src_udevid = sub_event->src_udevid;

    event.pid = sub_event->pid;
    event.dst_engine = sub_event->dst_engine;
    event.subevent_id = qid;
    event.event_id = (EVENT_ID)sub_event->eventid;
    event.grp_id = sub_event->groupid;
    event.msg = (char *)&msg;
    event.msg_len = sizeof(msg);

    submit_devid = queue_get_submit_devid_platform(submit_devid, event.dst_engine, sub_event->dst_devid);

    if (sub_event->spec_thread == true) {
        event.tid = sub_event->tid;
    } else {
        event.tid = QUEUE_INVALID_VALUE;
    }

    return halEschedSubmitEventEx(submit_devid, sub_event->dst_devid, &event);
}

#define MAX_DEQUEUE_TIME 0xffffffffffffffffULL
STATIC bool is_queue_de_queue_time_out(struct group_merge *merge)
{
    unsigned long long last_dequeue_time;
    unsigned long long diff_time = 0;
    unsigned long long cur_time;

    cur_time = buff_get_cur_timestamp();
    last_dequeue_time = merge->last_dequeue_time;
    if (cur_time > last_dequeue_time) {
        diff_time = cur_time - last_dequeue_time;
    }

    if (diff_time > ENQUE_EVENT_TIMEOUT) {
        merge->last_dequeue_time = MAX_DEQUEUE_TIME;
        return true;
    }
    return false;
}

static drvError_t queue_group_event_fail(struct queue_manages *manage, struct group_merge *merge, drvError_t result)
{
    if (result == DRV_ERROR_NO_PROCESS) {
        ATOMIC_INC(&manage->stat.enque_none_subscrib);
        return DRV_ERROR_NONE;
    }

    ATOMIC_INC(&merge->event_stat.user_event_fail);
    ATOMIC_SET(&merge->enque_event_ret, (unsigned int)result);
    ATOMIC_INC(&manage->stat.enque_event_fail);
    ATOMIC_SET(&manage->stat.enque_event_ret, (unsigned int)result);

    QUEUE_LOG_ERR("send enqueue event failed. (device_id=%u; queue_id=%u; pid=%d; spec_thread=%d; tid=%u; ret=%d)\n",
                  manage->dev_id, manage->id, manage->consumer.pid, manage->consumer.spec_thread, manage->consumer.tid,
                  result);

    return result;
}

static void que_sub_info_order_assign(struct sub_info *sub, struct sub_info *event)
{
    event->pid = sub->pid;
    /* QueueSubscribe and SendQueueEvent may be concurrent.
     * Ensure other subscription information is consistent with the validity of the pid.
     */
    wmb();

    event->src_location = sub->src_location;
    event->src_udevid = sub->src_udevid;
    event->dst_engine = sub->dst_engine;
    event->groupid = sub->groupid;
    event->spec_thread = sub->spec_thread;
    event->tid = sub->tid;
    event->dst_devid = sub->dst_devid;
    event->sub_send = sub->sub_send;
    /* QueueUnsubscribe and SendQueueEvent may be concurrent.
     * Ensure other subscription information is consistent with the validity of the eventid.
     */
    wmb();

    event->eventid = sub->eventid;
}

static drvError_t send_group_queue_event(unsigned int devid, struct queue_manages *manage, int add_size)
{
    (void)add_size;
    struct group_merge *merge = NULL;
    int merge_idx = manage->merge_idx;
    unsigned int qid = queue_get_virtual_qid(manage->id, LOCAL_QUEUE);

    if (queue_is_merge_idx_valid(merge_idx) == false) {
        QUEUE_RUN_LOG_INFO("queue merge_idx is invalid. (queue_id=%u; bind_type=%d; merge_idx=%d)\n", manage->id,
                           manage->bind_type, merge_idx);
        return DRV_ERROR_INNER_ERR;
    }

    merge = queue_get_merge_addr(merge_idx);
    if (merge->last_dequeue_time == 0) {
        (void)CAS(&merge->last_dequeue_time, 0, buff_get_cur_timestamp());
    }
    if (merge->pause_flag == 0 && (CAS(&merge->atomic_flag, 0, 1) || is_queue_de_queue_time_out(merge))) {
        struct sub_info sub_event = {0};
        drvError_t ret;

        que_sub_info_order_assign(&manage->consumer, &sub_event);
        ret = submit_queue_event(devid, qid, &sub_event);
        if (ret != DRV_ERROR_NONE) {
            (void)CAS(&merge->atomic_flag, 1, 0);
            return queue_group_event_fail(manage, merge, ret);
        }

        ATOMIC_INC(&merge->event_stat.user_event_succ);
        ATOMIC_SET(&merge->enque_event_ret, DRV_ERROR_NONE);
        ATOMIC_INC(&manage->stat.enque_event_ok);
        ATOMIC_SET(&manage->stat.enque_event_ret, DRV_ERROR_NONE);
    }

    return DRV_ERROR_NONE;
}

static drvError_t queue_single_event_fail(struct queue_manages *manage, drvError_t result)
{
    if (result == DRV_ERROR_NO_PROCESS) {
        ATOMIC_INC(&manage->stat.enque_none_subscrib);
        return DRV_ERROR_NONE;
    }

    ATOMIC_INC(&manage->stat.enque_event_fail);
    ATOMIC_SET(&manage->stat.enque_event_ret, (unsigned int)result);

    QUEUE_LOG_ERR("send queue event failed. "
                  "(dev_id=%u; queue_id=%u; mode=%d; pid=%d; spec_thread=%d; tid=%u; ret=%d)\n",
                  manage->dev_id, manage->id, manage->work_mode, manage->consumer.pid, manage->consumer.spec_thread,
                  manage->consumer.tid, result);

    return result;
}

static drvError_t send_single_queue_event(unsigned int devid, struct queue_manages *manage)
{
    int mode = (manage->work_mode == 0) ? QUEUE_MODE_PUSH : manage->work_mode; /* Not set, default is push. */
    struct sub_info sub_event = {0};
    unsigned int qid = queue_get_virtual_qid(manage->id, LOCAL_QUEUE);
    drvError_t ret;

    if ((mode != (int)QUEUE_MODE_PULL) && (CAS(&manage->event_flag, 0, 1))) {
        que_sub_info_order_assign(&manage->consumer, &sub_event);
        ret = submit_queue_event(devid, qid, &sub_event);
        if (ret != DRV_ERROR_NONE) {
            CAS(&manage->event_flag, 1, 0);
            return queue_single_event_fail(manage, ret);
        }

        ATOMIC_INC(&manage->stat.enque_event_ok);
        ATOMIC_SET(&manage->stat.enque_event_ret, DRV_ERROR_NONE);
    } else if ((mode == (int)QUEUE_MODE_PULL) && (CAS(&manage->empty_flag, 1, 0))) {
        que_sub_info_order_assign(&manage->consumer, &sub_event);
        ret = submit_queue_event(devid, qid, &sub_event);
        if (ret != DRV_ERROR_NONE) {
            CAS(&manage->empty_flag, 0, 1);
            return queue_single_event_fail(manage, ret);
        }

        ATOMIC_INC(&manage->stat.enque_event_ok);
        ATOMIC_SET(&manage->stat.enque_event_ret, DRV_ERROR_NONE);
    }

    return DRV_ERROR_NONE;
}

void send_queue_event(unsigned int dev_id, struct queue_manages *manage)
{
    int type = manage->bind_type;

    if (manage->consumer.pid == 0) {
        ATOMIC_INC(&manage->stat.enque_none_subscrib);
        return;
    }

    if (type == (int)QUEUE_TYPE_GROUP) {
        (void)send_group_queue_event(dev_id, manage, 1);
    } else {
        (void)send_single_queue_event(dev_id, manage);
    }

    return;
}

static drvError_t over_write_queue(struct queue_manages *que_manage, unsigned int qid)
{
    unsigned int head, depth, size;
    union atomic_queue_head cur_head, new_head;
    queue_entity_node *que_entity = NULL;
    void *buff = NULL;
    drvError_t ret;
    uint32_t blk_id;

    queue_get_entity_and_depth(qid, &que_entity, &depth);
    if (que_entity == NULL) {
        QUEUE_LOG_ERR("queue entity is NULL.(qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    cur_head.head_value = queue_get_head_value(que_manage);
    head = ((cur_head.head_info.head) % queue_get_local_depth(qid));
    size = get_queue_size(head, queue_get_orig_tail(que_manage), depth);
    if ((size < (depth - QUEUE_DEPT_RES_SIZE)) || (size == 0)) {
        return DRV_ERROR_NONE;
    }

    buff = que_entity[head].node;
    blk_id = que_entity[head].node_desp.blk_id;
    new_head.head_value = queue_head_inc(cur_head, depth);
    if (CAS(&que_manage->queue_head.head_value, cur_head.head_value, new_head.head_value) == true) {
        Mbuf *mbuf = NULL;
        ATOMIC_INC(&que_manage->stat.enque_drop);
        ret = create_priv_mbuf_for_queue(&mbuf, buff, blk_id);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("ow dequeue mbuf is illegal. (qid=%u, mbuf=%p)\n", qid, buff);
            return DRV_ERROR_INNER_ERR;
        }
        ret = mbuf_free_for_queue(mbuf, (int)MBUF_FREE_BY_QUEUE_OW);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("ow free mbuf failed. (qid=%u, mbuf=%p, ret=%d)\n", qid, buff, ret);
            return DRV_ERROR_INNER_ERR;
        }
    }

    return DRV_ERROR_NONE;
}

static drvError_t queue_enqueue(struct queue_manages *que_manage, unsigned int qid, Mbuf *mbuf)
{
    void *buff = (void *)get_share_mbuf_by_mbuf(mbuf);
    unsigned int depth, tail;
    queue_entity_node *que_entity = NULL;

    queue_get_entity_and_depth(qid, &que_entity, &depth);
    if (que_entity == NULL) {
        QUEUE_LOG_ERR("queue entity is NULL.(qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }
    mbuf_owner_update_for_enque(mbuf, g_cur_pid, qid);

    /* ATOMIC_SET ensure that the node content has been updated before read */
    tail = queue_get_tail(que_manage, qid);
    (void)ATOMIC_SET((&que_entity[tail].node), buff);
    (void)ATOMIC_SET((&que_entity[tail].node_desp.blk_id), mbuf->blk_id);
    wmb(); // ensure that the node content has been updated before set tail
    if (queue_enque_set_tail(que_manage, depth, qid) == false) {
        return DRV_ERROR_QUEUE_NOT_CREATED;
    }

    destroy_priv_mbuf_for_queue(mbuf);
    ATOMIC_INC(&que_manage->stat.enque_ok);
    que_get_time(&que_manage->stat.last_enque_time);

    return DRV_ERROR_NONE;
}

static inline drvError_t queue_buff_verify(void *buff)
{
    if (buff == NULL) {
        QUEUE_LOG_ERR("buff is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

static inline drvError_t queue_enqueue_para_check(unsigned int dev_id, unsigned int qid, void *buff)
{
    if (queue_device_invalid(dev_id) || (qid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("para is error. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (queue_buff_verify(buff) != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("enqueue mbuf is illegal. (qid=%u, mbuf=%p)\n", qid, buff);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t queue_enqueue_local(unsigned int dev_id, unsigned int qid, void *mbuf)
{
    struct queue_manages *que_manage = NULL;
    enque_timestamps enque_timestamp = {0};
    drvError_t ret;

    enque_timestamp.total_start_timestamp = buff_get_cur_timestamp();

    ret = queue_enqueue_para_check(dev_id, qid, mbuf);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    que_manage = queue_get_local_mng(qid);
    if (que_manage == NULL) {
        queue_put(qid);
        QUEUE_LOG_ERR("Queue local manage is null. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if (que_manage->valid != QUEUE_CREATED) {
        queue_put(qid);
        QUEUE_LOG_ERR(" queue(%u) is not created.\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if ((que_manage->inter_dev_state == QUEUE_STATE_UNEXPORTED) ||
        (que_manage->inter_dev_state == QUEUE_STATE_UNIMPORTED)) {
        queue_put(qid);
        return DRV_ERROR_PARA_ERROR;
    }

    ATOMIC_INC(&que_manage->stat.enque_fail);

    if (CAS(&que_manage->enque_cas, 0, 1) == false) {
        queue_put(qid);
        QUEUE_LOG_WARN("queue is try to enque multiple times. (qid=%u)\n", qid);
        return DRV_ERROR_BUSY;
    }

    if ((que_manage->over_write != 0) &&
        (get_que_size_by_mng(que_manage, qid) >= (queue_get_local_depth(qid) - QUEUE_DEPT_RES_SIZE))) {
        enque_timestamp.overwrite_start_timstamp = buff_get_cur_timestamp();
        ret = over_write_queue(que_manage, qid);
        if (ret != DRV_ERROR_NONE) {
            goto ERR;
        }
        enque_timestamp.overwrite_end_timestamp = buff_get_cur_timestamp();
    } else if (is_queue_full(que_manage, qid)) {
        // When the queue is full, attempt to send an enqueue event to solve the problem of being unable to enqueue or
        // trigger enqueue events after the queue is full.
        send_queue_event(dev_id, que_manage);
        ATOMIC_INC(&que_manage->stat.enque_full);
        ATOMIC_SET(&que_manage->full_flag, 1);
        ret = DRV_ERROR_QUEUE_FULL;
        goto ERR;
    }

    ret = queue_enqueue(que_manage, qid, (Mbuf *)mbuf);
    if (ret != DRV_ERROR_NONE) {
        goto ERR;
    }
    /* Make sure to write the tail first and then read empty_flag,
     * otherwise concurrent enqueue and dequeue will cause E2NE event to be lost.
     */
    mb();
    ATOMIC_DEC(&que_manage->stat.enque_fail);

    send_queue_event(dev_id, que_manage);

    if (is_queue_full(que_manage, qid)) {
        ATOMIC_SET(&que_manage->full_flag, 1);
    }

ERR:
    enque_timestamp.total_end_timestamp = buff_get_cur_timestamp();

    if (CAS(&que_manage->enque_cas, 1, 0) == false) {
        QUEUE_LOG_ERR("enque write cas failed. (qid=%u; enque_cas=%d)\n", qid, que_manage->enque_cas);
    }
    queue_put(qid);
    if (enque_timestamp.total_end_timestamp - enque_timestamp.total_start_timestamp > MAX_ENQUE_TIME) {
        QUEUE_LOG_INFO("enqueue start:(%llu), enque end:(%llu), over_write start:(%llu), over_write end:(%llu), "
                       "event start:(%llu), event end:(%llu), enter kernel:(%llu), kernel handle start:(%llu), "
                       "kernel handle end:(%llu),enqueue total:(%lluns), over_write total:(%lluns), "
                       "event total:(%lluns),kernel total:(%lluns).\n",
                       enque_timestamp.total_start_timestamp, enque_timestamp.total_end_timestamp,
                       enque_timestamp.overwrite_start_timstamp, enque_timestamp.overwrite_end_timestamp,
                       enque_timestamp.submit_start_timestamp, enque_timestamp.submit_end_timestamp,
                       enque_timestamp.kern_start_timestamp, enque_timestamp.kern_submit_timstamp,
                       enque_timestamp.kern_end_timestamp,
                       enque_timestamp.total_end_timestamp - enque_timestamp.total_start_timestamp,
                       enque_timestamp.overwrite_end_timestamp - enque_timestamp.overwrite_start_timstamp,
                       enque_timestamp.submit_end_timestamp - enque_timestamp.submit_start_timestamp,
                       enque_timestamp.kern_end_timestamp - enque_timestamp.kern_start_timestamp);
    }
    return ret;
}

void send_f_to_nf_event(unsigned int dev_id, struct queue_manages *que_manage)
{
    struct sub_info sub_event = {0};
    drvError_t ret;
    unsigned int qid = queue_get_virtual_qid(que_manage->id, LOCAL_QUEUE);

    if (que_manage->producer.inner_sub_flag == QUEUE_INTER_SUB_FLAG) {
        qid = que_manage->remote_qid;
    }
    ATOMIC_SET(&que_manage->full_flag, 0);
    que_sub_info_order_assign(&que_manage->producer, &sub_event);
    ret = submit_queue_event(dev_id, qid, &sub_event);
    if (ret != DRV_ERROR_NONE) {
        if (ret == DRV_ERROR_NO_PROCESS) {
            return;
        }
        ATOMIC_SET(&que_manage->full_flag, 1);
        ATOMIC_INC(&que_manage->stat.f2nf_event_fail);
        if (ret == DRV_ERROR_NO_SUBSCRIBE_THREAD) {
            return;
        }
        QUEUE_LOG_ERR("send device queue f2nf event failed. (dev_id=%u; qid=%u; ret=%d)\n", dev_id, qid, ret);
    } else {
        ATOMIC_INC(&que_manage->stat.f2nf_event_ok);
    }

    return;
}

static void sub_queue_send_event(unsigned int dev_id, struct queue_manages *manage)
{
    if (manage->queue_head.head_info.head != queue_get_orig_tail(manage)) {
        send_queue_event(dev_id, manage);
    }
}

static drvError_t dequeue_one_mbuf(struct queue_manages *que_manage, unsigned int qid, void **buff_inner,
                                   uint32_t *blk_id)
{
    union atomic_queue_head cur_head, new_head;
    unsigned int depth, head, cas_ret;
    queue_entity_node *que_entity = NULL;
    uint32_t blk_id_;
    void *buff = NULL;
    unsigned int cnt;

    queue_get_entity_and_depth(qid, &que_entity, &depth);
    if (que_entity == NULL) {
        QUEUE_LOG_ERR("queue entity is NULL.(qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    for (cnt = 0; cnt < MAX_DEQUEUE_TRY_TIMES; cnt++) {
        cur_head.head_value = queue_get_head_value(que_manage);
        head = ((cur_head.head_info.head) % queue_get_local_depth(qid));
        if (head == queue_get_orig_tail(que_manage)) {
            return DRV_ERROR_QUEUE_EMPTY;
        }
        /* It is possible to read the old value. If speculative execution occurs,
         * read the corresponding content of head first and then determine whether the queue is empty.
         * Therefore, rmb should be added to preserve order.
         */
        rmb();
        buff = que_entity[head].node;
        blk_id_ = que_entity[head].node_desp.blk_id;
        new_head.head_value = queue_head_inc(cur_head, depth);
        cas_ret = CAS(&que_manage->queue_head.head_value, cur_head.head_value, new_head.head_value);
        if (cas_ret == true) {
            break;
        }
    }

    if (cas_ret == false) {
        QUEUE_LOG_WARN("try dequeue again. (qid=%u; cnt=%u)\n", qid, cnt);
        return DRV_ERROR_BUSY;
    }
    ATOMIC_INC(&que_manage->stat.deque_num);
    *buff_inner = buff;
    *blk_id = blk_id_;

    return DRV_ERROR_NONE;
}

static drvError_t dequeue_by_flow_ctrl(struct queue_manages *que_manage, unsigned int qid, Mbuf **mbuf)
{
    unsigned long timestamp;
    Mbuf *mbuf_inner = NULL;
    void *buff_inner = NULL;
    unsigned int size;
    drvError_t ret;
    uint32_t blk_id;

    ret = dequeue_one_mbuf(que_manage, qid, &buff_inner, &blk_id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    /*
     * If the mbuf is illegal, the error code DRV_ERROR_NO_RESOURCES needs to be returned,
     * and the upper layer will perform selective processing based on the error code.
     */
    ret = create_priv_mbuf_for_queue(&mbuf_inner, buff_inner, blk_id);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("dequeue mbuf is illegal. (qid=%u; mbuf=0x%llx)\n", qid,
                      (unsigned long long)(uintptr_t)buff_inner);
        return ret;
    }

    size = get_que_size_by_mng(que_manage, qid);
    timestamp = buff_get_cur_timestamp();

    if ((que_manage->fctl_flag != 0)) {
        unsigned int cnt = 0;
        while ((!is_queue_empty(que_manage)) && (cnt < size) &&
               (tick_to_millisec(timestamp - (unsigned long)mbuf_get_timestamp(mbuf_inner)) >= que_manage->drop_time)) {
            ret = mbuf_free_for_queue(mbuf_inner, MBUF_FREE_BY_QUEUE_DQ);
            if (ret != DRV_ERROR_NONE) {
                QUEUE_LOG_ERR("free mbuff failed. (qid=%u; mbuf=%p; ret=%d)\n", que_manage->id, buff_inner, ret);
                return DRV_ERROR_INNER_ERR;
            }

            ATOMIC_INC(&que_manage->stat.deque_drop);
            ret = dequeue_one_mbuf(que_manage, qid, &buff_inner, &blk_id);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
            /*
             * If the mbuf is illegal, the error code DRV_ERROR_NO_RESOURCES needs to be returned,
             * and the upper layer will perform selective processing based on the error code.
             */
            ret = create_priv_mbuf_for_queue(&mbuf_inner, buff_inner, blk_id);
            if (ret != DRV_ERROR_NONE) {
                QUEUE_LOG_ERR("dequeue mbuf is illegal. (qid=%u; mbuf=%p)\n", que_manage->id, buff_inner);
#ifndef EMU_ST
                return ret;
#endif
            }

            cnt++;
        }
    }

    *mbuf = mbuf_inner;
    ATOMIC_INC(&que_manage->stat.deque_ok);
    que_get_time(&que_manage->stat.last_deque_time);

    return DRV_ERROR_NONE;
}

static inline drvError_t queue_dequeue_para_check(unsigned int dev_id, unsigned int qid, void **mbuf)
{
    if (queue_device_invalid(dev_id) || (qid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("para is error. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf == NULL) {
        QUEUE_LOG_ERR("mbuf is NULL. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t queue_dequeue_local(unsigned int dev_id, unsigned int qid, void **mbuf)
{
    struct queue_manages *que_manage = NULL;
    static THREAD pid_t g_deque_pid = 0;
    struct group_merge *merge = NULL;
    Mbuf *p_mbuf = NULL;
    drvError_t ret;
    int merge_idx;

    ret = queue_dequeue_para_check(dev_id, qid, mbuf);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    que_manage = queue_get_local_mng(qid);
    if (que_manage == NULL) {
        queue_put(qid);
        QUEUE_LOG_ERR("Queue local manage is null. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if (que_manage->valid != QUEUE_CREATED) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue is not created. (qid=%u; valid=%d; QUEUE_CREATED=%d)\n", qid, que_manage->valid,
                      QUEUE_CREATED);
        return DRV_ERROR_NOT_EXIST;
    }

    ATOMIC_INC(&que_manage->stat.deque_fail);

    if (is_queue_empty(que_manage)) {
        /*
         * Enqueuing may occur before the empty_flag is set to 1.
         * Here, add an attempt to send an E2NE event.
         */
        que_manage->empty_flag = 1;
        /*
         * Make sure to set the empty_flag first and then read tail,
         * otherwise concurrent enqueue and dequeue will cause E2NE event to be lost.
         */
        mb();
        sub_queue_send_event(dev_id, que_manage);
        ATOMIC_INC(&que_manage->stat.deque_empty);
        queue_put(qid);
        return DRV_ERROR_QUEUE_EMPTY;
    }

    if (CAS(&que_manage->deque_cas, 0, 1) == false) {
        queue_put(qid);
        QUEUE_LOG_WARN("queue is try to deque multiple times. (qid=%u)\n", qid);
        return DRV_ERROR_BUSY;
    }

    ret = dequeue_by_flow_ctrl(que_manage, qid, &p_mbuf);
    (void)CAS(&que_manage->deque_cas, 1, 0);
    if (ret == DRV_ERROR_QUEUE_EMPTY) {
        /* Enqueuing may occur before the empty_flag is set to 1.
         * Here, add an attempt to send an E2NE event.
         */
        que_manage->empty_flag = 1;
        /* Make sure to set the empty_flag first and then read tail,
         * otherwise concurrent enqueue and dequeue will cause E2NE event to be lost.
         */
        mb();
        sub_queue_send_event(dev_id, que_manage);
        ATOMIC_INC(&que_manage->stat.deque_empty);
        queue_put(qid);
        return ret;
    } else if (ret != DRV_ERROR_NONE) {
        queue_put(qid);
        return ret;
    }

    ATOMIC_DEC(&que_manage->stat.deque_fail);
    if (que_manage->bind_type == QUEUE_TYPE_GROUP) {
        merge_idx = que_manage->merge_idx;
        if (queue_is_merge_idx_valid(merge_idx)) {
            merge = queue_get_merge_addr(merge_idx);
            merge->last_dequeue_time = buff_get_cur_timestamp();
        }
    }

    if (g_deque_pid == 0) {
        g_deque_pid = GETPID();
    }

    mbuf_owner_update_for_deque(p_mbuf, g_deque_pid, qid);
    *mbuf = p_mbuf;

    if (que_manage->full_flag != 0) {
        send_f_to_nf_event(dev_id, que_manage);
    }
    queue_put(qid);

    return DRV_ERROR_NONE;
}

drvError_t check_subscribe_para(unsigned int dev_id, unsigned int qid, int type)
{
    struct queue_manages *manage = NULL;
    drvError_t ret;

    ret = get_queue_manage_by_qid(dev_id, qid, &manage);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("para invalid. (qid=%u; type=%d)\n", qid, type);
        return ret;
    }

    if (manage->valid != QUEUE_CREATED) {
        QUEUE_LOG_ERR("queue is not created. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if (manage->consumer.pid != 0) {
        QUEUE_LOG_ERR("Queue has been subscribed. (qid=%u; pid=%d; spec_thread=%d; tid=%u)\n", manage->id,
                      manage->consumer.pid, manage->consumer.spec_thread, manage->consumer.tid);
        return DRV_ERROR_REPEATED_SUBSCRIBED;
    }

    if ((type > QUEUE_TYPE_SINGLE) || ((manage->work_mode == QUEUE_MODE_PULL) && (type == QUEUE_TYPE_GROUP))) {
        QUEUE_LOG_ERR("para invalid. (qid=%u; work_mode=%d; type=%d)\n", qid, manage->work_mode, type);
        return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

static drvError_t queue_get_event_src(unsigned int dev_id, unsigned int dst_engine, unsigned int *src_location,
                                      unsigned int *src_udevid)
{
    drvError_t ret;
    int location;

    ret = queue_get_event_src_udevid_platform(dev_id, dst_engine, src_udevid, &location);
    *src_location = (unsigned int)location;

    return ret;
}

drvError_t subscribe_queue(unsigned int dev_id, unsigned int qid, struct sub_info sub_info, int type)
{
    struct queue_manages *manage = NULL;
    drvError_t ret;

    manage = queue_get_local_mng(qid);
    if (manage == NULL) {
        QUEUE_LOG_ERR("Queue local manage is null. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_NOT_EXIST;
    }

    switch (type) {
        case QUEUE_TYPE_GROUP:
            ret = queue_merge_init(manage, qid, sub_info.pid, sub_info.groupid);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
            break;
        case QUEUE_TYPE_SINGLE:
            break;
        default:
            QUEUE_LOG_ERR("subscribe queue type is not valid. (qid=%u, type=%d)\n", manage->id, type);
            return DRV_ERROR_PARA_ERROR;
    }

    ret = queue_register_callback(sub_info.groupid);
    if (ret != DRV_ERROR_NONE) {
        if (type == QUEUE_TYPE_GROUP) {
            (void)queue_merge_uninit(manage, qid);
        }
        QUEUE_LOG_ERR("register callback failed. (dev_id=%u; queue_id=%u; type=%d)\n", dev_id, manage->id, type);
        return ret;
    }

    ret = queue_get_event_src(dev_id, sub_info.dst_engine, &manage->consumer.src_location,
                              &manage->consumer.src_udevid);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("get event src failed. (devid=%u, qid=%u)\n", dev_id, qid);
        return ret;
    }

    manage->bind_type = type;
    manage->consumer.dst_engine = sub_info.dst_engine;
    manage->consumer.eventid = sub_info.eventid;
    manage->consumer.groupid = sub_info.groupid;
    manage->consumer.sub_send = sub_info.sub_send;
    manage->consumer.spec_thread = sub_info.spec_thread;
    manage->consumer.tid = sub_info.tid;
    manage->consumer.dst_devid = sub_info.dst_devid;
    manage->consumer.inner_sub_flag = sub_info.inner_sub_flag;
    /* halQueueSubscribe and SendSingleQueueEvent(halQueueEnQueue) may be concurrent.
     * The enqueue event can be sent only after all structure variables are assigned values.
     */
    wmb();

    manage->consumer.pid = sub_info.pid;

    if (manage->consumer.sub_send == 1) {
        sub_queue_send_event(dev_id, manage);
    }

    QUEUE_LOG_INFO("subscribe queue success. (queue=%u; pid=%d; spec_thread=%d; tid=%u;"
                   " group=%u; event_id=%u; type=%d; event_flag=%d;"
                   " enque_ok=0x%llx; deque_ok=0x%llx;"
                   " enque_event_ok=0x%llx; enque_event_fail=0x%llx; enque_none_subscrib=0x%lx;"
                   " head=%u; tail=%u)\n",
                   manage->id, manage->consumer.pid, manage->consumer.spec_thread, manage->consumer.tid,
                   manage->consumer.groupid, manage->consumer.eventid, type, manage->event_flag, manage->stat.enque_ok,
                   manage->stat.deque_ok, manage->stat.enque_event_ok, manage->stat.enque_event_fail,
                   manage->stat.enque_none_subscrib, manage->queue_head.head_info.head, queue_get_orig_tail(manage));

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_subscribe_local(unsigned int dev_id, unsigned int qid, unsigned int group_id, int type)
{
    struct QueueSubPara sub_para = {0};

    sub_para.devId = dev_id;
    sub_para.qid = qid;
    sub_para.groupId = group_id;
    sub_para.eventType = QUEUE_ENQUE_EVENT;
    sub_para.queType = type;
    sub_para.flag = 0;

    return queue_sub_event_local(&sub_para);
}

STATIC drvError_t queue_unsubscribe_local(unsigned int dev_id, unsigned int qid)
{
    struct QueueUnsubPara unsub_para = {0};

    unsub_para.devId = dev_id;
    unsub_para.qid = qid;
    unsub_para.eventType = QUEUE_ENQUE_EVENT;

    return queue_unsub_event_local(&unsub_para);
}

drvError_t sub_f_to_nf_event(unsigned int dev_id, unsigned int qid, struct sub_info sub_event)
{
    static struct timeval last_log_time = {0};
    struct timeval current_time;
    struct queue_manages *manage = NULL;
    drvError_t ret;

    ret = get_queue_manage_by_qid(dev_id, qid, &manage);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("para qid(%u) invalid.\n", qid);
        return ret;
    }

    if (manage->valid != QUEUE_CREATED) {
        QUEUE_LOG_ERR("queue(%u) is not created.\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if ((manage->inter_dev_state == QUEUE_STATE_UNEXPORTED) || (manage->inter_dev_state == QUEUE_STATE_UNIMPORTED)) {
        QUEUE_LOG_INFO("queue(%u) has been unexported or unimported. (state=%d)\n", qid, manage->inter_dev_state);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (manage->producer.pid != 0) {
        QUEUE_LOG_ERR("queue(%u) has been subscribed.\n", qid);
        return DRV_ERROR_REPEATED_SUBSCRIBED;
    }

    ret = queue_get_event_src(dev_id, sub_event.dst_engine, &manage->producer.src_location,
                              &manage->producer.src_udevid);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("get event src failed. (devid=%u, qid=%u)\n", dev_id, qid);
        return ret;
    }

    manage->producer.dst_engine = sub_event.dst_engine;
    manage->producer.eventid = sub_event.eventid;
    manage->producer.groupid = sub_event.groupid;
    manage->producer.sub_send = sub_event.sub_send;
    manage->producer.spec_thread = sub_event.spec_thread;
    manage->producer.tid = sub_event.tid;
    manage->producer.dst_devid = sub_event.dst_devid;
    manage->producer.inner_sub_flag = sub_event.inner_sub_flag;
    /* halQueueSubF2NFEvent and SendF2NFEvent(halQueueDeQueue) may be concurrent.
     * The f2nf event can be sent only after all structure variables are assigned values.
     */
    wmb();

    manage->producer.pid = sub_event.pid;

    if (manage->producer.sub_send == 1 && manage->full_flag == 0) {
        send_f_to_nf_event(dev_id, manage);
    }

    que_get_time(&current_time);
    if ((current_time.tv_sec - last_log_time.tv_sec) > LOG_TIME_INTERVAL) {
        last_log_time = current_time;
        QUEUE_RUN_LOG_INFO("sub_f_to_nf_event success. (dev_id=%d; queue=%u; pid=%d; spec_thread=%d; tid=%u; group=%u;"
                           "enqueue_full=%llu)\n",
                           dev_id, qid, manage->producer.pid, manage->consumer.spec_thread, manage->consumer.tid,
                           manage->producer.groupid, manage->stat.enque_full);
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_sub_f_to_nf_event_local(unsigned int dev_id, unsigned int qid, unsigned int group_id)
{
    struct QueueSubPara sub_para = {0};

    sub_para.devId = dev_id;
    sub_para.qid = qid;
    sub_para.groupId = group_id;
    sub_para.eventType = QUEUE_F2NF_EVENT;
    sub_para.flag = 0;

    return queue_sub_event_local(&sub_para);
}

STATIC drvError_t queue_unsub_f_to_nf_event_local(unsigned int dev_id, unsigned int qid)
{
    struct QueueUnsubPara unsub_para = {0};

    unsub_para.devId = dev_id;
    unsub_para.qid = qid;
    unsub_para.eventType = QUEUE_F2NF_EVENT;

    return queue_unsub_event_local(&unsub_para);
}

static drvError_t queue_sub_para_check(struct QueueSubPara *sub_para)
{
    struct queue_manages *manage = NULL;
    drvError_t ret;

    if (sub_para->eventType >= QUEUE_EVENT_TYPE_MAX) {
        QUEUE_LOG_ERR("event_type is invalid. (event_type=%d; event_typeMax=%d)\n", sub_para->eventType,
                      QUEUE_EVENT_TYPE_MAX);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = get_queue_manage_by_qid(sub_para->devId, sub_para->qid, &manage);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("para is invalid. (dev_id=%u; qid=%u)\n", sub_para->devId, sub_para->qid);
        return ret;
    }

    if (manage->valid != QUEUE_CREATED) {
        QUEUE_LOG_ERR("queue is not created. (devid=%u; qid=%u)\n", sub_para->devId, sub_para->qid);
        return DRV_ERROR_NOT_EXIST;
    }

    return DRV_ERROR_NONE;
}

unsigned int queue_get_dst_engine(unsigned int dev_id, unsigned int group_id)
{
    return queue_get_grp_dst_engine_platform(dev_id, group_id);
}

static void queue_set_sub_info(struct QueueSubPara *sub_para, unsigned int event_id, struct sub_info *sub_info)
{
    unsigned int dst_dev = ((sub_para->flag & QUEUE_SUB_FLAG_SPEC_DST_DEVID) != 0) ? sub_para->dstDevId :
                                                                                     sub_para->devId;

    sub_info->dst_engine = queue_get_dst_engine(dst_dev, sub_para->groupId);
    sub_info->eventid = event_id;
    sub_info->groupid = sub_para->groupId;
    sub_info->pid = GETPID();
    sub_info->spec_thread = ((sub_para->flag & QUEUE_SUB_FLAG_SPEC_THREAD) != 0) ? true : false;
    sub_info->tid = sub_para->threadId;
    sub_info->dst_devid = ((sub_para->flag & QUEUE_SUB_FLAG_SPEC_DST_DEVID) != 0) ? sub_para->dstDevId :
                                                                                    QUEUE_INVALID_VALUE;
    sub_info->sub_send = 1;
}

static drvError_t queue_sub_enque_event(struct QueueSubPara *sub_para)
{
    struct queue_manages *manage = queue_get_local_mng(sub_para->qid);
    struct sub_info sub_info = {0};
    int event_id;

    if (manage->consumer.pid != 0) {
        QUEUE_LOG_ERR("Queue has been subscribed. (queue_id=%u; pid=%d; spec_thread=%d; tid=%u)\n", sub_para->qid,
                      manage->consumer.pid, manage->consumer.spec_thread, manage->consumer.tid);
        return DRV_ERROR_REPEATED_SUBSCRIBED;
    }

    if ((sub_para->queType > QUEUE_TYPE_SINGLE) ||
        ((manage->work_mode == QUEUE_MODE_PULL) && (sub_para->queType == QUEUE_TYPE_GROUP))) {
        QUEUE_LOG_ERR("para invalid. (qid=%u; work_mode=%d; type=%d)\n", sub_para->qid, manage->work_mode,
                      sub_para->queType);
        return DRV_ERROR_PARA_ERROR;
    }
    event_id = (manage->work_mode == QUEUE_MODE_PULL) ? EVENT_QUEUE_EMPTY_TO_NOT_EMPTY : EVENT_QUEUE_ENQUEUE;
    queue_set_sub_info(sub_para, (unsigned int)event_id, &sub_info);

    return subscribe_queue(sub_para->devId, sub_para->qid, sub_info, sub_para->queType);
}

static drvError_t queue_sub_f2nf_event(struct QueueSubPara *sub_para)
{
    unsigned int event_id = (unsigned int)EVENT_QUEUE_FULL_TO_NOT_FULL;
    struct sub_info sub_info = {0};

    queue_set_sub_info(sub_para, event_id, &sub_info);

    return sub_f_to_nf_event(sub_para->devId, sub_para->qid, sub_info);
}

drvError_t queue_sub_event_local(struct QueueSubPara *sub_para)
{
    drvError_t ret;

    ret = queue_sub_para_check(sub_para);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("sub_para check failed.\n");
        return ret;
    }

    switch (sub_para->eventType) {
        case QUEUE_ENQUE_EVENT:
            return queue_sub_enque_event(sub_para);
        case QUEUE_F2NF_EVENT:
            return queue_sub_f2nf_event(sub_para);
        default:
            QUEUE_LOG_ERR("not support event type. (event_type=%u)\n", sub_para->eventType);
            return DRV_ERROR_NOT_SUPPORT;
    }
}

static drvError_t queue_unsub_para_check(struct QueueUnsubPara *unsub_para)
{
    struct queue_manages *manage = NULL;
    drvError_t ret;

    if ((unsub_para == NULL) || (unsub_para->eventType >= QUEUE_EVENT_TYPE_MAX)) {
        QUEUE_LOG_ERR("sub_para is NULL. (sub_para_is_null=%d)\n", unsub_para == NULL);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = get_queue_manage_by_qid(unsub_para->devId, unsub_para->qid, &manage);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_WARN("para is invalid. (dev_id=%u; qid=%u)\n", unsub_para->devId, unsub_para->qid);
        return ret;
    }

    if (manage->valid != QUEUE_CREATED) {
        QUEUE_LOG_WARN("queue is not created. (dev_id=%u; qid=%u)\n", unsub_para->devId, unsub_para->qid);
        return DRV_ERROR_NOT_EXIST;
    }

    return DRV_ERROR_NONE;
}

static drvError_t queue_unsub_enque_event(struct QueueUnsubPara *unsub_para)
{
    struct queue_manages *manage = NULL;
    drvError_t ret;

    manage = queue_get_local_mng(unsub_para->qid);
    if (manage->bind_type == QUEUE_TYPE_GROUP) {
        ret = queue_merge_uninit(manage, unsub_para->qid);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("delete queue merge flag failed. (qid=%u)\n", unsub_para->qid);
            return ret;
        }
    }

    manage->consumer.eventid = 0;
    /* halQueueUnsubscribe and SendSingleQueueEvent may be concurrent.
     * As a result, the event_flag may be fixed to 1.
     */
    wmb();

    manage->consumer.pid = 0;
    manage->consumer.groupid = 0;
    manage->consumer.dst_engine = 0;
    manage->consumer.sub_send = 0;
    manage->consumer.tid = 0;
    manage->consumer.spec_thread = false;
    manage->consumer.inner_sub_flag = 0;
    manage->bind_type = 0;
    manage->event_flag = 0;
    manage->empty_flag = 0;

    QUEUE_LOG_INFO("unsub success. (dev_id=%u; queue_id=%u)\n", unsub_para->devId, unsub_para->qid);

    return DRV_ERROR_NONE;
}

static drvError_t queue_unsub_f2nf_event(struct QueueUnsubPara *unsub_para)
{
    static struct timeval last_log_time = {0};
    struct timeval current_time;
    struct queue_manages *manage = NULL;
    manage = queue_get_local_mng(unsub_para->qid);

    manage->producer.eventid = 0;
    /* halQueueUnsubF2NFEvent and SendF2NFEvent(halQueueDeQueue) may be concurrent. */
    wmb();

    manage->producer.groupid = 0;
    manage->producer.pid = 0;
    manage->producer.dst_engine = 0;
    manage->producer.sub_send = 0;
    manage->producer.inner_sub_flag = 0;

    que_get_time(&current_time);
    if ((current_time.tv_sec - last_log_time.tv_sec) > LOG_TIME_INTERVAL) {
        last_log_time = current_time;
        QUEUE_RUN_LOG_INFO("unsub F2NF event success. (dev_id=%u; queue_id=%u; enque_full=%llu)\n", unsub_para->devId,
                           unsub_para->qid, manage->stat.enque_full);
    }

    return DRV_ERROR_NONE;
}

drvError_t queue_unsub_event_local(struct QueueUnsubPara *unsub_para)
{
    drvError_t ret;

    ret = queue_unsub_para_check(unsub_para);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_WARN("sub_para check invalid.\n");
        return ret;
    }

    switch (unsub_para->eventType) {
        case QUEUE_ENQUE_EVENT:
            return queue_unsub_enque_event(unsub_para);
        case QUEUE_F2NF_EVENT:
            return queue_unsub_f2nf_event(unsub_para);
        default:
            QUEUE_LOG_ERR("not support event type. (event_type=%u)\n", unsub_para->eventType);
            return DRV_ERROR_NOT_SUPPORT;
    }
}

STATIC drvError_t queue_ctrl_event_local(struct QueueSubscriber *subscriber, QUE_EVENT_CMD cmd_type)
{
    if (subscriber == NULL) {
        QUEUE_LOG_ERR("subscriber para is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (subscriber->devId >= MAX_DEVICE || (subscriber->spGrpId < 0 || subscriber->spGrpId >= MAX_SHARE_GRP) ||
        subscriber->pid <= 0) {
        QUEUE_LOG_ERR("devid sp_grp_id invalid. (dev_id=%u, sp_grp_id=%d)\n", subscriber->devId, subscriber->spGrpId);
        return DRV_ERROR_INVALID_VALUE;
    }

    return queue_merge_ctrl(subscriber->devId, subscriber->pid, (unsigned int)subscriber->groupId, cmd_type);
}

drvError_t queue_query_info_local(unsigned int dev_id, unsigned int qid, QueueInfo *que_info)
{
    struct queue_manages *que_manage = NULL;
    queue_entity_node *que_entity = NULL;
    unsigned int depth;
    drvError_t ret;

    ret = get_queue_manage_by_qid(dev_id, qid, &que_manage);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("get_queue_manage_by_qid failed. (qid=%u; ret=%d)\n", qid, ret);
        return ret;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if (que_manage->valid != QUEUE_CREATED) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue is not created. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    queue_get_entity_and_depth(qid, &que_entity, &depth);
    if (que_entity == NULL) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue entity is NULL.(qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    que_info->depth = (int)depth;
    que_info->headDataPtr = (void *)(is_queue_empty(que_manage) ? NULL :
                                                                  que_entity[queue_get_head(que_manage, qid)].node);

    que_info->id = (int)qid;
    que_info->size = (int)get_que_size_by_mng(que_manage, qid);
    que_info->type = que_manage->bind_type;
    que_info->workMode = (que_manage->work_mode == 0) ? QUEUE_MODE_PUSH : que_manage->work_mode;
    que_info->subGroupId = (int)que_manage->consumer.groupid;
    que_info->subPid = que_manage->consumer.pid;
    que_info->subF2NFPid = que_manage->producer.pid;
    que_info->subF2NFGroupId = (int)que_manage->producer.groupid;
    que_info->stat.dequeCnt = que_manage->stat.deque_ok;
    que_info->stat.enqueCnt = que_manage->stat.enque_ok;
    que_info->stat.enqueFailCnt = que_manage->stat.enque_fail;
    que_info->stat.dequeFailCnt = que_manage->stat.deque_fail;
    que_info->stat.enqueEventOk = que_manage->stat.enque_event_ok;
    que_info->stat.enqueEventFail = que_manage->stat.enque_event_fail;
    que_info->stat.f2nfEventOk = que_manage->stat.f2nf_event_ok;
    que_info->stat.f2nfEventFail = que_manage->stat.f2nf_event_fail;
    que_info->stat.lastEnqueTime = que_manage->stat.last_enque_time;
    que_info->stat.lastDequeTime = que_manage->stat.last_deque_time;
    que_info->entity_type = 0;

    if (is_queue_full(que_manage, qid)) {
        que_info->status = QUEUE_FULL;
    } else {
        que_info->status = (int)(is_queue_empty(que_manage) ? QUEUE_EMPTY : QUEUE_NORMAL);
    }

    ret = strncpy_s(que_info->name, MAX_STR_LEN, que_manage->name, strnlen(que_manage->name, MAX_STR_LEN));
    if (ret != DRV_ERROR_NONE) {
        queue_put(qid);
        QUEUE_LOG_ERR("strncpy queue name failed. (ret=%d)\n", ret);
        return DRV_ERROR_PARA_ERROR;
    }
    queue_put(qid);
    return DRV_ERROR_NONE;
}

static drvError_t queue_get_status_para_check(QUEUE_QUERY_ITEM query_item, unsigned int len, void *data)
{
    (void)data;
    switch (query_item) {
        case QUERY_QUEUE_DROP_STAT:
            if (len != sizeof(QUEUE_DROP_PKT_STAT)) {
                QUEUE_LOG_ERR("len invalid. (len=%u; expect=%zu)\n", len, sizeof(QUEUE_DROP_PKT_STAT));
                return DRV_ERROR_INVALID_VALUE;
            }
            break;
        case QUERY_QUEUE_STATUS:
            if (len != sizeof(QUEUE_STATUS)) {
                QUEUE_LOG_ERR("len invalid. (len=%u; expect=%zu)\n", len, sizeof(QUEUE_STATUS));
                return DRV_ERROR_INVALID_VALUE;
            }
            break;
        case QUERY_QUEUE_DEPTH:
            if (len != sizeof(int)) {
                QUEUE_LOG_ERR("len invalid. (len=%u; expect=%zu)\n", len, sizeof(int));
                return DRV_ERROR_INVALID_VALUE;
            }
            break;
        default:
            QUEUE_LOG_WARN("query_item not support. (query_item=%u)\n", (unsigned int)query_item);
            return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t queue_get_status_local(unsigned int dev_id, unsigned int qid, QUEUE_QUERY_ITEM query_item, unsigned int len,
                                  void *data)
{
    struct queue_manages *que_manage = NULL;
    drvError_t ret;

    ret = queue_get_status_para_check(query_item, len, data);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    ret = get_queue_manage_by_qid(dev_id, qid, &que_manage);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_WARN("can not get_queue_manage_by_qid. (dev_id=%u; qid=%u; ret=%d)\n", dev_id, qid, ret);
        return ret;
    }

    if (que_manage->valid != QUEUE_CREATED) {
        QUEUE_LOG_WARN("queue is not created.(qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    switch (query_item) {
        case QUERY_QUEUE_DROP_STAT:
            ((QUEUE_DROP_PKT_STAT *)data)->enqueDropCnt = que_manage->stat.enque_drop;
            ((QUEUE_DROP_PKT_STAT *)data)->dequeDropCnt = que_manage->stat.deque_drop;
            break;
        case QUERY_QUEUE_STATUS:
            if (is_queue_full(que_manage, qid)) {
                *((int *)data) = QUEUE_FULL;
            } else {
                *((int *)data) = (int)(is_queue_empty(que_manage) ? QUEUE_EMPTY : QUEUE_NORMAL);
            }
            break;
        case QUERY_QUEUE_DEPTH:
            *((unsigned int *)data) = queue_get_local_depth(qid);
            break;
        default:
            QUEUE_LOG_WARN("query_item not support now. (query_item=%u)\n", (unsigned int)query_item);
            return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_get_qid_by_name_local(unsigned int dev_id, const char *name, unsigned int *qid)
{
    struct queue_manages *que_manage = NULL;
    unsigned long len;
    unsigned int i;

    if ((queue_device_invalid(dev_id)) || (name == NULL) || (qid == NULL)) {
        QUEUE_LOG_ERR("para invalid. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    len = strnlen(name, MAX_STR_LEN);
    if ((len >= MAX_STR_LEN) || (len == 0)) {
        QUEUE_LOG_ERR("name len is err. (len=%lu; max len=%d)\n", len, MAX_STR_LEN);
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < MAX_SURPORT_QUEUE_NUM; i++) {
        que_manage = queue_get_local_mng(i);
        if (que_manage == NULL) {
            continue;
        }

        if (que_manage->valid == QUEUE_CREATED) {
            if (strncmp(name, que_manage->name, (MAX_STR_LEN - 1)) == 0 && que_manage->dev_id == dev_id) {
                *qid = que_manage->id;
                return DRV_ERROR_NONE;
            }
        }
    }

    QUEUE_LOG_INFO("device can not find queue named. (dev_id=%u; name=%s)\n", dev_id, name);
    return DRV_ERROR_NOT_EXIST;
}

STATIC drvError_t queue_get_qidsby_pid_local(unsigned int dev_id, unsigned int pid, unsigned int max_que_size,
                                             QidsOfPid *info)
{
    struct queue_manages *que_manage = NULL;
    unsigned int *qids = NULL;
    uint32 index = 0;
    unsigned int i;

    if (queue_device_invalid(dev_id) || (pid == 0) || (max_que_size == 0) || (info == NULL)) {
        QUEUE_LOG_ERR("devid,pid or max_que_size is invalid. (dev_id=%u; pid=%u; max_que_size=%u)\n", dev_id, pid,
                      max_que_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    qids = info->qids;
    if (qids == NULL) {
        QUEUE_LOG_ERR("qids is invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < MAX_SURPORT_QUEUE_NUM; i++) {
        que_manage = queue_get_local_mng(i);
        if (que_manage == NULL) {
            continue;
        }

        if ((que_manage->valid == QUEUE_CREATED) && ((unsigned int)que_manage->creator_pid == pid) &&
            (que_manage->dev_id == dev_id)) {
            if (index >= max_que_size) {
                QUEUE_LOG_ERR("max_que_size is not enough. (index=%d; max_que_size=%u)\n", index, max_que_size);
                return DRV_ERROR_OVER_LIMIT;
            }
            qids[index] = que_manage->id;
            index++;
            info->queNum = index;
        }
    }

    QUEUE_LOG_INFO("devid pid find queue num. (dev_id=%u; pid=%u; que_num=%u)\n", dev_id, pid, info->queNum);
    return DRV_ERROR_NONE;
}

static drvError_t query_the_que_perm_info(unsigned int dev_id, QueueQueryInputPara *in_put,
                                          QueueQueryOutputPara *out_put)
{
    QueueQueryOutput *out_buff = (QueueQueryOutput *)(out_put->outBuff);
    QueueQueryInput *in_buff = (QueueQueryInput *)(in_put->inBuff);
    QueueShareAttr attr = {.manage = 1, .read = 1, .write = 1};
    struct queue_manages *que_manage = NULL;
    unsigned int qid;

    if ((in_buff == NULL) || (in_put->inLen < sizeof(QueQueryQueueAttr)) ||
        (out_put->outLen < sizeof(QueQueryQueueAttrInfo))) {
        QUEUE_LOG_ERR("Input para error. (in_buff=%p; in_len=%u; out_len=%u)\n", in_buff, in_put->inLen,
                      out_put->outLen);
        return DRV_ERROR_INVALID_VALUE;
    }

    qid = (unsigned int)in_buff->queQueryQueueAttr.qid;
    if (qid >= MAX_SURPORT_QUEUE_NUM) {
        QUEUE_LOG_ERR("Input para error. (qid=%u)\n", qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    que_manage = queue_get_local_mng(qid);
    if ((que_manage == NULL) || (que_manage->valid != QUEUE_CREATED) || (que_manage->dev_id != dev_id)) {
        (void)memset_s(&out_buff->queQueryQueueAttrInfo.attr, sizeof(QueueShareAttr), 0, sizeof(QueueShareAttr));
    } else {
        out_buff->queQueryQueueAttrInfo.attr = attr;
    }
    out_put->outLen = (unsigned int)sizeof(QueQueryQueueAttrInfo);

    return DRV_ERROR_NONE;
}

static drvError_t query_ques_perm_info(unsigned int dev_id, QueueQueryInputPara *in_put, QueueQueryOutputPara *out_put)
{
    (void)in_put;
    QueueQueryOutput *out_buff = (QueueQueryOutput *)(out_put->outBuff);
    unsigned int max_num = out_put->outLen / (unsigned int)sizeof(QueQueryQuesOfProcInfo);
    QueueShareAttr attr = {.manage = 1, .read = 1, .write = 1};
    struct queue_manages *que_manage = NULL;
    unsigned int cnt = 0;
    unsigned int qid;

    for (qid = 0; qid < MAX_SURPORT_QUEUE_NUM; qid++) {
        que_manage = queue_get_local_mng(qid);
        if ((que_manage == NULL) || (que_manage->valid != QUEUE_CREATED) || (que_manage->dev_id != dev_id)) {
            continue;
        }

        if (cnt >= max_num) {
            QUEUE_LOG_ERR("output buff not enough. (cnt=%u; max_num=%u; len=%u)\n", cnt, max_num, out_put->outLen);
            return DRV_ERROR_INVALID_VALUE;
        }

        out_buff->queQueryQuesOfProcInfo[cnt].qid = (int)qid;
        out_buff->queQueryQuesOfProcInfo[cnt].attr = attr;
        cnt++;
    }
    out_put->outLen = cnt * (unsigned int)sizeof(QueQueryQuesOfProcInfo);

    QUEUE_RUN_LOG_INFO("query_ques_perm_info. (dev_id=%u; cnt=%u)\n", dev_id, cnt);
    return DRV_ERROR_NONE;
}

static drvError_t query_queue_mbuf_info(unsigned int dev_id, QueueQueryInputPara *in_put, QueueQueryOutputPara *out_put)
{
    QueueQueryOutput *out_buff = (QueueQueryOutput *)(out_put->outBuff);
    QueueQueryInput *in_buff = (QueueQueryInput *)(in_put->inBuff);
    Mbuf *mbuf = NULL;
    unsigned int qid;
    drvError_t ret;

    if ((in_buff == NULL) || (in_put->inLen < sizeof(QueQueryQueueMbuf)) ||
        (out_put->outLen < sizeof(QueQueryQueueMbufInfo))) {
        QUEUE_LOG_ERR("Input para error. (in_buff=%p; in_len=%u; out_len=%u)\n", in_buff, in_put->inLen,
                      out_put->outLen);
        return DRV_ERROR_INVALID_VALUE;
    }

    qid = in_buff->queQueryQueueMbuf.qid;
    if (qid >= MAX_SURPORT_QUEUE_NUM) {
        QUEUE_LOG_ERR("Input para error. (qid=%u)\n", qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = queue_front(dev_id, qid, &mbuf);
    if (ret != 0) {
        return ret;
    }

    out_buff->queQueryQueueMbufInfo.timestamp = mbuf_get_timestamp(mbuf);
    out_put->outLen = (unsigned int)sizeof(QueQueryQueueMbufInfo);

    queue_un_front(mbuf);

    return ret;
}

static drvError_t query_queue_max_iovec_num(unsigned int dev_id, QueueQueryInputPara *in_put,
                                            QueueQueryOutputPara *out_put)
{
    (void)dev_id;
    (void)in_put;
    QueueQueryOutput *out_buff = (QueueQueryOutput *)(out_put->outBuff);

    if (out_put->outLen < sizeof(QueQueryMaxIovecNum)) {
        QUEUE_LOG_ERR("Input para error. (out_len=%u)\n", out_put->outLen);
        return DRV_ERROR_INVALID_VALUE;
    }

    out_buff->queQueryMaxIovecNum.count = QUEUE_MAX_IOVEC_NUM;

    return DRV_ERROR_NONE;
}

static drvError_t (*g_queue_query[QUEUE_QUERY_CMD_MAX])(unsigned int dev_id, QueueQueryInputPara *in_put,
                                                        QueueQueryOutputPara *out_put) = {
    [QUEUE_QUERY_QUE_ATTR_OF_CUR_PROC] = query_the_que_perm_info,
    [QUEUE_QUERY_QUES_OF_CUR_PROC] = query_ques_perm_info,
    [QUEUE_QUERY_QUEUE_MBUF_INFO] = query_queue_mbuf_info,
    [QUEUE_QUERY_MAX_IOVEC_NUM] = query_queue_max_iovec_num,
};

STATIC drvError_t queue_query_local(unsigned int dev_id, QueueQueryCmdType cmd, QueueQueryInputPara *in_put,
                                    QueueQueryOutputPara *out_put)
{
    if (g_queue_query[cmd] == NULL) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_queue_query[cmd](dev_id, in_put, out_put);
}

static drvError_t queue_peek_data_copy_ref(unsigned int dev_id, unsigned int qid, unsigned int flag, Mbuf **mbuf)
{
    Mbuf *priv_mbuf = NULL;
    drvError_t ret;

    ret = queue_front(dev_id, qid, &priv_mbuf);
    if (ret) {
        // don't print err log in here, because queue may be empty, which is normal
        return ret;
    }

    ret = (drvError_t)halMbufCopyRef(priv_mbuf, mbuf);
    if (ret) {
        QUEUE_LOG_ERR("mbuf copy failed. (dev_id=%u; qid=%u; flag=%u; ret=%d)\n", dev_id, qid, flag, ret);
    }

    queue_un_front(priv_mbuf);
    return ret;
}

static drvError_t (*g_peek_data[QUEUE_PEEK_DATA_TYPE_MAX])(unsigned int dev_id, unsigned int qid, unsigned int flag,
                                                           Mbuf **mbuf) = {
    [QUEUE_PEEK_DATA_COPY_REF] = queue_peek_data_copy_ref,
};

drvError_t queue_peek_data_local(unsigned int dev_id, unsigned int qid, unsigned int flag, QueuePeekDataType type,
                                 void **mbuf)
{
    if (g_peek_data[type] == NULL) {
        QUEUE_LOG_INFO("no support this type. (dev_id=%u; qid=%u; flag=%u; type=%u)\n", dev_id, qid, flag, type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_peek_data[type](dev_id, qid, flag, (Mbuf **)mbuf);
}

static drvError_t queue_set_work_mode_para_check(unsigned int dev_id, QueueSetInput *input)
{
    QueueSetWorkMode set_work_mode = input->queSetWorkMode;

    if (queue_device_invalid(dev_id)) {
        QUEUE_LOG_ERR("para is error. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (((set_work_mode.workMode != QUEUE_MODE_PUSH) && (set_work_mode.workMode != QUEUE_MODE_PULL)) ||
        (set_work_mode.qid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("Input para error. (workMode=%u; qid=%u)\n", set_work_mode.workMode, set_work_mode.qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static drvError_t queue_set_work_mode(unsigned int dev_id, QueueSetInput *input)
{
    QueueSetWorkMode set_work_mode = input->queSetWorkMode;
    struct queue_manages *manage = NULL;
    drvError_t ret;

    ret = queue_set_work_mode_para_check(dev_id, input);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    manage = queue_get_local_mng(set_work_mode.qid);
    if (manage == NULL) {
        QUEUE_LOG_ERR("queue local mng is null. (dev_id=%u; qid=%u)\n", dev_id, set_work_mode.qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if (manage->valid != QUEUE_CREATED) {
        QUEUE_LOG_ERR("queue is not created. (qid=%u; valid=%d; QUEUE_CREATED=%d)\n", set_work_mode.qid, manage->valid,
                      QUEUE_CREATED);
        return DRV_ERROR_NOT_EXIST;
    }
    if (set_work_mode.workMode == QUEUE_MODE_PULL && manage->bind_type == QUEUE_TYPE_GROUP) {
        QUEUE_LOG_ERR("queue workMode can not be set to QUEUE_MODE_PULL. (qid=%u; workMode=%u; "
                      "bind_type=%d)\n",
                      set_work_mode.qid, set_work_mode.workMode, manage->bind_type);
        return DRV_ERROR_INVALID_VALUE;
    }
    manage->work_mode = (int)(set_work_mode.workMode);

    return DRV_ERROR_NONE;
}

static drvError_t (*g_queue_set[QUEUE_SET_CMD_MAX])(unsigned int dev_id, QueueSetInput *input) = {
    [QUEUE_SET_WORK_MODE] = queue_set_work_mode,
};

drvError_t queue_set_local(unsigned int dev_id, QueueSetCmdType cmd, QueueSetInputPara *input)
{
    if ((dev_id >= MAX_DEVICE) || (input == NULL) || (cmd >= QUEUE_SET_CMD_MAX) || (g_queue_set[cmd] == NULL)) {
        QUEUE_LOG_ERR("Input para error. (dev_id=%u; cmd=%u; input=%p)\n", dev_id, cmd, input);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((input->inBuff == NULL) || (input->inLen != sizeof(QueueSetInput))) {
        QUEUE_LOG_ERR("Input para error. (in_buff=%p; in_len=%u)\n", input->inBuff, input->inLen);
        return DRV_ERROR_INVALID_VALUE;
    }

    return g_queue_set[cmd](dev_id, (QueueSetInput *)input->inBuff);
}

static drvError_t queue_front_para_check(unsigned int dev_id, unsigned int qid)
{
    if (queue_device_invalid(dev_id)) {
        QUEUE_LOG_ERR("para is error. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (qid >= MAX_SURPORT_QUEUE_NUM) {
        QUEUE_LOG_ERR("para is error. (qid=%u)\n", qid);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

drvError_t queue_front(unsigned int dev_id, unsigned int qid, Mbuf **mbuf)
{
    union atomic_queue_head cur_head;
    struct queue_manages *que_manage = NULL;
    queue_entity_node *que_entity = NULL;
    void *buff = NULL;
    unsigned int depth;
    drvError_t ret;
    uint32_t blk_id;
    uint32_t head;

    ret = queue_front_para_check(dev_id, qid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    que_manage = queue_get_local_mng(qid);
    if (que_manage == NULL) {
        queue_put(qid);
        QUEUE_LOG_ERR("Queue local manage is null. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if (que_manage->valid != QUEUE_CREATED) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue is not created. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if (is_queue_empty(que_manage)) {
        que_manage->empty_flag = 1;
        queue_put(qid);
        return DRV_ERROR_QUEUE_EMPTY;
    }

    queue_get_entity_and_depth(qid, &que_entity, &depth);
    if (que_entity == NULL) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue entity is NULL.(qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }
    cur_head.head_value = queue_get_head_value(que_manage);
    head = ((cur_head.head_info.head) % queue_get_local_depth(qid));
    buff = que_entity[head].node;
    blk_id = que_entity[head].node_desp.blk_id;
    if (buff == NULL) {
        que_manage->empty_flag = 1;
        queue_put(qid);
        return DRV_ERROR_QUEUE_EMPTY;
    }

    /*
     * If the mbuf is illegal, the error code DRV_ERROR_NO_RESOURCES needs to be returned and the data discarded.
     * The upper layer will perform selective processing based on the error code.
     */
    ret = create_priv_mbuf_for_queue(mbuf, buff, blk_id);
    if (ret == DRV_ERROR_NO_RESOURCES) {
        union atomic_queue_head new_head;
#ifndef EMU_ST
        new_head.head_value = queue_head_inc(cur_head, depth);
        (void)CAS(&que_manage->queue_head.head_value, cur_head.head_value, new_head.head_value);
#endif
    }
    queue_put(qid);
    return ret;
}

void queue_un_front(Mbuf *mbuf)
{
    destroy_priv_mbuf_for_queue(mbuf);
}

void queue_update_time(unsigned int dev_id, unsigned int qid, unsigned int host_time, unsigned int msg_type)
{
    struct queue_manages *que_manage = NULL;
    drvError_t ret;

    ret = queue_front_para_check(dev_id, qid);
    if (ret != DRV_ERROR_NONE) {
        return;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
        return;
    }

    que_manage = queue_get_local_mng(qid);
    if (que_manage == NULL) {
        queue_put(qid);
        QUEUE_LOG_ERR("Queue local manage is null. (qid=%u)\n", qid);
        return;
    }

    if (que_manage->valid != QUEUE_CREATED) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue is not created. (qid=%u)\n", qid);
        return;
    }

    if (msg_type == DRV_SUBEVENT_DEQUEUE_MSG) {
        ATOMIC_SET(&que_manage->stat.last_deque_time.tv_usec, host_time);
    } else {
        ATOMIC_SET(&que_manage->stat.last_enque_time.tv_usec, host_time);
    }
    queue_put(qid);
    return;
}

drvError_t queue_get_qid_create_time(unsigned int dev_id, unsigned int qid, unsigned long *create_time)
{
    struct queue_manages *que_mng = NULL;
    drvError_t ret;

    ret = get_queue_manage_by_qid(dev_id, qid, &que_mng);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("get queue manage failed. (qid=%u; ret=%d)\n", qid, ret);
        return ret;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if (que_mng->valid != QUEUE_CREATED) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue is not created. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }
    *create_time = que_mng->create_time;
    queue_put(qid);
    return DRV_ERROR_NONE;
}

bool queue_is_inter_dev(unsigned int dev_id, unsigned int qid)
{
    struct queue_manages *que_mng = NULL;
    drvError_t ret;

    ret = get_queue_manage_by_qid(dev_id, qid, &que_mng);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("get que manage failed. (qid=%u; ret=%d)\n", qid, ret);
        return false;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("que get failed. (qid=%u)\n", qid);
        return false;
    }

    if (que_mng->valid != QUEUE_CREATED) {
        queue_put(qid);
        QUEUE_LOG_ERR("que is not created. (qid=%u)\n", qid);
        return false;
    }

    if ((que_mng->inter_dev_state == QUEUE_STATE_IMPORTED) || (que_mng->inter_dev_state == QUEUE_STATE_EXPORTED)) {
        queue_put(qid);
        return true;
    }
    queue_put(qid);
    return false;
}

STATIC struct queue_comm_interface_list g_core_interface = {
    .queue_dc_init = queue_init_local,
    .queue_uninit = NULL,
    .queue_create = queue_create_local,
    .queue_grant = queue_grant_local,
    .queue_attach = queue_attach_local,
    .queue_destroy = queue_destroy_local,
    .queue_reset = queue_reset_local,
    .queue_en_queue = queue_enqueue_local,
    .queue_de_queue = queue_dequeue_local,
    .queue_subscribe = queue_subscribe_local,
    .queue_unsubscribe = queue_unsubscribe_local,
    .queue_sub_f_to_nf_event = queue_sub_f_to_nf_event_local,
    .queue_unsub_f_to_nf_event = queue_unsub_f_to_nf_event_local,
    .queue_sub_event = queue_sub_event_local,
    .queue_unsub_event = queue_unsub_event_local,
    .queue_ctrl_event = queue_ctrl_event_local,
    .queue_query_info = queue_query_info_local,
    .queue_get_status = queue_get_status_local,
    .queue_get_qid_by_name = queue_get_qid_by_name_local,
    .queue_get_qids_by_pid = queue_get_qidsby_pid_local,
    .queue_query = queue_query_local,
    .queue_peek_data = queue_peek_data_local,
    .queue_set = queue_set_local,
    .queue_finish_cb = queue_finish_callback_local,
    .queue_export = NULL,
    .queue_unexport = NULL,
    .queue_import = NULL,
    .queue_unimport = NULL,
};

STATIC int __attribute__((constructor)) queue_core_init(void)
{
    queue_set_comm_interface(LOCAL_QUEUE, &g_core_interface);
    return 0;
}
