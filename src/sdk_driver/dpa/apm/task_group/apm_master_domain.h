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
#ifndef APM_MASTER_DOMAIN_H
#define APM_MASTER_DOMAIN_H

#include "ka_common_pub.h"

#include "apm_master.h"
#include "apm_kernel_ioctl.h"

struct apm_master_domain_ops {
    void (*destroy)(struct master_ctx *m_ctx, int tgid);
    int (*bind)(struct master_ctx *m_ctx, int master_tgid, int slave_tgid, struct apm_cmd_bind *para);
    int (*unbind)(struct master_ctx *m_ctx, int master_tgid, int slave_tgid, struct apm_cmd_bind *para);
};

struct apm_master_query_domain_ops {
    struct task_ctx_domain *(*apm_master_query_domain_get)(u32 udevid);
    void (*apm_master_query_domain_put)(struct task_ctx_domain *domain);
};

void apm_master_domain_ops_register(struct apm_master_domain_ops *ops);

struct apm_master_domain_cmd_ops {
    int (*query_meminfo)(u32 udevid, int slave_tgid, processType_t process_type, processMemType_t type, u64 *size);
};
void apm_master_domain_cmd_ops_register(struct apm_master_domain_cmd_ops *ops);

int apm_master_domain_add_slave(struct apm_cmd_bind *para, int master_tgid, int slave_tgid);
int apm_master_domain_del_slave(struct apm_cmd_bind *para, int master_tgid, int slave_tgid);
int apm_master_domain_tgid_to_pid(int tgid, int *master_pid);
bool apm_is_cur_task_trusted(void);
void apm_master_set_query_domain_ops(struct apm_master_query_domain_ops ops);
int apm_master_domain_set_slave_status(int master_tgid, u32 udevid, int slave_tgid, int type, int status,
                                       int local_flag);
int apm_master_domain_get_tast_group_exit_stage(int master_tgid, int slave_tgid, u32 udevid, u32 proc_type_bitmap,
                                                int *exit_stage);
int apm_master_query_domain_get_slave_ssid(int master_tgid, struct apm_cmd_slave_ssid *para);
int apm_master_query_domain_set_slave_ssid(int master_tgid, struct apm_cmd_slave_ssid *para);
int apm_master_domain_get_slave_status(int master_tgid, u32 udevid, int proc_type, int type, int *status);
void apm_master_domain_task_exit(u32 udevid, int tgid, struct task_start_time *start_time);
void apm_master_domain_task_show(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq);
int apm_master_domain_init(void);
void apm_master_domain_uninit(void);
int apm_master_domain_pre_unbind(int master_tgid, struct apm_cmd_bind *para);
int apm_master_domain_check_all_master_is_bind_slave(u32 udevid);

#endif
