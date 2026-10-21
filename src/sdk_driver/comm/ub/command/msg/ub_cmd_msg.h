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

#ifndef _UB_CMD_MSG_H_
#define _UB_CMD_MSG_H_

#include "ubcore_types.h"
#include "comm_msg_chan_cmd.h"

#ifdef CFG_ENV_HOST
#define UBDRV_COMMON_CLIENT_CNT DEVDRV_COMMON_MSG_TYPE_MAX
#else
#define UBDRV_COMMON_CLIENT_CNT DEVDRV_COMMON_MSG_TYPE_MAX
#endif

#define UBDRV_MSG_DESC_RSV 7
#define UBDRV_CREATE_NON_TRANS_CMD_RSV 3
#define UBDRV_FREE_NON_TRANS_CMD_RSV 3
#define UBDRV_CREATE_URMA_CHAN_CMD_RSV 2
#define JETTY_EXCHANGE_DATA_RSV 5
#define UBDRV_JETTY_EXCHANGE_DATA_RSV 3
#define UBDRV_MSG_CHAN_INFO_RSV 8
#define UBDRV_MSG_CHAN_RX_INFO_RSV 32
#define UBDRV_MSG_CHAN_DFX_INFO_RSV 28
#define UBDRV_COMMON_MSG_STAT_RSV 7
#define UBDRV_CHAN_DFX_CMD_RSV 3
#define UBDRV_VERSION_LEN 64
#define UBDRV_LINK_EXCHANGE_DATA_RSV 1

enum ubdrv_msg_chan_mode {
    UBDRV_MSG_CHAN_FOR_LINK = 0,
    UBDRV_MSG_CHAN_FOR_ADMIN,
    UBDRV_MSG_CHAN_FOR_NON_TRANS,
    UBDRV_MSG_CHAN_FOR_RAO,
    UBDRV_MSG_CHAN_FOR_URMA,
    UBDRV_MSG_CHAN_FOR_MAX,
};

enum ubdrv_link_request_type {
    UBDRV_LINK_EXCHANGE_DATA = 0,
    UBDRV_LINK_NOTIFIY_ONLINE,
    UBDRV_LINK_MAX
};

// =============================================================================
// @struct ascend_ub_msg_desc
// @compatibility: link msg version 1.0
// @review_required: module owner and se
// =============================================================================
struct ascend_ub_msg_desc {
    u32 status;
    u32 seg_id;
    u32 in_data_len;
    u32 out_data_len;
    u32 real_data_len;
    u32 msg_num;
    u32 time_out;
    u32 client_type; // common msg type
    u32 opcode;
    u32 rsv[UBDRV_MSG_DESC_RSV];
    char user_data[];
};

// =============================================================================
// @struct jetty_exchange_data
// @compatibility: link msg version 1.0
// @review_required: module owner and se
// =============================================================================
struct jetty_exchange_data {
    u32 dev_id;
    u32 token_value;
    u32 id;
    u32 reserved[JETTY_EXCHANGE_DATA_RSV];
    struct ubcore_eid_info eid;
    struct ubcore_seg seg;
};

struct ubdrv_create_non_trans_cmd {
    u32 msg_type; /* enum devdrv_msg_client_type */
    u32 chan_id;
    u32 sq_size;
    u32 cq_size;
    u16 sq_depth;
    u16 cq_depth;
    u32 reserved[UBDRV_CREATE_NON_TRANS_CMD_RSV];
    struct jetty_exchange_data s_jetty_data;
    struct jetty_exchange_data r_jetty_data;
    enum ubdrv_msg_chan_mode chan_mode;
};

struct ubdrv_free_non_trans_cmd {
    u32 chan_id;
    u32 reserved[UBDRV_FREE_NON_TRANS_CMD_RSV];
    enum ubdrv_msg_chan_mode chan_mode;
};

struct ubdrv_create_urma_chan_cmd {
    u32 dev_id;
    u32 chan_id;
    u32 reserved[UBDRV_CREATE_URMA_CHAN_CMD_RSV];
    struct jetty_exchange_data jetty_data;
};

struct ubdrv_jetty_exchange_data {
    struct jetty_exchange_data admin_jetty_info;
    u32 ue_idx;
    u32 reserved[UBDRV_JETTY_EXCHANGE_DATA_RSV];
};

enum ubdrv_proc_admin_sub_cmd {
    UBDRV_PROCFS_ADMIN_CHAN_CMD = 0,
    UBDRV_PROCFS_NONTRANS_CHAN_CMD,
    UBDRV_PROCFS_COMMON_CHAN_CMD,
    UBDRV_PROCFS_ENABLE_PROF_CMD,
    UBDRV_PROCFS_DISABLE_PROF_CMD,
    UBDRV_PROC_TYPE_MAX
};

struct ubdrv_msg_chan_stat {
    // chan info
    u32 dev_id;
    u32 chan_id;
    u8 rsv[UBDRV_MSG_CHAN_INFO_RSV];

    // tx dfx info
    u64 tx_total;
    u64 tx_post_send_err;
    u64 tx_cqe;
    u64 tx_cqe_timeout; // local send cqe timeout
    u64 tx_poll_cqe_err;
    u64 tx_cqe_status_err;
    u64 tx_rebuild;
    u64 tx_recv_cqe_timeout; // poll remote reply data cqe timeout
    u64 tx_recv_cqe;
    u64 tx_recv_poll_cqe_err;
    u64 tx_recv_cqe_status_err;
    u64 tx_recv_data_err; // remote reply data content check err
    u64 tx_start_stamp;
    u64 tx_post_stamp;
    u64 tx_wait_reply_stamp;
    u64 tx_recv_stamp;

    // rx dfx info
    u64 rx_total; // recv msg jfce call cnt
    u64 rx_work;  // recv msg process work cnt
    u64 rx_poll_cqe_err;
    u64 rx_cqe_status_err;
    u64 rx_process_err;
    u64 rx_null_process;
    u64 rx_work_finish;
    u64 rx_post_jfr_err;
    u64 rx_tx_post_err;
    u64 rx_tx_poll_cqe_err;
    u64 rx_tx_cqe_timeout;
    u64 rx_tx_cqe;
    u64 rx_tx_cqe_status_err;
    u64 rx_rebuild;
    u64 rx_stamp;
    u64 rx_work_stamp;
    u32 rx_max_time;
    u32 rx_work_max_time;
    u8 rx_rsv[UBDRV_MSG_CHAN_RX_INFO_RSV];

    // chan dfx info
    u32 jfae_err;
    u32 tx_jfr_id;
    u32 tx_jfr_jfc_id;
    u32 tx_jfs_id;
    u32 tx_jfs_jfc_id;
    u32 rx_jfr_id;
    u32 rx_jfr_jfc_id;
    u32 rx_jfs_id;
    u32 rx_jfs_jfc_id;
    u8 reserved[UBDRV_MSG_CHAN_DFX_INFO_RSV];
};

struct ubdrv_common_msg_stat {
    u64 tx_total;
    u64 tx_check_data_err;
    u64 tx_chan_null_err;
    u64 tx_enodev_err;
    u64 tx_chan_err;
    u64 rx_total;
    u64 rx_cb_process_err;
    u64 rx_null_cb_err;
    u64 rx_finish;
    u64 reserved[UBDRV_COMMON_MSG_STAT_RSV];
};

#pragma pack(4)
struct ubdrv_chan_dfx_cmd {
    enum ubdrv_proc_admin_sub_cmd opcode;
    u32 chan_id; // msg chan id
    u32 reserved[UBDRV_CHAN_DFX_CMD_RSV];
    union {
        struct ubdrv_msg_chan_stat chan;
        struct ubdrv_common_msg_stat client[UBDRV_COMMON_CLIENT_CNT];
    } stat;
};
#pragma pack()

struct ubdrv_id_info {
    u16 device_id;
    u16 vendor_id;
    u16 module_vendor_id;
    u16 module_id;
};

// =============================================================================
// @struct ubdrv_link_exchange_data
// @compatibility: link msg version 1.0
// @review_required: module owner and se
// =============================================================================
struct ubdrv_link_exchange_data {
    enum ubdrv_link_request_type opcode;
    u32 dev_id;
    u32 module_id : 16; // bit0~15:module_id
    u32 slot_id : 16;   // bit16~31:slot_id
    u32 host_token;
    char version[UBDRV_VERSION_LEN];
    struct jetty_exchange_data recv_admin_info;
    struct jetty_exchange_data send_admin_info;
    struct jetty_exchange_data client_info;
    struct ubdrv_id_info id_info;
    u32 chip_type;
    u32 reserved[UBDRV_LINK_EXCHANGE_DATA_RSV];
};

#endif