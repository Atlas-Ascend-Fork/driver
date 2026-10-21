/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "esched_platform.h"

#define UNINITED_STATUS 0
#define INITED_STATUS 1

THREAD__ int32_t sync_init_status[ESCHED_DEV_NUM] = {
    0,
};
THREAD__ pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

struct sync_event_res {
    struct event_res res;
    bool occupied_flag;
    pthread_mutex_t res_lock;
};

struct sync_event_aicpu_res {
    int32_t sync_init_status;
    struct sync_event_res thead_res;
};

#define DRV_MSG_EVENT_RES_MAX_NUM 128
THREAD__ struct sync_event_res drv_sync_res[ESCHED_DEV_NUM][DRV_MSG_EVENT_RES_MAX_NUM];

#define EVENT_RES_MAX_NUM 64
THREAD__ struct sync_event_res ccpu_sync_res[ESCHED_DEV_NUM][COMM_EVENT_TYPE_MAX][EVENT_RES_MAX_NUM];
THREAD__ struct sync_event_aicpu_res aicpu_sync_res[ESCHED_DEV_NUM][EVENT_RES_MAX_NUM];
THREAD__ uint32_t aicpu_sync_res_grp[ESCHED_DEV_NUM];

STATIC int32_t esched_init_thread_res(uint32_t dev_id, uint32_t gid, uint32_t event_id, uint32_t tid,
                                      struct sync_event_res *res)
{
    int32_t ret = 0;
    struct event_info back_event_info = {0};
    unsigned long long event_bitmap = (1UL << event_id);

    ret = (int)halEschedSubscribeEvent(dev_id, gid, tid, event_bitmap);
    if (ret != 0) {
        sched_err("Failed to invoke the halEschedSubscribeEvent. (ret=%d)\n", ret);
        return ret;
    }

    (void)halEschedWaitEvent(dev_id, gid, tid, 0, &back_event_info);
    res->res.gid = gid;
    res->res.tid = tid;
    res->res.event_id = event_id;
    res->res.subevent_id = 0;
    res->occupied_flag = false;
    (void)pthread_mutex_init(&res->res_lock, NULL);

    return DRV_ERROR_NONE;
}

static inline void esched_grp_para_cfg(struct esched_grp_para *grp_para, uint32_t grp_index)
{
    grp_para->type = GRP_TYPE_BIND_CP_CPU;
    grp_para->threadNum = SCHED_MAX_SYNC_THREAD_NUM_PER_GRP;
    (void)snprintf_s(grp_para->grp_name, EVENT_MAX_GRP_NAME_LEN, EVENT_MAX_GRP_NAME_LEN - 1, "%s_%u", "ESCHED_GRP",
                     grp_index);
}

STATIC int32_t esched_init_ccpu_grp_res(uint32_t dev_id, uint32_t gid, uint32_t event_type, uint32_t grp_index)
{
    uint32_t thread_idx, event_id, tid;
    int32_t ret = 0;
    struct sync_event_res *res = NULL;

    thread_idx = grp_index * SCHED_MAX_SYNC_THREAD_NUM_PER_GRP;
    for (tid = 0; tid < SCHED_MAX_SYNC_THREAD_NUM_PER_GRP; tid++) {
        res = &ccpu_sync_res[dev_id][event_type][thread_idx];
        event_id = SCHED_SYNC_START_EVENT_ID + tid;
        ret = esched_init_thread_res(dev_id, gid, event_id, tid, res);
        if (ret != 0) {
            sched_err("Failed to init sync thread. (ret=%d; dev_id=%d; gid=%d;)\n", ret, dev_id, gid);
            return ret;
        }
        thread_idx++;
    }
    return DRV_ERROR_NONE;
}

static long esched_get_cpu(unsigned int *cpu, unsigned int *node)
{
    return syscall(SYS_getcpu, cpu, node, NULL);
}

STATIC struct sync_event_res *esched_get_aicpu_res(uint32_t dev_id)
{
    uint32_t gid, event_id, cpu_id, node;
    int32_t ret = 0;
    struct sync_event_aicpu_res *sync_res = NULL;

    if (esched_get_cpu(&cpu_id, &node) != 0) {
        sched_err("Failed to get cpu id. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    if (cpu_id >= EVENT_RES_MAX_NUM) {
        sched_err("cpu id is invalid. (dev_id=%u; cpu_id=%u)\n", dev_id, cpu_id);
        return NULL;
    }

    (void)pthread_mutex_lock(&init_mutex);
    sync_res = &aicpu_sync_res[dev_id][cpu_id];
    if (sync_res->sync_init_status == INITED_STATUS) {
        goto out;
    }

    gid = aicpu_sync_res_grp[dev_id];
    event_id = SCHED_SYNC_START_EVENT_ID + cpu_id;
    ret = esched_init_thread_res(dev_id, gid, event_id, cpu_id, &sync_res->thead_res);
    if (ret != 0) {
        sched_err("Failed to subscribe thread. (ret=%d)\n", ret);
        (void)pthread_mutex_unlock(&init_mutex);
        return NULL;
    }
    sync_res->sync_init_status = INITED_STATUS;
    sched_info("Init aicpu sync msg thread. (cpu_id=%u; gid=%u)\n", cpu_id, gid);
out:
    (void)pthread_mutex_unlock(&init_mutex);
    return &sync_res->thead_res;
}

STATIC void esched_sync_res_invalid(uint32_t dev_id)
{
    uint32_t tid, cpuid, event_type;
    struct sync_event_res *res = NULL;
    for (event_type = NORMAL_EVENT; event_type < COMM_EVENT_TYPE_MAX; event_type++) {
        for (tid = 0; tid < EVENT_RES_MAX_NUM; tid++) {
            res = &ccpu_sync_res[dev_id][event_type][tid];
            res->occupied_flag = true;
        }
    }

    for (cpuid = 0; cpuid < EVENT_RES_MAX_NUM; cpuid++) {
        res = &aicpu_sync_res[dev_id][cpuid].thead_res;
        res->occupied_flag = true;
    }

    for (cpuid = 0; cpuid < DRV_MSG_EVENT_RES_MAX_NUM; cpuid++) {
        res = &drv_sync_res[dev_id][cpuid];
        res->occupied_flag = true;
    }

    aicpu_sync_res_grp[dev_id] = 0;
}

void esched_sync_res_reset(uint32_t dev_id)
{
    uint32_t i;

    (void)pthread_mutex_lock(&init_mutex);
    sync_init_status[dev_id] = UNINITED_STATUS;
    for (i = 0; i < EVENT_RES_MAX_NUM; i++) {
        aicpu_sync_res[dev_id][i].sync_init_status = UNINITED_STATUS;
    }
    aicpu_sync_res_grp[dev_id] = 0;
    (void)pthread_mutex_unlock(&init_mutex);
}

STATIC int32_t esched_ccpu_res_init(uint32_t dev_id)
{
    int32_t ret = 0;
    uint32_t inited_num = 0;
    uint32_t gid, curr_grp_index, i, event_type, cur_grp_num;
    struct esched_grp_para grp_para = {0};
    int32_t grp_num_per_event_type[COMM_EVENT_TYPE_MAX] = {4, 4, 4, 4};

    /* support gid_index(0-15) and event_id(28-43) used for sync event res */
    for (event_type = 0; event_type < COMM_EVENT_TYPE_MAX; event_type++) {
        cur_grp_num = (uint32_t)grp_num_per_event_type[event_type];
        if ((cur_grp_num + inited_num) > SCHED_MAX_SYNC_GRP_NUM) {
            sched_err("sync group config error. (grp_inited_num=%u; event_type=%u)\n", inited_num, event_type);
            return DRV_ERROR_PARA_ERROR;
        }
        for (i = 0; i < cur_grp_num; i++) {
            curr_grp_index = i + inited_num;
            esched_grp_para_cfg(&grp_para, curr_grp_index);
            ret = (int)halEschedCreateGrpEx(dev_id, &grp_para, &gid);
            if (ret != 0) {
                sched_err("Failed to create group. (grp_index=%u; ret=%d)\n", curr_grp_index, ret);
                return ret;
            }
            ret = esched_init_ccpu_grp_res(dev_id, gid, event_type, i);
            if (ret != 0) {
                sched_err("Failed to init sync group. (grp_index=%u; ret=%d)\n", curr_grp_index, ret);
                return ret;
            }
        }
        inited_num += cur_grp_num;
    }
    return ret;
}

STATIC int32_t esched_aicpu_res_init(uint32_t dev_id)
{
    int32_t ret, len;
    uint32_t gid;
    struct esched_grp_para grp_para = {0};

    grp_para.type = GRP_TYPE_BIND_DP_CPU;
    grp_para.threadNum = EVENT_RES_MAX_NUM;
    len = snprintf_s(grp_para.grp_name, EVENT_MAX_GRP_NAME_LEN, EVENT_MAX_GRP_NAME_LEN - 1, "%s", "ESCHED_GRP_AI");
    if (len <= 0) {
        sched_err("Failed to init grp_name. (dev_id=%d)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = (int)halEschedCreateGrpEx(dev_id, &grp_para, &gid);
    if (ret != 0) {
        sched_err("Failed to create aicpu group. (ret=%d)\n", ret);
        return ret;
    }
    aicpu_sync_res_grp[dev_id] = gid;
    sched_info("Init aicpu sync msg grp. (gid=%u)\n", gid);
    return ret;
}

STATIC int32_t esched_drv_res_init(uint32_t dev_id)
{
    drvError_t ret;
    int32_t len;
    uint32_t gid, tid;
    struct esched_grp_para grp_para = {0};
    struct sync_event_res *res = NULL;

    grp_para.type = GRP_TYPE_BIND_CP_CPU;
    grp_para.threadNum = DRV_MSG_EVENT_RES_MAX_NUM;
    len = snprintf_s(grp_para.grp_name, EVENT_MAX_GRP_NAME_LEN, EVENT_MAX_GRP_NAME_LEN - 1, "%s", "DRV_ESCHED_GRP");
    if (len <= 0) {
        sched_err("Failed to init grp_name. (dev_id=%d)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = halEschedCreateGrpEx(dev_id, &grp_para, &gid);
    if (ret != DRV_ERROR_NONE) {
        sched_err("Call hal_esched_create_grp failed. (dev_id=%u, ret=%d)\n", dev_id, (int)ret);
        return ret;
    }

    for (tid = 0; tid < DRV_MSG_EVENT_RES_MAX_NUM; tid++) {
        res = &drv_sync_res[dev_id][tid];
        ret = esched_init_thread_res(dev_id, gid, EVENT_DRV_MSG_EX, tid, res);
        if (ret != 0) {
            sched_err("Failed to init sync thread. (ret=%d; dev_id=%d; gid=%d;)\n", ret, dev_id, gid);
            return ret;
        }
    }
    sched_info("Init drv inner sync res group success. (dev_id=%u, gid=%u)\n", dev_id, gid);

    return DRV_ERROR_NONE;
}

STATIC int32_t esched_res_init(uint32_t dev_id)
{
    int32_t ret = 0;

    (void)pthread_mutex_lock(&init_mutex);
    if (sync_init_status[dev_id] == INITED_STATUS) {
        goto out;
    }
    esched_sync_res_invalid(dev_id);

    ret = (int)halEschedAttachDevice(dev_id);
    if ((ret != 0) && (ret != DRV_ERROR_PROCESS_REPEAT_ADD)) {
        sched_err("Failed to attach device. (ret=%d)\n", ret);
        goto out;
    }

    ret = esched_ccpu_res_init(dev_id);
    if (ret != 0) {
        sched_err("Failed to init comm res. (ret=%d)\n", ret);
        goto out;
    }

    ret = esched_aicpu_res_init(dev_id);
    if (ret != 0) {
        sched_err("Failed to init aicpu res. (ret=%d)\n", ret);
        goto out;
    }

    ret = esched_drv_res_init(dev_id);
    if (ret != 0) {
        sched_err("Failed to init drv res. (ret=%d)\n", ret);
        goto out;
    }
    sync_init_status[dev_id] = INITED_STATUS;
    sched_info("Init sync res group success. (dev_id=%u)\n", dev_id);
out:
    (void)pthread_mutex_unlock(&init_mutex);
    return ret;
}

STATIC struct sync_event_res *esched_get_event_res_pool(uint32_t dev_id, int32_t event_type, int32_t *max_thread)
{
    int32_t thread_num;
    struct sync_event_res *res = NULL;

    switch (event_type) {
        case DRV_INNER_EVENT:
            thread_num = DRV_MSG_EVENT_RES_MAX_NUM;
            res = &drv_sync_res[dev_id][0];
            break;
        case QUEUE_ACPU_EVENT:
            thread_num = 1;
            res = esched_get_aicpu_res(dev_id);
            break;
        default:
            thread_num = EVENT_RES_MAX_NUM;
            res = &ccpu_sync_res[dev_id][event_type][0];
            break;
    }
    *max_thread = thread_num;
    return res;
}

STATIC int32_t esched_find_idle_res(struct sync_event_res *res, int32_t max_thread, struct event_res *e_res)
{
#ifndef EMU_VCAST_ST
    int32_t i;

    for (i = 0; i < max_thread; i++) {
        (void)pthread_mutex_lock(&res[i].res_lock);
        if (res[i].occupied_flag == false) {
            res[i].res.subevent_id++;
            res[i].res.subevent_id = (res[i].res.subevent_id >= SCHED_SYNC_MAX_SUB_EVENT_ID) ? 0 :
                                                                                               res[i].res.subevent_id;
            *e_res = res[i].res;
            res[i].occupied_flag = true;
            (void)pthread_mutex_unlock(&res[i].res_lock);
            return 0;
        }
        (void)pthread_mutex_unlock(&res[i].res_lock);
    }
    return (int32_t)DRV_ERROR_NO_EVENT_RESOURCES;
#else
    return 0;
#endif
}

int32_t esched_alloc_event_res(uint32_t dev_id, int32_t event_type, struct event_res *e_res)
{
    int32_t max_thread, ret;
    struct sync_event_res *res = NULL;

    if ((esched_device_check(dev_id) != 0) || (event_type >= EVENT_TYPE_BUTT)) {
        sched_err("Invalid para. (dev_id=%u; event_type=%d)\n", dev_id, event_type);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = esched_res_init(dev_id);
    if (ret != 0) {
        sched_err("esched_res_init failed. (ret=%d; dev_id=%u; event_type=%d)\n", ret, dev_id, event_type);
        return ret;
    }

    res = esched_get_event_res_pool(dev_id, event_type, &max_thread);
    if (res == NULL) {
        sched_err("sync res alloc failed. (ret=%d; dev_id=%u; event_type=%d)\n", ret, dev_id, event_type);
        return ret;
    }

    return esched_find_idle_res(res, max_thread, e_res);
}

STATIC void esched_ccpu_res_free(uint32_t devId, int32_t event_type, const struct event_res *e_res)
{
    struct sync_event_res *res = NULL;
    int32_t i;

    for (i = 0; i < (int32_t)EVENT_RES_MAX_NUM; i++) {
        res = &ccpu_sync_res[devId][event_type][i];
        (void)pthread_mutex_lock(&res->res_lock);
        if ((e_res->gid == res->res.gid) && (e_res->tid == res->res.tid) && (e_res->event_id == res->res.event_id)) {
            res->occupied_flag = false;
            (void)pthread_mutex_unlock(&res->res_lock);
            return;
        }
        (void)pthread_mutex_unlock(&res->res_lock);
    }
}

STATIC void esched_aicpu_res_free(uint32_t dev_id)
{
    struct sync_event_res *res = esched_get_aicpu_res(dev_id);

    if (res == NULL) {
        sched_err("get res failed. (dev_id=%u)\n", dev_id);
        return;
    }

    (void)pthread_mutex_lock(&res->res_lock);
    if (res->occupied_flag == false) {
        (void)pthread_mutex_unlock(&res->res_lock);
        sched_info("aicpu sync res exception. (dev_id=%u; tid=%d)\n", dev_id, res->res.tid);
        return;
    }
    res->occupied_flag = false;
    (void)pthread_mutex_unlock(&res->res_lock);
}

STATIC void esched_drv_res_free(uint32_t dev_id, const struct event_res *e_res)
{
    struct sync_event_res *res = NULL;
    uint32_t tid = e_res->tid;

    if (tid >= DRV_MSG_EVENT_RES_MAX_NUM) {
        sched_err("Invalid para. (dev_id=%u; tid=%d)\n", dev_id, tid);
        return;
    }

    res = &drv_sync_res[dev_id][tid];
    (void)pthread_mutex_lock(&res->res_lock);
    if (res->occupied_flag == false) {
        (void)pthread_mutex_unlock(&res->res_lock);
        sched_err("sync res exception. (dev_id=%u; tid=%d)\n", dev_id, tid);
        return;
    }
    res->occupied_flag = false;
    (void)pthread_mutex_unlock(&res->res_lock);
}

void esched_free_event_res(uint32_t dev_id, int32_t event_type, const struct event_res *e_res)
{
    if ((esched_device_check(dev_id) != 0) || (event_type >= EVENT_TYPE_BUTT)) {
        sched_err("Invalid para. (dev_id=%u; event_type=%d)\n", dev_id, event_type);
        return;
    }

    switch (event_type) {
        case DRV_INNER_EVENT:
            esched_drv_res_free(dev_id, e_res);
            break;
        case QUEUE_ACPU_EVENT:
            esched_aicpu_res_free(dev_id);
            break;
        default:
            esched_ccpu_res_free(dev_id, event_type, e_res);
            break;
    }
    return;
}

void esched_fill_sync_msg(struct event_summary *event, const struct event_res *res)
{
    unsigned int dst_engine;

    (void)dst_engine;
    dst_engine = event->dst_engine;
    ((struct event_sync_msg *)event->msg)->pid = (uint32_t)esched_get_sync_pid(dst_engine) & 0x3FFFFF;

    ((struct event_sync_msg *)event->msg)->dst_engine = (uint32_t)esched_get_sync_dst_engine() & 0xF;
    ((struct event_sync_msg *)event->msg)->gid = (uint32_t)res->gid & 0x3F;
    ((struct event_sync_msg *)event->msg)->event_id = (uint32_t)res->event_id & 0x3F;
    ((struct event_sync_msg *)event->msg)->subevent_id = res->subevent_id;
    ((struct event_sync_msg *)event->msg)->tid = (uint32_t)res->tid & 0xFF;
}

void esched_fill_sync_msg_for_peer_que(uint32_t dev_id, uint32_t phy_dev_id, struct event_summary *event,
                                       const struct event_res *res)
{
    esched_fill_sync_msg(event, res);

    esched_fill_sync_msg_peer_platform(dev_id, event, res);
    ((struct event_sync_msg *)event->msg)->dev_id = (uint32_t)phy_dev_id & 0x3F;
}

static void esched_fill_wait_info(struct sync_event_wait_info *wait_info, struct event_summary *event,
                                  struct event_res *res, int32_t timeout)
{
    wait_info->grp_id = res->gid;
    wait_info->thread_id = res->tid;
    wait_info->pid = event->pid;
    wait_info->event_id = event->event_id;
    wait_info->subevent_id = event->subevent_id;
    wait_info->timeout = timeout;
    return;
}

drvError_t halEschedSubmitEventSync(uint32_t devId, struct event_summary *event, int32_t timeout,
                                    struct event_reply *reply)
{
    int wait_succ_cnt = 0;
    int32_t ret, event_type;
    struct event_res res;
    struct event_info back_event_info;
    esched_event_buffer *event_buffer = (esched_event_buffer *)back_event_info.priv.msg;
    unsigned long long timestamp = esched_get_cur_cpu_timestamp();
    struct sync_event_wait_info wait_info = {0};

    if (event == NULL) {
        sched_err("The variable event is NULL.\n");
        report_arg_null_pointer(__FUNCTION__, "event");
        return DRV_ERROR_PARA_ERROR;
    }
    if (reply == NULL) {
        sched_err("The variable reply is NULL.\n");
        report_arg_null_pointer(__FUNCTION__, "reply");
        return DRV_ERROR_PARA_ERROR;
    }
    if (event->msg == NULL) {
        sched_err("The event->msg is NULL.\n");
        report_arg_null_pointer(__FUNCTION__, "event->msg");
        return DRV_ERROR_PARA_ERROR;
    }
    if (event->msg_len < sizeof(struct event_sync_msg)) {
        sched_err("event->msg_len is invalid.(event->msg_len=%u)\n", event->msg_len);
        return DRV_ERROR_PARA_ERROR;
    }

    event_type = (event->event_id == EVENT_DRV_MSG_EX) ? DRV_INNER_EVENT : NORMAL_EVENT;
    ret = esched_alloc_event_res(devId, event_type, &res);
    if (ret != 0) {
        sched_err("Failed to invoke the Esched_sync_init. (ret=%d)\n", ret);
        return ret;
    }

    if (event_type == DRV_INNER_EVENT) {
        res.subevent_id = (uint32_t)event->subevent_id & 0xFFF;
    }
    esched_fill_sync_msg(event, &res);
    ret = (int)halEschedSubmitEvent(devId, event);
    if (ret != 0) {
        /* flowgw maybe not ready, client will retry. */
        sched_warn("Unable to invoke the halEschedSubmitEvent. (devId=%u, event_id=%u; ret=%d)\n", devId,
                   event->event_id, ret);
        esched_free_event_res(devId, event_type, &res);
        return ret;
    }

    event_buffer->msg = reply->buf;
    event_buffer->msg_len = reply->buf_len;

    do {
        esched_fill_wait_info(&wait_info, event, &res, timeout);
        ret = esched_wait_sync_event(devId, &wait_info, &back_event_info);
        if (ret != 0) {
            if ((ret == DRV_ERROR_WAIT_TIMEOUT) ||
                (esched_is_svm_alloc_event(devId, event->event_id, event->subevent_id))) {
                sched_run_info("Sync event not success. (ret=%d; dst_pid=%d; event_id=%u; subevent_id=%u; "
                               "sync_gid=%u; sync_event_id=%u; sync_subevent_id=%u; sync_tid=%u; timeout=%dms; "
                               "current_tick=%llu)\n",
                               ret, event->pid, event->event_id, event->subevent_id, res.gid, res.event_id,
                               res.subevent_id, res.tid, timeout, timestamp);
            } else {
                sched_err("Sync event failed. (ret=%d; dst_pid=%d; event_id=%u; subevent_id=%u; "
                          "sync_gid=%u; sync_event_id=%u; sync_subevent_id=%u; sync_tid=%u; timeout=%dms; "
                          "current_tick=%llu)\n",
                          ret, event->pid, event->event_id, event->subevent_id, res.gid, res.event_id, res.subevent_id,
                          res.tid, timeout, timestamp);
            }
            if (ret == DRV_ERROR_WAIT_TIMEOUT) {
                esched_query_sync_msg_trace(devId, event, res.gid, res.tid);
            }
            esched_free_event_res(devId, event_type, &res);
            return ret;
        }

        wait_succ_cnt++;
        if ((back_event_info.comm.submit_timestamp >= timestamp) &&
            (back_event_info.comm.subevent_id == res.subevent_id)) {
            break;
        }

        sched_run_info("Successfully waited for an event but the condition was not met. (devid=%u; gid=%u; "
                       "tid=%u; cnt=%d; check_time=%llu; back_time=%llu; check_subevent=%u; back_subevent=%u)\n",
                       devId, res.gid, res.tid, wait_succ_cnt, timestamp, back_event_info.comm.submit_timestamp,
                       res.subevent_id, back_event_info.comm.subevent_id);
    } while (1);

    esched_free_event_res(devId, event_type, &res);

    reply->reply_len = back_event_info.priv.msg_len;

    return 0;
}
