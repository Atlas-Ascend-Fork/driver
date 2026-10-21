/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_MON_MANAGEMENT_H
#define DEV_MON_MANAGEMENT_H

#include <signal.h>
#include "device_monitor_type.h"
#include "dm_common.h"

#define DEV_MSG_DATA_MAX_LENGTH 4096
#define DEV_MONITOR_EVENT_QUE_INVALID (0xFFFFFFFFU)

extern SYSTEM_CB_T *g_sys_main_cb;

typedef struct msg_proc_st {
    SYSTEM_CB_T *cb;
    DM_INTF_S *intf;
    DM_RECV_ST *msg;
#ifdef IAM_CONFIG
    int task_index;
#endif
} MSG_PROC_ST;

void free_msg_buff(DM_RECV_ST **msg);

int init_sys_ctl(void);
void exit_sys_ctl(void);

sig_atomic_t get_sys_running_flag(void);
void set_sys_running_flag(unsigned int value);
void *get_sys_ctl_cb(void);

void *dev_mon_management_task(void *arg);

void dm_command_time_out(const DM_MSG_ST *req, DM_MSG_ST *resp);
void dm_command_not_support(const DM_MSG_ST *req, DM_MSG_ST *resp);
void dm_message_handle(DM_INTF_S *intf, DM_RECV_ST *req, const void *userdata, int userdatalen);

void pid_handler(int sig);

int get_dmp_thread_num(void);
#ifdef IAM_CONFIG
void dev_mon_inc_queue_monitor_thread_cnt(void);
void dev_mon_dec_queue_monitor_thread_cnt(void);
int dev_mon_get_queue_monitor_thread_cnt(void);
#endif

#endif
