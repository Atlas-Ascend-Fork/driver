/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdbool.h>

#include "securec.h"
#include "davinci_interface.h"
#include "pbl_uda_user.h"

#include "svm_user_adapt.h"
#include "svm_log.h"
#include "svm_pub.h"
#include "svm_criu.h"
#include "svm_ioctl_ex.h"
#include "svm_ioctl_api.h"

struct svm_dev_intf {
    int valid;
    int fd;
    int pid;
};

static struct svm_dev_intf svm_dev_fd[SVM_MAX_DEV_NUM];
static pthread_mutex_t svm_fd_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifndef L2_EMU_ST
static void svm_set_dev_fd(u32 devid, int fd)
{
    svm_dev_fd[devid].fd = fd;
    svm_dev_fd[devid].pid = svm_getpid();
    svm_dev_fd[devid].valid = (fd == -1) ? 0 : 1;
}

static int svm_get_dev_fd(u32 devid)
{
    if (svm_dev_fd[devid].valid == 0) {
        return -1;
    }

    if (svm_dev_fd[devid].pid != svm_getpid()) {
        return -1;
    }

    return svm_dev_fd[devid].fd;
}
#endif

static int svm_char_dev_open(u32 udevid, int *fd)
{
    struct davinci_intf_open_arg arg;
    int tmp_errno, ret, cnt = 0;
    bool retry = false;

    arg.device_id = (int)udevid;
    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, SVM_CHAR_DEV_NAME);
    if (ret < 0) {
        svm_err("Strcpy_s failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return DRV_ERROR_INNER_ERR;
    }

    *fd = svm_file_open(davinci_intf_get_dev_path(), O_RDWR | O_CLOEXEC);
    if (*fd < 0) {
        tmp_errno = errno;
        svm_err("Open device failed. (udevid=%u; fd=%d; errno=%d, error=%s)\n", udevid, *fd, tmp_errno,
                strerror(tmp_errno));
        return errno_to_user_errno(tmp_errno);
    }

    /* If a process with the same PID is restarted, the resources of the old process are not reclaimed.
       As a result, the new process fails to be added to the hash table. */
    do {
        ret = svm_user_ioctl(*fd, DAVINCI_INTF_IOCTL_OPEN, &arg);
        cnt++;
        retry = ((ret != 0) && (errno == EBUSY) && (cnt < 1000)); /* 1000 retry cnt */
        if (retry) {
            usleep(100000); /* 100000us(100ms) */
        }
    } while (retry);
    if (ret != 0) {
        tmp_errno = errno;
        svm_err("Ioctl failed. (udevid=%u; fd=%d; errno=%d, error=%s)\n", udevid, *fd, tmp_errno, strerror(tmp_errno));
        svm_file_close(*fd);
        return errno_to_user_errno(tmp_errno);
    }

    return 0;
}

static int svm_char_dev_close(u32 udevid, int fd)
{
    struct davinci_intf_close_arg arg;
    int ret, tmp_errno;

    arg.device_id = (int)udevid;
    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, SVM_CHAR_DEV_NAME);
    if (ret < 0) {
        svm_err("Strcpy_s failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = svm_user_ioctl(fd, DAVINCI_INTF_IOCTL_CLOSE, &arg);
    if (ret != 0) {
        tmp_errno = errno;
        svm_err("Ioctl warn. (udevid=%u; fd=%d; errno=%d, error=%s)\n", udevid, fd, tmp_errno, strerror(tmp_errno));
        return errno_to_user_errno(tmp_errno);
    }

    svm_file_close(fd);

    return 0;
}

#define SVM_IOCTL_DEV_HANDLE_MAX_NUM 21

static int (*svm_ioctl_dev_init_post_handle[SVM_IOCTL_DEV_HANDLE_MAX_NUM])(u32 devid) = {
    NULL,
};
static int (*svm_ioctl_dev_uninit_pre_handle[SVM_IOCTL_DEV_HANDLE_MAX_NUM])(u32 devid) = {
    NULL,
};

int svm_register_ioctl_dev_init_post_handle(int (*fn)(u32 devid))
{
    int i;

    for (i = 0; i < SVM_IOCTL_DEV_HANDLE_MAX_NUM; i++) {
        if (svm_ioctl_dev_init_post_handle[i] == NULL) {
            svm_ioctl_dev_init_post_handle[i] = fn;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_INNER_ERR;
}

int svm_register_ioctl_dev_uninit_pre_handle(int (*fn)(u32 devid))
{
    int i;

    for (i = 0; i < SVM_IOCTL_DEV_HANDLE_MAX_NUM; i++) {
        if (svm_ioctl_dev_uninit_pre_handle[i] == NULL) {
            svm_ioctl_dev_uninit_pre_handle[i] = fn;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_INNER_ERR;
}

static int svm_call_ioctl_dev_init_post_handle(u32 devid)
{
    int ret, i;

    for (i = 0; i < SVM_IOCTL_DEV_HANDLE_MAX_NUM; i++) {
        if (svm_ioctl_dev_init_post_handle[i] != NULL) {
            ret = svm_ioctl_dev_init_post_handle[i](devid);
            if (ret != DRV_ERROR_NONE) {
                svm_err("Post handle failed. (i=%d; ret=%d; devid=%u)\n", i, ret, devid);
                return ret;
            }
        }
    }

    return DRV_ERROR_NONE;
}

static int svm_call_ioctl_dev_uninit_pre_handle(u32 devid)
{
    int ret, i;

    for (i = SVM_IOCTL_DEV_HANDLE_MAX_NUM - 1; i >= 0; i--) {
        if (svm_ioctl_dev_uninit_pre_handle[i] != NULL) {
            ret = svm_ioctl_dev_uninit_pre_handle[i](devid);
            if (ret != DRV_ERROR_NONE) {
                svm_err("Pre handle failed. (i=%d; ret=%d; devid=%u)\n", i, ret, devid);
                return ret;
            }
        }
    }

    return DRV_ERROR_NONE;
}

/* including the use of host device */
int svm_ioctl_dev_init(u32 devid)
{
    u32 udevid;
    int fd, ret;

    if (devid >= SVM_MAX_DEV_NUM) {
        svm_err("Invalid para. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = uda_get_udevid_by_devid_ex(devid, &udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", devid, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    (void)pthread_mutex_lock(&svm_fd_mutex);
    fd = svm_get_dev_fd(devid);
    if (fd >= 0) {
        (void)pthread_mutex_unlock(&svm_fd_mutex);
        return DRV_ERROR_REPEATED_INIT;
    }

    ret = svm_char_dev_open(udevid, &fd);
    share_log_read_run_info(HAL_MODULE_TYPE_DEVMM);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&svm_fd_mutex);
        svm_err("Open failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    svm_set_dev_fd(devid, fd);

    ret = svm_call_ioctl_dev_init_post_handle(devid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Post handle failed. (ret=%d; devid=%u)\n", ret, devid);
        svm_set_dev_fd(devid, -1);
        (void)svm_char_dev_close(udevid, fd);
        (void)pthread_mutex_unlock(&svm_fd_mutex);
        return ret;
    }

    (void)pthread_mutex_unlock(&svm_fd_mutex);

    svm_debug("Open fd. (pid=%d; devid=%u)\n", svm_getpid(), devid);

    return 0;
}

/* including the use of host device */
int svm_ioctl_dev_uninit(u32 devid)
{
    u32 udevid;
    int fd, ret;

    if (devid >= SVM_MAX_DEV_NUM) {
        svm_err("Invalid para. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = uda_get_udevid_by_devid_ex(devid, &udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    (void)pthread_mutex_lock(&svm_fd_mutex);
    fd = svm_get_dev_fd(devid);
    if (fd < 0) {
        (void)pthread_mutex_unlock(&svm_fd_mutex);
        svm_err("Repeated close. (devid=%u)\n", devid);
        return DRV_ERROR_NOT_EXIST;
    }

    ret = svm_call_ioctl_dev_uninit_pre_handle(devid);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&svm_fd_mutex);
        return ret;
    }

    ret = svm_char_dev_close(udevid, fd);
    share_log_read_run_info(HAL_MODULE_TYPE_DEVMM);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&svm_fd_mutex);
        return ret;
    }

    svm_set_dev_fd(devid, -1);
    (void)pthread_mutex_unlock(&svm_fd_mutex);

    svm_debug("Close fd. (pid=%d; devid=%u)\n", svm_getpid(), devid);

    return 0;
}

int svm_cmd_ioctl(u32 devid, u32 cmd, void *para)
{
    int fd, ret, tmp_errno;

    if (devid >= SVM_MAX_DEV_NUM) {
        svm_err("Invalid para. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_criu_is_resetting(devid)) {
        return DRV_ERROR_NONE;
    }

    fd = svm_get_dev_fd(devid);
    if (fd < 0) {
        svm_err("No valid fd. (devid=%u)\n", devid);
        return DRV_ERROR_NOT_EXIST;
    }

    do {
        ret = svm_user_ioctl(fd, (unsigned int)cmd, para);
        tmp_errno = errno;
        if ((ret == -1) && (tmp_errno == EINTR)) {
            svm_run_info("Ioctl retry. (devid=%u; cmd=%u)\n", devid, cmd);
        }
    } while ((ret == -1) && (tmp_errno == EINTR));

    return (ret == 0) ? 0 : errno_to_user_errno(tmp_errno);
}

int svm_ioctl_criu_reset(u32 devid)
{
    svm_set_dev_fd(devid, -1);
    return 0;
}
