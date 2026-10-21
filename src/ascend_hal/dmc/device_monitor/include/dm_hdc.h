/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DM_HDC_H
#define DM_HDC_H

#include "dm_common.h"
#include "ascend_hal.h"
#define DM_HDC_RECV_BUF_LEN 4096
#define MAX_HDC_CLIENT 64
#define MAX_HDC_RECV_RETRY 10

#ifndef OK
#define OK 0
#endif

#define HDC_SERVER_TYPE 1
#define HDC_CLIENT_TYPE 0
#define RETRY_COUNT 3

#define DSMI_MSG_TIMEOUT 120000

#pragma pack(1)
typedef struct HDC_MSG_S {
    unsigned long long session;
    int peer_devid;
    int src_devid;
    signed long long msgid;
    DM_MSG_TYPE msg_type;
    unsigned short data_len;
    unsigned char data[DM_MSG_DATA_MAX];
} HDC_MSG_ST;
#pragma pack()

#define HDCMSG_HEAD_SIZE (sizeof(HDC_MSG_ST) - DM_MSG_DATA_MAX)

#ifndef return_val_do_info
#define return_val_do_info(expr, val, msg, info) \
    do {                                         \
        if ((expr)) {                            \
            free(msg);                           \
            msg = NULL;                          \
            info;                                \
            return val;                          \
        }                                        \
    } while (0)
#endif

enum HDC_ADDR_STATUS {
    HDC_ADDR_CLOSE,
    HDC_ADDR_WORK
};

typedef struct DM_HDC_ADDR_S {
    int addr_type;
    short channel;
    int hdc_type; /* 0:client,1:server */
    int peer_node;
    int peer_devid;
    enum HDC_ADDR_STATUS hdc_work_status; // recode is open or close, 0 : close , 1: work
    unsigned long long session;
    int dev_id;
} DM_HDC_ADDR_ST;

typedef struct DM_HDC_CB_S {
    void *cb;
} DM_HDC_CB_ST;

typedef struct DM_HDC_TASK_S {
    void *intf;
    void *session;
    DM_MSG_TYPE msg_type;
} DM_HDC_TASK_ST;

int dm_hdc_init(DM_INTF_S **my_intf, DM_CB_S *cb, DM_MSG_TIMEOUT_HNDL_T timeout_hndl, DM_ADDR_ST *my_addr,
                const char *my_name, int name_len);

#endif
