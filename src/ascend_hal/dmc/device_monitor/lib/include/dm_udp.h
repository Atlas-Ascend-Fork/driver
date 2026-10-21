/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DM_UDP_H
#define DM_UDP_H

#include <sys/un.h>
#include "dm_msg_intf.h"

#define PORT_TO_SLAVE(port) ((unsigned char)((port) & 0xFF))

typedef struct DM_UDP_ADDR_S {
    int addr_type;
    short channel;
    unsigned int dev_id;
    int service_type;
    struct sockaddr_un sock_addr;
} DM_UDP_ADDR_ST;

#define UPD_RESERVED2_LEN 16
#define MAX_UDP_MSG_BUF 4096

#define UDP_MSG_VERSION 1

#define DM_UDP_MAX_RETRY_TIMES 2
#define DM_UDP_MAX_TETRY_TIME_MS 30000
#define DM_UDP_MAX_TRANS_LEN 512

#define DMP_SOCKET_MANAGEMENT_PERMISSION 0
#define DMP_SOCKET_SERVICE_PERMISSION 0660

#define DMP_UDP_TYPE_MANAGEMENT 0
#define DMP_UDP_TYPE_SERVICE 1

#pragma pack(1)
typedef struct UDP_MSG_S {
    unsigned short fromport;                    /* message source port */
    unsigned short toport;                      /* message destination port */
    unsigned short type;                        /* message type */
    unsigned short version;                     /* message version */
    unsigned short data_len;                    /* message length, starting from data */
    DM_MSG_TYPE msg_type;                       /* message type */
    unsigned int dev_id;                        /* target device id */
    unsigned long sequence;                     /* message sequence number */
    unsigned char reserved2[UPD_RESERVED2_LEN]; /* 16 reserve data */
    unsigned char data[MAX_UDP_MSG_BUF];        /* message date */
} UDP_MSG_T;
#pragma pack()

int dm_udp_init(DM_INTF_S **my_intf, DM_CB_S *cb, DM_MSG_TIMEOUT_HNDL_T timeout_hndl, const DM_ADDR_ST *my_addr,
                const char *my_name, int name_len);

#ifdef STATIC_SKIP
void __format_udp_msg(UDP_MSG_T *udp_msg, DM_MSG_TYPE msg_type, const DM_UDP_ADDR_ST *dst_addr,
                      const DM_UDP_ADDR_ST *src_addr, const DM_MSG_ST *msg, signed long msgid);
#endif

#ifdef STATIC_SKIP
int __dm_udp_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, DM_ADDR_ST *addr, unsigned int addr_len, const DM_MSG_ST *msg,
                  signed long msgid);
#endif

#ifdef STATIC_SKIP
int __dm_udp_settime_send(DM_INTF_S *intf, DM_MSG_TYPE msg_type, DM_ADDR_ST *addr, unsigned int addr_len,
                          const DM_MSG_ST *msg, int retries, unsigned int retry_time_ms, signed long msgid);
#endif

#ifdef STATIC_SKIP
int __dm_udp_recv(DM_INTF_S *intf, int fd, short revents, DM_RECV_ST *irecv);
#endif

#ifdef STATIC_SKIP
int __dm_udp_set_retries(DM_INTF_S *intf, int retries, unsigned int retry_time_ms);
#endif

#ifdef STATIC_SKIP
int __dm_udp_get_retries(DM_INTF_S *intf, int *retries, unsigned int *retry_time_ms);
#endif

#ifdef STATIC_SKIP
int __dm_udp_open(DM_INTF_S *intf);
#endif

#ifdef STATIC_SKIP
void __dm_udp_close(DM_INTF_S *intf);
#endif

#ifdef STATIC_SKIP
int __dm_udp_recv_set_irecv_data(UDP_MSG_T *msg, DM_RECV_ST *irecv, struct sockaddr_un from, socklen_t fromlen,
                                 struct ucred *cred);
#endif

#endif
