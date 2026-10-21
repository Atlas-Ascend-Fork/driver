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

#ifndef _HDCDRV_CMD_MSG_H_
#define _HDCDRV_CMD_MSG_H_

#define HDCDRV_CTRL_MEM_REGISTER_MAX_LEN (2 * 1024 * 1024) /* ctrl mem register mem len check */
#define HDCDRV_CTRL_MEM_SEND_MAX_LEN (252 * 1024)          /* ctrl mem send len check */
#define HDCDRV_MEM_MIN_LEN (4 * 1024)
#define HDCDRV_MEM_CACHE_LIMIT (64 * 1024)
#define HDCDRV_MEM_MAX_NUM (HDCDRV_MEM_MAX_LEN / HDCDRV_MEM_MIN_LEN)

#define HDCDRV_DEFAULT_PM_FID 0
#define HDCDRV_DEFAULT_LOCAL_FID 0

struct hdcdrv_dma_mem {
    unsigned long long addr;
    unsigned int len;
    unsigned int resv;
};

struct hdcdrv_ctrl_msg_sync_mem_info {
    int type;
    int error_code;
    int flag;
    int phy_addr_num;
    int mem_type;
    unsigned int alloc_len;
    unsigned long long hash_va;
    long long pid;
#ifdef CFG_FEATURE_HDC_REG_MEM
    unsigned int align_size;
    unsigned int register_offset;
    unsigned long long user_va;
#endif
    unsigned int reserved[8];
    struct hdcdrv_dma_mem mem[];
};

enum hdc_dfx_print_type {
    HDC_DFX_PRINT_IN_SYSFS = 0,
    HDC_DFX_PRINT_IN_LOG = 1,
};

struct hdcdrv_sysfs_ctrl_msg_head {
    int type;
    int error_code;
    unsigned int msg_len;
    unsigned int para;
    enum hdc_dfx_print_type print_type;
};

struct hdcdrv_sysfs_ctrl_msg {
    struct hdcdrv_sysfs_ctrl_msg_head head;
    char reserved[4];
    char data[];
};

/* ctrl msg */
struct hdcdrv_ctrl_msg_sync {
    int segment;
    int peer_dev_id;
};

struct hdcdrv_ctrl_msg_connect {
    int service_type;
    int client_session;
    unsigned int fast_chan_id;
    unsigned int normal_chan_id;
    int run_env;
    int euid;
    int uid;
    int root_privilege;
    unsigned int fid;
    unsigned int unique_val;
    unsigned long long local_pid;
    unsigned long long peer_pid;
};

struct hdcdrv_ctrl_msg_connect_reply {
    int client_session;
    int server_session;
    int run_env;
    unsigned int fid;
    unsigned int unique_val;
    unsigned long long local_pid;
    unsigned long long peer_pid;
    unsigned int fast_chan_id;
    unsigned int normal_chan_id;
};

struct hdcdrv_ctrl_msg_close {
    int local_session;
    int remote_session;
    int session_close_state;
    unsigned int fid;
    unsigned int unique_val;
};

struct hdcdrv_ctrl_msg_chan_set {
    unsigned int normal_chan_num;
};

struct hdcdrv_ctrl_msg_get_stat {
    unsigned int msg_len;
    unsigned int para;
};

struct hdcdrv_ctrl_msg_get_feature_info {
    unsigned int feature_type;
    unsigned int is_support;
    unsigned int is_npu_depend;
    unsigned int version;
};

struct hdcdrv_ctrl_msg {
    int type;
    int error_code;
    union {
        struct hdcdrv_ctrl_msg_sync sync_msg;
        struct hdcdrv_ctrl_msg_connect connect_msg;
        struct hdcdrv_ctrl_msg_connect_reply connect_msg_reply;
        struct hdcdrv_ctrl_msg_close close_msg;
        struct hdcdrv_ctrl_msg_chan_set chan_set_msg;
        struct hdcdrv_ctrl_msg_get_stat stat;
        struct hdcdrv_ctrl_msg_get_feature_info feature_info;
    };
} __attribute__((aligned(128)));

/* dma payload is 128 bytes; cq/sq must aligned with 128, not within same cacheline */
/* sq cq description */
struct hdcdrv_sq_desc {
    int local_session;
    int remote_session;
    unsigned long long src_data_addr;
    unsigned long long dst_data_addr;
    unsigned long long src_ctrl_addr;
    unsigned long long dst_ctrl_addr;
    unsigned int offset; // src data addr seque offset, used for CFG_FEATURE_OVER_XCOM
    int data_len;
    int ctrl_len;
    unsigned int src_pid;
    unsigned int src_fid;
#ifdef CFG_FEATURE_PFSTAT
    unsigned int trans_id;
#endif
    unsigned int inner_checker;
    unsigned int desc_crc;
    unsigned int valid;
} __attribute__((aligned(128)));

struct hdcdrv_cq_desc {
    int status; /* if status > 0  is pcie err, other hdc err */
    unsigned int sq_head;
    int session;
    unsigned int desc_crc;
    unsigned int valid;
} __attribute__((aligned(128)));

#define HDCDRV_SQ_RESV_LEN 2
#define HDCDRV_SQ_DESC_SIZE sizeof(struct hdcdrv_sq_desc)
#define HDCDRV_SQ_DESC_CRC_LEN ka_offsetof(struct hdcdrv_sq_desc, desc_crc)
#define HDCDRV_CQ_DESC_SIZE sizeof(struct hdcdrv_cq_desc)
#define HDCDRV_CQ_DESC_CRC_LEN ka_offsetof(struct hdcdrv_cq_desc, desc_crc)
#endif
