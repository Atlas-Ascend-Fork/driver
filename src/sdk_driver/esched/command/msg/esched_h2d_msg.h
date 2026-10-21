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
#ifndef ESCHED_H2D_MSG_H
#define ESCHED_H2D_MSG_H

#include "hal_pkg/esched_pkg.h"
#include "esched_event_msg.h"

typedef enum esched_msg_type {
    ESCHED_MSG_TYPE_ADD_HOST_PID,
    ESCHED_MSG_TYPE_DEL_HOST_PID,
    ESCHED_MSG_TYPE_ADD_POOL,
    ESCHED_MSG_TYPE_GET_CPU_MBID,
    ESCHED_MSG_TYPE_ADD_MB,
    ESCHED_MSG_TYPE_CONF_INTR,
    ESCHED_MSG_TYPE_REMOTE_SUBMIT,
    ESCHED_MSG_TYPE_REMOTE_QUERY_GID,
    ESCHED_MSG_TYPE_GET_POOL_ID,
    ESCHED_MSG_TYPE_D2D_EVENT_DISPATCH,
    ESCHED_MSG_TYPE_REMOTE_QUERY_TRACE,
    ESCHED_MSG_TYPE_MAX_NUM
} ESCHED_MSG_TYPE;

struct esched_ctrl_msg_cfg_host_pid {
    int vfid;
    int host_ctrl_pid;
    int pid_type;
    int pid;
};

struct esched_ctrl_msg_cfg_pool {
    u32 cpu_type;
};

struct esched_ctrl_msg_get_cpu_mbid {
    u32 cpu_type;
    u32 mb_id;
    u32 wait_mb_id;
};

struct esched_ctrl_msg_cfg_mb {
    u32 vf_id;
};

struct esched_ctrl_msg_intr {
    u32 irq;
};

struct esched_msg_head {
    ESCHED_MSG_TYPE type;
    u32 src_pid;
    int error_code;
    int rsv;
};

struct esched_remote_submit_msg {
    struct sched_published_event_info event_info;
    char msg[SCHED_MAX_EVENT_MSG_LEN_EX];
};

struct esched_publish_msg {
    struct esched_msg_head head;
    struct esched_remote_submit_msg submit_msg;
};

struct esched_remote_query_gid_msg {
    int dst_devid;
    int pid;
    char grp_name[EVENT_MAX_GRP_NAME_LEN];
    u32 gid;
};

struct esched_ctrl_msg_get_pool_id {
    u32 pool_id;
};

struct esched_query_trace_msg {
    struct sched_trace_input para;
    struct sched_sync_event_trace sched_trace;
};

struct esched_ctrl_msg {
    struct esched_msg_head head;
    union {
        struct esched_ctrl_msg_cfg_host_pid host_pid_msg;
        struct esched_ctrl_msg_cfg_pool pool_msg;
        struct esched_ctrl_msg_get_cpu_mbid mbid_msg;
        struct esched_ctrl_msg_cfg_mb mb_msg;
        struct esched_ctrl_msg_intr intr_msg;
        struct esched_remote_query_gid_msg query_gid_msg;
        struct esched_ctrl_msg_get_pool_id pool_id_msg;
        struct esched_query_trace_msg trace_msg;
    };
};

static inline void sched_remote_msg_head_init(struct esched_msg_head *head, ESCHED_MSG_TYPE type)
{
    head->type = type;
    head->error_code = 0;
    head->src_pid = ka_task_get_current()->tgid;
    head->rsv = 0;
}
#endif