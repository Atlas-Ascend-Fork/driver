/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <unistd.h>
#include "securec.h"
#include "mmpa_api.h"
#include "devmng_common.h"
#include "devmng_user_common.h"
#include "devdrv_ioctl.h"
#include "dms/dms_devdrv_info_comm.h"
#include "davinci_interface.h"
#include "errno.h"

#ifndef __linux
#pragma comment(lib, "libc_sec.lib")
#define PTHREAD_MUTEX_INITIALIZER NULL
#define fd_is_invalid(fd) (fd == (mmProcess)DEVDRV_INVALID_FD_OR_INDEX)
#else
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#define fd_is_invalid(fd) ((fd) < 0)
#endif

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

STATIC mmProcess devdrv_manager_fd = (mmProcess)DEVDRV_INVALID_FD_OR_INDEX;
STATIC mmMutex_t devdrv_manager_fd_mutex = PTHREAD_MUTEX_INITIALIZER;

#define DEVDRV_INVALID_TGID (-1)
STATIC pid_t devdrv_manager_tgid = (pid_t)DEVDRV_INVALID_TGID;

#define DAVINCI_INTF_MODULE_DEVMNG "DEVMNG"
#define DAVINCI_INTF_INVALID_STATUS (-1)
#define PCIE_NUM (3)
#define INVALID_VF_DEVICE_ID 0xFFFFFFFF

void devdrv_close_device_manager(void);

STATIC int devmng_ioctl_open(int fd)
{
    struct davinci_intf_close_arg arg = {0};
    int status = false;
    int ret;

    ret = drvGetCommonDriverInitStatus(&status);
    if (ret != 0) {
        DEVDRV_DRV_ERR("drvGetCommonDriverInitStatus failed, ret(%d).\n", ret);
        return ret;
    }

    if (status == false) {
        return 0;
    }

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_INTF_MODULE_DEVMNG);
    if (ret < 0) {
        DEVDRV_DRV_ERR("strcpy_s failed, ret(%d).\n", ret);
        return ret;
    }

    ret = ioctl(fd, DAVINCI_INTF_IOCTL_OPEN, &arg);
    if (ret != 0) {
        DEVDRV_DRV_ERR("call devdrv_device ioctl open failed, ret(%d).\n", ret);
        return ret;
    }

    return 0;
}

STATIC void devmng_ioctl_close(int fd)
{
    struct davinci_intf_close_arg arg = {0};
    int status = false;
    int ret;

    ret = drvGetCommonDriverInitStatus(&status);
    if (ret != 0) {
        return;
    }

    if (status == false) {
        return;
    }

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_INTF_MODULE_DEVMNG);
    if (ret < 0) {
        return;
    }

    (void)ioctl(fd, DAVINCI_INTF_IOCTL_CLOSE, &arg);
}

mmProcess devdrv_open_device_manager(void)
{
    mmProcess fd = -1;
    int err = 0;
#ifdef __linux
    int ret, ret_do_container;
#endif

    /* to improve performance */
    if (!fd_is_invalid(devdrv_manager_fd)) {
        if (devdrv_manager_tgid == getpid()) {
            return devdrv_manager_fd;
        }
    }

#ifndef __linux
    if (devdrv_manager_fd_mutex == NULL) {
        mmMutexInit(&devdrv_manager_fd_mutex);
    }
#endif

    (void)mmMutexLock(&devdrv_manager_fd_mutex);

    if (!fd_is_invalid(devdrv_manager_fd)) {
        if (devdrv_manager_tgid != getpid()) {
            devdrv_manager_fd = (mmProcess)DEVDRV_INVALID_FD_OR_INDEX;
        } else {
            fd = devdrv_manager_fd;
            goto out;
        }
    }

#ifdef __linux
    fd = mmOpen2(davinci_intf_get_dev_path(), M_RDWR | O_CLOEXEC, M_IRUSR);
    err = (__errno_location() != NULL ? errno : 0);
    ret = devmng_ioctl_open(fd);
    dmanage_share_log_create();
#else
    fd = devdrv_open_device_manager_win(&devdrv_manager_fd);
#endif
    if (fd_is_invalid(fd)) {
        DEVDRV_DRV_ERR("Failed to Open device manager. (name=%s; ret_fd=%d; old_fd=%d; err=%d)\n",
                       davinci_intf_get_dev_path(), fd, devdrv_manager_fd, err);
        fd = (mmProcess)DEVDRV_INVALID_FD_OR_INDEX;
        dmanage_share_log_destroy();
        goto out;
    }
#ifdef __linux
    u32 dev_num = 0;
    ret_do_container = drvGetDevNum(&dev_num);
    if ((ret != 0) || (ret_do_container != 0)) {
        devmng_ioctl_close(fd);
        (void)mmClose(fd);
        DEVDRV_DRV_ERR("devdrv_do_container return error. (ret=%d, fd=%d)\n", ret_do_container, fd);
        fd = (mmProcess)DEVDRV_INVALID_FD_OR_INDEX;
        if (ret_do_container == DRV_ERROR_RESOURCE_OCCUPIED) {
            fd = (mmProcess)(-DRV_ERROR_RESOURCE_OCCUPIED);
        }
        dmanage_share_log_destroy();
        goto out;
    }
#endif
    devdrv_manager_fd = fd;
    devdrv_manager_tgid = getpid();
out:
    (void)mmMutexUnLock(&devdrv_manager_fd_mutex);
    return fd;
}

void devdrv_close_device_manager(void)
{
    (void)mmMutexLock(&devdrv_manager_fd_mutex);
    if (!fd_is_invalid(devdrv_manager_fd)) {
#if defined __linux
        if (devdrv_manager_tgid == getpid()) {
            devmng_ioctl_close(devdrv_manager_fd);
            (void)mmClose(devdrv_manager_fd);
            dmanage_share_log_destroy();
        }
#endif
        devdrv_manager_fd = (mmProcess)DEVDRV_INVALID_FD_OR_INDEX;
        devdrv_manager_tgid = (pid_t)DEVDRV_INVALID_TGID;
    }
    (void)mmMutexUnLock(&devdrv_manager_fd_mutex);
}

mmProcess devdrv_get_dev_manager_fd(void)
{
    mmProcess fd = -1;

    (void)mmMutexLock(&devdrv_manager_fd_mutex);

    fd = devdrv_manager_fd;

    (void)mmMutexUnLock(&devdrv_manager_fd_mutex);

    return fd;
}

void drv_ioctl_param_init(mmIoctlBuf *icotl_buf, void *inbuf, int inbufLen)
{
    icotl_buf->inbuf = inbuf;
    icotl_buf->inbufLen = inbufLen;
    icotl_buf->outbuf = icotl_buf->inbuf;
    icotl_buf->outbufLen = icotl_buf->inbufLen;
    icotl_buf->oa = NULL;
}

int drv_common_ioctl(mmIoctlBuf *icotl_buf, int cmd)
{
    int ret;
    int err_buf;
    mmProcess fd;

    fd = devdrv_open_device_manager();
    if ((int)fd == -DRV_ERROR_RESOURCE_OCCUPIED) {
        DEVDRV_DRV_ERR("open device manager failed, device is busy.\n");
        return DRV_ERROR_RESOURCE_OCCUPIED;
    }
    if (fd_is_invalid(fd)) {
        DEVDRV_DRV_ERR("open device manager failed, fd = %d.\n", fd);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = dmanage_mmIoctl(fd, cmd, icotl_buf);
    if (ret == 0) {
        return 0;
    }
    /* to avoid errno be changed by print */
    err_buf = errno;
#ifndef DEVDRV_UT
    if (err_buf == EOPNOTSUPP || err_buf == EAGAIN) {
        DEVDRV_DRV_WARN("cmd = %d, ret = %d\n", cmd, ret);
        ret = errno_to_user_errno(err_buf);
    } else {
        DEVDRV_DRV_ERR_EXTEND(err_buf, EBUSY, "Ioctl info. (cmd=%d; ret=%d)\n", cmd, err_buf);
        ret = errno_to_user_errno(err_buf);
    }
#endif
    return ret;
}

void init_devdrv_manager_lock(void)
{
    (void)mmMutexInit(&devdrv_manager_fd_mutex);
    return;
}

drvError_t devdrv_close_restore_device_manager(void)
{
#ifdef CFG_FEATURE_DRV_DEVMNG_INIT
    devdrv_close_device_manager();
    unsigned int container_flag = false;
    if (access(davinci_intf_get_dev_path(), R_OK | W_OK) != 0) {
        return DRV_ERROR_NO_DEVICE;
    }
    /* davinci_manager has been opened in dmanager_get_container_flag
       have to close davinci_manager when get flag fail */
    if (dmanage_get_container_flag(&container_flag)) {
        devdrv_close_device_manager();
        DEVDRV_DRV_ERR("Get container flag failed.\n");
        return DRV_ERROR_OPEN_FAILED;
    }
#endif
    return DRV_ERROR_NONE;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wprio-ctor-dtor"
/* By default, constructor priorities are reserved from 0 to 100 */
STATIC void __attribute__((constructor(0))) drv_manager_init(void)
{
#ifdef CFG_FEATURE_DRV_DEVMNG_INIT
    unsigned int container_flag = false;
    if (access(davinci_intf_get_dev_path(), R_OK | W_OK) != 0) {
        return;
    }
    /* davinci_manager has been opened in dmanager_get_container_flag
       have to close davinci_manager when get flag fail */
    if (dmanage_get_container_flag(&container_flag)) {
        devdrv_close_device_manager();
        DEVDRV_DRV_ERR("get container flag failed.\n");
        return;
    }
#endif
}
#pragma GCC diagnostic pop

STATIC void __attribute__((destructor)) drv_manager_un_init(void)
{
#ifdef CFG_FEATURE_DRV_DEVMNG_INIT
    devdrv_close_device_manager();
#endif
}
