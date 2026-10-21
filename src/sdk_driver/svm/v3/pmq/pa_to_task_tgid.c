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
#include "ka_kernel_def_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_common_pub.h"
#include "ka_sched_pub.h"

#include "ascend_kernel_hal.h"
#include "dpa_kernel_interface.h"

#include "pbl_ka_mem_query.h"
#include "pbl_uda.h"
#include "pmq.h"
#include "framework_task.h"
#include "svm_kernel_interface.h"
#include "svm_kern_log.h"
#include "svm_pgtable.h"
#include "svm_cgroup.h"
#include "svm_slab.h"
#include "pmm.h"

struct pmq_query_pfn_owner_info {
    int tgid;
    u64 pfn;
    ka_list_head_t *head;
};

#define SVM_MAX_DEV_PID_CNT 64U
int hal_kernel_svm_query_devpid_by_pfn(u64 pfn, int *devpid)
{
    struct devmm_pfn_owner_info *owner_info = NULL;
    ka_list_head_t head = KA_LIST_HEAD_INIT(head);

    if (devpid == NULL) {
        svm_err("Devpid is NULL.\n");
        return -EPERM;
    }

    if (hal_kernel_svm_query_pfn_owner_info(0, pfn, &head) == 0) {
        owner_info = ka_list_first_entry(&head, struct devmm_pfn_owner_info, list);
        *devpid = owner_info->dev_pid;
        hal_kernel_svm_clear_pfn_owner_info_list(&head);
        return 0;
    }

    return -ESRCH;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_svm_query_devpid_by_pfn);

static int svm_add_matched_va_to_list(int tgid, u64 va, ka_list_head_t *head)
{
    struct devmm_pfn_owner_info *entry = NULL;
    u64 page_size = svm_va_to_page_size(tgid, va);

    entry = svm_kvmalloc(sizeof(struct devmm_pfn_owner_info), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (entry == NULL) {
        svm_err("Kvalloc failed. (size=%lu)\n", sizeof(struct devmm_pfn_owner_info));
        return -ENOMEM;
    }

    entry->dev_pid = tgid;
    entry->va = ka_base_round_down(va, page_size);
    entry->size_shift = svm_page_size_to_page_shift(page_size);

    svm_debug("search succ. (devpid=%d; va=0x%llx; size_shift=0x%u)\n", entry->dev_pid, entry->va, entry->size_shift);

    ka_list_add_tail(&entry->list, head);

    return 0;
}

static int svm_pmm_handle_of_pfn_to_task(void *priv, u64 va, u64 size)
{
    struct pmq_query_pfn_owner_info *info = (void *)priv;
    unsigned long stamp = (unsigned long)ka_jiffies;
    u64 start_va, matched_va;
    u64 end_va = va + size;
    int ret;

    for (start_va = va; start_va < end_va; start_va = matched_va + 1) {
        ret = svm_pmq_query_va(info->tgid, KA_MM_PFN_PHYS(info->pfn), start_va, end_va, &matched_va);
        if (ret != 0) {
            return 0;
        }

        ret = svm_add_matched_va_to_list(info->tgid, matched_va, info->head);
        if (ret != 0) {
            return ret;
        }
        ka_try_cond_resched(&stamp);
    }

    return 0;
}

static int svm_search_va_match_with_pfn_per_proc(u32 udevid, int tgid, u64 pfn, ka_list_head_t *head)
{
    struct pmq_query_pfn_owner_info info = {.tgid = tgid, .pfn = pfn, .head = head};
    ka_mem_cgroup_t *old_memcg = NULL;
    ka_mem_cgroup_t *memcg = NULL;
    int ret;

    ret = svm_cur_memcg_change(tgid, &old_memcg, &memcg);
    if (ret != 0) {
        return ret;
    }

    ret = pmm_for_each_seg(udevid, tgid, svm_pmm_handle_of_pfn_to_task, (void *)&info);
    if (ret != 0) {
        hal_kernel_svm_clear_pfn_owner_info_list(head);
    }
    svm_cur_memcg_reset(old_memcg, memcg);
    return ret;
}

int hal_kernel_svm_query_pfn_owner_info(u32 devid, u64 pfn, ka_list_head_t *head)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    u32 dev_num = uda_get_udev_max_num();
    u32 udevid, i;
    int ret;

    if ((head == NULL) || !ka_list_empty_careful(head)) {
        svm_err("List_head is invalid. (head_is_null=%d)\n", head == NULL);
        return -EPERM;
    }

    ka_task_might_sleep();

    for (udevid = 0; udevid < dev_num; udevid++) {
        u32 task_max_num = svm_get_task_max_num(udevid);
        int *tgids = NULL;

        ka_try_cond_resched(&stamp);

        if (task_max_num == 0) {
            continue;
        }

        tgids = svm_vzalloc(sizeof(int) * task_max_num);
        if (tgids == NULL) {
            svm_err("Vzalloc failed. (task_max_num=%u)\n", task_max_num);
            continue;
        }

        svm_get_all_task_tgids(udevid, tgids, task_max_num);
        for (i = 0; i < task_max_num; ++i) {
            processType_t proc_type;

            ka_try_cond_resched(&stamp);
            if (tgids[i] == 0) {
                break;
            }

            ret = apm_query_proc_type_by_slave(tgids[i], &proc_type);
            if ((ret != 0) || (proc_type != PROCESS_CP1)) { /* Process may exit already. */
                continue;
            }

            ret = svm_search_va_match_with_pfn_per_proc(udevid, tgids[i], pfn, head);
            if (ret != 0) {
                continue;
            }
        }
        svm_vfree(tgids);
    }
    return (ka_list_empty_careful(head) == 0) ? 0 : -ESRCH;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_svm_query_pfn_owner_info);

void hal_kernel_svm_clear_pfn_owner_info_list(ka_list_head_t *head)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;

    if ((head == NULL) || ka_list_empty_careful(head)) {
        return;
    }

    ka_task_might_sleep();
    ka_list_for_each_safe(pos, n, head)
    {
        struct devmm_pfn_owner_info *node = ka_list_entry(pos, struct devmm_pfn_owner_info, list);
        ka_list_del(&node->list);
        svm_kvfree(node);
        ka_try_cond_resched(&stamp);
    }
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_svm_clear_pfn_owner_info_list);