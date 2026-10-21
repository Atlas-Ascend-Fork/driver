/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "dms_user_common.h"
#include "ascend_dev_num.h"
#include "dms_device_info.h"
#include "dms_p2p_com.h"
#include "pbl_uda_user.h"

#ifndef __linux
#pragma comment(lib, "libc_sec.lib")
#include "devdrv_manager_win.h"
#define PTHREAD_MUTEX_INITIALIZER NULL
#define FdIsValid(fd) fd != (mmProcess)DEVDRV_INVALID_FD_OR_INDEX
#else
#include <sys/prctl.h>
#include <errno.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define FdIsValid(fd) ((fd) >= 0)
#endif

#include "securec.h"
#include "mmpa_api.h"
#include "davinci_interface.h"
#include "dmc_user_interface.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devmng_common.h"
#include "devdrv_container.h"
#define DMS_DEVICE_FILE_NAME davinci_intf_get_dev_path()
#define DMS_INVALID_PID_OR_FD (-1)

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

static mmProcess g_dms_fd = (mmProcess)DMS_INVALID_PID_OR_FD;
static mmMutex_t g_dms_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pid_t g_dms_tgid = (pid_t)DMS_INVALID_PID_OR_FD;
static int g_env_virt = false;

int DmsGetVirtFlag(void)
{
    return g_env_virt;
}

int dms_set_virt_flag(int env_virt)
{
    return g_env_virt = env_virt;
}

STATIC void dms_ioctl_close(int fd)
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
    }
}

STATIC mmProcess dms_open_intf(void)
{
    mmProcess fd = DMS_INVALID_PID_OR_FD;
    int err = 0;
    int ret = 0;

    /* to improve performance */
    if (FdIsValid(g_dms_fd) && (g_dms_tgid == getpid())) {
        return g_dms_fd;
    }

#ifndef __linux
    if (g_dms_fd_mutex == NULL) {
        (void)mmMutexInit(&g_dms_fd_mutex);
    }
#endif
    (void)mmMutexLock(&g_dms_fd_mutex);
    if (FdIsValid(g_dms_fd)) {
        if (g_dms_tgid != getpid()) {
            g_dms_fd = (mmProcess)DMS_INVALID_PID_OR_FD;
        } else {
            fd = g_dms_fd;
            goto out;
        }
    }
#ifdef __linux
    fd = mmOpen2(DMS_DEVICE_FILE_NAME, M_RDWR | O_CLOEXEC, M_IRUSR);
    err = (__errno_location() != NULL ? errno : 0);
    ret = dms_ioctl_open(fd);
#else
    fd = devdrv_open_device_manager_win(&g_dms_fd);
#endif
    if (!FdIsValid(fd)) {
        DMS_ERR("Open failed. (dev_name=%s; fd=%d; g_dms_fd=%d; errno=%d)\n", DMS_DEVICE_FILE_NAME, fd, g_dms_fd, err);
        fd = (mmProcess)DMS_INVALID_PID_OR_FD;
        goto out;
    }
    if (ret != 0) {
        dms_ioctl_close(fd);
        (void)mmClose(fd);
        DMS_ERR("Dms ioctl open failed. (ret=%d; fd=%d)\n", ret, fd);
        fd = (mmProcess)DMS_INVALID_PID_OR_FD;
        goto out;
    }

    g_dms_fd = fd;
    g_dms_tgid = getpid();
out:
    (void)mmMutexUnLock(&g_dms_fd_mutex);
    return fd;
}

static void dms_close_intf(void)
{
    (void)mmMutexLock(&g_dms_fd_mutex);
    if (FdIsValid(g_dms_fd)) {
#if defined __linux
        if (g_dms_tgid == getpid()) {
            dms_ioctl_close(g_dms_fd);
            (void)mmClose(g_dms_fd);
        }
#endif
        g_dms_fd = (mmProcess)DMS_INVALID_PID_OR_FD;
        g_dms_tgid = (pid_t)DMS_INVALID_PID_OR_FD;
    }
    (void)mmMutexUnLock(&g_dms_fd_mutex);
}

static void __attribute__((constructor)) DmsInit(void)
{
    mmProcess fd = DMS_INVALID_PID_OR_FD;

    if (access(DMS_DEVICE_FILE_NAME, R_OK | W_OK) != 0) {
        return;
    }
    fd = dms_open_intf();
    if (!FdIsValid(fd)) {
        dms_close_intf();
        DMS_ERR("Open dms device failed. (fd=%d)\n", fd);
    }
}

static void __attribute__((destructor)) dms_un_init(void)
{
    dms_close_intf();
}

int DmsIoctl(int cmd, struct dms_ioctl_arg *ioarg)
{
    mmIoctlBuf ioctlBuf = {0};
    mmProcess fd;
    int ret;
    struct urd_ioctl_arg urd_ioarg = {0};

    if (ioarg == NULL) {
        DMS_ERR("Para ioarg is NULL\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (!dms_ioctl_cmd_check((unsigned long)cmd, ioarg->main_cmd, ioarg->sub_cmd, ioarg->filter)) {
        return DRV_ERROR_PERMISSION;
    }

    fd = dms_open_intf();
    if (!FdIsValid(fd)) {
        DMS_ERR("Open dms device failed. (fd=%d; cmd=%d)\n", fd, cmd);
        return DRV_ERROR_OPEN_FAILED;
    }
#ifndef CFG_FEATURE_SRIOV
    if (g_env_virt == true) {
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif
    /* ioarg translate urd_ioctl_arg */
    urd_ioarg.cmd.main_cmd = ioarg->main_cmd;
    urd_ioarg.cmd.sub_cmd = ioarg->sub_cmd;
    urd_ioarg.cmd.filter = ioarg->filter;
    urd_ioarg.cmd.filter_len = ioarg->filter_len;

    urd_ioarg.cmd_para.input = ioarg->input;
    urd_ioarg.cmd_para.input_len = ioarg->input_len;
    urd_ioarg.cmd_para.output = ioarg->output;
    urd_ioarg.cmd_para.output_len = ioarg->output_len;

    ioctlBuf.inbuf = (void *)&urd_ioarg;
    ioctlBuf.inbufLen = sizeof(struct urd_ioctl_arg);
    ioctlBuf.outbuf = ioctlBuf.inbuf;
    ioctlBuf.outbufLen = ioctlBuf.inbufLen;
    ioctlBuf.oa = NULL;
    ret = mmIoctl(fd, cmd, &ioctlBuf);
    if (ret < 0) {
        ret = (__errno_location() != NULL ? errno : EIO);
        share_log_read_err(HAL_MODULE_TYPE_DEV_MANAGER);
    }

    return ret;
}

static inline int is_valid_user_errno(int err)
{
    return ((err >= 0) && (err <= DRV_ERROR_POWER_OP_FAIL)) || (err == DRV_ERROR_NOT_SUPPORT);
}

/*
    |kernel return   | ioctl return | errno |
    |ret=[, -4096]   | ret          |       |
    |ret=[-4095, -1] | -1           | |ret| |
    |ret=[0, ]       | ret          |       |

    convert ioctl return/errno to userspace errno.
    support kernel return kernel errno(negative) or userspace errno.
*/
drvError_t ioctl_errno_convert(int ret, int errno_param)
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

drvError_t DmsIoctlConvertErrno(unsigned long cmd, struct urd_ioctl_arg *ioarg)
{
    mmProcess fd;
    drvError_t ret;

    if (ioarg == NULL) {
        DMS_ERR("ioctl arg is null. (cmd=%lu)\n", cmd);
        return DRV_ERROR_PARA_ERROR;
    }

    if (!dms_ioctl_cmd_check(cmd, ioarg->cmd.main_cmd, ioarg->cmd.sub_cmd, ioarg->cmd.filter)) {
        return DRV_ERROR_PERMISSION;
    }

    fd = dms_open_intf();
    if (!FdIsValid(fd)) {
        DMS_ERR("Open dms device failed. (fd=%d; cmd=%lu)\n", fd, cmd);
        return DRV_ERROR_OPEN_FAILED;
    }
#if !defined(CFG_FEATURE_SRIOV)
    if (g_env_virt == true) {
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif
    ret = ioctl(fd, cmd, ioarg);
    if (ret == 0) {
        return 0;
    }
    ret = ioctl_errno_convert(ret, errno);
    share_log_read_err(HAL_MODULE_TYPE_DEV_MANAGER);
    return ret;
}

int dmanage_check_module_init(const char *module_name)
{
    char *buff = NULL;
    FILE *fp = NULL;
    size_t name_len;
    int retry_times = -1;

    if (module_name == NULL) {
        DMS_ERR("para is NULL.\n");
        return -1;
    }
    name_len = strnlen(module_name, DEV_MODULE_INIT_INFO_LEN);
    if (name_len >= DEV_MODULE_INIT_INFO_LEN) {
        DMS_ERR("length out of range. (length=%d; max=%d)\n", name_len, DEV_MODULE_INIT_INFO_LEN);
        return -1;
    }
    buff = (char *)malloc(DEV_MODULE_INIT_INFO_LEN);
    if (buff == NULL) {
        DMS_ERR("malloc memory error. (size=%d)\n", DEV_MODULE_INIT_INFO_LEN);
        return -1;
    }

    do {
        fp = fopen(PROC_MOUDULE_FILE_NAME, "r");
        retry_times++;
    } while (fp == NULL && retry_times < MAX_FOPEN_RETRY_TIMES);

    if (fp == NULL) {
        DMS_ERR("fopen error. (file=\"%s\"; errno:%d, retry_times=%d.)\n", PROC_MOUDULE_FILE_NAME, errno, retry_times);
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

int dms_run_proc(const char **arg)
{
    unsigned int status;
    unsigned int status1;
    pid_t tftpchildpid;
    pid_t wait_ppid;
    char *envp[] = {0, NULL};

    if (arg == NULL) {
        DMS_ERR("arg is null.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }
    status = 0;
    tftpchildpid = vfork();
    if (tftpchildpid == 0) {
        (void)execve("/usr/bin/sudo", (void *)arg, envp);
        _exit(-1);
    }
    if (tftpchildpid == -1) {
        DMS_ERR("vfork fail.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    do {
        wait_ppid = waitpid(tftpchildpid, (int *)&status, 0);
    } while ((wait_ppid < 0) && (errno == EINTR));
    status1 = WIFEXITED(status);
    if (status1 == 0) {
        DMS_ERR("execve failed. (errno=%d; status1=%u)\n", errno, status1);
        return DRV_ERROR_INVALID_VALUE;
    }

    status1 = WEXITSTATUS(status);
    if (status1 != 0) {
        DMS_ERR("cmd run failed. (errno=%d; status1=%u)\n", errno, status1);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

int dms_run_proc_normal_user(char **arg)
{
    unsigned int status;
    unsigned int status1;
    pid_t tftpchildpid;
    pid_t wait_ppid;
    char *envp[] = {0, NULL};

    if (arg == NULL) {
        DMS_ERR("arg is null.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }
    DMS_ERR("!!! dms_run_proc_normal_user arg[0]=%s\n", arg[0]);
    status = 0;
    tftpchildpid = vfork();
    if (tftpchildpid == 0) {
        (void)execve(arg[0], arg, envp);
        _exit(-1);
    }
    if (tftpchildpid == -1) {
        DMS_ERR("vfork fail.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    do {
        wait_ppid = waitpid(tftpchildpid, (int *)&status, 0);
    } while ((wait_ppid < 0) && (errno == EINTR));
    status1 = WIFEXITED(status);
    if (status1 == 0) {
        DMS_ERR("execve failed. (errno=%d; status1=%u)\n", errno, status1);
        return DRV_ERROR_INVALID_VALUE;
    }

    status1 = WEXITSTATUS(status);
    if (status1 != 0) {
        DMS_ERR("cmd run failed. (errno=%d; status1=%u)\n", errno, status1);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t dmsCloseRestoreHandler(uint32_t devid, halDevCloseIn *in)
{
    UNUSED(devid);
    UNUSED(in);
    dms_close_intf();
    mmProcess fd = DMS_INVALID_PID_OR_FD;

    if (access(DMS_DEVICE_FILE_NAME, R_OK | W_OK) != 0) {
        return DRV_ERROR_NO_DEVICE;
    }
    fd = dms_open_intf();
    if (!FdIsValid(fd)) {
        dms_close_intf();
        DMS_ERR("Open dms device failed. (fd=%d)\n", fd);
        return DRV_ERROR_OPEN_FAILED;
    }
    int ret;
    ret = devdrv_close_restore_device_manager();
    if (ret != 0) {
        dms_close_intf();
        DMS_ERR("Close restore device manager failed.\n");
        return ret;
    }
    drvClearBareTgid();
    return ret;
}

static int dms_enable_p2p_per_type(u32 dev_id, u32 peer_phy_id)
{
    int ret;
    u32 remaining_cnt = 0, p2p_type;

    for (p2p_type = DMS_P2P_TYPE_MEM; p2p_type < DMS_P2P_TYPE_MAX; ++p2p_type) {
        (void)dms_get_p2p_restore_info(dev_id, peer_phy_id, p2p_type, &remaining_cnt);

        while (remaining_cnt > 0) {
            ret = DmsEnableP2P(dev_id, peer_phy_id, p2p_type);
            if (ret != 0) {
                DMS_ERR("p2p restore enable fail. (dev_id=%u; peer_phy_id=%u; p2p_type=%u; ret=%d)\n", dev_id,
                        peer_phy_id, p2p_type, ret);
                return ret;
            }
            remaining_cnt--;
        }
    }
    return DRV_ERROR_NONE;
}

drvError_t dms_res_enable_p2p(u32 dev_num)
{
    int ret;
    u32 dev_id, peer_phy_id;

    for (dev_id = 0; dev_id < dev_num; ++dev_id) {
        for (peer_phy_id = 0; peer_phy_id < ASCEND_PDEV_MAX_NUM; ++peer_phy_id) {
            ret = dms_enable_p2p_per_type(dev_id, peer_phy_id);
            if (ret != 0) {
                return ret;
            }
        }
    }
    DMS_INFO("p2p restore success.\n");

    return DRV_ERROR_NONE;
}

drvError_t dmsProcResRestoreHandler(halProcResRestoreInfo *info)
{
    int ret;
    (void)info;
    u32 dev_num;

    ret = drvGetDevNum(&dev_num);
    if (ret != 0) {
        DMS_ERR("Get dev num fail. (ret=%d)\n", ret);
        return ret;
    }

    dms_set_p2p_restore_info_flag(DMS_RES_FLAG_DISABLE);
    ret = dms_res_enable_p2p(dev_num);
    dms_set_p2p_restore_info_flag(DMS_RES_FLAG_ENABLE);

    return ret;
}

void hal_report_not_support(const char *func_name, const char *reason)
{
    const char *keys[] = {"func_name", "reason"};
    const char *values[] = {func_name, reason};

    REPORT_PREDEFINED_ERR_MSG("EL0021", keys, values, 2UL);
}

void hal_report_device_occupied(uint32_t devId)
{
    int ret = 0;
    char dev_id_str[16] = {0};
    const char *keys[] = {"deviceid"};
    const char *values[] = {dev_id_str};

    ret = snprintf_s(dev_id_str, sizeof(dev_id_str), sizeof(dev_id_str) - 1, "%u", devId);
    if (ret < 0) {
        return;
    }
    REPORT_PREDEFINED_ERR_MSG("EL0020", keys, values, 1UL);
}

void hal_report_invalid_dev_id(const char *func_name, uint32_t devId, uint32_t max_devid)
{
    int ret = 0;
    char dev_id_str[16] = {0};
    char reason[128] = {0};
    const char *keys[] = {"func_name", "para_value", "para_name", "reason"};
    const char *values[] = {func_name, dev_id_str, "devId", reason};

    ret = snprintf_s(dev_id_str, sizeof(dev_id_str), sizeof(dev_id_str) - 1, "%u", devId);
    if (ret < 0) {
        return;
    }

    ret = snprintf_s(reason, sizeof(reason), sizeof(reason) - 1,
                     "The parameter value is out of range. The valid range is [0, %u]", max_devid);
    if (ret < 0) {
        return;
    }

    REPORT_PREDEFINED_ERR_MSG("EL0016", keys, values, 4UL);
}
