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
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_sched_pub.h"

#include "svm_kern_log.h"
#include "svm_pgtable.h"
#include "svm_mm.h"
#include "pmq.h"

#ifndef EMU_ST /* Simulation ST ignore pte */
struct svm_pmq_query_para {
    u64 target_pa;
    u64 matched_va;
};

static int svm_pmq_pte_entry(ka_pte_t *pte, u64 addr, u64 next, enum ka_pte_level level, struct ka_pgwalk *walk)
{
    struct svm_pmq_query_para *para = (struct svm_pmq_query_para *)walk->priv;
    u64 page_size, pa, pfn, offset, range_size;
    int ret;

    page_size = ka_pte_level_to_page_size(level);
    if (page_size == 0ULL) {
        return 0;
    }

    ret = svm_pte_to_pfn(pte, &pfn);
    if (ret != 0) {
        return 0;
    }

    pa = (unsigned long)KA_MM_PFN_PHYS(pfn);
    offset = addr - ka_base_round_down(addr, page_size);
    pa += offset;

    range_size = ka_base_min(next - addr, page_size - offset);
    if ((para->target_pa >= pa) && (para->target_pa < (pa + range_size))) {
        para->matched_va = addr + (para->target_pa - pa);
        return -EEXIST;
    }

    return 0;
}

static int _svm_pmq_query_va(ka_vm_area_struct_t *vma, u64 pa, u64 start_va, u64 end_va, u64 *matched_va)
{
    struct svm_pmq_query_para para = {
        .target_pa = pa,
        .matched_va = 0,
    };
    struct ka_pgwalk_ops ops = {NULL};

    ops.pte_entry = svm_pmq_pte_entry;

    (void)ka_walk_page_range(vma, start_va, end_va, &ops, (void *)&para);
    if (para.matched_va != 0) {
        *matched_va = para.matched_va;
        return 0;
    }

    return -ESRCH;
}
#else
int _svm_pmq_query_va(ka_vm_area_struct_t *vma, u64 pa, u64 start_va, u64 end_va, u64 *matched_va);
#endif

int svm_pmq_query_va(int tgid, u64 pa, u64 start_va, u64 end_va, u64 *matched_va)
{
    ka_vm_area_struct_t *vma = NULL;
    ka_mm_struct_t *mm = NULL;
    int ret;

    mm = svm_mm_get(tgid, false);
    if (mm == NULL) {
        return -ENOSYS;
    }

    vma = ka_mm_find_vma(mm, start_va);
    if (vma == NULL) {
        svm_mm_put(mm, false);
        return -EFAULT;
    }

    ret = _svm_pmq_query_va(vma, pa, start_va, end_va, matched_va);
    svm_mm_put(mm, false);
    return ret;
}