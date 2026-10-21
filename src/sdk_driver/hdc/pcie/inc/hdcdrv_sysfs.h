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

#ifndef _EVENT_SYSFS_H_
#define _EVENT_SYSFS_H_

#include "ka_fs_pub.h"
#include "hdcdrv_core.h"

#define HDCDRV_ATTR_RD (KA_S_IRUSR | KA_S_IRGRP)
#define HDCDRV_ATTR_WR (KA_S_IWUSR | KA_S_IWGRP)
#define HDCDRV_ATTR_RW (HDCDRV_ATTR_RD | HDCDRV_ATTR_WR)

struct hdcdrv_cmd_get_stat {
    int ret;
    int dev_id; /* input, -1 not care */
    int fid;
    int pid;
    int service_type;       /* input, -1 not care */
    int session;            /* input, -1 not care */
    int chan_id;            /* input, -1 not care */
    int vf_flag;            /* input, 0 is dev_stat, 1 is vdev_stat */
    void __ka_user *outbuf; /* output */
};

struct hdcdrv_cmd_stat_session_brief {
    int active_num;
    int *active_list;
    int remote_close_num;
    int *remote_close_list;
    int idle_num;
    int *idle_list;
    int accept_num;
    int connect_num;
    int close_num;
    int cur_alloc_long_session;
    int cur_alloc_short_session;
    int total_idle_session_num;
};

struct hdcdrv_cmd_stat_all {
    int dev_num;
    int dev_list[HDCDRV_SUPPORT_MAX_DEV];
    struct hdcdrv_cmd_stat_session_brief s_brief;
};

struct hdcdrv_mem_info {
    unsigned int small_pool_size;
    unsigned int small_pool_remain_size;
    unsigned int huge_pool_size;
    unsigned int huge_pool_remain_size;
};
struct hdcdrv_cmd_stat_dev_service {
    struct hdcdrv_cmd_stat_session_brief s_brief;
    struct hdcdrv_stats stat;
    struct hdcdrv_mem_info tx_mem_info;
    struct hdcdrv_mem_info rx_mem_info;
};

struct hdcdrv_cmd_stat_chan {
    int w_sq_head;
    int w_sq_tail;
    int r_sq_head;
    int dma_head;
    int rx_head;
    int submit_dma_head;
    struct hdcdrv_stats stat;
    struct hdcdrv_dbg_stats dbg_stat;
};
struct hdcdrv_cmd_stat_session {
    int dev_id;
    int service_type;
    int status;
    int fast_chan_id;
    int chan_id;
    int local_session;
    int remote_session;
    int pkts_in_fast_list;
    int pkts_in_list;
    int remote_close_state;
    int local_close_state;
    u32 local_fid;
    u32 container_id;
    u64 create_pid;
    u64 peer_create_pid;
    u64 owner_pid;
    int work_cancel_cnt;
    struct hdcdrv_stats stat;
    struct hdcdrv_timeout timeout;
    struct hdcdrv_dbg_stats dbg_stat;
};

void hdcdrv_sysfs_init(ka_device_t *dev);
void hdcdrv_sysfs_uninit(ka_device_t *dev);
int hdcdrv_sysfs_ctrl_msg_get_session_stat(u32 dev_id, void *data, u32 *real_out_len);
int hdcdrv_sysfs_ctrl_msg_get_chan_stat(u32 dev_id, void *data, u32 *real_out_len);
int hdcdrv_sysfs_ctrl_msg_get_connect_stat(u32 dev_id, void *data, u32 *real_out_len);
int hdcdrv_sysfs_get_session_inner(char *buf, u32 buf_len, u32 session_fd, ssize_t *len, enum hdc_dfx_print_type type);
ssize_t hdcdrv_fill_buf_chan_info(char *buf, u32 buf_len, const struct hdcdrv_dbg_stats *stat);
int hdcdrv_sysfs_ctrl_msg_get_dbg_time_taken(u32 dev_id, void *data, u32 *real_out_len);

#endif
