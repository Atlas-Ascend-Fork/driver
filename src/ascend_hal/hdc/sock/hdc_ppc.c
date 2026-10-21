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
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#include "ascend_hal.h"
#include "hdc_cmn.h"
#include "stddef.h"
#include "securec.h"
#include "hdc_ppc.h"
#include "hdc_adapt.h"

#if defined(CFG_PLATFORM_EQUIP) && defined(CFG_SOC_PLATFORM_RC)
#define DRV_HDC_RC_USER_NUM 4
#define DRV_HDC_RC_USER_NAME_LEN 20
static struct {
    char user_name[DRV_HDC_RC_USER_NAME_LEN];
} hdc_rc_user_name_list[DRV_HDC_RC_USER_NUM] = {
    {"root"},
    {"HwHiAiUser"},
    {"HwDmUser"},
    {"HwBaseUser"},
};

STATIC bool drv_hdc_ppc_check_user(struct passwd *user)
{
    int i;

    for (i = 0; i < DRV_HDC_RC_USER_NUM; i++) {
        if (strcmp(user->pw_name, hdc_rc_user_name_list[i].user_name) == 0) {
            return true;
        }
    }

    return false;
}
#endif

STATIC void drv_hdc_socket_create_dir(void)
{
#ifdef CFG_SOC_PLATFORM_RC
    int ret;
    struct passwd *user = NULL;

    user = getpwuid(getuid());
    if (user == NULL) {
        HDC_LOG_ERR("Failed to get the current user.\n");
        return;
    }

    /* miniRC: /home/user/hdc_ppc or /root/hdc_ppc */
    if (strcmp(user->pw_name, "root") == 0) {
        ret = sprintf_s(g_ppc_dirs, sizeof(g_ppc_dirs), "%s%s%s", "/", user->pw_name, "/hdc_ppc/");
    } else {
        ret = sprintf_s(g_ppc_dirs, sizeof(g_ppc_dirs), "%s%s%s", "/home/", user->pw_name, "/hdc_ppc/");
    }

    if (ret < 0) {
        HDC_LOG_ERR("Failed to invoke sprintf_s for g_ppc_dirs.\n");
        return;
    }
#endif

    if (mmAccess(g_ppc_dirs) == -1) {
        hdc_create_dir_info_output();
#if defined(CFG_PLATFORM_EQUIP) && defined(CFG_SOC_PLATFORM_RC)
        if (drv_hdc_ppc_check_user(user)) {
            ret = mkdir((const char *)g_ppc_dirs, (mode_t)(S_IRWXU | S_IRGRP | S_IXGRP)); // 0750
            if (ret != 0) {
                HDC_LOG_ERR("Try to mkdir failed(result=%d; errno=%d).\n", ret, errno);
            }
        }
#endif
    }

    return;
}

STATIC void __attribute__((constructor)) drv_hdc_socket_init(void)
{
    drv_hdc_socket_create_dir();
}

STATIC drvError_t drv_hdc_socket_sock_path(struct sockaddr_un *addr, enum sock_type type, int dev_id,
                                           const signed int pid, signed int *path_len)
{
    const char *type_str = NULL;
    signed int len = 0;
    if ((addr == NULL) || (path_len == NULL)) {
#ifndef TMP_UT
        HDC_LOG_ERR("Input parameter addr or path_len is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
#endif
    }

    type_str = type == SOCK_CLIENT ? "sock_client_" : "sock_server_";

    if ((len = sprintf_s(addr->sun_path, sizeof(addr->sun_path), "%s%s%d_%d", g_ppc_dirs, type_str, pid, dev_id)) ==
        -1) {
#ifndef TMP_UT
        *path_len = 0;
        HDC_LOG_ERR("Call sprintf_s failed.\n");
        return DRV_ERROR_SOCKET_SET;
#endif
    }

    *path_len = (signed int)offsetof(struct sockaddr_un, sun_path) + len;
    return DRV_ERROR_NONE;
}

drvError_t drv_hdc_socket_session_connect(int dev_id, signed int server_pid, PPC_SESSION *session)
{
    struct hdc_client_session *pSession = NULL;
    signed int fd = -1;
    signed int len, ret;
    drvError_t rval;
    struct sockaddr_un srv_addr = {0};

    if (session == NULL) {
#ifndef TMP_UT
        HDC_LOG_ERR("Input parameter session is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
#endif
    }

    /* create a UNIX domain stream socket */
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
#ifndef TMP_UT
        HDC_LOG_ERR("Create ppc socket error. (strerror=\"%s\"; errno=%d)\n", strerror(errno), errno);
        return DRV_ERROR_SOCKET_CREATE;
#endif
    }

    /* fill socket address structure with server's address */
    srv_addr.sun_family = AF_UNIX;
    if (drv_hdc_socket_sock_path(&srv_addr, SOCK_SERVER, dev_id, server_pid, &len) != DRV_ERROR_NONE) {
        rval = DRV_ERROR_SOCKET_SET;
        goto errout;
    }

    do {
        ret = connect(fd, (struct sockaddr *)&srv_addr, (unsigned int)len);
    } while ((ret == -1) && (mm_get_error_code() == EINTR));

    if (ret < 0) {
        rval = DRV_ERROR_SOCKET_CONNECT;
        HDC_LOG_WARN("Connect ppc socket not success. (strerror=\"%s\"; errno=%d; server_pid=%d)\n", strerror(errno),
                     errno, server_pid);
        goto errout;
    }
#ifndef TMP_UT
    /* malloc client session */
    pSession = (struct hdc_client_session *)drv_hdc_zalloc(sizeof(struct hdc_client_session));
    if (pSession == NULL) {
        rval = DRV_ERROR_MALLOC_FAIL;
        HDC_LOG_ERR("Call malloc failed.\n");
        goto errout;
    }

    HDC_LOG_INFO("Ppc connect session. (session_fd=%d; pid=%d)\n", fd, getpid());
    pSession->session.magic = HDC_MAGIC_WORD;
    pSession->session.sockfd = fd;
    *session = (PPC_SESSION)pSession;
    return DRV_ERROR_NONE;
#endif

errout:
    (void)close(fd);
    return rval;
}

drvError_t drv_hdc_socket_server_create(int dev_id, signed int server_pid, PPC_SERVER *server)
{
    signed int len;
    drvError_t rval;
    signed int fd = -1;
    struct hdc_server_head *pHead = NULL;
    struct sockaddr_un srv_addr = {0};

    if (server == NULL) {
#ifndef TMP_UT
        HDC_LOG_ERR("Input parameter server is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
#endif
    }

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
#ifndef TMP_UT
        HDC_LOG_ERR("Create ppc socket error. (strerror=\"%s\"; errno=%d)\n", strerror(errno), errno);
        return DRV_ERROR_SOCKET_CREATE;
#endif
    }

    /* fill in socket address structure */
    srv_addr.sun_family = AF_UNIX;
    if (drv_hdc_socket_sock_path(&srv_addr, SOCK_SERVER, dev_id, server_pid, &len) != DRV_ERROR_NONE) {
#ifndef TMP_UT
        rval = DRV_ERROR_SOCKET_SET;
        goto errout;
#endif
    }
    (void)unlink(srv_addr.sun_path);

    /* bind the name to the descriptor */
    if (bind(fd, (struct sockaddr *)&srv_addr, (unsigned int)len) < 0) {
#ifndef TMP_UT
        rval = DRV_ERROR_SOCKET_BIND;
        HDC_LOG_ERR("Bind ppc socket error. (strerror=\"%s\"; errno=%d; server_pid=%d)\n", strerror(errno), errno,
                    server_pid);
        goto errout;
#endif
    }

    if (chmod(srv_addr.sun_path, PPC_FILE_PERMISSION_WRITE) != 0) {
        rval = DRV_ERROR_SOCKET_SET;
        HDC_LOG_ERR("Chmod ppc socket error. (strerror=\"%s\"; errno=%d; server_pid=%d)\n", strerror(errno), errno,
                    server_pid);
        goto errout;
    }

    if (listen(fd, QLEN) < 0) { /* tell kernel we're a server */
        rval = DRV_ERROR_SOCKET_LISTEN;
        HDC_LOG_ERR("Listen ppc socket error. (strerror=\"%s\"; errno=%d)\n", strerror(errno), errno);
        goto errout;
    }

#ifndef TMP_UT
    pHead = (struct hdc_server_head *)drv_hdc_zalloc(sizeof(struct hdc_server_head));
    if (pHead == NULL) {
        rval = DRV_ERROR_MALLOC_FAIL;
        HDC_LOG_ERR("Call malloc failed.\n");
        goto errout;
    }

    HDC_LOG_INFO("Ppc Server Create. (Fd=%d; pid=%d)\n", fd, server_pid);
    pHead->listenFd = fd;
    pHead->accept_wait = HDC_ACCEPT_NOT_WAITING;
    *server = (PPC_SERVER)pHead;
    return DRV_ERROR_NONE;
#endif

errout:
    (void)close(fd);
    return rval;
}

drvError_t drv_hdc_socket_session_accept(PPC_SERVER server, PPC_SESSION *session)
{
    struct hdc_server_head *pHead = NULL;
    struct hdc_server_session *pSession = NULL;
    signed int clifd = -1;
    signed int len;
    struct sockaddr_un un;

    if ((server == NULL) || (session == NULL)) {
#ifndef TMP_UT
        HDC_LOG_WARN("Input parameter server or session is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
#endif
    }
    pHead = (struct hdc_server_head *)server;
    len = (int)sizeof(un);
    pHead->accept_wait = HDC_ACCEPT_WAITING;

    do {
        clifd = accept(pHead->listenFd, (struct sockaddr *)&un, (socklen_t *)&len);
    } while ((clifd == -1) && (mm_get_error_code() == EINTR));

    if (clifd < 0) {
        if (pHead->listenFd == -1) {
#ifndef TMP_UT
            HDC_LOG_WARN("Server socket closed pHead\n");
            pHead->accept_wait = HDC_ACCEPT_NOT_WAITING;
            return DRV_ERROR_SOCKET_CLOSE;
#endif
        }
        HDC_LOG_ERR("Accept ppc socket error. (strerror=\"%s\"; errno=%d; listenFd=%d)\n", strerror(errno), errno,
                    pHead->listenFd);
        pHead->accept_wait = HDC_ACCEPT_NOT_WAITING;
        return DRV_ERROR_SOCKET_ACCEPT;
    }

#ifndef TMP_UT
    HDC_LOG_INFO("Ppc Accept Session. (clifd=%d; Server_fd=%d; pid=%d)\n", clifd, pHead->listenFd, getpid());
    pHead->accept_wait = HDC_ACCEPT_NOT_WAITING;

    pSession = (struct hdc_server_session *)malloc(sizeof(struct hdc_server_session));
    if (pSession == NULL) {
        HDC_LOG_ERR("Call malloc failed.\n");
        (void)close(clifd);
        clifd = -1;
        return DRV_ERROR_MALLOC_FAIL;
    }
    if (memset_s(pSession, sizeof(struct hdc_server_session), 0, sizeof(struct hdc_server_session)) != 0) {
        free(pSession);
        pSession = NULL;
        (void)close(clifd);
        clifd = -1;
        HDC_LOG_ERR("Call memset_s error. (strerror=\"%s\")\n", strerror(errno));
        return DRV_ERROR_INVALID_VALUE;
    }

    pSession->session.magic = HDC_MAGIC_WORD;
    pSession->session.sockfd = clifd;
    *session = (PPC_SESSION)pSession;

    return DRV_ERROR_NONE;
#endif
}
