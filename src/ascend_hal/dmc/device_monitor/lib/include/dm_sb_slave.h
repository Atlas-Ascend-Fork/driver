/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DM_SB_SLAVE_H
#define DM_SB_SLAVE_H

#include <netinet/in.h>
#include "dm_msg_intf.h"
#define DEV_SB_SLAVE "/dev/i2c0_slave"

#define PORT_TO_SLAVE(port) ((unsigned char)((port) & 0xFF))

typedef struct DM_SB_SLAVE_ADDR_S {
    int addr_type;
    short channel;
} DM_SB_SLAVE_ADDR_ST;

#define SB_SLAVE_MSG_RESERVED2_LEN 16
#define MAX_SB_SLAVE_MSG_BUF 4096

#define MAX_SB_SLAVE_MAX_TRANSLEN 32

#pragma pack(1)
typedef struct SB_SLAVE_MSG_S {
    unsigned short type;                                 /* message type */
    unsigned short version;                              /* message version */
    unsigned short data_len;                             /* message length, starting from data */
    DM_MSG_TYPE msg_type;                                /* message type */
    unsigned long sequence;                              /* message sequence number */
    unsigned char reserved2[SB_SLAVE_MSG_RESERVED2_LEN]; /* reserved for expansion */
    unsigned char data[MAX_SB_SLAVE_MSG_BUF];            /* message data */
} SB_SLAVE_MSG_T;

#pragma pack()

int dm_sb_slave_init(DM_INTF_S **my_intf, DM_CB_S *cb, DM_MSG_TIMEOUT_HNDL_T timeout_hndl, const DM_ADDR_ST *my_addr,
                     const char *my_name, int name_len);

#ifdef STATIC_SKIP
void __format_sb_slave_msg(SB_SLAVE_MSG_T *sb_slave_msg, DM_MSG_TYPE msg_type, const DM_SB_SLAVE_ADDR_ST *dst_addr,
                           const DM_SB_SLAVE_ADDR_ST *src_addr, const DM_MSG_ST *msg, signed long msgid);
#endif

#ifdef STATIC_SKIP
int __dm_sb_slave_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, const DM_ADDR_ST *addr, unsigned int addr_len,
                       const DM_MSG_ST *msg, signed long msgid);
#endif

#ifdef STATIC_SKIP
int __dm_sb_slave_settime_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, const DM_ADDR_ST *addr, unsigned int addr_len,
                               const DM_MSG_ST *msg, int retries, unsigned int retry_time_ms, signed long msgid);
#endif

#ifdef STATIC_SKIP
int __dm_sb_slave_recv(DM_INTF_S *intf, int fd, short revents, DM_RECV_ST *irecv);
#endif

#ifdef STATIC_SKIP
int __dm_sb_slave_set_retries(DM_INTF_S *intf, int retries, unsigned int retry_time_ms);
#endif

#ifdef STATIC_SKIP
int __dm_sb_slave_get_retries(DM_INTF_S *intf, int *retries, unsigned int *retry_time_ms);
#endif

#ifdef STATIC_SKIP
int __dm_sb_slave_open(DM_INTF_S *intf);
#endif

#ifdef STATIC_SKIP
void __dm_sb_slave_close(DM_INTF_S *intf);
#endif

#endif
