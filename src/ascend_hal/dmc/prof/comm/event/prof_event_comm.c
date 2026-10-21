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
#include "esched_user_interface.h"
#include "prof_event_comm.h"

static drvError_t prof_get_event_grpid(uint32_t dev_id, uint32_t remote_pid, uint32_t *grp_id)
{
    struct esched_query_gid_output gid_out = {0};
    struct esched_query_gid_input gid_in = {0};
    struct esched_output_info out_put = {0};
    struct esched_input_info in_put = {0};
    const char *group_name = EVENT_DRV_MSG_GRP_NAME;
    size_t grp_name_len;
    drvError_t drv_ret;
    int ret;

    if (remote_pid == 0) {
        *grp_id = 0;
        return DRV_ERROR_NONE;
    }

    gid_in.pid = (int)remote_pid;
    grp_name_len = strlen(group_name);
    ret = memcpy_s(gid_in.grp_name, EVENT_MAX_GRP_NAME_LEN, group_name, grp_name_len);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to memcpy_s. (ret=%d, size=%zu)\n", ret, grp_name_len);
        return ret;
    }

    in_put.inBuff = &gid_in;
    in_put.inLen = sizeof(struct esched_query_gid_input);
    out_put.outBuff = &gid_out;
    out_put.outLen = sizeof(struct esched_query_gid_output);
#ifdef DRV_HOST
    drv_ret = halEschedQueryInfo(dev_id, QUERY_TYPE_REMOTE_GRP_ID, &in_put, &out_put);
#else
    drv_ret = halEschedQueryInfo(dev_id, QUERY_TYPE_LOCAL_GRP_ID, &in_put, &out_put);
#endif
    if (drv_ret == DRV_ERROR_NONE) {
        *grp_id = gid_out.grp_id;
        PROF_INFO("Query grpid ok. (dev_id=%u, devpid=%d, group_name=%s, grp_id=%u)\n", dev_id, gid_in.pid, group_name,
                  *grp_id);
        return DRV_ERROR_NONE;
    }

    *grp_id = 0; // grp not exist, use default grpid 0.
    PROF_WARN("Query grpid unsuccessfully. (ret=%d, dev_id=%u, devpid=%d, group_name=%s).\n", ret, dev_id, gid_in.pid,
              group_name);
    return ret;
}

STATIC drvError_t prof_event_info_init(struct prof_event_para *para, struct event_summary *event_info)
{
    struct event_sync_msg *msg_head = NULL;
    uint32_t dev_id = para->dev_id;
    uint32_t remote_pid = para->remote_pid;
    struct prof_event_msg *msg_send = &para->msg_send;
    drvError_t ret;

    ret = prof_get_event_grpid(dev_id, remote_pid, &event_info->grp_id);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to get event grpid. (dev_id=%u, remote_pid=%u, ret=%d).\n", dev_id, remote_pid, (int)ret);
        return ret;
    }

    event_info->msg_len = (unsigned int)(sizeof(struct event_sync_msg) + msg_send->msg_len);
    event_info->msg = (char *)malloc(event_info->msg_len);
    if (event_info->msg == NULL) {
        PROF_ERR("Failed to alloc event msg. (size=%d, dev_id=%u)\n", event_info->msg_len, dev_id);
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    msg_head = (struct event_sync_msg *)event_info->msg;
#ifdef DRV_HOST
    msg_head->dst_engine = CCPU_HOST;
#else
    msg_head->dst_engine = CCPU_LOCAL;
#endif

    if ((msg_send->msg_len > 0) && (msg_send->msg != NULL)) {
        ret = memcpy_s(event_info->msg + sizeof(struct event_sync_msg), msg_send->msg_len, msg_send->msg,
                       msg_send->msg_len);
        if (ret != EOK) {
            free(event_info->msg);
            event_info->msg = NULL;
            PROF_ERR("Failed to copy event msg. (ret=%d, dev_id=%u, msg_len=%zu)\n", (int)ret, dev_id,
                     msg_send->msg_len);
            return DRV_ERROR_PARA_ERROR;
        }
    }
    event_info->dst_engine = CCPU_DEVICE;
    event_info->policy = ONLY;
    event_info->event_id = para->event_id;
    event_info->subevent_id = para->subevent_id;
    event_info->pid = (int)remote_pid;

    return DRV_ERROR_NONE;
}

STATIC void prof_event_info_uninit(struct event_summary *event_info)
{
    if (event_info->msg != NULL) {
        free(event_info->msg);
        event_info->msg = NULL;
    }
}

STATIC drvError_t prof_event_reply_init(struct prof_event_para *para, struct event_reply *reply)
{
    reply->buf_len = (uint32_t)(para->msg_recv.msg_len + PROF_EVENT_REPLY_BUFFER_RET_OFFSET);
    reply->buf = (char *)malloc(reply->buf_len);
    if (reply->buf == NULL) {
        PROF_ERR("Failed to alloc reply. (devid=%u, size=%u, remote_pid=%u)\n", para->dev_id, reply->buf_len,
                 para->remote_pid);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    return DRV_ERROR_NONE;
}

STATIC void prof_event_reply_uninit(struct event_reply *reply)
{
    if (reply->buf != NULL) {
        free(reply->buf);
        reply->buf = NULL;
    }
}

STATIC drvError_t prof_event_submit_sync_para_init(struct prof_event_para *para, struct event_summary *event_info,
                                                   struct event_reply *reply)
{
    drvError_t ret;

    ret = prof_event_info_init(para, event_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = prof_event_reply_init(para, reply);
    if (ret != DRV_ERROR_NONE) {
        prof_event_info_uninit(event_info);
    }

    return ret;
}

STATIC void prof_event_submit_sync_para_uninit(struct event_summary *event_info, struct event_reply *reply)
{
    prof_event_reply_uninit(reply);
    prof_event_info_uninit(event_info);
}

STATIC drvError_t prof_event_get_submit_sync_result(struct prof_event_para *para, struct event_reply *reply)
{
    struct prof_event_msg *msg_recv = &para->msg_recv;
    int ret;

    ret = prof_comm_errcode_convert(PROF_EVENT_REPLY_BUFFER_RET(reply->buf));
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            PROF_ERR("Failed to submit event. (result=%d, dev_id=%u, subevent_id=%u).\n", ret, para->dev_id,
                     para->subevent_id);
        }
        return ret;
    }

    if (reply->reply_len > reply->buf_len) {
        PROF_ERR("Reply len invalid. (ret=%d, dev_id=%u, remote_pid=%u, reply_len=%u, buf_len=%u)\n", ret, para->dev_id,
                 para->remote_pid, reply->reply_len, reply->buf_len);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((msg_recv->msg != NULL) && (msg_recv->msg_len != 0)) {
        ret = memcpy_s(msg_recv->msg, msg_recv->msg_len, PROF_EVENT_REPLY_BUFFER_DATA_PTR(reply->buf),
                       msg_recv->msg_len);
        if (ret != EOK) {
            PROF_ERR("Failed to copy out msg. (ret=%d, out_len=%ld)\n", ret, msg_recv->msg_len);
            return DRV_ERROR_MEMORY_OPT_FAIL;
        }
    }

    return ret;
}

drvError_t prof_event_submit_event_sync(struct prof_event_para *para)
{
    struct event_summary event_info = {0};
    struct event_reply reply;
    drvError_t ret;

    ret = prof_event_submit_sync_para_init(para, &event_info, &reply);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = halEschedSubmitEventSync(para->dev_id, &event_info, 10000, &reply); /* 10000 -> 10s */

    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to submit event. (ret=%d, dev_id=%u, remote_pid=%u, subevent_id=%u).\n", ret, para->dev_id,
                 para->remote_pid, event_info.subevent_id);
        goto out;
    }
    PROF_RUN_INFO("Prof submit event successfully.(dev_id=%u,remote_pid=%u,subevent_id=%u)\n", para->dev_id,
                  para->remote_pid, event_info.subevent_id);
    ret = prof_event_get_submit_sync_result(para, &reply);
out:
    prof_event_submit_sync_para_uninit(&event_info, &reply);
    return ret;
}

#else
int prof_event_comm_ut_test(void)
{
    return 0;
}
#endif
