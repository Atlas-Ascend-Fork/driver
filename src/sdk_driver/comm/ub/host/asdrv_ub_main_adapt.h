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
#ifndef _ASDRV_UB_MAIN_ADAPT_H_
#define _ASDRV_UB_MAIN_ADAPT_H_

#include "ka_fs_pub.h"
#include "comm_kernel_interface.h"
#include "addr_trans.h"
#include "asdrv_ub_dev.h"
#include "asdrv_ub_common.h"

/* asdrv_ub_dev_online.h */
// ub reserve 1024+128, host device is not same
#define UBDRV_LINK_CHAN_JETTY_ID 1152
#define UBDRV_SERVER_JETTY_ID 0
#define PHY_DEV_NUM 8

/* asdrv_ub_msg.h */
struct ubdrv_common_msg {
    void *msg_chan[ASCEND_UB_DEV_MAX_NUM];
    int (*common_fun[ASCEND_UB_DEV_MAX_NUM][DEVDRV_COMMON_MSG_TYPE_MAX])(u32 devid, void *data, u32 in_data_len,
                                                                         u32 out_data_len, u32 *real_out_len);
    struct ubdrv_common_msg_stat com_msg_stat[ASCEND_UB_DEV_MAX_NUM][DEVDRV_COMMON_MSG_TYPE_MAX];
    ka_rw_semaphore_t rwlock[ASCEND_UB_DEV_MAX_NUM];
};

int devdrv_ub_register_common_msg_client(const struct devdrv_common_msg_client *msg_client);
int devdrv_ub_unregister_common_msg_client(u32 devid, const struct devdrv_common_msg_client *msg_client);
int devdrv_ub_common_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len,
                              enum devdrv_common_msg_type msg_type);
int devdrv_ub_msg_alloc_non_trans_queue_process(struct ubdrv_non_trans_chan *chan,
                                                struct ubdrv_create_non_trans_cmd *cmd);
int devdrv_ub_msg_free_non_trans_queue_process(struct ubdrv_non_trans_chan *chan, struct ubdrv_free_non_trans_cmd *cmd);
void *devdrv_ub_msg_alloc_non_trans_queue(u32 dev_id, struct devdrv_non_trans_msg_chan_info *chan_info);
int devdrv_ub_msg_free_non_trans_queue(void *msg_chan);
int devdrv_ub_sync_msg_send(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);

/* asdrv_ub_unit.h */
#define UBDRV_NONTRANS_TYPE_CNT devdrv_msg_client_max

int ubdrv_device_rescan(u32 dev_id);
int ubdrv_device_pre_reset(u32 dev_id);
int ubdrv_hot_reset_device(u32 dev_id);

int ubdrv_query_host_mem_decoder_info(u32 dev_id, enum ubdrv_log_level log_level);
int ubdrv_procfs_mem_host_cfg_dfx_show(ka_seq_file_t *seq, void *offset);
int ubdrv_mem_host_cfg_ctrl_init(void);
void ubdrv_mem_host_cfg_ctrl_uninit(void);
int ubdrv_rao_read(u32 dev_id, enum devdrv_rao_client_type type, u64 offset, u64 len);
int ubdrv_rao_write(u32 dev_id, enum devdrv_rao_client_type type, u64 offset, u64 len);

#endif