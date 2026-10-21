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
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_base_pub.h"
#include "ka_compiler_pub.h"
#include "ka_list_pub.h"
#include "securec.h"
#include "pbl_mem_alloc_interface.h"
#include "udis_log.h"
#include "udis_management.h"
#include "udis_timer.h"

#define UDIS_TIMER_STEP_MS 10
#define UDIS_DEFAULT_WQ_MAX_ACTIVES 0
#define UDIS_TIMER_TASK_BACKOFF_FACTOR 10
struct udis_timer g_udis_timer = {0};
static ka_workqueue_struct_t *g_free_workqueue = NULL;

STATIC void udis_period_work_handle(ka_work_struct_t *work_data)
{
    int ret;
    int new_expried_cnt, old_expried_cnt;
    struct udis_period_task_node *task_node = NULL;

    task_node = ka_container_of(work_data, struct udis_period_task_node, work);

    old_expried_cnt = ka_base_atomic_read(&task_node->expired_cnt);
    new_expried_cnt = old_expried_cnt * UDIS_TIMER_TASK_BACKOFF_FACTOR;
    if (new_expried_cnt < old_expried_cnt) {
        goto out;
    }

    ret = task_node->period_task_func(task_node->udevid, task_node->privilege_data);
    if (ka_unlikely(ret != 0)) {
        /* If the periodic task execution fails, suppress the task execution, increase its period by tenfold.*/
        ka_base_atomic_set(&task_node->expired_cnt, new_expried_cnt);
    } else if ((unsigned int)old_expried_cnt != task_node->original_expired_cnt) {
        /* If the periodic task execution is successful and the task period has been modified,
         * the task period is restored to the initial value.
         */
        ka_base_atomic_set(&task_node->expired_cnt, task_node->original_expired_cnt);
    }
out:
    ka_base_atomic_dec(&task_node->queueflag);
}

STATIC void udis_timer_queue_work(ka_workqueue_struct_t *wq, struct udis_period_task_node *task_node)
{
    /* the work of period task node has been in wq*/
    if (ka_base_atomic_read(&task_node->queueflag) != 0) {
        return;
    }

    ka_base_atomic_inc(&task_node->queueflag);
#ifdef DRV_HOST
    ka_task_queue_work(wq, &task_node->work);
#endif
}

STATIC ka_workqueue_struct_t *udis_timer_alloc_workqueue(const char *name, unsigned int flags, int max_active)
{
    ka_workqueue_struct_t *wq;

#ifdef DRV_HOST
    flags |= WQ_UNBOUND;
#endif

    wq = ka_task_alloc_workqueue("%s", flags, max_active, name);
    if (wq == NULL) {
        udis_err("Failed to alloc workqueue. (name=%s; flags=0x%x; max_active=%d)\n", name, flags, max_active);
        return NULL;
    }
    /* In the device side, it is necessary to bind the workqueue with the ctrl CPU */
    return wq;
}

STATIC ka_hrtimer_restart_t udis_hrtimer_irq_handle(ka_hrtimer_t *htimer)
{
    struct udis_period_task_node *task_node = NULL;
    ka_workqueue_struct_t *wq = NULL;

    ka_task_rcu_read_lock();
    ka_list_for_each_entry_rcu(task_node, &g_udis_timer.period_task_list, node)
    {
        task_node->cur_cnt++;
        if (task_node->cur_cnt < (unsigned int)ka_base_atomic_read(&task_node->expired_cnt)) {
            continue;
        }
        task_node->cur_cnt = 0;
        wq = (task_node->work_type == COMMON_WORK ? g_udis_timer.common_wq : task_node->workqueue);
        udis_timer_queue_work(wq, task_node);
    }
    ka_task_rcu_read_unlock();

    (void)ka_system_hrtimer_forward_now(htimer, ka_system_ms_to_ktime(UDIS_TIMER_STEP_MS));
    return KA_HRTIMER_RESTART;
}

STATIC void udis_timer_node_free_work(ka_work_struct_t *work)
{
    struct udis_period_task_node *task_node = ka_container_of(work, struct udis_period_task_node, free_work);

    if (task_node->workqueue != NULL) {
        ka_task_flush_workqueue(task_node->workqueue);
        ka_task_destroy_workqueue(task_node->workqueue);
        task_node->workqueue = NULL;
    }
    dbl_kfree(task_node);
    task_node = NULL;
}

STATIC int udis_period_task_node_init(unsigned int udevid, const struct udis_timer_task *timer_task,
                                      struct udis_period_task_node *task_node)
{
    int ret;

    if (task_node == NULL) {
        udis_err("Invalid param, task_node is NULL. (udevid=%u)", udevid);
        return -EINVAL;
    }

    task_node->original_expired_cnt = timer_task->period_ms / UDIS_TIMER_STEP_MS;
    task_node->cur_cnt = timer_task->cur_ms / UDIS_TIMER_STEP_MS;
    task_node->udevid = udevid;
    task_node->work_type = timer_task->work_type;
    task_node->privilege_data = timer_task->privilege_data;
    task_node->period_task_func = timer_task->period_task_func;
    task_node->workqueue = NULL;
    ret = strcpy_s(task_node->task_name, TASK_NAME_MAX_LEN, timer_task->task_name);
    if (ret != 0) {
        udis_err("Call strncpy_s failed. (udevid=%u; task_name=%s; ret=%d)\n", udevid, timer_task->task_name, ret);
        return -ENOMEM;
    }
    KA_TASK_INIT_WORK(&task_node->work, udis_period_work_handle);
    KA_TASK_INIT_WORK(&task_node->free_work, udis_timer_node_free_work);
    ka_base_atomic_set(&task_node->queueflag, 0);
    ka_base_atomic_set(&task_node->expired_cnt, task_node->original_expired_cnt);

    if (task_node->work_type == COMMON_WORK) {
        return 0;
    }

    task_node->workqueue = udis_timer_alloc_workqueue(task_node->task_name, WQ_HIGHPRI, 1);
    if (task_node->workqueue == NULL) {
        return -ENOMEM;
    }
    return 0;
}

STATIC int udis_timer_check_task_para(const struct udis_timer_task *timer_task)
{
    if (timer_task == NULL) {
        udis_err("Invalid param, timer_task is NULL.\n");
        return -EINVAL;
    }

    if (timer_task->period_task_func == NULL) {
        udis_err("Invalid param, period_task_func is NULL. (task_name=%s)\n", timer_task->task_name);
        return -EINVAL;
    }

    if (timer_task->work_type >= UDIS_WORK_TYPE_MAX) {
        udis_err("Invalid timer_task work type. (work_type=%u; task_name=%s)\n", timer_task->work_type,
                 timer_task->task_name);
        return -EINVAL;
    }

    if (timer_task->period_ms < UDIS_TIMER_STEP_MS) {
        udis_err("Task's period is less then timer step. (period=%ums; timer_step=%ums; task_name=%s)\n",
                 timer_task->period_ms, UDIS_TIMER_STEP_MS, timer_task->task_name);
        return -EINVAL;
    }

    if (timer_task->period_ms % UDIS_TIMER_STEP_MS != 0) {
        udis_err("Task's period cannot be divided by the timer step. (period=%ums; timer_step=%ums; task_name=%s)\n",
                 timer_task->period_ms, UDIS_TIMER_STEP_MS, timer_task->task_name);
        return -EINVAL;
    }

    return 0;
}

STATIC struct udis_period_task_node *udis_timer_find_task_node(const char *task_name)
{
    struct udis_period_task_node *task_node = NULL;
    ka_list_for_each_entry(task_node, &g_udis_timer.period_task_list, node)
    {
        if (ka_base_strcmp(task_node->task_name, task_name) != 0) {
            continue;
        }
        return task_node;
    }
    return NULL;
}

int hal_kernel_register_period_task(unsigned int udevid, const struct udis_timer_task *timer_task)
{
    struct udis_period_task_node *task_node = NULL;
    int ret;

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX) || udis_timer_check_task_para(timer_task) != 0) {
        udis_err("Invalid param. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ka_task_mutex_lock(&g_udis_timer.task_list_lock);

    if (g_udis_timer.task_num >= UDIS_TIMER_TASK_MAX_NUM) {
        ka_task_mutex_unlock(&g_udis_timer.task_list_lock);
        udis_err("Udis timer task num is full. (udevid=%u; task_name=%s)\n", udevid, timer_task->task_name);
        return -EUSERS;
    }

    task_node = udis_timer_find_task_node(timer_task->task_name);
    if (task_node != NULL) {
        ka_task_mutex_unlock(&g_udis_timer.task_list_lock);
        udis_info("Udis timer task node is existed. (udevid=%u; task_name=%s)\n", udevid, timer_task->task_name);
        return -EEXIST;
    }

    task_node = dbl_kzalloc(sizeof(struct udis_period_task_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (task_node == NULL) {
        ka_task_mutex_unlock(&g_udis_timer.task_list_lock);
        udis_err("Udis timer malloc task_node failed. (udevid=%u; task_name=%s)\n", udevid, timer_task->task_name);
        return -ENOMEM;
    }

    ret = udis_period_task_node_init(udevid, timer_task, task_node);
    if (ret != 0) {
        ka_task_mutex_unlock(&g_udis_timer.task_list_lock);
        dbl_kfree(task_node);
        task_node = NULL;
        udis_err("Udis timer init task_node failed. (udevid=%u; task_name=%s; ret=%d)\n", udevid, timer_task->task_name,
                 ret);
        return ret;
    }

    ka_list_add_tail_rcu(&task_node->node, &g_udis_timer.period_task_list);
    g_udis_timer.task_num++;

    ka_task_mutex_unlock(&g_udis_timer.task_list_lock);
    udis_info("udis timer task register success. (udevid=%u; task_name=%s)\n", udevid, timer_task->task_name);
    return 0;
}

STATIC void udis_timer_node_rcu_free(ka_rcu_head_t *head)
{
    struct udis_period_task_node *task_node = ka_container_of(head, struct udis_period_task_node, rcu);
#ifndef EMU_ST
    ka_task_queue_work(g_free_workqueue, &task_node->free_work);
#else
    udis_timer_node_free_work(&task_node->free_work);
#endif
}

int hal_kernel_unregister_period_task(unsigned int udevid, const char *task_name)
{
    struct udis_period_task_node *task_node = NULL;

    ka_task_mutex_lock(&g_udis_timer.task_list_lock);

    task_node = udis_timer_find_task_node(task_name);
    if (task_node == NULL) {
        ka_task_mutex_unlock(&g_udis_timer.task_list_lock);
        udis_info("Udis timer task node does not existed. (udevid=%d; task_name=%s)\n", udevid, task_name);
        return -ENODATA;
    }

    g_udis_timer.task_num--;
    ka_list_del_rcu(&task_node->node);

    if (task_node->workqueue == NULL) {
        ka_task_flush_workqueue(g_udis_timer.common_wq);
    }
    ka_task_mutex_unlock(&g_udis_timer.task_list_lock);
    ka_system_call_rcu(&task_node->rcu, udis_timer_node_rcu_free);

    udis_info("udis timer unregister success. (udevid=%d; task_name=%s)\n", udevid, task_name);
    return 0;
}

int udis_timer_init(void)
{
    char *common_wq_name = "udis_timer";
    KA_INIT_LIST_HEAD_RCU(&g_udis_timer.period_task_list);
    ka_task_mutex_init(&g_udis_timer.task_list_lock);
    ka_system_hrtimer_init(&g_udis_timer.timer, KA_CLOCK_MONOTONIC, KA_HRTIMER_MODE_REL);
    g_udis_timer.timer.function = udis_hrtimer_irq_handle;
    g_udis_timer.task_num = 0;
    g_udis_timer.common_wq = udis_timer_alloc_workqueue(common_wq_name, WQ_MEM_RECLAIM, UDIS_DEFAULT_WQ_MAX_ACTIVES);
    if (g_udis_timer.common_wq == NULL) {
        return -ENOMEM;
    }
    g_free_workqueue = udis_timer_alloc_workqueue("udis_timer_free", WQ_UNBOUND, 1);
    if (g_free_workqueue == NULL) {
        ka_task_destroy_workqueue(g_udis_timer.common_wq);
        return -ENOMEM;
    }

    ka_system_hrtimer_start(&g_udis_timer.timer, ka_system_ms_to_ktime(UDIS_TIMER_STEP_MS), KA_HRTIMER_MODE_REL);
    udis_info("udis timer init success, start timer.\n");
    return 0;
}

void udis_timer_uninit(void)
{
    struct udis_period_task_node *task_node = NULL;
    struct udis_period_task_node *next = NULL;
    ka_task_mutex_lock(&g_udis_timer.task_list_lock);
    (void)ka_system_hrtimer_cancel(&g_udis_timer.timer);
    ka_system_rcu_barrier();
    ka_task_flush_workqueue(g_udis_timer.common_wq);
    ka_task_destroy_workqueue(g_udis_timer.common_wq);
    ka_task_flush_workqueue(g_free_workqueue);
    ka_task_destroy_workqueue(g_free_workqueue);
    ka_list_for_each_entry_safe(task_node, next, &g_udis_timer.period_task_list, node)
    {
        if (task_node->workqueue != NULL) {
            ka_task_flush_workqueue(task_node->workqueue);
            ka_task_destroy_workqueue(task_node->workqueue);
        }
        ka_list_del(&task_node->node);
        dbl_kfree(task_node);
        task_node = NULL;
    }
    ka_task_mutex_unlock(&g_udis_timer.task_list_lock);
    udis_info("udis timer uninit success, cancel timer.\n");
    return;
}
