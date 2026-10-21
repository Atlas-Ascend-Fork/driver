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

#ifndef ESCHED_DRV_ADAPT_H
#define ESCHED_DRV_ADAPT_H

#include "esched.h"

#define HOST_VF_DEVID_START 100
#define DEVICE_VF_DEVID_START 32U
#define MAX_VF_NUM_PER_DEVICE 16U

#define PID_MAP_MASK 0x80000000U
#define MB_PID_MASK 0x7FFFFFFFU
#define TOPIC_EVENT_QUEUE_LIMIT 32U

#define TOPIC_SCHED_TS_PID_INDEX 6
#define TOPIC_SCHED_TS_CALLBACK_PID_INDEX 8

u32 esched_drv_get_node_aicpu_chan_mask(u32 start_id, u32 aicpu_chan_num);
u32 esched_get_cpuid_in_os(u32 chip_id, u32 cpuid_in_node);

int esched_hw_dev_init(struct sched_numa_node *node);
void esched_hw_dev_uninit(struct sched_numa_node *node);

int esched_drv_submit_event_distribute(u32 dev_id, u32 event_src, struct sched_published_event_info *event_info,
                                       struct sched_published_event_func *event_func);
int esched_drv_map_host_pid(struct sched_proc_ctx *proc_ctx);
void esched_drv_unmap_host_pid(struct sched_proc_ctx *proc_ctx);
void sched_drv_task_exit_check_sched_cpu(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx);
void sched_drv_try_resched_cpu(struct sched_numa_node *node);

int esched_drv_init_comm_pid_mapping(u32 node_id);
void esched_drv_uninit_comm_pid_mapping(u32 node_id);

int esched_drv_device_aicpu_chan_init(u32 chip_id);
void esched_drv_device_aicpu_chan_uninit(u32 chip_id);

int esched_drv_conf_sched_cpu(struct sched_numa_node *node, u32 sched_cpu_num);
u32 esched_get_devid_from_hw_vfid(u32 chip_id, u32 hw_vfid, u32 sub_dev_num, u32 topic_id);
u32 esched_get_hw_vfid_from_devid(u32 dev_id);
u32 esched_get_chipid_from_devid(u32 dev_id);
void esched_set_udevid_hw_vfid_belong_to(u32 chip_id, u32 udevid, u32 hw_vfid);
void esched_init_vf_udevid(u32 chip_id);
bool esched_is_phy_dev(u32 dev_id);
int esched_drv_config_pid(struct sched_proc_ctx *proc_ctx, u32 identity, devdrv_host_pids_info_t *pids_info);
void esched_drv_del_pid(struct sched_proc_ctx *proc_ctx, u32 identity);

void esched_drv_cpu_report(struct topic_data_chan *topic_chan, u32 error_code, u32 status);
bool esched_drv_is_mb_valid(struct topic_data_chan *topic_chan);
void esched_drv_intr_clr(struct topic_data_chan *topic_chan);
void esched_drv_get_status_report(struct topic_data_chan *topic_chan, u32 status);
bool esched_drv_is_get_mb_valid(struct topic_data_chan *topic_chan);
int esched_cpu_port_submit_task(struct topic_data_chan *topic_chan, void *split_task, u32 timeout);
void esched_cpu_port_reset(struct topic_data_chan *topic_chan, struct sched_cpu_port_clear_info *clr_info);
void esched_drv_init_aicpu_pool(struct sched_numa_node *node, u32 start_id, u32 aicpu_chan_num, u32 comcpu_chan_num);
void esched_drv_uninit_aicpu_pool(struct sched_hard_res *res);
void esched_drv_reset_pool(struct sched_hard_res *res);
void esched_drv_init_non_aicpu_pool(struct sched_hard_res *res);
int esched_drv_init_msgq_config(struct sched_numa_node *node, u32 start_id, u32 aicpu_chan_num, u32 comcpu_chan_num);
bool esched_res_is_belong_to_proc(int master_tgid, int slave_tgid, u32 udevid, struct res_map_info_in *res_info);
int esched_get_res_addr(u32 udevid, struct res_map_info_in *res_info, u64 *pa, u32 *len);
bool esched_drv_topic_status_valid(struct topic_data_chan *topic_chan);

/* use for esched_drv_mia.c */
int esched_drv_reset_phy_dev(u32 devid);
void esched_drv_restore_phy_dev(u32 devid);
int esched_drv_init_topic_table(u32 chip_id, u32 identity);
void esched_drv_uninit_topic_table(u32 chip_id, u32 identity);

void esched_drv_mb_intr_enable(struct topic_data_chan *topic_chan);

/* use for esched_mia_msg.c esched_sia_msg.c */
u64 esched_drv_get_host_ccpu_mask(u32 pool_id);
void esched_drv_init_cpuid_map(struct sched_hard_res *res);
#endif
