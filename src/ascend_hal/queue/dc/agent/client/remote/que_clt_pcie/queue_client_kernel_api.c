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
#include <unistd.h>
#include <sys/ioctl.h>

#include "davinci_interface.h"
#include "queue_client_base.h"
#include "queue_kernel_api.h"
#include "securec.h"

#ifdef EMU_ST
#include <sys/syscall.h>
#ifndef THREAD
#define THREAD __thread
#endif
#else
#ifndef THREAD
#define THREAD
#endif
#endif

THREAD int g_queue_fd = -1;

void set_g_queue_fd(int fd)
{
    g_queue_fd = fd;
}

int get_g_queue_fd(void)
{
    return g_queue_fd;
}

#ifndef EMU_ST
STATIC int queue_open_dev(void)
{
    struct davinci_intf_open_arg arg = {{0}, 0};
    int ret, fd, temp_fd;

    temp_fd = get_g_queue_fd();
    if (temp_fd > 0) {
        return temp_fd;
    }
    fd = open(davinci_intf_get_dev_path(), O_RDWR | O_SYNC);
    if (fd < 0) {
        QUEUE_LOG_ERR("open dev=%d.\n", fd);
        return fd;
    } else {
        int flags = fcntl(fd, F_GETFD);
        flags = (int)((unsigned int)flags | FD_CLOEXEC);
        (void)fcntl(fd, F_SETFD, flags);
    }

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_QUEUE_SUB_MODULE_NAME);
    if (ret != 0) {
#ifndef QUEUE_UT
        QUEUE_LOG_ERR("strcpy_s failed, ret(%d).\n", ret);
        (void)close(fd);
        return -1;
#endif
    }
    share_log_create(HAL_MODULE_TYPE_QUEUE_MANAGER, SHARE_LOG_MAX_SIZE);
    ret = ioctl(fd, DAVINCI_INTF_IOCTL_OPEN, &arg);
    if (ret != 0) {
        QUEUE_LOG_ERR("DAVINCI_INTF_IOCTL_OPEN failed, ret(%d).\n", ret);
        (void)close(fd);
        return -1;
    }
    set_g_queue_fd(fd);

    return fd;
}

void queue_close_dev(int fd)
{
    struct davinci_intf_open_arg arg = {{0}, 0};
    int ret;

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_QUEUE_SUB_MODULE_NAME);
    if (ret != 0) {
        QUEUE_LOG_ERR("strcpy_s failed, ret(%d).\n", ret);
        (void)close(fd);
        set_g_queue_fd(-1);
        return;
    }

    ret = ioctl(fd, DAVINCI_INTF_IOCTL_CLOSE, &arg);
    if (ret != 0) {
        QUEUE_LOG_ERR("DAVINCI_INTF_IOCTL_CLOSE failed, ret(%d).\n", ret);
    }

    (void)close(fd);
    set_g_queue_fd(-1);
}

STATIC drvError_t queue_ioctl(unsigned int cmd, void *para)
{
    int ret;

    if (get_g_queue_fd() < 0) {
        QUEUE_LOG_ERR("queue fd not init.\n");
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = ioctl(get_g_queue_fd(), cmd, para);
    if (ret != 0) {
        QUEUE_LOG_ERR("ioctl failed. (cmd=%x; error=%d; ret=%d)\n", cmd, errno, ret);
        share_log_read_err(HAL_MODULE_TYPE_QUEUE_MANAGER);
        share_log_read_run_info(HAL_MODULE_TYPE_QUEUE_MANAGER);
        if (ret > 0) {
            return (drvError_t)ret;
        } else {
            return DRV_ERROR_IOCRL_FAIL;
        }
    }

    return DRV_ERROR_NONE;
}
#endif

int queue_open_dev_base(void)
{
    return queue_open_dev();
}

drvError_t queue_host_common_queue_init(unsigned int dev_id)
{
    struct queue_ioctl_host_common_op arg;
    arg.op_flag = QUEUE_INIT;
    arg.devid = dev_id;
    return queue_ioctl(QUEUE_HOST_COMMON_OP_CMD, &arg);
}

drvError_t queue_host_common_queue_uninit(unsigned int dev_id)
{
    struct queue_ioctl_host_common_op arg;
    arg.op_flag = QUEUE_UNINIT;
    arg.devid = dev_id;
    return queue_ioctl(QUEUE_HOST_COMMON_OP_CMD, &arg);
}

drvError_t queue_ctrl_msg_send(struct queue_ctrl_msg_send_stru *ctrl_msg_send)
{
    struct queue_ioctl_ctrl_msg_send arg;
    if (memcpy_s(&arg, sizeof(struct queue_ioctl_ctrl_msg_send), ctrl_msg_send,
                 sizeof(struct queue_ioctl_ctrl_msg_send)) != 0) {
        QUEUE_LOG_ERR("set msg_send args err!");
        return DRV_ERROR_INNER_ERR;
    }
    return queue_ioctl(QUEUE_CTRL_MSG_SEND_CMD, &arg);
}

drvError_t queue_enqueue_cmd(struct queue_enqueue_stru *queue_enqueue)
{
    struct queue_enqueue_stru arg;
    if (memcpy_s(&arg, sizeof(struct queue_ioctl_enqueue), queue_enqueue, sizeof(struct queue_ioctl_enqueue)) != 0) {
        QUEUE_LOG_ERR("queue_enqueue args err!");
        return DRV_ERROR_INNER_ERR;
    }
    return queue_ioctl(QUEUE_ENQUEUE_CMD, &arg);
}

drvError_t queue_get_all_feature(struct queue_ioctl_feature *all_features)
{
    drvError_t ret;
    all_features->user_feature_num = QUEUE_FEATURE_MAX;
    ret = queue_ioctl(QUEUE_GET_ALL_FEATURE_CMD, all_features);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_get_all_feature: ioctl QUEUE_GET_ALL_FEATURE_CMD failed. (ret=%d)\n", ret);
    }
    return ret;
}
