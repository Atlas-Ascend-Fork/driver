/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <unistd.h>
#include <sys/epoll.h>
#include <stdint.h>
#include <sys/eventfd.h>

#include "ascend_hal.h"
#include "hdc_cmn.h"
#include "hdc_epoll.h"
#include "hdc_ub_drv.h"
#include "hdc_core.h"

struct hdc_process_thread_info {
    mmMutex_t lock;
    pthread_t thread;
    bool exist_flag;
    int ref;
};

struct hdc_process_thread_info g_hdc_thread_info[HDC_MAX_UB_DEV_CNT] = {
    [0 ... HDC_MAX_UB_DEV_CNT - 1] = {.lock = PTHREAD_MUTEX_INITIALIZER, .ref = 0, .exist_flag = false}};

// process hdc remote connect request, equal to crtl_msg_connect in pcie
drvError_t hdc_connect_event_proc(uint32_t dev_id, void *msg, uint32_t msg_len)
{
    struct hdcdrv_event_msg *conn_msg = (struct hdcdrv_event_msg *)msg;
    struct hdc_server_head *p_server_head = NULL;
    int serviceType;
    mmSsize_t num;
    int ts_fail = 0;
    struct hdc_pipe_event_msg pipe_msg = {0};

    (void)msg_len;
    serviceType = conn_msg->connect_msg.service_type;

    HDC_LOG_DEBUG("event type=%d; peer_id=%llu; service_type=\"%s\"; client_pid=%llu; euid=%d; uid=%u\n",
                  conn_msg->type, conn_msg->peer_pid, hdc_get_sevice_str(conn_msg->connect_msg.service_type),
                  conn_msg->connect_msg.client_pid, conn_msg->connect_msg.euid, conn_msg->connect_msg.uid);

    if (!hdc_service_type_vaild(serviceType)) {
        HDC_LOG_ERR("service type is invalid.(service type=%d)\n", serviceType);
        return DRV_ERROR_INVALID_VALUE;
    }

    p_server_head = (struct hdc_server_head *)g_hdcConfig.server_list[dev_id][serviceType];
    if (p_server_head == NULL) {
        HDC_LOG_WARN("server was not found, maybe the server has been destroyed.(service type=%d)\n", serviceType);
        return 0;
    }

    /* Send a receiving notification to the accept thread through the pipe. */
    if (p_server_head->conn_notify == -1) {
        HDC_LOG_WARN("maybe the server has been destroyed.(dev_id=%d; service_type=\"%s\")\n", dev_id,
                     hdc_get_sevice_str(serviceType));
        return HDCDRV_SESSION_HAS_CLOSED;
    }
    pipe_msg.event_msg = *conn_msg;
    hdc_get_time_record(&pipe_msg.pipe_write_start, &ts_fail);

    num = mm_write_file(p_server_head->conn_notify, &pipe_msg, sizeof(pipe_msg));
    if (num != (mmSsize_t)sizeof(pipe_msg)) {
        HDC_LOG_ERR("send connection event message data failed.(dev_id=%d; service_type=\"%s\")\n", dev_id,
                    hdc_get_sevice_str(serviceType));
        return HDCDRV_SEND_CTRL_MSG_FAIL;
    }

    HDC_LOG_DEBUG("received a connection. (device id=%u, serviceType=%d)\n", dev_id, serviceType);
    hdc_touch_connect_notify((int)dev_id, conn_msg->connect_msg.client_pid, serviceType);

    return 0;
}

// process hdc remote dfx query request
drvError_t hdc_dfx_query_event_proc(uint32_t dev_id, void *msg, uint32_t msg_len)
{
    struct hdcdrv_event_msg *dfx_msg = NULL;
    int idx, ret;
    struct hdc_ub_session *session = NULL;

    (void)msg_len;
    dfx_msg = (struct hdcdrv_event_msg *)msg;

    if ((dfx_msg->dfx_msg.r_session_id >= HDCDRV_UB_SINGLE_DEV_MAX_SESSION) ||
        (dev_id >= (unsigned int)hdc_get_max_device_num())) {
        HDC_LOG_ERR("session para invalid.(id=%d)\n", dfx_msg->dfx_msg.r_session_id);
        return DRV_ERROR_NOT_SUPPORT;
    }

    idx = hdc_get_lock_index((int)dev_id, (int)dfx_msg->dfx_msg.r_session_id);
    (void)mmMutexLock(&g_hdcConfig.session_lock[idx]);

    session = hdc_find_session_in_list(dfx_msg->dfx_msg.r_session_id, (int)dev_id,
                                       dfx_msg->dfx_msg.para_info.unique_val, idx);
    if ((session == NULL) || (session->status == HDC_SESSION_STATUS_IDLE)) {
        HDC_LOG_WARN("Can not find session, session may close.(local_session=%u, remote_session=%u, unique_val=%u)\n",
                     dfx_msg->dfx_msg.r_session_id, dfx_msg->dfx_msg.l_session_id,
                     dfx_msg->dfx_msg.para_info.unique_val);
        (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        return -HDCDRV_SESSION_HAS_CLOSED;
    }

    ret = hdc_dfx_query_handle((int)dev_id, session, &dfx_msg->dfx_msg);

    (void)mmMutexUnLock(&g_hdcConfig.session_lock[idx]);

    return ret;
}

// Only close use common thread to process close_session message, OTHER event no need to register
static drvError_t (*g_hdc_event_proc[HDCDRV_NOTIFY_MSG_MAX])(uint32_t dev_id, void *msg, uint32_t msg_len) = {
    [HDCDRV_NOTIFY_MSG_CONNECT] = hdc_connect_event_proc,
    // process hdc remote close request, equal to crtl_msg_close in pcie
    [HDCDRV_NOTIFY_MSG_CLOSE] = hdc_remote_close_proc,
    [HDCDRV_NOTIFY_MSG_DFX] = hdc_dfx_query_event_proc,
};

struct hdc_process_para {
    mmProcess fd;
    unsigned int dev_id;
};

STATIC bool hdc_check_event_msg(uint32_t dev_id, void *msg, unsigned int msg_len)
{
    struct hdcdrv_event_msg *msg_para = (struct hdcdrv_event_msg *)msg;

    if ((msg == NULL) || (msg_len < sizeof(struct hdcdrv_event_msg))) {
        HDC_LOG_ERR("Invalid msg len. (len=%u; dev_id=%u)\n", msg_len, dev_id);
        return false;
    }

    if (dev_id >= (unsigned int)hdc_get_max_device_num()) {
        HDC_LOG_ERR("Invalid dev id. (dev_id=%u)\n", dev_id);
        return false;
    }
    if ((msg_para->type >= HDCDRV_NOTIFY_MSG_MAX) || (msg_para->type < 0)) {
        HDC_LOG_ERR("Invalid msg id. (msg_id=%u; dev_id=%u)\n", msg_para->type, dev_id);
        return false;
    }

    return true;
}

// If no need to exit, update retry_cnt
STATIC bool hdc_process_need_to_exit(unsigned int dev_id, int *retry_cnt)
{
    if (g_hdc_thread_info[dev_id].ref != 0) {
        *retry_cnt = 0;
        return false;
    }

    if ((g_hdc_thread_info[dev_id].ref == 0) && (*retry_cnt < HDC_RETRY_MAX_TIME)) {
        *retry_cnt = *retry_cnt + 1;
        return false;
    }

    return true;
}

STATIC void *hdc_process_thread_handle(void *arg)
{
    union hdcdrv_cmd hdc_cmd = {0};
    struct hdcdrv_cmd_wait_ctrl_msg *wait_msg = &hdc_cmd.wait_ctrl_msg;
    struct hdcdrv_event_msg event_msg = {0};
    struct hdc_process_para *para = (struct hdc_process_para *)arg;
    unsigned int dev_id = para->dev_id;
    mmProcess fd = para->fd;
    int ret, retry_cnt = 0;

    free(para);

    (void)mmMutexLock(&g_hdc_thread_info[dev_id].lock);
    while (!hdc_process_need_to_exit(dev_id, &retry_cnt)) {
        (void)mmMutexUnLock(&g_hdc_thread_info[dev_id].lock);
        wait_msg->msg_buf = &event_msg;
        wait_msg->dev_id = (int)dev_id;
        wait_msg->msg_len = sizeof(struct hdcdrv_event_msg);
        ret = hdc_ub_ioctl(fd, HDCDRV_WAIT_CTRL_MSG, &hdc_cmd);
        if ((ret == 0) && hdc_check_event_msg(dev_id, wait_msg->msg_buf, wait_msg->msg_len)) {
            if (g_hdc_event_proc[event_msg.type] != NULL) {
                ret = g_hdc_event_proc[event_msg.type](dev_id, wait_msg->msg_buf, wait_msg->msg_len);
                if (ret != 0) {
                    HDC_LOG_WARN("g_hdc_event_proc execute not successfully.(dev_id=%u; type=%d)\n", dev_id,
                                 event_msg.type);
                }
            }
        } else if ((ret == DRV_ERROR_NO_PROCESS) || (ret == DRV_ERROR_DEVICE_NOT_READY) ||
                   (ret == DRV_ERROR_NO_DEVICE)) {
            (void)mmMutexLock(&g_hdc_thread_info[dev_id].lock);
            break;
        } else {
            HDC_LOG_ERR("Process wait event_msg but get invalid msg or invalid ret.(ret=%d; msg_len=%u; dev_id=%u)\n",
                        ret, wait_msg->msg_len, dev_id);
        }
        (void)memset_s(&event_msg, sizeof(struct hdcdrv_event_msg), 0, sizeof(struct hdcdrv_event_msg));
        (void)mmMutexLock(&g_hdc_thread_info[dev_id].lock);
    }

    // No session/server will use thread, close process ctrl node
    hdc_cmd.uninit_ctrl_node.dev_id = (int)dev_id;
    (void)hdc_ub_ioctl(fd, HDCDRV_UNINIT_CTRL_NODE, &hdc_cmd);
    g_hdc_thread_info[dev_id].ref--;
    g_hdc_thread_info[dev_id].exist_flag = false;
    hdc_ub_close(fd);
    (void)mmMutexUnLock(&g_hdc_thread_info[dev_id].lock);

    return NULL;
}

int hdc_process_thread_init(unsigned int dev_id, const char *func_name)
{
    int ret;
    pthread_attr_t attr;
    struct hdc_process_para *para;
    union hdcdrv_cmd init_ctrl_node = {0};

    (void)mmMutexLock(&g_hdc_thread_info[dev_id].lock);
    if (g_hdc_thread_info[dev_id].exist_flag) {
        g_hdc_thread_info[dev_id].ref++;
        (void)mmMutexUnLock(&g_hdc_thread_info[dev_id].lock);
        return 0;
    }

    para = (struct hdc_process_para *)malloc(sizeof(struct hdc_process_para));
    if (para == NULL) {
        ret = DRV_ERROR_OUT_OF_MEMORY;
        hdc_report_out_of_mem_err_msg(func_name, sizeof(struct hdc_process_para));
        goto process_init_exit;
    }

    para->dev_id = dev_id;
    para->fd = hdc_ub_open();
    if (para->fd == (mmProcess)EN_ERROR) {
        ret = DRV_ERROR_OPEN_FAILED;
        goto free_process_mem;
    }

    init_ctrl_node.init_ctrl_node.dev_id = (int)dev_id;
    ret = hdc_ub_ioctl(para->fd, HDCDRV_INIT_CTRL_NODE, &init_ctrl_node);
    if (ret != 0) {
        HDC_LOG_ERR("ctrl_node_init failed in kernel.(ret=%d; dev_id=%u)\n", ret, dev_id);
        goto close_para_fd;
    }

    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&g_hdc_thread_info[dev_id].thread, &attr, hdc_process_thread_handle, (void *)para);
    if (ret != 0) {
        HDC_LOG_ERR("pthread_create not success(dev_id=%u; ret=%d).\n", dev_id, ret);
        ret = DRV_ERROR_INNER_ERR;
        goto close_fd_for_process;
    }
    g_hdc_thread_info[dev_id].ref++;
    g_hdc_thread_info[dev_id].exist_flag = true;
    (void)pthread_attr_destroy(&attr);
    (void)mmMutexUnLock(&g_hdc_thread_info[dev_id].lock);

    return 0;

close_fd_for_process:
    (void)pthread_attr_destroy(&attr);
close_para_fd:
    hdc_ub_close(para->fd);
free_process_mem:
    free(para);
process_init_exit:
    (void)mmMutexUnLock(&g_hdc_thread_info[dev_id].lock);
    return ret;
}

// uninit will not clear process thread
void hdc_process_thread_uninit(unsigned int dev_id)
{
    (void)mmMutexLock(&g_hdc_thread_info[dev_id].lock);
    g_hdc_thread_info[dev_id].ref--;
    (void)mmMutexUnLock(&g_hdc_thread_info[dev_id].lock);

    return;
}