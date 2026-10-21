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

#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include "ka_task_pub.h"

bool hw_vdavinci_changed_cpu(ka_task_struct_t *p, const ka_cpumask_t *next_mask);
bool hw_vdavinci_scheduled(struct hw_vdavinci *vdavinci, unsigned long current_pages, unsigned long max_pages,
                           unsigned int timeout, ka_page_t *page);
bool get_node_cpu_by_page(struct hw_vdavinci *vdavinci, unsigned int current_cpu, ka_page_t *page,
                          ka_cpumask_t *cpumask);
static inline void hw_vdavinci_cancel_work_sync(ka_work_struct_t *work)
{
    if (work == NULL || work->func == NULL) {
        return;
    }
    (void)ka_task_cancel_work_sync(work);
}

#endif
