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
#include <sys/ioctl.h>
#include <pthread.h>
#include <unistd.h>

#include "securec.h"
#include "davinci_interface.h"

#include "trs_shr_id_ioctl.h"
#include "trs_user_pub_def.h"

#ifdef EMU_ST
#define THREAD__ __thread
#else
#define THREAD__
#endif

static pthread_mutex_t shrid_device_mutex = PTHREAD_MUTEX_INITIALIZER;

static THREAD__ int shrid_fd = -1;
static THREAD__ pid_t shrid_pid = -1;

#ifndef EMU_ST
static int _shrid_open(const char *pathname, int flags)
{
    return open(pathname, flags);
}

static int _shrid_close(int fd)
{
    return close(fd);
}

static int _shrid_ioctl(int fd, unsigned int cmd, void *para)
{
    return ioctl(fd, cmd, para);
}
#else
int _shrid_open(const char *pathname, int flags);
int _shrid_close(int fd);
int _shrid_ioctl(int fd, unsigned int cmd, void *para);
#endif /* EMU_ST */

static void shrid_set_pid(pid_t pid)
{
    shrid_pid = pid;
}

static int shrid_get_pid(void)
{
    return shrid_pid;
}

void shrid_set_fd(int fd)
{
    shrid_fd = fd;
}

static int _shrid_get_fd(void)
{
    if ((shrid_fd >= 0) && (shrid_get_pid() == getpid())) {
        return shrid_fd;
    }

    if (shrid_fd >= 0) {
        (void)_shrid_close(shrid_fd);
    }

    return -1;
}

#define MAX_SHRID_OPEN_RETRY_TIME 3000
#define SHRID_OPEN_RETRY_INTERVAL 100000 /* 100ms */
static int shrid_ioctl_open(void)
{
    struct davinci_intf_open_arg arg = {};
    int ret, fd, cnt = 0;
    bool retry = false;

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_INTF_MODULE_TRS_SHR_ID);
    if (ret < 0) {
        trs_err("strcpy_s failed, ret(%d).\n", ret);
        return -1;
    }

    fd = _shrid_open(davinci_intf_get_dev_path(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        trs_err("open failed, fd(%d), errno(%d), error(%s).\n", fd, errno, strerror(errno));
        return fd;
    }

    do {
        ret = _shrid_ioctl(fd, DAVINCI_INTF_IOCTL_OPEN, &arg);
        cnt++;
        retry = ((ret != 0) && (errno == EBUSY) && (cnt < MAX_SHRID_OPEN_RETRY_TIME));
        if (retry) {
            usleep(SHRID_OPEN_RETRY_INTERVAL);
        }
    } while (retry);

    if (ret != 0) {
        trs_err("ioctl failed, fd(%d), errno(%d), error(%s).\n", fd, errno, strerror(errno));
        (void)_shrid_close(fd);
        fd = -1;
    }
    return fd;
}

static int shrid_open(void)
{
    int fd = -1;

    fd = _shrid_get_fd();
    if (fd < 0) {
        fd = shrid_ioctl_open();
        if (fd < 0) {
            trs_err("open failed, fd(%d), errno(%d), error(%s).\n", fd, errno, strerror(errno));
            return -1;
        }
        shrid_set_fd(fd);

        int flags = fcntl(fd, F_GETFD);
        flags = (int)((unsigned int)flags | FD_CLOEXEC);
        (void)fcntl(fd, F_SETFD, flags);
        shrid_set_pid(getpid());
    }

    return fd;
}

static int shrid_get_fd(void)
{
    int fd;

    (void)pthread_mutex_lock(&shrid_device_mutex);
    fd = _shrid_get_fd();
    if (fd <= 0) {
        fd = shrid_open();
    }
    (void)pthread_mutex_unlock(&shrid_device_mutex);

    return fd;
}

static int get_type_by_name(const char *name, int *id_type)
{
    u32 tgid, devid, tsid, tmp_type;
    size_t name_len;
    int ret;

    name_len = strnlen(name, SHR_ID_NSM_NAME_SIZE);
    if ((name_len == 0) || (name_len >= SHR_ID_NSM_NAME_SIZE)) {
        trs_err("Length out of range. (name_len=%lu; valid range=[1, %lu))\n", name_len,
                (unsigned long)SHR_ID_NSM_NAME_SIZE);
        return -EINVAL;
    }

    ret = sscanf_s(name, "%08x %08x %08x %08x", &tgid, &devid, &tsid, &tmp_type);
    if (ret != 4) { /* 4 for parse nums */
        trs_err("sscanf failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    *id_type = (int)tmp_type;
    if ((*id_type >= SHR_ID_TYPE_MAX) || (*id_type < 0)) {
        trs_err("Para invalid. (id_type=%u)\n", *id_type);
        return -EINVAL;
    }
    return 0;
}

drvError_t shrid_ioctl(u32 cmd, void *para)
{
    int fd, ret, type;

    fd = shrid_get_fd();
    if (fd < 0) {
        trs_err("Open failed. (fd=%d; cmd=%u)\n", fd, _IOC_NR(cmd));
        return DRV_ERROR_OPEN_FAILED;
    }

    ret = _shrid_ioctl(fd, cmd, para);
    if (ret != 0) {
        if (errno == EOPNOTSUPP) {
            return DRV_ERROR_NOT_SUPPORT;
        } else if (errno == EPERM) {
            return DRV_ERROR_OPER_NOT_PERMITTED;
        } else {
            if ((errno == EACCES) && (cmd == SHR_ID_OPEN)) {
                ret = get_type_by_name(((struct shr_id_ioctl_info *)para)->name, &type);
                if (ret != 0) {
                    trs_err("get type by name failed. (ret=%d)\n", ret);
                    return DRV_ERROR_IOCRL_FAIL;
                }
                if (type == SHR_ID_EVENT_TYPE) {
                    trs_event_operation_not_supported_err_msg();
                } else if (type == SHR_ID_NOTIFY_TYPE) {
                    trs_notify_operation_not_supported_err_msg(((struct shr_id_ioctl_info *)para)->name);
                }
            }
            trs_err("Ioctl failed. (ret=%d; cmd=%u).\n", errno, _IOC_NR(cmd));
            return DRV_ERROR_IOCRL_FAIL;
        }
    }
    return DRV_ERROR_NONE;
}

void shrid_close(void)
{
    int fd = -1;

    (void)pthread_mutex_lock(&shrid_device_mutex);
    fd = _shrid_get_fd();
    if (fd >= 0) {
        (void)_shrid_close(fd);
    }
    shrid_set_fd(-1);
    shrid_set_pid(-1);
    (void)pthread_mutex_unlock(&shrid_device_mutex);
}
