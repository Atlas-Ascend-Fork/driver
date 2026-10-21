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

#ifndef _UDIS_TIMER_H_
#define _UDIS_TIMER_H_

#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_base_pub.h"

#define UDIS_TIMER_TASK_MAX_NUM 2048
#define TASK_NAME_MAX_LEN 16

enum udis_timer_work_type {
    COMMON_WORK = 0,
    INDEPENDENCE_WORK,
    UDIS_WORK_TYPE_MAX
};

struct udis_timer_task {
    unsigned int period_ms;
    unsigned int cur_ms;
    enum udis_timer_work_type work_type;
    char task_name[TASK_NAME_MAX_LEN];
    unsigned long privilege_data;
    int (*period_task_func)(unsigned int udevid, unsigned long privilege_data);
};

struct udis_period_task_node {
    ka_list_head_t node;
    ka_rcu_head_t rcu;
    ka_work_struct_t free_work;
    char task_name[TASK_NAME_MAX_LEN];
    unsigned long privilege_data;
    int (*period_task_func)(unsigned int udevid, unsigned long privilege_data);
    unsigned int original_expired_cnt;
    ka_atomic_t expired_cnt;
    unsigned int cur_cnt;
    unsigned int udevid;
    enum udis_timer_work_type work_type;
    ka_atomic_t queueflag;
    ka_workqueue_struct_t *workqueue;
    ka_work_struct_t work;
};

struct udis_timer {
    ka_hrtimer_t timer;
    ka_mutex_t task_list_lock;
    ka_list_head_t period_task_list;
    unsigned int task_num;
    ka_workqueue_struct_t *common_wq;
};

int udis_timer_init(void);
void udis_timer_uninit(void);
int hal_kernel_register_period_task(unsigned int udevid, const struct udis_timer_task *timer_task);
int hal_kernel_unregister_period_task(unsigned int udevid, const char *task_name);

#endif