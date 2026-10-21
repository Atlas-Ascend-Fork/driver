/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>

#include "devmng_common.h"
#include "devdrv_ioctl.h"
#include "devdrv_user_common.h"
#include "devdrv_host_flush_cache.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devdrv_container.h"
#include "dms/dms_drv_internal.h"

#define INVALID_PID (-1)
#define MAX_SINGLE_NUM_OF_DEV_ID 9
#define MAX_LEN_OF_DEV_ID_STR 2
#define INVALID_LEN_OF_DEV_ID_STR 0

#define tgid_is_valid(tgid) ((tgid) >= 0)

STATIC pthread_mutex_t g_tgid_lock = PTHREAD_MUTEX_INITIALIZER;
STATIC pid_t g_bare_tgid = (pid_t)INVALID_PID;
STATIC pid_t g_bare_getpid = (pid_t)INVALID_PID;

void drvClearBareTgid(void)
{
    (void)pthread_mutex_lock(&g_tgid_lock);
    g_bare_tgid = (pid_t)INVALID_PID;
    g_bare_getpid = (pid_t)INVALID_PID;
    (void)pthread_mutex_unlock(&g_tgid_lock);
}

pid_t drvDeviceGetBareTgid(void)
{
    struct devdrv_container_para container_para = {{0}, 0};
    int fd = -1;
    pid_t tgid = (pid_t)(-1);
    int ret;

    /* to improve performance */
    if (tgid_is_valid(g_bare_tgid)) {
        if (g_bare_getpid == getpid()) {
            return g_bare_tgid;
        }
    }

    fd = devdrv_open_device_manager();
    if (fd < 0) {
        DEVDRV_DRV_ERR("open device manager failed, fd(%d).\n", fd);
        return INVALID_PID;
    }
    container_para.para.cmd = DEVDRV_CONTAINER_GET_BARE_TGID;
    container_para.para.out = &tgid;
    ret = ioctl(fd, DEVDRV_MANAGER_CONTAINER_CMD, &container_para);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, ret(%d).\n", ret);
        return INVALID_PID;
    }

    (void)pthread_mutex_lock(&g_tgid_lock);
    g_bare_tgid = tgid;
    mb();
    g_bare_getpid = getpid();
    (void)pthread_mutex_unlock(&g_tgid_lock);

    return tgid;
}

drvError_t drvStartContainerServe(void)
{
#ifndef CFG_FEATURE_DISABLE_CONTAINER
    int fd = -1;

    fd = devdrv_open_device_manager();
    if ((int)fd == -DRV_ERROR_RESOURCE_OCCUPIED) {
        DEVDRV_DRV_ERR("Device is busy. (ret=%d)\n", (int)fd);
        return DRV_ERROR_RESOURCE_OCCUPIED;
    }
    if (fd < 0) {
        DEVDRV_DRV_ERR("open device manager failed, fd(%d).\n", fd);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return DRV_ERROR_NONE;
#else
    return DRV_ERROR_NOT_SUPPORT;
#endif
}
