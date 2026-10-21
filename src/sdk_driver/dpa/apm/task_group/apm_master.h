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
#ifndef APM_MASTER_H
#define APM_MASTER_H

#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_list_pub.h"

#include "ascend_hal_define.h"
#include "apm_task_group_def.h"

#define MASTER_PROC_NUM 8192U
#define MAX_USER_SLAVE_NUM 256U

struct master_slave_info {
    int valid;
    int slave_pid;
    int slave_tgid;
    int ssid;
    int local_flag;
    int exit_stage;
    int oom_status;
    int oom_cnt;
    int try_unbind_flag;
};

struct master_dev_ctx {
    int valid;
    u32 num;
    ka_atomic_t refcnt;
    struct master_slave_info slave_info[APM_PROC_TYPE_NUM];
};

struct master_slave_node {
    ka_list_head_t node; /* store  */
    u32 udevid;
    int proc_type;
    struct master_slave_info info;
};

struct master_ctx {
    ka_proc_dir_entry_t *entry;
    int master_pid;
    int master_exit_stage;
    u32 num;
    u32 user_slave_num;
    ka_list_head_t head; /* store user process */
    struct master_dev_ctx dev_ctx[];
};

int apm_master_ctx_create(struct task_ctx_domain *domain, int master_tgid, int master_pid,
                          struct task_start_time *start_time);
void apm_master_ctx_destroy(struct task_ctx_domain *domain, int master_tgid);
int apm_master_add_slave(struct task_ctx_domain *domain, int master_tgid, int slave_tgid, int local_flag,
                         struct apm_cmd_bind *para);
int apm_master_del_slave(struct task_ctx_domain *domain, int master_tgid, int slave_tgid, int local_flag,
                         struct apm_cmd_bind *para);
int apm_master_query_slave_pid(struct task_ctx_domain *domain, int master_tgid, struct apm_cmd_query_slave_pid *para);
int apm_master_get_slave_ssid(struct task_ctx_domain *domain, int master_tgid, struct apm_cmd_slave_ssid *para);
int apm_master_set_slave_ssid(struct task_ctx_domain *domain, int master_tgid, struct apm_cmd_slave_ssid *para);
int apm_master_tgid_to_pid(struct task_ctx_domain *domain, int master_tgid, int *master_pid);
int apm_master_set_exit_stage(struct task_ctx_domain *domain, int master_tgid, struct task_start_time *time, int stage);
int apm_master_get_exit_stage(struct task_ctx_domain *domain, int master_tgid, int *stage);
int apm_master_set_slave_exit_stage(struct task_ctx_domain *domain, int master_tgid, u32 udevid, int slave_tgid,
                                    int stage, int local_flag);
int apm_master_get_all_slave_exit_stage(struct task_ctx_domain *domain, int master_tgid, int *stage);
int apm_master_set_slave_oom_status(struct task_ctx_domain *domain, int master_tgid, u32 udevid, int slave_tgid,
                                    int oom_status, int local_flag);
int apm_master_get_slave_oom_status(struct task_ctx_domain *domain, int master_tgid, u32 udevid, int proc_type,
                                    int *oom_status);
void apm_master_ctx_show(struct task_ctx_domain *domain, int master_tgid, ka_seq_file_t *seq);
int apm_master_para_check(u32 udevid, int proc_type);
int apm_master_inc_ref(struct task_ctx_domain *domain, int master_tgid, u32 udevid);
int apm_master_dec_ref(struct task_ctx_domain *domain, int master_tgid, u32 udevid);
int apm_master_pre_unbind(struct task_ctx_domain *domain, int master_tgid, struct apm_cmd_bind *para);
int apm_master_check_all_master_is_bind_slave(struct task_ctx_domain *domain, u32 udevid);

#endif
