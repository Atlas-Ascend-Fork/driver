/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#ifndef APM_SLAVE_DOMAIN_H
#define APM_SLAVE_DOMAIN_H

#include "ka_common_pub.h"

#include "apm_slave.h"

enum apm_slave_status_type {
    SLAVE_STATUS_EXIT,
    SLAVE_STATUS_OOM,
    SLAVE_STATUS_MAX
};

struct apm_slave_domain_ops {
    int (*perm_check)(struct apm_cmd_bind *para);
    int (*get_master_tgid)(int master_pid, int *master_tgid);
    int (*bind)(struct apm_cmd_bind *para, int master_tgid, int slave_tgid);
    int (*unbind)(struct apm_cmd_bind *para, int master_tgid, int slave_tgid);
    int (*set_status)(int master_tgid, u32 udevid, int slave_tgid, int type, int status, int local_flag);
    int (*get_tast_group_exit_stage)(int master_tgid, int slave_tgid, u32 udevid, u32 proc_type_bitmap,
                                     int *exit_stage);
};

struct apm_slave_mem_stats_ops {
    void (*bind)(u32 udevid, int proc_type, int master_tgid, int slave_tgid);
    void (*unbind)(u32 udevid, int proc_type, int slave_tgid);
};

void apm_try_to_set_slave_oom(int tgid, int oom_status);

void apm_slave_domain_ops_register(int mode, struct apm_slave_domain_ops *ops);
void apm_slave_mem_stats_ops_register(int mode, struct apm_slave_mem_stats_ops *ops);
int apm_slave_domain_query_master(struct apm_cmd_query_master_info *para);
int apm_slave_domain_get_ssid(int slave_tgid, struct apm_cmd_slave_ssid *para);
int apm_slave_domain_set_ssid(int slave_tgid, struct apm_cmd_slave_ssid *para);
int apm_slave_domain_tgid_to_pid(int tgid, int *slave_pid);
void apm_slave_domain_task_exit(u32 udevid, int tgid, struct task_start_time *start_time);
int apm_slave_domain_task_exit_check(void *priv);
void apm_slave_domain_task_show(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq);
int apm_slave_domain_init(void);
void apm_slave_domain_uninit(void);

#endif
