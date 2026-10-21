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

#ifndef _HDCDRV_CHAN_H_
#define _HDCDRV_CHAN_H_

#include "hdcdrv_dfx.h"
#include "hdcdrv_mem_com.h"
#include "comm_kernel_interface.h"

#define HDCDRV_DESC_QUEUE_DEPTH 16
#define HDCDRV_RX_BUDGET 16
#define HDCDRV_TX_BUDGET 16
#define HDCDRV_MSG_TX_STATUS_FULL 1
#define HDCDRV_MSG_TX_STATUS_EMPTY 0
#define HDCDRV_MSG_CHAN_TYPE_NORMAL 0
#define HDCDRV_MSG_CHAN_TYPE_FAST 1
#define HDCDRV_MANAGE_SERVICE_CHAN_NUM 2U
#define HDCDRV_MSG_CHAN_FLAG_NOT_ALLOCED 0
#define HDCDRV_MSG_CHAN_FLAG_ALLOCED 1
#define HDCDRV_INVALID_CHAN_ID 0XFFFFFFFF
/* msg chan cnt for 3559 host */
#define HDCDRV_NORMAL_MSG_CHAN_CNT 1
#define HDCDRV_RX_SCHED_WORK 1
#define HDCDRV_RX_SCHED_TASKLET 0
#define HDCDRV_DMA_CHAN_INVALID_INSTANCE (-1)

struct hdcdrv_buf_desc {
    void *buf;
    u64 addr;   // src_data_addr
    u32 offset; // src data addr seque offset
    int len;    // data_len
    int ctrl_len;

    u64 src_data_addr_va; // primal src_data_addr
    u64 src_ctrl_addr_va; // primal src_ctrl_addr
    u64 dst_data_addr;
    u64 src_ctrl_addr;
    u64 dst_ctrl_addr;

    int status;
    int skip_flag;
    int local_session;
    int remote_session;

    u32 inner_checker;
    struct hdcdrv_latency_info latency_info;

    struct hdcdrv_fast_mem *src_data;
    struct hdcdrv_fast_mem *dst_data;
    struct hdcdrv_fast_mem *src_ctrl;
    struct hdcdrv_fast_mem *dst_ctrl;
    ka_sg_table_t *dma_sgt;
    u32 dev_id;
    u32 fid; /* Only when the VM is not 0, the container is 0, and the physical machine is 0. */
    u32 pid;
};

struct hdcdrv_recv_buf_desc_list {
    struct hdcdrv_buf_desc buf_d;
    struct hdcdrv_recv_buf_desc_list *next;
};

struct hdcdrv_msg_chan_tasklet_status {
    unsigned long long schedule_in;
    unsigned long long schedule_in_last;
    int no_schedule_cnt;
};

struct hdcdrv_msg_chan {
    int chan_id;
    u32 dev_id;
    int type;
    void *chan;
    int rx_trigger_flag;
    int submit_dma_head;
    int dma_head;
    int rx_head;
    int session_cnt;
    int sq_head;
    int dma_need_submit_flag;
    int is_allocated;
    u64 dfx_rx_stamp; /* period print rx statistic */
    u64 dfx_tx_stamp; /* period print tx statistic */
    enum devdrv_dma_data_type data_type;
    struct hdcdrv_stats stat;
    struct hdcdrv_dbg_stats dbg_stat;
    ka_tasklet_struct_t tx_finish_task;
    ka_tasklet_struct_t rx_task;
    ka_tasklet_struct_t tx_sq_task;
    ka_tasklet_struct_t tx_cq_task;
    ka_workqueue_struct_t *rx_workqueue;
    ka_work_struct_t rx_notify_work;
    ka_tasklet_struct_t rx_notify_task;
    struct hdcdrv_msg_chan_tasklet_status tx_finish_task_status;
    struct hdcdrv_msg_chan_tasklet_status rx_notify_task_status;
    struct hdcdrv_msg_chan_tasklet_status rx_task_status;
    struct hdcdrv_buf_desc tx[HDCDRV_DESC_QUEUE_DEPTH];
    struct hdcdrv_buf_desc rx[HDCDRV_DESC_QUEUE_DEPTH];
    ka_wait_queue_head_t send_wait;
    ka_list_head_t wait_mem_list;
    ka_device_t *dev;
    struct devdrv_dma_node *node;
    ka_mutex_t mutex;
    unsigned long long hdcdrv_rx_stamp;
    unsigned long long hdcdrv_tx_stamp;
    unsigned long long send_wait_stamp;
    int rx_recv_sched_dma_full;
    int rx_task_sched_rx_full;
    unsigned long long fast_recv_wake_stamp;
};

#endif /* _HDCDRV_CHAN_H_ */
