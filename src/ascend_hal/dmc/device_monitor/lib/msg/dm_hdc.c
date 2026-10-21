/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <errno.h>
#include "securec.h"

#include "dm_loop.h"
#include "dev_mon_log.h"
#include "device_monitor_type.h"
#include "mmpa_api.h"
#include "ascend_inpackage_hal.h"
#include "dms_user_interface.h"
#include "dm_hdc.h"

#ifdef DEV_MON_UT
#define STATIC
#else
#define STATIC static
#endif
STATIC unsigned int g_dm_hdc_run = 1;
#ifdef CFG_FEATURE_UB
#define DM_HDC_RETRY_TIMES 400
#else
#define DM_HDC_RETRY_TIMES 40
#endif
#define DM_HDC_RETRY_TIMEDELAY 3
#define DMHDC_CLIENT_SEND_RETRYTIME 40
#define DMHDC_CLIENT_SEND_NO_SESSION_RETRYTIME 20
#define DMHDC_CLIENT_SEND_TIMEOUT_RETRY_TIMES 3

#if (defined CFG_ENV_FPGA || defined CFG_ENV_ESL)
#define HDC_MSG_TIMEOUT (3000 * 50)
#elif defined CFG_ENV_EMU
#define HDC_MSG_TIMEOUT (3000 * 1000)
#else
#define HDC_MSG_TIMEOUT 3000
#endif

typedef struct recv_msg_st {
    HDC_SESSION session;
    DM_INTF_S *intf;
} RECV_MSG_ST;

STATIC int dm_hdc_get_intf_name(int dev_id, char *intf_name, int name_len)
{
    int ret = sprintf_s(intf_name, (size_t)name_len, DM_HDC_INTF "%d", dev_id);

    if (ret < 0) {
        DEV_MON_ERR("sprintf_s return error: %d.\n", ret);
        return EIO;
    }

    return 0;
}

STATIC int __format_hdc_msg(HDC_MSG_ST *hdc_msg, const DM_HDC_ADDR_ST *dst_addr, const DM_HDC_ADDR_ST *src_addr,
                            const DM_MSG_ST *msg, signed long msgid)
{
    int ret;
    ret = memset_s(hdc_msg, sizeof(*hdc_msg), 0, sizeof(*hdc_msg));
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, DEV_MON_ERR("hdc msg memset_s error: %d\n", ret));

    hdc_msg->peer_devid = dst_addr->peer_devid;
    hdc_msg->src_devid = src_addr->peer_devid;
    hdc_msg->msgid = (signed long long)msgid;

    /* copy the user data */
    if (msg->data_len > 0) {
        ret = memcpy_s(&(hdc_msg->data[0]), DM_MSG_DATA_MAX, msg->data, msg->data_len);
        DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, DEV_MON_ERR("hdc msg memcpy_s error: %d\n", ret));
    }

    hdc_msg->data_len = msg->data_len;
    return 0;
}

STATIC int __hdc_write_to_pipe(DM_INTF_S *intf, DM_MSG_TYPE msg_type, void *session, const DM_MSG_ST *msg,
                               signed long msgid)
{
    int ret;
    int err_buf;

    (void)msg_type;
    (void)session;
    (void)msgid;

    ret = (int)mm_write_file(intf->wfd, msg->data, msg->data_len);
    if (ret == -1) {
        err_buf = errno;
        if (err_buf == EAGAIN) {
            intf->stats.pipe_wr_fail++;
            if ((intf->stats.pipe_wr_fail >= DM_INTF_PIPE_WR_FAILED_CNT) &&
                (intf->stats.pipe_wr_fail <= DM_INTF_PIPE_WR_FAILED_MAX)) {
                DEV_MON_ERR("Unable to write to the pipe. (errno=%d; pipe_wr_fail=%u)\r\n", err_buf,
                            intf->stats.pipe_wr_fail);
            }
        } else {
            DEV_MON_ERR("Failed to write to the pipe. (errno=%d)\r\n", err_buf);
        }
        return err_buf;
    }
    intf->stats.pipe_wr_fail = 0;
    return 0;
}

STATIC int __dm_session_hdc_recv_proc(HDC_SESSION session, struct drvHdcMsg **p_rcvmsg, char **p_rcvbuf, int *buf_len)
{
    int ret;
    int recv_cnt = 0;
    int rcvbuf_count;

    ret = drvHdcAllocMsg(session, p_rcvmsg, 1);
    if (ret != OK) {
        return ret;
    }

    /* retry drvHdcRecv until exceed max num */
    while (recv_cnt < MAX_HDC_RECV_RETRY) {
        ret = halHdcRecv(session, *p_rcvmsg, DM_HDC_RECV_BUF_LEN, HDC_FLAG_WAIT_TIMEOUT, &rcvbuf_count,
                         HDC_MSG_TIMEOUT);
        if (ret == OK) {
            break;
        } else if (ret == DRV_ERROR_WAIT_TIMEOUT) {
            DEV_MON_INFO("halHdcRecv timeout recv_cnt:%d\n", recv_cnt);
            recv_cnt++;
            continue;
        } else {
            (void)drvHdcFreeMsg(*p_rcvmsg);
            *p_rcvmsg = NULL;
            return ret;
        }
    }

    if (recv_cnt == MAX_HDC_RECV_RETRY) {
        DEV_MON_ERR("halHdcRecv timeout.\n");
        (void)drvHdcFreeMsg(*p_rcvmsg);
        *p_rcvmsg = NULL;
        return ret;
    }

    ret = drvHdcGetMsgBuffer(*p_rcvmsg, 0, p_rcvbuf, buf_len);
    if (ret != OK) {
        (void)drvHdcFreeMsg(*p_rcvmsg);
        *p_rcvmsg = NULL;
        return ret;
    }

    return 0;
}

STATIC int __dm_session_recv_proc(DM_INTF_S *intf, HDC_SESSION session, DM_MSG_TYPE msg_type)
{
    DM_HDC_CB_ST *chan_cb = NULL;
    int ret;
    struct drvHdcMsg *p_rcvmsg = NULL;
    char *p_rcvbuf = NULL;
    int buf_len;
    DM_MSG_ST msg = {0};
    HDC_MSG_ST *hdc_msg = NULL;
    DRV_CHECK_RETV(intf, FAILED);
    DRV_CHECK_RETV(session, FAILED);
    chan_cb = (DM_HDC_CB_ST *)(intf->channel_cb);
    DRV_CHECK_RETV(chan_cb, FAILED);

    ret = __dm_session_hdc_recv_proc(session, &p_rcvmsg, &p_rcvbuf, &buf_len);
    if (ret != 0) {
        DEV_MON_ERR("hdc recv error %d \n", ret);
        return ret;
    }

    /* write data to pipe */
    hdc_msg = (HDC_MSG_ST *)p_rcvbuf;
    if (hdc_msg == NULL) {
        ret = FAILED;
        DEV_MON_ERR("p_rcvbuf is NULL\n");
        goto out;
    }

    if (hdc_msg->data_len > DM_MSG_DATA_MAX) {
        ret = FAILED;
        DEV_MON_ERR("hdc_msg->data_len %d is invalid, max len %d\n", hdc_msg->data_len, DM_MSG_DATA_MAX);
        goto out;
    }
    hdc_msg->session = (unsigned long long)(uintptr_t)session;
    msg.data = (unsigned char *)p_rcvbuf;
    msg.data_len = (unsigned short)buf_len;

    if (msg.data_len > sizeof(HDC_MSG_ST)) {
        ret = FAILED;
        DEV_MON_ERR("msg.data_len %u is invalid, max len %u\n", msg.data_len, sizeof(HDC_MSG_ST));
        goto out;
    }
    ret = __hdc_write_to_pipe(intf, msg_type, session, &msg, (signed long)(uintptr_t)&p_rcvbuf);

out:
    (void)drvHdcFreeMsg(p_rcvmsg);
    p_rcvmsg = NULL;

    return ret;
}

STATIC int g_hdc_thread_num = 0;
#define HDC_ACCEPT_THREAD_MAX 1024
#define HDC_THREAD_WAIT_INTERVAL (10 * 1000)
#define HDC_THREAD_WAIT_LOG_CNT 500
STATIC void hdc_thread_num_limit(void)
{
    unsigned int log_cnt = 0;

    while (g_hdc_thread_num >= HDC_ACCEPT_THREAD_MAX) {
        if (log_cnt % HDC_THREAD_WAIT_LOG_CNT == 0) {
            DEV_MON_WARNING("HDC thread_num exceed limit, (thread_num=%d)\n", g_hdc_thread_num);
        }
        log_cnt++;
        (void)usleep(HDC_THREAD_WAIT_INTERVAL);
    }
    if (log_cnt > 0) {
        DEV_MON_INFO("HDC thread_num recover, (thread_num=%d)\n", g_hdc_thread_num);
    }
}

STATIC void *__dm_server_recv_msg_proc(void *arg)
{
    int ret;
    HDC_SESSION session = NULL;
    DM_INTF_S *intf = NULL;
    RECV_MSG_ST *recv_msg = NULL;

    (void)prctl(PR_SET_NAME, "server_recv_msg_proc");

    if (arg == NULL) {
        DEV_MON_ERR("input arg is NULL\n");
        return NULL;
    }

    recv_msg = (RECV_MSG_ST *)arg;
    session = recv_msg->session;
    intf = recv_msg->intf;
    (void)drvHdcSetSessionReference(session);
    ret = __dm_session_recv_proc(intf, session, REQUEST_MSG);
    if (ret != 0) {
        if (ret != EAGAIN) {
            DEV_MON_ERR("Session processing failed. (ret=%d)\n", ret);
        }
        (void)drvHdcSessionClose(session);
    }

    free(recv_msg);
    recv_msg = NULL;
    (void)__sync_fetch_and_sub(&g_hdc_thread_num, 1);
    return NULL;
}

STATIC void *__dm_server_accept_proc(void *arg)
{
    HDC_SESSION session = NULL;
    HDC_SERVER server = NULL;
    DM_INTF_S *intf = NULL;
    DM_HDC_CB_ST *chan_cb = NULL;
    int ret;
    pthread_t server_thread;
    pthread_attr_t attr = {{0}};
    RECV_MSG_ST *recv_msg_proc_st = NULL;

    intf = (DM_INTF_S *)arg;
    chan_cb = (DM_HDC_CB_ST *)(intf->channel_cb);
    server = (HDC_SERVER)chan_cb->cb;

    (void)mmSetCurrentThreadName("server_accept");

    while (g_dm_hdc_run != 0) {
        ret = drvHdcSessionAccept(server, &session);
        if (ret != OK) {
            (void)usleep(1000); // 1000 sleep 1ms
            continue;
        }

        recv_msg_proc_st = malloc(sizeof(RECV_MSG_ST));
        if (recv_msg_proc_st == NULL) {
            (void)drvHdcSessionClose(session);
            continue;
        }

        /* the pointer recv_msg_proc_st get value below, so not need memset */
        recv_msg_proc_st->intf = intf;
        recv_msg_proc_st->session = session;
        (void)pthread_attr_init(&attr);
        (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        (void)pthread_attr_setstacksize(&attr, DEV_MON_ROOT_STACK_SIZE);

        if ((pthread_create(&server_thread, &attr, __dm_server_recv_msg_proc, (void *)recv_msg_proc_st) != 0)) {
            DEV_MON_ERR("pthread_create fail errno=%d:%s\n", errno, strerror(errno));
            (void)drvHdcSessionClose(session);
            (void)pthread_attr_destroy(&attr);
            free(recv_msg_proc_st);
            recv_msg_proc_st = NULL;
            (void)usleep(1000); // 1000 sleep 1ms
            continue;
        }
        (void)pthread_attr_destroy(&attr);
        __sync_fetch_and_add(&g_hdc_thread_num, 1);
        hdc_thread_num_limit();
    }

    return NULL;
}

#define DM_HDC_FD_NUM 2
STATIC int __dm_hdc_open(DM_INTF_S *intf)
{
    int fds[DM_HDC_FD_NUM] = {0};
    HDC_CLIENT client = NULL;
    HDC_SERVER server = NULL;
    DM_HDC_CB_ST *chan_cb = NULL;
    int ret;
    unsigned int retry_times = 0;
    DM_HDC_ADDR_ST *myaddr = NULL;
    pthread_t server_thread;
    mmThreadAttr thread_attr = {0};
    mmThread *thread_handle = {0};
    mmUserBlock_t func_block = {0};

    if ((pipe(fds) < 0) || (fcntl(fds[0], F_SETFL, O_NONBLOCK) < 0) || (fcntl(fds[1], F_SETFL, O_NONBLOCK) < 0) ||
        (fcntl(fds[0], F_SETFD, FD_CLOEXEC) < 0) || (fcntl(fds[1], F_SETFD, FD_CLOEXEC) < 0)) {
        return errno;
    }

    intf->rfd = fds[0];
    intf->wfd = fds[1];
    intf->channel_cb = malloc(sizeof(DM_HDC_CB_ST));
    DRV_CHECK_RETV((intf->channel_cb != NULL), -ENOMEM);
    ret = memset_s((void *)intf->channel_cb, sizeof(DM_HDC_CB_ST), 0, sizeof(DM_HDC_CB_ST));
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail: %d\n", ret);
        free(intf->channel_cb);
        intf->channel_cb = NULL;
        return ret;
    }

    myaddr = (DM_HDC_ADDR_ST *)&(intf->my_addr);
    chan_cb = (DM_HDC_CB_ST *)(intf->channel_cb);
    chan_cb->cb = NULL;

    if (!myaddr->hdc_type) {
        ret = drvHdcClientCreate(&client, MAX_HDC_CLIENT, HDC_SERVICE_TYPE_DMP, 0);
        if (ret != OK) {
            DEV_MON_ERR("%s%d%s%d\n", __FUNCTION__, __LINE__, "error=", ret);
            free(intf->channel_cb);
            intf->channel_cb = NULL;
            return ret;
        }

        chan_cb->cb = (void *)client;
    } else {
    retry:
        ret = drvHdcServerCreate(myaddr->dev_id, HDC_SERVICE_TYPE_DMP, &server);
        if (ret != OK) {
            if (retry_times++ > DM_HDC_RETRY_TIMES) {
                free(intf->channel_cb);
                intf->channel_cb = NULL;
                DEV_MON_ERR("drvHdcServerCreate failed,%s%d%s%d\n", __FUNCTION__, __LINE__, "error=", ret);
                return ret;
            } else {
                (void)sleep(DM_HDC_RETRY_TIMEDELAY);
                goto retry;
            }
        }

        chan_cb->cb = (void *)server;

        thread_attr.stackFlag = TRUE;
        thread_attr.detachFlag = TRUE;
        thread_attr.stackSize = DEV_MON_ROOT_STACK_SIZE;
        thread_handle = &server_thread;
        func_block.procFunc = __dm_server_accept_proc;
        func_block.pulArg = (void *)intf;

        ret = mmCreateTaskWithThreadAttr(thread_handle, &func_block, &thread_attr);
        if (ret != 0) {
            DEV_MON_ERR("pthread_create error,ret = %d,errno=%d:%s\n", ret, errno, strerror(errno));
            (void)drvHdcServerDestroy(server);
            ret = mmSleep(1);
            DRV_CHECK_STR(ret == 0);
            free(intf->channel_cb);
            intf->channel_cb = NULL;
            return -1;
        }
    }

    return 0;
}

STATIC int dm_hdc_session_connect(int peer_node, int peer_devid, HDC_CLIENT client, HDC_SESSION *session)
{
    unsigned int dmp_started = false;
    int retry_i = 0;
    int retry_j = 0;
    int ret;
    (void)peer_node;

    ret = drvGetDmpStarted((uint32_t)peer_devid, &dmp_started);
    if (ret != OK) {
        DEV_MON_ERR("%s %d err, devid %d ret %d, dmp_started %u\n", __FUNCTION__, __LINE__, peer_devid, ret,
                    dmp_started);
        return DRV_ERROR_DEVICE_NOT_READY;
    }

retry:
    ret = drvHdcSessionConnect(0, peer_devid, client, session);
    /* need to retry when Session connection not listen */
    if ((ret == DRV_ERROR_REMOTE_NOT_LISTEN || ret == DRV_ERROR_DEVICE_NOT_READY) && (dmp_started == false) &&
        (retry_i < DMHDC_CLIENT_SEND_RETRYTIME)) {
        retry_i++;
        (void)sleep(1);
        goto retry;
    } else if ((ret == DRV_ERROR_REMOTE_NO_SESSION) && (retry_j < DMHDC_CLIENT_SEND_NO_SESSION_RETRYTIME)) {
        retry_j++;
        (void)usleep(1000); /* 1000 sleep 1ms */
        goto retry;
    }
    if (ret != OK) {
        DEV_MON_ERR("%s %d failed.(dev_id=%d ret=%d, retry_i=%d, retry_j=%d)\n", __FUNCTION__, __LINE__, peer_devid,
                    ret, retry_i, retry_j);
        return ret;
    }
    return ret;
}

STATIC int dm_hdc_send_msg(const DM_ADDR_ST *addr, DM_INTF_S *intf, const DM_MSG_ST *msg, DM_MSG_TYPE msg_type,
                           signed long msgid, HDC_SESSION session)
{
    HDC_MSG_ST *hdc_msg = NULL;
    struct drvHdcMsg *p_msg_snd = NULL;
    const DM_HDC_ADDR_ST *dstaddr = (const DM_HDC_ADDR_ST *)addr;
    DM_HDC_ADDR_ST *myaddr = (DM_HDC_ADDR_ST *)&(intf->my_addr);
    int retry_times = 0;
    int ret;

    hdc_msg = (HDC_MSG_ST *)malloc(sizeof(HDC_MSG_ST));
    DRV_CHECK_RETV_DO_SOMETHING((hdc_msg != NULL), DRV_ERROR_NO_RESOURCES, DEV_MON_ERR("msg malloc err\n"));
    ret = __format_hdc_msg(hdc_msg, dstaddr, myaddr, msg, msgid);
    if (ret != 0) {
        DEV_MON_ERR("hdc msg format failed, (ret=%d)\n", ret);
        goto FREE_HDC_MSG;
    }
    (void)drvHdcSetSessionReference(session);
    hdc_msg->session = (unsigned long long)(uintptr_t)session;
    hdc_msg->msg_type = msg_type;

    ret = drvHdcAllocMsg(session, &p_msg_snd, 1);
    if (ret != OK) {
        DEV_MON_ERR("%s %d alloc msg err. (ret=%d)\n", __FUNCTION__, __LINE__, ret);
        goto FREE_HDC_MSG;
    }

    ret = drvHdcAddMsgBuffer(p_msg_snd, (char *)hdc_msg, (int)(HDCMSG_HEAD_SIZE + hdc_msg->data_len));
    if (ret != OK) {
        DEV_MON_ERR("%s %d add msg err, ret %d\n", __FUNCTION__, __LINE__, ret);
        goto FREE_MSG_SND;
    }

    while (retry_times < DMHDC_CLIENT_SEND_TIMEOUT_RETRY_TIMES) {
        ret = halHdcSend(session, p_msg_snd, HDC_FLAG_WAIT_TIMEOUT, HDC_MSG_TIMEOUT);
        if (ret == DRV_ERROR_WAIT_TIMEOUT) {
            DEV_MON_INFO("halHdcSend timeout. (ret=%d; retry_count=%d)\n", ret, retry_times);
            retry_times++;
            continue;
        }
        break;
    }
    if (ret != OK) {
        DEV_MON_ERR("Sending message was abnormal. (func=\"%s\"; line=%d; retry_count=%d; ret=%d)\n", __FUNCTION__,
                    __LINE__, retry_times, ret);
        goto FREE_MSG_SND;
    }

FREE_MSG_SND:
    (void)drvHdcFreeMsg(p_msg_snd);
    p_msg_snd = NULL;
FREE_HDC_MSG:
    free(hdc_msg);
    hdc_msg = NULL;
    return ret;
}

STATIC int __dm_hdc_client_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, const DM_ADDR_ST *addr, unsigned int addr_len,
                                const DM_MSG_ST *msg, signed long msgid)
{
    int ret;
    HDC_SESSION session = NULL;
    DM_HDC_CB_ST *chan_cb = intf->channel_cb;
    const DM_HDC_ADDR_ST *dstaddr = (const DM_HDC_ADDR_ST *)addr;
    (void)addr_len;

    ret = dm_hdc_session_connect(0, dstaddr->peer_devid, (HDC_CLIENT)(chan_cb->cb), &session);
    if (ret != 0) {
        DEV_MON_ERR("dm_hdc_session_connect failed. (dev_id=%d; ret=%d)\n", dstaddr->peer_devid, ret);
        return ret;
    }

    ret = dm_hdc_send_msg(addr, intf, msg, msg_type, msgid, session);
    if (ret != 0) {
        DEV_MON_ERR("dm_hdc_send_msg failed. (dev_id=%d; ret=%d)\n", dstaddr->peer_devid, ret);
        (void)drvHdcSessionClose(session);
        return ret;
    }

    ret = __dm_session_recv_proc(intf, session, msg_type);
    if (ret != 0) {
        DEV_MON_ERR("__dm_session_recv_proc failed. (ret=%d)\n", ret);
        (void)drvHdcSessionClose(session);
        return ret;
    }

    (void)drvHdcSessionClose(session);
    return 0;
}

STATIC int __dm_hdc_server_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, DM_ADDR_ST *addr, unsigned int addr_len,
                                const DM_MSG_ST *msg, signed long msgid)
{
    int ret;
    HDC_SESSION session = NULL;
    struct drvHdcMsg *p_msg_snd = NULL;
    DM_HDC_ADDR_ST *dstaddr = NULL;
    DM_HDC_ADDR_ST *myaddr = NULL;
    HDC_MSG_ST *hdc_msg = NULL;
    (void)addr_len;

    dstaddr = (DM_HDC_ADDR_ST *)addr;
    myaddr = (DM_HDC_ADDR_ST *)&(intf->my_addr);
    session = (HDC_SESSION)(uintptr_t)dstaddr->session;

    if (dstaddr->hdc_work_status == HDC_ADDR_CLOSE) {
        DEV_MON_WARNING("__dm_hdc_server_send: session already close. hdc_work_status = %d\n",
                        dstaddr->hdc_work_status);
        return DRV_ERROR_INNER_ERR;
    }

    hdc_msg = (HDC_MSG_ST *)calloc(1, sizeof(HDC_MSG_ST));
    if (hdc_msg == NULL) {
        DEV_MON_ERR("__dm_hdc_server_send: malloc failed. (errno=%d)\n", errno);
        (void)drvHdcSessionClose(session);
        dstaddr->hdc_work_status = HDC_ADDR_CLOSE;
        return DRV_ERROR_INNER_ERR;
    }

    ret = __format_hdc_msg(hdc_msg, dstaddr, myaddr, msg, msgid);
    if (ret != 0) {
        DEV_MON_ERR("hdc msg format error: %d\n", ret);
        goto dm_hdc_server_free_hdc_msg;
    }
    hdc_msg->session = (unsigned long long)(uintptr_t)session;
    hdc_msg->msg_type = msg_type;

    ret = drvHdcAllocMsg(session, &p_msg_snd, 1);
    if (ret != OK) {
        DEV_MON_ERR("%s %d alloc msg err, ret %d\n", __FUNCTION__, __LINE__, ret);
        goto dm_hdc_server_free_hdc_msg;
    }

    ret = (int)drvHdcAddMsgBuffer(p_msg_snd, (char *)hdc_msg, (int)(HDCMSG_HEAD_SIZE + hdc_msg->data_len));
    if (ret != OK) {
        DEV_MON_ERR("%s %d add msg err, ret %d\n", __FUNCTION__, __LINE__, ret);
        goto dm_hdc_server_free_msg;
    }

    ret = halHdcSend(session, p_msg_snd, HDC_FLAG_WAIT_TIMEOUT, HDC_MSG_TIMEOUT);
    if (ret != OK) {
        DEV_MON_ERR("%s %d send err, ret %d\n", __FUNCTION__, __LINE__, ret);
        goto dm_hdc_server_free_msg;
    }

dm_hdc_server_free_msg:
    (void)drvHdcFreeMsg(p_msg_snd);
    p_msg_snd = NULL;
dm_hdc_server_free_hdc_msg:
    free(hdc_msg);
    hdc_msg = NULL;
    /* server CLOSE */
    (void)drvHdcSessionClose(session);
    dstaddr->hdc_work_status = HDC_ADDR_CLOSE;
    return ret;
}

STATIC int __dm_hdc_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, DM_ADDR_ST *addr, unsigned int addr_len,
                         const DM_MSG_ST *msg, signed long msgid)
{
    DM_HDC_ADDR_ST *myaddr = NULL;
    DRV_CHECK_RETV(intf, FAILED);
    myaddr = (DM_HDC_ADDR_ST *)&(intf->my_addr);
    DRV_CHECK_RETV(myaddr, FAILED);

    /* if there is no connection on the server, sending will fail directly */
    if (myaddr->hdc_type == HDC_SERVER_TYPE) {
        return __dm_hdc_server_send(intf, msg_type, addr, addr_len, msg, msgid);
    } else {
        return __dm_hdc_client_send(intf, msg_type, addr, addr_len, msg, msgid);
    }
}

STATIC int __dm_hdc_settime_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, DM_ADDR_ST *addr, unsigned int addr_len,
                                 const DM_MSG_ST *msg, int retries, unsigned int retry_time_ms, signed long msgid)
{
    (void)retries;
    (void)retry_time_ms;
    return __dm_hdc_send(intf, msg_type, addr, addr_len, msg, msgid);
}

STATIC int __dm_hdc_set_retries(DM_INTF_S *intf, int retries, unsigned int retry_time_ms)
{
    /* intf need not retry, openipmi driver will accomplish the task */
    intf->retries = retries;
    intf->retry_time_ms = retry_time_ms;
    return 0;
}

STATIC int __dm_hdc_get_retries(DM_INTF_S *intf, int *retries, unsigned int *retry_time_ms)
{
    /* intf need not retry, openipmi driver will accomplish the task */
    *retries = intf->retries;
    *retry_time_ms = intf->retry_time_ms;
    return 0;
}

STATIC int dm_get_session_propery(DM_RECV_ST *irecv, HDC_MSG_ST *msg)
{
    int run_env = 0;
    int root_priv = 0;
    int vfid = 0;
    int ret;

    ret = halHdcGetSessionAttr((void *)(uintptr_t)msg->session, HDC_SESSION_ATTR_UID, &root_priv);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), -1, free(msg); msg = NULL;
                                DEV_MON_ERR("get session uid failed and ret=%d\n", ret););
    irecv->host_root = root_priv;

    ret = halHdcGetSessionAttr((void *)(uintptr_t)msg->session, HDC_SESSION_ATTR_VFID, &vfid);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), -1, free(msg); msg = NULL;
                                DEV_MON_ERR("get session vfid failed and ret=%d\n", ret););
    irecv->vfid = (unsigned int)vfid;

    ret = halHdcGetSessionAttr((void *)(uintptr_t)msg->session, HDC_SESSION_ATTR_RUN_ENV, &run_env);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), -1, free(msg); msg = NULL;
                                DEV_MON_ERR("get session run env failed and ret=%d\n", ret););
    switch (run_env) {
        case RUN_ENV_PHYSICAL_CONTAINER:
        case RUN_ENV_VIRTUAL_CONTAINER:
#ifndef DMPUT
            irecv->session_prop = CONTAINER_PROP;
            break;
#endif
            /* except exceptions and host, the rest are guest */
        case RUN_ENV_VIRTUAL:
            irecv->session_prop = GUEST_PROP;
            break;
        case RUN_ENV_PHYSICAL:
            irecv->session_prop = ADMIN_PROP;
            break;
        default:
            DEV_MON_ERR("don't know session property(%d)\n", run_env);
            free(msg);
            msg = NULL;
            return -1;
    }
    return 0;
}

STATIC int __dm_hdc_recv(DM_INTF_S *intf, int fd, short revents, DM_RECV_ST *irecv)
{
    HDC_MSG_ST *msg = NULL;
    DM_HDC_ADDR_ST *addr = (DM_HDC_ADDR_ST *)irecv->addr;
    DM_HDC_ADDR_ST *myaddr = NULL;
    DEV_MP_MSG_ST *msg_content = NULL;
    int ret;
    unsigned short tmp_event = (unsigned short)revents;
    myaddr = (DM_HDC_ADDR_ST *)&(intf->my_addr);
    (void)intf;

    if ((tmp_event & POLLIN) != 0) {
        msg = (HDC_MSG_ST *)malloc(sizeof(HDC_MSG_ST));
        DRV_CHECK_RETV((msg != NULL), -ENOMEM);
        ret = memset_s(msg, sizeof(HDC_MSG_ST), 0, (sizeof(HDC_MSG_ST)));

        return_val_do_info((ret != 0), -EINVAL, msg, DEV_MON_ERR("__dm_hdc_recv: memset_s fail.errno=%d\n", errno));

        /* read msg from selfloop channel */
        ret = (int)mm_read_file(fd, (void *)msg, HDCMSG_HEAD_SIZE);
        if (ret != HDCMSG_HEAD_SIZE) {
            DEV_MON_ERR("%s%d\n", "__dm_hdc_recv: read fail.errno=", errno);
            free(msg);
            msg = NULL;
            return -1;
        }

        if ((msg->data_len) > DM_MSG_DATA_MAX) {
            DEV_MON_ERR("%s%d\n", "__dm_hdc_recv: msg->data_len too long=", msg->data_len);
            free(msg);
            msg = NULL;
            return -1;
        }

        ret = (int)mm_read_file(fd, (void *)msg->data, msg->data_len);
        if (msg->data_len != ret) {
            DEV_MON_ERR("%s%d\n", "__dm_hdc_recv: read fail.errno=", errno);
            free(msg);
            msg = NULL;
            return -1;
        }
        irecv->recv_type = msg->msg_type;
        /* get source address */
        addr = (DM_HDC_ADDR_ST *)irecv->addr;
        addr->addr_type = DM_HDC_ADDR_TYPE;
        addr->channel = DM_HDC_CHANNEL;
        addr->hdc_type = ((myaddr->hdc_type) ? 0 : 1);
        addr->peer_node = msg->src_devid;
        addr->peer_devid = msg->peer_devid;
        addr->session = msg->session;
        addr->hdc_work_status = HDC_ADDR_WORK;
        irecv->addr_len = sizeof(*addr);
        irecv->msgid = msg->msgid;
        irecv->dev_id = myaddr->dev_id;

        /* get session property from hdc in server */
        if (myaddr->hdc_type == DMP_SERVER) {
            ret = dm_get_session_propery(irecv, msg);
            DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, (void)drvHdcSessionClose((HDC_SESSION)addr->session);
                                        DEV_MON_ERR("get session property failed.\n"));
        }

        if (irecv->msg.data_len >= msg->data_len) {
            irecv->msg.data_len = msg->data_len;
        }

        if (irecv->msg.data_len && (irecv->msg.data_len <= DM_MSG_DATA_MAX)) {
            ret = memcpy_s(irecv->msg.data, irecv->msg.data_len, msg->data, irecv->msg.data_len);
            DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(msg); msg = NULL;
                                        (void)drvHdcSessionClose((HDC_SESSION)addr->session);
                                        DEV_MON_ERR("memcpy_s error\n"));
        }

        // update content data_len for security reason
        msg_content = (DEV_MP_MSG_ST *)irecv->msg.data;
        msg_content->length = (unsigned int)(irecv->msg.data_len < (unsigned short)(sizeof(DEV_MP_MSG_ST)) ?
                                                 0 :
                                                 irecv->msg.data_len - sizeof(DEV_MP_MSG_ST));
        free(msg);
        msg = NULL;
    }

    return 0;
}

STATIC void __dm_hdc_close(DM_INTF_S *intf)
{
    int ret;
    DM_HDC_CB_ST *chan_cb = NULL;
    DM_HDC_ADDR_ST *myaddr = NULL;
    g_dm_hdc_run = 0;
    ret = strncmp(intf->name, DM_HDC_INTF, strlen(DM_HDC_INTF));
    if (ret != 0) {
        DEV_MON_ERR("strncmp failed, %s cannot match %s\n", intf->name, DM_HDC_INTF);
        return;
    }

    myaddr = (DM_HDC_ADDR_ST *)&(intf->my_addr);
    chan_cb = (DM_HDC_CB_ST *)(intf->channel_cb);

    if (chan_cb != NULL) {
        if (!myaddr->hdc_type) {
            (void)drvHdcClientDestroy((HDC_CLIENT)chan_cb->cb);
        } else {
            (void)drvHdcServerDestroy((HDC_SERVER)chan_cb->cb);
        }
        free(intf->channel_cb);
        intf->channel_cb = NULL;
    }

    ret = mm_close_file(intf->rfd);
    DRV_CHECK_CHK(ret == 0);
    ret = mm_close_file(intf->wfd);
    DRV_CHECK_CHK(ret == 0);
    return;
}

int dm_hdc_init(DM_INTF_S **my_intf, DM_CB_S *cb, DM_MSG_TIMEOUT_HNDL_T timeout_hndl, DM_ADDR_ST *my_addr,
                const char *my_name, int name_len)
{
    DM_INTF_S *intf = NULL;
    char name[sizeof(intf->name)] = {0};
    struct drvHdcCapacity capacity = {0};
    int ret;
    DM_HDC_ADDR_ST *hdc_addr = (DM_HDC_ADDR_ST *)my_addr;
    unsigned int channel_max_trans_len;

    if (cb == NULL) {
        return -EINVAL;
    }

    if ((my_addr == NULL) || (my_addr->addr_type != DM_HDC_ADDR_TYPE) || (my_addr->channel != DM_HDC_CHANNEL)) {
        return -EINVAL;
    }

    if ((my_name != NULL) && (name_len >= (int)sizeof(intf->name))) {
        return -EINVAL;
    }

    ret = dm_hdc_get_intf_name(hdc_addr->dev_id, name, sizeof(intf->name));
    if (ret != 0) {
        return ret;
    }

    ret = drvHdcGetCapacity(&capacity);
    if (ret != 0) {
        DEV_MON_ERR("drvHdcGetCapacity fail: %d\n", ret);
        return ret;
    }

    name[sizeof(name) - 1] = 0;
    intf = dm_get_intf(cb, name, (int)strlen(name));
    if (intf != NULL) {
        DEV_MON_ERR("duplicate interface [%s]\r\n", name);
        return -EBUSY;
    }

    intf = calloc(1, sizeof(*intf));
    if (intf == NULL) {
        return -ENOMEM;
    }

    ret = strncpy_s(intf->name, sizeof(intf->name), name, sizeof(intf->name) - 1);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(intf); intf = NULL; DEV_MON_ERR("strncpy_s error\n"));
    intf->name[DM_INTF_NAME_LEN - 1] = '\0';
    intf->my_addr = *my_addr;
    ret = __dm_hdc_open(intf);
    if (ret != 0) {
        free(intf);
        intf = NULL;
        return ret;
    }

    intf->retries = 0;
    intf->retry_time_ms = DSMI_MSG_TIMEOUT;
    /* Data frame format: | dm: hdc msg head | dm msg head | dm cmd payload | */
    channel_max_trans_len = capacity.maxSegment > (unsigned int)DM_MSG_DATA_MAX ? (unsigned int)DM_MSG_DATA_MAX :
                                                                                  capacity.maxSegment;
    DRV_CHECK_RETV_DO_SOMETHING((channel_max_trans_len > (unsigned int)HDCMSG_HEAD_SIZE), -EINVAL,
                                DEV_MON_ERR("Hdc channel length is abnormal. (len=%u, min=%u)\n", channel_max_trans_len,
                                            (unsigned int)HDCMSG_HEAD_SIZE);
                                __dm_hdc_close(intf); free(intf); intf = NULL);
    intf->max_trans_len = (unsigned int)(channel_max_trans_len - HDCMSG_HEAD_SIZE);
    intf->recv_msg = __dm_hdc_recv;
    intf->send_msg = __dm_hdc_send;
    intf->send_msg_settime = __dm_hdc_settime_send;
    intf->set_retries = __dm_hdc_set_retries;
    intf->get_retries = __dm_hdc_get_retries;
    intf->close = __dm_hdc_close;
    intf->timeout_hndl = timeout_hndl;
    intf->dm_cb = (void *)cb;
    intf->stats.pipe_wr_fail = 0;
    intf->stats.msg_handle_timeout = 0;
    ret = dm_intf_register(cb, intf);
    if (ret != 0) {
        DEV_MON_ERR("dm_hdc_init->dm_intf_register fail.\n");
        __dm_hdc_close(intf);
        free(intf);
        intf = NULL;
        return ret;
    }

    if (my_intf != NULL) {
        *my_intf = intf;
    }

    return 0;
}
