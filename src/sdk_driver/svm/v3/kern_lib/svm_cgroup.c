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
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"

#include "kernel_version_adapt.h"

#include "svm_mm.h"
#include "svm_cgroup.h"

static inline ka_mem_cgroup_t *svm_set_active_memcg(ka_mem_cgroup_t *memcg)
{
    ka_mem_cgroup_t *old = NULL;

    old = ka_task_get_current_active_memcg();
    ka_task_set_current_active_memcg(memcg);

    return old;
}

int svm_cur_memcg_change(ka_pid_t pid, ka_mem_cgroup_t **old_cg, ka_mem_cgroup_t **new_cg)
{
    ka_mm_struct_t *mm = NULL;

    mm = svm_mm_get(pid, false);
    if (mm == NULL) {
        return -ESRCH;
    }

    *new_cg = ka_mm_get_mem_cgroup_from_mm(mm);
    if (*new_cg == NULL) {
        svm_mm_put(mm, false);
        return -ESRCH;
    }

    *old_cg = svm_set_active_memcg(*new_cg);
    svm_mm_put(mm, false);
    return 0;
}

void svm_cur_memcg_reset(ka_mem_cgroup_t *old_cg, ka_mem_cgroup_t *new_cg)
{
    (void)svm_set_active_memcg(old_cg);

    if (new_cg != NULL) {
        ka_mm_mem_cgroup_put(new_cg);
    }
}
