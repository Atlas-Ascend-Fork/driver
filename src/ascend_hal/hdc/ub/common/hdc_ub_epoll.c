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
#include <errno.h>
#include <stdint.h>
#include <sys/eventfd.h>

#include "ascend_hal.h"
#include "drv_user_common.h"
#include "hdc_cmn.h"
#include "hdc_epoll.h"
#include "hdc_ub_drv.h"

#define HDC_EPOLL_SESSION_EVENT 0
#define HDC_EPOLL_SERVER_EVENT 1
struct hdc_epoll_node_info {
    signed int fd;
    unsigned int dev_id;
    unsigned int data_id;    // if session, data_id means session_id; if server, data_id means service_type
    unsigned int unique_val; // Only for session
    unsigned int op_type;
    void *data_ptr;
    struct list_head node;
};

drvError_t drv_hdc_ub_epoll_create(struct hdc_epoll_head *epoll_head, signed int size)
{
    epoll_head->epoll_events = (struct epoll_event *)malloc((size_t)size * sizeof(struct epoll_event));
    if (epoll_head->epoll_events == NULL) {
        HDC_LOG_ERR("Call malloc ep_event failed.\n");
        return DRV_ERROR_MALLOC_FAIL;
    }

    epoll_head->epfd = epoll_create(size);
    if (epoll_head->epfd < 0) {
        free(epoll_head->epoll_events);
        epoll_head->epoll_events = NULL;
        HDC_LOG_ERR("Epoll create error. (strerror=\"%s\"; errno=%d)\n", strerror(errno), errno);
        return DRV_ERROR_SOCKET_CREATE;
    }

    INIT_LIST_HEAD(&epoll_head->server_list);
    INIT_LIST_HEAD(&epoll_head->session_list);
    (void)mmMutexInit(&epoll_head->epoll_lock);
    HDC_LOG_DEBUG("Epoll create. (fd=%d, size=%u)\n", epoll_head->epfd, size);
    return DRV_ERROR_NONE;
}

drvError_t drv_hdc_ub_epoll_close(struct hdc_epoll_head *epoll_head)
{
    signed int epfd = epoll_head->epfd;
    signed int ret;

    HDC_LOG_DEBUG("Epoll close. (fd=%d)\n", epfd);
    ret = close(epfd);
    if (ret != 0) {
        HDC_LOG_ERR("Close epfd error. (epfd=%d; strerror=\"%s\"; errno=%d)\n", epfd, strerror(errno), errno);
        return DRV_ERROR_SOCKET_CLOSE;
    }
    epoll_head->epfd = -1;

    free(epoll_head->epoll_events);
    epoll_head->epoll_events = NULL;

    return DRV_ERROR_NONE;
}

STATIC int hdc_alloc_event_info(void *target, struct epoll_event *ep_event, void *data_ptr, signed int fd,
                                unsigned int flag)
{
    struct hdc_epoll_node_info *event_info = NULL;
    struct hdc_session *session_head = NULL;
    struct hdc_ub_session *session = NULL;
    struct hdc_server_head *server_head = NULL;

    event_info = malloc(sizeof(struct hdc_epoll_node_info));
    if (event_info == NULL) {
        HDC_LOG_ERR("malloc for epoll_node_info failed.(fd=%d)\n", fd);
        return ENOMEM;
    }

    if (flag == HDC_EPOLL_SESSION_EVENT) {
        session_head = (struct hdc_session *)target;
        session = session_head->ub_session;
        if (session == NULL) {
            HDC_LOG_ERR("session has been closed.\n");
            free(event_info);
            return EINVAL;
        }
        event_info->dev_id = (unsigned int)session->dev_id;
        event_info->data_id = session->local_id;
        event_info->unique_val = session->unique_val;
    } else {
        server_head = (struct hdc_server_head *)target;
        event_info->dev_id = (unsigned int)server_head->deviceId;
        event_info->data_id = (unsigned int)server_head->serviceType;
    }
    INIT_LIST_HEAD(&event_info->node);
    event_info->data_ptr = data_ptr; // Need to return to user when epoll_wait
    event_info->fd = fd;             // fd for which add in epoll
    event_info->op_type = flag;
    ep_event->data.ptr = event_info;

    return 0;
}

STATIC void hdc_free_event_info(struct hdc_epoll_node_info *event_info)
{
    event_info->unique_val = 0;
    free(event_info);
}

STATIC void hdc_find_event_info_and_free(signed int fd, struct hdc_epoll_head *epoll_head, unsigned int flag)
{
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    struct list_head *head = NULL;
    struct hdc_epoll_node_info *node_info = NULL;

    if (flag == HDC_EPOLL_SESSION_EVENT) {
        head = &epoll_head->session_list;
    } else {
        head = &epoll_head->server_list;
    }

    (void)mmMutexLock(&epoll_head->epoll_lock);
    list_for_each_safe(pos, n, head)
    {
        node_info = list_entry(pos, struct hdc_epoll_node_info, node);
        if ((node_info != NULL) && (node_info->fd == fd)) {
            drv_user_list_del(&node_info->node);
            hdc_free_event_info(node_info);
            break;
        }
    }
    (void)mmMutexUnLock(&epoll_head->epoll_lock);
    return;
}

STATIC drvError_t drv_hdc_ub_epoll_ctrl_connect_in(struct hdc_epoll_head *epoll_head, signed int ep_op, void *target,
                                                   struct epoll_event *ep_event, void *data_ptr)
{
    struct hdc_server_head *server_head = (struct hdc_server_head *)target;
    struct hdc_epoll_node_info *event_info = NULL;
    signed int epfd;
    signed int fd;
    int ret;

    if (server_head == NULL) {
        HDC_LOG_ERR("server not found.\n");
        return DRV_ERROR_INNER_ERR;
    }

    epfd = (signed int)epoll_head->epfd;
    fd = server_head->conn_wait;
    if (ep_op == EPOLL_CTL_ADD) {
        ret = hdc_alloc_event_info(target, ep_event, data_ptr, fd, HDC_EPOLL_SERVER_EVENT);
        if (ret != 0) {
            return ret;
        }
        ret = epoll_ctl(epfd, ep_op, fd, ep_event);
        if (ret != 0) {
            HDC_LOG_ERR("HDC_EPOLL_CONNECT_IN event epoll ctrl failed. (ret=%d; fd=%d)\n", ret, fd);
            hdc_free_event_info((struct hdc_epoll_node_info *)ep_event->data.ptr);
            return DRV_ERROR_FILE_OPS;
        }
        event_info = (struct hdc_epoll_node_info *)ep_event->data.ptr;
        (void)mmMutexLock(&epoll_head->epoll_lock);
        drv_user_list_add_tail(&event_info->node, &epoll_head->server_list);
        (void)mmMutexUnLock(&epoll_head->epoll_lock);
    } else {
        ret = epoll_ctl(epfd, ep_op, fd, ep_event);
        if (ret != 0) {
            HDC_LOG_ERR("HDC_EPOLL_CONNECT_IN event epoll ctrl has problem. (ret=%d)\n", ret);
            return DRV_ERROR_FILE_OPS;
        }
        hdc_find_event_info_and_free(fd, epoll_head, HDC_EPOLL_SERVER_EVENT);
    }
    HDC_LOG_DEBUG("HDC_EPOLL_CONNECT_IN event. (epfd=%d, op=%d, fd=%d)\n", epfd, ep_op, fd);
    return 0;
}

STATIC drvError_t drv_hdc_ub_epoll_ctrl_data_in(struct hdc_epoll_head *epoll_head, signed int ep_op, void *target,
                                                struct epoll_event *ep_event, void *data_ptr)
{
    struct hdc_session *session_head = (struct hdc_session *)target;
    struct hdc_epoll_node_info *event_info = NULL;
    struct hdc_ub_session *session = NULL;
    signed int epfd;
    signed int fd;
    int ret;

    if ((session_head == NULL) || (session_head->ub_session == NULL)) {
        HDC_LOG_ERR("session not found.\n");
        return DRV_ERROR_INNER_ERR;
    }

    epfd = (signed int)epoll_head->epfd;
    session = session_head->ub_session;
    fd = session->recv_eventfd;
    if (ep_op == EPOLL_CTL_ADD) {
        if (fd != -1) {
            HDC_LOG_ERR("eventfd for session data in has been added repeatedly.\n");
            return 0;
        }
        fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (fd == -1) {
            HDC_LOG_ERR("create eventfd for session data in failed.(errno=%d)\n", errno);
            return DRV_ERROR_FILE_OPS;
        }
        ret = hdc_alloc_event_info(target, ep_event, data_ptr, fd, HDC_EPOLL_SESSION_EVENT);
        if (ret != 0) {
            close(fd);
            return ret;
        }
        ret = epoll_ctl(epfd, ep_op, fd, ep_event);
        if (ret != 0) {
            HDC_LOG_ERR("HDC_EPOLL_DATA_IN event epoll ctrl failed. (ret=%d)\n", ret);
            hdc_free_event_info((struct hdc_epoll_node_info *)ep_event->data.ptr);
            close(fd);
            return DRV_ERROR_FILE_OPS;
        }
        session->recv_eventfd = fd;
        event_info = (struct hdc_epoll_node_info *)ep_event->data.ptr;
        (void)mmMutexLock(&epoll_head->epoll_lock);
        drv_user_list_add_tail(&event_info->node, &epoll_head->session_list);
        (void)mmMutexUnLock(&epoll_head->epoll_lock);
    } else {
        if (fd == -1) {
            HDC_LOG_INFO("eventfd for data in has not been added.\n");
            return DRV_ERROR_NONE;
        }
        ret = epoll_ctl(epfd, ep_op, fd, ep_event);
        if (ret != 0) {
            HDC_LOG_ERR("HDC_EPOLL_DATA_IN event epoll ctrl has problem. (ret=%d)\n", ret);
            return DRV_ERROR_FILE_OPS;
        }
        hdc_find_event_info_and_free(fd, epoll_head, HDC_EPOLL_SESSION_EVENT);
        close(fd);
        session->recv_eventfd = -1;
    }
    HDC_LOG_DEBUG("HDC_EPOLL_DATA_IN event. (epfd=%d, op=%d, fd=%d)\n", epfd, ep_op, fd);
    return 0;
}

STATIC drvError_t drv_hdc_ub_epoll_ctrl_session_close(struct hdc_epoll_head *epoll_head, signed int ep_op, void *target,
                                                      struct epoll_event *ep_event, void *data_ptr)
{
    struct hdc_session *session_head = (struct hdc_session *)target;
    struct hdc_epoll_node_info *event_info = NULL;
    signed int epfd;
    int fd;
    int ret;

    epfd = (signed int)epoll_head->epfd;
    fd = session_head->close_eventfd;
    if (ep_op == EPOLL_CTL_ADD) {
        if (fd != -1) {
            HDC_LOG_ERR("eventfd for closing session has been added repeatedly.\n");
            return 0;
        }
        fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (fd == -1) {
            HDC_LOG_ERR("create eventfd for closing session failed.(errno=%d)\n", errno);
            return DRV_ERROR_FILE_OPS;
        }
        ret = hdc_alloc_event_info(target, ep_event, data_ptr, fd, HDC_EPOLL_SESSION_EVENT);
        if (ret != 0) {
            close(fd);
            return ret;
        }
        ret = epoll_ctl(epfd, ep_op, fd, ep_event);
        if (ret != 0) {
            HDC_LOG_ERR("HDC_EPOLL_SESSION_CLOSE event epoll ctrl failed. (ret=%d)\n", ret);
            hdc_free_event_info((struct hdc_epoll_node_info *)ep_event->data.ptr);
            close(fd);
            return DRV_ERROR_FILE_OPS;
        }
        session_head->close_eventfd = fd;
        session_head->epoll_head = epoll_head;
        event_info = (struct hdc_epoll_node_info *)ep_event->data.ptr;
        (void)mmMutexLock(&epoll_head->epoll_lock);
        drv_user_list_add_tail(&event_info->node, &epoll_head->session_list);
        (void)mmMutexUnLock(&epoll_head->epoll_lock);
    } else {
        if (fd == -1) {
            HDC_LOG_INFO("eventfd for closing session has not been added.\n");
            return DRV_ERROR_NONE;
        }
        ret = epoll_ctl(epfd, ep_op, fd, ep_event);
        if (ret != 0) {
            HDC_LOG_WARN("HDC_EPOLL_SESSION_CLOSE event epoll ctrl has problem. (ret=%d)\n", ret);
            return DRV_ERROR_FILE_OPS;
        }
        hdc_find_event_info_and_free(fd, epoll_head, HDC_EPOLL_SESSION_EVENT);
        close(fd);
        session_head->close_eventfd = -1;
        session_head->epoll_head = NULL;
    }
    HDC_LOG_DEBUG("HDC_EPOLL_SESSION_CLOSE event. (epfd=%d, op=%d, fd=%d)\n", epfd, ep_op, fd);
    return 0;
}

drvError_t drv_hdc_ub_epoll_ctl(struct hdc_epoll_head *epoll_head, signed int op, void *target,
                                const struct drvHdcEvent *event)
{
    struct epoll_event ep_event;
    signed int epfd;
    signed int ep_op;
    int ret;

    epfd = (signed int)epoll_head->epfd;
    ep_op = (op == HDC_EPOLL_CTL_ADD) ? EPOLL_CTL_ADD : EPOLL_CTL_DEL;
    ep_event.events = EPOLLIN;

    if (event->events & HDC_EPOLL_CONN_IN) {
        ret = drv_hdc_ub_epoll_ctrl_connect_in(epoll_head, ep_op, target, &ep_event, (void *)event->data);
        if (ret != 0) {
            HDC_LOG_ERR("HDC_EPOLL_CONNECT_IN event failed. (epfd=%d, op=%d)\n", epfd, ep_op);
            return ret;
        }
    }

    /* event of HDC_EPOLL_DATA_IN and HDC_EPOLL_SESSION_CLOSE may be set by one time */
    if (event->events & HDC_EPOLL_DATA_IN) {
        ret = drv_hdc_ub_epoll_ctrl_data_in(epoll_head, ep_op, target, &ep_event, (void *)event->data);
        if (ret != 0) {
            HDC_LOG_ERR("HDC_EPOLL_DATA_IN event failed. (epfd=%d, op=%d)\n", epfd, ep_op);
            return ret;
        }
    }
    if (event->events & HDC_EPOLL_SESSION_CLOSE) {
        ret = drv_hdc_ub_epoll_ctrl_session_close(epoll_head, ep_op, target, &ep_event, (void *)event->data);
        if (ret != 0) {
            HDC_LOG_ERR("HDC_EPOLL_SESSION_CLOSE event failed. (epfd=%d, op=%d)\n", epfd, ep_op);
            return ret;
        }
    }

    return 0;
}

drvError_t drv_hdc_ub_epoll_wait(const struct hdc_epoll_head *epoll_head, struct drvHdcEvent *events,
                                 signed int maxevents, signed int timeout, signed int *eventnum)
{
    signed int i, idx;
    signed int epfd = (signed int)epoll_head->epfd;
    struct hdc_epoll_node_info *node_info = NULL;

    do {
        *eventnum = epoll_wait(epfd, epoll_head->epoll_events, maxevents, timeout);
    } while ((*eventnum == -1) && (mm_get_error_code() == EINTR));

    if ((*eventnum < 0) || (*eventnum > HDCDRV_EPOLL_FD_EVENT_NUM)) {
        HDC_LOG_ERR("Call epoll_wait error. (epfd=%d; strerror=\"%s\"; errno=%d)\n", epfd, strerror(errno), errno);
        return DRV_ERROR_IOCRL_FAIL;
    }

    for (i = 0; i < *eventnum; i++) {
        /* change EPOLLIN to HDC epoll event */
        node_info = (struct hdc_epoll_node_info *)epoll_head->epoll_events[i].data.ptr;
        if (node_info->op_type == HDC_EPOLL_SERVER_EVENT) {
            events[i].events = HDC_EPOLL_CONN_IN;
        } else {
            idx = hdc_get_lock_index((int)node_info->dev_id, (int)node_info->data_id);
            if (idx >= (HDC_MAX_UB_DEV_CNT * HDCDRV_UB_SINGLE_DEV_MAX_SESSION)) {
                continue;
            }
            mmMutexLock(&g_hdcConfig.session_lock[idx]);
            if (hdc_session_alive_check((int)node_info->dev_id, (int)node_info->data_id, node_info->unique_val) != 0) {
                events[i].events = HDC_EPOLL_SESSION_CLOSE;
            } else {
                events[i].events = HDC_EPOLL_DATA_IN;
            }
            mmMutexUnLock(&g_hdcConfig.session_lock[idx]);
        }
        events[i].data = (uintptr_t)(node_info->data_ptr);
        HDC_LOG_DEBUG("Epoll wait success. (index=%d, fd=%d, events=0x%x)\n", i, epfd, events[i].events);
    }

    return DRV_ERROR_NONE;
}

struct hdc_epoll_ops *drv_get_hdc_ub_epoll_ops(void)
{
    static struct hdc_epoll_ops ub_epoll_ops = {
        drv_hdc_ub_epoll_create,
        drv_hdc_ub_epoll_ctl,
        drv_hdc_ub_epoll_wait,
        drv_hdc_ub_epoll_close,
    };

    return &ub_epoll_ops;
}

HDC_EPOLL g_thread_epoll; // To solve wait_jfc can not quit when delete_jfc
mmMutex_t g_thread_epoll_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t g_epoll_thread;
bool g_epoll_thread_create = false;
int g_epoll_ref = 0;
// dfx: abnormal count of hdc_recv_data_in_event_handle in epoll thread, single writer so no lock needed
STATIC unsigned long long g_epoll_recv_abnormal_cnt = 0;

bool hdc_ub_is_epoll_thread_created(void)
{
    bool created;

    (void)mmMutexLock(&g_thread_epoll_lock);
    created = g_epoll_thread_create;
    (void)mmMutexUnLock(&g_thread_epoll_lock);
    return created;
}

signed int hdc_ub_get_epoll_thread_epfd(void)
{
    struct hdc_epoll_head *epoll_head;
    signed int epfd = -1;

    // lock protects the lifetime of g_thread_epoll, not the value itself
    (void)mmMutexLock(&g_thread_epoll_lock);
    epoll_head = (struct hdc_epoll_head *)g_thread_epoll;
    if (epoll_head != NULL) {
        epfd = (signed int)epoll_head->epfd;
    }
    (void)mmMutexUnLock(&g_thread_epoll_lock);
    return epfd;
}

unsigned long long hdc_ub_get_epoll_recv_abnormal_cnt(void)
{
    return g_epoll_recv_abnormal_cnt;
}

STATIC void *hdc_ub_epoll_thread_handle(void *arg)
{
    struct hdc_epoll_head *epoll_head = (struct hdc_epoll_head *)g_thread_epoll;
    signed int epfd = (signed int)epoll_head->epfd;
    int eventnum;

    (void)arg;
    (void)mmMutexLock(&g_thread_epoll_lock);
    while (g_epoll_ref != 0) {
        (void)mmMutexUnLock(&g_thread_epoll_lock);
        do {
            eventnum = epoll_wait(epfd, epoll_head->epoll_events, 1, 10000); // 1 means 1 event, 10000 means 10s
        } while ((eventnum == -1) && (mm_get_error_code() == EINTR));

        if ((eventnum < 0) || (eventnum > HDCDRV_EPOLL_FD_EVENT_NUM)) {
            HDC_LOG_ERR("Call epoll_wait error. (epfd=%d; strerror=\"%s\"; errno=%d)\n", epfd, strerror(errno), errno);
        } else if (eventnum != 0) {
            if (hdc_recv_data_in_event_handle((struct hdc_ub_epoll_node *)epoll_head->epoll_events[0].data.ptr) != 0) {
                g_epoll_recv_abnormal_cnt++;
            }
        }
        (void)mmMutexLock(&g_thread_epoll_lock);
    }

    drvHdcEpollClose(g_thread_epoll);
    g_epoll_thread_create = false;
    g_thread_epoll = NULL;
    (void)mmMutexUnLock(&g_thread_epoll_lock);

    return NULL;
}

int hdc_ub_epoll_thread_init(void)
{
    int ret;
    pthread_attr_t attr;

    (void)mmMutexLock(&g_thread_epoll_lock);
    if (g_epoll_thread_create) {
        g_epoll_ref++;
        (void)mmMutexUnLock(&g_thread_epoll_lock);
        return 0;
    }
    ret = drvHdcEpollCreate(1024, &g_thread_epoll); // 1024 means max fd num in process
    if (ret != DRV_ERROR_NONE) {
        (void)mmMutexUnLock(&g_thread_epoll_lock);
        HDC_LOG_ERR("Call drvHdcEpollCreate failed. (ret=%d)\n", ret);
        return ret;
    }

    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    g_epoll_ref++;
    ret = pthread_create(&g_epoll_thread, &attr, hdc_ub_epoll_thread_handle, NULL);
    if (ret != 0) {
        HDC_LOG_ERR("pthread_create g_epoll_thread not success, can not connect.\n");
        ret = DRV_ERROR_INNER_ERR;
        goto del_close_event;
    }
    g_epoll_thread_create = true;
    (void)pthread_attr_destroy(&attr);
    (void)mmMutexUnLock(&g_thread_epoll_lock);

    return 0;

del_close_event:
    g_epoll_ref--;
    (void)pthread_attr_destroy(&attr);
    (void)drvHdcEpollClose(g_thread_epoll);
    g_thread_epoll = NULL;
    (void)mmMutexUnLock(&g_thread_epoll_lock);

    return ret;
}

void hdc_ub_epoll_thread_uninit(void)
{
    (void)mmMutexLock(&g_thread_epoll_lock);
    g_epoll_ref--;
    (void)mmMutexUnLock(&g_thread_epoll_lock);

    return;
}

int hdc_ub_add_ctl_to_thread_epoll(struct hdc_ub_session *session)
{
    struct hdc_epoll_head *epoll_head;
    signed int recv_listen_fd;
    int ret;
    struct epoll_event ep_event;

    epoll_head = g_thread_epoll;
    // recv need jfce_id to wakeup
    recv_listen_fd = session->ub_res_info.l_jfce_r_fd;

    // Both data_in and session_close listen session
    ep_event.data.ptr = (void *)session->epoll_event_node;
    ep_event.events = EPOLLIN;

    // listen data_in
    ret = epoll_ctl((signed int)epoll_head->epfd, EPOLL_CTL_ADD, recv_listen_fd, &ep_event);
    if (ret != 0) {
        HDC_LOG_ERR("Call epoll_ctl for data_in failed. (ret=%d; dev_id=%d; l_id=%u; service_type=\"%s\")\n", ret,
                    session->dev_id, session->local_id, hdc_get_sevice_str(session->service_type));
        return ret;
    }

    return 0;
}

void hdc_ub_del_ctl_to_thread_epoll(struct hdc_ub_session *session)
{
    struct epoll_event ep_event;
    struct hdc_epoll_head *epoll_head;
    signed int recv_listen_fd;

    epoll_head = g_thread_epoll;
    recv_listen_fd = session->ub_res_info.l_jfce_r_fd;
    ep_event.data.ptr = (void *)session->epoll_event_node;

    (void)epoll_ctl((signed int)epoll_head->epfd, EPOLL_CTL_DEL, (int)recv_listen_fd, &ep_event);
    return;
}