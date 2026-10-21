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

#ifndef _HDCDRV_DFX_H_
#define _HDCDRV_DFX_H_

#include "ka_memory_pub.h"
#include "hdcdrv_core_com.h"

#define HDCDRV_DFX_PRINT_LIMIT_CNT 100U
#define HDCDRV_SHOW_PKT_LEN 128
#define HDCDRV_SHOW_PKT_RX_NUM 50
#define HDCDRV_DFX_MAX_PROCESS_NUM 64 // only for dfx, max support 64 process
#define HDCDRV_TIME_COMPARE_2MS 2
#define HDCDRV_TIME_COMPARE_4MS 4

struct hdcdrv_latency_info {
    u64 send_timestamp;
    u64 wait_sq_timestamp;
    u64 dma_copy_timestamp;
    u64 rx_task_timestamp;
    u64 sq_dma_timestamp;
    u64 cq_dma_timestamp;
};

struct hdcdrv_dbg_stats {
    unsigned long long hdcdrv_msg_chan_send1;
    unsigned long long hdcdrv_rx_msg_notify1;
    unsigned long long hdcdrv_rx_msg_notify_task_check1;
    unsigned long long hdcdrv_rx_msg_notify_task_check2;
    unsigned long long hdcdrv_rx_msg_notify_task_delay_over2ms;
    unsigned long long hdcdrv_rx_msg_notify_task_delay_over4ms;
    unsigned long long hdcdrv_rx_msg_notify_task_delay_new;
    unsigned long long hdcdrv_rx_msg_notify_task_delay_max;
    unsigned long long hdcdrv_rx_msg_notify_task1;
    unsigned long long hdcdrv_rx_msg_notify_task2;
    unsigned long long hdcdrv_rx_msg_notify_task3;
    unsigned long long hdcdrv_rx_msg_notify_task4;
    unsigned long long hdcdrv_rx_msg_notify_task5;
    unsigned long long hdcdrv_rx_msg_notify_task6;
    unsigned long long hdcdrv_rx_msg_notify_task7;
    unsigned long long hdcdrv_rx_msg_notify_task8;
    unsigned long long hdcdrv_rx_msg_notify_task9;
    unsigned long long hdcdrv_normal_dma_copy1;
    unsigned long long hdcdrv_rx_msg_callback1;
    unsigned long long hdcdrv_rx_msg_callback2;
    unsigned long long hdcdrv_rx_msg_callback3;
    unsigned long long hdcdrv_rx_msg_task_check1;
    unsigned long long hdcdrv_rx_msg_task_check2;
    unsigned long long hdcdrv_msg_chan_recv_task1;
    unsigned long long hdcdrv_msg_chan_recv_task2;
    unsigned long long hdcdrv_msg_chan_recv_task3;
    unsigned long long hdcdrv_msg_chan_recv_task4;
    unsigned long long hdcdrv_msg_chan_recv_task5;
    unsigned long long hdcdrv_msg_chan_recv_task6;
    unsigned long long hdcdrv_msg_chan_recv_task7;
    unsigned long long hdcdrv_msg_chan_recv_task8;
    unsigned long long hdcdrv_tx_finish_notify1;
    unsigned long long hdcdrv_tx_finish_task_check1;
    unsigned long long hdcdrv_tx_finish_task_check2;
    unsigned long long hdcdrv_tx_finish_notify_task_delay_over2ms;
    unsigned long long hdcdrv_tx_finish_notify_task_delay_over4ms;
    unsigned long long hdcdrv_tx_finish_notify_task_delay_new;
    unsigned long long hdcdrv_tx_finish_notify_task_delay_max;
    unsigned long long hdcdrv_tx_finish_notify_task1;
    unsigned long long hdcdrv_tx_finish_notify_task2;
    unsigned long long hdcdrv_tx_finish_notify_task3;
    unsigned long long hdcdrv_tx_finish_notify_task4;
    unsigned long long hdcdrv_tx_finish_notify_task5;
    unsigned long long hdcdrv_tx_finish_notify_task6;
    unsigned long long hdcdrv_tx_finish_notify_task7;
    unsigned long long hdcdrv_tx_finish_notify_task8;
    unsigned long long hdcdrv_tx_finish_notify_session_no_update;
    unsigned long long hdcdrv_tx_finish_notify_sq_head_no_update;
    unsigned long long hdcdrv_mem_avail1;
    unsigned long long hdcdrv_recv_data_times;
    unsigned long long hdcdrv_wait_mem_normal;
    unsigned long long hdcdrv_wait_mem_fifo_full;
    unsigned long long hdcdrv_msg_chan_send2;
};

enum dbg_time_op {
    DBG_TIME_OP_SEND = 0,
    DBG_TIME_OP_RECV,
    DBG_TIME_OP_CONN,
    DBG_TIME_OP_ACCEPT,
    DBG_TIME_OP_MAX
};

enum conn_timestamp {
    CONN_TIME_BF_ALLOC_SESSION = 0,
    CONN_TIME_BF_SEND_CONN,
    CONN_TIME_RECV_CONN_REPLY,
    CONN_TIME_AFT_WAKE_UP_WQ_CONN,
    CONN_TIME_MAX
};

enum accept_timestamp {
    ACCEPT_TIME_RECV_CONN = 0,
    ACCEPT_TIME_AFT_ALLOC_SESSION,
    ACCEPT_TIME_BF_SEND_CONN_REPLY,
    ACCEPT_TIME_MAX
};

enum conn_timetaken {
    CONN_TIME_TAKEN_ALLOC_SESSION = 0,
    CONN_TIME_TAKEN_RECV_CONN_REPLY,
    CONN_TIME_TAKEN_WAKE_UP_WQ_CONN,
    CONN_TIME_TAKEN_MAX
};

enum accept_timetaken {
    ACCEPT_TIME_TAKEN_ALLOC_SESSION = 0,
    ACCEPT_TIME_TAKEN_WAKE_UP_WQ_CONN_AVAIL,
    ACCEPT_TIME_TAKEN_MAX
};

enum tx_timestamp {
    TX_TIME_BF_ALLOC_MEM = 0,
    TX_TIME_BF_COPY_TX_DATA_FROM_USER,
    TX_TIME_AFT_COPY_TX_DATA_FROM_USER,
    TX_TIME_BF_FILL_SQ_DESC,
    TX_TIME_AFT_COPY_SQ_DESC,
    TX_TIME_RECV_CQ_DESC,
    TX_TIME_AFT_UPDATE_SQ_HEAD,
    TX_TIME_WAKE_UP_SEND_WAIT,
    TX_TIME_MAX
};

enum rx_timestamp {
    RX_TIME_RECV_SQ_DESC = 0,
    RX_TIME_RECV_RX_DATA,
    RX_TIME_BF_SEND_CQ_DESC,
    RX_TIME_BF_COPY_RX_DATA_TO_USER,
    RX_TIME_AFT_COPY_RX_DATA_TO_USER,
    RX_TIME_MAX
};

enum tx_timetaken {
    TX_TIME_TAKEN_ALLOC_TX_MEM = 0,
    TX_TIME_TAKEN_COPY_TX_DATA_FROM_USER,
    TX_TIME_TAKEN_WAIT_SQ_NOT_FULL,
    TX_TIME_TAKEN_COPY_SQ_DESC,
    TX_TIME_TAKEN_PEER_RECV_DATA,
    TX_TIME_TAKEN_UPDATE_SQ_HEAD,
    TX_TIME_TAKEN_WAKE_UP_SEND_WAIT,
    TX_TIME_TAKEN_MAX
};

enum rx_timetaken {
    RX_TIME_TAKEN_DMA_COPY_RX_BUF = 0,
    RX_TIME_TAKEN_INSERT_SESSION_LIST,
    RX_TIME_TAKEN_USER_GET_DATA,
    RX_TIME_TAKEN_COPY_RX_DATA_TO_USER,
    RX_TIME_TAKEN_MAX
};

struct hdcdrv_dbg_timeout_count {
    u32 timeout_1ms_cnt;
    u32 timeout_10ms_cnt;
    u32 timeout_100ms_cnt;
    u32 timeout_1s_cnt;
};

struct hdcdrv_dbg_time {
    u32 conn_type; /* DBG_TIME_OP_CONN or DBG_TIME_OP_ACCEPT */
    union {
        struct {
            u64 conn_time_stamp[CONN_TIME_MAX];
            u64 conn_time_taken[CONN_TIME_TAKEN_MAX];
        };
        struct {
            u64 accept_time_stamp[ACCEPT_TIME_MAX];
            u64 accept_time_taken[ACCEPT_TIME_TAKEN_MAX];
        };
    };

    u64 tx_time_stamp[TX_TIME_MAX];
    u64 tx_last_time_taken[TX_TIME_TAKEN_MAX];
    u64 tx_max_time_taken[TX_TIME_TAKEN_MAX];

    u64 rx_time_stamp[RX_TIME_MAX];
    u64 rx_last_time_taken[RX_TIME_TAKEN_MAX];
    u64 rx_max_time_taken[RX_TIME_TAKEN_MAX];

    struct hdcdrv_dbg_timeout_count tx_timeout_cnt[TX_TIME_TAKEN_MAX];
    struct hdcdrv_dbg_timeout_count rx_timeout_cnt[RX_TIME_TAKEN_MAX];
};

struct hdcdrv_stats {
    unsigned long long tx;
    unsigned long long tx_bytes;
    unsigned long long rx;
    unsigned long long rx_bytes;
    unsigned long long tx_finish;
    unsigned long long rx_finish;
    unsigned long long tx_full;
    unsigned long long tx_fail;
    unsigned long long rx_fail;
    unsigned long long rx_full;
    unsigned long long rx_total;
    unsigned long long alloc_mem_err;
};

struct hdcdrv_service_stats {
    u64 accept_session_num;
    u64 connect_session_num_total;
    u64 connect_session_num;
    u64 close_session_num;
    u32 send_print_cnt;
    u64 send_jiffies;
    u32 send_print_cnt1;
    u64 send_jiffies1;
    u32 recv_print_cnt;
    u64 recv_jiffies;
    u32 recv_print_cnt1;
    u64 recv_jiffies1;
    u64 accept_wait_stamp;
    struct hdcdrv_link_ctrl_msg_stats connect_msg_stat;
    struct hdcdrv_link_ctrl_msg_stats reply_msg_stat;
    struct hdcdrv_link_ctrl_msg_stats close_msg_stat;
};

#define HDCDRV_DEBUG_MODE_TIMEOUT (600 * KA_HZ)
#define HDCDRV_CHAN_DFX_DELAY_TIME (15 * 60 * 1000) /* 15min */

struct hdcdrv_debug {
    long long valid;
    long long pid;
};
#endif /* _HDCDRV_DFX_H_ */
