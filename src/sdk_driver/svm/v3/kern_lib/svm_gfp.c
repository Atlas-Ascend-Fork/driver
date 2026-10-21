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

#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_common_pub.h"
#include "ka_sched_pub.h"

#include "pbl_chip_config.h"
#include "pbl_runenv_config.h"

#include "svm_kern_log.h"
#include "svm_addr_desc.h"
#include "svm_gfp.h"
#include "svm_pub.h"

void svm_page_ref_dec(ka_page_t *pg, void (*clear_func)(ka_page_t *pg), void (*dec_func)(ka_page_t *pg))
{
    int ref;

    ka_mm_lock_page(pg);
    ref = ka_mm_page_count(pg);
    if (ref > 1) {
        dec_func(pg);
        ka_mm_unlock_page(pg);
    } else {
        ka_mm_unlock_page(pg);
        /* Clear user data for security. */
        if (clear_func != NULL) {
            clear_func(pg);
        }
        dec_func(pg);
    }
}

static int (*get_nids)(u32 udevid, u32 memtype, u32 sub_memtype, int nids[], int *out_num) = NULL;
void svm_register_numa_id_handle(int (*handle)(u32 udevid, u32 memtype, u32 sub_memtype, int nids[], int *out_num))
{
    get_nids = handle;
}

static inline int svm_get_nids_by_type(u32 udevid, u32 memtype, u32 sub_memtype, int nids[], int *out_num)
{
    if (get_nids != NULL) {
        return get_nids(udevid, memtype, sub_memtype, nids, out_num);
    }

    *out_num = 1;
    nids[0] = KA_NUMA_NO_NODE;
    return 0;
}

static inline int svm_get_fixed_nids(u32 numa_id, int nids[], int *out_num)
{
    if (ka_mm_node_online(numa_id) == 0) {
        svm_err("Numa not online. (numa_id=%u)\n", numa_id);
        return -EINVAL;
    }

    *out_num = 1;
    nids[0] = numa_id;
    return 0;
}

static int svm_get_nids(u32 udevid, u32 flag, int nids[], int *out_num)
{
    if ((flag & SVM_GFP_FLAG_FIXED_NUMA) != 0) {
        return svm_get_fixed_nids(gfp_flag_get_numa_id(flag), nids, out_num);
    } else {
        u32 sub_memtype = ((flag & SVM_GFP_FLAG_P2P) != 0) ? DBL_SUB_MEMTYPE_P2P : DBL_SUB_MEMTYPE_AI;
        return svm_get_nids_by_type(udevid, DBL_MEMTYPE_HBM, sub_memtype, nids, out_num);
    }
}

static struct svm_page_ops *page_ops[SVM_PAGE_GRAN_MAX] = {
    NULL,
};
static svm_print_nodes_info_ops print_nodes_info_ops = NULL;

void svm_register_page_ops(enum svm_page_granularity gran, const struct svm_page_ops *ops)
{
    page_ops[gran] = (struct svm_page_ops *)ops;
}

void svm_register_print_nodes_info_ops(svm_print_nodes_info_ops print_ops)
{
    print_nodes_info_ops = print_ops;
}

static void svm_print_nodes_info(void)
{
    if (print_nodes_info_ops != NULL) {
        print_nodes_info_ops();
    }
}

int svm_alloc_pages(u32 udevid, enum svm_page_granularity gran, ka_page_t **pages, u64 pg_num, u32 flag)
{
    int nids[DBL_NUMA_ID_MAX_NUM] = {0};
    int nid_num;
    int ret;

    if (gran >= SVM_PAGE_GRAN_MAX) {
        svm_err("Invalid gran. (gran=%u)\n", gran);
        return -EINVAL;
    }

    if (page_ops[gran] == NULL) {
        return -EOPNOTSUPP;
    }

    ret = svm_get_nids(udevid, flag, nids, &nid_num);
    if (ret != 0) {
        svm_err("Get nids failed. (ret=%d; udevid=%u; flag=0x%x)\n", ret, udevid, flag);
        return ret;
    }

    ret = page_ops[gran]->alloc(nids, nid_num, pages, pg_num, flag);
    if (ret != 0) {
        svm_print_nodes_info();
    }

    return ret;
}

void svm_free_pages(enum svm_page_granularity gran, ka_page_t **pages, u64 pg_num, u32 flag)
{
    if (gran >= SVM_PAGE_GRAN_MAX) {
        svm_err("Invalid gran. (gran=%u)\n", gran);
        return;
    }

    if (page_ops[gran] == NULL) {
        return;
    }

    return page_ops[gran]->free(pages, pg_num, flag);
}

static inline bool svm_pa_is_continue(u64 prev_pa, u64 post_pa, u64 prev_size)
{
    return ((prev_pa + prev_size) == post_pa);
}

u64 svm_make_pa_continues(struct svm_pa_seg *pa_seg, u64 seg_num)
{
    u64 size;
    u64 i, j, continues_seg_num;
    unsigned long stamp = (unsigned long)ka_jiffies;

    continues_seg_num = 0;

    for (i = 0; i < seg_num; i = j) {
        ka_try_cond_resched(&stamp);
        size = pa_seg[i].size;
        for (j = i + 1; j < seg_num; j++) {
            ka_try_cond_resched(&stamp);
            if (!svm_pa_is_continue(pa_seg[j - 1].pa, pa_seg[j].pa, pa_seg[j - 1].size)) {
                break;
            }
            size += pa_seg[j].size;
        }

        pa_seg[continues_seg_num].pa = pa_seg[i].pa;
        pa_seg[continues_seg_num].size = size;
        continues_seg_num++;
    }

    return continues_seg_num;
}

u64 svm_get_pa_size(struct svm_pa_seg *pa_seg, u64 seg_num)
{
    u64 i, size;

    size = 0;
    for (i = 0; i < seg_num; i++) {
        size += pa_seg[i].size;
    }

    return size;
}

/* This func is time-consuming operation, may cause performance problem. */
bool svm_pa_is_local_mem(u64 pa)
{
    return ka_mm_mem_is_ram(pa);
}

u64 svm_get_page_size_by_pa_seg(u64 va, u64 size, struct svm_pa_seg *pa_seg, u64 seg_num)
{
    u64 total_seg_size = 0;
    u64 page_size;
    bool is_local_mem = svm_pa_is_local_mem(pa_seg[0].pa);
    bool is_huge_page = true;
    bool is_giant_page = true;
    /* The OS does not have an interface for creating giant page tables based on PA;
       it only supports creating them based on pages. Therefore, it degrades to the huge page size. */
    u64 giant_page_size = is_local_mem ? SVM_GPAGE_SIZE : KA_HPAGE_SIZE;
    u64 i;

    for (i = 0; i < seg_num; i++) {
        if (((pa_seg[i].pa % SVM_GPAGE_SIZE) != 0) || ((pa_seg[i].size % SVM_GPAGE_SIZE) != 0)) {
            is_giant_page = false;
        }

        if (((pa_seg[i].pa % KA_HPAGE_SIZE) != 0) || ((pa_seg[i].size % KA_HPAGE_SIZE) != 0)) {
            is_huge_page = false;
        }

        if (((pa_seg[i].pa % KA_MM_PAGE_SIZE) != 0) || ((pa_seg[i].size % KA_MM_PAGE_SIZE) != 0)) {
            svm_err("Invalid pa seg. (size=%llx; i=%llu; seg_num=%llu)\n", pa_seg[i].size, i, seg_num);
            return 0;
        }

        total_seg_size += pa_seg[i].size;
    }

    if (size != total_seg_size) {
        svm_err("Invalid pa seg. (size=%llx; total_seg_size=%llx; seg_num=%llu)\n", size, total_seg_size, seg_num);
        return 0;
    }

    if ((is_giant_page) && (SVM_IS_ALIGNED(va, giant_page_size))) {
        page_size = giant_page_size;
    } else if ((is_huge_page) && (SVM_IS_ALIGNED(va, KA_HPAGE_SIZE))) {
        page_size = KA_HPAGE_SIZE;
    } else {
        page_size = KA_MM_PAGE_SIZE;
    }

#ifdef DRV_HOST
    /* host only support normal page table */
    page_size = KA_MM_PAGE_SIZE;
#endif

    if ((!SVM_IS_ALIGNED(va, page_size)) || (!SVM_IS_ALIGNED(size, page_size))) {
        svm_err("Not align. (va=%llx; size=%llx; page_size=%llx)\n", va, size, page_size);
        return 0;
    }

    return page_size;
}

u64 svm_get_max_page_size_by_pa_seg(struct svm_pa_seg *pa_seg, u64 seg_num)
{
    bool is_local_mem = svm_pa_is_local_mem(pa_seg[0].pa);
    bool is_huge_page = true;
    bool is_giant_page = true;
    /* The OS does not have an interface for creating giant page tables based on PA;
    it only supports creating them based on pages. Therefore, it degrades to the huge page size. */
    u64 giant_page_size = is_local_mem ? SVM_GPAGE_SIZE : KA_HPAGE_SIZE;
    u64 i;

    for (i = 0; i < seg_num; i++) {
        if (((pa_seg[i].pa % SVM_GPAGE_SIZE) != 0) || ((pa_seg[i].size % SVM_GPAGE_SIZE) != 0)) {
            is_giant_page = false;
        }

        if (((pa_seg[i].pa % KA_HPAGE_SIZE) != 0) || ((pa_seg[i].size % KA_HPAGE_SIZE) != 0)) {
            is_huge_page = false;
        }

        if (((pa_seg[i].pa % KA_MM_PAGE_SIZE) != 0) || ((pa_seg[i].size % KA_MM_PAGE_SIZE) != 0)) {
            return 0;
        }
    }

    return is_giant_page ? giant_page_size : (is_huge_page ? KA_HPAGE_SIZE : KA_MM_PAGE_SIZE);
}

u64 svm_get_same_page_size_seg_num_by_pa_seg(u64 va, struct svm_pa_seg *pa_seg, u64 seg_num, u64 *seg_sum_size,
                                             u64 *page_size)
{
    u64 pre_page_size = 0;
    u64 cur_va = va;
    u64 i;

    for (i = 0; i < seg_num; i++) {
        u64 cur_page_size = svm_get_page_size_by_pa_seg(cur_va, pa_seg[i].size, &pa_seg[i], 1);
        if (cur_page_size == 0) {
            svm_warn("Invalid pa seg. (va=0x%llx; size=0x%llx; offset=%llu)\n", cur_va, pa_seg[i].size, i);
            return 0;
        }

        if ((pre_page_size != 0) && (pre_page_size != cur_page_size)) {
            break;
        }
        pre_page_size = cur_page_size;
        cur_va += pa_seg[i].size;
    }
    *page_size = pre_page_size;
    *seg_sum_size = cur_va - va;
    return i;
}
