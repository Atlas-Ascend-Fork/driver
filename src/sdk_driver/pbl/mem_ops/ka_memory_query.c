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

#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"

#include "pbl_ka_mem_query.h"

static struct svm_mem_query_ops g_svm_mem_query_ops = {0};
static KA_TASK_DECLARE_RWSEM(g_svm_mem_query_lock);

int hal_kernel_get_mem_pa_list(u32 devid, int tgid, struct ka_mem_attr *mem, u64 *pa_num, struct ka_pa_wraper *pa_list)
{
    int ret = -EFAULT;
    ka_task_down_read(&g_svm_mem_query_lock);
    if (g_svm_mem_query_ops.get_svm_mem_pa != NULL) {
        ret = g_svm_mem_query_ops.get_svm_mem_pa(devid, tgid, mem, pa_num, pa_list);
    }

    ka_task_up_read(&g_svm_mem_query_lock);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_mem_pa_list);

int hal_kernel_put_mem_pa_list(u32 devid, int tgid, struct ka_mem_attr *mem, u64 pa_num, struct ka_pa_wraper *pa_list)
{
    int ret = -EFAULT;
    ka_task_down_read(&g_svm_mem_query_lock);
    if (g_svm_mem_query_ops.put_svm_mem_pa != NULL) {
        ret = g_svm_mem_query_ops.put_svm_mem_pa(devid, tgid, mem, pa_num, pa_list);
    }

    ka_task_up_read(&g_svm_mem_query_lock);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_put_mem_pa_list);

u32 hal_kernel_get_mem_page_size(u32 devid, int tgid, struct ka_mem_attr *mem)
{
    u32 page_size = 0;

    ka_task_down_read(&g_svm_mem_query_lock);
    if (g_svm_mem_query_ops.get_svm_mem_page_size != NULL) {
        page_size = g_svm_mem_query_ops.get_svm_mem_page_size(devid, tgid, mem);
    }

    ka_task_up_read(&g_svm_mem_query_lock);
    return page_size;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_mem_page_size);

int hal_kernel_register_mem_query_ops(struct svm_mem_query_ops *query_ops)
{
    if ((query_ops == NULL) || (query_ops->get_svm_mem_pa == NULL) || (query_ops->put_svm_mem_pa == NULL)) {
        return -EINVAL;
    }

    ka_task_down_write(&g_svm_mem_query_lock);

    g_svm_mem_query_ops.get_svm_mem_pa = query_ops->get_svm_mem_pa;
    g_svm_mem_query_ops.put_svm_mem_pa = query_ops->put_svm_mem_pa;
    g_svm_mem_query_ops.get_svm_mem_page_size = query_ops->get_svm_mem_page_size;

    ka_task_up_write(&g_svm_mem_query_lock);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_register_mem_query_ops);

int hal_kernel_unregister_mem_query_ops(void)
{
    ka_task_down_write(&g_svm_mem_query_lock);

    g_svm_mem_query_ops.get_svm_mem_pa = NULL;
    g_svm_mem_query_ops.put_svm_mem_pa = NULL;
    g_svm_mem_query_ops.get_svm_mem_page_size = NULL;

    ka_task_up_write(&g_svm_mem_query_lock);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_unregister_mem_query_ops);
