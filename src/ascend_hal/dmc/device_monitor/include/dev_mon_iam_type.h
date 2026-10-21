/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_MON_IAM_TYPE_H
#define DEV_MON_IAM_TYPE_H

#include "device_monitor_type.h"

#define IAM_CLIENT 0
#define IAM_SERVER 1

#define SYS_OK 0
#define SYS_ERROR (-1)

#define STATE_MANAGE_TYPE 0
#define CONFIG_MANAGE_TYPE 1
#define FAULT_MANAGE_TYPE 2
#define FIRMWARE_UPDATE_TYPE 3
#define CRITICAL_STATE_TYPE 4
#define MAX_FILE_TYPE 5
#define UNDEFINED_TYPE 6

#define MSG_QUEUE_NUM_MAX 12
#define IAM_CLIENT_APP_NAME_LEN 200
#define IAM_MSG_QUEUE_SIZE 32
typedef struct DM_IAM_ADDR_S {
    int addr_type;
    short channel;
    unsigned int src_devid;
    int iam_type;
    int msg_type;
    int file_type;
    signed long long msgid;
    unsigned int rpc_session_id;
    unsigned int rpc_req_id;
    int iam_res_fd;
    void *iam_queue;
    unsigned char app_name_len;
    char app_name[IAM_CLIENT_APP_NAME_LEN];
} DM_IAM_ADDR_ST;

struct iam_msg_queue {
    char app_name[IAM_CLIENT_APP_NAME_LEN];
    unsigned char app_name_len;
    unsigned long msg_queue_id;
    unsigned long long last_enqueue_time;
};

struct IAM_MSG_TASK_CB {
    SYSTEM_CB_T *sys_cb;
    int process_request_thread_cnt;
    int task_index;
};

#pragma pack(1)
#define DM_IMA_MSG_DATA_MAX 4096
typedef struct IAM_MSG_S {
    unsigned int src_devid;
    unsigned int iam_type;
    int file_type;
    int msg_type;
    signed long long msgid;
    unsigned int rpc_session_id;
    unsigned int rpc_req_id;
    int iam_res_fd;
    void *iam_queue;
    unsigned char app_name_len;
    char app_name[IAM_CLIENT_APP_NAME_LEN];
    unsigned short data_len;
    unsigned char data[DM_IMA_MSG_DATA_MAX];
} IAM_MSG_ST;
#define IAMMSG_HEAD_SIZE (sizeof(IAM_MSG_ST) - DM_IMA_MSG_DATA_MAX)
#pragma pack()
#endif
