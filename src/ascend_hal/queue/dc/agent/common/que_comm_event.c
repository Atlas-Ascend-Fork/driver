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

#include "securec.h"
#include "ascend_hal_error.h"
#include "ascend_hal_define.h"
#include "event_sched.h"
#include "queue.h"
#include "que_compiler.h"
#include "queue_client_comm.h"
#include "que_comm_agent.h"
#include "que_comm_event.h"
#include "que_platform_ub.h"
#ifndef EMU_ST
static int que_comm_event_sum_init(struct que_comm_event_attr *attr, struct que_event_msg *msg,
                                   struct event_summary *event_sum)
{
    int ret;

    event_sum->msg_len = (unsigned int)(sizeof(struct event_sync_msg) + msg->in_len);
    event_sum->msg = (char *)malloc(event_sum->msg_len);
    if (que_unlikely(event_sum->msg == NULL)) {
        QUEUE_LOG_ERR("event msg alloc fail. (size=%d; devid=%u; remote_devid=%u; remote_pid=%d)\n", event_sum->msg_len,
                      attr->devid, attr->remote_devid, attr->remote_pid);
        return DRV_ERROR_QUEUE_OUT_OF_MEM;
    }

    if (msg->in_len > 0) {
        ret = memcpy_s(event_sum->msg + sizeof(struct event_sync_msg), msg->in_len, msg->in, msg->in_len);
        if (que_unlikely(ret != EOK)) {
            free(event_sum->msg);
            event_sum->msg = NULL;
            QUEUE_LOG_ERR("event msg copy fail. (ret=%d; devid=%u; remote_devid=%u; remote_pid=%d)\n", ret, attr->devid,
                          attr->remote_devid, attr->remote_pid);
            return DRV_ERROR_MEMORY_OPT_FAIL;
        }
    }
    event_sum->grp_id = attr->remote_grpid;
    event_sum->dst_engine = que_comm_event_get_dst_engine_platform(attr->remote_devid);
    event_sum->policy = ONLY;
    event_sum->event_id = EVENT_DRV_MSG;
    event_sum->subevent_id = attr->sub_event;
    event_sum->pid = attr->remote_pid;
    event_sum->tid = SCHED_INVALID_TID;

    return DRV_ERROR_NONE;
}

void que_comm_event_sum_uninit(struct event_summary *event)
{
    if (que_likely(event->msg != NULL)) {
        free(event->msg);
        event->msg = NULL;
    }
}

int que_event_reply_init(struct que_event_msg *msg, struct event_reply *reply)
{
    reply->buf_len = (unsigned int)(msg->out_len + sizeof(int));
    reply->buf = (char *)malloc(reply->buf_len);
    if (que_unlikely(reply->buf == NULL)) {
        QUEUE_LOG_ERR("reply buf alloc fail. (size=%u)\n", reply->buf_len);
        return DRV_ERROR_QUEUE_OUT_OF_MEM;
    }

    return DRV_ERROR_NONE;
}

void que_event_reply_uninit(struct event_reply *reply)
{
    if (que_likely(reply->buf != NULL)) {
        free(reply->buf);
        reply->buf = NULL;
    }
}

int que_event_get_result(struct que_event_msg *msg, struct event_reply *reply)
{
    if ((msg->out != NULL) && (msg->out_len != 0)) {
        int ret = memcpy_s(msg->out, msg->out_len, DRV_EVENT_REPLY_BUFFER_DATA_PTR(reply->buf), msg->out_len);
        if (que_unlikely(ret != EOK)) {
            QUEUE_LOG_ERR("msg out copy fail. (ret=%d; out_len=%ld)\n", ret, msg->out_len);
            return DRV_ERROR_MEMORY_OPT_FAIL;
        }
    }

    return DRV_EVENT_REPLY_BUFFER_RET(reply->buf);
}

drvError_t check_event_and_reply(struct event_summary *event, struct event_reply *reply)
{
    if (event == NULL || reply == NULL) {
        QUEUE_LOG_ERR("the variable event or reply is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (event->msg == NULL || event->msg_len < sizeof(struct event_sync_msg)) {
        QUEUE_LOG_ERR("the event->msg is NULL or event->msg_len is invalid. (msg_len=%u)\n", event->msg_len);
        return DRV_ERROR_PARA_ERROR;
    }
    return DRV_ERROR_NONE;
}

drvError_t wait_event_and_check(uint32_t devid, struct event_res *res, struct event_info *back_event_info,
                                int32_t timeout, unsigned long timestamp)
{
    drvError_t ret;
    int wait_succ_cnt = 0;
    struct event_info event_info_tmp = {0};

    do {
        ret = (int)esched_wait_event_ex(devid, res->gid, res->tid, timeout, back_event_info);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("esched wait failed. (ret=%d; event_id=%u; gid=%u; tid=%u; timeout=%dms; subevent_id=%u)\n",
                          ret, res->event_id, res->gid, res->tid, timeout, res->subevent_id);
            return ret;
        }

        wait_succ_cnt++;
        if ((back_event_info->comm.submit_timestamp >= timestamp) &&
            (back_event_info->comm.subevent_id == res->subevent_id)) {
            (void)halEschedWaitEvent(devid, res->gid, res->tid, 0, &event_info_tmp);
            break;
        }

        QUEUE_LOG_WARN("successfully waited for an event but the condition was not met. (devid=%u; gid=%u; "
                       "tid=%u; cnt=%d; check_time=%lu; back_time=%llu; check_subevent=%u; back_subevent=%u)\n",
                       devid, res->gid, res->tid, wait_succ_cnt, timestamp, back_event_info->comm.submit_timestamp,
                       res->subevent_id, back_event_info->comm.subevent_id);
    } while (1);
    return DRV_ERROR_NONE;
}

void que_get_sched_event_type(uint32_t devid, int32_t *event_type)
{
    que_get_sched_event_type_platform(devid, event_type);
}

unsigned int que_get_sched_engine_type(uint32_t devid)
{
    return que_get_sched_engine_type_platform(devid);
}

static drvError_t que_comm_esched_submit_event_sync(uint32_t devid, unsigned int remote_devid,
                                                    struct event_summary *event, int32_t timeout,
                                                    struct event_reply *reply)
{
    struct event_info back_event_info = {{0}, {0}};
    esched_event_buffer *event_buffer = (esched_event_buffer *)back_event_info.priv.msg;
    struct event_res res;
    unsigned long timestamp = esched_get_cur_cpu_timestamp();
    drvError_t ret;
    uint32_t phy_devid = que_get_unified_devid(devid);
    int32_t event_type;

    if (check_event_and_reply(event, reply) != DRV_ERROR_NONE) {
        return DRV_ERROR_PARA_ERROR;
    }

    que_get_sched_event_type(devid, &event_type);
    ret = esched_alloc_event_res(devid, event_type, &res);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("failed to invoke the esched_alloc_event_res. (ret=%d)\n", ret);
        return ret;
    }

    esched_fill_sync_msg_for_peer_que(devid, phy_devid, event, &res);
    ret = (int)halEschedSubmitEventEx(devid, remote_devid, event);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
#ifndef EMU_ST
        /* flowgw maybe not ready, client will retry. */
        QUEUE_LOG_WARN(
            "unable to invoke the halEschedSubmitEventEx. (devid=%u, remote_devid=%u; event_id=%u; ret=%d)\n", devid,
            remote_devid, event->event_id, ret);
#endif
        esched_free_event_res(devid, event_type, &res);
        return ret;
    }

    if (event_type == QUEUE_ACPU_EVENT) {
        ret = halEschedThreadSwapout(devid, SCHED_INVALID_GID, SCHED_INVALID_TID);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR(
                "current thread swapout fail. (devid=%u, remote_devid=%u; event_id=%u; subevent_id=%u; ret=%d)\n",
                devid, remote_devid, event->event_id, event->subevent_id, ret);
            esched_free_event_res(devid, event_type, &res);
            return ret;
        }
    }

    event_buffer->msg = reply->buf;
    event_buffer->msg_len = reply->buf_len;
    ret = wait_event_and_check(devid, &res, &back_event_info, timeout, timestamp);
    if (ret == DRV_ERROR_WAIT_TIMEOUT) {
        esched_query_sync_msg_trace(remote_devid, event, res.gid, res.tid);
    }

    esched_free_event_res(devid, event_type, &res);

    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }

    reply->reply_len = back_event_info.priv.msg_len;
    return DRV_ERROR_NONE;
}

int que_comm_event_send(struct que_comm_event_attr *attr, struct que_event_msg *msg, int timeout_ms)
{
    struct event_summary event_sum = {0};
    struct event_reply reply;
    int ret;

    ret = que_comm_event_sum_init(attr, msg, &event_sum);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("event summary init fail. (ret=%d; devid=%u; local_pid=%d; remote_pid=%d; sub_event=%u)\n", ret,
                      attr->devid, attr->local_pid, attr->remote_pid, attr->sub_event);
        return ret;
    }

    ret = que_event_reply_init(msg, &reply);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        que_comm_event_sum_uninit(&event_sum);
        QUEUE_LOG_ERR("event reply init fail. (ret=%d; devid=%u; local_pid=%d; remote_pid=%d; sub_event=%u)\n", ret,
                      attr->devid, attr->local_pid, attr->remote_pid, attr->sub_event);
        return ret;
    }

    ret = que_comm_esched_submit_event_sync(attr->devid, attr->remote_devid, &event_sum, timeout_ms, &reply);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR(
            "submit event fail. (ret=%d; devid=%u; remote_devid=%u; local_pid=%d; remote_pid=%d; sub_event=%u)\n", ret,
            attr->devid, attr->remote_devid, attr->local_pid, attr->remote_pid, attr->sub_event);
        ret = DRV_ERROR_INNER_ERR;
        goto out;
    }

    ret = que_event_get_result(msg, &reply);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        /* It is not recommended to add error logs when the return value is DRV_ERROR_QUEUE_EMPTY or
        DRV_ERROR_QUEUE_FULL, as this will cause the error logs to be flushed. */
        goto out;
    }

    ret = (reply.reply_len != reply.buf_len) ? DRV_ERROR_PARA_ERROR : DRV_ERROR_NONE;
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("reply len invalid. (ret=%d; devid=%u; local_pid=%d; remote_pid=%d; sub_event=%u; "
                      "reply_len=%u; buf_len=%u)\n",
                      ret, attr->devid, attr->local_pid, attr->remote_pid, attr->sub_event, reply.reply_len,
                      reply.buf_len);
    }
out:
    que_event_reply_uninit(&reply);
    que_comm_event_sum_uninit(&event_sum);
    return ret;
}

int que_inter_dev_get_remote_pid(unsigned int remote_devid, pid_t *remote_pid)
{
    return que_inter_dev_get_remote_pid_platform(remote_devid, remote_pid);
}

int que_inter_dev_get_remote_grpid(unsigned int devid, unsigned int remote_devid, pid_t remote_pid, unsigned int *grpid)
{
    struct esched_query_gid_output gid_out = {0};
    struct esched_query_gid_input gid_in = {0};
    struct esched_output_info out = {0};
    struct esched_input_info in = {0};
    int ret;

    gid_in.pid = remote_pid;
    ret = strcpy_s(gid_in.grp_name, EVENT_MAX_GRP_NAME_LEN, EVENT_DRV_MSG_GRP_NAME);
    if (que_unlikely(ret != EOK)) {
        QUEUE_LOG_ERR("grp name copy fail. (remote devid=%u; remote pid=%d; name_len=%ld)\n", remote_devid, remote_pid,
                      strlen(EVENT_DRV_MSG_GRP_NAME));
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    in.inBuff = &gid_in;
    in.inLen = (unsigned int)sizeof(struct esched_query_gid_input);
    out.outBuff = &gid_out;
    out.outLen = (unsigned int)sizeof(struct esched_query_gid_output);

    ret = halEschedQueryInfoEx(devid, remote_devid, QUERY_TYPE_REMOTE_GRP_ID, &in, &out);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que event query info fail. (remote devid=%u; remote pid=%d)\n", remote_devid, remote_pid);
    } else {
        *grpid = gid_out.grp_id;
    }

    return ret;
}

int que_get_local_devid_grpid(unsigned int devid, pid_t *devpid, unsigned int *grpid)
{
    struct esched_query_gid_output gid_out = {0};
    struct esched_query_gid_input gid_in = {0};
    struct esched_output_info out = {0};
    struct esched_input_info in = {0};
    int ret;

    gid_in.pid = getpid();
    ret = strcpy_s(gid_in.grp_name, EVENT_MAX_GRP_NAME_LEN, EVENT_DRV_MSG_GRP_NAME);
    if (que_unlikely(ret != EOK)) {
        QUEUE_LOG_ERR("grp name copy fail. (local_devid=%u; local pid=%d; name_len=%ld)\n", devid, gid_in.pid,
                      strlen(EVENT_DRV_MSG_GRP_NAME));
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    in.inBuff = &gid_in;
    in.inLen = (unsigned int)sizeof(struct esched_query_gid_input);
    out.outBuff = &gid_out;
    out.outLen = (unsigned int)sizeof(struct esched_query_gid_output);

    ret = halEschedQueryInfo(devid, QUERY_TYPE_LOCAL_GRP_ID, &in, &out);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que event query info fail. (local_devid=%u; local_pid=%d)\n", devid, gid_in.pid);
        return ret;
    }

    *devpid = gid_in.pid;
    *grpid = gid_out.grp_id;
    return ret;
}

int que_comm_event_send_ex(unsigned int devid, unsigned int qid, unsigned int sub_event, struct que_event_msg *msg,
                           int timeout_ms)
{
    struct que_comm_event_attr attr = {.devid = devid, .sub_event = sub_event, .local_pid = getpid()};
    int ret;

    ret = que_get_peer_proc_info(devid, qid, &attr.remote_pid, &attr.remote_devid, &attr.remote_grpid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que get peer proc info fail. (remote devid=%u; ret=%d)\n", attr.remote_devid, ret);
        return ret;
    }

    ret = que_comm_event_send(&attr, msg, timeout_ms);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }
    return DRV_ERROR_NONE;
}

int que_inter_dev_get_info(unsigned int devid, unsigned int remote_devid, pid_t *remote_devpid,
                           unsigned int *remote_grpid)
{
    int ret;

    ret = que_inter_dev_get_remote_pid(remote_devid, remote_devpid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("get remote pid failed. (ret=%d; devid=%u)\n", ret, remote_devid);
        return ret;
    }

    ret = que_inter_dev_get_remote_grpid(devid, remote_devid, *remote_devpid, remote_grpid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("get remote grpid failed. (ret=%d; devid=%u; pid=%d)\n", ret, remote_devid, *remote_devpid);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int que_comm_event_send_d2d(unsigned int devid, unsigned int remote_devid, unsigned int sub_event,
                            struct que_event_msg *msg, int timeout_ms)
{
    struct que_comm_event_attr attr = {.devid = halGetHostDevid(), .sub_event = sub_event, .local_pid = getpid()};
    int ret;

    ret = uda_get_udevid_by_devid_ex(remote_devid, &attr.remote_devid);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("get udevid failed. (remote_devid=%u)\n", remote_devid);
        return ret;
    }

    ret = que_inter_dev_get_info(devid, remote_devid, &attr.remote_pid, &attr.remote_grpid);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que inter dev import send failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = que_comm_event_send(&attr, msg, timeout_ms);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }
    return DRV_ERROR_NONE;
}
#else /* EMU_ST */

void que_clt_event_emu_test(void)
{}

#endif /* EMU_ST */
