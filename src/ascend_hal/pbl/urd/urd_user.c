/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include "securec.h"
#include "mmpa_api.h"
#include "davinci_interface.h"
#include "dmc_user_interface.h"
#include "urd_cmd.h"
#include "urd_user_inner.h"
#include "pbl/pbl_urd_user.h"

#define URD_INVALID_PID_OR_FD (-1)
#define FdIsValid(fd) ((fd) >= 0)

#define PROC_MOUDULE_FILE_NAME "/proc/modules"
#define DEV_MODULE_INIT_INFO_LEN 1024
#define MAX_FOPEN_RETRY_TIMES 5

static mmProcess g_urd_fd = (mmProcess)URD_INVALID_PID_OR_FD;
static mmMutex_t g_urd_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pid_t g_urd_tgid = (pid_t)URD_INVALID_PID_OR_FD;
static int g_env_virt = false;

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#ifdef CFG_FEATURE_MODULE_CHECK
STATIC int urd_check_module_init(const char *module_name)
{
    char *buff = NULL;
    FILE *fp = NULL;
    size_t name_len;
    int retry_times = -1;

    if (module_name == NULL) {
        urd_err("Para is NULL.\n");
        return -1;
    }
    name_len = strnlen(module_name, DEV_MODULE_INIT_INFO_LEN);
    if (name_len >= DEV_MODULE_INIT_INFO_LEN) {
        urd_err("Length out of range. (length=%zu)\n", name_len);
        return -1;
    }
    buff = (char *)malloc(DEV_MODULE_INIT_INFO_LEN);
    if (buff == NULL) {
        urd_err("Malloc memory error. (size=%d)\n", DEV_MODULE_INIT_INFO_LEN);
        return -1;
    }

    do {
        fp = fopen(PROC_MOUDULE_FILE_NAME, "r");
        retry_times++;
    } while (fp == NULL && retry_times < MAX_FOPEN_RETRY_TIMES);

    if (fp == NULL) {
        urd_err("Fopen error. (file=\"%s\"; errno:%d, retry_time=%d)\n", PROC_MOUDULE_FILE_NAME, errno, retry_times);
        (void)free(buff);
        buff = NULL;
        return -1;
    }
    while (fgets(buff, DEV_MODULE_INIT_INFO_LEN, fp) != NULL) {
        if (strstr(buff, module_name) != NULL) {
            (void)free(buff);
            (void)fclose(fp);
            buff = NULL;
            fp = NULL;
            return 0;
        }
    }

    (void)free(buff);
    (void)fclose(fp);
    buff = NULL;
    fp = NULL;
    return -1;
}
#endif

STATIC int urd_ioctl_open(int fd)
{
    struct davinci_intf_open_arg arg = {0};
    int ret;

    if (!FdIsValid(fd)) {
        return DRV_ERROR_INVALID_VALUE;
    }
#ifdef CFG_FEATURE_MODULE_CHECK
    if (urd_check_module_init("asdrv_vdms") == 0) {
        g_env_virt = true;
        return 0;
    }
#endif
    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_INTF_MODULE_URD);
    if (ret != 0) {
        urd_err("Strcpy_s failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = ioctl(fd, DAVINCI_INTF_IOCTL_OPEN, &arg);
    if (ret != 0) {
        urd_err("Call devdrv_device ioctl open failed. (ret=%d; errno=%d)\n", ret, errno);
        return ret;
    }

    return 0;
}

STATIC void urd_ioctl_close(int fd)
{
    struct davinci_intf_close_arg arg = {0};
    int ret;

    if ((!FdIsValid(fd)) || (g_env_virt == true)) {
        return;
    }

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_INTF_MODULE_URD);
    if (ret != 0) {
        return;
    }

    ret = ioctl(fd, DAVINCI_INTF_IOCTL_CLOSE, &arg);
    if (ret != 0) {
        return;
    }
    return;
}

STATIC mmProcess urd_open_intf(void)
{
    mmProcess fd = URD_INVALID_PID_OR_FD;
    int err = 0;
    int ret = 0;

    /* to improve performance */
    if (FdIsValid(g_urd_fd) && (g_urd_tgid == getpid())) {
        return g_urd_fd;
    }

    (void)mmMutexLock(&g_urd_fd_mutex);
    if (FdIsValid(g_urd_fd)) {
        if (g_urd_tgid != getpid()) {
            g_urd_fd = (mmProcess)URD_INVALID_PID_OR_FD;
        } else {
            fd = g_urd_fd;
            goto out;
        }
    }

    fd = mmOpen2(davinci_intf_get_dev_path(), M_RDWR | O_CLOEXEC, M_IRUSR);
    err = (__errno_location() != NULL ? errno : 0);
    if (!FdIsValid(fd)) {
        urd_err("Open failed. (dev_name=%s; fd=%d; g_urd_fd=%d; errno=%d)\n", davinci_intf_get_dev_path(), fd, g_urd_fd,
                err);
        fd = (mmProcess)URD_INVALID_PID_OR_FD;
        goto out;
    }
    ret = urd_ioctl_open(fd);
    if (ret != 0) {
        urd_ioctl_close(fd);
        (void)mmClose(fd);
        urd_err("Urd ioctl open failed. (ret=%d; fd=%d)\n", ret, fd);
        fd = (mmProcess)URD_INVALID_PID_OR_FD;
        goto out;
    }

    g_urd_fd = fd;
    g_urd_tgid = getpid();
out:
    (void)mmMutexUnLock(&g_urd_fd_mutex);
    return fd;
}

STATIC void urd_close_intf(void)
{
    (void)mmMutexLock(&g_urd_fd_mutex);
    if (FdIsValid(g_urd_fd)) {
        if (g_urd_tgid == getpid()) {
            urd_ioctl_close(g_urd_fd);
            (void)mmClose(g_urd_fd);
        }
        g_urd_fd = (mmProcess)URD_INVALID_PID_OR_FD;
        g_urd_tgid = (pid_t)URD_INVALID_PID_OR_FD;
    }
    (void)mmMutexUnLock(&g_urd_fd_mutex);
}

STATIC void __attribute__((constructor)) urd_user_init(void)
{
    mmProcess fd = URD_INVALID_PID_OR_FD;

    if (access(davinci_intf_get_dev_path(), R_OK | W_OK) != 0) {
        return;
    }
    fd = urd_open_intf();
    if (!FdIsValid(fd)) {
        urd_close_intf();
        urd_err("Open dms device failed. (fd=%d)\n", fd);
    }
}

STATIC void __attribute__((destructor)) urd_user_uninit(void)
{
    urd_close_intf();
}

STATIC inline int is_valid_user_errno(int err)
{
    return ((err >= 0) && (err <= DRV_ERROR_CONNECT)) || (err == DRV_ERROR_NOT_SUPPORT);
}

/*
    |kernel return   | ioctl return | errno |
    |ret=[, -4096]   | ret          |       |
    |ret=[-4095, -1] | -1           | |ret| |
    |ret=[0, ]       | ret          |       |

    convert ioctl return/errno to userspace errno.
    support kernel return kernel errno(negative) or userspace errno.
*/
STATIC drvError_t urd_ioctl_errno_convert(int ret, int errno_param)
{
    /* kernel return [-4095, -1] */
    if (ret == -1) { /* only if ret is -1, errno is set */
        return errno_to_user_errno(errno_param);
    }
    /* kernel return <= -4096 or >= 0 */
    if (is_valid_user_errno(ret)) {
        return ret;
    }
    return DRV_ERROR_IOCRL_FAIL;
}

STATIC drvError_t urd_ioctl(unsigned long cmd, struct urd_ioctl_arg *ioarg)
{
    mmProcess fd;
    drvError_t ret;

    if (ioarg == NULL) {
        urd_err("Ioctl arg is null\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (!urd_ioctl_cmd_check(cmd, ioarg->cmd.main_cmd, ioarg->cmd.sub_cmd, ioarg->cmd.filter)) {
        return DRV_ERROR_PERMISSION;
    }

    fd = urd_open_intf();
    if (!FdIsValid(fd)) {
        urd_err("Open device failed. (fd=%d)\n", fd);
        return DRV_ERROR_OPEN_FAILED;
    }

#ifndef CFG_FEATURE_SRIOV
    if (g_env_virt == true) {
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif
    ret = ioctl(fd, cmd, ioarg);
    if (ret == 0) {
        return 0;
    }
    ret = urd_ioctl_errno_convert(ret, errno);
    share_log_read_err(HAL_MODULE_TYPE_DEV_MANAGER);
    return ret;
}

int urd_dev_usr_cmd(uint32_t devid, struct urd_cmd *cmd, struct urd_cmd_para *cmd_para)
{
    return urd_dev_usr_cmd_ex(devid, URD_IOCTL_CMD, cmd, cmd_para);
}

int urd_dev_usr_cmd_ex(uint32_t devid, unsigned long ioctl_cmd, struct urd_cmd *cmd, struct urd_cmd_para *cmd_para)
{
    struct urd_ioctl_arg ioarg = {0};
    int ret;

    if ((cmd == NULL) || (cmd_para == NULL)) {
        urd_err("Invalid cmd_para or cmd is NULL. (devid=%u, cmd=%s, cmd_para=%s)\n", devid,
                (cmd == NULL) ? "NULL" : "OK", (cmd_para == NULL) ? "NULL" : "OK");
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.devid = devid;
    ret = memcpy_s(&(ioarg.cmd), sizeof(ioarg.cmd), cmd, sizeof(*cmd));
    if (ret != 0) {
        urd_err("Memcpy fail. (ret=%d; devid=%u)\n", ret, devid);
        return DRV_ERROR_INNER_ERR;
    }

    ret = memcpy_s(&(ioarg.cmd_para), sizeof(ioarg.cmd_para), cmd_para, sizeof(*cmd_para));
    if (ret != 0) {
        urd_err("Memcpy fail. (ret=%d; devid=%u)\n", ret, devid);
        return DRV_ERROR_INNER_ERR;
    }

    ret = urd_ioctl(ioctl_cmd, &ioarg);
    if (ret != 0) {
        urd_ex_err(ret, "Ioctl fail. (ret=%d; devid=%u; main_cmd=0x%x; sub_cmd=0x%x)\n", ret, devid, cmd->main_cmd,
                   cmd->sub_cmd);
        return ret;
    }
    return 0;
}

int urd_usr_cmd_ex(unsigned long ioctl_cmd, struct urd_cmd *cmd, struct urd_cmd_para *cmd_para)
{
    /* 0: no used */
    return urd_dev_usr_cmd_ex(0, ioctl_cmd, cmd, cmd_para);
}

int urd_usr_cmd(struct urd_cmd *cmd, struct urd_cmd_para *cmd_para)
{
    /* 0: no used */
    return urd_dev_usr_cmd(0, cmd, cmd_para);
}
#ifndef DMS_UT
drvError_t urdCloseRestoreHandler(uint32_t devid, halDevCloseIn *in)
{
    UNUSED(devid);
    UNUSED(in);
    urd_close_intf();
    mmProcess fd = URD_INVALID_PID_OR_FD;

    if (access(davinci_intf_get_dev_path(), R_OK | W_OK) != 0) {
        return DRV_ERROR_NO_DEVICE;
    }
    fd = urd_open_intf();
    if (!FdIsValid(fd)) {
        urd_close_intf();
        urd_err("Open URD(User Request Distribute) failed. (fd=%d)\n", fd);
        return DRV_ERROR_OPEN_FAILED;
    }
    return DRV_ERROR_NONE;
}
#endif