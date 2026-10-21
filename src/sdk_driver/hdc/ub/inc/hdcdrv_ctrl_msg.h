/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
#ifndef _HDCDRV_CTRL_MSG_H_
#define _HDCDRV_CTRL_MSG_H_

#include "ka_type.h"
#include "ka_common_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_hashtable_pub.h"
#include "hdcdrv_adapt_ub.h"

#define HDC_UB_NODE_HASH_BIT 10
// When the value below changes, the value of the user_space macro with the same name also needs to be modified.
#define HDCDRV_UB_SINGLE_DEV_MAX_SESSION 272 /* 64 * 4(log + tsd + dvpp + reserved) + 8(short) + 8(short_extend) */
#define HDCDRV_UB_CTRL_MSG_QUEUE_DEPTH 1024
#define HDCDRV_UB_DMP_MSG_QUEUE_DEPTH 96 /* 24(dmp's real max support session) * 4(connect + close + dfx + reserve) */

/* Long session configuration: 256 sessions per device */
#define HDCDRV_UB_LONG_SESSION_NUM 256
#define HDCDRV_UB_LONG_SESSION_BASE 0

/* Short session configuration: 8 sessions per device */
#define HDCDRV_UB_SHORT_SESSION_NUM 8
#define HDCDRV_UB_SHORT_SESSION_BASE 256

/* Short extend session range: 264-271, total 8 sessions */
#define HDCDRV_UB_SHORT_EXTEND_SESSION_BASE 264
#define HDCDRV_UB_SHORT_EXTEND_SESSION_NUM 8

#define HDCDRV_NODE_IDLE 0
#define HDCDRV_NODE_BUSY 1
#define HDCDRV_NODE_NEED_FREE 2

#define HDCDRV_MAX_COPY_RETRY_TIME 3

enum hdcdrv_msg_pool_type {
    HDCDRV_MSG_POOL_TYPE_NORMAL = 0,
    HDCDRV_MSG_POOL_TYPE_DMP,
    HDCDRV_MSG_POOL_TYPE_MAX,
};

static inline u32 hdcdrv_get_pool_type_by_service_type(int service_type)
{
    return (service_type == HDCDRV_SERVICE_TYPE_DMP) ? HDCDRV_MSG_POOL_TYPE_DMP : HDCDRV_MSG_POOL_TYPE_NORMAL;
}

struct hdcdrv_event_msg_block {
    u32 type; /* pool type, see enum hdcdrv_msg_pool_type */
    u32 fail_count;
    struct hdcdrv_event_msg data;
    ka_list_head_t list;
};

struct hdcdrv_event_msg_block_pool {
    int tail;
    int head;
    int depth;
    struct hdcdrv_event_msg_block
        *ctrl_msg_block_queue[HDCDRV_UB_CTRL_MSG_QUEUE_DEPTH]; /* fixed array, block alloc in init */
    u32 msg_type_count[HDCDRV_NOTIFY_MSG_MAX];
};

enum hdcdrv_msg_pool_dfx_op {
    MSG_POOL_DFX_OP_ADD = 0,
    MSG_POOL_DFX_OP_DEL
};

// dev level, htable is shared across all pool types
struct hdcdrv_event_msg_pool {
    KA_DECLARE_HASHTABLE(htable, HDC_UB_NODE_HASH_BIT); /* store pid node, shared */
    struct hdcdrv_event_msg_block_pool block_pool[HDCDRV_MSG_POOL_TYPE_MAX];
};

struct hdcdrv_ctrl_msg_node {
    ka_hlist_node_t hnode; /* hash pid node */
    u64 hash_pid;          /* use pid as hash val */
    ka_wait_queue_head_t wq_ctrl_msg;
    void *ctx;
    u32 ref;
    u32 status;
    ka_list_head_t wait_list;
};

struct hdcdrv_ctrl_msg_pool_stat {
    int pool_type;
    int depth;
    int head;
    int tail;
    int connect_msg_num;
    int dfx_msg_num;
    int close_msg_num;
};

int hdcdrv_ctrl_msg_queue_init(struct hdcdrv_dev *dev);
void hdcdrv_ctrl_msg_queue_uninit(struct hdcdrv_dev *dev);
struct hdcdrv_ctrl_msg_node *hdcdrv_add_ctrl_msg_node(struct hdcdrv_dev *dev, u64 hash_pid);
void hdcdrv_del_ctrl_msg_node(struct hdcdrv_dev *dev, struct hdcdrv_ctrl_msg_node *ctrl_node);
void hdcdrv_ctrl_msg_node_list_res_clear(struct hdcdrv_ctrl_msg_node *ctrl_node, struct hdcdrv_dev *dev);
int hdcdrv_wake_up_ctrl_node(struct hdcdrv_dev *dev, u64 pid, struct hdcdrv_event_msg *event_msg, u32 pool_type);
void hdcdrv_recycle_event_msg_block(struct hdcdrv_dev *dev, struct hdcdrv_event_msg_block *block);
int hdcdrv_get_ctrl_msg_event(struct hdcdrv_dev *dev, struct hdcdrv_ctrl_msg_node *ctrl_node,
                              struct hdcdrv_event_msg_block *block, struct hdcdrv_cmd_wait_ctrl_msg *cmd);
bool hdcdrv_is_msg_queue_empty(struct hdcdrv_dev *dev, u32 pool_type);
struct hdcdrv_ctrl_msg_node *hdcdrv_find_ctrl_msg_node(struct hdcdrv_dev *dev, u64 hash_pid);
void hdcdrv_clear_event_msg_pool(struct hdcdrv_dev *dev);
bool hdcdrv_is_wait_list_empty(struct hdcdrv_ctrl_msg_node *ctrl_node);
void hdcdrv_get_ctrl_msg_pool_stat(struct hdcdrv_dev *dev, u32 pool_type, struct hdcdrv_ctrl_msg_pool_stat *stat);
#endif // _HDCDRV_CTRL_MSG_H_