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
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_common_pub.h"

#include "kernel_version_adapt.h"
#include "pbl_task_ctx.h"
#include "svm_mm.h"

ka_mm_struct_t *svm_mm_get(int tgid, bool wlock)
{
    ka_task_struct_t *tsk = NULL;
    ka_mm_struct_t *mm = NULL;

    tsk = task_get_by_tgid(tgid);
    if (tsk == NULL) {
        return NULL;
    }

    mm = ka_task_get_task_mm(tsk);
    if (mm == NULL) {
        goto task_put;
    }

    if (wlock) {
        ka_task_down_write(get_mmap_sem(mm));
    } else {
        ka_task_down_read(get_mmap_sem(mm));
    }

    /* Keep the mm reference until the mmap lock is released in svm_mm_put. */
task_put:
    ka_task_put_task_struct(tsk);
    return mm;
}

void svm_mm_put(ka_mm_struct_t *mm, bool wlock)
{
    if (wlock) {
        ka_task_up_write(get_mmap_sem(mm));
    } else {
        ka_task_up_read(get_mmap_sem(mm));
    }
    ka_mm_mmput(mm);
}
