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
#include "ascend_hal_error.h"

#include "esched_kernel_interface.h"
#include "pbl_feature_loader.h"
#include "pbl_runenv_config.h"

#include "svm_sub_event_type_uk_msg.h"
#include "svm_kern_log.h"
#include "svm_um_handle.h"

#define SVM_EVENT_HANDLE_MSG_MAX_LEN 128

#define UM_HANDLE_REPLY_RET(ptr) (*((int *)ptr))
#define UM_HANDLE_REPLY_DATA_PTR(ptr) (((char *)ptr) + sizeof(int))
#define UM_HANDLE_REPLY_DATE_LEN(msg_len) (msg_len - sizeof(int))

struct svm_event_handle {
    int (*pre_handle)(u32 udevid, int master_tgid, int slave_tgid, void *msg, u32 msg_len);
    void (*pre_cancel_handle)(u32 udevid, int master_tgid, int slave_tgid, void *msg, u32 msg_len);
    int (*post_handle)(u32 udevid, int master_tgid, int slave_tgid, void *msg, u32 msg_len);
};

static struct svm_event_handle svm_event_hanldes[SVM_SUB_EVNET_TYPE_NUM] = {
    NULL,
};

void svm_um_register_handle(u32 subevent_id,
                            int (*pre_handle)(u32 udevid, int master_tgid, int slave_tgid, void *msg, u32 msg_len),
                            void (*pre_cancel_handle)(u32 udevid, int master_tgid, int slave_tgid, void *msg,
                                                      u32 msg_len),
                            int (*post_handle)(u32 udevid, int master_tgid, int slave_tgid, void *msg, u32 msg_len))
{
    u32 sub_event_type = subevent_id - SVM_SUB_EVNET_TYPE_BASE;
    if (sub_event_type < SVM_SUB_EVNET_TYPE_NUM) {
        svm_event_hanldes[sub_event_type].pre_handle = pre_handle;
        svm_event_hanldes[sub_event_type].pre_cancel_handle = pre_cancel_handle;
        svm_event_hanldes[sub_event_type].post_handle = post_handle;
    }
}

static struct svm_event_handle *svm_um_get_subevent_handle(u32 sub_event_type)
{
    return &svm_event_hanldes[sub_event_type];
}

static bool svm_um_is_in_host(void)
{
    return (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT);
}

static bool is_svm_sub_event(u32 subevent_id)
{
    return ((subevent_id >= SVM_SUB_EVNET_TYPE_BASE) &&
            (subevent_id < (SVM_SUB_EVNET_TYPE_BASE + SVM_SUB_EVNET_TYPE_NUM)));
}

static int svm_sync_event_request_handle(u32 udevid, int master_tgid, int slave_tgid, struct svm_event_handle *handle,
                                         void *msg, u32 msg_len)
{
    if (handle->pre_handle != NULL) {
        struct event_sync_msg *sync_msg = (struct event_sync_msg *)msg;
        if (msg_len <= sizeof(*sync_msg)) {
            svm_err("Msg len error. (msg_len=%u)\n", msg_len);
            return -EINVAL;
        }
        return handle->pre_handle(udevid, master_tgid, slave_tgid, sync_msg->msg, msg_len - sizeof(*sync_msg));
    }
    return 0;
}

static int svm_sync_event_response_handle(u32 udevid, int master_tgid, int slave_tgid, struct svm_event_handle *handle,
                                          void *msg, u32 msg_len)
{
    if (msg_len < sizeof(int)) {
        svm_err("Msg len error. (msg_len=%u)\n", msg_len);
        return -EINVAL;
    }

    if (UM_HANDLE_REPLY_RET(msg) != 0) {
        if (handle->pre_cancel_handle != NULL) {
            handle->pre_cancel_handle(udevid, master_tgid, slave_tgid, UM_HANDLE_REPLY_DATA_PTR(msg),
                                      UM_HANDLE_REPLY_DATE_LEN(msg_len));
        }
    } else {
        if (handle->post_handle != NULL) {
            return handle->post_handle(udevid, master_tgid, slave_tgid, UM_HANDLE_REPLY_DATA_PTR(msg),
                                       UM_HANDLE_REPLY_DATE_LEN(msg_len));
        }
    }
    return 0;
}

static bool svm_um_local_event_is_request(u32 subevent_id)
{
    if (svm_um_is_in_host()) {
        if (svm_sub_event_is_h2d(subevent_id)) {
            return true;
        }
    } else {
        if (svm_sub_event_is_d2h(subevent_id)) {
            return true;
        }
    }

    return false;
}

static bool svm_um_local_event_is_response(u32 subevent_id)
{
    if (svm_um_is_in_host()) {
        if (svm_sub_event_is_d2h(subevent_id)) {
            return true;
        }
    } else {
        if (svm_sub_event_is_h2d(subevent_id)) {
            return true;
        }
    }

    return false;
}

static int _svm_local_submit_handle(u32 udevid, struct sched_published_event_info *event_info,
                                    struct sched_published_event_func *event_func)
{
    struct svm_event_handle *handle = NULL;
    int master_tgid = svm_um_is_in_host() ? ka_task_get_current_tgid() : event_info->pid;
    int slave_tgid = svm_um_is_in_host() ? event_info->pid : ka_task_get_current_tgid();

    if (!is_svm_sub_event(event_info->subevent_id)) {
        return 0;
    }

    handle = svm_um_get_subevent_handle(event_info->subevent_id - SVM_SUB_EVNET_TYPE_BASE);
    if ((handle->pre_handle == NULL) && (handle->pre_cancel_handle == NULL) && (handle->post_handle == NULL)) {
        return 0;
    }

    if ((event_info->msg == NULL) || (event_info->msg_len == 0) ||
        (event_info->msg_len > SVM_EVENT_HANDLE_MSG_MAX_LEN)) {
        svm_err("Msg para error. (subevent_id=%u; msg_len=%u)\n", event_info->subevent_id, event_info->msg_len);
        return -EINVAL;
    }

    if (svm_um_local_event_is_request(event_info->subevent_id)) {
        return svm_sync_event_request_handle(udevid, master_tgid, slave_tgid, handle, event_info->msg,
                                             event_info->msg_len);
    } else if (svm_um_local_event_is_response(event_info->subevent_id)) {
        return svm_sync_event_response_handle(udevid, master_tgid, slave_tgid, handle, event_info->msg,
                                              event_info->msg_len);
    }

    return 0;
}

static bool svm_um_remote_event_is_request(u32 subevent_id)
{
    if (svm_um_is_in_host()) {
        if ((svm_sub_event_is_d2h(subevent_id)) || (svm_sub_event_is_k2u(subevent_id))) {
            return true;
        }
    } else {
        if (svm_sub_event_is_h2d(subevent_id)) {
            return true;
        }
    }

    return false;
}

static bool svm_um_remote_event_is_response(u32 subevent_id)
{
    if (svm_um_is_in_host()) {
        if (svm_sub_event_is_h2d(subevent_id)) {
            return true;
        }
    } else {
        if ((svm_sub_event_is_d2h(subevent_id)) || (svm_sub_event_is_k2u(subevent_id))) {
            return true;
        }
    }

    return false;
}

static int _svm_remote_submit_handle(u32 udevid, struct sched_published_event_info *event_info,
                                     struct sched_published_event_func *event_func)
{
    struct svm_event_handle *handle = NULL;
    int master_tgid = svm_um_is_in_host() ? event_info->pid : 0; /* adapt later */
    int slave_tgid = svm_um_is_in_host() ? 0 : event_info->pid;  /* adapt later */

    if (!is_svm_sub_event(event_info->subevent_id)) {
        return 0;
    }

    handle = svm_um_get_subevent_handle(event_info->subevent_id - SVM_SUB_EVNET_TYPE_BASE);

    if ((event_info->msg == NULL) || (event_info->msg_len == 0) ||
        (event_info->msg_len > SVM_EVENT_HANDLE_MSG_MAX_LEN)) {
        svm_err("Msg para error. (subevent_id=%u; msg_len=%u)\n", event_info->subevent_id, event_info->msg_len);
        return -EINVAL;
    }

    if (svm_um_remote_event_is_request(event_info->subevent_id)) {
        if (event_info->gid == SVM_INVALID_EVENT_GID) {
            int ret = sched_query_local_task_gid(udevid, event_info->pid, EVENT_DRV_MSG_GRP_NAME, &event_info->gid);
            if (ret != 0) {
                svm_err("Update gid failed. (subevent_id=%u; msg_len=%u)\n", event_info->subevent_id,
                        event_info->msg_len);
                return ret;
            }
        }

        return svm_sync_event_request_handle(udevid, master_tgid, slave_tgid, handle, event_info->msg,
                                             event_info->msg_len);
    } else if (svm_um_remote_event_is_response(event_info->subevent_id)) {
        return svm_sync_event_response_handle(udevid, master_tgid, slave_tgid, handle, event_info->msg,
                                              event_info->msg_len);
    }

    return 0;
}

static int svm_um_kerror_to_uerror(int kerror)
{
    switch (kerror) {
        case 0:
            return SCHED_EVENT_PRE_PROC_SUCCESS;
        case -EINPROGRESS:
            return SCHED_EVENT_PRE_PROC_SUCCESS_RETURN;
        case -EINVAL:
            return DRV_ERROR_INVALID_VALUE;
        case -ENOMEM:
            return DRV_ERROR_OUT_OF_MEMORY;
        case -EBUSY:
            return DRV_ERROR_BUSY;
        default:
            return DRV_ERROR_IOCRL_FAIL;
    }
}

static int svm_local_submit_handle(u32 udevid, struct sched_published_event_info *event_info,
                                   struct sched_published_event_func *event_func)
{
    int ret;
    ret = _svm_local_submit_handle(udevid, event_info, event_func);
    return svm_um_kerror_to_uerror(ret); /* Esched need uerror */
}

static int svm_remote_submit_handle(u32 udevid, struct sched_published_event_info *event_info,
                                    struct sched_published_event_func *event_func)
{
    int ret;
    ret = _svm_remote_submit_handle(udevid, event_info, event_func);
    return svm_um_kerror_to_uerror(ret); /* Esched need uerror */
}

int svm_um_handle_init(void)
{
    int ret;

    ret = hal_kernel_sched_register_event_pre_proc_handle(EVENT_DRV_MSG_EX, SCHED_PRE_PROC_POS_LOCAL,
                                                          svm_local_submit_handle);
    ret |= hal_kernel_sched_register_event_pre_proc_handle(EVENT_DRV_MSG_EX, SCHED_PRE_PROC_POS_REMOTE,
                                                           svm_remote_submit_handle);

    return ret;
}
DECLAER_FEATURE_AUTO_INIT(svm_um_handle_init, FEATURE_LOADER_STAGE_5);

void svm_um_handle_uninit(void)
{
    hal_kernel_sched_unregister_event_pre_proc_handle(EVENT_DRV_MSG_EX, SCHED_PRE_PROC_POS_LOCAL,
                                                      svm_local_submit_handle);
    hal_kernel_sched_unregister_event_pre_proc_handle(EVENT_DRV_MSG_EX, SCHED_PRE_PROC_POS_REMOTE,
                                                      svm_remote_submit_handle);
}
DECLAER_FEATURE_AUTO_UNINIT(svm_um_handle_uninit, FEATURE_LOADER_STAGE_5);
