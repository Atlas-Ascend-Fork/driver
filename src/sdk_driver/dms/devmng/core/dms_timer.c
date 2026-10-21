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

#include "ka_common_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"

#ifndef CFG_HOST_ENV
#include "queue_work_affinity.h"
#include "drv_cpu_type.h"
#include "workqueue_affinity.h"
#endif
#include "dms_define.h"
#include "pbl_mem_alloc_interface.h"
#include "dms_timer.h"

#define TIMER_INVALID_ID 0xFFFF
struct dms_sched_task_timer {
    ka_list_head_t node_list;
    ka_mutex_t node_lock;
    ka_hrtimer_t timer;
    ka_workqueue_struct_t *workqueue;
    u32 task_nums;
    u16 ids[MAX_TASK_NUMS];
};
static struct dms_sched_task_timer g_timer = {{0}};
static ka_workqueue_struct_t *g_free_workqueue = NULL;

struct devdrv_sched_task_node {
    ka_list_head_t list;
    ka_rcu_head_t rcu;
    ka_work_struct_t free_work;
    u32 expire_count; // time period: ms / TIMER_STEP_MS
    u32 cur_count;
    u32 node_id;
    u64 run_time;
    u64 user_data;
    int (*exec_task)(u64 data);
    ka_work_struct_t work_data;
    ka_workqueue_struct_t *workqueue;
    timer_task_handler_mode handler_mode;
};

STATIC ka_hrtimer_restart_t dms_timer_irq_handler(ka_hrtimer_t *htr)
{
    struct devdrv_sched_task_node *node = NULL;

    ka_task_rcu_read_lock();
    ka_list_for_each_entry_rcu(node, &g_timer.node_list, list)
    {
        if (node->cur_count > 1) {
            node->cur_count--;
            continue;
        }
        node->cur_count = node->expire_count;

        if (node->handler_mode == TIMER_IRQ) {
            node->exec_task(node->user_data);
        } else if (node->handler_mode == INDEPENDENCE_WORK || node->handler_mode == HIGH_PRI_WORK) {
            // Queue in self one thread.
#ifndef CFG_HOST_ENV
            queue_work_affinity(node->workqueue, &node->work_data);
#else
#ifndef DRV_SOFT_UT
            ka_task_queue_work(node->workqueue, &node->work_data);
#endif
#endif
        } else {
            // Queue in multi common thread.
#ifndef CFG_HOST_ENV
            queue_work_affinity(g_timer.workqueue, &node->work_data);
#else
            ka_task_queue_work(g_timer.workqueue, &node->work_data);
#endif
        }
    }
    ka_task_rcu_read_unlock();

    (void)ka_system_hrtimer_forward_now(htr, ka_system_ms_to_ktime(TIMER_STEP_MS));
    return KA_HRTIMER_RESTART;
}

STATIC void dms_timer_callback_delay(ka_work_struct_t *work)
{
    struct devdrv_sched_task_node *sched_task = NULL;

    sched_task = ka_container_of(work, struct devdrv_sched_task_node, work_data);
    sched_task->exec_task(sched_task->user_data);
}

STATIC void dms_timer_clear_node_id(u32 node_id)
{
    if (node_id < MAX_TASK_NUMS) {
        g_timer.ids[node_id] = TIMER_INVALID_ID;
    }
}

/* Get valid node id.
 * id Range: 1 ~ MAX_TASK_NUMS
 * */
STATIC u32 dms_timer_get_valid_node_id(void)
{
    int i;
    u32 node_id = TIMER_INVALID_ID;

    for (i = 0; i < MAX_TASK_NUMS; i++) {
        if (g_timer.ids[i] == TIMER_INVALID_ID) {
            g_timer.ids[i] = i;
            node_id = (u32)g_timer.ids[i];
            break;
        }
    }
    return node_id;
}

STATIC void dms_timer_node_free_work(ka_work_struct_t *work)
{
    struct devdrv_sched_task_node *node = ka_container_of(work, struct devdrv_sched_task_node, free_work);

    if (node->workqueue != NULL) {
        ka_task_flush_workqueue(node->workqueue);
        ka_task_destroy_workqueue(node->workqueue);
        node->workqueue = NULL;
    }
    dbl_kfree(node);
    node = NULL;
}

STATIC int dms_timer_node_init(const struct dms_timer_task *task, struct devdrv_sched_task_node *node)
{
    u32 node_id;
    char queue_name[WORKQUEUE_NAME_LENGTH];
    int ret;

    node_id = dms_timer_get_valid_node_id();
    if (node_id == TIMER_INVALID_ID) {
        /* Node id valid range: 0~MAX_TASK_NUMS-1 */
        dms_err("Timer task list is full.\n");
        return -ENOSPC;
    }

    node->node_id = node_id;
    node->expire_count = task->expire_ms / TIMER_STEP_MS;
    if (task->count_ms >= task->expire_ms) {
        node->cur_count = 0;
    } else {
        node->cur_count = (task->expire_ms - task->count_ms) / TIMER_STEP_MS;
    }
    node->user_data = task->user_data;
    node->exec_task = task->exec_task;
    node->handler_mode = task->handler_mode;
    node->run_time = 0;
    node->workqueue = NULL;
    KA_TASK_INIT_WORK(&node->work_data, dms_timer_callback_delay);
    KA_TASK_INIT_WORK(&node->free_work, dms_timer_node_free_work);
    if ((task->handler_mode != INDEPENDENCE_WORK) && (task->handler_mode != HIGH_PRI_WORK)) {
        return 0;
    }

    ret = snprintf_s(queue_name, sizeof(queue_name), sizeof(queue_name) - 1, "dms_timer_%u", node_id);
    if (ret <= 0) {
        dms_timer_clear_node_id(node_id);
        dms_err("Call snprintf_s failed. (node_id=%u; ret=%d)\n", node_id, ret);
        return -EFAULT;
    }

    if (task->handler_mode == INDEPENDENCE_WORK) {
        node->workqueue = ka_task_create_singlethread_workqueue(queue_name);
    } else {
        /* HIGH_PRI_WORK */
        node->workqueue = ka_task_alloc_workqueue("%s", WQ_HIGHPRI, 1, queue_name);
    }

    if (node->workqueue == NULL) {
        dms_timer_clear_node_id(node_id);
        dms_err("Timer task queue is NULL.\n");
        return -ENOMEM;
    }
    return 0;
}

int dms_timer_task_register(const struct dms_timer_task *task, u32 *node_id)
{
    struct devdrv_sched_task_node *node = NULL;
    int ret;

    if ((task == NULL) || (node_id == NULL) || task->exec_task == NULL) {
        dms_err("Timer task param invalid. node or task is null.\n");
        return -EINVAL;
    }
    if (task->handler_mode > HIGH_PRI_WORK) {
        dms_err("Timer task param invalid. handler_mode(%u) out of range.\n", task->handler_mode);
        return -EINVAL;
    }
    if (task->expire_ms < TIMER_STEP_MS) {
        dms_err("Timer task param invalid. expire_ms(%u) < TIMER_STEP_MS.\n", task->expire_ms);
        return -EINVAL;
    }
    if (g_timer.task_nums >= MAX_TASK_NUMS) {
        dms_err("Timer task list is full.\n");
        return -ENOSPC;
    }

    ka_task_mutex_lock(&g_timer.node_lock);
    node = dbl_kzalloc(sizeof(struct devdrv_sched_task_node), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        ka_task_mutex_unlock(&g_timer.node_lock);
        dms_err("Timer task node malloc failed.\n");
        return -ENOMEM;
    }

    ret = dms_timer_node_init(task, node);
    if (ret != 0) {
        dbl_kfree(node);
        node = NULL;
        ka_task_mutex_unlock(&g_timer.node_lock);
        dms_err("Timer task node init failed. (ret=%d)\n", ret);
        return ret;
    }

    *node_id = node->node_id;
    ka_list_add_tail_rcu(&node->list, &g_timer.node_list);
    g_timer.task_nums++;

    ka_task_mutex_unlock(&g_timer.node_lock);
    dms_info("Timer register success. (node_id=%u; expire_ms=%u)\n", *node_id, task->expire_ms);
    return 0;
}
KA_EXPORT_SYMBOL(dms_timer_task_register);

STATIC void dms_timer_node_rcu_free(ka_rcu_head_t *head)
{
    struct devdrv_sched_task_node *node = ka_container_of(head, struct devdrv_sched_task_node, rcu);

#ifndef CFG_HOST_ENV
    queue_work_affinity(g_free_workqueue, &node->free_work);
#else
    ka_task_queue_work(g_free_workqueue, &node->free_work);
#endif
}

int dms_timer_task_unregister(u32 node_id)
{
    struct devdrv_sched_task_node *node = NULL;

    ka_task_mutex_lock(&g_timer.node_lock);
    ka_list_for_each_entry(node, &g_timer.node_list, list)
    {
        if (node->node_id == node_id) {
            g_timer.task_nums--;
            ka_list_del_rcu(&node->list);
            // clean workqueue
            if (node->workqueue == NULL) {
                ka_task_flush_workqueue(g_timer.workqueue);
            }
            dms_timer_clear_node_id(node_id);
            ka_task_mutex_unlock(&g_timer.node_lock);
            ka_system_call_rcu(&node->rcu, dms_timer_node_rcu_free);
            dms_info("Timer unregister success. (node_id=%u)\n", node_id);
            return 0;
        }
    }

    ka_task_mutex_unlock(&g_timer.node_lock);
    dms_err("Timer node does not exist. (node_id=%u)\n", node_id);
    return -EINVAL;
}
KA_EXPORT_SYMBOL(dms_timer_task_unregister);

#if defined(CFG_FEATURE_WORK_QUEUE_BIND_CORE)
STATIC void dms_set_timer_workqueue_affinity(ka_workqueue_struct_t *wq)
{
    ka_cpumask_t wq_cpumask;
    int ret;

    ret = drv_get_ctrlcpu_mask(&wq_cpumask);
    if (ret != 0) {
        dms_warn("get ctrl cpu mask warn.(ret=%d)\n", ret);
        return;
    }

    if (set_workqueue_affinity(wq, 0, &wq_cpumask) != 0) {
        dms_warn("bind workqueue dms_timer_common warn\n");
    }
}
#endif
int dms_timer_init(void)
{
    int i;

    KA_INIT_LIST_HEAD_RCU(&g_timer.node_list);
    ka_task_mutex_init(&g_timer.node_lock);
#if defined(CFG_HOST_ENV)
    ka_system_hrtimer_init(&g_timer.timer, KA_CLOCK_MONOTONIC, KA_HRTIMER_MODE_REL);
#else
    ka_system_hrtimer_init(&g_timer.timer, KA_CLOCK_MONOTONIC, KA_HRTIMER_MODE_REL_PINNED);
#endif
    for (i = 0; i < MAX_TASK_NUMS; i++) {
        g_timer.ids[i] = TIMER_INVALID_ID;
    }
    g_timer.timer.function = dms_timer_irq_handler;
    g_timer.task_nums = 0;
#if defined(CFG_FEATURE_WORK_QUEUE_BIND_CORE)
    g_timer.workqueue = ka_task_alloc_workqueue("dms_timer_common", WQ_UNBOUND, 1);
#else
    g_timer.workqueue = ka_task_create_workqueue("dms_timer_common");
#endif
    if (g_timer.workqueue == NULL) {
        dms_err("Timer task work queue is NULL.\n");
        return -ENOMEM;
    }
#if defined(CFG_FEATURE_WORK_QUEUE_BIND_CORE)
    g_free_workqueue = ka_task_alloc_workqueue("dms_timer_free", WQ_UNBOUND, 1);
#else
    g_free_workqueue = ka_task_create_workqueue("dms_timer_free");
#endif
    if (g_free_workqueue == NULL) {
        ka_task_destroy_workqueue(g_timer.workqueue);
        dms_err("Timer free work queue is NULL.\n");
        return -ENOMEM;
    }

#if defined(CFG_FEATURE_WORK_QUEUE_BIND_CORE)
    dms_set_timer_workqueue_affinity(g_timer.workqueue);
    dms_set_timer_workqueue_affinity(g_free_workqueue);
#endif
#if defined(CFG_HOST_ENV)
    ka_system_hrtimer_start(&g_timer.timer, ka_system_ms_to_ktime(TIMER_STEP_MS), KA_HRTIMER_MODE_REL);
#else
    ka_system_hrtimer_start(&g_timer.timer, ka_system_ms_to_ktime(TIMER_STEP_MS), KA_HRTIMER_MODE_REL_PINNED);
#endif
    dms_info("Dms timer init success, start timer.\n");
    return 0;
}

void dms_timer_uninit(void)
{
    struct devdrv_sched_task_node *node = NULL;
    struct devdrv_sched_task_node *next = NULL;

    ka_task_mutex_lock(&g_timer.node_lock);
    (void)ka_system_hrtimer_cancel(&g_timer.timer);
    dms_info("Dms timer uninit success, cancel timer.\n");

    ka_system_rcu_barrier();
    ka_task_flush_workqueue(g_timer.workqueue);
    ka_task_destroy_workqueue(g_timer.workqueue);
    ka_task_flush_workqueue(g_free_workqueue);
    ka_task_destroy_workqueue(g_free_workqueue);
    ka_list_for_each_entry_safe(node, next, &g_timer.node_list, list)
    {
        if (node->workqueue != NULL) {
            ka_task_flush_workqueue(node->workqueue);
            ka_task_destroy_workqueue(node->workqueue);
        }
        dbl_kfree(node);
        node = NULL;
    }
    ka_task_mutex_unlock(&g_timer.node_lock);
    ka_task_mutex_destroy(&g_timer.node_lock);
}
