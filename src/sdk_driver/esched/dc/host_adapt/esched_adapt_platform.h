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

#ifndef ESCHED_ADAPT_PLATFORM_H
#define ESCHED_ADAPT_PLATFORM_H

#include "securec.h"

#include "ka_fs_pub.h"
#include "ka_driver_pub.h"
#include "esched.h"
#include "esched_adapt.h"
#include "topic_sched_drv.h"
#include "topic_sched_drv_common.h"

void esched_adapt_wait_trace_update(struct sched_proc_ctx *proc_ctx, int dfx_gid, int dfx_tid,
                                    struct sched_event *event);
void esched_adapt_publish_trace_update(struct sched_proc_ctx *proc_ctx, int dfx_gid, int dfx_tid, u64 curr_time,
                                       u32 gid, u32 event_id, u32 subevent_id);
void esched_adapt_submit_trace_update(struct sched_proc_ctx *proc_ctx, int dfx_gid, int dfx_tid,
                                      struct sched_published_event_info *event_info, u64 curr_time, u32 gid,
                                      u32 event_id, u32 subevent_id);
u32 esched_publish_event_adapt_chip_id(u32 chip_id, struct sched_published_event_info *event_info);
void esched_drv_set_topic_type(u8 topic_types[][DST_ENGINE_MAX]);
bool esched_is_need_local_os_filter(u32 dst_engine);
bool esched_adapt_is_cpu_belongs_to_vf(struct sched_numa_node *node, struct sched_cpu_ctx *cpu_ctx);
bool esched_adapt_is_cpu_belongs_to_vf_mia(struct sched_numa_node *node, u32 vfid, struct sched_cpu_ctx *cpu_ctx);
void esched_adapt_query_remote_trace(u32 chip_id, struct sched_trace_input *para,
                                     struct sched_sync_event_trace *trace_result);
int esched_adapt_drv_init_sched_task_submit_chan_irq(u32 chip_id, u32 pool_id);
int esched_drv_get_normal_sched_submit_chan(struct sched_rtsq_res *rtsq, u32 qos, int *chan_id);
int esched_adapt_proc_mnt_ns_check(u32 chip_id, struct sched_published_event_info *event_info);
int esched_adapt_submit_check_master_pid(u32 devid, struct sched_published_event_info *event_info);
u32 esched_adapt_get_devid_for_submit(u32 devid);
int esched_update_sqe_user_data(struct topic_sched_sqe *sqe, struct sched_published_event_info *event_info);
int esched_create_default_dev(void);
void esched_destroy_default_dev(void);
int esched_adapt_restore_mb_usr_pid(struct topic_sched_mailbox *mb, u32 *pid);
int esched_adapt_fill_sqe_host_pid(u32 chip_id, struct sched_published_event_info *event_info,
                                   struct topic_sched_sqe *sqe, u32 *host_pid);
int esched_adapt_query_master_pid(u32 chip_id, u32 dst_pid, u32 *master_pid);
int esched_get_real_pid(struct topic_sched_mailbox *mb, u32 devid, u32 pid);
void esched_adapt_setup_dev_hw_ops_host(struct sched_dev_ops *ops);
void esched_adapt_setup_dev_hw_ops_device(struct sched_dev_ops *ops);
void esched_adapt_exit_check_sched_cpu(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx);
int esched_adapt_check_swapout_support(u32 chip_id, int timeout);
void esched_adapt_init_sched_cpu_ctx(struct sched_numa_node *node, u32 cpuid);
int esched_adapt_check_hw_sched_msgq(u32 chip_id, struct sched_numa_node *node, struct sched_grp_ctx *grp_ctx,
                                     struct sched_proc_ctx *proc_ctx, int32_t pid, u32 gid);
void esched_adapt_fill_exact_event_priv(u32 chip_id, struct sched_event *event, struct sched_thread_ctx *thread_ctx);
int esched_adapt_wait_event_ub_precheck(u32 chip_id);
void esched_adapt_set_wait_swapout_flag(u32 chip_id, struct sched_thread_ctx *thread_ctx);
void esched_adapt_clear_event_finish_func(u32 chip_id, struct sched_cpu_ctx *cpu_ctx,
                                          struct sched_thread_ctx *thread_ctx);
bool esched_adapt_guard_cpu_skip(struct sched_numa_node *node, struct sched_cpu_ctx *cpu_ctx);
int esched_adapt_publish_event_to_sched_grp(u32 chip_id, struct sched_event *event, struct sched_grp_ctx *grp_ctx);
bool esched_adapt_has_sia_map_topic_table(u32 chip_id);
int esched_adapt_publish_event_to_nonlocal(u32 dev_id, u32 event_src, struct sched_published_event_info *event_info,
                                           struct sched_published_event_func *event_func);
bool esched_adapt_is_soc_miniv3(u32 chip_id);
bool esched_adapt_has_hard_soft_sched(void);
bool esched_adapt_has_hardware_mia(u32 chip_id);
bool esched_adapt_is_soc_cloud_v4(u32 chip_id);
bool esched_adapt_need_clear_publish_tid(u32 chip_id, u32 event_id);
bool esched_adapt_release_task_on_close(u32 devid);
int32_t sched_fop_set_sched_cpu(u32 devid, unsigned long arg);
int32_t sched_fop_feature_get(u32 devid, unsigned long arg);
int esched_drv_submit_split_event(struct sched_numa_node *node, u32 event_src,
                                  struct sched_published_event_info *event_info, u32 timeout);
bool esched_dst_engine_is_local(u32 dst_engine);
#endif /* ESCHED_ADAPT_PLATFORM_H */
