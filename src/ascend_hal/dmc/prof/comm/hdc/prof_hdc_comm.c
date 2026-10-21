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
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include "prof_h2d_kernel_msg.h"
#include "prof_common.h"
#include "prof_hdc_comm.h"

#define PROF_NSEC_PER_MSECOND 1000000

STATIC struct prof_hdc_common_info g_prof_hdc_info = {
    .prof_hdc_mutex = PTHREAD_MUTEX_INITIALIZER,
    .prof_epoll_client_recv_flag = PROF_HDC_RECEIVE_THREAD_DISABLE,
    .recv_thread_run_flag = PROF_HDC_RECEIVE_THREAD_STOP,
};

void (*g_hdc_msg_proc_fuc)(uint32_t dev_id, void *msg, uint32_t len) = NULL;

void prof_hdc_register_msg_proc_func(void (*func)(uint32_t dev_id, void *msg, uint32_t len))
{
    g_hdc_msg_proc_fuc = func;
}

STATIC void prof_hdc_call_msg_proc_func(uint32_t dev_id, void *msg, uint32_t len)
{
    if (g_hdc_msg_proc_fuc != NULL) {
        g_hdc_msg_proc_fuc(dev_id, msg, len);
    }
}

STATIC void prof_hdc_session_close(struct drvHdcEvent event)
{
    drvError_t ret;
    int i, j;

    (void)pthread_mutex_lock(&(g_prof_hdc_info.prof_hdc_mutex));

    ret = (int)drvHdcEpollCtl(g_prof_hdc_info.epoll, HDC_EPOLL_CTL_DEL, (void *)event.data, &event);
    PROF_RUN_INFO("Profile HDC poll deleted the session. (ret=%d)\r\n", ret);

    for (i = 0; i < DEV_NUM; i++) {
        if (g_prof_hdc_info.session[i] == (HDC_SESSION)event.data) {
            g_prof_hdc_info.session_count--;
            for (j = 0; j < PROF_CHANNEL_NUM_MAX; j++) {
                g_prof_hdc_info.prof_channel_enable_flag[i][j] = 0;
            }
            break;
        }
    }

    (void)pthread_mutex_unlock(&(g_prof_hdc_info.prof_hdc_mutex));
}

STATIC void prof_wait_epoll_client_recv_thread_exit(void)
{
    int cnt = 0;
    long timecost;
    struct timespec start = {0};
    struct timespec end = {0};
    /* wait recv thread exit */
    (void)clock_gettime(CLOCK_MONOTONIC, &start);
    while ((g_prof_hdc_info.recv_thread_run_flag != PROF_HDC_RECEIVE_THREAD_STOP) &&
           (cnt < 5000)) {  /* wait times max 5000 */
        (void)usleep(1000), /* 1000 us */
            cnt++;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &end);
    /* 1000: ms */
    timecost = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / PROF_NSEC_PER_MSECOND;
    /* > 10 ms print warn */
    if (timecost > 10) {
        PROF_WARN("Profile waited for the ending of thread at the poll. (timecost=%ldms)\n", timecost);
    }
    if (g_prof_hdc_info.recv_thread_run_flag == PROF_HDC_RECEIVE_THREAD_RUNNING) {
        PROF_WARN("Profile waited for the ending of thread at the poll. The thread was not over. (cnt=%d)\n", cnt);
    }
}

STATIC void prof_handle_hdc_events(struct drvHdcEvent *events, int eventnum)
{
    struct drvHdcMsg *hdc_msg = NULL;
    HDC_SESSION recv_session;
    struct drvHdcEvent event;
    char *p_buf = NULL;
    int dev_id = 0;
    int recv_buf_count;
    int buf_len = 0;
    drvError_t ret;
    int i;

    for (i = 0; i < eventnum; i++) {
        recv_session = (HDC_SESSION)events[i].data;

        if ((events[i].events & HDC_EPOLL_SESSION_CLOSE) != 0) {
            event.events = HDC_EPOLL_DATA_IN;
            event.data = (uintptr_t)recv_session;
            prof_hdc_session_close(event);
            continue;
        }

        ret = drvHdcAllocMsg(recv_session, &hdc_msg, 1);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("Failed to invoke function [drvHdcAllocMsg]. (ret=%d)\n", (int)ret);
            continue;
        }

        ret = halHdcRecv(recv_session, hdc_msg, 0, 1, &recv_buf_count, 0);
        if (ret != DRV_ERROR_NONE) {
            if (ret == DRV_ERROR_NON_BLOCK) {
                PROF_DEBUG("No report.\n");
            } else {
                PROF_ERR("Failed to invoke function [halHdcRecv]. (ret=%d)\n", (int)ret);
            }
            goto hdc_msg_free;
        }

        ret = drvHdcGetMsgBuffer(hdc_msg, 0, &p_buf, &buf_len);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("Failed to invoke function [drvHdcGetMsgBuffer]. (ret=%d)\n", (int)ret);
            goto hdc_msg_free;
        }
        ret = halHdcGetSessionAttr(recv_session, HDC_SESSION_ATTR_DEV_ID, &dev_id);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("Failed to invoke function [halHdcGetSessionAttr] to get the device ID. (ret=%d)\n", (int)ret);
            goto hdc_msg_free;
        }

        prof_hdc_call_msg_proc_func((uint32_t)dev_id, (unsigned char *)p_buf, (uint32_t)buf_len);

    hdc_msg_free:
        ret = drvHdcFreeMsg(hdc_msg);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("Failed to invoke function [drvHdcFreeMsg]. (ret=%d)\n", (int)ret);
        }
    }
}

STATIC void prof_recv_all_session_msg(void)
{
    struct drvHdcEvent events = {0};
    int i;

    for (i = 0; i < DEV_NUM; i++) {
        if (g_prof_hdc_info.prof_channel_num_count[i] > 0) {
            PROF_DEBUG("try recv per session report. (devid=%d)\n", i);
            events.data = (uintptr_t)g_prof_hdc_info.session[i];
            prof_handle_hdc_events(&events, 1);
        }
    }
}

STATIC drvError_t prof_client_create(void)
{
    drvError_t ret;

    ret = drvHdcEpollCreate(PROF_HDC_EVENT_NUM_MAX, &g_prof_hdc_info.epoll);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to invoke drvHdcEpollCreate. (ret=%d)\n", (int)ret);
        return ret;
    }

    ret = drvHdcClientCreate(&g_prof_hdc_info.client, PROF_HDC_EVENT_NUM_MAX, HDC_SERVICE_TYPE_PROF, 0);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to invoke drvHdcClientCreate. (ret=%d)\n", (int)ret);
        goto prof_client_create_err;
    }

    return ret;

prof_client_create_err:
    ret = drvHdcEpollClose(g_prof_hdc_info.epoll);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to close the poll of HDC driver. (ret=%d)\n", (int)ret);
    }
    return ret;
}

STATIC void prof_client_destroy(void)
{
    drvError_t ret;

    ret = drvHdcEpollClose(g_prof_hdc_info.epoll);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to close the poll of HDC driver. (ret=%d)\n", (int)ret);
    }

    ret = drvHdcClientDestroy(g_prof_hdc_info.client);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to destroy HDC client. (ret=%d)\n", (int)ret);
    }

    /* debug log , need to be del before merge*/
    PROF_INFO("HDC client destroyed.\n");
}

#define PROF_EPOLL_SLEEP_TIME 100000
#define PROF_CLIENT_EPOLL_TIMEOUT_MS 1000
STATIC struct drvHdcEvent g_prof_epoll_events[PROF_HDC_EVENT_NUM_MAX];
STATIC void *prof_epoll_client_recv(void *arg)
{
    HDC_EPOLL epoll = (HDC_EPOLL)arg;
    int eventnum;
    drvError_t ret;
    bool wait_epoll_thread_end = false;

    g_prof_hdc_info.recv_thread_run_flag = PROF_HDC_RECEIVE_THREAD_RUNNING;
    while (g_prof_hdc_info.prof_epoll_client_recv_flag == PROF_HDC_RECEIVE_THREAD_ENABLE) {
        if (wait_epoll_thread_end) {
            // if drvHdcEpollWait returned fail, should wait epoll thread end, and then release hdc epoll and client
            // memory
            (void)usleep(PROF_EPOLL_SLEEP_TIME); // 100ms
            continue;
        }

        eventnum = PROF_HDC_EVENT_NUM_MAX;
        ret = drvHdcEpollWait(epoll, &g_prof_epoll_events[0], PROF_HDC_EVENT_NUM_MAX, PROF_CLIENT_EPOLL_TIMEOUT_MS,
                              &eventnum); /* timeout 1000ms */
        if ((ret != DRV_ERROR_NONE) || (eventnum < 0) || ((uint32_t)eventnum > PROF_HDC_EVENT_NUM_MAX)) {
            if (ret == DRV_ERROR_EPOLL_CLOSE) {
                PROF_INFO("Profile HDC was closed. The client receiving thread at the poll was over."
                          " (ret=%d)\n",
                          ret);
            } else {
                PROF_ERR("Failed to invoke function [drvHdcEpollWait] or the variable [eventnum] was invalid."
                         " (ret=%d, eventnum=%d)\n",
                         ret, eventnum);
            }
            wait_epoll_thread_end = true;
            continue;
        }

        if (eventnum == 0) {
            PROF_DEBUG("Wait report timeout, try recv all session msg.\n");
            prof_recv_all_session_msg();
            continue;
        }

        prof_handle_hdc_events(&g_prof_epoll_events[0], eventnum);
    }

    prof_client_destroy();

    g_prof_hdc_info.recv_thread_run_flag = PROF_HDC_RECEIVE_THREAD_STOP;

    PROF_INFO("Epoll recv thread exit.\n");
    return NULL;
}

STATIC drvError_t prof_create_receive_thread(void)
{
    pthread_attr_t attr;
    drvError_t ret;

    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    (void)pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    ret = pthread_create(&g_prof_hdc_info.epoll_thread, &attr, prof_epoll_client_recv, (void *)g_prof_hdc_info.epoll);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to create the thread. (ret=%d)\n", ret);
    }
    (void)pthread_attr_destroy(&attr);

    return ret;
}

STATIC void prof_destroy_receive_thread(void)
{
    g_prof_hdc_info.prof_epoll_client_recv_flag = PROF_HDC_RECEIVE_THREAD_DISABLE;
}

STATIC drvError_t prof_client_and_receive_thread_create(void)
{
    drvError_t ret;

    if (g_prof_hdc_info.prof_epoll_client_recv_flag == PROF_HDC_RECEIVE_THREAD_ENABLE) {
        return DRV_ERROR_NONE;
    }

    if (g_prof_hdc_info.recv_thread_run_flag == PROF_HDC_RECEIVE_THREAD_RUNNING) {
        prof_wait_epoll_client_recv_thread_exit();
    }

    ret = prof_client_create();
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to create profile client. (ret=%d)\n", (int)ret);
        return ret;
    }

    g_prof_hdc_info.session_count = 0;
    g_prof_hdc_info.prof_epoll_client_recv_flag = PROF_HDC_RECEIVE_THREAD_ENABLE;

    ret = prof_create_receive_thread();
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to create receive thread. (ret=%d)\n", (int)ret);
        g_prof_hdc_info.prof_epoll_client_recv_flag = PROF_HDC_RECEIVE_THREAD_DISABLE;
        prof_client_destroy();
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_per_session_connect(uint32_t dev_id, uint32_t chan_id)
{
    struct drvHdcEvent event;
    drvError_t ret;

    if (g_prof_hdc_info.prof_channel_enable_flag[dev_id][chan_id] == 1) {
        return DRV_ERROR_NONE;
    }

    if (g_prof_hdc_info.prof_channel_num_count[dev_id] != 0) {
        g_prof_hdc_info.prof_channel_num_count[dev_id]++;
        g_prof_hdc_info.prof_channel_enable_flag[dev_id][chan_id] = 1;
        return DRV_ERROR_NONE;
    }

    ret = drvHdcSessionConnect(0, (int)dev_id, g_prof_hdc_info.client, &g_prof_hdc_info.session[dev_id]);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to invoke function [drvHdcSessionConnect]. (ret=%d)\n", (int)ret);
        return ret;
    }

    PROF_INFO("The function [drvHdcSessionConnect] invoked was successful. (dev_id=%u)\n", dev_id);

    ret = drvHdcSetSessionReference(g_prof_hdc_info.session[dev_id]);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to invoke function [drvHdcSetSessionReference] to add the session. (ret=%d)\n", (int)ret);
        (void)drvHdcSessionClose(g_prof_hdc_info.session[dev_id]);
        return ret;
    }

    event.events = HDC_EPOLL_DATA_IN;
    event.data = (uintptr_t)g_prof_hdc_info.session[dev_id];
    ret = drvHdcEpollCtl(g_prof_hdc_info.epoll, HDC_EPOLL_CTL_ADD, (void *)g_prof_hdc_info.session[dev_id], &event);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to add the session. (ret=%d)\n", (int)ret);
        (void)drvHdcSessionClose(g_prof_hdc_info.session[dev_id]);
        return ret;
    }

    g_prof_hdc_info.session_count++;
    g_prof_hdc_info.prof_channel_num_count[dev_id]++;
    g_prof_hdc_info.prof_channel_enable_flag[dev_id][chan_id] = 1;

    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_session_connect(uint32_t dev_id, uint32_t chan_id)
{
    drvError_t ret;

    ret = prof_client_and_receive_thread_create();
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to create the thread for client to connect with profile sessions. (ret=%d)\n", ret);
        return ret;
    }

    ret = prof_per_session_connect(dev_id, chan_id);
    if (ret != DRV_ERROR_NONE) {
        prof_destroy_receive_thread();
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_session_msg_send(HDC_SESSION session, unsigned char *buff_base, uint32_t buff_len)
{
    struct prof_hdc_msg *cmd_msg = (struct prof_hdc_msg *)buff_base;
    struct drvHdcMsg *hdc_msg = NULL;
    drvError_t ret;

    ret = drvHdcAllocMsg(session, &hdc_msg, 1);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to invoke function [drvHdcAllocMsg]. (ret=%d)\n", (int)ret);
        return ret;
    }

    ret = drvHdcAddMsgBuffer(hdc_msg, (char *)buff_base, (int)buff_len);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to invoke function [drvHdcAddMsgBuffer]. (ret=%d)\n", (int)ret);
        (void)drvHdcFreeMsg(hdc_msg);
        return ret;
    }

    (void)pthread_mutex_unlock(&(g_prof_hdc_info.prof_hdc_mutex));

    ret = halHdcSend(session, hdc_msg, 0, 0);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to invoke function [halHdcSend]. (ret=%d)\n", (int)ret);
        (void)drvHdcFreeMsg(hdc_msg);
        (void)pthread_mutex_lock(&(g_prof_hdc_info.prof_hdc_mutex));
        return ret;
    }

    PROF_DEBUG("Profile command sending was successful. (msg_type=%d)\n", cmd_msg->msg_type);

    ret = drvHdcFreeMsg(hdc_msg);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to invoke function [drvHdcFreeMsg]. (ret=%d)\n", (int)ret);
    }

    (void)pthread_mutex_lock(&(g_prof_hdc_info.prof_hdc_mutex));
    return ret;
}

STATIC drvError_t prof_per_session_disconnect(uint32_t dev_id, uint32_t chan_id)
{
    struct prof_hdc_msg cmd_msg;
    struct drvHdcEvent event;
    drvError_t ret;
    HDC_SESSION session_tmp = NULL;
    event.events = HDC_EPOLL_DATA_IN;

    if (g_prof_hdc_info.prof_channel_num_count[dev_id] == 0 ||
        g_prof_hdc_info.prof_channel_enable_flag[dev_id][chan_id] == 0) {
        PROF_RUN_INFO("Profile session had no channel client. HDC session had been closed. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_NOT_SUPPORT;
    }

    g_prof_hdc_info.prof_channel_num_count[dev_id]--;
    g_prof_hdc_info.prof_channel_enable_flag[dev_id][chan_id] = 0;
    if (g_prof_hdc_info.prof_channel_num_count[dev_id] != 0) {
        return DRV_ERROR_NONE;
    }

    session_tmp = g_prof_hdc_info.session[dev_id];
    ret = drvHdcEpollCtl(g_prof_hdc_info.epoll, HDC_EPOLL_CTL_DEL, (void *)session_tmp, &event);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to  delete the session. (dev_id=%u, ret=%d)\n", dev_id, (int)ret);
        return ret;
    }

    /* notice device close session */
    cmd_msg.msg_type = PROF_HDC_CLOSE_SESSION;
    ret = prof_session_msg_send(session_tmp, (unsigned char *)&cmd_msg, sizeof(struct prof_hdc_msg));
    if (ret != DRV_ERROR_NONE) {
        PROF_WARN("Unsuccessful to make profile session send messages. (dev_id=%u, ret=%d)\n", dev_id, (int)ret);
    }

    ret = drvHdcSessionClose(session_tmp);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to close session for profile device ID. (dev_id=%u, ret=%d)\n", dev_id, (int)ret);
        return ret;
    }

    g_prof_hdc_info.session_count--;
    PROF_INFO("Profile HDC Session had closed successfully. (dev_id=%u)\n", dev_id);

    return DRV_ERROR_NONE;
}

drvError_t prof_session_destroy(uint32_t dev_id, uint32_t chan_id)
{
    drvError_t ret;

    (void)pthread_mutex_lock(&(g_prof_hdc_info.prof_hdc_mutex));

    ret = prof_per_session_disconnect(dev_id, chan_id);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&(g_prof_hdc_info.prof_hdc_mutex));
        return ret;
    }

    if (g_prof_hdc_info.session_count == 0) {
        prof_destroy_receive_thread();
    }

    (void)pthread_mutex_unlock(&(g_prof_hdc_info.prof_hdc_mutex));

    return DRV_ERROR_NONE;
}

drvError_t prof_hdc_msg_send(uint32_t dev_id, uint32_t chan_id, unsigned char *buff_base, uint32_t buff_len)
{
    drvError_t ret;

    (void)pthread_mutex_lock(&(g_prof_hdc_info.prof_hdc_mutex));
    ret = prof_session_connect(dev_id, chan_id);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&(g_prof_hdc_info.prof_hdc_mutex));
        PROF_ERR("Failed to connect the prof session. (ret=%d, dev_id=%u, chan_id=%u)\n", (int)ret, dev_id, chan_id);
        return ret;
    }

    ret = prof_session_msg_send(g_prof_hdc_info.session[dev_id], buff_base, buff_len);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&(g_prof_hdc_info.prof_hdc_mutex));
        (void)prof_session_destroy(dev_id, chan_id);
        return ret;
    }

    (void)pthread_mutex_unlock(&(g_prof_hdc_info.prof_hdc_mutex));

    return DRV_ERROR_NONE;
}
#else
int prof_hdc_comm_ut_test(void)
{
    return 0;
}
#endif
