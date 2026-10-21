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

#ifndef TOPIC_SCHED_DRV_COMMON_H
#define TOPIC_SCHED_DRV_COMMON_H

#include "ascend_hal_define.h"
#include "esched.h"
#include "topic_sched_drv.h"

#define TOPIC_SCHED_SPLIT_TASK_LEN 64U

int esched_mb_user_data_subtract(struct topic_sched_mailbox *mb, u16 size, u32 *value);

extern u8 sched_topic_types[POLICY_MAX][DST_ENGINE_MAX];

int esched_drv_fill_sqe(u32 chip_id, u32 event_src, struct topic_sched_sqe *sqe,
                        struct sched_published_event_info *event_info, bool wait_thread_check);
int esched_drv_fill_split_task(u32 chip_id, u32 event_src, struct sched_published_event_info *event_info,
                               void *split_task);
int esched_restore_mb_user_data(struct topic_sched_mailbox *mb, u32 *dst_devid, u32 *tid, u32 *pid);
int esched_drv_map_host_dev_pid(struct sched_proc_ctx *proc_ctx, u32 identity);
void esched_drv_unmap_host_dev_pid(struct sched_proc_ctx *proc_ctx, u32 identity);
int esched_get_real_pid(struct topic_sched_mailbox *mb, u32 devid, u32 pid);
bool esched_drv_check_dst_is_support(u32 dst_engine);
int esched_drv_get_ccpu_flag(u32 dst_engine);
void esched_drv_flush_mb_mbid(u8 *mb_id_ptr, u8 mb_id);
int esched_convert_src_pid_to_cp_pid(struct topic_sched_mailbox *mb, u32 devid, int cp_type);

#endif
