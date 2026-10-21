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
#ifndef TRS_SHR_ID_NODE_H
#define TRS_SHR_ID_NODE_H

#include "ka_type.h"

#include "trs_shr_id_ioctl.h"
#include "trs_id.h"

struct shr_id_node_op_attr {
    struct trs_id_inst inst;
    int res_type; /* trs id type */
    int type;     /* shr id type */
    u32 id;
    ka_pid_t pid; /* create pid */
    u64 start_time;
    char name[SHR_ID_NSM_NAME_SIZE];
    u32 flag;         /* remote flag */
    u32 stars_die_id; /* die id of stars in this ts inst */
    u32 get_seq;      /* global seq for remote get/put idempotency */
    u32 get_hpid;     /* host pid for remote get/put idempotency */

    int (*res_get)(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq);
    int (*res_put)(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq);
};

void shr_id_node_init(void);
int shr_id_node_create(struct shr_id_node_op_attr *attr);
int shr_id_node_destroy(const char *name, int type, ka_pid_t pid, u32 mode);
int shr_id_node_open(const char *name, ka_pid_t pid, unsigned long start_time, struct shr_id_node_op_attr *attr);
int shr_id_node_close(const char *name, int type, ka_pid_t pid);
int shr_id_node_set_pids(const char *name, int type, ka_pid_t create_pid, ka_pid_t pid[], u32 pid_num);
int shr_id_node_record(const char *name, int type, ka_pid_t pid);
int shr_id_node_set_attr(const char *name, int type, ka_pid_t pid);
int shr_id_node_get_info(const char *name, int type, struct shr_id_ioctl_info *info);
int shr_id_node_get_attribute(const char *name, int type, struct shr_id_ioctl_info *info);

int shr_id_name_update(u32 devid, char *name);
int shr_id_get_type_by_name(const char *name, int *id_type);
void *shr_id_node_get(const char *name, int type);
void shr_id_node_put(void *node);
int shr_id_node_get_attr(const char *name, struct shr_id_node_op_attr *attr);
void *shr_id_node_get_priv(void *node);
void shr_id_node_set_priv(void *node, void *priv);
void shr_id_node_priv_mutex_lock(void *node);
void shr_id_node_priv_mutex_unlock(void *node);
void shr_id_node_spin_lock(void *node);
void shr_id_node_spin_unlock(void *node);
bool shr_id_node_need_wlist(void *node);
struct shr_id_node_ops {
    int (*destory_node)(void *node, u32 mode);
    int (*name_update)(u32 devid, char *name);
};
void shr_id_register_node_ops(struct shr_id_node_ops *ops);

#endif
