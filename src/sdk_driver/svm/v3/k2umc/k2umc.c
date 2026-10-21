/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "ka_base_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"

#include "pbl_feature_loader.h"
#include "securec.h"
#include "esched_kernel_interface.h"

#include "framework_cmd.h"
#include "smm_uk_msg.h"
#include "svm_um_handle.h"
#include "svm_kern_log.h"
#include "svm_idr.h"
#include "svm_slab.h"
#include "k2umc.h"

#define SVM_K2UMC_SYNC_TIMEOUT_MS 300000
#define SVM_K2UMC_SYNC_MAX_NUM 4096

struct svm_k2umc_msg_sync {
    struct event_sync_msg sync_head;
    char msg[0];
};

struct svm_k2umc_sync_res {
    ka_completion_t completion;
};

static KA_TASK_DEFINE_MUTEX(g_svm_k2umc_sync_mutex);
static struct svm_idr g_svm_k2umc_sync_idr;
static bool g_svm_k2umc_sync_inited = false;

static int k2umc_sync_handle_to_id(u64 sync_handle)
{
    return (int)(sync_handle - 1);
}

static u32 k2umc_id_to_sync_handle(int id)
{
    return (u32)id + 1;
}

static void k2umc_sync_res_release(void *priv)
{
    svm_kfree(priv);
}

static int k2umc_sync_res_create(struct svm_k2umc_sync_res **res_out, u32 *sync_id)
{
    struct svm_k2umc_sync_res *res = NULL;
    int id;
    int ret;

    res = (struct svm_k2umc_sync_res *)svm_kzalloc(sizeof(*res), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (res == NULL) {
        return -ENOMEM;
    }
    ka_task_init_completion(&res->completion);

    ka_task_mutex_lock(&g_svm_k2umc_sync_mutex);
    if (!g_svm_k2umc_sync_inited) {
        ka_task_mutex_unlock(&g_svm_k2umc_sync_mutex);
        svm_kfree(res);
        return -ENODEV;
    }

    ret = svm_idr_alloc(&g_svm_k2umc_sync_idr, res, &id);
    ka_task_mutex_unlock(&g_svm_k2umc_sync_mutex);
    if (ret != 0) {
        svm_kfree(res);
        return ret;
    }

    *res_out = res;
    *sync_id = k2umc_id_to_sync_handle(id);
    return 0;
}

static void k2umc_sync_res_destroy(u32 sync_id)
{
    struct svm_k2umc_sync_res *res = NULL;

    if ((sync_id == 0) || (sync_id > SVM_K2UMC_SYNC_MAX_NUM)) {
        return;
    }

    ka_task_mutex_lock(&g_svm_k2umc_sync_mutex);
    if (g_svm_k2umc_sync_inited) {
        res = (struct svm_k2umc_sync_res *)svm_idr_remove(&g_svm_k2umc_sync_idr, k2umc_sync_handle_to_id(sync_id));
    }
    ka_task_mutex_unlock(&g_svm_k2umc_sync_mutex);
    if (res != NULL) {
        svm_kfree(res);
    }
}

static int k2umc_wait(struct svm_k2umc_sync_res *res)
{
    unsigned long timeout = ka_system_msecs_to_jiffies(SVM_K2UMC_SYNC_TIMEOUT_MS);
    long wait_ret;

    wait_ret = ka_task_wait_for_completion_interruptible_timeout(&res->completion, timeout);
    if (wait_ret <= 0) {
        return (wait_ret < 0) ? wait_ret : DRV_ERROR_WAIT_TIMEOUT;
    }

    return 0;
}

static int k2umc_pack_sync_head(struct event_sync_msg *sync_head, struct svm_k2umc_desc *dst_desc, u32 flag)
{
    unsigned int response_gid = 0;
    int ret;

    /* This header routes the user-mode response back to the kernel-side requester. */
    if ((flag & K2UMC_FLAG_SYNC) != 0) {
        ret = sched_query_local_task_gid(dst_desc->request_udevid, dst_desc->response_pid, EVENT_DRV_MSG_GRP_NAME,
                                         &response_gid);
        if (ret != 0) {
            svm_err("Query k2umc response gid failed. (ret=%d; request_udevid=%u; response_pid=%d; subevent_id=%u)\n",
                    ret, dst_desc->request_udevid, dst_desc->response_pid, dst_desc->subevent_id);
            return ret;
        }
        sync_head->gid = response_gid;
    }
    sync_head->dev_id = dst_desc->request_udevid;
    sync_head->pid = dst_desc->response_pid;
    sync_head->dst_engine = ((flag & K2UMC_FLAG_TO_DEVICE) != 0) ? CCPU_HOST : CCPU_DEVICE;
    sync_head->tid = 0;
    sync_head->event_id = EVENT_DRV_MSG_EX;
    sync_head->subevent_id = dst_desc->subevent_id;
    return 0;
}

static void k2umc_pack_event_info(struct sched_published_event_info *event_info, struct svm_k2umc_desc *dst_desc,
                                  u32 flag)
{
    /* The published event carries the request to the target user-mode process. */
    event_info->dst_engine = ((flag & K2UMC_FLAG_TO_DEVICE) != 0) ? CCPU_DEVICE : CCPU_HOST;
    event_info->policy = 0;
    event_info->pid = dst_desc->request_pid;
    event_info->gid = SVM_INVALID_EVENT_GID;
    event_info->event_id = EVENT_DRV_MSG_EX;
    event_info->subevent_id = dst_desc->subevent_id;
}

static int k2umc_pack_event(struct svm_k2umc_desc *dst_desc, u32 flag, char *msg, u64 msg_len,
                            struct sched_published_event *publish_event, struct svm_k2umc_msg_sync **msg_sync)
{
    u64 total_len;
    int ret;

    if (msg_len > (U32_MAX - sizeof(**msg_sync))) {
        svm_debug("Invalid k2umc msg length. (msg_len=%llu; max_len=%u)\n", msg_len, U32_MAX - (u32)sizeof(**msg_sync));
        return -EINVAL;
    }
    total_len = sizeof(**msg_sync) + msg_len;

    *msg_sync = (struct svm_k2umc_msg_sync *)svm_kzalloc(total_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (*msg_sync == NULL) {
        svm_debug("Alloc k2umc event msg failed, no memory. (total_len=%llu)\n", total_len);
        return -ENOMEM;
    }

    ret = k2umc_pack_sync_head(&(*msg_sync)->sync_head, dst_desc, flag);
    if (ret != 0) {
        svm_kfree(*msg_sync);
        *msg_sync = NULL;
        return ret;
    }
    if ((msg_len != 0) && (memcpy_s((*msg_sync)->msg, msg_len, msg, msg_len) != EOK)) {
        svm_debug("Copy k2umc event msg failed. (msg_len=%llu)\n", msg_len);
        svm_kfree(*msg_sync);
        *msg_sync = NULL;
        return -EINVAL;
    }

    k2umc_pack_event_info(&publish_event->event_info, dst_desc, flag);
    publish_event->event_info.msg = (char *)(*msg_sync);
    publish_event->event_info.msg_len = (u32)total_len;
    publish_event->event_func.event_finish_func = NULL;
    publish_event->event_func.event_ack_func = NULL;
    svm_debug("Pack k2umc event. (request_udevid=%u; request_pid=%d; response_pid=%d; flag=0x%x; "
              "subevent_id=%u; msg_len=%llu)\n",
              dst_desc->request_udevid, dst_desc->request_pid, dst_desc->response_pid, flag, dst_desc->subevent_id,
              msg_len);
    return 0;
}

static int k2umc_submit_event(struct svm_k2umc_desc *dst_desc, u32 flag, char *msg, u64 msg_len)
{
    struct sched_published_event publish_event = {0};
    struct svm_k2umc_msg_sync *msg_sync = NULL;
    int ret;

    ret = k2umc_pack_event(dst_desc, flag, msg, msg_len, &publish_event, &msg_sync);
    if (ret != 0) {
        return ret;
    }

    ret = hal_kernel_sched_submit_event(dst_desc->request_udevid, &publish_event);
    if (ret == SCHED_EVENT_PRE_PROC_SUCCESS_RETURN) {
        ret = 0;
    }
    svm_kfree(msg_sync);
    return ret;
}

int svm_k2umc_send(struct svm_k2umc_desc *dst_desc, u32 flag, char *msg, u64 msg_len)
{
    struct svm_uk_msg_head *msg_head = (struct svm_uk_msg_head *)msg;
    struct svm_k2umc_sync_res *sync_res = NULL;
    u32 sync_id = 0;
    int ret;

    if ((dst_desc == NULL) || ((msg == NULL) && (msg_len != 0))) {
        return -EINVAL;
    }

    if ((flag & K2UMC_FLAG_SYNC) != 0) {
        if (msg_len < sizeof(*msg_head)) {
            return -EINVAL;
        }

        ret = k2umc_sync_res_create(&sync_res, &sync_id);
        if (ret != 0) {
            return ret;
        }
        msg_head->k2u_sync_handle = sync_id;
    }

    ret = k2umc_submit_event(dst_desc, flag, msg, msg_len);
    if ((ret == 0) && (sync_res != NULL)) {
        ret = k2umc_wait(sync_res);
    } else if (ret != 0) {
        svm_debug("Submit k2umc event failed. (ret=%d; sync_id=%u; request_udevid=%u; request_pid=%d; "
                  "subevent_id=%u)\n",
                  ret, sync_id, dst_desc->request_udevid, dst_desc->request_pid, dst_desc->subevent_id);
    }

    k2umc_sync_res_destroy(sync_id);
    return ret;
}

void svm_k2umc_finish(u64 k2u_sync_handle)
{
    struct svm_k2umc_sync_res *res = NULL;
    int id;

    if ((k2u_sync_handle == 0) || (k2u_sync_handle > SVM_K2UMC_SYNC_MAX_NUM)) {
        svm_debug("Ignore invalid k2umc sync id. (sync_id=%llu)\n", k2u_sync_handle);
        return;
    }
    id = k2umc_sync_handle_to_id(k2u_sync_handle);

    ka_task_mutex_lock(&g_svm_k2umc_sync_mutex);
    if (g_svm_k2umc_sync_inited) {
        res = (struct svm_k2umc_sync_res *)ka_base_idr_find(&g_svm_k2umc_sync_idr.idr, id);
    }
    if (res == NULL) {
        ka_task_mutex_unlock(&g_svm_k2umc_sync_mutex);
        svm_debug("K2umc sync resource not found. (sync_id=%llu)\n", k2u_sync_handle);
        return;
    }

    /* Complete under the idr lock, so destroy cannot free the waiter concurrently. */
    ka_task_complete(&res->completion);
    ka_task_mutex_unlock(&g_svm_k2umc_sync_mutex);
}

int svm_k2umc_init(void)
{
    ka_task_mutex_lock(&g_svm_k2umc_sync_mutex);
    if (g_svm_k2umc_sync_inited) {
        ka_task_mutex_unlock(&g_svm_k2umc_sync_mutex);
        return 0;
    }

    svm_idr_init(SVM_K2UMC_SYNC_MAX_NUM, &g_svm_k2umc_sync_idr);
    g_svm_k2umc_sync_inited = true;
    ka_task_mutex_unlock(&g_svm_k2umc_sync_mutex);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_k2umc_init, FEATURE_LOADER_STAGE_3);

void svm_k2umc_uninit(void)
{
    ka_task_mutex_lock(&g_svm_k2umc_sync_mutex);
    if (g_svm_k2umc_sync_inited) {
        svm_idr_uninit(&g_svm_k2umc_sync_idr, k2umc_sync_res_release, false);
        g_svm_k2umc_sync_inited = false;
    }
    ka_task_mutex_unlock(&g_svm_k2umc_sync_mutex);
}
DECLAER_FEATURE_AUTO_UNINIT(svm_k2umc_uninit, FEATURE_LOADER_STAGE_3);
