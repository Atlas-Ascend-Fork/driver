/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DM_COMMON_H
#define DM_COMMON_H

#include "dm_list.h"
#include "dm_poller.h"

#define DM_MSG_MODULE "dm_msg"
#define DM_MAX_ADDR_SIZE 300
#define DM_MSG_DATA_MAX 4096
#define FDS_INTEGER_NUMBAR 8

#define SELFLOOP_INTF "selfloop"
#define DM_LOOP_ADDR_TYPE 0x61 /* OEM addr type for loopback */
#define DM_LOOP_CHANNEL 0x01

#define DM_UDP_ADDR_TYPE 0x62 /* OEM addr type for socket */
#define DM_UDP_CHANNEL 0x02

#define DM_HDC_ADDR_TYPE 0x63
#define DM_HDC_CHANNEL 0x03

#define DM_SMBUS_ADDR_TYPE 0x64
#define DM_SMBUS_CHANNEL 0x04

#define DM_SMBUS_MCU_ADDR 0x64

#define DM_SB_SLAVE_ADDR_TYPE 0x65
#define DM_SB_SLAVE_CHANNEL 0x05

#define DM_IPC_ADDR_TYPE 0x66
#define DM_IPC_CHANNEL 0x06

#define DM_IAM_ADDR_TYPE 0x67
#define DM_IAM_CHANNEL 0x07

#define DM_UDP_MANAGEMENT_INTF "dm_udp_management"
#define DM_UDP_SERVICE_INTF "dm_udp_service"
#define DM_UDP_INTF "dm_udp"
#define DM_HDC_INTF "dm_hdc"
#define DM_SMBUS_INTF "dm_smbus"
#define DM_SB_SLAVE_INTF "dm_sb_slave"
#define DM_IPC_INTF "dm_ipc"
#define DM_IAM_INTF "dm_iam"
#define ROOT_PRIV 1
#define DMP_SERVER 1
#define GUEST_PROP 0
#define ADMIN_PROP 1
#define CONTAINER_PROP 2
#define NOT_ROOT 0

#define ROOT_PROP 0
#define USER_PROP 0xff
#define UDP_MSG_TYPE 0

#define CTL_LOG_OUT 600

#define DM_INTF_NAME_LEN 32
#define DM_CLIENT 0
#define DM_SERVER 1
#define DM_RESPONSE_RECV_TYPE 0x10
#define DM_CMD_RECV_TYPE 0x11
#define DM_COMMON_INIT_EXIT_DELAY 200000
#define DEV_MON_ROOT_STACK_SIZE 0x20000

typedef struct ipmi_addr {
    int addr_type;
    short channel;
    char data[DM_MAX_ADDR_SIZE];
} DM_ADDR_ST;

typedef enum dm_msg_type {
    REQUEST_MSG = 0,
    RESPONSE_MSG
} DM_MSG_TYPE;

typedef struct dm_msg {
    unsigned short data_len;
    unsigned char *data;
} DM_MSG_ST;

typedef struct dm_recv {
    int recv_type;
    int dev_id;
    unsigned int vfid;
    unsigned char session_prop;
    int host_root;
    unsigned char *addr;
    unsigned int addr_len;
    long msgid;
    DM_MSG_ST msg;
} DM_RECV_ST;

#define DM_INTF_PIPE_WR_FAILED_CNT 5
#define DM_INTF_PIPE_WR_FAILED_MAX 8
typedef struct intf_statistic_item {
    unsigned int pipe_wr_fail;
    unsigned int msg_handle_timeout;
} DM_INTF_STATS;

typedef void (*DM_MSG_TIMEOUT_HNDL_T)(const DM_MSG_ST *req, DM_MSG_ST *resp);
typedef struct dm_intf DM_INTF_S;
/* dm msg channel handle interface */
struct dm_intf {
    char name[DM_INTF_NAME_LEN];
    DM_ADDR_ST my_addr;
    void *channel_cb;
    int rfd;
    int wfd;
    int retries;
    unsigned int retry_time_ms;
    unsigned int max_trans_len;
    /* receive msg function of interface */
    int (*recv_msg)(DM_INTF_S *intf, int fd, short revents, DM_RECV_ST *recv);

    /* send msg function of interface */
    int (*send_msg)(DM_INTF_S *intf, DM_MSG_TYPE msg_type, DM_ADDR_ST *addr, unsigned int addr_len,
                    const DM_MSG_ST *msg, long msgid);

    int (*send_msg_settime)(DM_INTF_S *intf, DM_MSG_TYPE msg_type, DM_ADDR_ST *addr, unsigned int addr_len,
                            const DM_MSG_ST *msg, int retries, unsigned int retry_time_ms, long msgid);
    int (*set_retries)(DM_INTF_S *intf, int retries, unsigned int retry_time_ms);

    int (*get_retries)(DM_INTF_S *intf, int *retries, unsigned int *retry_time_ms);

    DM_MSG_TIMEOUT_HNDL_T timeout_hndl;

    void (*close)(DM_INTF_S *intf);

    void *dm_cb;

    DM_INTF_STATS stats;
};

typedef void (*DM_MSG_CMD_HNDL_T)(DM_INTF_S *intf, DM_RECV_ST *recv, void *user_data, int data_len);

/* cmd unsupported handle func */
typedef void (*DM_MSG_UNSUP_HNDL_T)(const DM_MSG_ST *req, DM_MSG_ST *resp);

typedef struct dm_cb {
    DM_MSG_UNSUP_HNDL_T unsup_hndl;
    POLLER_T *intf_poller; /* poller */
    LIST_T *intf_list;     /* msg channel interface list */
    LIST_T *pending_list;  /* meg penging list */
    LIST_T *cmd_reg_list;  /* cmd register list */
} DM_CB_S;

typedef struct PENDING_REQ_S {
    DM_INTF_S *intf;
    DM_ADDR_ST addr;
    unsigned int addr_len;
    DM_MSG_ST msg;
    unsigned char msg_data[DM_MSG_DATA_MAX];
    DM_MSG_CMD_HNDL_T rsp_hndl;
    void *user_data;
    int data_len;

    int retries;
    POLLER_TIMER_ID_T timer_id;
} PENDING_REQ_T;

typedef struct DM_CMD_REGISTER_S {
    DM_MSG_CMD_HNDL_T hndl;
    void *user_data;
    int data_len;
} DM_CMD_REGISTER_T;

#endif
