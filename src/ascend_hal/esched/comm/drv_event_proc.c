/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <pthread.h>

#include "ascend_hal.h"

#include "securec.h"
#include "esched_user_interface.h"

#include <sys/prctl.h>

#if defined CFG_FEATURE_SYSLOG
#include <syslog.h>
#define DRV_EVENT_LOG_ERR(fmt, ...) syslog(LOG_ERR, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define DRV_EVENT_LOG_WARN(fmt, ...) syslog(LOG_WARNING, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define DRV_EVENT_LOG_DBG(fmt, ...) syslog(LOG_DEBUG, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define DRV_EVENT_LOG_RUN_INFO(fmt, ...) syslog(LOG_DEBUG, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#ifndef EMU_ST
#include "dmc_user_interface.h"
#else
#include "ascend_inpackage_hal.h"
#include "ut_log.h"
#endif

#define DRV_EVENT_LOG_ERR(format, ...)                          \
    do {                                                        \
        DRV_ERR(HAL_MODULE_TYPE_COMMON, format, ##__VA_ARGS__); \
    } while (0)

#define DRV_EVENT_LOG_WARN(format, ...)                          \
    do {                                                         \
        DRV_WARN(HAL_MODULE_TYPE_COMMON, format, ##__VA_ARGS__); \
    } while (0)

#define DRV_EVENT_LOG_DBG(format, ...)                            \
    do {                                                          \
        DRV_DEBUG(HAL_MODULE_TYPE_COMMON, format, ##__VA_ARGS__); \
    } while (0)
#define DRV_EVENT_LOG_RUN_INFO(format, ...)                          \
    do {                                                             \
        DRV_RUN_INFO(HAL_MODULE_TYPE_COMMON, format, ##__VA_ARGS__); \
    } while (0)

#endif

#define SCHED_INVALID_TID 0xFFFFFFFFU
#define DRV_PROC_WAIT_BUFFER_LEN 512U
#define EVENT_PROC_RESULT_BUFFER_LEN 512U
#define SCHED_INVALID_DEVID 0xFFFFFFFFU
struct event_proc_result_buffer {
    int buffer_len;
    char *buffer;
};

struct drv_event_proc g_drv_event_proc[DRV_SUBEVENT_MAX_MSG] = {0};

static pthread_mutex_t g_drv_thread_lock = PTHREAD_MUTEX_INITIALIZER;

void drv_registert_event_proc(DRV_SUBEVENT_ID id, struct drv_event_proc *event_proc)
{
    g_drv_event_proc[id] = *event_proc;
}

static drvError_t drv_check_event(struct event_info *event)
{
#ifndef EMU_ST
    if ((event->comm.event_id != EVENT_DRV_MSG) && (event->comm.event_id != EVENT_DRV_MSG_EX)) {
        DRV_EVENT_LOG_ERR("invalid event_id(%u).\n", event->comm.event_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (event->comm.subevent_id >= DRV_SUBEVENT_MAX_MSG) {
        DRV_EVENT_LOG_ERR("invalid subevent_id(%u).\n", event->comm.subevent_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (g_drv_event_proc[event->comm.subevent_id].proc_func == NULL) {
        DRV_EVENT_LOG_WARN("not support subevent_id(%u).\n", event->comm.subevent_id);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (event->priv.msg_len < sizeof(struct event_sync_msg)) {
        DRV_EVENT_LOG_ERR("invalid msg_len(%u).\n", event->priv.msg_len);
        return DRV_ERROR_PARA_ERROR;
    }

    if (g_drv_event_proc[event->comm.subevent_id].proc_size != (event->priv.msg_len - sizeof(struct event_sync_msg))) {
        DRV_EVENT_LOG_ERR("subevent_id(%u) has invalid msg_len(%u).\n", event->comm.subevent_id, event->priv.msg_len);
        return DRV_ERROR_PARA_ERROR;
    }
#endif

    return DRV_ERROR_NONE;
}

static drvError_t drv_event_proc_dispatch(unsigned int dev_id, struct event_info *event,
                                          esched_event_buffer *msg_buffer, struct drv_event_proc_rsp *rsp)
{
#ifndef EMU_ST
    return g_drv_event_proc[event->comm.subevent_id].proc_func(
        dev_id, (void *)(msg_buffer->msg + sizeof(struct event_sync_msg)), (int)msg_buffer->msg_len, rsp);
#else
    char msg[EVENT_MAX_MSG_LEN];
    drvError_t ret, result;
    int len, r_len;

    len = sizeof(char) * EVENT_MAX_MSG_LEN - sizeof(struct event_sync_msg);
    r_len = event->priv.msg_len - sizeof(struct event_sync_msg);
    ret = memcpy_s((void *)msg, len, (void *)(msg_buffer->msg + sizeof(struct event_sync_msg)), r_len);
    if (ret != 0) {
        DRV_EVENT_LOG_ERR("Memcpy failed.\n");
        return DRV_ERROR_PARA_ERROR;
    }
    result = g_drv_event_proc[event->comm.subevent_id].proc_func(dev_id, (void *)msg, EVENT_MAX_MSG_LEN, rsp);
    ret = memcpy_s((void *)(msg_buffer->msg + sizeof(struct event_sync_msg)), r_len, (void *)msg, r_len);
    if (ret != 0) {
        DRV_EVENT_LOG_ERR("Memcpy failed.\n");
        return DRV_ERROR_PARA_ERROR;
    }

#ifdef CFG_FEATURE_TSDRV_CORE_LIB
    return result;
#else
    return ret;
#endif
#endif
}

void drv_update_dst_devid(struct event_sync_msg *sync_msg, unsigned int *dst_dev_id, unsigned int *tid)
{
    drvError_t ret;
    switch (sync_msg->dst_engine) {
        case SPECIFYED_ACPU_DEVICE:
            *tid = sync_msg->tid;
            *dst_dev_id = sync_msg->dev_id;
            return;
        case SPECIFYED_CCPU_DEVICE:
            *dst_dev_id = sync_msg->dev_id;
            return;
        case VIRTUAL_CCPU_HOST:
            ret = halGetHostID(dst_dev_id);
            if (ret != DRV_ERROR_NONE) {
                DRV_EVENT_LOG_ERR("drv update dst_dev_id failed. (ret=%d)\n", ret);
            }
            return;
        default:
            return;
    }
}

/*
 * Fill the reply buffer with the proc result and submit the back event to the dst engine.
 * Return the submit result (DRV_ERROR_NONE on success).
 */
static drvError_t drv_event_submit_back(unsigned int dev_id, struct event_info *event, esched_event_buffer *msg_buffer,
                                        struct event_proc_result_buffer *rsp_buffer, struct drv_event_proc_rsp *rsp,
                                        drvError_t proc_ret)
{
    struct event_sync_msg *msg_head = NULL;
    struct event_summary back_event = {0};
    unsigned int dst_dev_id = SCHED_INVALID_DEVID;
    unsigned int tid = SCHED_INVALID_TID;
    drvError_t ret;

    msg_head = (struct event_sync_msg *)msg_buffer->msg;
    drv_update_dst_devid(msg_head, &dst_dev_id, &tid);
    DRV_EVENT_REPLY_BUFFER_RET(rsp_buffer->buffer) = proc_ret;
    back_event.dst_engine = msg_head->dst_engine;
    back_event.policy = ONLY;
    back_event.pid = msg_head->pid;
    back_event.grp_id = msg_head->gid;
    back_event.event_id = msg_head->event_id;
    back_event.subevent_id = msg_head->subevent_id;
    back_event.msg_len = (uint32_t)(rsp->real_rsp_data_len + (int)sizeof(proc_ret));
    back_event.msg = rsp_buffer->buffer;
    back_event.tid = (msg_head->event_id == EVENT_DRV_MSG_EX) ? msg_head->tid : tid;

    DRV_EVENT_LOG_DBG("submit event start. (proc_name=%s; dst_dev_id=%u; pid=%u; dst_engine=%u; event_id=%u; gid=%u; "
                      "subevent_id=%u)\n",
                      g_drv_event_proc[event->comm.subevent_id].proc_name, dst_dev_id, msg_head->pid,
                      msg_head->dst_engine, msg_head->event_id, msg_head->gid, msg_head->subevent_id);

    ret = halEschedSubmitEventEx(dev_id, dst_dev_id, &back_event);
    if (ret != 0) {
#ifndef EMU_ST
        DRV_EVENT_LOG_RUN_INFO("halEschedSubmitEvent, ret=%d.\n", ret);
#endif
    }

    DRV_EVENT_LOG_DBG(
        "Event_proc end. (proc_name=%s; dst_dev_id=%u; pid=%u; dst_engine=%u; event_id=%u; gid=%u; subevent_id=%u)\n",
        g_drv_event_proc[event->comm.subevent_id].proc_name, dst_dev_id, msg_head->pid, msg_head->dst_engine,
        msg_head->event_id, msg_head->gid, msg_head->subevent_id);

    return ret;
}

int drv_event_proc(unsigned int dev_id, struct event_info *event, esched_event_buffer *msg_buffer,
                   struct event_proc_result_buffer *rsp_buffer)
{
    struct drv_event_proc_rsp rsp = {0};
    drvError_t ret;

    ret = drv_check_event(event);
    if (ret != DRV_ERROR_NONE) {
        DRV_EVENT_LOG_ERR("Check_event failed, ret=%d.\n", ret);
        return ret;
    }
    DRV_EVENT_LOG_DBG("Event_proc start. (devid=%u)\n", dev_id);

    rsp.rsp_data_buf = (void *)DRV_EVENT_REPLY_BUFFER_DATA_PTR(rsp_buffer->buffer);
    rsp.rsp_data_buf_len = (int)(rsp_buffer->buffer_len - (int)sizeof(int));
    rsp.need_rsp = true;

    ret = drv_event_proc_dispatch(dev_id, event, msg_buffer, &rsp);
    if (ret != DRV_ERROR_NONE) {
        DRV_EVENT_LOG_DBG("Event_proc not success, ret=%d, subevent_id=%d.\n", ret, event->comm.subevent_id);
    }

    if (rsp.real_rsp_data_len > rsp.rsp_data_buf_len) {
        DRV_EVENT_LOG_ERR("Rsp_len beyond. (rsp_len=%d; max=%d)\n", rsp.real_rsp_data_len,
                          rsp_buffer->buffer_len - (int)sizeof(int));
#ifndef EMU_ST
        return DRV_ERROR_INNER_ERR;
#endif
    }

    /* not need submit */
    if (rsp.need_rsp == false) {
        return DRV_ERROR_NONE;
    }

    return drv_event_submit_back(dev_id, event, msg_buffer, rsp_buffer, &rsp, ret);
}

drvError_t halEventProc(unsigned int devId, struct event_info *event)
{
    esched_event_buffer msg_buffer = {0};
    struct event_proc_result rsp = {0};
    struct event_proc_result_buffer rsp_buffer = {sizeof(struct event_proc_result), (char *)&rsp};

    if (event == NULL) {
        DRV_EVENT_LOG_ERR("event is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    msg_buffer.msg = event->priv.msg;
    msg_buffer.msg_len = event->priv.msg_len;

    return drv_event_proc(devId, event, &msg_buffer, &rsp_buffer);
}

static int drv_event_query_grid(unsigned int dev_id, unsigned int *grp_id)
{
    struct esched_query_gid_output gid_out = {0};
    struct esched_query_gid_input gid_in = {0};
    struct esched_output_info out_put = {0};
    struct esched_input_info in_put = {0};
    drvError_t ret;

    gid_in.pid = (int)getpid();
    (void)strcpy_s(gid_in.grp_name, EVENT_MAX_GRP_NAME_LEN, EVENT_DRV_MSG_GRP_NAME);
    in_put.inBuff = &gid_in;
    in_put.inLen = sizeof(struct esched_query_gid_input);
    out_put.outBuff = &gid_out;
    out_put.outLen = sizeof(struct esched_query_gid_output);
    ret = halEschedQueryInfo(dev_id, QUERY_TYPE_LOCAL_GRP_ID, &in_put, &out_put);
    if (ret == DRV_ERROR_NONE) {
        *grp_id = gid_out.grp_id;
    }

    return ret;
}

static void *drv_event_thread_proc(void *data)
{
    unsigned int dev_id = (unsigned int)(uintptr_t)data;
    struct event_info event;
    esched_event_buffer *event_buffer = (esched_event_buffer *)event.priv.msg;
    struct event_proc_result_buffer rsp_buffer = {0};
    unsigned int grp_id;
    int ret;

    (void)prctl(PR_SET_NAME, "drv_event_proc");
    ret = drv_event_query_grid(dev_id, &grp_id);
    if (ret != DRV_ERROR_NONE) {
        DRV_EVENT_LOG_ERR("Get gid failed. (ret=%d; dev_id=%u, tgid=%d)\n", ret, dev_id, getpid());
        return NULL;
    }

    event_buffer->msg = (char *)malloc(sizeof(char) * DRV_PROC_WAIT_BUFFER_LEN);
    if (event_buffer->msg == NULL) {
        DRV_EVENT_LOG_ERR("Malloc wait buffer failed. (dev_id=%u, tgid=%d)\n", dev_id, getpid());
        return NULL;
    }
    event_buffer->msg_len = DRV_PROC_WAIT_BUFFER_LEN;

    rsp_buffer.buffer = (char *)malloc(sizeof(char) * EVENT_PROC_RESULT_BUFFER_LEN);
    if (rsp_buffer.buffer == NULL) {
        free(event_buffer->msg);
        DRV_EVENT_LOG_ERR("Malloc rep buffer failed. (dev_id=%u, tgid=%d)\n", dev_id, getpid());
        return NULL;
    }
    rsp_buffer.buffer_len = sizeof(char) * EVENT_PROC_RESULT_BUFFER_LEN;

    while (1) {
        ret = esched_wait_event_ex(dev_id, grp_id, 0, 10000, &event); /* timeout 10000 ms */
        if (ret == DRV_ERROR_NONE) {
            DRV_EVENT_LOG_DBG("Wait event. (dev_id=%u; event_id=%u; subevent_id=%u)\n", dev_id, event.comm.event_id,
                              event.comm.subevent_id);
            if ((event.comm.event_id == EVENT_DRV_MSG) || (event.comm.event_id == EVENT_DRV_MSG_EX)) {
                (void)drv_event_proc(dev_id, &event, event_buffer, &rsp_buffer);
            }
        } else if ((ret != DRV_ERROR_SCHED_WAIT_TIMEOUT) && (ret != DRV_ERROR_NO_EVENT)) {
            break;
        }
    }

    free(rsp_buffer.buffer);
    free(event_buffer->msg);

    return NULL;
}

static int drv_event_create_task_with_detach(unsigned int dev_id)
{
    pthread_t event_thread;
    pthread_attr_t attr;
    int ret;

    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&event_thread, &attr, drv_event_thread_proc, (void *)(unsigned long)dev_id);
    (void)pthread_attr_destroy(&attr);
    if (ret != 0) {
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

drvError_t halDrvEventThreadInit(unsigned int devId)
{
    struct event_info event = {0};
    struct esched_grp_para grp_para;
    unsigned long event_bitmap = ((0x1UL << EVENT_DRV_MSG) | (0x1UL << EVENT_DRV_MSG_EX));
    unsigned int grp_id;
    int ret;

    /* process attach device only need once */
    ret = halEschedAttachDevice(devId);
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_PROCESS_REPEAT_ADD) {
            DRV_EVENT_LOG_ERR("Call halEschedAttachDevice failed. (devId=%u, ret=%d)\n", devId, ret);
            return ret;
        }
    }
#ifndef EMU_ST
    (void)pthread_mutex_lock(&g_drv_thread_lock);
    ret = drv_event_query_grid(devId, &grp_id);
    if (ret == DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&g_drv_thread_lock);
        return DRV_ERROR_NONE;
    }

    /* process attach device only need once */
    grp_para.type = GRP_TYPE_BIND_CP_CPU;
    grp_para.threadNum = 1;
    grp_para.rsv[0] = 0; // rsv word 0
    grp_para.rsv[1] = 0; // rsv word 1
    grp_para.rsv[2] = 0; // rsv word 2
    (void)strcpy_s(grp_para.grp_name, EVENT_MAX_GRP_NAME_LEN, EVENT_DRV_MSG_GRP_NAME);

    ret = halEschedCreateGrpEx(devId, &grp_para, &grp_id);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&g_drv_thread_lock);
        (void)halEschedDettachDevice(devId);
        DRV_EVENT_LOG_ERR("Call halEschedCreateGrpEx failed. (devId=%u, ret=%d)\n", devId, ret);
        return ret;
    }
    (void)pthread_mutex_unlock(&g_drv_thread_lock);
#endif
    ret = halEschedSubscribeEvent(devId, grp_id, 0, event_bitmap);
    if (ret != DRV_ERROR_NONE) {
        (void)halEschedDettachDevice(devId);
        DRV_EVENT_LOG_ERR("Call halEschedSubscribeEvent failed. (devId=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    (void)halEschedWaitEvent(devId, grp_id, 0, 0, &event); /* wait timeout 0s to register subscriber */
    ret = drv_event_create_task_with_detach(devId);
    if (ret != 0) {
        (void)halEschedDettachDevice(devId);
        DRV_EVENT_LOG_ERR("Call drv_event_create_task_with_detach failed. (devId=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t halDrvEventThreadUninit(unsigned int devId)
{
    return halEschedDettachDevice(devId);
}
