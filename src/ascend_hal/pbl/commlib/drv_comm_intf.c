/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "ascend_dev_num.h"
#include "soc_resmng_ioctl.h"
#include "davinci_interface.h"
#include "drv_comm_intf.h"
#include "drv_current.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

STATIC bool g_drv_open_close_array[ASCEND_DEV_MAX_NUM] = {false};
static pthread_mutex_t drv_open_dev_mutex = PTHREAD_MUTEX_INITIALIZER;
STATIC bool proc_res_had_backup = false;
static pthread_mutex_t drv_proc_res_mutex = PTHREAD_MUTEX_INITIALIZER;

STATIC int drvDevCloseHostUserComponent(uint32_t devid, halDevCloseIn *in, int index)
{
    int i;
    int ret;
    int err_ret = 0;
    for (i = index - 1; i >= 0; i--) {
        if (drv_close_host_user_handlers[i] == NULL) {
            continue;
        }

        ret = drv_close_host_user_handlers[i](devid, in);
        if (ret != 0) {
            DRV_COMM_EX_NOTSUPPORT_ERR(ret, "Close failed. (devid=%u; func_id=%d; ret=%d)\n", devid, i, ret);
            err_ret = ret;
        }
    }

    return err_ret;
}

STATIC int drvDevCloseComponent(uint32_t devid, halDevCloseIn *in, int index)
{
    int i;
    int ret;
    int err_ret = 0;
    for (i = index - 1; i >= 0; i--) {
        if (drv_close_handlers[i] == NULL) {
            continue;
        }

        ret = drv_close_handlers[i](devid, in);
        if (ret != 0) {
            DRV_COMM_EX_NOTSUPPORT_ERR(ret, "drvDevCloseComponent failed. (devid=%u; func_id=%d; ret=%d)\n", devid, i,
                                       ret);
            err_ret = ret;
        }
    }

    return err_ret;
}

STATIC int drvdev_check_input_para(halDevCloseIn *in)
{
    if (in == NULL) {
        return 0;
    }

#ifndef CFG_FEATURE_DEVICE_REPLACE
    if (in->close_type == DEV_CLOSE_ONLY_HOST) {
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif

    return 0;
}

drvError_t halDeviceOpen(uint32_t devid, halDevOpenIn *in, halDevOpenOut *out)
{
    int i, ret;

    if (out == NULL) {
        DRV_COMM_ERR("out ptr is NULL. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (devid >= ASCEND_DEV_MAX_NUM) {
        DRV_COMM_ERR("Devid is invalid. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    (void)pthread_mutex_lock(&drv_open_dev_mutex);
    if (g_drv_open_close_array[devid] == true) {
        (void)pthread_mutex_unlock(&drv_open_dev_mutex);
        DRV_COMM_ERR("Device is already open. (devid=%u)\n", devid);
        return DRV_ERROR_REPEATED_INIT;
    }

    for (i = 0; i < MAX_DEV_OPERATION; i++) {
        if (drv_open_handlers[i] == NULL) {
            continue;
        }

        ret = drv_open_handlers[i](devid, in, out);
        if (ret != 0) {
            /* set the flag(NULL) to close all resources on the host and device sides. */
            (void)drvDevCloseComponent(devid, NULL, i);
            (void)pthread_mutex_unlock(&drv_open_dev_mutex);
            DRV_COMM_EX_NOTSUPPORT_ERR(ret, "halDeviceOpen failed. (devid=%u; func_id=%d; ret=%d)\n", devid, i, ret);
            return ret;
        }
    }

    g_drv_open_close_array[devid] = true;
    (void)pthread_mutex_unlock(&drv_open_dev_mutex);

    return DRV_ERROR_NONE;
}

drvError_t halDeviceClose(uint32_t devid, halDevCloseIn *in)
{
    int ret;

    DRV_CURRENT_API_SCOPE();

    ret = drvdev_check_input_para(in);
    if (ret != 0) {
        DRV_COMM_EX_NOTSUPPORT_ERR(ret, "Parameter input check failed. (ret=%d)\n", ret);
        return ret;
    }

    if (devid >= ASCEND_DEV_MAX_NUM) {
        DRV_COMM_ERR("Devid is invalid. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    (void)pthread_mutex_lock(&drv_open_dev_mutex);
    if (g_drv_open_close_array[devid] == false) {
        (void)pthread_mutex_unlock(&drv_open_dev_mutex);
        DRV_COMM_ERR("Device is not open. (devid=%u)\n", devid);
        return DRV_ERROR_UNINIT;
    }

    if ((in != NULL) && (in->close_type == DEV_CLOSE_HOST_USER)) {
        ret = drvDevCloseHostUserComponent(devid, in, MAX_DEV_OPERATION);
        if (ret != 0) {
            DRV_COMM_EX_NOTSUPPORT_ERR(ret, "Close host user failed. (devid=%u; ret=%d)\n", devid, ret);
        }
    } else {
        ret = drvDevCloseComponent(devid, in, MAX_DEV_OPERATION);
        if (ret != 0) {
            DRV_COMM_EX_NOTSUPPORT_ERR(ret, "halDeviceClose failed. (devid=%u; ret=%d)\n", devid, ret);
        }
    }

    g_drv_open_close_array[devid] = false;
    (void)pthread_mutex_unlock(&drv_open_dev_mutex);

    return ret;
}

drvError_t halProcessResBackup(halProcResBackupInfo *info)
{
    drvError_t ret;
    int i;

    DRV_COMM_EVENT("Process resource backup start.\n");

    if (info == NULL) {
        DRV_COMM_ERR("Info is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    (void)pthread_mutex_lock(&drv_proc_res_mutex);
    if (proc_res_had_backup == true) {
        DRV_COMM_EVENT("Process resource backup again.\n");
    }

    for (i = 0; i < MAX_DEV_OPERATION; i++) {
        if (drv_proc_res_backup_handlers[i] == NULL) {
            continue;
        }

        ret = drv_proc_res_backup_handlers[i](info);
        if (ret != DRV_ERROR_NONE) {
            (void)pthread_mutex_unlock(&drv_proc_res_mutex);
            DRV_COMM_ERR("Process resource backup failed. (func_id=%d; ret=%d)\n", i, ret);
            return ret;
        } else {
            DRV_COMM_EVENT("Process resource backup succ. (func_id=%d; ret=%d)\n", i, ret);
        }
    }
    proc_res_had_backup = true;
    DRV_COMM_EVENT("Process resource backup end.\n");
    (void)pthread_mutex_unlock(&drv_proc_res_mutex);

    return DRV_ERROR_NONE;
}

drvError_t halProcessResRestore(halProcResRestoreInfo *info)
{
    drvError_t ret;
    int i;

    DRV_CURRENT_API_SCOPE();

    DRV_COMM_EVENT("Process resource restore start.\n");

    if (info == NULL) {
        DRV_COMM_ERR("Info is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    (void)pthread_mutex_lock(&drv_proc_res_mutex);
    if (proc_res_had_backup == false) {
        (void)pthread_mutex_unlock(&drv_proc_res_mutex);
        DRV_COMM_ERR("Process resource not backup or had restore.\n");
        return DRV_ERROR_OPER_NOT_PERMITTED;
    }

    for (i = 0; i < MAX_DEV_OPERATION; i++) {
        if (drv_proc_res_restore_handlers[i] == NULL) {
            continue;
        }

        ret = drv_proc_res_restore_handlers[i](info);
        if (ret != DRV_ERROR_NONE) {
            (void)pthread_mutex_unlock(&drv_proc_res_mutex);
            DRV_COMM_ERR("Process resource restore failed. (func_id=%d; ret=%d)\n", i, ret);
            return ret;
        } else {
            DRV_COMM_EVENT("Process resource restore succ. (func_id=%d; ret=%d)\n", i, ret);
        }
    }

    proc_res_had_backup = false;
    DRV_COMM_EVENT("Process resource restore end.\n");
    (void)pthread_mutex_unlock(&drv_proc_res_mutex);

    return DRV_ERROR_NONE;
}

u32 soc_res_get_ver(u32 udevid, enum soc_ver_type type, u32 *ver)
{
    int fd = -1, ret, tmp_errno, tmp;
    struct davinci_intf_open_arg arg = {{0}};

    fd = open(davinci_intf_get_dev_path(), O_RDWR | O_SYNC | O_CLOEXEC);
    if (fd < 0) {
        DRV_COMM_ERR("Open device fail. (fd=%d; chardev=%s; errno=%d).\n", fd, davinci_intf_get_dev_path(), errno);
        return fd;
    }

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, SOC_RESMNG_MODULE_NAME);
    if (ret != 0) {
        DRV_COMM_ERR("Strcpy_s failed. (ret=%d)\n", ret);
        ret = DRV_ERROR_INNER_ERR;
        goto close_fd;
    }

    ret = ioctl(fd, (unsigned long)DAVINCI_INTF_IOCTL_OPEN, &arg);
    if (ret != 0) {
        tmp_errno = errno;
        DRV_COMM_ERR("DAVINCI_INTF_IOCTL_OPEN failed. (ret=%d)\n", ret);
        ret = DRV_COMM_KERNEL_ERR(tmp_errno);
        goto davinci_close;
    }

    if (type == VER_TYPE_DEV) {
        ret = ioctl(fd, SOC_RESMNG_GET_DEV_VER, ver);
        if (ret < 0) {
            DRV_COMM_ERR("get device version fail, (udevid=%u; errno=%d).\n", udevid, errno);
            goto davinci_close;
        }
    } else if (type == VER_TYPE_HOST) {
        ret = ioctl(fd, SOC_RESMNG_GET_HOST_VER, ver);
        if (ret < 0) {
            DRV_COMM_ERR("get device version fail, (udevid=%u; errno=%d)\n", udevid, errno);
            goto davinci_close;
        }
    } else {
        DRV_COMM_ERR("invalid version type, (udevid=%u)\n", udevid);
        ret = DRV_ERROR_INNER_ERR;
        goto davinci_close;
    }

davinci_close:
    tmp = ioctl(fd, (unsigned long)DAVINCI_INTF_IOCTL_CLOSE, &arg);
    if (tmp != 0) {
        DRV_COMM_ERR("Davinci close fail. (ret=%d)\n", tmp);
    }
close_fd:
    tmp = close(fd);
    if (tmp != 0) {
        DRV_COMM_ERR("Close device fail. (ret=%d; fd=%d)\n", tmp, fd);
    }
    return ret;
}
