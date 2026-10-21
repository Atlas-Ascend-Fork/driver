/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "mmpa_api.h"
#include "ascend_hal.h"
#include "ascend_hal_external.h"
#include "davinci_interface.h"
#include "dms_user_interface.h"
#include "comm_user_interface.h"
#include "hdc_epoll.h"
#include "hdc_ub_drv.h"
#include "hdc_core.h"

#ifndef ERESTARTSYS
#define ERESTARTSYS 512
#endif

#ifdef DRV_UT
#define STATIC
#else
#define STATIC static
#endif

#ifdef CFG_FEATURE_SUPPORT_UB

static struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int thread_count;
    bool closing;
} g_remote_close_info = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
};

static struct {
    char str[HDCDRV_STR_NAME_LEN];
} hdcdrv_service_type_str[HDCDRV_SUPPORT_MAX_SERVICE + 1] = {
    {"service_dmp"},            /* HDCDRV_SERVICE_TYPE_DMP */
    {"service_profiling"},      /* HDCDRV_SERVICE_TYPE_PROFILING */
    {"service_IDE1"},           /* HDCDRV_SERVICE_TYPE_IDE1 */
    {"service_file_trans"},     /* HDCDRV_SERVICE_TYPE_FILE_TRANS */
    {"service_IDE2"},           /* HDCDRV_SERVICE_TYPE_IDE2 */
    {"service_log"},            /* HDCDRV_SERVICE_TYPE_LOG */
    {"service_rdma"},           /* HDCDRV_SERVICE_TYPE_RDMA */
    {"service_bbox"},           /* HDCDRV_SERVICE_TYPE_BBOX */
    {"service_framework"},      /* HDCDRV_SERVICE_TYPE_FRAMEWORK */
    {"service_TSD"},            /* HDCDRV_SERVICE_TYPE_TSD */
    {"service_TDT"},            /* HDCDRV_SERVICE_TYPE_TDT */
    {"service_PROF"},           /* HDCDRV_SERVICE_TYPE_PROF */
    {"service_IDE_file_trans"}, /* HDCDRV_SERVICE_TYPE_IDE_FILE_TRANS */
    {"service_dump"},           /* HDCDRV_SERVICE_TYPE_DUMP */
    {"service_user3"},          /* HDCDRV_SERVICE_TYPE_USER3 */
    {"service_DVPP"},           /* HDCDRV_SERVICE_TYPE_DVPP */
    {"service_queue"},          /* HDCDRV_SERVICE_TYPE_QUEUE */
    {"service_upgrade"},        /* HDCDRV_SERVICE_TYPE_UPGRADE */
    {"service_rdma_v2"},        /* HDCDRV_SERVICE_TYPE_RDMA_V2 */
    {"service_test"},           /* HDCDRV_SERVICE_TYPE_TEST */
};

STATIC void hdcdrv_fill_service_type_str(void)
{
    int i, ret;

    for (i = HDCDRV_SERVICE_TYPE_USER_START; i < HDCDRV_SUPPORT_MAX_SERVICE; i++) {
        ret = snprintf_s(hdcdrv_service_type_str[i].str, HDCDRV_STR_NAME_LEN, HDCDRV_STR_NAME_LEN - 1,
                         "service_user_%d", i - HDCDRV_SERVICE_TYPE_USER_START);
        if (ret < 0) {
            HDC_LOG_ERR("snprintf_s error, service %d can not print error string in log.\n",
                        i - HDCDRV_SERVICE_TYPE_USER_START);
            hdcdrv_service_type_str[i].str[0] = '\0';
        }
    }
}

static struct {
    const char str[HDCDRV_STR_NAME_LEN];
} hdcdrv_close_state_str[HDCDRV_CLOSE_TYPE_MAX + 1] = {
    {"state_none"},                  /* HDCDRV_CLOSE_TYPE_NONE */
    {"closed_by_user"},              /* HDCDRV_CLOSE_TYPE_USER */
    {"closed_by_user_form_kernel"},  /* HDCDRV_CLOSE_TYPE_KERNEL */
    {"closed_by_release"},           /* HDCDRV_CLOSE_TYPE_RELEASE */
    {"closed_by_set_owner_timeout"}, /* HDCDRV_CLOSE_TYPE_NOT_SET_OWNER */
    {"remote_close_post"},           /* HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST */
    {"state_max"}                    /* HDCDRV_CLOSE_TYPE_MAX */
};

STATIC const char *hdc_get_close_str(int close_state)
{
    if ((close_state < HDCDRV_CLOSE_TYPE_NONE) || (close_state > HDCDRV_CLOSE_TYPE_MAX)) {
        return hdcdrv_close_state_str[HDCDRV_CLOSE_TYPE_MAX].str;
    } else {
        return hdcdrv_close_state_str[close_state].str;
    }
}

const char *hdc_get_sevice_str(int service_type)
{
    if ((service_type < HDCDRV_SERVICE_TYPE_DMP) || (service_type >= HDCDRV_SUPPORT_MAX_SERVICE)) {
        return "service_invalid";
    } else if ((service_type < HDCDRV_SERVICE_TYPE_USER_START) && (service_type >= HDCDRV_SERVICE_TYPE_APPLY_MAX)) {
        return "service_reserved";
    } else {
        return hdcdrv_service_type_str[service_type].str;
    }
}

signed int drv_hdc_ub_session_para_check(const struct hdc_session *p_session)
{
    struct hdc_ub_session *session;

    if ((p_session == NULL) || (p_session->ub_session == NULL)) {
        HDC_LOG_WARN("session is null, session may have been closed.\n");
        return -HDCDRV_SESSION_HAS_CLOSED;
    }

    session = p_session->ub_session;
    if (session->status == HDC_SESSION_STATUS_IDLE) {
        HDC_LOG_WARN(
            "session has been closed. (session_fd=%u; session_remote_fd=%u; dev_id=%d; service_type=\"%s\";"
            "local_close_state=\"%s\"; remote_close_state=\"%s\"; local_session_fd=%u; remote_session_fd=%u)\n",
            session->local_id, session->remote_id, session->dev_id, hdc_get_sevice_str(session->service_type),
            hdc_get_close_str(session->local_close_state), hdc_get_close_str(session->remote_close_state),
            session->local_id, session->remote_id);
        return -HDCDRV_SESSION_HAS_CLOSED;
    }

    return DRV_ERROR_NONE;
}

signed int hdc_ub_ioctl(mmProcess handle, signed int ioctl_code, union hdcdrv_cmd *hdc_cmd)
{
    mmIoctlBuf arg = {0};
    int ret;

    arg.inbuf = (void *)hdc_cmd;
    arg.inbufLen = sizeof(union hdcdrv_cmd);
    arg.outbuf = hdc_cmd;
    arg.outbufLen = sizeof(union hdcdrv_cmd);

    ret = mmIoctl(handle, ioctl_code, &arg);
    if (ret == -HDCDRV_NOT_SUPPORT) {
        ret = DRV_ERROR_NOT_SUPPORT;
    }
    return ret;
}

mmProcess hdc_ub_open(void)
{
    struct davinci_intf_open_arg arg = {{0}, 0};
    mmIoctlBuf io_buf = {0};
    mmProcess fd;
    int ret;
    int flags;

    fd = mmOpen2("/dev/davinci_manager", M_RDWR, M_IRUSR | M_IWRITE);
    if (fd < 0) {
        HDC_LOG_ERR("Open device failed. (fd=%d)\n", fd);
        return fd;
    }

    flags = fcntl(fd, F_GETFD);
    flags = (int)((unsigned int)flags | FD_CLOEXEC);
    (void)fcntl(fd, F_SETFD, flags);

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, HDC_MODULE_NAME);
    if (ret != 0) {
#ifndef DRV_UT
        HDC_LOG_ERR("Strcpy_s failed. (ret=%d)\n", ret);
        goto open_fail;
#endif
    }

    io_buf.inbuf = (void *)&arg;
    ret = mmIoctl(fd, DAVINCI_INTF_IOCTL_OPEN, &io_buf);
    if (ret != 0) {
#ifndef DRV_UT
        HDC_LOG_ERR("Davinci intf ioctl open failed. (ret=%d)\n", ret);
        goto open_fail;
#endif
    }

    return fd;

#ifndef DRV_UT
open_fail:
    mmClose(fd);
    return -1; /* when fail fd < 0 */
#endif
}

void hdc_ub_close(mmProcess handle)
{
    struct davinci_intf_open_arg arg = {{0}, 0};
    mmIoctlBuf io_buf = {0};
    int ret;

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, HDC_MODULE_NAME);
    if (ret != 0) {
        HDC_LOG_ERR("Strcpy_s failed. (ret=%d)\n", ret);
        goto close_fd;
    }
    io_buf.inbuf = (void *)&arg;
    ret = mmIoctl(handle, DAVINCI_INTF_IOCTL_CLOSE, &io_buf);
    if (ret != 0) {
        HDC_LOG_ERR("Davinci intf ioctl close failed. (ret=%d)\n", ret);
    }
close_fd:
    mmClose(handle);
    return;
}

signed int hdc_ub_get_session_attr(mmProcess handle, const struct hdc_session *p_session, int attr, int *value)
{
    struct hdc_ub_session *session = NULL;
    union hdcdrv_cmd hdc_cmd;
    int ret;

    ret = drv_hdc_ub_session_para_check(p_session);
    if (ret != 0) {
        if (attr == HDCDRV_SESSION_ATTR_STATUS) {
            *value = HDCDRV_SESSION_HAS_CLOSED;
            ret = DRV_ERROR_NONE;
        }
        return ret;
    }

    session = p_session->ub_session;

    switch (attr) {
        case HDCDRV_SESSION_ATTR_VFID:
            *value = 0;
            ret = DRV_ERROR_NONE;
            break;
        case HDCDRV_SESSION_ATTR_LOCAL_CREATE_PID:
            *value = (int)session->create_pid;
            ret = DRV_ERROR_NONE;
            break;
        case HDCDRV_SESSION_ATTR_PEER_CREATE_PID:
            *value = (int)session->peer_create_pid;
            ret = DRV_ERROR_NONE;
            break;
        case HDCDRV_SESSION_ATTR_STATUS:
        case HDCDRV_SESSION_ATTR_RUN_ENV:
            hdc_cmd.get_session_attr.session = p_session->sockfd;
            hdc_cmd.get_session_attr.dev_id = (int)p_session->device_id;
            hdc_cmd.get_session_attr.cmd_type = attr;
            ret = hdc_ub_ioctl(handle, HDCDRV_GET_SESSION_ATTR, &hdc_cmd);
            if (ret == 0) {
                *value = hdc_cmd.get_session_attr.output;
            }
            break;
        case HDCDRV_SESSION_ATTR_DFX:
            *value = hdc_ub_get_session_dfx(p_session->device_id, session);
            ret = DRV_ERROR_NONE;
            break;
        default:
            HDC_LOG_ERR("Input parameter is error.\n");
            ret = DRV_ERROR_INVALID_VALUE;
            break;
    }

    return ret;
}

signed int hdc_alloc_connect_session(union hdcdrv_cmd *hdc_cmd, struct hdc_session *p_session,
                                     struct hdc_mem_res_info *mem_info, int peer_pid, signed int dev_id)
{
    mmProcess handle = mem_info->bind_fd;
    int retry_cnt = 0;
    signed int ret;

    hdc_cmd->connect.dev_id = dev_id;
    hdc_cmd->connect.service_type = mem_info->service_type;
    hdc_cmd->connect.peer_pid = peer_pid;
    hdc_cmd->connect.user_va = mem_info->user_va;

    p_session->bind_fd = handle;

    do {
        ret = hdc_ub_ioctl(handle, HDCDRV_CONNECT, hdc_cmd); // used for alloc session in kernel
        if (ret == 0) {
            p_session->sockfd = hdc_cmd->connect.session;
            p_session->device_id = (unsigned int)dev_id;
            return 0;
        }
        if (ret == ERESTARTSYS) {
            retry_cnt++;
            continue;
        }
        return ret;
    } while (retry_cnt < 1000); // 1000 is retry times

    return ret;
}

signed int hdc_alloc_accept_session(union hdcdrv_cmd *hdc_cmd, struct hdc_session *p_session, mmProcess bind_fd,
                                    signed int dev_id)
{
    mmProcess handle = bind_fd;
    int retry_cnt = 0;
    signed int ret;

    p_session->bind_fd = handle;

    do {
        ret = hdc_ub_ioctl(handle, HDCDRV_ACCEPT, hdc_cmd); // used for alloc session in kernel
        if (ret == 0) {
            p_session->sockfd = hdc_cmd->accept.session;
            p_session->device_id = (unsigned int)dev_id;
            return 0;
        }
        if (ret == ERESTARTSYS) {
            retry_cnt++;
            continue;
        }
        return ret;
    } while (retry_cnt < 1000); // 1000 is retry times

    return ret;
}

void hdc_fill_send_ctrl_msg_cmd(union hdcdrv_cmd *hdc_cmd, int dev_id, void *event_msg, size_t msg_len)
{
    hdc_cmd->send_ctrl_msg.dev_id = dev_id;
    hdc_cmd->send_ctrl_msg.msg_buf = event_msg;
    hdc_cmd->send_ctrl_msg.msg_len = (unsigned int)msg_len;
}

signed int hdc_send_ctrl_msg(mmProcess fd, union hdcdrv_cmd *hdc_cmd)
{
    return hdc_ub_ioctl(fd, HDCDRV_SEND_CTRL_MSG, hdc_cmd);
}

STATIC void hdc_epoll_wait_node_uninit(struct hdc_ub_epoll_node *event_node)
{
    pthread_cond_destroy(&event_node->cond);
    pthread_mutex_destroy(&event_node->mutex);
    return;
}

void hdc_ub_session_free(struct hdc_ub_session *session)
{
    struct hdc_ub_epoll_node *node = NULL;
    int retry_time = 0;
    if (session == NULL) {
        HDC_LOG_WARN("Session is NULL, will not free session.\n");
        return;
    }

    session->psession_ptr = NULL;

    node = session->epoll_event_node;
    // Maximum wait time: 1s
    (void)pthread_mutex_lock(&node->mutex);
    while ((node->ref != 0) && (retry_time < 10000)) { // 1s / 100us
        (void)pthread_mutex_unlock(&node->mutex);
        usleep(100); // 100us
        retry_time++;
        (void)pthread_mutex_lock(&node->mutex);
    }
    (void)pthread_mutex_unlock(&node->mutex);
    hdc_epoll_wait_node_uninit(session->epoll_event_node);
    free(session->epoll_event_node);
    free(session);
}

STATIC int hdc_epoll_wait_node_init(struct hdc_ub_session *session)
{
    pthread_condattr_t cond_attr;
    int ret;

    ret = pthread_condattr_init(&cond_attr);
    if (ret != 0) {
        HDC_LOG_ERR("pthread_condattr_init failed.(strerror=\"%s\"; ret=%d)\n", strerror(errno), ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    if (ret != 0) {
        HDC_LOG_ERR("pthread_condattr_setclock failed.(strerror=\"%s\"; ret=%d)\n", strerror(errno), ret);
        ret = DRV_ERROR_INNER_ERR;
        goto uninit_cond_attr;
    }

    ret = pthread_mutex_init(&session->epoll_event_node->mutex, NULL);
    if (ret != 0) {
        HDC_LOG_ERR("pthread_mutex_init failed.(strerror=\"%s\"; ret=%d)\n", strerror(errno), ret);
        ret = DRV_ERROR_INNER_ERR;
        goto uninit_cond_attr;
    }

    ret = pthread_cond_init(&session->epoll_event_node->cond, &cond_attr);
    if (ret != 0) {
        HDC_LOG_ERR("pthread_cond_init failed.(strerror=\"%s\"; ret=%d)\n", strerror(errno), ret);
        ret = DRV_ERROR_INNER_ERR;
        goto destroy_mutex;
    }

    session->epoll_event_node->ref = 0;
    session->epoll_event_node->pkt_num = 0;
    session->epoll_event_node->session = session;
    return 0;

destroy_mutex:
    pthread_mutex_destroy(&session->epoll_event_node->mutex);
uninit_cond_attr:
    (void)pthread_condattr_destroy(&cond_attr);

    return ret;
}

STATIC signed int hdc_ub_session_pre_init(struct hdc_session *p_session, struct hdc_mem_res_info *mem_info,
                                          unsigned long long peer_pid, unsigned int unique_val, const char *func_name)
{
    void *user_va = (void *)(uintptr_t)mem_info->user_va;
    signed int dev_id = (signed int)p_session->device_id;
    int idx = hdc_get_lock_index(dev_id, p_session->sockfd);
    struct hdc_ub_session *session = NULL;

    session = (struct hdc_ub_session *)drv_hdc_zalloc(sizeof(struct hdc_ub_session));
    if (session == NULL) {
        HDC_LOG_ERR("Call malloc failed.(dev_id=%d; type=\"%s\")\n", dev_id,
                    hdc_get_sevice_str(mem_info->service_type));
        hdc_report_out_of_mem_err_msg(func_name, sizeof(struct hdc_ub_session));
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    session->epoll_event_node = (struct hdc_ub_epoll_node *)drv_hdc_zalloc(sizeof(struct hdc_ub_epoll_node));
    if (session->epoll_event_node == NULL) {
        free(session);
        HDC_LOG_ERR("Call malloc failed for epoll_event_node.(dev_id=%d; type=\"%s\")\n", dev_id,
                    hdc_get_sevice_str(mem_info->service_type));
        hdc_report_out_of_mem_err_msg(func_name, sizeof(struct hdc_ub_epoll_node));
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    if (hdc_epoll_wait_node_init(session) != 0) {
        free(session->epoll_event_node);
        free(session);
        return DRV_ERROR_INNER_ERR;
    }

    session->dev_id = dev_id;
    session->service_type = mem_info->service_type;
    session->local_id = (unsigned int)p_session->sockfd;
    session->create_pid = (unsigned long long)getpid();
    session->lock_idx = (unsigned int)dev_id * HDCDRV_UB_SINGLE_DEV_MAX_SESSION + (unsigned int)p_session->sockfd;
    session->ctx = NULL;
    session->peer_create_pid = peer_pid;
    session->unique_val = unique_val;
    session->user_va = mem_info->user_va;
    session->bind_fd = mem_info->bind_fd;
    INIT_LIST_HEAD(&session->node);
    session->status = HDC_SESSION_STATUS_CONN;

    (void)memset_s(user_va, HDCDRV_UB_MEM_POOL_LEN, 0, HDCDRV_UB_MEM_POOL_LEN);

    p_session->ub_session = session;
    p_session->epoll_head = NULL;
    p_session->close_eventfd = -1;
    session->recv_eventfd = -1;
    session->psession_ptr = p_session;
    hdc_ub_init_dfx_info(&session->dbg_stat);
    g_hdcConfig.info_list[idx].session = session;
    g_hdcConfig.info_list[idx].unique_val = unique_val;
    g_hdcConfig.info_list[idx].status = HDC_SESSION_STATUS_CONN;
    return DRV_ERROR_NONE;
}

void hdc_ub_session_uninit(struct hdc_session *p_session, int dev_id, int session_id)
{
    struct hdc_ub_session *session = NULL;
    int idx = hdc_get_lock_index(dev_id, session_id);

    if (p_session == NULL || p_session->ub_session == NULL) {
        return;
    }

    g_hdcConfig.info_list[idx].status = HDC_SESSION_STATUS_IDLE;
    g_hdcConfig.info_list[idx].unique_val = 0;

    session = p_session->ub_session;
    g_hdcConfig.info_list[idx].session = NULL;

    hdc_epoll_wait_node_uninit(session->epoll_event_node);
    free(session->epoll_event_node);
    session->epoll_event_node = NULL;
    free(session);
    p_session->ub_session = NULL;
    return;
}

STATIC int hdc_import_remote_jetty(hdc_ub_context_t *ctx, hdc_jfr_id_info_t *info,
                                   struct hdc_time_record_for_urma_init *record)
{
    urma_rjfr_t remote_jfr = {.jfr_id =
                                  {
                                      .eid = info->eid,
                                      .uasid = info->uasid,
                                  },
                              .trans_mode = URMA_TM_RM};

#ifdef SSAPI_USE_MAMI
    urma_get_tp_cfg_t tpid_cfg = {
        .trans_mode = URMA_TM_RM,
        .local_eid = ctx->urma_ctx->eid, // local_eid
        .peer_eid = info->eid,           // remote_eid
        .flag.bs.ctp = true,
    };

    remote_jfr.jfr_id.id = info->jfr_id; // Do not delete this line
    remote_jfr.flag.bs.token_policy = URMA_TOKEN_PLAIN_TEXT;
    uint32_t tp_cnt = 1;
    urma_tp_info_t tpid_info = {0};
    urma_status_t status;
    urma_active_tp_cfg_t active_tp_cfg = {0};

    status = ascend_urma_get_tp_list(ctx->urma_ctx, &tpid_cfg, &tp_cnt, &tpid_info);
    if ((status != 0) || (tp_cnt == 0)) {
        HDC_LOG_ERR("urma get tp list failed (status=%d; tp_cnt=%u)\n", status, tp_cnt);
        return -HDCDRV_UB_INTERFACE_ERR;
    }
    hdc_get_time_record(&record->urma_record.get_tp_list, &record->fail_times);
    active_tp_cfg.tp_handle = tpid_info.tp_handle;
    remote_jfr.tp_type = URMA_CTP;
    ctx->tjfr = ascend_urma_import_jfr_ex(ctx->urma_ctx, &remote_jfr, &info->token_val, &active_tp_cfg);
#else
    remote_jfr.jfr_id.id = info->jfr_id;
    ctx->tjfr = urma_import_jfr(ctx->urma_ctx, &remote_jfr, &ctx->token);
#endif
    if (ctx->tjfr == NULL) {
        HDC_LOG_ERR("Failed to import remote jfr.\n");
        return -HDCDRV_UB_INTERFACE_ERR;
    }
    hdc_get_time_record(&record->urma_record.import_jfr, &record->fail_times);
    return 0;
}

STATIC void hdc_unimport_remote_jetty(hdc_ub_context_t *ctx)
{
    if (ctx->tjfr != NULL) {
        (void)ascend_urma_unimport_jfr_ex(ctx->tjfr);
        ctx->tjfr = NULL;
    }
}

STATIC signed int hdc_ub_session_kernel_close(mmProcess handle, unsigned int dev_id, union hdcdrv_cmd *hdc_cmd)
{
    signed int ret;

    if (handle == (mmProcess)EN_ERROR) {
#ifndef DRV_UT
        HDC_LOG_INFO("Input parameter handle is not correct.(dev_id=%d)\n", dev_id);
#endif
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = hdc_ub_ioctl(handle, HDCDRV_CLOSE, hdc_cmd);

    return ret;
}

STATIC int hdc_init_jfc_res(hdc_ub_context_t *ctx, hdc_urma_jfc_t *hdc_jfc)
{
    urma_jfc_cfg_t jfc_cfg = {0};
    int ret;

    hdc_jfc->jfce = urma_create_jfce(ctx->urma_ctx);
    if (hdc_jfc->jfce == NULL) {
        HDC_LOG_ERR("Failed to create jfce\n");
        return HDCDRV_ERR;
    }
    jfc_cfg.depth = 16; // default jfc depth is 16
    jfc_cfg.jfce = hdc_jfc->jfce;
    jfc_cfg.flag.bs.jfc_inline = 1;

    hdc_jfc->jfc = urma_create_jfc(ctx->urma_ctx, &jfc_cfg);
    if (hdc_jfc->jfc == NULL) {
        HDC_LOG_ERR("Failed to create jfc.\n");
        goto free_jfce;
    }
    ret = urma_rearm_jfc(hdc_jfc->jfc, false);
    if (ret != URMA_SUCCESS) {
        HDC_LOG_ERR("Failed to rearm jfc. ret=%d\n", ret);
        goto free_jfc;
    }
    return 0;

free_jfc:
    (void)urma_delete_jfc(hdc_jfc->jfc);
    hdc_jfc->jfc = NULL;
free_jfce:
    (void)urma_delete_jfce(hdc_jfc->jfce);
    hdc_jfc->jfce = NULL;
    return HDCDRV_ERR;
}

STATIC void hdc_uninit_jfc_res(hdc_ub_context_t *ctx, hdc_urma_jfc_t *hdc_jfc)
{
    (void)ctx;

    if (hdc_jfc->jfc != NULL) {
        (void)urma_delete_jfc(hdc_jfc->jfc);
        hdc_jfc->jfc = NULL;
    }

    if (hdc_jfc->jfce != NULL) {
        (void)urma_delete_jfce(hdc_jfc->jfce);
        hdc_jfc->jfce = NULL;
    }
}

STATIC void hdc_uninit_jfs_res(hdc_ub_context_t *ctx, hdc_urma_jfs_t *hdc_jfs)
{
    (void)ctx;

    if (hdc_jfs->jfs == NULL) {
        return;
    }

    (void)urma_delete_jfs(hdc_jfs->jfs);
    hdc_jfs->jfs = NULL;
}

STATIC int hdc_init_jfs_res(hdc_ub_context_t *ctx, hdc_urma_jfs_t *hdc_jfs, hdc_urma_jfc_t *hdc_jfc,
                            struct hdc_ub_session *session)
{
    int i;

    urma_jfs_cfg_t jfs_cfg = {.depth = HDC_URMA_MAX_JFS_DEPTH,
                              .trans_mode = URMA_TM_RM,
                              .max_sge = 1,
                              .max_inline_data = 0,
                              .rnr_retry = URMA_TYPICAL_RNR_RETRY,
                              .err_timeout = URMA_TYPICAL_ERR_TIMEOUT,
                              .jfc = hdc_jfc->jfc,
                              .user_ctx = (uint64_t)NULL};
    jfs_cfg.priority = hdc_ub_get_jfs_priority_by_type(session->service_type);
    hdc_jfs->jfs = urma_create_jfs(ctx->urma_ctx, &jfs_cfg);
    if (hdc_jfs->jfs == NULL) {
        HDC_LOG_ERR("Failed to create jfs.(session_id=%d; dev_id=%d)\n", session->local_id, session->dev_id);
        return HDCDRV_ERR;
    }

    for (i = 0; i < HDC_URMA_MAX_JFS_DEPTH; ++i) {
        hdc_jfs->jfs_tseg_flag[i] = HDC_UB_VALID;
    }

    return 0;
}

STATIC int hdc_get_jfs_tseg(struct hdc_ub_session *session, hdc_urma_jfs_t *hdc_jfs, uint32_t *seg_id, long timeout_ms)
{
    unsigned int i;

    (void)session;
    (void)timeout_ms;
    for (i = 0; i < HDC_URMA_MAX_JFS_DEPTH; ++i) {
        if (hdc_jfs->jfs_tseg_flag[i] == HDC_UB_VALID) {
            hdc_jfs->jfs_tseg_flag[i] = HDC_UB_INVALID;
            *seg_id = i;
            return HDCDRV_OK;
        }
    }

    HDC_LOG_ERR("fail to get tseg, some problem may have happened, post sema\n");
    return -HDCDRV_ERR;
}

STATIC void hdc_put_jfs_tseg(hdc_ub_context_t *ctx, hdc_urma_jfs_t *hdc_jfs, uint32_t seg_id)
{
    (void)ctx;
    hdc_jfs->jfs_tseg_flag[seg_id] = HDC_UB_VALID;
}

STATIC void hdc_uninit_jfr_res(hdc_ub_context_t *ctx, hdc_urma_jfr_t *hdc_jfr)
{
    (void)ctx;
    (void)urma_delete_jfr(hdc_jfr->jfr);
    hdc_jfr->jfr = NULL;
}

STATIC int hdc_init_jfr_res(hdc_ub_context_t *ctx, hdc_urma_jfr_t *hdc_jfr, hdc_urma_jfc_t *hdc_jfc,
                            struct hdc_ub_session *session)
{
    urma_jfr_cfg_t jfr_cfg = {.depth = HDC_URMA_MAX_JFR_DEPTH,
                              .flag.bs.tag_matching = URMA_NO_TAG_MATCHING,
                              .flag.bs.token_policy = URMA_TOKEN_PLAIN_TEXT,
                              .trans_mode = URMA_TM_RM,
                              .max_sge = 1,
                              .min_rnr_timer = URMA_TYPICAL_MIN_RNR_TIMER,
                              .jfc = hdc_jfc->jfc,
                              .token_value = ctx->token,
                              .id = 0};

    hdc_jfr->jfr = urma_create_jfr(ctx->urma_ctx, &jfr_cfg);
    if (hdc_jfr->jfr == NULL) {
        HDC_LOG_ERR("Failed to create jfr.(session_id=%d; dev_id=%d)\n", session->local_id, session->dev_id);
        return HDCDRV_ERR;
    }

    return 0;
}

STATIC int hdc_post_seg_to_jfr(urma_jfr_t *jfr, urma_target_seg_t *tseg, uint64_t start_addr, uint64_t user_ctx,
                               struct hdc_ub_session *session)
{
    urma_jfr_wr_t *bad_wr = NULL;
    urma_jfr_wr_t wr = {0};
    uint64_t seg_len;
    uint64_t seg_va;
    urma_sge_t src;
    int ret;

    seg_len = HDC_MEM_BLOCK_SIZE;
    seg_va = start_addr;

    src.addr = seg_va;
    src.len = (unsigned int)seg_len;
    src.tseg = tseg;

    wr.user_ctx = user_ctx;
    wr.src.sge = &src;
    wr.src.num_sge = 1;
    ret = urma_post_jfr_wr(jfr, &wr, &bad_wr);
    if (ret != 0) {
        HDC_LOG_ERR("Failed to post jfr.(jfr_id=%d; dev_id=%d; l_id=%u; r_id=%u; ret=%d)\n", jfr->jfr_id.id,
                    session->dev_id, session->local_id, session->remote_id, ret);
        session->dbg_stat.rx_fail_ub++;
        return HDCDRV_ERR;
    }
    return 0;
}

/* ----------------------------------------------
 * Before the wr is put into the jfr, the user_ctx needs to be filled in the wr
 * The CQE information received by the jfc is cr. cr searches for the corresponding memory based on user_ctx
 * The HDC has JFR with 4k and 64k, but only one JFC.
 * Therefore, user_ctx must be able to distinguish the memory of the two specifications.
 * Another usage: user_ctx is u64 and can point to the memory of tseg, which is convenient, but may cause wild pointer
 */
STATIC int hdc_init_context_jfr_seg(hdc_ub_context_t *ctx, struct hdc_ub_session *session)
{
    uint64_t user_ctx, i, addr;
    int ret = 0;

    /* The first 16 memory blocks in the memory pool are used by the tx of the jfs.
    rx in jfr uses last 16 memory pool blocks
    Therefore, the offset of 16 needs to be added when the post is sent to the JFR. */

    for (i = 0; i < HDC_URMA_MAX_JFR_DEPTH; ++i) {
        user_ctx = i + HDC_URMA_MAX_JFS_DEPTH;
        addr = session->user_va + user_ctx * HDC_MEM_BLOCK_SIZE;
        ret += hdc_post_seg_to_jfr(ctx->jfr.jfr, ctx->tseg, addr, user_ctx, session);
    }

    if (ret != 0) {
        // No need to recycle seg, directly delete jfr
        HDC_LOG_ERR("Failed to post seg_to jfr.\n");
        return ret;
    }

    return 0;
}

STATIC int hdc_init_urma_resource(hdc_ub_context_t *ctx, struct hdc_ub_session *session,
                                  struct hdc_time_record_for_urma_init *record)
{
    int ret;

    ret = hdc_register_dev_dmp_urma_seg(ctx, HDCDRV_UB_MEM_POOL_LEN, session->user_va, session->service_type);
    hdc_get_time_record(&record->urma_record.reg_dmp_end, &record->fail_times);
    if (ret != 0) {
        HDC_LOG_ERR("Failed to register dev dmp seg.(session_id=%d; dev_id=%d)\n", session->local_id, session->dev_id);
        return ret;
    }

    ret = hdc_init_jfc_res(ctx, &ctx->jfc_send);
    if (ret != 0) {
        goto create_send_jfc_failed;
    }

    ret = hdc_init_jfc_res(ctx, &ctx->jfc_recv);
    if (ret != 0) {
        goto create_recv_jfc_failed;
    }
    hdc_get_time_record(&record->urma_record.create_jfc, &record->fail_times);

    ret = hdc_init_jfs_res(ctx, &ctx->jfs, &ctx->jfc_send, session);
    if (ret != 0) {
        goto create_jfs_failed;
    }
    hdc_get_time_record(&record->urma_record.create_jfs, &record->fail_times);

    ctx->jfr.seg_len = HDC_MEM_BLOCK_SIZE;
    ret = hdc_init_jfr_res(ctx, &ctx->jfr, &ctx->jfc_recv, session);
    if (ret != 0) {
        goto create_jfr_4K_failed;
    }
    hdc_get_time_record(&record->urma_record.create_jfr, &record->fail_times);

    ret = hdc_init_context_jfr_seg(ctx, session);
    if (ret != 0) {
        HDC_LOG_ERR("Failed to init urma resource.(dev_id=%d; ret=%d)\n", session->dev_id, ret);
        goto post_jfr_seg_failed;
    }
    hdc_get_time_record(&record->urma_record.post_jfr_wr, &record->fail_times);

    return 0;

post_jfr_seg_failed:
    hdc_uninit_jfr_res(ctx, &ctx->jfr);
create_jfr_4K_failed:
    hdc_uninit_jfs_res(ctx, &ctx->jfs);
create_jfs_failed:
    hdc_uninit_jfc_res(ctx, &ctx->jfc_recv);
create_recv_jfc_failed:
    hdc_uninit_jfc_res(ctx, &ctx->jfc_send);
create_send_jfc_failed:
    hdc_unregister_dmp_urma_seg(ctx->tseg, session->service_type);
    return ret;
}

// suggest delete pos: jfr -> jfs -> jfc;
STATIC void hdc_uninit_urma_resource(hdc_ub_context_t *ctx, struct hdc_ub_session *session,
                                     struct hdc_time_record_for_urma_uninit *record)
{
    hdc_ub_epoll_node_t *node = session->epoll_event_node;
    // one jfc for jfr, delete jfr and jfr_jfc first
    (void)mmMutexLock(&node->mutex);
    hdc_uninit_jfr_res(ctx, &ctx->jfr);
    hdc_get_time_record(&record->del_jfr, &record->fail_times);
    hdc_get_time_record(&record->wait_data_fin, &record->fail_times);
    hdc_uninit_jfc_res(ctx, &ctx->jfc_recv);
    hdc_get_time_record(&record->del_jfcr, &record->fail_times);
    (void)mmMutexUnLock(&node->mutex);

    // one jfc for jfs, delete jfs and jfs_jfc
    hdc_uninit_jfs_res(ctx, &ctx->jfs);
    hdc_get_time_record(&record->del_jfs_1, &record->fail_times);
    hdc_uninit_jfc_res(ctx, &ctx->jfc_send);
    hdc_get_time_record(&record->del_jfcs_1, &record->fail_times);

    hdc_unregister_dmp_urma_seg(ctx->tseg, session->service_type);
    hdc_unregister_own_urma_seg(ctx->tseg, ctx->token_id_node, ctx->devid, session->service_type);
    ctx->tseg = NULL;
    ctx->token_id = NULL;
    ctx->token_id_node = NULL;
    hdc_get_time_record(&record->own_unreg, &record->fail_times);
}

hdcError_t hdc_urma_init(struct hdcConfig *hdc_config, uint32_t dev_id, struct hdc_time_record_for_urma_init *record)
{
    struct dms_ub_dev_info dev_info = {0};
    urma_device_t *urma_dev;
    unsigned int eid_index;
    int num, ret = 0;

    hdc_get_time_record(&record->urma_record.hdc_init_urma_start, &record->fail_times);
    hdc_config->urma_attr[dev_id] = calloc(1, sizeof(hdc_urma_info_t));
    if (hdc_config->urma_attr[dev_id] == NULL) {
        HDC_LOG_ERR("alloc mem for urma_ctx failed.(dev_id=%u)\n", dev_id);
        goto finish_unlock;
    }
    hdc_get_time_record(&record->urma_record.attr_calloc, &record->fail_times);

    ret = dms_get_ub_dev_info(dev_id, &dev_info, &num);
    if ((ret != 0) || (num == 0)) {
        HDC_LOG_ERR("dms_get_ub_dev_info failed.(dev_id=%u; ret=%d; num=%d)\n", dev_id, ret, num);
        goto free_ctx_mem;
    }
    hdc_get_time_record(&record->dms_record.get_dev_info, &record->fail_times);

    urma_dev = dev_info.urma_dev[0];
    eid_index = dev_info.eid_index[0];

    hdc_config->urma_attr[dev_id]->urma_ctx = ascend_urma_create_context(urma_dev, eid_index);
    if (hdc_config->urma_attr[dev_id]->urma_ctx == NULL) {
        HDC_LOG_ERR("Failed to create instance.(dev_id=%u)\n", dev_id);
        goto free_ctx_mem;
    }
    hdc_get_time_record(&record->urma_record.create_ctx, &record->fail_times);

    if (hdc_alloc_token_id(hdc_config->urma_attr[dev_id]->urma_ctx, true, &hdc_config->urma_attr[dev_id]->token_id) !=
        0) {
        goto delete_urma_ctx;
    }
    hdc_get_time_record(&record->urma_record.alloc_token_id, &record->fail_times);

    hdc_config->urma_attr[dev_id]->cnt = 0;

    return 0;

delete_urma_ctx:
    (void)ascend_urma_delete_context(hdc_config->urma_attr[dev_id]->urma_ctx);
    hdc_config->urma_attr[dev_id]->urma_ctx = NULL;
free_ctx_mem:
    free(hdc_config->urma_attr[dev_id]);
    hdc_config->urma_attr[dev_id] = NULL;
finish_unlock:
    return HDCDRV_ERR;
}

void hdc_urma_uninit(struct hdcConfig *hdc_config, uint32_t dev_id, struct hdc_time_record_for_urma_uninit *record)
{
    hdc_free_token_id(hdc_config->urma_attr[dev_id]->token_id);
    hdc_config->urma_attr[dev_id]->token_id = NULL;
    if (record != NULL) {
        hdc_get_time_record(&record->free_token_id, &record->fail_times);
    }
    (void)ascend_urma_delete_context(hdc_config->urma_attr[dev_id]->urma_ctx);
    hdc_config->urma_attr[dev_id]->urma_ctx = NULL;
    if (record != NULL) {
        hdc_get_time_record(&record->del_ctx, &record->fail_times);
    }
    free(hdc_config->urma_attr[dev_id]);
    hdc_config->urma_attr[dev_id] = NULL;

    return;
}

// Create a Jetty process. Invoke this interface to create Jetty in the connect/accept process
STATIC hdc_ub_context_t *hdc_create_ub_context(uint32_t dev_id, struct hdc_ub_session *session,
                                               struct hdc_time_record_for_urma_init *record)
{
    int ret;
    unsigned int token_val;
    hdc_ub_context_t *ctx = NULL;

    record->urma_record.need_init = false;

    hdc_get_time_record(&record->urma_record.ctx_calloc, &record->fail_times);
    ctx = calloc(1, sizeof(hdc_ub_context_t));
    if (ctx == NULL) {
        HDC_LOG_ERR("calloc failed, len=%lu.(dev_id=%u)\n", sizeof(hdc_ub_context_t), dev_id);
        return NULL;
    }

    hdc_get_time_record(&record->dms_record.get_token_val, &record->fail_times);
    ret = dms_get_token_val(dev_id, SHARED_TOKEN_VAL, &token_val);
    if (ret != 0) {
        HDC_LOG_ERR("Failed to get token_val. (dev_id=%u)\n", dev_id);
        goto free;
    }

    hdc_get_time_record(&record->urma_record.urma_init_start, &record->fail_times);
    mmMutexLock(&g_hdcConfig.urma_attr_lock[dev_id]);
    hdc_get_time_record(&record->urma_record.lock_wait, &record->fail_times);
    if (g_hdcConfig.urma_attr[dev_id] == NULL) {
        record->urma_record.need_init = true;
        ret = hdc_urma_init(&g_hdcConfig, dev_id, record);
        if (ret != 0) {
            HDC_LOG_ERR("Process urma_attr is NULL(dev_id=%u).\n", dev_id);
            goto unlock_exit;
        }

        ret = hdc_register_share_urma_seg(&g_hdcConfig, dev_id, token_val);
        if (ret != 0) {
            HDC_LOG_ERR("Failed to register seg.(session_id=%d; dev_id=%d)\n", session->local_id, session->dev_id);
            goto urma_uninit;
        }
    }

    hdc_get_time_record(&record->urma_record.urma_init_end, &record->fail_times);
    g_hdcConfig.urma_attr[dev_id]->cnt++;

    ctx->urma_ctx = g_hdcConfig.urma_attr[dev_id]->urma_ctx;
    ctx->devid = dev_id;
    ctx->token.token = token_val;
    ctx->tseg = g_hdcConfig.urma_attr[dev_id]->tseg;
    ctx->token_id = g_hdcConfig.urma_attr[dev_id]->token_id;

    hdc_get_time_record(&record->urma_record.res_init_start, &record->fail_times);
    ret = hdc_register_own_urma_seg(ctx, HDCDRV_UB_MEM_POOL_LEN, session->user_va, session->service_type, record);
    hdc_get_time_record(&record->urma_record.register_own_seg, &record->fail_times);
    if (ret != 0) {
        HDC_LOG_ERR("Failed to register seg.(dev_id=%u)\n", dev_id);
        goto reg_failed;
    }
    mmMutexUnLock(&g_hdcConfig.urma_attr_lock[dev_id]);

    ret = hdc_init_urma_resource(ctx, session, record);
    if (ret != 0) {
        HDC_LOG_ERR("Failed to init urma resource. (dev_id=%u)\n", dev_id);
        goto urma_seg_uninit;
    }
    hdc_get_time_record(&record->urma_record.res_init_end, &record->fail_times);

    return ctx;

urma_seg_uninit:
    hdc_unregister_own_urma_seg(ctx->tseg, ctx->token_id_node, ctx->devid, session->service_type);
    ctx->tseg = NULL;
    ctx->token_id = NULL;
    ctx->token_id_node = NULL;
    mmMutexLock(&g_hdcConfig.urma_attr_lock[dev_id]);
reg_failed:
    g_hdcConfig.urma_attr[dev_id]->cnt--;
    if (g_hdcConfig.urma_attr[dev_id]->cnt == 0) {
        hdc_unregister_share_urma_seg(g_hdcConfig.urma_attr[dev_id]->tseg);
    }
urma_uninit:
    if (g_hdcConfig.urma_attr[dev_id]->cnt == 0) {
        hdc_urma_uninit(&g_hdcConfig, dev_id, NULL);
    }
unlock_exit:
    mmMutexUnLock(&g_hdcConfig.urma_attr_lock[dev_id]);
free:
    ctx->tseg = NULL;
    ctx->token_id = NULL;
    ctx->token_id_node = NULL;
    ctx->token.token = 0;
    ctx->urma_ctx = NULL;
    free(ctx);
    return NULL;
}

STATIC void hdc_delete_ub_context(hdc_ub_context_t *ctx, uint32_t dev_id, struct hdc_ub_session *session,
                                  struct hdc_time_record_for_urma_uninit *in_record)
{
    struct hdc_time_record_for_urma_uninit record = {0};
    hdc_uninit_urma_resource(ctx, session, &record);
    ctx->token.token = 0;
    ctx->urma_ctx = NULL;
    free(ctx);

    mmMutexLock(&g_hdcConfig.urma_attr_lock[dev_id]);
    if (g_hdcConfig.urma_attr[dev_id] == NULL) {
        HDC_LOG_ERR("Process urma_attr is NULL(dev_id=%u).\n", dev_id);
        mmMutexUnLock(&g_hdcConfig.urma_attr_lock[dev_id]);
        goto copy_record;
    }

    g_hdcConfig.urma_attr[dev_id]->cnt--;
    if (g_hdcConfig.urma_attr[dev_id]->cnt == 0) {
        record.need_uninit = true;
        hdc_unregister_share_urma_seg(g_hdcConfig.urma_attr[dev_id]->tseg);
        hdc_get_time_record(&record.share_unreg, &record.fail_times);
        hdc_urma_uninit(&g_hdcConfig, dev_id, &record);
    }
    mmMutexUnLock(&g_hdcConfig.urma_attr_lock[dev_id]);
copy_record:
    if (in_record != NULL) {
        memcpy_s(in_record, sizeof(struct hdc_time_record_for_urma_uninit), &record,
                 sizeof(struct hdc_time_record_for_urma_uninit));
    }
    return;
}

STATIC void hdc_ub_wait_remote_close_thread(void)
{
    struct timespec time;
    int ret;

    ret = clock_gettime(CLOCK_MONOTONIC, &time);
    if (ret == 0) {
        time.tv_sec += SESSION_REMOTE_CLOSED_TIME_US / CONVERT_S_TO_US;
    } else {
        HDC_LOG_WARN("clock_gettime failed, skip waiting remote-close threads.\n");
    }

    pthread_mutex_lock(&g_remote_close_info.mutex);
    g_remote_close_info.closing = true;
    pthread_cond_broadcast(&g_remote_close_info.cond);
    while ((g_remote_close_info.thread_count > 0) && (ret == 0)) {
        if (pthread_cond_timedwait(&g_remote_close_info.cond, &g_remote_close_info.mutex, &time) == ETIMEDOUT) {
            HDC_LOG_WARN("Wait remote-close threads timeout, count=%d.\n", g_remote_close_info.thread_count);
            break;
        }
    }
    pthread_mutex_unlock(&g_remote_close_info.mutex);
}

void hdc_ub_init(struct hdcConfig *hdc_config)
{
    uint32_t i = 0;
    pthread_condattr_t cond_attr;

    hdc_ub_perf_log_env_init();

    if (pthread_condattr_init(&cond_attr) == 0) {
        (void)pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
        (void)pthread_cond_init(&g_remote_close_info.cond, &cond_attr);
        (void)pthread_condattr_destroy(&cond_attr);
    }

    for (i = 0; i < HDC_MAX_UB_DEV_CNT; i++) {
        hdc_config->urma_attr[i] = NULL;
    }

    for (i = 0; i < HDC_MAX_UB_DEV_CNT * HDCDRV_UB_SINGLE_DEV_MAX_SESSION; i++) {
        (void)mmMutexInit(&hdc_config->session_lock[i]);
    }

    hdc_config->f_pid = getpid();

    hdc_ub_notiy_init(hdc_config);
    hdcdrv_fill_service_type_str();
    hdc_token_id_list_init();

    return;
}

void hdc_ub_uninit(struct hdcConfig *hdc_config)
{
    uint32_t i = 0;

    hdc_ub_wait_remote_close_thread();

    for (i = 0; i < HDC_MAX_UB_DEV_CNT; i++) {
        mmMutexLock(&hdc_config->urma_attr_lock[i]);
        if (hdc_config->urma_attr[i] != NULL) {
            HDC_LOG_WARN("urma_attr mem not free(dev_id=%u; cnt=%d)\n", i, hdc_config->urma_attr[i]->cnt);
        }
        mmMutexUnLock(&hdc_config->urma_attr_lock[i]);
    }

    return;
}

signed int hdc_link_event_pre_init(unsigned int dev_id, const char *func_name)
{
    signed int ret;

    // Create hdc process thread
    ret = hdc_process_thread_init(dev_id, func_name);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("hdc_process_thread_init failed.(dev_id=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    // Create epoll thread
    ret = hdc_ub_epoll_thread_init();
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("hdc_ub_epoll_thread_init failed.(dev_id=%u;ret=%d)\n", dev_id, ret);
        goto uninit_hdc_process_thread;
    }

    return 0;

uninit_hdc_process_thread:
    hdc_process_thread_uninit(dev_id);

    return ret;
}

void hdc_link_event_pre_uninit(signed int dev_id)
{
    // uninit epoll thread
    hdc_ub_epoll_thread_uninit();

    // uninit hdc process thread
    hdc_process_thread_uninit((unsigned int)dev_id);
}

int hdc_check_connect_wait(struct hdcdrv_event_msg *msg_back, unsigned int dev_id, int service_type)
{
    int ret;

    switch (msg_back->connect_msg_reply.server_error_code) {
        case 0:
            ret = 0;
            break;
        case -HDCDRV_DMA_MEM_ALLOC_FAIL:
            ret = DRV_ERROR_SERVER_BUSY;
            break;
        /* For NO_SESSION and NO_PERMISSION, the business may consider retrying to make the business run normally.
            There is no need to print the ERROR log, which is consistent with the PCIE scenario. */
        case -HDCDRV_NO_SESSION:
        case -HDCDRV_NO_PERMISSION:
            ret = msg_back->connect_msg_reply.server_error_code;
            break;
        case -HDCDRV_MEM_ALLOC_FAIL:
            HDC_LOG_ERR("Remote mmap failed, check remote side log.(dev_id=%u; error_code=%d; service=\"%s\")\n",
                        dev_id, msg_back->connect_msg_reply.server_error_code, hdc_get_sevice_str(service_type));
            ret = DRV_ERROR_NO_RESOURCES;
            break;
        case -HDCDRV_UB_INTERFACE_ERR:
            HDC_LOG_ERR("Remote ub resource init failed, check remote side log.(dev_id=%u; error_code=%d; "
                        "service=\"%s\")\n",
                        dev_id, msg_back->connect_msg_reply.server_error_code, hdc_get_sevice_str(service_type));
            ret = DRV_ERROR_NO_RESOURCES;
            break;
        default:
            HDC_LOG_ERR("Unknown error_code, check remote side log.(dev_id=%u; error_code=%d; service=\"%s\")\n",
                        dev_id, msg_back->connect_msg_reply.server_error_code, hdc_get_sevice_str(service_type));
            ret = DRV_ERROR_INNER_ERR;
            break;
    }

    return ret;
}

void hdc_ub_submit_close_event(struct hdc_ub_session *session, union hdcdrv_cmd *hdc_cmd)
{
    struct hdcdrv_event_msg msg = {0};
    union hdcdrv_cmd send_cmd = {0};
    signed int ret = 0;

    hdc_fill_event_msg(&msg, hdc_cmd, session, HDCDRV_NOTIFY_MSG_CLOSE);
    hdc_fill_send_ctrl_msg_cmd(&send_cmd, session->dev_id, (void *)&msg, sizeof(struct hdcdrv_event_msg));
    ret = hdc_send_ctrl_msg(session->bind_fd, &send_cmd);
    if (ret != 0) {
        HDC_LOG_WARN("hdc close remote session not success(ret=%d), will not stop local side close\n", ret);
    } else {
        HDC_LOG_INFO("hdc recv remote close reply.\n");
    }

    return;
}

STATIC void hdc_close_remote_session_for_connect_failed(struct hdc_ub_session *ub_session)
{
    union hdcdrv_cmd hdc_cmd = {0};

    hdc_cmd.close.ret = 0;
    hdc_cmd.close.session = (int)ub_session->local_id;
    hdc_cmd.close.remote_session = ub_session->remote_id;
    hdc_cmd.close.dev_id = (int)ub_session->dev_id;
    hdc_cmd.close.local_close_state = HDCDRV_CLOSE_TYPE_NOT_SET_OWNER;
    hdc_cmd.close.remote_local_state = HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST;
    hdc_ub_submit_close_event(ub_session, &hdc_cmd);

    return;
}

signed int hdc_ub_connect(signed int dev_id, struct hdc_client_head *p_head, signed int peer_pid,
                          struct hdc_session *p_session)
{
    struct hdc_time_record_for_connect connect_record = {0};
    struct hdc_mem_res_info mem_info = {0};
    struct hdc_ub_session *session = NULL;
    hdc_jfr_id_info_t *jetty_info = NULL;
    struct hdcdrv_event_msg msg = {0};
    signed int ret, ret_tmp, idx;
    union hdcdrv_cmd hdc_cmd;
    union hdcdrv_cmd send_cmd = {0};
    hdc_ub_context_t *ctx;

    hdc_get_time_record(&connect_record.connect_start, &connect_record.fail_times);
    // Process of event scheduling preprocessing
    ret = hdc_link_event_pre_init((unsigned int)dev_id, "halHdcSessionConnectEx");
    if (ret != 0) {
        HDC_LOG_ERR("hdc_link_event_pre_init failed in connect.(ret=%d; dev_id=%d; service_type=\"%s\")\n", ret, dev_id,
                    hdc_get_sevice_str(p_head->serviceType));
        return ret;
    }
    hdc_get_time_record(&connect_record.link_pre_init, &connect_record.fail_times);

    ret = hdc_mem_res_init(&mem_info, p_head->serviceType);
    if (ret != 0) {
        HDC_LOG_ERR("hdc_mem_res_init failed.(ret=%d; dev_id=%d)\n", ret, dev_id);
        goto uninit_event_thread;
    }
    hdc_get_time_record(&connect_record.mem_res_init, &connect_record.fail_times);

    ret = hdc_alloc_connect_session(&hdc_cmd, p_session, &mem_info, peer_pid, dev_id);
    if (ret != 0) {
        HDC_LOG_ERR("alloc session from kernel failed.(ret=%d; dev_id=%d; service_type=\"%s\")\n", ret, dev_id,
                    hdc_get_sevice_str(p_head->serviceType));
        goto uninit_mem_res;
    }
    hdc_get_time_record(&connect_record.alloc_session, &connect_record.fail_times);

    idx = hdc_get_lock_index(dev_id, p_session->sockfd);
    (void)mmMutexLock(&g_hdcConfig.session_lock[idx]);
    ret = hdc_ub_session_pre_init(p_session, &mem_info, (unsigned long long)peer_pid, hdc_cmd.connect.unique_val,
                                  "halHdcSessionConnectEx");
    if (ret != 0) {
        HDC_LOG_ERR("Pre init ub session failed.(ret=%d; dev_id=%d; session_id=%d; service_type=\"%s\")\n", ret, dev_id,
                    p_session->sockfd, hdc_get_sevice_str(p_head->serviceType));
        goto close_session;
    }
    hdc_get_time_record(&connect_record.pre_init, &connect_record.fail_times);

    session = p_session->ub_session;

    // alloc jetty resources
    ctx = hdc_create_ub_context((uint32_t)p_session->device_id, session, &connect_record.urma_func_cost);
    if (ctx == NULL) {
        HDC_LOG_ERR("Init UB context failed.(ret=%d; dev_id=%d; session_id=%d; service_type=\"%s\")\n", ret, dev_id,
                    p_session->sockfd, hdc_get_sevice_str(p_head->serviceType));
        ret = HDCDRV_ERR;
        goto session_uninit;
    }
    hdc_get_time_record(&connect_record.create_ub_ctx, &connect_record.fail_times);

    session->ctx = ctx;
    session->epoll_event_node->ctx = ctx;
    hdc_get_ub_res_info(session->ctx, &session->ub_res_info);
    ret = hdc_ub_add_ctl_to_thread_epoll(session);
    if (ret != 0) {
        goto delete_ub_context;
    }
    hdc_get_time_record(&connect_record.get_res_info_and_add_ctrl, &connect_record.fail_times);

    hdc_fill_event_msg(&msg, &hdc_cmd, session, HDCDRV_NOTIFY_MSG_CONNECT);
    hdc_fill_send_ctrl_msg_cmd(&send_cmd, dev_id, &msg, sizeof(struct hdcdrv_event_msg));
    ret = hdc_send_ctrl_msg(session->bind_fd, &send_cmd);
    hdc_get_time_record(&connect_record.send_ctrl_msg, &connect_record.fail_times);
    if (ret != 0) {
        HDC_LOG_WARN("send ctrl msg not success.(ret=%d)\n", ret);
        goto del_epoll_ctl;
    }

    ret = hdc_check_connect_wait(&msg, (uint32_t)dev_id, p_head->serviceType);
    if (ret != 0) {
        goto del_epoll_ctl;
    }
    hdc_get_time_record(&connect_record.check_reply, &connect_record.fail_times);
    // Save the information to the local session
    session->remote_id = msg.connect_msg_reply.server_session;
    session->peer_create_pid = msg.connect_msg_reply.server_pid;
    jetty_info = (hdc_jfr_id_info_t *)&msg.connect_msg_reply.jetty_info;
    // Import the jetty on the Server
    ret = hdc_import_remote_jetty(ctx, (hdc_jfr_id_info_t *)&msg.connect_msg_reply.jetty_info,
                                  &connect_record.urma_func_cost);
    if (ret != 0) {
        HDC_LOG_ERR("Client import remote jetty failed.(ret=%d; dev_id=%d; session_id=%d; service_type=\"%s\")\n", ret,
                    dev_id, p_session->sockfd, hdc_get_sevice_str(p_head->serviceType));
        // If the operation fails, does the peer end need to be notified to close the session??
        goto import_remote_jfr_failed;
    }

    p_session->ub_session->status = HDC_SESSION_STATUS_CONN;
    hdc_ub_fill_jetty_info(&session->jetty_info, session);
    hdc_get_time_record(&connect_record.free_tid_and_fill_jetty_info, &connect_record.fail_times);

    HDC_LOG_INFO("Connect success(l_id=%u; r_ids=%u; handle=%d; dev_id=%d; server=\"%s\";"
                 "l_jfr_4=%u; jfs=%u; jfc_r=%u; jfc_s=%u; r_jfr_4=%u; tjfr=%u)\n",
                 session->local_id, session->remote_id, p_session->bind_fd, dev_id,
                 hdc_get_sevice_str(p_head->serviceType), session->ctx->jfr.jfr->jfr_id.id,
                 session->ctx->jfs.jfs->jfs_id.id, session->ctx->jfc_recv.jfc->jfc_id.id,
                 session->ctx->jfc_send.jfc->jfc_id.id, jetty_info->jfr_id, ctx->tjfr->id.id);
    hdc_get_time_record(&connect_record.connect_end, &connect_record.fail_times);
    hdc_get_connect_time_cost(&connect_record, session, &send_cmd);
    (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
    return ret;

import_remote_jfr_failed:
    hdc_close_remote_session_for_connect_failed(session);
del_epoll_ctl:
    hdc_ub_del_ctl_to_thread_epoll(session);
delete_ub_context:
    hdc_delete_ub_context(session->ctx, (unsigned int)dev_id, session, NULL);
    session->ctx = NULL;
    ctx = NULL;
session_uninit:
    hdc_ub_session_uninit(p_session, dev_id, (int)session->local_id);
close_session:
    (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
    hdc_cmd.close.dev_id = dev_id;
    hdc_cmd.close.local_close_state = HDCDRV_CLOSE_TYPE_NONE;
    hdc_cmd.close.remote_local_state = HDCDRV_CLOSE_TYPE_NONE;
    hdc_cmd.close.session = p_session->sockfd;
    ret_tmp = hdc_ub_session_kernel_close(p_session->bind_fd, (unsigned int)dev_id, &hdc_cmd);
    if (ret_tmp != 0) {
        HDC_LOG_ERR("Kernel close failed.(ret=%d; session=%d; dev_id=%d).\n", ret_tmp, p_session->sockfd, dev_id);
    }
    p_session->bind_fd = HDC_SESSION_FD_INVALID;
uninit_mem_res:
    hdc_mem_res_uninit(&mem_info, (unsigned int)dev_id);
uninit_event_thread:
    hdc_link_event_pre_uninit(dev_id);
    return ret;
}

STATIC void hdc_accept_abnormal_reply(signed int dev_id, struct hdcdrv_cmd_accept *accept,
                                      struct hdcdrv_event_msg *msg_back, int result, int bind_fd)
{
    struct hdcdrv_event_msg msg = {0};
    union hdcdrv_cmd send_cmd = {0};
    int ret;

    msg.type = HDCDRV_NOTIFY_MSG_CONNECT_REPLY;
    msg.connect_msg_reply.server_error_code = result;
    msg.connect_msg_reply.client_session = msg_back->connect_msg.client_session;
    msg.connect_msg_reply.server_session = (unsigned int)accept->session;
    msg.connect_msg_reply.unique_val = accept->unique_val;

    hdc_fill_send_ctrl_msg_cmd(&send_cmd, dev_id, (void *)&msg, sizeof(struct hdcdrv_event_msg));
    ret = hdc_send_ctrl_msg(bind_fd, &send_cmd);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("hdc_send_ctrl_msg in AbnormalReply failed.(ret=%d; dev_id=%d)\n", ret, dev_id);
    }

    return;
}

signed int hdc_ub_accept(struct hdc_server_head *p_serv, signed int dev_id, signed int service_type,
                         struct hdc_session *p_session)
{
    struct hdc_time_record_for_accept accept_record = {0};
    struct hdcdrv_event_msg conn_reply = {0};
    struct hdc_mem_res_info mem_info = {0};
    struct hdcdrv_event_msg *msg_back = NULL;
    struct hdc_pipe_event_msg pipe_msg = {0};
    struct hdc_ub_session *session = NULL;
    hdc_jfr_id_info_t *jetty_info = NULL;
    signed int ret, ret_tmp, idx;
    union hdcdrv_cmd hdc_cmd;
    union hdcdrv_cmd send_cmd = {0};
    hdc_ub_context_t *ctx;

    hdc_get_time_record(&accept_record.accept_start, &accept_record.fail_times);
    HDC_LOG_DEBUG("hdc_ub_accept waiting for connection. device id=%u; service_type=%d\n", dev_id, service_type);

    ssize_t num = mm_read_file(p_serv->conn_wait, &pipe_msg, sizeof(pipe_msg));
    if (num != (ssize_t)sizeof(pipe_msg)) {
        if (num == 0) {
            HDC_LOG_WARN("read connection message data not success.(ret=%ld)\n", num);
            return -HDCDRV_DEVICE_RESET;
        } else {
            HDC_LOG_ERR("read connection message data failed.(ret=%ld)\n", num);
            return DRV_ERROR_FILE_OPS;
        }
    }

    // pipe_msg is a local buffer alive for the whole function, point to it instead of a stack copy
    msg_back = &pipe_msg.event_msg;
    accept_record.pipe_write_start = pipe_msg.pipe_write_start;

    hdc_get_time_record(&accept_record.conn_wait, &accept_record.fail_times);

    ret = hdc_mem_res_init(&mem_info, service_type);
    if (ret != 0) {
        HDC_LOG_ERR("hdc_mem_res_init failed.(ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }
    hdc_get_time_record(&accept_record.res_init, &accept_record.fail_times);

    // alloc session in kernel
    hdc_cmd.accept.dev_id = dev_id;
    hdc_cmd.accept.service_type = service_type;
    hdc_cmd.accept.remote_session = msg_back->connect_msg.client_session;
    hdc_cmd.accept.unique_val = msg_back->connect_msg.unique_val;
    hdc_cmd.accept.user_va = mem_info.user_va;

    ret = hdc_alloc_accept_session(&hdc_cmd, p_session, mem_info.bind_fd, dev_id);
    if (ret != 0) {
        if (ret == -HDCDRV_CONNECT_TIMEOUT) {
            HDC_LOG_WARN("The connect request has stayed over time.(ret=%d; dev_id=%d; service_type=\"%s\"; "
                         "local_id=%u; remote_id=%u)\n",
                         ret, dev_id, hdc_get_sevice_str(service_type), p_session->sockfd,
                         msg_back->connect_msg.client_session);
        } else {
            HDC_LOG_ERR("alloc session from kernel failed.(ret=%d; dev_id=%d; service_type=\"%s\")\n", ret, dev_id,
                        hdc_get_sevice_str(service_type));
        }
        goto mem_uninit_res;
    }
    hdc_get_time_record(&accept_record.alloc_session, &accept_record.fail_times);

    idx = hdc_get_lock_index(dev_id, p_session->sockfd);
    (void)mmMutexLock(&g_hdcConfig.session_lock[idx]);
    ret = hdc_ub_session_pre_init(p_session, &mem_info, hdc_cmd.accept.peer_pid, hdc_cmd.accept.unique_val,
                                  "drvHdcSessionAccept");
    if (ret != 0) {
        HDC_LOG_ERR("Pre init ub session failed.(ret=%d; dev_id=%d; session_id=%d; service_type=\"%s\")\n", ret, dev_id,
                    p_session->sockfd, hdc_get_sevice_str(service_type));
        hdc_accept_abnormal_reply(dev_id, &hdc_cmd.accept, msg_back, ret, mem_info.bind_fd);
        goto close_session;
    }
    hdc_get_time_record(&accept_record.pre_init, &accept_record.fail_times);

    session = p_session->ub_session;
    // alloc jetty resources
    ctx = hdc_create_ub_context((uint32_t)p_session->device_id, session, &accept_record.urma_func_cost);
    if (ctx == NULL) {
        HDC_LOG_ERR("Init UB context failed.(ret=%d; dev_id=%d; session_id=%d; service_type=\"%s\")\n", ret, dev_id,
                    p_session->sockfd, hdc_get_sevice_str(service_type));
        ret = -HDCDRV_UB_INTERFACE_ERR;
        hdc_accept_abnormal_reply(dev_id, &hdc_cmd.accept, msg_back, ret, mem_info.bind_fd);
        goto session_uninit;
    }
    hdc_get_time_record(&accept_record.create_ub_ctx, &accept_record.fail_times);

    session->ctx = ctx;
    session->epoll_event_node->ctx = ctx;
    hdc_get_ub_res_info(session->ctx, &session->ub_res_info);
    ret = hdc_ub_add_ctl_to_thread_epoll(session);
    if (ret != 0) {
        goto delete_ub_context;
    }
    hdc_get_time_record(&accept_record.add_ctrl, &accept_record.fail_times);

    jetty_info = (hdc_jfr_id_info_t *)&msg_back->connect_msg.jetty_info;
    ret = hdc_import_remote_jetty(ctx, (hdc_jfr_id_info_t *)&msg_back->connect_msg.jetty_info,
                                  &accept_record.urma_func_cost);
    if (ret != 0) {
        HDC_LOG_ERR("Import remote jetty failed.(ret=%d; dev_id=%d; session_id=%d; service_type=\"%s\")\n", ret, dev_id,
                    p_session->sockfd, hdc_get_sevice_str(service_type));
        hdc_accept_abnormal_reply(dev_id, &hdc_cmd.accept, msg_back, ret, mem_info.bind_fd);
        goto del_epoll_ctl;
    }

    // Save the information to the local session
    p_session->ub_session->remote_id = msg_back->connect_msg.client_session;
    p_session->close_eventfd = -1;

    hdc_fill_event_msg(&conn_reply, &hdc_cmd, p_session->ub_session, HDCDRV_NOTIFY_MSG_CONNECT_REPLY);
    hdc_fill_send_ctrl_msg_cmd(&send_cmd, dev_id, (void *)&conn_reply, sizeof(struct hdcdrv_event_msg));
    ret = hdc_send_ctrl_msg(session->bind_fd, &send_cmd);
    hdc_get_time_record(&accept_record.send_ctrl_msg, &accept_record.fail_times);
    if (ret != DRV_ERROR_NONE) {
        hdc_get_accept_time_cost(&accept_record, session, false, &send_cmd);
        HDC_LOG_ERR("hdc_send_ctrl_msg failed.(ret=%d; dev_id=%d; session_id=%d; service_type=\"%s\")\n", ret, dev_id,
                    p_session->sockfd, hdc_get_sevice_str(service_type));
        goto unimport_remote_jetty;
    }

    p_session->ub_session->status = HDC_SESSION_STATUS_CONN; // The session status is set to CONN
    hdc_ub_fill_jetty_info(&session->jetty_info, session);

    HDC_LOG_INFO("Accept success(l_id=%u; r_id=%u; handle=%d; devid=%d; server=\"%s\";"
                 "l_jfr_4=%u; jfs=%u; jfc_r=%u; jfc_s=%u; r_jfr_4=%u; tjfr=%u)\n",
                 session->local_id, session->remote_id, p_session->bind_fd, dev_id, hdc_get_sevice_str(service_type),
                 session->ctx->jfr.jfr->jfr_id.id, session->ctx->jfs.jfs->jfs_id.id,
                 session->ctx->jfc_recv.jfc->jfc_id.id, session->ctx->jfc_send.jfc->jfc_id.id, jetty_info->jfr_id,
                 ctx->tjfr->id.id);
    hdc_get_time_record(&accept_record.accept_end, &accept_record.fail_times);
    hdc_get_accept_time_cost(&accept_record, session, true, &send_cmd);
    (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
    return ret;

unimport_remote_jetty:
    hdc_unimport_remote_jetty(session->ctx);
del_epoll_ctl:
    hdc_ub_del_ctl_to_thread_epoll(session);
delete_ub_context:
    hdc_delete_ub_context(session->ctx, (unsigned int)dev_id, session, NULL);
    session->ctx = NULL;
    ctx = NULL;
session_uninit:
    hdc_ub_session_uninit(p_session, dev_id, (int)session->local_id);
close_session:
    (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
    hdc_cmd.close.dev_id = dev_id;
    hdc_cmd.close.local_close_state = HDCDRV_CLOSE_TYPE_NONE;
    hdc_cmd.close.remote_local_state = HDCDRV_CLOSE_TYPE_NONE;
    hdc_cmd.close.session = p_session->sockfd;
    ret_tmp = hdc_ub_session_kernel_close(p_session->bind_fd, (unsigned int)dev_id, &hdc_cmd);
    if (ret_tmp != 0) {
        HDC_LOG_ERR("Kernel close failed.(ret=%d; session=%d; dev_id=%d).\n", ret_tmp, p_session->sockfd, dev_id);
    }
    p_session->bind_fd = HDC_SESSION_FD_INVALID;
mem_uninit_res:
    hdc_mem_res_uninit(&mem_info, (unsigned int)dev_id);
    return ret;
}

signed int hdc_ub_set_session_owner(const struct hdc_session *p_session)
{
    struct hdc_ub_session *session = p_session->ub_session;

    if (session == NULL) {
        HDC_LOG_ERR("ub session is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (session->create_pid != (unsigned long long)getpid()) {
        HDC_LOG_ERR("In UB, create_pid cannot be different from the current process pid.(create_pid=%lld; pid=%d)\n",
                    session->create_pid, getpid());
        return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

STATIC void hdc_get_session_mem_info(struct hdc_ub_session *session, struct hdc_mem_res_info *mem_info)
{
    struct hdc_session *p_session = session->psession_ptr;

    mem_info->bind_fd = p_session->bind_fd;
    mem_info->user_va = session->user_va;
    mem_info->service_type = session->service_type;

    return;
}

STATIC void hdc_wake_up_recv_when_close(struct hdc_ub_epoll_node *node)
{
    pthread_mutex_lock(&node->mutex);
    node->pkt_num = -1;
    pthread_cond_signal(&node->cond);
    pthread_mutex_unlock(&node->mutex);
}

STATIC void hdc_fill_close_record(struct hdc_time_record_for_close *close_record, struct hdc_ub_session *session)
{
    close_record->dev_id = session->dev_id;
    close_record->local_id = session->local_id;
    close_record->service_type = session->service_type;
}

STATIC void hdc_fill_id_into_cmd(union hdcdrv_cmd *hdc_cmd, struct hdc_ub_session *session)
{
    hdc_cmd->close.session = (int)session->local_id;
    hdc_cmd->close.remote_session = session->remote_id;
    hdc_cmd->close.dev_id = (int)session->dev_id;
}

int hdc_ub_session_close_handle(struct hdc_session *p_session, unsigned int dev_id, union hdcdrv_cmd *hdc_cmd,
                                int close_flag, struct hdc_time_record_for_close *record)
{
    int close_state = hdc_cmd->close.local_close_state;
    signed int fd, idx, service_type, ret = 0;
    struct hdc_mem_res_info mem_info = {0};
    mmProcess handle = p_session->bind_fd;
    struct hdc_ub_session *session = NULL;
    unsigned long long peer_pid;
    int last_status;

    HDC_LOG_INFO("hdc_ub_session_close enter.(session=%d; dev_id=%d; flag=%d)\n", hdc_cmd->close.session, dev_id,
                 close_flag);
    hdc_get_time_record(&record->start_close_handle, &record->fail_times);
    fd = hdc_cmd->close.session;
    if ((fd >= HDCDRV_UB_SINGLE_DEV_MAX_SESSION) || (fd < 0) || (dev_id >= (unsigned int)hdc_get_max_device_num())) {
        HDC_LOG_ERR("session para invalid.(id=%d; dev_id=%d)\n", fd, dev_id);
        return DRV_ERROR_NOT_SUPPORT;
    }

    idx = hdc_get_lock_index((int)dev_id, fd);
    (void)mmMutexLock(&g_hdcConfig.session_lock[idx]);
    session = p_session->ub_session;
    if ((session == NULL) || (hdc_session_alive_check((int)dev_id, fd, hdc_cmd->close.unique_val) != 0)) {
        (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        record->close_type = HDCDRV_CLOSE_TYPE_NONE;
        HDC_LOG_INFO("session has been closed.\n");
        return DRV_ERROR_NONE;
    }

    if (close_state != HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST) {
        hdc_fill_id_into_cmd(hdc_cmd, session);
    }

    hdc_fill_close_record(record, session);
    last_status = session->status;
    session->status = HDC_SESSION_STATUS_IDLE; // The session status is set to IDLE
    session->local_close_state = hdc_cmd->close.local_close_state;
    session->remote_close_state = hdc_cmd->close.remote_local_state;
    g_hdcConfig.info_list[idx].status = HDC_SESSION_STATUS_IDLE;
    g_hdcConfig.info_list[idx].session = NULL;
    peer_pid = session->peer_create_pid;
    service_type = session->service_type;

    // stop recv data in msg
    hdc_ub_del_ctl_to_thread_epoll(session);
    hdc_get_time_record(&record->del_recv_epoll, &record->fail_times);
    /* The user closes the thread and needs to send a closing event to the peer side */
    if ((close_state == HDCDRV_CLOSE_TYPE_USER) && (close_flag == HDC_SESSION_CLOSE_FLAG_NORMAL) &&
        (last_status == HDC_SESSION_STATUS_CONN)) {
        hdc_ub_submit_close_event(session, hdc_cmd);
    }
    hdc_get_time_record(&record->sub_close_event, &record->fail_times);

    /* remove epoll event */
    if (p_session->epoll_head != NULL) {
        struct drvHdcEvent event = {HDC_EPOLL_DATA_IN, 0};
        drv_get_hdc_ub_epoll_ops()->hdc_epoll_ctl(p_session->epoll_head, HDC_EPOLL_CTL_DEL, session->psession_ptr,
                                                  &event);
    }
    hdc_get_time_record(&record->del_data_epoll, &record->fail_times);

    if (session->ctx != NULL) {
        pthread_mutex_lock(&session->epoll_event_node->mutex);
        session->epoll_event_node->ctx = NULL;
        pthread_mutex_unlock(&session->epoll_event_node->mutex);
        hdc_unimport_remote_jetty(session->ctx);
        hdc_get_time_record(&record->unimport_jetty, &record->fail_times);
        hdc_delete_ub_context(session->ctx, dev_id, session, &record->urma_uninit);
        session->ctx = NULL;
    }
    hdc_get_time_record(&record->del_ub_ctx, &record->fail_times);
    // Before unmap an address in user mode, the address mapping must be moved in kernel mode.
    ret = hdc_ub_session_kernel_close(handle, dev_id, hdc_cmd);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("hdc ioctl close failed.(ret=%d; dev_id=%u; session_id=%d; service_type=\"%s\")\n", ret, dev_id,
                    session->local_id, hdc_get_sevice_str(session->service_type));
        ret = DRV_ERROR_IOCRL_FAIL;
    }
    hdc_get_time_record(&record->close_kernel, &record->fail_times);

    hdc_get_session_mem_info(session, &mem_info);
    hdc_mem_res_uninit(&mem_info, dev_id);
    hdc_get_time_record(&record->mem_uninit, &record->fail_times);
    p_session->bind_fd = HDC_SESSION_FD_INVALID;
    hdc_wake_up_recv_when_close(session->epoll_event_node);
    hdc_get_time_record(&record->wake_recv, &record->fail_times);
    (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);

    HDC_LOG_INFO("hdc_ub_session_close exit, session_id:%d, handle=%d.\n", hdc_cmd->close.session, handle);

    hdc_touch_close_notify((int)dev_id, peer_pid, service_type);
    hdc_get_time_record(&record->close_notify, &record->fail_times);

    return ret;
}

signed int hdc_ub_session_close(unsigned int dev_id, struct hdc_session *p_session, int close_state, int close_flag)
{
    struct hdc_ub_session *ub_session = p_session->ub_session;
    struct hdc_time_record_for_close record = {0};
    union hdcdrv_cmd hdc_cmd;
    int ret = 0;

    /* If the value of session is NULL, it means that the session has
     * automatically closed when received the close event from the remote */
    if (ub_session != NULL) {
        record.close_type = HDCDRV_CLOSE_TYPE_USER;
        hdc_cmd.close.ret = 0;
        hdc_cmd.close.session = p_session->sockfd;
        hdc_cmd.close.local_close_state = close_state;
        hdc_cmd.close.remote_local_state = HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST;
        hdc_cmd.close.unique_val = ub_session->unique_val;
        ret = hdc_ub_session_close_handle(p_session, dev_id, &hdc_cmd, close_flag, &record);
        if (ret != 0) {
            HDC_LOG_ERR("close HDC session failed. (ret=%d, close state=%d)\n", ret, close_state);
        }
        // UbSession close when user call drvHdcSessionClose or halHdcCloseEx, remote close will not free session
        hdc_ub_session_free(p_session->ub_session);
        p_session->ub_session = NULL;
        hdc_get_time_record(&record.session_free, &record.fail_times);
    } else {
        HDC_LOG_INFO("ub session has been closed.(id=%d)\n", p_session->sockfd);
    }

    /* If the drvHdcEpollCtl function isn't be called to delete the HDC_EPOLL_SESSION_CLOSE event,
     * the event is automatically delete from epoll event list and close the close_eventfd */
    if (p_session->close_eventfd != -1) {
        struct drvHdcEvent event = {HDC_EPOLL_SESSION_CLOSE, 0};
        ret = drv_get_hdc_ub_epoll_ops()->hdc_epoll_ctl(p_session->epoll_head, HDC_EPOLL_CTL_DEL, p_session, &event);
        if (ret != 0) {
            HDC_LOG_WARN("delete HDC_EPOLL_SESSION_CLOSE event from epoll has problem.(ret=%d)\n", ret);
        }
    }
    hdc_get_time_record(&record.del_close_epoll, &record.fail_times);
    hdc_get_close_time_cost(&record);

    return ret;
}

struct hdc_ub_session *hdc_find_session_in_list(unsigned int fd, int dev_id, uint32_t unique_val, int idx)
{
    struct hdc_ub_session *psession = NULL;

    psession = g_hdcConfig.info_list[idx].session;
    if (psession != NULL) {
        if ((psession->local_id == fd) && (psession->dev_id == dev_id) && (psession->unique_val == unique_val)) {
            return psession;
        }
    }
    return NULL;
}

void hdc_remote_close_handle(struct hdc_remote_close_thread_para *msg_para)
{
    struct hdc_time_record_for_close record = {0};
    drvError_t ret;
    struct hdc_ub_session *session;
    struct hdc_session *p_session;
    union hdcdrv_cmd hdc_cmd;
    int idx;

    HDC_LOG_INFO("hdc_remote_close_handle enter after sleep 3s.(session=%u; dev_id=%d)\n", msg_para->local_session,
                 msg_para->dev_id);
    record.close_type = HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST;
    record.remote_close = &msg_para->record;
    // before session close, judge first to avoid wait
    idx = hdc_get_lock_index((int)msg_para->dev_id, (int)msg_para->local_session);
    (void)mmMutexLock(&g_hdcConfig.session_lock[idx]);
    session = hdc_find_session_in_list(msg_para->local_session, (int)msg_para->dev_id, msg_para->unique_val, idx);
    if ((session == NULL) || (session->status == HDC_SESSION_STATUS_IDLE)) {
        (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        HDC_LOG_INFO("Can not find session, session may close.(dev_id=%d; local_session=%u; remote_session=%u)\n",
                     msg_para->dev_id, msg_para->local_session, msg_para->remote_session);
        return;
    }

    p_session = session->psession_ptr;
    if ((p_session == NULL) || (p_session->magic != HDC_MAGIC_WORD)) {
        (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        HDC_LOG_ERR("Parameter psession_magic error.(dev_id=%d; session_id=%d)\n", msg_para->dev_id,
                    msg_para->local_session);
        return;
    }

    (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
    // Ioctl close
    hdc_cmd.close.session = (int)msg_para->local_session;
    hdc_cmd.close.remote_session = msg_para->remote_session;
    hdc_cmd.close.local_close_state = HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST;
    hdc_cmd.close.remote_local_state = msg_para->session_close_state;
    hdc_cmd.close.dev_id = (int)msg_para->dev_id;
    hdc_cmd.close.unique_val = msg_para->unique_val;
    ret = hdc_ub_session_close_handle(p_session, msg_para->dev_id, &hdc_cmd, HDC_SESSION_CLOSE_FLAG_NORMAL, &record);
    if (ret != 0) {
        HDC_LOG_ERR("close ub session failed.(ret=%d; dev_id=%d; session_id=%d)\n", ret, msg_para->dev_id,
                    msg_para->local_session);
        return;
    }

    /* Sends data to the server to close the session. */
    if (p_session->close_eventfd != -1) {
        uint64_t inc = 1;
        ret = mm_write_file(p_session->close_eventfd, &inc, sizeof(inc));
        if (ret != sizeof(inc)) {
            HDC_LOG_WARN("send close event not success.(ret=%d; id=%d; dev=%d)\n", ret, p_session->sockfd,
                         msg_para->dev_id);
        }
    }
    hdc_get_time_record(&record.write_file, &record.fail_times);
    hdc_get_close_time_cost(&record);

    HDC_LOG_INFO("hdc_remote_close_proc finish.(session=%u; dev_id=%d)\n", msg_para->local_session, msg_para->dev_id);
    return;
}

void hdc_fill_remote_close_thread_para(struct hdc_remote_close_thread_para *thread_para,
                                       struct hdcdrv_event_msg *msg_para, unsigned int dev_id,
                                       struct hdc_time_record_for_remote_close *record)
{
    thread_para->dev_id = dev_id;
    // close_msg.remote_session means local_session for local side, but remote_session for remote side
    thread_para->local_session = msg_para->close_msg.remote_session;
    thread_para->remote_session = msg_para->close_msg.local_session;
    thread_para->session_close_state = msg_para->close_msg.session_close_state;
    thread_para->unique_val = msg_para->close_msg.unique_val;
    memcpy_s(&thread_para->record, sizeof(struct hdc_time_record_for_remote_close), record,
             sizeof(struct hdc_time_record_for_remote_close));
}

STATIC int hdc_remote_close_thread_wait(void)
{
    struct timespec ts;
    int ret;

    pthread_mutex_lock(&g_remote_close_info.mutex);
    if (g_remote_close_info.closing) {
        pthread_mutex_unlock(&g_remote_close_info.mutex);
        return DRV_ERROR_UNINIT;
    }
    ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ret == 0) {
        ts.tv_sec += SESSION_REMOTE_CLOSED_TIME_US / CONVERT_S_TO_US;
        while (!g_remote_close_info.closing) {
            if (pthread_cond_timedwait(&g_remote_close_info.cond, &g_remote_close_info.mutex, &ts) == ETIMEDOUT) {
                break;
            }
        }
        pthread_mutex_unlock(&g_remote_close_info.mutex);
    } else {
        pthread_mutex_unlock(&g_remote_close_info.mutex);
        usleep(SESSION_REMOTE_CLOSED_TIME_US);
    }
    return 0;
}

void *hdc_remote_close_thread(void *thread_para)
{
    struct hdc_remote_close_thread_para *msg_para = (struct hdc_remote_close_thread_para *)thread_para;

    if (hdc_remote_close_thread_wait() == 0) {
        hdc_remote_close_handle(msg_para);
    }

    free(thread_para);

    // decrease thread count and signal when all threads done
    pthread_mutex_lock(&g_remote_close_info.mutex);
    g_remote_close_info.thread_count--;
    if (g_remote_close_info.thread_count == 0) {
        pthread_cond_broadcast(&g_remote_close_info.cond);
    }
    pthread_mutex_unlock(&g_remote_close_info.mutex);

    return NULL;
}

drvError_t hdc_remote_close_proc(uint32_t dev_id, void *msg, uint32_t msg_len)
{
    struct hdcdrv_event_msg *msg_para = (struct hdcdrv_event_msg *)msg;
    struct hdc_remote_close_thread_para thread_para_tmp = {0};
    struct hdc_remote_close_thread_para *thread_para = NULL;
    struct hdc_time_record_for_remote_close record;
    drvError_t ret;
    struct hdc_ub_session *session;
    int idx;
    pthread_t remote_close_thread;
    pthread_attr_t attr;

    (void)msg_len;

    if ((msg_para->close_msg.remote_session >= HDCDRV_UB_SINGLE_DEV_MAX_SESSION) ||
        (dev_id >= (unsigned int)hdc_get_max_device_num())) {
        HDC_LOG_ERR("Parameter error.(dev_id=%d; session_id=%d)\n", dev_id, msg_para->close_msg.remote_session);
        return DRV_ERROR_INVALID_VALUE;
    }

    HDC_LOG_INFO("hdc_remote_close_proc enter.(session=%u; dev_id=%u)\n", msg_para->close_msg.remote_session, dev_id);
    idx = hdc_get_lock_index((int)dev_id, (int)msg_para->close_msg.remote_session);
    (void)mmMutexLock(&g_hdcConfig.session_lock[idx]);

    // before session close, judge first to avoid wait
    session = hdc_find_session_in_list(msg_para->close_msg.remote_session, (int)dev_id, msg_para->close_msg.unique_val,
                                       idx);
    if ((session == NULL) || (session->status == HDC_SESSION_STATUS_IDLE)) {
        (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        HDC_LOG_INFO("Can not find session, session may close.(local_session=%u, remote_session=%u)\n",
                     msg_para->close_msg.remote_session, msg_para->close_msg.local_session);
        // session has closed, no need to do other things, return success
        return 0;
    }

    if (hdc_session_alive_check((int)dev_id, (int)msg_para->close_msg.remote_session, msg_para->close_msg.unique_val) !=
        0) {
        // If session has closed, no need to do other operation
        (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        return 0;
    } else {
        session->status = HDC_SESSION_STATUS_REMOTE_CLOSE; // The session status is set to REMOTE CLOSE
        // When remote close, remote jfr has been destroy, local side won't support send, so destroy jfs and jfc_send
        hdc_get_time_record(&record.close_proc, &record.fail_times);
        hdc_uninit_jfs_res(session->ctx, &session->ctx->jfs);
        hdc_get_time_record(&record.del_jfs, &record.fail_times);
        hdc_uninit_jfc_res(session->ctx, &session->ctx->jfc_send);
        hdc_get_time_record(&record.del_jfcs, &record.fail_times);
    }

    if (msg_para->close_msg.session_close_state == HDCDRV_CLOSE_TYPE_RELEASE) {
        HDC_LOG_INFO("Remote close status is release, no need to wait 3s, close session immediately.\n");
        goto remote_close_immediately;
    }

    thread_para = (struct hdc_remote_close_thread_para *)malloc(sizeof(struct hdc_remote_close_thread_para));
    if (thread_para == NULL) {
        HDC_LOG_WARN("malloc thread_para not success, close session immediately without sleep 3s.\n");
        goto remote_close_immediately;
    }

    hdc_fill_remote_close_thread_para(thread_para, msg_para, dev_id, &record);

    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_mutex_lock(&g_remote_close_info.mutex);
    if (g_remote_close_info.closing) {
        pthread_mutex_unlock(&g_remote_close_info.mutex);
        (void)pthread_attr_destroy(&attr);
        goto pthread_failed;
    }
    g_remote_close_info.thread_count++;
    pthread_mutex_unlock(&g_remote_close_info.mutex);

    ret = pthread_create(&remote_close_thread, &attr, hdc_remote_close_thread, (void *)thread_para);
    if (ret != 0) {
        HDC_LOG_WARN("pthread_create not success, close session immediately without sleep 3s.\n");
        (void)pthread_attr_destroy(&attr);
        // decrease thread count if create failed
        pthread_mutex_lock(&g_remote_close_info.mutex);
        g_remote_close_info.thread_count--;
        pthread_mutex_unlock(&g_remote_close_info.mutex);
        goto pthread_failed;
    }

    (void)pthread_attr_destroy(&attr);
    (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);

    return 0;

pthread_failed:
    free(thread_para);
remote_close_immediately:
    hdc_fill_remote_close_thread_para(&thread_para_tmp, msg_para, dev_id, &record);
    (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
    hdc_remote_close_handle(&thread_para_tmp);

    return 0;
}

signed int hdc_ub_server_create(mmProcess handle, signed int dev_id, signed int service_type,
                                struct hdc_server_head *p_head)
{
    union hdcdrv_cmd hdc_cmd;
    signed int ret;
    int pipe_fd[2];

    p_head->conn_wait = -1;
    p_head->conn_notify = -1;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = hdc_link_event_pre_init((unsigned int)dev_id, "drvHdcServerCreate");
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("hdc_link_event_pre_init failed in server create.(ret=%d; dev_id=%d)\n", ret, dev_id);
        return ret;
    }

    if (pipe(pipe_fd) < 0) {
        HDC_LOG_ERR("pipe error\n");
        hdc_link_event_pre_uninit(dev_id);
        return DRV_ERROR_NO_RESOURCES;
    }

    hdc_cmd.server_create.dev_id = dev_id;
    hdc_cmd.server_create.service_type = service_type;
    ret = hdc_ub_ioctl(handle, HDCDRV_SERVER_CREATE, &hdc_cmd);
    if (ret != DRV_ERROR_NONE) {
        hdc_link_event_pre_uninit(dev_id);
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return ret;
    }

    p_head->deviceId = dev_id;
    p_head->conn_wait = pipe_fd[0];
    p_head->conn_notify = pipe_fd[1];
    g_hdcConfig.server_list[dev_id][service_type] = (HDC_SERVER)p_head;

    HDC_LOG_DEBUG("create hdc server success.(device id=%d; service_type=%u)\n", p_head->deviceId, p_head->serviceType);
    /* create a pipe for accepting the connect request */
    return DRV_ERROR_NONE;
}

hdcError_t hdc_ub_server_destroy(struct hdc_server_head *p_serv, signed int dev_id, signed int service_type)
{
    union hdcdrv_cmd hdc_cmd;
    signed int ret;

    mmProcess handle = p_serv->bind_fd;
    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (p_serv->conn_wait != -1) {
        close(p_serv->conn_wait);
        p_serv->conn_wait = -1;
    }
    if (p_serv->conn_notify != -1) {
        close(p_serv->conn_notify);
        p_serv->conn_notify = -1;
    }

    g_hdcConfig.server_list[dev_id][service_type] = NULL;
    hdc_cmd.server_create.dev_id = dev_id;
    hdc_cmd.server_create.service_type = service_type;
    ret = hdc_ub_ioctl(handle, HDCDRV_SERVER_DESTROY, &hdc_cmd);
    return ret;
}

STATIC bool hdc_check_alive_after_poll_jfc(urma_cr_status_t cr_status, struct hdc_ub_session *session, int tx_rx_flag)
{
    int session_status = session->status;
    if ((session_status != HDC_SESSION_STATUS_CONN) && (tx_rx_flag == HDC_UB_TX)) {
        return false;
    }

    if ((session_status == HDC_SESSION_STATUS_IDLE) && (tx_rx_flag == HDC_UB_RX)) {
        return false;
    }

#if !defined(CFG_PLATFORM_ESL) && !defined(CFG_PLATFORM_FPGA)
    if ((cr_status == URMA_CR_REM_ACCESS_ABORT_ERR) && (tx_rx_flag == HDC_UB_TX)) {
        HDC_RUN_LOG_INFO("remote session has closed. (type=%d; status=%d; jfc_id=%u; l_id=%u; "
                         "r_id=%u; dev_id=%d; session_status=%d; service_type=\"%s\")\n",
                         tx_rx_flag, cr_status, hdc_get_jfc_id_by_type(&session->jetty_info, tx_rx_flag),
                         session->local_id, session->remote_id, session->dev_id, session_status,
                         hdc_get_sevice_str(session->service_type));
        return false;
    }
#else
    (void)cr_status;
#endif

    return true;
}

int hdc_poll_jfc(hdc_urma_jfc_t *hdc_jfc, urma_cr_t *cr, struct hdc_ub_session *session, int tx_rx_flag,
                 int service_type)
{
    static uint32_t rx_empty_cnt = 0;
    bool rx_empty = false;
    int cnt = 0, ret = 0;
    u32 token_id = 0;

    if ((session->ctx != NULL) && (session->ctx->token_id != NULL)) {
        token_id = session->ctx->token_id->token_id;
    }

    cnt = urma_poll_jfc(hdc_jfc->jfc, 1, cr);
    if (cnt <= 0 || cr->status != URMA_CR_SUCCESS) {
        /* URMA_CR_REM_ACCESS_ABORT_ERR for ESL / URMA_CR_RNR_RETRY_CNT_EXC_ERR for RM mode /
            URMA_CR_ACK_TIMEOUT_ERR for RC mode */
        if ((cr->status == HDC_TIMEOUT_CQE_STATUS) && (tx_rx_flag == HDC_UB_TX)) {
            hdc_jfc_dbg_fill(tx_rx_flag, session, HDC_URMA_POLL_FAIL_BY_REM_ACESS_ABORT);
            ret = HDC_TIMEOUT_RETRY;
        } else if ((cr->status == URMA_CR_ACK_TIMEOUT_ERR) && (tx_rx_flag == HDC_UB_TX)) {
            HDC_RUN_LOG_INFO("retransmission exceeds the maximum number of times. (type=%d; cnt=%d; jfc_id=%u; "
                             "l_id=%u; r_id=%u; dev_id=%d; service_type=\"%s\")\n",
                             tx_rx_flag, cnt, hdc_get_jfc_id_by_type(&session->jetty_info, tx_rx_flag),
                             session->local_id, session->remote_id, session->dev_id, hdc_get_sevice_str(service_type));
            hdc_jfc_dbg_fill(tx_rx_flag, session, HDC_URMA_POLL_FAIL_BY_REM_ACESS_ABORT);
            ret = HDC_TIMEOUT_REBUILD;
        } else if (!hdc_check_alive_after_poll_jfc(cr->status, session, tx_rx_flag)) {
            // session remote close when jfs send, return session has closed
            ret = -HDCDRV_SESSION_HAS_CLOSED;
        } else {
            if ((cnt == 0) && (cr->status == URMA_CR_SUCCESS) && (tx_rx_flag == HDC_UB_RX)) {
                rx_empty = true;
                rx_empty_cnt++;
            }
            HDC_POLL_JFC_LOG_ERROR(
                rx_empty,
                "Failed to poll jfc. (type=%d; cnt=%d; jfc_id=%u; l_id=%u; r_id=%u; "
                "dev_id=%d; session_status=%d; service_type=\"%s\"; cr_status=%d; rx_empty=%u; token_id=%u)\n",
                tx_rx_flag, cnt, hdc_get_jfc_id_by_type(&session->jetty_info, tx_rx_flag), session->local_id,
                session->remote_id, session->dev_id, session->status, hdc_get_sevice_str(service_type), cr->status,
                rx_empty_cnt, token_id);
            hdc_ub_get_urma_dfx_by_flag(session, session->ctx, tx_rx_flag);
            hdc_jfc_dbg_fill(tx_rx_flag, session, HDC_URMA_POLL_FAIL);
            ret = DRV_ERROR_TRANS_LINK_ABNORMAL;
        }
    }

    return ret;
}

// for ack jfc and rearm jfc, if wait jfc succ but not ack jfc, jfc will not be deleted
void hdc_ack_jfc(hdc_urma_jfc_t *hdc_jfc)
{
    uint32_t ack_cnt = 1;

    // hdc_jfc->jfc = ev_jfc
    urma_ack_jfc((urma_jfc_t **)&hdc_jfc->jfc, &ack_cnt, 1);

    return;
}

// for ack jfc and rearm jfc, if wait jfc succ but not ack jfc, jfc will not be deleted
int hdc_rearm_jfc(hdc_urma_jfc_t *hdc_jfc, struct hdc_ub_session *session, int tx_rx_flag)
{
    int ret = 0;

    ret = urma_rearm_jfc(hdc_jfc->jfc, false);
    if (ret != URMA_SUCCESS) {
        HDC_LOG_ERR("Failed to rearm jfc. (ret=%d; jfc_id=%u; l_id=%u; r_id=%u; dev_id=%d)\n", ret,
                    hdc_get_jfc_id_by_type(&session->jetty_info, tx_rx_flag), session->local_id, session->remote_id,
                    session->dev_id);
        hdc_ub_get_urma_dfx(session, NULL, NULL, hdc_jfc);
        hdc_jfc_dbg_fill(tx_rx_flag, session, HDC_URMA_REARM_FAIL);
        return DRV_ERROR_TRANS_LINK_ABNORMAL;
    }

    return 0;
}

STATIC unsigned long hdc_get_time_cost_static(struct timespec *start_time)
{
    struct timespec now_time;
    unsigned long long timecost = 0;

    (void)clock_gettime(CLOCK_MONOTONIC, &now_time);
    timecost = hdc_get_time_cost(start_time, &now_time);
    timecost = timecost / CONVERT_MS_TO_US;

    return (unsigned long)timecost;
}

struct hdc_wait_retry_info {
    int timeout_retry_cnt;      // retry_time for wait timeout
    int non_retry_cnt;          // retry_time for non block
    int wait;                   // wait flag, for 3 types
    int wait_time;              // remain wait time for jfc_wait
    struct timespec start_time; // recore start time for cal wait time
    unsigned long timeout;      // user timeout
};

STATIC void hdc_init_wait_and_retry_info(signed int wait, unsigned long timeout, struct hdc_wait_retry_info *wait_info)
{
    if (wait == HDC_NOWAIT) {
        wait_info->wait_time = 1;
    } else if (wait == HDC_WAIT_TIMEOUT) {
        wait_info->timeout = timeout;
        (void)clock_gettime(CLOCK_MONOTONIC, &wait_info->start_time);
    } else {
        wait_info->wait_time = -1;
    }

    wait_info->timeout_retry_cnt = 0;
    wait_info->non_retry_cnt = 0;
    wait_info->wait = wait;

    return;
}

STATIC int hdc_fill_jfs_wr(void *user_data, urma_sge_t *src, urma_jfs_wr_t *wr, uint32_t seg_id,
                           struct hdc_ub_session *session)
{
    hdc_ub_context_t *ctx = session->ctx;
    urma_target_seg_t *tseg = ctx->tseg;
    uint64_t seg_va, seg_len;
    int ret = 0;

    wr->tjetty = ctx->tjfr;

    seg_va = session->user_va + seg_id * HDC_MEM_BLOCK_SIZE;
    seg_len = HDC_MEM_BLOCK_SIZE;
    ret = memcpy_s((void *)(uintptr_t)seg_va, seg_len, user_data, src->len);
    if (ret != 0) {
        HDC_LOG_ERR("Failed to copy user data.\n");
        return ret;
    }

    src->addr = seg_va;
    src->tseg = tseg;

    wr->flag.bs.complete_enable = 1;
    wr->opcode = URMA_OPC_SEND;
    wr->user_ctx = (uint64_t)seg_id;
    wr->send.src.sge = src;
    wr->send.src.num_sge = 1;
    wr->next = NULL;

    return 0;
}

STATIC int hdc_jfc_process(struct hdc_ub_session *session, struct hdc_wait_retry_info *wait_info,
                           struct hdc_time_record_for_single_send *send_record)
{
    int cnt = 0, type = session->service_type;
    hdc_ub_context_t *ctx = session->ctx;
    unsigned long cost_time = 0;
    urma_cr_t cr = {0};
    urma_jfc_t *ev_jfc;
    int ret = 0;

    hdc_get_time_record(&send_record->wait_jfc_send_start, &send_record->fail_times);
    // for send, must wait for jfc cqe come back, so timeout is -1
    cnt = ascend_urma_wait_jfc(ctx->jfc_send.jfce, 1, -1, &ev_jfc);
    if (cnt != 1 || ctx->jfc_send.jfc != ev_jfc) {
        HDC_LOG_ERR("Wait jfc has problem.(cnt=%d; jfc_id=%u; l_id=%u; r_id=%u; dev_id=%d; service_type=\"%s\")\n", cnt,
                    hdc_get_jfc_id_by_type(&session->jetty_info, HDC_UB_TX), session->local_id, session->remote_id,
                    session->dev_id, hdc_get_sevice_str(type));
        hdc_ub_get_urma_dfx(session, &ctx->jfs, NULL, &ctx->jfc_send);
        hdc_jfc_dbg_fill(HDC_UB_TX, session, HDC_URMA_WAIT_FAIL);
        return DRV_ERROR_TRANS_LINK_ABNORMAL;
    }

    wmb();
    hdc_get_time_record(&send_record->wait_jfc_send_finish, &send_record->fail_times);

    ret = hdc_poll_jfc(&ctx->jfc_send, &cr, session, HDC_UB_TX, type);
    if (ret != 0) {
        goto send_poll_error;
    }

    wmb();
    hdc_get_time_record(&send_record->poll_jfc_send, &send_record->fail_times);

    hdc_ack_jfc(&ctx->jfc_send);
    ret = hdc_rearm_jfc(&ctx->jfc_send, session, HDC_UB_TX);
    return ret;

send_poll_error:
    // here poll jfc has already have error, no need to care rearm error
    hdc_ack_jfc(&ctx->jfc_send);
    (void)hdc_rearm_jfc(&ctx->jfc_send, session, HDC_UB_TX);

    if ((ret == DRV_ERROR_TRANS_LINK_ABNORMAL) || (ret == HDC_TIMEOUT_REBUILD)) {
        return ret;
    }

    // for no wait, if cr status is not 0, return error
    if (wait_info->wait == HDC_NOWAIT) {
        return -HDCDRV_NO_BLOCK;
    }

    // for wait, if timeout is less than timecost, return timeout to user
    if (wait_info->wait == HDC_WAIT_TIMEOUT) {
        cost_time = hdc_get_time_cost_static(&wait_info->start_time);
        if (cost_time >= wait_info->timeout) {
            return -HDCDRV_TX_TIMEOUT;
        }
    }

    if (ret != -HDCDRV_SESSION_HAS_CLOSED && ret != HDC_TIMEOUT_RETRY) {
        HDC_LOG_ERR("hdc_poll_jfc_wait has problem.(dev_id=%d; session_id=%d; wait_flag=%d; wait_timeout_ms=%d; "
                    "remain_wait_time_ms=%lu; ret=%d; timeout_retry=%d; non_block_retry=%d)\n",
                    session->dev_id, session->local_id, wait_info->wait, wait_info->wait_time, wait_info->timeout, ret,
                    wait_info->timeout_retry_cnt, wait_info->non_retry_cnt);
    }

    return ret;
}

STATIC int hdc_update_timeout_and_status(struct hdc_wait_retry_info *wait_info, struct hdc_ub_session *session)
{
    unsigned long cost_time;

    // Only allow send when status == CONN
    if (session->status != HDC_SESSION_STATUS_CONN) {
        return -HDCDRV_SESSION_HAS_CLOSED;
    }

    // for no_wait or wait_always, timeout is fixed, no need to update
    if (wait_info->wait != HDC_WAIT_TIMEOUT) {
        return 0;
    }

    cost_time = hdc_get_time_cost_static(&wait_info->start_time);
    if (cost_time < wait_info->timeout) {
        wait_info->wait_time = (int)(wait_info->timeout - cost_time);
    } else if (cost_time == wait_info->timeout) {
        wait_info->wait_time = 1;
    } else {
        return -HDCDRV_TX_TIMEOUT;
    }

    return 0;
}

STATIC int hdc_rebuild_jfs(struct hdc_ub_session *session, hdc_ub_context_t *ctx)
{
    int ret;

    hdc_uninit_jfs_res(ctx, &ctx->jfs);
    hdc_uninit_jfc_res(ctx, &ctx->jfc_send);
    ret = hdc_init_jfc_res(ctx, &ctx->jfc_send);
    if (ret != 0) {
        HDC_LOG_ERR("Rebuild jfc resource failed. (dev_id=%u;session_id=%u;service=\"%s\";ret=%d)\n", ctx->devid,
                    ctx->session_fd, hdc_get_sevice_str(session->service_type), ret);
        return HDCDRV_ERR;
    }

    ret = hdc_init_jfs_res(ctx, &ctx->jfs, &ctx->jfc_send, session);
    if (ret != 0) {
        HDC_LOG_ERR("Rebuild jfs resource failed. (dev_id=%u;session_id=%u;service=\"%s\";ret=%d)\n", ctx->devid,
                    ctx->session_fd, hdc_get_sevice_str(session->service_type), ret);
        hdc_uninit_jfc_res(ctx, &ctx->jfc_send);
        return HDCDRV_ERR;
    }
    return ret;
}

STATIC int hdc_init_jfs_wr(struct hdc_ub_session *session, struct hdc_ub_send_info *send_info,
                           struct hdc_wait_retry_info *wait_info, struct hdc_time_record_for_single_send *send_record)
{
    uint32_t user_len = (uint32_t)send_info->buf_list->len;
    void *user_data = send_info->buf_list->pBuf;
    hdc_ub_context_t *ctx = session->ctx;
    int ret;

    hdc_get_time_record(&send_record->ub_send_start, &send_record->fail_times);
    wmb();

    if (user_len > HDC_MEM_BLOCK_SIZE) {
        HDC_LOG_ERR("len exceed max segment(max_segment=%d, user_len=%u).\n", HDC_MEM_BLOCK_SIZE, user_len);
        session->dbg_stat.tx_fail_hdc++;
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = hdc_get_jfs_tseg(session, &ctx->jfs, &send_info->seg_id, wait_info->wait_time);
    if (ret != 0) {
        session->dbg_stat.tx_fail_hdc++;
        HDC_LOG_ERR("Alloc jfs tseg failed. (dev_id=%u;session_id=%u;wait_time_ms=%u;ret=%d)\n", ctx->devid,
                    ctx->session_fd, wait_info->wait_time, ret);
        return ret;
    }
    wmb();
    hdc_get_time_record(&send_record->find_idle_block, &send_record->fail_times);

    send_info->src.len = user_len;
    ret = hdc_fill_jfs_wr(user_data, &send_info->src, &send_info->wr, send_info->seg_id, session);
    if (ret != 0) {
        session->dbg_stat.tx_fail_hdc++;
        hdc_put_jfs_tseg(ctx, &ctx->jfs, send_info->seg_id);
        return ret;
    }
    wmb();
    hdc_get_time_record(&send_record->fill_jfs_wr, &send_record->fail_times);
    return 0;
}

STATIC int hdc_post_jfs_wr(struct hdc_ub_session *session, struct hdc_ub_send_info *send_info,
                           struct hdc_wait_retry_info *wait_info, struct hdc_time_record_for_single_send *send_record)
{
    hdc_ub_context_t *ctx = session->ctx;
    urma_jfs_wr_t *bad_wr = NULL;
    int ret;

    ret = hdc_update_timeout_and_status(wait_info, session);
    if (ret != 0) {
        goto send_result_process;
    }

    wmb();
    hdc_get_time_record(&send_record->post_jfs_wr_start, &send_record->fail_times);
    ret = urma_post_jfs_wr(ctx->jfs.jfs, &send_info->wr, &bad_wr);
    if (ret != 0) {
        session->dbg_stat.tx_fail_ub++;
        HDC_LOG_ERR("Failed to post jfs send wr(ret=%d; jfs_id=%u; l_id=%u; r_id=%u; dev_id=%u)\n", ret,
                    ctx->jfs.jfs->jfs_id.id, session->local_id, session->remote_id, session->dev_id);
        hdc_put_jfs_tseg(ctx, &ctx->jfs, send_info->seg_id);
        return ret;
    }

    ret = hdc_jfc_process(session, wait_info, send_record);
    if (ret == HDC_TIMEOUT_RETRY) {
        hdc_put_jfs_tseg(ctx, &ctx->jfs, send_info->seg_id);
        return ret;
    }

send_result_process:
    hdc_put_jfs_tseg(ctx, &ctx->jfs, send_info->seg_id);
    wmb();
    if (ret == 0) {
        session->dbg_stat.tx++;
        session->dbg_stat.tx_bytes += (unsigned long long)send_info->buf_list->len;
    } else if (ret == HDC_TIMEOUT_REBUILD) {
        ret = hdc_rebuild_jfs(session, ctx);
        if (ret == 0) {
            ret = DRV_ERROR_TRANS_LINK_ABNORMAL;
        }
    } else {
        if (ret != -HDCDRV_SESSION_HAS_CLOSED) {
            HDC_LOG_ERR(
                "Failed to get cqe for send.(wait_flag=%d; retry_cnt=%d; l_id=%u; r_id=%u; dev_id=%d; ret=%d)\n",
                wait_info->wait, wait_info->timeout_retry_cnt, session->local_id, session->remote_id, session->dev_id,
                ret);
        }
    }

    return ret;
}

STATIC void hdc_get_recv_wait_timespec(struct timespec *start_time, struct timespec *end_time, int timeout)
{
    clock_gettime(CLOCK_MONOTONIC, start_time);
    end_time->tv_sec = start_time->tv_sec + timeout / CONVERT_MS_TO_S;
    end_time->tv_nsec = start_time->tv_nsec + timeout % CONVERT_MS_TO_S * CONVERT_MS_TO_NS;
    if (end_time->tv_nsec >= CONVERT_S_TO_NS) {
        end_time->tv_sec++;
        end_time->tv_nsec -= CONVERT_S_TO_NS;
    }
}

#define HDC_TSD_RETRY_TIME 300
STATIC int hdc_wait_epoll_notify(struct hdc_ub_session *session, int timeout, struct hdc_recv_config *recv_config,
                                 int *len)
{
    struct hdc_ub_epoll_node *node = session->epoll_event_node;
    struct timespec start_time, end_time;
    int ret = 0, retry_time = 0;
    int service_type;

    if (node == NULL) {
        return -HDCDRV_SESSION_HAS_CLOSED;
    }

    if (recv_config->wait == HDC_WAIT_TIMEOUT) {
        hdc_get_recv_wait_timespec(&start_time, &end_time, timeout);
    }

    pthread_mutex_lock(&node->mutex);
    node->ref++;
    service_type = session->service_type;
    while (hdc_rx_list_is_empty(node) && (node->pkt_num != -1)) {
        if (recv_config->wait == HDC_NOWAIT) {
            if ((service_type == HDC_SERVICE_TYPE_TSD) && (retry_time < HDC_TSD_RETRY_TIME)) {
                retry_time++;
                pthread_mutex_unlock(&node->mutex);
                usleep(10000); // 10000us = 10ms
                pthread_mutex_lock(&node->mutex);
                continue;
            }
            ret = -HDCDRV_NO_BLOCK;
            break;
        } else if (recv_config->wait == HDC_WAIT_ALWAYS) {
            ret = pthread_cond_wait(&node->cond, &node->mutex);
        } else {
            ret = pthread_cond_timedwait(&node->cond, &node->mutex, &end_time);
        }
        if (ret != EINTR && ret != 0) {
            break;
        }
    }

    if (ret == ETIMEDOUT) {
        ret = -HDCDRV_RX_TIMEOUT;
    } else if (node->pkt_num == -1) {
        ret = -HDCDRV_SESSION_HAS_CLOSED;
    } else if ((ret != 0) && (ret != -HDCDRV_NO_BLOCK)) {
        HDC_LOG_ERR("pthread_cond has problem.(error=%d; service=\"%s\").\n", ret, hdc_get_sevice_str(service_type));
        ret = DRV_ERROR_INNER_ERR;
    }
    if (ret == 0) {
        *len = (int)node->rx_list[node->head].len;
    }
    node->ref--;
    pthread_mutex_unlock(&node->mutex);

    return ret;
}

STATIC int hdc_recv_ub_msg_to_rx_buf(hdc_ub_context_t *ctx, int timeout, signed int *len,
                                     struct hdc_ub_session *session, struct hdc_recv_config *recv_config)
{
    int idx = hdc_get_lock_index(session->dev_id, (int)session->local_id);
    int ret;

    (void)ctx;

    ret = hdc_wait_epoll_notify(session, timeout, recv_config, len);
    if (ret != 0) {
        return ret;
    }

    mmMutexLock(&g_hdcConfig.session_lock[idx]);
    if (hdc_session_alive_check(session->dev_id, (int)session->local_id, session->unique_val) != 0) {
        mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        return -HDCDRV_SESSION_HAS_CLOSED;
    }
    mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
    return 0;
}

signed int hdc_ub_send(const struct hdc_session *p_session, struct drvHdcMsg *p_msg, signed int wait,
                       unsigned int timeout)
{
    int ret, idx;
    struct hdc_ub_session *session;
    hdc_ub_context_t *ctx;
    struct hdc_time_record_for_single_send send_record = {0};
    struct hdc_wait_retry_info wait_info = {0};
    struct hdc_ub_send_info send_info = {0};

    ret = drv_hdc_ub_session_para_check(p_session);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Input parameter session is invalid. (dev_id=%u; sockfd=%d)\n", p_session->device_id,
                    p_session->sockfd);
        return ret;
    }
    idx = hdc_get_lock_index((int)p_session->device_id, p_session->sockfd);
    session = p_session->ub_session;
    (void)mmMutexLock(&g_hdcConfig.session_lock[idx]);
    if (!hdc_check_session_valid_by_idx(&g_hdcConfig, idx, session)) {
        HDC_LOG_WARN("Session has been closed. (dev_id=%u; sockfd=%d)\n", p_session->device_id, p_session->sockfd);
        (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        return -HDCDRV_SESSION_HAS_CLOSED;
    }
    ctx = session->ctx;

    if (ctx->jfs.jfs == NULL) {
        ret = hdc_rebuild_jfs(session, ctx);
        if (ret != 0) {
            (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
            HDC_LOG_ERR("The JFS of the input parameter session is invalid, rebuild failed."
                        "(ret=%d; dev_id=%u; sockfd=%d)\n",
                        ret, p_session->device_id, p_session->sockfd);
            return HDCDRV_ERR;
        }
    }
    hdc_init_wait_and_retry_info(wait, (unsigned long)timeout, &wait_info);
    send_info.buf_list = &(p_msg->bufList[0]);
post_jfs_wr_again:
    ret = hdc_init_jfs_wr(session, &send_info, &wait_info, &send_record);
    if (ret != 0) {
        (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        if (ret != -HDCDRV_SESSION_HAS_CLOSED) {
            HDC_LOG_ERR("Failed to init wr.(ret=%d; l_id=%u; r_id=%u; dev_id=%d)\n", ret, session->local_id,
                        session->remote_id, session->dev_id);
        }
        return ret;
    }

    ret = hdc_post_jfs_wr(session, &send_info, &wait_info, &send_record);
    if (ret == HDC_TIMEOUT_RETRY) {
        (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        usleep(1000);
        wait_info.timeout_retry_cnt++;
        (void)mmMutexLock(&g_hdcConfig.session_lock[idx]);
        if (!hdc_check_session_valid_by_idx(&g_hdcConfig, idx, session)) {
            HDC_LOG_WARN("Session has been closed. (dev_id=%u; sockfd=%d)\n", p_session->device_id, p_session->sockfd);
            (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
            return -HDCDRV_SESSION_HAS_CLOSED;
        } else {
            goto post_jfs_wr_again;
        }
    } else if (ret != 0) {
        (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        if (ret != -HDCDRV_SESSION_HAS_CLOSED) {
            HDC_LOG_ERR("Failed to send wr.(ret=%d; l_id=%u; r_id=%u; dev_id=%d)\n", ret, session->local_id,
                        session->remote_id, session->dev_id);
        }
        return ret;
    }

    hdc_get_time_record(&send_record.ub_send_end, &send_record.fail_times);
    hdc_get_send_time_cost(&send_record, session);
    (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
    return ret;
}

/* Some services use len=0 as the termination receiving condition, so no log is printed */
signed int hdc_ub_recv_peek(const struct hdc_session *p_session, signed int *len, struct hdc_recv_config *recv_config)
{
    int ret;
    struct hdc_ub_session *session;
    signed int recv_peek_len = 0;
    hdc_ub_context_t *ctx;
    int wait_time;

    // 0 is normal recv, return one rx len to user
    if (recv_config->group_flag != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = drv_hdc_ub_session_para_check(p_session);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (recv_config->wait == HDC_NOWAIT) {
        wait_time = 0;
    } else if (recv_config->wait == HDC_WAIT_TIMEOUT) {
        wait_time = (int)recv_config->timeout;
    } else {
        wait_time = -1;
    }

    session = p_session->ub_session;
    ctx = session->ctx;
    ret = hdc_recv_ub_msg_to_rx_buf(ctx, wait_time, &recv_peek_len, session, recv_config);
    if (ret == 0) {
        *len = recv_peek_len;
        recv_config->buf_count = 1;
        session->dbg_stat.rx++;
        session->dbg_stat.rx_bytes += (unsigned long long)recv_peek_len;
    } else {
        *len = 0;
        recv_config->buf_count = 0;
    }

    return ret;
}

signed int hdc_ub_recv(const struct hdc_session *p_session, char *buf, signed int len, signed int *out_len,
                       struct hdc_recv_config *recv_config)
{
    struct hdc_time_record_for_single_recv recv_record = {0};
    hdc_ub_epoll_node_t *node = NULL;
    struct hdc_ub_session *session;
    hdc_ub_context_t *ctx;
    uint64_t inc = 1;
    int service_type;
    ssize_t num;
    int ret;

    // 0 is normal recv, return one rx len to user
    if (recv_config->group_flag != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    hdc_get_time_record(&recv_record.ub_recv_start, &recv_record.fail_times);
    wmb();
    ret = drv_hdc_ub_session_para_check(p_session);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Input parameter session is invalid. (dev_id=%u; sockfd=%d)\n", p_session->device_id,
                    p_session->sockfd);
        return ret;
    }

    session = p_session->ub_session;
    ctx = session->ctx;
    service_type = session->service_type;
    node = session->epoll_event_node;
    (void)mmMutexLock(&node->mutex);
    if (node->rx_list[node->head].len > (unsigned long)len) {
        HDC_LOG_ERR("The input len is invalid. (dev_id=%u; sockfd=%d; service_type=%d; recv_buf=%d; len=%u)\n",
                    p_session->device_id, p_session->sockfd, service_type, len, node->rx_list[node->head].len);
        ret = DRV_ERROR_INVALID_VALUE;
        goto exit_recv;
    }
    if (node->rx_list[node->head].addr == 0) {
        HDC_RUN_LOG_INFO("The data in rx_list is unavailable. (dev_id=%u; sockfd=%d; service_type=%d)\n",
                         p_session->device_id, p_session->sockfd, service_type);
        ret = DRV_ERROR_REPEATED_USERD;
        goto exit_recv;
    }
    (void)memcpy_s(buf, (unsigned long)len, (void *)(uintptr_t)(node->rx_list[node->head].addr),
                   node->rx_list[node->head].len);
    wmb();
    hdc_get_time_record(&recv_record.copy_buf_to_user, &recv_record.fail_times);
    *out_len = (int)node->rx_list[node->head].len;

    ret = hdc_post_seg_to_jfr(ctx->jfr.jfr, node->rx_list[node->head].tseg, node->rx_list[node->head].addr,
                              node->rx_list[node->head].idx, session);
    if (ret != 0) {
        HDC_LOG_ERR("Failed to post seg to jfr.\n");
        goto exit_recv;
    }
    node->rx_list[node->head].addr = 0;

    wmb();
    hdc_get_time_record(&recv_record.ub_recv_end, &recv_record.fail_times);
    hdc_update_session_recv_record(&node->rx_list[node->head], &recv_record);
    hdc_get_recv_time_cost(&node->rx_list[node->head].recv_record, session);
    node->head = (node->head + 1) % HDC_RX_LIST_LEN;
    if ((session->recv_eventfd != -1) && (hdc_rx_list_is_empty(node))) {
        num = mm_read_file(session->recv_eventfd, &inc, sizeof(uint64_t));
        if (num != (ssize_t)sizeof(uint64_t)) {
            HDC_LOG_ERR("read num in recv_eventfd failed. ret=%ld\n", num);
        }
    }
exit_recv:
    (void)mmMutexUnLock(&node->mutex);

    return ret;
}

#endif // CFG_FEATURE_SUPPORT_UB
