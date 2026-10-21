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
#include "esched_user_interface.h"
#include "event_sched.h"
#include "queue.h"
#include "que_ctx.h"
#include "que_compiler.h"
#include "queue_client_comm.h"
#include "queue_interface.h"
#include "que_event.h"
#ifndef EMU_ST

typedef enum sched_grp_type {
    EVENT_DRV_MSG_GRP = 0,
    PROXY_HOST_QUEUE_GRP,
    GRP_TYPE_BUTT
} sched_grp_type_t;

static int g_grpid[MAX_DEVICE][GRP_TYPE_BUTT];
static pthread_mutex_t g_event_lock = PTHREAD_MUTEX_INITIALIZER;

void que_init_grpid_by_dev(int devid)
{
    for (int grp_type = 0; grp_type < GRP_TYPE_BUTT; grp_type++) {
        g_grpid[devid][grp_type] = -1;
    }
}

void que_init_grpid(void)
{
    for (int devid = 0; devid < MAX_DEVICE; devid++) {
        que_init_grpid_by_dev(devid);
    }
}

static int _que_event_get_grpid(unsigned int devid, pid_t devpid, unsigned int grp_type, unsigned int *grpid)
{
    struct esched_query_gid_output gid_out = {0};
    struct esched_query_gid_input gid_in = {0};
    struct esched_output_info out = {0};
    struct esched_input_info in = {0};
    size_t name_len[GRP_TYPE_BUTT] = {strlen(EVENT_DRV_MSG_GRP_NAME), strlen(PROXY_HOST_QUEUE_GRP_NAME)};
    const char *grp_name[GRP_TYPE_BUTT] = {EVENT_DRV_MSG_GRP_NAME, PROXY_HOST_QUEUE_GRP_NAME};
    int ret;

    gid_in.pid = devpid;
    ret = memcpy_s(gid_in.grp_name, EVENT_MAX_GRP_NAME_LEN, grp_name[grp_type], name_len[grp_type]);
    if (que_unlikely(ret != EOK)) {
        QUEUE_LOG_ERR("grp name copy fail. (devid=%u; devpid=%d; name_len=%ld)\n", devid, devpid, name_len[grp_type]);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    in.inBuff = &gid_in;
    in.inLen = (unsigned int)sizeof(struct esched_query_gid_input);
    out.outBuff = &gid_out;
    out.outLen = (unsigned int)sizeof(struct esched_query_gid_output);
    ret = halEschedQueryInfo(devid, QUERY_TYPE_REMOTE_GRP_ID, &in, &out);
    if (ret == DRV_ERROR_NONE) {
        *grpid = gid_out.grp_id;
        return DRV_ERROR_NONE;
    } else if (ret == DRV_ERROR_UNINIT) {
        *grpid = 0; // PROXY_HOST_GRP_NAME grp not exist, use default grpid 0.
        return DRV_ERROR_NONE;
    }

    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que event query info fail. (devid=%u; devpid=%d)\n", devid, devpid);
    }

    return ret;
}

static int que_event_get_grpid(unsigned int devid, pid_t devpid, unsigned int grp_type, unsigned int *grpid)
{
    if (que_unlikely(devid >= MAX_DEVICE)) {
        QUEUE_LOG_ERR("invalid devid. (devid=%u; max_devid=%u)\n", devid, (unsigned int)MAX_DEVICE);
        return DRV_ERROR_PARA_ERROR;
    }

    if (g_grpid[devid][grp_type] != -1) {
        *grpid = (unsigned int)g_grpid[devid][grp_type];
        return DRV_ERROR_NONE;
    }

    (void)pthread_mutex_lock(&g_event_lock);
    if (g_grpid[devid][grp_type] != -1) {
        *grpid = (unsigned int)g_grpid[devid][grp_type];
        (void)pthread_mutex_unlock(&g_event_lock);
        return DRV_ERROR_NONE;
    }

    if (g_grpid[devid][grp_type] == -1) {
        int ret = _que_event_get_grpid(devid, devpid, grp_type, grpid);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            (void)pthread_mutex_unlock(&g_event_lock);
            QUEUE_LOG_ERR("get grpid fail. (ret=%d; devid=%u; devpid=%d)\n", ret, devid, devpid);
            return ret;
        }
        g_grpid[devid][grp_type] = (int)*grpid;
    }
    (void)pthread_mutex_unlock(&g_event_lock);

    return DRV_ERROR_NONE;
}

int que_event_sum_init(struct que_event_attr *attr, struct que_event_msg *msg, struct event_summary *event_sum)
{
    int ret;
    /* DRV_SUBEVENT_CREATE_MSG msg len is greater than sqe payload length 40byte */
    unsigned int grp_type = ((attr->sub_event != DRV_SUBEVENT_CREATE_MSG) &&
                             (attr->sub_event != DRV_SUBEVENT_QUEUE_INIT_MSG) &&
                             (attr->sub_event != DRV_SUBEVENT_QUEUE_ALIVE_MSG) &&
                             (attr->sub_event != DRV_SUBEVENT_ATTACH_INTER_DEV_MSG)) ?
                                PROXY_HOST_QUEUE_GRP :
                                EVENT_DRV_MSG_GRP;

    ret = que_event_get_grpid(attr->devid, attr->devpid, grp_type, &event_sum->grp_id);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("get grpid fail. (ret=%d; devid=%u; devpid=%d)\n", ret, attr->devid, attr->devpid);
        return ret;
    }

    event_sum->msg_len = (unsigned int)(sizeof(struct event_sync_msg) + msg->in_len);
    event_sum->msg = (char *)malloc(event_sum->msg_len);
    if (que_unlikely(event_sum->msg == NULL)) {
        QUEUE_LOG_ERR("event msg alloc fail. (size=%d; devid=%u; devpid=%d)\n", event_sum->msg_len, attr->devid,
                      attr->devpid);
        return DRV_ERROR_QUEUE_OUT_OF_MEM;
    }

    if (msg->in_len > 0) {
        ret = memcpy_s(event_sum->msg + sizeof(struct event_sync_msg), msg->in_len, msg->in, msg->in_len);
        if (que_unlikely(ret != EOK)) {
            free(event_sum->msg);
            event_sum->msg = NULL;
            QUEUE_LOG_ERR("event msg copy fail. (ret=%d; devid=%u; devpid=%d)\n", ret, attr->devid, attr->devpid);
            return DRV_ERROR_MEMORY_OPT_FAIL;
        }
    }
    event_sum->dst_engine = (grp_type == PROXY_HOST_QUEUE_GRP) ? ACPU_DEVICE : CCPU_DEVICE;
    event_sum->policy = ONLY;
    event_sum->event_id = EVENT_DRV_MSG;
    event_sum->subevent_id = attr->sub_event;
    event_sum->pid = attr->devpid;

    return DRV_ERROR_NONE;
}

void que_event_sum_uninit(struct event_summary *event)
{
    que_comm_event_sum_uninit(event);
}

drvError_t que_event_sync_send(uint32_t dev_id, int32_t timeout, struct event_summary *event, struct event_res *res,
                               struct event_info *back_event_info)
{
    drvError_t ret;
    unsigned long timestamp = esched_get_cur_cpu_timestamp();

    ret = (int)halEschedSubmitEvent(dev_id, event);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
#ifndef EMU_ST
        /* flowgw maybe not ready, client will retry. */
        QUEUE_LOG_WARN("unable to invoke the halEschedSubmitEvent. (dev_id=%u, event_id=%u; ret=%d)\n", dev_id,
                       event->event_id, ret);
#endif
        return ret;
    }
    return wait_event_and_check(dev_id, res, back_event_info, timeout, timestamp);
}

static drvError_t que_esched_submit_event_sync(uint32_t dev_id, unsigned int retry_flg, struct event_summary *event,
                                               int32_t timeout, struct event_reply *reply)
{
    struct event_info back_event_info;
    esched_event_buffer *event_buffer = (esched_event_buffer *)back_event_info.priv.msg;
    struct event_res res;
    drvError_t ret;

    ret = check_event_and_reply(event, reply);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }

    ret = esched_alloc_event_res(dev_id, QUEUE_EVENT, &res);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("failed to invoke the esched_alloc_event_res. (ret=%d)\n", ret);
        return ret;
    }

    esched_fill_sync_msg(event, &res);

    event_buffer->msg = reply->buf;
    event_buffer->msg_len = reply->buf_len;

    ret = que_event_sync_send(dev_id, timeout, event, &res, &back_event_info);
    if ((retry_flg == QUE_SEND_WITH_RETRY) && (ret == DRV_ERROR_WAIT_TIMEOUT)) {
        ret = que_event_sync_send(dev_id, QUE_EVENT_MAX_WAIT_10S, event, &res, &back_event_info);
    }

    if (ret == DRV_ERROR_WAIT_TIMEOUT) {
        esched_query_sync_msg_trace(dev_id, event, res.gid, res.tid);
    }

    esched_free_event_res(dev_id, QUEUE_EVENT, &res);

    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }
    reply->reply_len = back_event_info.priv.msg_len;

    return DRV_ERROR_NONE;
}
int que_event_send(struct que_event_attr *attr, struct que_event_msg *msg, int timeout_ms)
{
    struct event_summary event_sum = {0};
    struct event_reply reply;
    int ret;

    ret = que_event_sum_init(attr, msg, &event_sum);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("event summary init fail. (ret=%d; devid=%u; hostpid=%d; devpid=%d; sub_event=%u)\n", ret,
                      attr->devid, attr->hostpid, attr->devpid, attr->sub_event);
        return ret;
    }

    ret = que_event_reply_init(msg, &reply);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        que_event_sum_uninit(&event_sum);
        QUEUE_LOG_ERR("event reply init fail. (ret=%d; devid=%u; hostpid=%d; devpid=%d; sub_event=%u)\n", ret,
                      attr->devid, attr->hostpid, attr->devpid, attr->sub_event);
        return ret;
    }

    ret = que_esched_submit_event_sync(attr->devid, attr->retry_flg, &event_sum, timeout_ms, &reply);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("submit event fail. (ret=%d; devid=%u; hostpid=%d; devpid=%d; sub_event=%u)\n", ret, attr->devid,
                      attr->hostpid, attr->devpid, attr->sub_event);
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
        QUEUE_LOG_ERR("reply len invalid. (ret=%d; devid=%u; hostpid=%d; devpid=%d; sub_event=%u; "
                      "reply_len=%u; buf_len=%u)\n",
                      ret, attr->devid, attr->hostpid, attr->devpid, attr->sub_event, reply.reply_len, reply.buf_len);
    }
out:
    que_event_reply_uninit(&reply);
    que_event_sum_uninit(&event_sum);
    return ret;
}

int que_event_send_ex(unsigned int devid, unsigned int retry_flg, unsigned int sub_event, struct que_event_msg *msg,
                      int timeout_ms)
{
    struct que_event_attr attr = {.devid = devid, .sub_event = sub_event, .retry_flg = retry_flg};
    int ret;

    ret = que_ctx_get_pids(devid, &attr.hostpid, &attr.devpid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ctx get pids fail. (ret=%d; devid=%u; sub_event=%u; timeout_ms=%dms)\n", ret, devid,
                      sub_event, timeout_ms);
        return ret;
    }
    ret = que_event_send(&attr, msg, timeout_ms);
    return ret;
}

int que_clt_send_event_with_wait(unsigned int devid, unsigned int qid, unsigned int sub_event,
                                 struct que_event_msg *msg, int timeout_ms)
{
    int ret, wait_ret;
    int timeout_ms_tmp = timeout_ms;

    while (1) {
        struct timeval start, end;

        ret = que_event_send_ex(devid, QUE_SEND_NORMAL, sub_event, msg, QUE_EVENT_TIMEOUT_MS);
        if (ret != (int)DRV_ERROR_QUEUE_EMPTY) {
            break;
        }

        if (!que_is_need_sync_wait(devid, qid, sub_event)) {
            break;
        }

        que_get_time(&start);
        wait_ret = queue_wait_event(devid, qid, ret, timeout_ms_tmp);
        que_get_time(&end);
        if (que_unlikely(wait_ret != DRV_ERROR_NONE)) {
            break;
        }

        queue_updata_timeout(start, end, &timeout_ms_tmp);
    };

    return ret;
}

#else /* EMU_ST */

void que_clt_event_emu_test(void)
{}

#endif /* EMU_ST */
