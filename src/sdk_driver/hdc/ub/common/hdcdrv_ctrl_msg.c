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

#include "kernel_version_adapt.h"
#include "ka_compiler_pub.h"
#include "ka_base_pub.h"
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "hdcdrv_cmd.h"
#include "hdcdrv_core_com_ub.h"
#include "hdcdrv_mem_com_ub.h"
#include "hdcdrv_ctrl_msg.h"
#include "hdcdrv_core_ub.h"

struct hdcdrv_ctrl_msg_node *hdcdrv_find_ctrl_msg_node(struct hdcdrv_dev *dev, u64 hash_pid)
{
    struct hdcdrv_ctrl_msg_node *ctrl_node = NULL;
    u64 key = hash_pid; /* Search the hash table using dev_id as the key value */

    ka_hash_for_each_possible(dev->event_msg_pool.htable, ctrl_node, hnode, key)
    {
        if (ctrl_node->hash_pid == hash_pid) {
            return ctrl_node;
        }
    }

    return NULL;
}

struct hdcdrv_ctrl_msg_node *hdcdrv_add_ctrl_msg_node(struct hdcdrv_dev *dev, u64 hash_pid)
{
    struct hdcdrv_ctrl_msg_node *ctrl_node = NULL;
    u64 key = hash_pid;

    if (hdcdrv_find_ctrl_msg_node(dev, hash_pid) != NULL) {
        hdcdrv_err("Repeat add node.(dev_id=%u; pid=%llu)\n", dev->dev_id, hash_pid);
        return NULL;
    }

    ctrl_node = hdcdrv_ub_kvmalloc(sizeof(struct hdcdrv_ctrl_msg_node), KA_SUB_MODULE_TYPE_1);
    if (ctrl_node == NULL) {
        hdcdrv_err("Alloc Node for process failed.(dev_id=%u; pid=%llu)\n", dev->dev_id, hash_pid);
        return NULL;
    }

    ctrl_node->hash_pid = hash_pid;
    ctrl_node->ref = 0;
    ctrl_node->status = HDCDRV_NODE_IDLE;
    ctrl_node->ctx = NULL;
    KA_INIT_LIST_HEAD(&ctrl_node->wait_list);
    ka_hash_add(dev->event_msg_pool.htable, &ctrl_node->hnode, key);
    ka_task_init_waitqueue_head(&ctrl_node->wq_ctrl_msg);

    return ctrl_node;
}

STATIC void hdcdrv_update_pool_dfx_info(struct hdcdrv_event_msg_block_pool *pool, u32 event_type, u32 op)
{
    if (op == MSG_POOL_DFX_OP_ADD) {
        pool->msg_type_count[event_type]++;
    } else {
        pool->msg_type_count[event_type]--;
    }
}

// Recycle block to its pool based on block->type
void hdcdrv_recycle_event_msg_block(struct hdcdrv_dev *dev, struct hdcdrv_event_msg_block *block)
{
    struct hdcdrv_event_msg_block_pool *pool = &dev->event_msg_pool.block_pool[block->type];

    pool->ctrl_msg_block_queue[pool->head] = block;
    pool->head = (pool->head + 1) % pool->depth;
    hdcdrv_update_pool_dfx_info(pool, block->data.type, MSG_POOL_DFX_OP_DEL);
}

// Clear all event in list and wakeup node, this operation should be in the dev mutex
void hdcdrv_ctrl_msg_node_list_res_clear(struct hdcdrv_ctrl_msg_node *ctrl_node, struct hdcdrv_dev *dev)
{
    struct hdcdrv_event_msg_block *block = NULL;

    // Set node idle, then the wait_event thread will not deal with event and exit after wakeup
    ctrl_node->status = HDCDRV_NODE_IDLE;

    while (!ka_list_empty(&ctrl_node->wait_list)) {
        block = ka_list_first_entry(&ctrl_node->wait_list, struct hdcdrv_event_msg_block, list);
        ka_list_del(&block->list);
        hdcdrv_recycle_event_msg_block(dev, block);
    }

    ka_task_wake_up_interruptible(&ctrl_node->wq_ctrl_msg);
}

void hdcdrv_del_ctrl_msg_node(struct hdcdrv_dev *dev, struct hdcdrv_ctrl_msg_node *ctrl_node)
{
    struct hdcdrv_ctx *ctx = ctrl_node->ctx;

    ka_hash_del(&ctrl_node->hnode);
    if (ctx != NULL) {
        ka_task_mutex_lock(&ctx->ctx_lock);
        ctrl_node->ctx = NULL;
        ctx->ctrl_node = NULL;
        ka_task_mutex_unlock(&ctx->ctx_lock);
    }
    if (ctrl_node->ref != 0) {
        ctrl_node->status = HDCDRV_NODE_NEED_FREE;
        ka_task_wake_up_interruptible(&ctrl_node->wq_ctrl_msg);
        return;
    }
    // Free must be after wait_interruptible has been wake up and wait_event thread has exit
    hdcdrv_ub_kvfree((void **)&ctrl_node, KA_SUB_MODULE_TYPE_1);
}

// Used in uninstance, clear all event_block and wakeup all ctrl_node
void hdcdrv_clear_event_msg_pool(struct hdcdrv_dev *dev)
{
    struct hdcdrv_ctrl_msg_node *ctrl_node = NULL;
    ka_hlist_node_t *local_hnode = NULL;
    u32 bkt = 0;

    ka_hash_for_each_safe(dev->event_msg_pool.htable, bkt, local_hnode, ctrl_node, hnode)
    {
        hdcdrv_ctrl_msg_node_list_res_clear(ctrl_node, dev);
    }
}

STATIC void hdcdrv_clear_dev_all_ctrl_msg_node(struct hdcdrv_dev *dev)
{
    struct hdcdrv_ctrl_msg_node *ctrl_node = NULL;
    ka_hlist_node_t *local_hnode = NULL;
    u32 bkt = 0;

    ka_hash_for_each_safe(dev->event_msg_pool.htable, bkt, local_hnode, ctrl_node, hnode)
    {
        hdcdrv_ctrl_msg_node_list_res_clear(ctrl_node, dev);
        hdcdrv_del_ctrl_msg_node(dev, ctrl_node);
    }
}

int hdcdrv_ctrl_msg_queue_init(struct hdcdrv_dev *dev)
{
    struct hdcdrv_event_msg_block_pool *pool = NULL;
    u32 pool_type;
    int i;
    int depths[HDCDRV_MSG_POOL_TYPE_MAX] = {HDCDRV_UB_CTRL_MSG_QUEUE_DEPTH, HDCDRV_UB_DMP_MSG_QUEUE_DEPTH};

    for (pool_type = 0; pool_type < HDCDRV_MSG_POOL_TYPE_MAX; pool_type++) {
        pool = &dev->event_msg_pool.block_pool[pool_type];
        pool->tail = 0;
        pool->head = 0;
        pool->depth = depths[pool_type];
        for (i = 0; i < depths[pool_type]; i++) {
            pool->ctrl_msg_block_queue[i] = (struct hdcdrv_event_msg_block *)hdcdrv_vzalloc(
                sizeof(struct hdcdrv_event_msg_block), KA_SUB_MODULE_TYPE_0);
            if (pool->ctrl_msg_block_queue[i] == NULL) {
                hdcdrv_err("alloc node failed for event queue.(dev_id=%u; pool_type=%u)\n", dev->dev_id, pool_type);
                goto exit_queue_init;
            }
            pool->ctrl_msg_block_queue[i]->type = pool_type;
            KA_INIT_LIST_HEAD(&pool->ctrl_msg_block_queue[i]->list);
        }
    }

    return 0;

exit_queue_init:
    for (pool_type = 0; pool_type < HDCDRV_MSG_POOL_TYPE_MAX; pool_type++) {
        pool = &dev->event_msg_pool.block_pool[pool_type];
        for (i = 0; i < pool->depth; i++) {
            if (pool->ctrl_msg_block_queue[i] != NULL) {
                hdcdrv_vfree(pool->ctrl_msg_block_queue[i], KA_SUB_MODULE_TYPE_0);
                pool->ctrl_msg_block_queue[i] = NULL;
            }
        }
    }

    return -ENOMEM;
}

void hdcdrv_ctrl_msg_queue_uninit(struct hdcdrv_dev *dev)
{
    struct hdcdrv_event_msg_block_pool *pool = NULL;
    u32 pool_type;
    int i, cnt = 0;

    hdcdrv_clear_dev_all_ctrl_msg_node(dev);
    for (pool_type = 0; pool_type < HDCDRV_MSG_POOL_TYPE_MAX; pool_type++) {
        pool = &dev->event_msg_pool.block_pool[pool_type];
        cnt = 0;
        for (i = 0; i < pool->depth; i++) {
            if (pool->ctrl_msg_block_queue[i] == NULL) {
                cnt++;
                continue;
            }
            hdcdrv_vfree(pool->ctrl_msg_block_queue[i], KA_SUB_MODULE_TYPE_0);
            pool->ctrl_msg_block_queue[i] = NULL;
        }
        if (cnt != 0) {
            hdcdrv_err("Existing node has not been recycled.(pool_type=%u; cnt=%d)\n", pool_type, cnt);
        }
    }
}

STATIC bool hdcdrv_is_msg_queue_full(struct hdcdrv_event_msg_block_pool *pool)
{
    return (((pool->tail + 1) % pool->depth) == pool->head);
}

bool hdcdrv_is_msg_queue_empty(struct hdcdrv_dev *dev, u32 pool_type)
{
    struct hdcdrv_event_msg_block_pool *pool = &dev->event_msg_pool.block_pool[pool_type];

    return (pool->tail == pool->head);
}

bool hdcdrv_is_wait_list_empty(struct hdcdrv_ctrl_msg_node *ctrl_node)
{
    if (ka_list_empty(&ctrl_node->wait_list)) {
        return true;
    }

    return false;
}

int hdcdrv_wake_up_ctrl_node(struct hdcdrv_dev *dev, u64 pid, struct hdcdrv_event_msg *event_msg, u32 pool_type)
{
    struct hdcdrv_ctrl_msg_node *ctrl_node = NULL;
    struct hdcdrv_event_msg_block_pool *pool = &dev->event_msg_pool.block_pool[pool_type];
    struct hdcdrv_event_msg_block *block = NULL;

    if (hdcdrv_is_msg_queue_full(pool)) {
        hdcdrv_warn("process queue has been full.(dev_id=%u; pool_type=%u)\n", dev->dev_id, pool_type);
        return HDCDRV_DMA_QUE_FULL;
    }

    ctrl_node = hdcdrv_find_ctrl_msg_node(dev, pid);
    if (ctrl_node == NULL) {
#ifndef DRV_UT
        hdcdrv_warn("process not found. (dev_id=%u; pid=%llu)\n", dev->dev_id, pid);
#endif
        return DRV_ERROR_NO_PROCESS;
    }

    if (ctrl_node->status != HDCDRV_NODE_BUSY) {
        hdcdrv_warn("process ctrl_node is idle.(dev_id=%u; pid=%llu; status=%u)\n", dev->dev_id, pid,
                    ctrl_node->status);
        return DRV_ERROR_NO_PROCESS;
    }

    block = pool->ctrl_msg_block_queue[pool->tail];
    pool->ctrl_msg_block_queue[pool->tail] = NULL;
    block->fail_count = 0;
    (void)memcpy_s(&block->data, sizeof(struct hdcdrv_event_msg), event_msg, sizeof(struct hdcdrv_event_msg));
    pool->tail = (pool->tail + 1) % pool->depth;
    ka_list_add_tail(&block->list, &ctrl_node->wait_list);
    hdcdrv_update_pool_dfx_info(pool, event_msg->type, MSG_POOL_DFX_OP_ADD);
    ka_task_wake_up_interruptible(&ctrl_node->wq_ctrl_msg);

    return 0;
}

/*
    1. copy data from block to event_msg
    2. return block to pool
*/
int hdcdrv_get_ctrl_msg_event(struct hdcdrv_dev *dev, struct hdcdrv_ctrl_msg_node *ctrl_node,
                              struct hdcdrv_event_msg_block *block, struct hdcdrv_cmd_wait_ctrl_msg *cmd)
{
    int ret;

    ret = ka_base_copy_to_user((void __ka_user *)cmd->msg_buf, &block->data, sizeof(struct hdcdrv_event_msg));
    if (ret != 0) {
        block->fail_count++;
        ret = HDCDRV_COPY_TO_USER_FAIL;
#ifndef DRV_UT
        hdcdrv_warn("Copy to user unsuccessful. (dev_id=%u; retry_time=%u)\n", dev->dev_id, block->fail_count);
#endif
        if (block->fail_count < HDCDRV_MAX_COPY_RETRY_TIME) {
            ka_list_add(&block->list, &ctrl_node->wait_list);
            return ret;
        }
    }

    block->fail_count = 0;
    hdcdrv_recycle_event_msg_block(dev, block);

    return ret;
}

void hdcdrv_get_ctrl_msg_pool_stat(struct hdcdrv_dev *dev, u32 pool_type, struct hdcdrv_ctrl_msg_pool_stat *stat)
{
    struct hdcdrv_event_msg_block_pool *pool = &dev->event_msg_pool.block_pool[pool_type];

    stat->pool_type = pool_type;
    stat->depth = pool->depth;
    stat->head = pool->head;
    stat->tail = pool->tail;
    stat->connect_msg_num = pool->msg_type_count[HDCDRV_NOTIFY_MSG_CONNECT];
    stat->dfx_msg_num = pool->msg_type_count[HDCDRV_NOTIFY_MSG_DFX];
    stat->close_msg_num = pool->msg_type_count[HDCDRV_NOTIFY_MSG_CLOSE];
}