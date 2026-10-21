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
#ifndef DEVMM_CHAN_HANDLERS_H
#define DEVMM_CHAN_HANDLERS_H

#include "devmm_proc_info.h"
#include "svm_kernel_msg.h"

struct devmm_chan_handlers_st {
    int (*chan_msg_processes)(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *msg, u32 *ack_len);
    u32 msg_size;
    u32 extend_size;
    u32 msg_bitmap;
};

extern struct devmm_chan_handlers_st devmm_channel_msg_processes[DEVMM_CHAN_MAX_ID];
extern struct devmm_chan_handlers_st devmm_channel_p2p_msg_processes[DEVMM_CHAN_MAX_ID];

void svm_register_h2d_msg_handle(u32 msg_id, const struct devmm_chan_handlers_st *handle);
void svm_register_d2d_msg_handle(u32 msg_id, const struct devmm_chan_handlers_st *handle);
void svm_register_d2h_msg_handle(u32 msg_id, const struct devmm_chan_handlers_st *handle);

int devmm_notify_device_close_process(struct devmm_svm_process *svm_pro, u32 logical_devid, u32 phy_devid, u32 vfid);
int devmm_chan_send_msg_free_pages(struct devmm_chan_free_pages *free_info, struct devmm_svm_heap *heap,
                                   struct devmm_svm_process *svm_proc, int shared_flag, u32 free_self);
void devmm_svm_free_share_page_msg(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap,
                                   unsigned long start, u64 real_size, u32 *page_bitmap);
int devmm_dev_page_fault_get_vaflgs(struct devmm_svm_process *svm_process, struct devmm_svm_heap *heap,
                                    struct devmm_chan_page_query *flg_msg);
int devmm_chan_update_msg_logic_id(struct devmm_svm_process *svm_proc, struct devmm_chan_msg_head *msg_head);
int devmm_chan_msg_dispatch(void *msg, u32 in_data_len, u32 out_data_len, u32 *ack_len,
                            const struct devmm_chan_handlers_st *msg_process);

#endif
