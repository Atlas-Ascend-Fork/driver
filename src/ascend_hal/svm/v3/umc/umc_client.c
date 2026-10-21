/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <string.h>
#include <semaphore.h>
#include <stdint.h>

#include "securec.h"
#include "ascend_hal.h"

#include "esched_user_interface.h"

#include "svm_pub.h"
#include "svm_criu.h"
#include "svm_log.h"
#include "svm_user_adapt.h"
#include "svm_sub_event_type_uk_msg.h"
#include "svm_umc_client.h"

#define UMC_MAX_CONCURRENCY_NUM (3u)
#define UMC_TIMEOUT_MS (30u * 60u * 1000u)

static sem_t g_event_sem[SVM_MAX_DEV_AGENT_NUM][SVM_SUB_EVNET_H2D_TYPE_NUM];

static void __attribute__((constructor)) umc_client_init(void)
{
    u32 i, j;

    for (i = 0; i < SVM_MAX_DEV_AGENT_NUM; i++) {
        for (j = 0; j < SVM_SUB_EVNET_H2D_TYPE_NUM; j++) {
            (void)sem_init(&g_event_sem[i][j], 0, UMC_MAX_CONCURRENCY_NUM);
        }
    }
}

static sem_t *umc_get_sem(u32 devid, u32 subevent_id)
{
    return &g_event_sem[devid][subevent_id - SVM_SUB_EVNET_H2D_TYPE_BASE];
}

static bool umc_is_submit_to_local(u32 flag)
{
    return ((flag & UMC_TO_LOCAL) != 0);
}

static bool umc_is_submit_from_device(u32 flag)
{
    return ((flag & UMC_DEVICE_SUBMIT) != 0);
}

static u32 umc_flag_to_dst_engine(u32 flag)
{
    return umc_is_submit_to_local(flag) ? CCPU_LOCAL : (umc_is_submit_from_device(flag) ? CCPU_HOST : CCPU_DEVICE);
}

static int umc_fill_event_summary(struct svm_umc_msg_head *head, u32 flag, struct svm_umc_msg *msg,
                                  struct event_summary *event_info)
{
    int ret;

    event_info->grp_id = head->grp_id;
    event_info->dst_engine = umc_flag_to_dst_engine(flag);
    event_info->policy = ONLY;
    event_info->event_id = EVENT_DRV_MSG_EX;
    event_info->subevent_id = head->subevent_id;
    event_info->pid = head->pid;

    event_info->msg_len = (unsigned int)(sizeof(struct event_sync_msg)) + (unsigned int)msg->msg_in_len;
    event_info->msg = malloc(event_info->msg_len);
    if (event_info->msg == NULL) {
        svm_err("malloc failed. (size=%u)\n", event_info->msg_len);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    if (msg->msg_in_len == 0) {
        return DRV_ERROR_NONE;
    }

    ret = (int)memcpy_s(event_info->msg + sizeof(struct event_sync_msg), msg->msg_in_len, msg->msg_in, msg->msg_in_len);
    if (ret != DRV_ERROR_NONE) {
        svm_err("memcpy_s failed. (ret=%d).\n", ret);
        free(event_info->msg);
        event_info->msg = NULL;
        return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

static void umc_clear_event_summary(struct event_summary *event_info)
{
    free(event_info->msg);
    event_info->msg = NULL;
    event_info->msg_len = 0;
}

static int umc_fill_reply(struct svm_umc_msg *msg, struct event_reply *reply)
{
    u64 reply_len = msg->msg_out_len + sizeof(int);

    reply->buf = (char *)malloc(reply_len);
    if (reply->buf == NULL) {
        svm_err("Malloc reply buffer failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    reply->buf_len = (unsigned int)reply_len;
    return DRV_ERROR_NONE;
}

static int umc_parse_reply(struct event_reply *reply, struct svm_umc_msg *msg, int *reply_ret)
{
    u64 reply_len = msg->msg_out_len + sizeof(int);

    if (reply->reply_len != reply_len) {
        svm_err("Unexpected, reply_len != buf_len. (reply_len=%u; buf_len=%u)\n", reply->reply_len, reply->buf_len);
        return DRV_ERROR_INNER_ERR;
    }

    if ((msg->msg_out != NULL) && (msg->msg_out_len != 0)) {
        (void)memcpy_s(msg->msg_out, msg->msg_out_len, DRV_EVENT_REPLY_BUFFER_DATA_PTR(reply->buf), msg->msg_out_len);
    }

    *reply_ret = DRV_EVENT_REPLY_BUFFER_RET(reply->buf);
    return DRV_ERROR_NONE;
}

static void umc_clear_reply(struct event_reply *reply)
{
    free(reply->buf);
    reply->buf = NULL;
    reply->buf_len = 0;
}

int svm_umc_send(struct svm_umc_msg_head *head, u32 flag, struct svm_umc_msg *msg)
{
    struct event_summary event_info;
    struct event_reply reply;
    int ret, reply_ret;

    ret = umc_fill_event_summary(head, flag, msg, &event_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = umc_fill_reply(msg, &reply);
    if (ret != DRV_ERROR_NONE) {
        goto clear_summary;
    }

    if (umc_submit_sync_event(flag)) {
        ret = halEschedSubmitEventSync(head->devid, &event_info, UMC_TIMEOUT_MS, &reply);
    } else {
        ret = halEschedSubmitEvent(head->devid, &event_info);
    }

    if (ret != DRV_ERROR_NONE) {
        share_log_read_err(HAL_MODULE_TYPE_DEVMM);
        ret = (ret < 0) ? DRV_ERROR_INNER_ERR : ret; /* esched sometimes will return -ret. */
        goto clear_reply;
    }

    if (umc_submit_sync_event(flag)) {
        ret = umc_parse_reply(&reply, msg, &reply_ret);
        if (ret == DRV_ERROR_NONE) {
            ret = reply_ret;
        }
    }

clear_reply:
    umc_clear_reply(&reply);
clear_summary:
    umc_clear_event_summary(&event_info);
    return ret;
}

static int _svm_umc_h2d_send(struct svm_umc_msg_head *head, struct svm_umc_msg *msg, u32 flag)
{
    int ret;

    if (svm_criu_is_resetting(head->devid)) {
        if ((msg->msg_out != NULL) && (msg->msg_out_len != 0)) {
            (void)memset_s(msg->msg_out, msg->msg_out_len, 0, msg->msg_out_len);
        }
        return DRV_ERROR_NONE;
    }

    ret = sem_wait(umc_get_sem(head->devid, head->subevent_id));
    if (ret != 0) {
        svm_err("Sem wait failed. (errno=%d; errstr=%s)\n", errno, strerror(errno));
        return DRV_ERROR_INNER_ERR;
    }

    ret = svm_umc_send(head, flag, msg);
    (void)sem_post(umc_get_sem(head->devid, head->subevent_id));
    return ret;
}

int svm_umc_h2d_send(struct svm_umc_msg_head *head, struct svm_umc_msg *msg)
{
    return _svm_umc_h2d_send(head, msg, 0);
}

int svm_umc_h2d_send_async(struct svm_umc_msg_head *head, struct svm_umc_msg *msg)
{
    return _svm_umc_h2d_send(head, msg, UMC_USE_ASYNC);
}
