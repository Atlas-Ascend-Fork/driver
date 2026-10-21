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

#include "uvm_kthread_pool.h"
#include "devmm_mem_alloc_interface.h"
#include "devmm_common.h"

static int kthread_worker(void *data)
{
#ifndef EMU_ST
    struct kthread_pool *pool = data;
    struct kthread_task *task;

    while (!ka_task_kthread_should_stop()) {
        ka_task_wait_event_interruptible(pool->waitq,
                                         !ka_list_empty(&pool->task_list) || ka_task_kthread_should_stop());

        if (ka_task_kthread_should_stop()) {
            break;
        }

        ka_task_spin_lock(&pool->lock);
        /* Secondary check prevents false wakeups or concurrent preemptions */
        if (ka_list_empty(&pool->task_list)) {
            ka_task_spin_unlock(&pool->lock);
            continue;
        }

        task = list_first_entry(&pool->task_list, struct kthread_task, list);
        ka_list_del(&task->list);
        ka_task_spin_unlock(&pool->lock);

        task->func(task->arg);
        devmm_kfree_ex(task);
    }
#endif
    return 0;
}

struct kthread_pool *kthread_pool_create(int num_threads)
{
    struct kthread_pool *pool;
    int i;

    pool = devmm_kzalloc_ex(sizeof(*pool), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (!pool)
        return NULL;

    KA_INIT_LIST_HEAD(&pool->task_list);
    ka_task_spin_lock_init(&pool->lock);
    ka_task_init_waitqueue_head(&pool->waitq);
    pool->num_threads = num_threads;

    pool->threads = devmm_kcalloc_ex(num_threads, sizeof(struct task_struct *), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (!pool->threads) {
        devmm_kfree_ex(pool);
        return NULL;
    }

    for (i = 0; i < num_threads; i++) {
        pool->threads[i] = ka_task_kthread_create(kthread_worker, pool, "kthread_pool/%d", i);
        if (KA_IS_ERR(pool->threads[i])) {
            pool->threads[i] = NULL;
            goto err_destroy_pool;
        }

        ka_task_kthread_bind(pool->threads[i], 0);
        ka_task_wake_up_process(pool->threads[i]);
    }

    return pool;

err_destroy_pool:
    kthread_pool_destroy(pool);
    return NULL;
}

void kthread_pool_destroy(struct kthread_pool *pool)
{
    int i;
    struct kthread_task *task, *tmp;

    if (!pool) {
        return;
    }
    ka_task_spin_lock(&pool->lock);
    pool->stop = true;
    ka_task_spin_unlock(&pool->lock);

    for (i = 0; i < pool->num_threads; i++) {
        if (pool->threads[i]) {
            ka_task_kthread_stop(pool->threads[i]);
        }
        pool->threads[i] = NULL;
    }

    ka_task_spin_lock(&pool->lock);
    list_for_each_entry_safe(task, tmp, &pool->task_list, list)
    {
        ka_list_del(&task->list);
        devmm_kfree_ex(task);
    }
    ka_task_spin_unlock(&pool->lock);

    devmm_kfree_ex(pool->threads);
    devmm_kfree_ex(pool);
}

#ifndef EMU_ST
int kthread_pool_submit(struct kthread_pool *pool, void (*func)(void *), void *arg)
{
    struct kthread_task *task;

    if (!pool || !func || !arg)
        return -EINVAL;

    ka_task_spin_lock(&pool->lock);
    if (pool->stop) {
        ka_task_spin_unlock(&pool->lock);
        return -ESHUTDOWN;
    }

    task = devmm_kzalloc_ex(sizeof(*task), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (!task) {
        ka_task_spin_unlock(&pool->lock);
        return -ENOMEM;
    }

    KA_INIT_LIST_HEAD(&task->list);
    task->func = func;
    task->arg = arg;

    ka_list_add_tail(&task->list, &pool->task_list);
    ka_task_spin_unlock(&pool->lock);
    ka_task_wake_up(&pool->waitq);

    return 0;
}
#endif