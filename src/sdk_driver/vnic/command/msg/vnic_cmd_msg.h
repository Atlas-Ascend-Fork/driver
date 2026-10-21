/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _VNIC_CMD_MSG_H_
#define _VNIC_CMD_MSG_H_

/* cq/sq must aligned with 128, not within same cacheline */
struct pcivnic_sq_desc {
    u64 data_buf_addr;
    u32 data_len;
    u32 valid;
} __attribute__((aligned(128)));

struct pcivnic_cq_desc {
    u32 sq_head;
    u32 status;
    u32 cq_id;
    u32 valid;
} __attribute__((aligned(128)));

struct pcivnic_s2s_data_info {
    u32 reserve[62]; /* reserve 248 Byte */
    u32 msg_len;     /* must be here, otherwise, there will be version compatibility issues */
    unsigned char data[];
};

#define PCIVNIC_CTRL_MSG_TYPE_EXCHANGE_NET_INFO 0
#define PCIVNIC_CTRL_MSG_TYPE_GET_STAT 1
#define PCIVNIC_CTRL_MSG_TYPE_RIGISTER_NETDEV 2
#define PCIVNIC_CTRL_MSG_TYPE_INSTANCE 3
#define PCIVNIC_CTRL_MSG_TYPE_SYNC_STATUS 4
#define PCIVNIC_CTRL_MSG_TYPE_MAX 5

#define PCIVNIC_CTRL_MSG_RESERVE_NUM 3

#define PCIVNIC_CTRL_MSG_ERR_CODE_NONE 0
#define PCIVNIC_CTRL_MSG_ERR_CODE_UNSUPPORT 95

struct pcivnic_ctrl_msg_head {
    u32 msg_type;
    u32 host_udevid;
    int error_code; // when send ctrl msg finish, error_code must be checked
    u32 reserve[PCIVNIC_CTRL_MSG_RESERVE_NUM];
};

#define PCIVNIC_ETH_ALEN 6 // PCIVNIC_ETH_ALEN must be the same as KA_ETH_ALEN

struct pcivnic_ctrl_msg_net_info {
    struct pcivnic_ctrl_msg_head head;
    unsigned char mac[PCIVNIC_ETH_ALEN];
    u32 ip;
};

struct pcivnic_ctrl_msg_register_netdev {
    struct pcivnic_ctrl_msg_head head;
};

struct pcivnic_ctrl_msg_dev_instance {
    struct pcivnic_ctrl_msg_head head;
};

#define VNIC_STAT_MSG_MAX_LEN (60 * 1024)
#define VNIC_MM_PAGE_SIZE 4096 // need equal to npu's KA_MM_PAGE_SIZE
struct pcivnic_ctrl_msg_get_stat {
    struct pcivnic_ctrl_msg_head head;
    u32 msg_len;
    char msg[VNIC_MM_PAGE_SIZE];
};

#endif
