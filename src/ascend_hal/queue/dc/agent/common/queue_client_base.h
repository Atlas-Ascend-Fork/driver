/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef QUEUE_CLIENT_BASE_H
#define QUEUE_CLIENT_BASE_H

#include "queue_ioctl.h"
#include "esched_ioctl.h"
#include "ascend_hal_define.h"

#ifdef EMU_ST
#ifndef THREAD
#define THREAD __thread
#endif
#else
#ifndef THREAD
#define THREAD
#endif
#endif

void queue_close_dev(int fd);
void set_g_queue_fd(int fd);
int get_g_queue_fd(void);
int queue_open_dev_base(void);
extern THREAD int g_queue_tgid;

struct queue_ctrl_msg_send_stru {
    struct sched_published_event_info event_info;
    unsigned int devid;
    const void *ctrl_data_addr;
    unsigned int ctrl_data_len;
    unsigned int host_timestamp;
};

struct queue_enqueue_stru {
    struct sched_published_event_info event_info;
    unsigned int devid;
    unsigned int qid;
    unsigned int type;
    int time_out;
    unsigned int iovec_count;
    struct buff_iovec *vector;
};

drvError_t queue_host_common_queue_init(unsigned int dev_id);
drvError_t queue_host_common_queue_uninit(unsigned int dev_id);
drvError_t queue_ctrl_msg_send(struct queue_ctrl_msg_send_stru *ctrl_msg_send);
drvError_t queue_enqueue_cmd(struct queue_enqueue_stru *queue_enqueue);
drvError_t queue_get_all_feature(struct queue_ioctl_feature *all_features);

#endif