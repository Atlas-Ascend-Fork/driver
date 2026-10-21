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
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "securec.h"

#include "dm_msg_intf.h"
#include "dev_mon_log.h"
#include "mmpa_api.h"
#include "dm_loop.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define __DM_LOOP_FD_NUM 2

STATIC int __selfloop_open(DM_INTF_S *intf)
{
    int fds[__DM_LOOP_FD_NUM];

    if ((pipe(fds) < 0) || (fcntl(fds[0], F_SETFL, O_NONBLOCK) < 0) || (fcntl(fds[1], F_SETFL, O_NONBLOCK) < 0) ||
        (fcntl(fds[0], F_SETFD, FD_CLOEXEC) < 0) || (fcntl(fds[1], F_SETFD, FD_CLOEXEC) < 0)) {
        return errno;
    }

    intf->rfd = fds[0];
    intf->wfd = fds[1];
    return 0;
}

STATIC void __selfloop_close(DM_INTF_S *intf)
{
    int ret;
    ret = strncmp(intf->name, SELFLOOP_INTF, strlen(SELFLOOP_INTF));
    if (ret != 0) {
        return;
    }

    ret = mm_close_file(intf->rfd);
    DRV_CHECK_CHK(ret == 0);
    ret = mm_close_file(intf->wfd);
    DRV_CHECK_CHK(ret == 0);
    return;
}

STATIC int __selfloop_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, DM_ADDR_ST *addr, unsigned int addr_len,
                           const DM_MSG_ST *msg, signed long msgid)
{
    int ret;
    int err_buf;
    SELF_MSG_ST *req = NULL;
    req = (SELF_MSG_ST *)malloc(sizeof(SELF_MSG_ST));
    DRV_CHECK_RETV((req != NULL), -ENOMEM);
    ret = memset_s(req, sizeof(SELF_MSG_ST), 0, (sizeof(SELF_MSG_ST)));
    if (ret != 0) {
        free(req);
        req = NULL;
        DEV_MON_ERR("__selfloop_send: memset_s fail.errno=%d\n", errno);
        return -EINVAL;
    }

    req->addr = *addr;
    req->addr_len = (int)addr_len;
    req->msgid = msgid;
    req->msg_type = msg_type;

    if (msg->data_len > DM_MSG_DATA_MAX) {
        free(req);
        req = NULL;
        DEV_MON_ERR("__selfloop_send: msg->data_len too long, data_len=%d, max=%d\n", msg->data_len, DM_MSG_DATA_MAX);
        return EINVAL;
    }

    req->data_len = msg->data_len;
    ret = memcpy_s(req->data, DM_MSG_DATA_MAX, msg->data, msg->data_len);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(req); req = NULL; DEV_MON_ERR("memcpy_s error\n"));
    ret = (int)mm_write_file(intf->wfd, req, (int)(SELFMSG_HEAD_SIZE + req->data_len));
    if (ret == -1) {
        err_buf = errno;
        DEV_MON_ERR("__selfloop_send: write fail.errno=%d\n", err_buf);
        free(req);
        req = NULL;
        return err_buf;
    }

    free(req);
    req = NULL;
    return 0;
}

STATIC int __selfloop_settime_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, DM_ADDR_ST *addr, unsigned int addr_len,
                                   const DM_MSG_ST *msg, int retries, unsigned int retry_time_ms, signed long msgid)
{
    (void)retries;
    (void)retry_time_ms;
    return __selfloop_send(intf, msg_type, addr, addr_len, msg, msgid);
}

STATIC int __selfloop_recv(DM_INTF_S *intf, int fd, short revents, DM_RECV_ST *irecv)
{
    SELF_MSG_ST *tmp = NULL;
    DM_ADDR_ST *addr = (DM_ADDR_ST *)irecv->addr;
    int ret;
    unsigned short tmp_event = (unsigned short)revents;
    (void)intf;

    if ((tmp_event & POLLIN) != 0) {
        tmp = (SELF_MSG_ST *)malloc(sizeof(SELF_MSG_ST));
        DRV_CHECK_RETV((tmp != NULL), -ENOMEM);
        ret = memset_s(tmp, sizeof(SELF_MSG_ST), 0, (sizeof(SELF_MSG_ST)));
        if (ret != 0) {
            free(tmp);
            tmp = NULL;
            DEV_MON_ERR("__selfloop_recv: memset_s fail.errno=%d\n", errno);
            return -EINVAL;
        }

        /* read msg from selfloop channel */
        ret = (int)mm_read_file(fd, (void *)tmp, SELFMSG_HEAD_SIZE);
        DRV_CHECK_DO_SOMETHING(ret == SELFMSG_HEAD_SIZE, free(tmp); tmp = NULL; DEV_MON_ERR("errno=%d\n", errno);
                               return -1);

        if ((tmp->data_len <= DM_MSG_DATA_MAX) && (tmp->data_len > 0)) {
            ret = (int)mm_read_file(fd, (void *)tmp->data, tmp->data_len);
            DRV_CHECK_DO_SOMETHING(tmp->data_len == ret, free(tmp); tmp = NULL;
                                   DEV_MON_ERR("__selfloop_recv: read failed. (errno=%d)\n", errno); return -1);
        } else {
            DEV_MON_ERR("%s%d\r\n", "__selfloop_recv: msg->data_len too long=", tmp->data_len);
            free(tmp);
            tmp = NULL;
            return -1;
        }

        irecv->recv_type = tmp->msg_type;
        *addr = tmp->addr;
        irecv->addr_len = 6; // 6 addr len
        irecv->msgid = tmp->msgid;
        irecv->dev_id = 0;
        irecv->session_prop = ADMIN_PROP;
        irecv->host_root = ROOT_PRIV;
        DEV_MON_INFO("selfloop data_len %d, 0x%x\n", tmp->data_len, *(tmp->data));

        if (irecv->msg.data_len >= tmp->data_len) {
            irecv->msg.data_len = tmp->data_len;
        }

        if (irecv->msg.data_len && (irecv->msg.data_len <= DM_MSG_DATA_MAX) && (irecv->msg.data_len >= tmp->data_len)) {
            ret = memcpy_s(irecv->msg.data, irecv->msg.data_len, tmp->data, tmp->data_len);
            DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(tmp); tmp = NULL; DEV_MON_ERR("memcpy_s error\n"));
        }
    }

    free(tmp);
    tmp = NULL;
    return 0;
}

STATIC int __selfloop_set_retries(DM_INTF_S *intf, int retries, unsigned int retry_time_ms)
{
    /* intf need not retry, openipmi driver will accomplish the task */
    intf->retries = retries;
    intf->retry_time_ms = retry_time_ms;
    return 0;
}

STATIC int __selfloop_get_retries(DM_INTF_S *intf, int *retries, unsigned int *retry_time_ms)
{
    /* intf need not retry, openipmi driver will accomplish the task */
    *retries = intf->retries;
    *retry_time_ms = intf->retry_time_ms;
    return 0;
}

STATIC void selfloop_init_intf(DM_CB_S *cb, DM_INTF_S *intf)
{
    intf->retries = 0;
    intf->retry_time_ms = RETRY_TIMEOUT_MS;
    intf->recv_msg = __selfloop_recv;
    intf->send_msg_settime = __selfloop_settime_send;
    intf->send_msg = __selfloop_send;
    intf->set_retries = __selfloop_set_retries;
    intf->get_retries = __selfloop_get_retries;
    intf->close = __selfloop_close;
    intf->dm_cb = (void *)cb;
}

int selfloop_init(DM_INTF_S **my_intf, DM_CB_S *cb, const DM_ADDR_ST *my_addr, const char *my_name, int name_len)
{
    DM_INTF_S *intf = NULL;
    DM_LOOP_ADDR_S *addr = NULL;
    char name[sizeof(intf->name)] = {0};
    int ret, ret_t, errno_tmp;

    (void)name_len;
    if ((cb == NULL) ||
        ((my_addr != NULL) && ((my_addr->addr_type != DM_LOOP_ADDR_TYPE) || (my_addr->channel != DM_LOOP_CHANNEL)))) {
        return EINVAL;
    }

    ret = strncpy_s(name, sizeof(name), my_name ? my_name : (const char *)SELFLOOP_INTF, sizeof(name) - 1);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, DEV_MON_ERR("strncpy_s error\n"));
    name[sizeof(name) - 1] = 0;
    intf = dm_get_intf(cb, name, sizeof(name));
    DRV_CHECK_DO_SOMETHING(intf == NULL, DEV_MON_ERR("duplicate interface [%s]\r\n", name); return EBUSY);

    intf = malloc(sizeof(*intf));
    errno_tmp = errno;
    DRV_CHECK_DO_SOMETHING(intf != NULL, DEV_MON_ERR("malloc fail, errno=%d.\n", errno_tmp); return ENOMEM);

    ret = memset_s((void *)intf, sizeof(*intf), 0, sizeof(*intf));
    DRV_CHECK_DO_SOMETHING(ret == 0, free(intf); intf = NULL; DEV_MON_ERR("memset_s fail:%d\n", ret); return ret);

    ret = strncpy_s(intf->name, sizeof(intf->name), name, sizeof(name) - 1);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(intf); intf = NULL; DEV_MON_ERR("strncpy_s error\n"));
    intf->name[DM_INTF_NAME_LEN - 1] = '\0';

    if (my_addr == NULL) {
        addr = (DM_LOOP_ADDR_S *)&intf->my_addr;
        addr->addr_type = DM_LOOP_ADDR_TYPE;
        addr->channel = DM_LOOP_CHANNEL;
        addr->module_id = 0;
    } else {
        intf->my_addr = *my_addr;
    }

    ret = __selfloop_open(intf);
    DRV_CHECK_DO_SOMETHING(ret == 0, free(intf); intf = NULL; return ret);

    selfloop_init_intf(cb, intf);

    ret = dm_intf_register(cb, intf);
    if (ret != 0) {
        ret_t = mm_close_file(intf->rfd);
        DRV_CHECK_CHK(ret_t == 0);
        ret_t = mm_close_file(intf->wfd);
        DRV_CHECK_CHK(ret_t == 0);
        free(intf);
        intf = NULL;
        return ret;
    }

    if (my_intf != NULL) {
        *my_intf = intf;
    }

    return 0;
}
