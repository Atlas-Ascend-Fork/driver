/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include "securec.h"

#include "dev_mon_log.h"
#include "mmpa_api.h"
#include "dm_udp.h"
#include "drvdmp_adapt.h"
#include "drv_base_adapt.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define DM_UDP_MSG_RETRY_TIMES 3000 /* timeout 30s */
#define DM_UDP_MSG_RETRY_ONCE 10000 /* 10ms */
#define DM_TIMEOUT_TEN_TIMES 10

typedef union tag_DMP_CMSGHDR_U {
    struct cmsghdr cmh;
    char control[CMSG_SPACE(sizeof(struct ucred)) + CMSG_SPACE(sizeof(int))];
} DMP_CMSGHDR_U;

STATIC void __format_udp_msg(UDP_MSG_T *udp_msg, DM_MSG_TYPE msg_type, const DM_UDP_ADDR_ST *dst_addr,
                             const DM_UDP_ADDR_ST *src_addr, const DM_MSG_ST *msg, signed long msgid)
{
    int ret;
    (void)src_addr;

    ret = memset_s(udp_msg, sizeof(*udp_msg), 0, sizeof(*udp_msg));
    DRV_CHECK_RET_DO_SOMETHING((ret == 0), DEV_MON_ERR("memset_s error\n"));
    udp_msg->type = UDP_MSG_TYPE;
    udp_msg->version = 1;
    udp_msg->msg_type = msg_type;
    udp_msg->sequence = (unsigned long)msgid;
    udp_msg->dev_id = dst_addr->dev_id;

    /* copy the user data */
    if (msg->data_len > 0) {
        ret = memmove_s(&(udp_msg->data[0]), msg->data_len, msg->data, msg->data_len);
        DRV_CHECK_RET_DO_SOMETHING((ret == 0), DEV_MON_ERR("memmove_s error\n"));
    }

    udp_msg->data_len = msg->data_len;
    return;
}

STATIC int __dm_send_msg(int sockfd, void *buf, unsigned int buflen, struct sockaddr_un *to, socklen_t tolen)
{
    struct msghdr msg;
    struct iovec iov[1];
    int ret, err_buf;
    int timeout = 0;

    ret = memset_s(&msg, sizeof(msg), 0, sizeof(msg));
    if (ret != 0) {
        DEV_MON_ERR("memset_s error:%s.\n", strerror(errno));
        return ENODATA;
    }

    msg.msg_name = to;
    msg.msg_namelen = tolen;
    msg.msg_iov = iov;
    msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);
    iov[0].iov_base = buf;
    iov[0].iov_len = buflen;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    ret = (int)sendmsg(sockfd, &msg, 0);
    err_buf = errno;
    while ((ret == -1) && ((err_buf == EINTR) || (err_buf == EAGAIN))) {
        if ((++timeout) > DM_UDP_MSG_RETRY_TIMES) {
            break;
        }
        (void)usleep(DM_UDP_MSG_RETRY_ONCE);
        ret = (int)sendmsg(sockfd, &msg, 0);
        err_buf = errno;
    }

    if (ret == -1) {
        DEV_MON_ERR("sendmsg fail. (errno=%d; timeout=%dms)\n", err_buf, timeout * DM_TIMEOUT_TEN_TIMES);
        return err_buf;
    }

    return 0;
}

STATIC int __dm_udp_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, DM_ADDR_ST *addr, unsigned int addr_len,
                         const DM_MSG_ST *msg, signed long msgid)
{
    DM_UDP_ADDR_ST *to_addr = NULL;
    DM_UDP_ADDR_ST *my_addr = NULL;
    UDP_MSG_T *req = NULL;
    int ret;
    int send_addr_len;

    if (msg->data_len > DM_MSG_DATA_MAX) {
        DEV_MON_ERR("__dm_udp_send: msg->data_len(%d) exceed allow size(%d)\n", msg->data_len, DM_MSG_DATA_MAX);
        return -EINVAL;
    }

    req = (UDP_MSG_T *)malloc(sizeof(UDP_MSG_T));
    DRV_CHECK_RETV((req != NULL), -ENOMEM);
    ret = memset_s(req, sizeof(UDP_MSG_T), 0, (sizeof(UDP_MSG_T)));
    if (ret != 0) {
        free(req);
        req = NULL;
        DEV_MON_ERR("__dm_udp_send: memset_s fail.errno=%d\n", errno);
        return -EINVAL;
    }

    to_addr = (DM_UDP_ADDR_ST *)addr;
    my_addr = (DM_UDP_ADDR_ST *)&intf->my_addr;
    __format_udp_msg(req, msg_type, to_addr, my_addr, msg, msgid);

    if (my_addr->service_type == DM_CLIENT) {
        send_addr_len = sizeof(to_addr->sock_addr);
    } else {
        send_addr_len = (int)addr_len - (int)(sizeof(*to_addr) - sizeof(to_addr->sock_addr));
    }

    ret = __dm_send_msg(intf->wfd, req, sizeof(UDP_MSG_T), (void *)&to_addr->sock_addr, (socklen_t)send_addr_len);
    if (ret != 0) {
        DEV_MON_ERR("__dm_send_msg: sendto failed. (ret=%d)\r\n", ret);
    }

    free(req);
    req = NULL;
    return ret;
}

STATIC int __dm_udp_settime_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, DM_ADDR_ST *addr, unsigned int addr_len,
                                 const DM_MSG_ST *msg, int retries, unsigned int retry_time_ms, signed long msgid)
{
    (void)retries;
    (void)retry_time_ms;
    return __dm_udp_send(intf, msg_type, addr, addr_len, msg, msgid);
}

STATIC int __dm_udp_recv_set_irecv_data(UDP_MSG_T *msg, DM_RECV_ST *irecv, struct sockaddr_un from, socklen_t fromlen,
                                        struct ucred *cred)
{
    DM_UDP_ADDR_ST *addr = NULL;
    int err_buf;
    int ret;
    (void)cred;

    /* not to check the length of data */
    irecv->recv_type = msg->msg_type;

    /* get source address */
    addr = (DM_UDP_ADDR_ST *)irecv->addr;
    addr->addr_type = DM_UDP_ADDR_TYPE;
    addr->channel = DM_UDP_CHANNEL;
    addr->sock_addr = from;
    irecv->addr_len = (unsigned int)(sizeof(*addr) - sizeof(addr->sock_addr)) + fromlen;
    irecv->dev_id = (int)msg->dev_id;

    /* get message */
    irecv->msgid = (long)msg->sequence;
    irecv->msg.data_len = msg->data_len;
#ifdef __DEBUG_
    DEV_MON_INFO("recv data:\n");

    for (i = 0; i < irecv->msg.data_len; i++) {
        DEV_MON_INFO("%02x ", msg->data[i]);
    }

    DEV_MON_INFO("\n");
#endif
#ifndef DEV_MON_UTCASE__

    /* copy the src data to recv msg */
    if ((irecv->msg.data_len <= MAX_UDP_MSG_BUF) && (msg->data_len <= MAX_UDP_MSG_BUF) &&
        (irecv->msg.data_len >= msg->data_len)) {
        ret = memcpy_s(irecv->msg.data, irecv->msg.data_len, msg->data, msg->data_len);
        err_buf = errno;
        DRV_CHECK_RETV_DO_SOMETHING((ret == 0), err_buf, DEV_MON_ERR("memcpy_s error\n"));
    } else {
        DEV_MON_ERR("Invalid buffer size. (irecv->msg.data_len=%hu; msg->data_len=%hu)\n", irecv->msg.data_len,
                    msg->data_len);
        return -EINVAL;
    }

#endif
    return 0;
}

STATIC int __dm_recv_msg_with_cred(int sockfd, struct ucred *cred, void *buf, unsigned int buflen,
                                   struct sockaddr_un *from, socklen_t *fromlen)
{
    struct cmsghdr *cmsg = NULL;
    struct msghdr msg = {0};
    struct iovec iov[1];
    int ret, err_buf;
    int timeout = 0;
    DMP_CMSGHDR_U control_un;

    msg.msg_name = from;
    msg.msg_namelen = fromlen ? *fromlen : 0;

    msg.msg_iov = iov;
    msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);
    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    iov[0].iov_base = buf;
    iov[0].iov_len = buflen;

    ret = (int)recvmsg(sockfd, &msg, 0);
    err_buf = errno;
    while ((ret == -1) && (err_buf == EINTR)) {
        if ((++timeout) > DM_UDP_MSG_RETRY_TIMES) {
            break;
        }
        (void)usleep(DM_UDP_MSG_RETRY_ONCE);
        ret = (int)recvmsg(sockfd, &msg, 0);
        err_buf = errno;
    }

    if (ret == -1) {
        DEV_MON_ERR("recvmsg fail. (errno=%d, timeout=%dms)\n", err_buf, timeout * DM_TIMEOUT_TEN_TIMES);
        return err_buf;
    }

    if (fromlen != NULL) {
        *fromlen = msg.msg_namelen;
    }
    ret = memset_s(cred, sizeof(*cred), 0, sizeof(*cred));
    if (ret != 0) {
        DEV_MON_ERR("memset_s error:%s.\n", strerror(errno));
        return ENODATA;
    }
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET) {
            continue;
        }

        switch (cmsg->cmsg_type) {
            case SCM_CREDENTIALS:
                *cred = *(struct ucred *)CMSG_DATA(cmsg);
                break;
            default:
                break;
        }
    }

    return 0;
}

STATIC int __dm_udp_recv(DM_INTF_S *intf, int fd, short revents, DM_RECV_ST *irecv)
{
    UDP_MSG_T *msg = NULL;
    int ret;
    struct ucred cred = {0};
    struct sockaddr_un from = {0};
    socklen_t from_len = sizeof(struct sockaddr_un);
    unsigned short tmp_event = (unsigned short)revents;

    msg = (UDP_MSG_T *)malloc(sizeof(UDP_MSG_T));
    DRV_CHECK_RETV((msg != NULL), -ENOMEM);
    ret = memset_s(msg, sizeof(UDP_MSG_T), 0, sizeof(UDP_MSG_T));
    if (ret != 0) {
        free(msg);
        msg = NULL;
        DEV_MON_ERR("__dm_udp_recv: memset_s fail.errno=%d\n", errno);
        return -EINVAL;
    }

    if ((tmp_event & POLLIN) != 0) {
        ret = __dm_recv_msg_with_cred(fd, &cred, msg, sizeof(UDP_MSG_T), &from, &from_len);
        if (ret != 0) {
            DEV_MON_ERR("__dm_recv_msg_with_cred: recvfrom fail.errno=%d\r\n", ret);
            goto out;
        }

        if ((msg->version != UDP_MSG_VERSION) || (msg->type != UDP_MSG_TYPE)) {
            ret = -1;
            goto out;
        }

        ret = __dm_udp_recv_set_irecv_data(msg, irecv, from, from_len, &cred);
        if (ret != 0) {
            DEV_MON_ERR("init irecv failed. (ret=%d)\n", ret);
            goto out;
        }

        __dm_udp_recv_set_irecv_authority(intf, irecv);
    }

out:
    free(msg);
    msg = NULL;
    return ret;
}

STATIC int __dm_udp_set_retries(DM_INTF_S *intf, int retries, unsigned int retry_time_ms)
{
    /* intf need not retry, openipmi driver will accomplish the task */
    intf->retries = retries;
    intf->retry_time_ms = retry_time_ms;
    return 0;
}

STATIC int __dm_udp_get_retries(DM_INTF_S *intf, int *retries, unsigned int *retry_time_ms)
{
    /* intf need not retry, openipmi driver will accomplish the task */
    *retries = intf->retries;
    *retry_time_ms = intf->retry_time_ms;
    return 0;
}

STATIC int __dm_udp_open(DM_INTF_S *intf)
{
    DM_UDP_ADDR_ST *udp_addr = (DM_UDP_ADDR_ST *)&intf->my_addr;
    socklen_t addrlen;
    int sockfd = -1;
    int err_buf;
    int ret;

    sockfd = mmSocket(AF_LOCAL, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        ret = errno;
        return ret;
    }

#ifndef DEV_MON_UT
    if ((fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) || (fcntl(sockfd, F_SETFD, FD_CLOEXEC) < 0)) {
        err_buf = errno;
        (void)close(sockfd);
        sockfd = -1;
        DEV_MON_ERR("failed %d\n", err_buf);
        return err_buf;
    }

#endif

    if (udp_addr->service_type == DM_SERVER) {
        addrlen = sizeof(udp_addr->sock_addr);
    } else {
        addrlen = sizeof(short);
    }

    ret = bind(sockfd, (const struct sockaddr *)&udp_addr->sock_addr, addrlen);
    if (ret != 0) {
        err_buf = errno;
        (void)close(sockfd);
        sockfd = -1;
        DEV_MON_ERR("failed %d\n", err_buf);
        return err_buf;
    }

    intf->rfd = sockfd;
    intf->wfd = sockfd;
    return 0;
}

STATIC void __dm_udp_close(DM_INTF_S *intf)
{
    int ret_management, ret_service;

    ret_management = strcmp(intf->name, DM_UDP_MANAGEMENT_INTF);
    ret_service = strcmp(intf->name, DM_UDP_SERVICE_INTF);
    if ((ret_management != 0) && (ret_service != 0)) {
        return;
    }

    (void)close(intf->rfd);
    intf->rfd = -1;
    return;
}

STATIC int dm_udp_init_check_para(DM_CB_S *cb, const DM_ADDR_ST *my_addr, const char *my_name, int name_len)
{
    if (cb == NULL) {
        return EINVAL;
    }

    if ((my_addr == NULL) || (my_addr->addr_type != DM_UDP_ADDR_TYPE) || (my_addr->channel != DM_UDP_CHANNEL)) {
        return EINVAL;
    }

    if ((my_name != NULL) && (name_len >= DM_INTF_NAME_LEN)) {
        return EINVAL;
    }
    return 0;
}

static int dm_udp_init_intf(DM_INTF_S **intf, DM_MSG_TIMEOUT_HNDL_T timeout_hndl, DM_CB_S *cb,
                            const DM_ADDR_ST *my_addr, const char *name, int name_len)
{
    int ret;

    if (name_len > DM_INTF_NAME_LEN) {
        DEV_MON_ERR("name len %d is larger than %d\n", name_len, DM_INTF_NAME_LEN);
        return EINVAL;
    }

    *intf = malloc(sizeof(DM_INTF_S));
    if ((*intf) == NULL) {
        return ENOMEM;
    }

    ret = memset_s((void *)(*intf), sizeof(DM_INTF_S), 0, sizeof(DM_INTF_S));
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail: %d\n", ret);
        free((*intf));
        *intf = NULL;
        return ret;
    }

    ret = strncpy_s((*intf)->name, sizeof((*intf)->name), name, sizeof((*intf)->name) - 1);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(*intf); *intf = NULL; DEV_MON_ERR("strncpy_s error\n"));
    (*intf)->name[DM_INTF_NAME_LEN - 1] = '\0';
    (*intf)->my_addr = *my_addr;
    ret = __dm_udp_open(*intf);
    if (ret != 0) {
        free(*intf);
        *intf = NULL;
        return ret;
    }

    (*intf)->retries = DM_UDP_MAX_RETRY_TIMES;
    (*intf)->retry_time_ms = DM_UDP_MAX_TETRY_TIME_MS;
    (*intf)->max_trans_len = DM_UDP_MAX_TRANS_LEN;
    (*intf)->recv_msg = __dm_udp_recv;
    (*intf)->send_msg = __dm_udp_send;
    (*intf)->send_msg_settime = __dm_udp_settime_send;
    (*intf)->set_retries = __dm_udp_set_retries;
    (*intf)->get_retries = __dm_udp_get_retries;
    (*intf)->close = __dm_udp_close;
    (*intf)->timeout_hndl = timeout_hndl;
    (*intf)->dm_cb = (void *)cb;
    return 0;
}

int dm_udp_init(DM_INTF_S **my_intf, DM_CB_S *cb, DM_MSG_TIMEOUT_HNDL_T timeout_hndl, const DM_ADDR_ST *my_addr,
                const char *my_name, int name_len)
{
    DM_INTF_S *intf = NULL;
    char name[sizeof(intf->name)] = {0};
    int ret;

    ret = dm_udp_init_check_para(cb, my_addr, my_name, name_len);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, DEV_MON_ERR("udp init fail ret = %d\n", ret));

    ret = strncpy_s(name, sizeof(intf->name), (const char *)(my_name ? my_name : DM_UDP_INTF), sizeof(name) - 1);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, DEV_MON_ERR("strncpy_s error\n"));
    name[sizeof(name) - 1] = 0;
    intf = dm_get_intf(cb, name, (int)strlen(name));
    if (intf != NULL) {
        DEV_MON_ERR("duplicate interface [%s]\r\n", name);
        return EBUSY;
    }

    ret = dm_udp_init_intf(&intf, timeout_hndl, cb, my_addr, name, sizeof(name));
    if (ret != 0) {
        DEV_MON_ERR("init intf failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = dm_intf_register(cb, intf);
    if (ret != 0) {
        __dm_udp_close(intf);
        free(intf);
        intf = NULL;
        return ret;
    }

    if (my_intf != NULL) {
        *my_intf = intf;
    }

    return 0;
}
