/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DM_SMBUS_H
#define DM_SMBUS_H

#include "dm_msg_intf.h"

#define PORT_TO_SLAVE(port) ((unsigned char)((port) & 0xFF))

typedef struct DM_SMBUS_ADDR_S {
    int addr_type;
    short channel;
    short smb_addr;
} DM_SMBUS_ADDR_ST;

#define MAX_SMBUS_MSG_RESERVERED_LEN 16
#define MAX_SMBUS_MSG_BUF 4096
#define MAX_SMBUS_MAX_TRANSLEN 32

#define SMBUS_DMP_WRITE_CMD 0X20

#define SMBUS_DMP_READ_RSP 0X21

#define I2C_SEND_FAILED (-2)
#define RCV_TIME_OUT (-3)

#ifndef ERROR
#define ERROR (-1)
#endif

#define DEV_MEM_NAME "/dev/mem"
#define SM_BUS_CTL_BASE_ADDR 0x130070000

#define RESET_I2C_OFFSET 0xd00
#define RESET_REG_VALUE_OFFSET 0x5d00
#define UNRESET_I2C_OFFSET 0xd04
#define I2C_RESET_MAP_SIZE (4096 * 6)

#define SWITCH_STATUS_GPIO 500

#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif
/* IT determines that the communication address of the MCU is 0xc8 */
#define SMBUS_MCU_ADDR 0x64

#define SMBUS_TIMEOUT_PER_TIME_MS 10
#define SMBUS_MSG_RECV_RETRY_TIME 200

#pragma pack(1)
typedef struct SMBUS_MSG_S {
    unsigned short type;                                   /* message type */
    unsigned short version;                                /* message version */
    unsigned short data_len;                               /* message length, starting from data */
    unsigned char reserved2[MAX_SMBUS_MSG_RESERVERED_LEN]; /* reserved for expansion */
    unsigned char data[MAX_SMBUS_MSG_BUF];                 /* message data */
} SMBUS_MSG_T;

#pragma pack()

int smbus_save_open(void);

int smbus_msg_recv(int fd, unsigned char *data_recv, unsigned int *recv_len);
int smbus_msg_send(int fd, const unsigned char *data, unsigned short data_len);

#endif
