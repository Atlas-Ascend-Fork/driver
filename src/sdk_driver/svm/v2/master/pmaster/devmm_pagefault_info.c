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

#include "ka_type.h"

#include "devmm_proc_info.h"
#include "devmm_common.h"
#include "svm_heap_mng.h"
#include "svm_define.h"
#include "svm_ioctl.h"
#include "svm_master_proc_mng.h"
#include "svm_master_mem_share.h"
#include "svm_phy_addr_blk_mng.h"
#include "svm_log.h"
#include "devmm_pagefault_info.h"

/* Step b: find contiguous mapped sub-segment from end_va backwards within an allocated segment.
 *
 * Uses pfn (page frame number) to index page_bitmap directly -- no VA alignment ambiguity.
 * Scans from end_pfn towards start_pfn, finding the longest contiguous run of mapped pages
 * (bit 3 DEV_MAPPED or bit 4 HOST_MAPPED) closest to end_va.
 *
 * Returns 0 on success (out_seg_start/out_seg_end/out_bitmap filled), -EINVAL if no mapped page.
 */
static int devmm_find_mapped_seg_from_end(struct devmm_svm_process *svm_process, u64 alloc_start_va, u64 alloc_end_va,
                                          u64 *out_seg_start, u64 *out_seg_end, u32 *out_bitmap)
{
    struct devmm_svm_heap *heap = NULL;
    u64 page_size, start_pfn, end_pfn, pfn;
    u32 *page_bitmap = NULL;
    u64 mapped_end_pfn = 0;
    u64 mapped_start_pfn = 0;
    bool in_mapped_run = false;
    u32 stamp = (u32)ka_jiffies;

    heap = devmm_svm_get_heap(svm_process, alloc_end_va);
    if (heap == NULL || heap->page_bitmap == NULL) {
        return -EINVAL;
    }
    page_size = (u64)heap->chunk_page_size;
    page_bitmap = heap->page_bitmap;

    /* Convert VA to pfn -- floor division gives the page index, no alignment needed */
    start_pfn = (alloc_start_va - heap->start) / page_size;
    end_pfn = (alloc_end_va - heap->start) / page_size;

    /* Scan from end_pfn backwards to start_pfn */
    for (pfn = end_pfn; pfn + 1 > start_pfn; pfn--) {
        devmm_try_cond_resched(&stamp);
        if (devmm_page_bitmap_is_dev_mapped(&page_bitmap[pfn]) || devmm_page_bitmap_is_host_mapped(&page_bitmap[pfn])) {
            if (!in_mapped_run) {
                mapped_end_pfn = pfn; /* First mapped page (closest to end_va) */
                in_mapped_run = true;
                *out_bitmap = page_bitmap[pfn]; /* Bitmap value of rightmost mapped page */
            }
            mapped_start_pfn = pfn; /* Extend run leftward */
        } else {
            if (in_mapped_run) {
                break; /* End of contiguous run */
            }
        }
    }

    if (!in_mapped_run) {
        return -EINVAL;
    }

    /* Convert pfn to VA -- only at the end, no ambiguity */
    *out_seg_start = mapped_start_pfn * page_size + heap->start;
    *out_seg_end = (mapped_end_pfn + 1) * page_size + heap->start - 1;
    return 0;
}

/* Find the nearest valid (allocated + page-table-built) segment to the LEFT of fault_va.
 *
 * Two-step layered approach:
 *   Step a: devmm_check_alloced_va finds allocated segment; if heap not found,
 *           check_va -= DEVMM_HEAP_SIZE to jump to previous heap and retry.
 *   Step b: within allocated segment, find contiguous mapped sub-segment from end_va backwards.
 *           If no mapped page, continue to previous heap.
 *
 * Returns 0 on success, -EINVAL if no valid segment found.
 */
static int devmm_pagefault_find_left_valid_seg(struct devmm_svm_process *svm_process, u64 fault_va, u64 *out_seg_start,
                                               u64 *out_seg_end, u32 *out_mapped_bitmap)
{
    u64 check_va = fault_va;
    u64 alloc_start_va = 0;
    u64 alloc_end_va = 0;
    int alloc_ret;
    u64 mapped_seg_start = 0;
    u64 mapped_seg_end = 0;
    u32 mapped_bitmap = 0;
    int map_ret;
    u32 stamp = (u32)ka_jiffies;

    /* Step a: cross-heap search for allocated segment */
    while (true) {
        devmm_try_cond_resched(&stamp);
        alloc_ret = devmm_check_alloced_va(svm_process, check_va, &alloc_start_va, &alloc_end_va,
                                           DEVMM_PRE_ALLOCED_FLAG);
        if (alloc_ret != 0) {
            /* Heap not found or no allocated page -- jump to previous heap (1G step) */
            if (check_va < DEVMM_SVM_MEM_START + DEVMM_HEAP_SIZE) {
                return -EINVAL; /* Reached VA space lower limit */
            }
            check_va -= DEVMM_HEAP_SIZE;
            continue;
        }

        /* Step b: within allocated segment, find contiguous mapped sub-segment from end_va backwards */
        map_ret = devmm_find_mapped_seg_from_end(svm_process, alloc_start_va, alloc_end_va, &mapped_seg_start,
                                                 &mapped_seg_end, &mapped_bitmap);
        if (map_ret == 0) {
            /* Found contiguous mapped segment [mapped_seg_start, mapped_seg_end] */
            *out_seg_start = mapped_seg_start;
            *out_seg_end = mapped_seg_end;
            *out_mapped_bitmap = mapped_bitmap;
            return 0;
        }

        /* Allocated but no contiguous mapped page in this segment.
         * Do NOT jump to previous heap -- same heap may have other allocated segments
         * to the left of alloc_start_va. Step back by one page so step a can find
         * the next allocated segment within the same heap. */
        if (alloc_start_va <= DEVMM_SVM_MEM_START) {
            return -EINVAL; /* Reached VA space lower limit */
        }
        check_va = alloc_start_va - 1; /* Stay in same heap, find previous allocated segment */
    }
}

/* Extract shared flag from the mapped page's bitmap and heap type.
 *
 * Shared sources (only two):
 *   - IPC open: page_bitmap bit 0 (IPC_MEM_OPEN)
 *   - VMM import: SUB_RESERVE_TYPE heap -> VMMA -> map_node -> blk_type == IMPORT_TYPE
 * Bit 2 (REMOTE_MAPPED/NOSYNC_FLG) is NOT checked -- multiplexed bit, false-positive risk.
 */
static void devmm_pagefault_info_extract_shared(struct devmm_svm_process *svm_process, u64 mapped_va, u32 bitmap_val,
                                                struct svmPagefaultInfo *info)
{
    struct devmm_svm_heap *heap = devmm_svm_get_heap(svm_process, mapped_va);

    if ((heap != NULL) && (heap->heap_sub_type == SUB_RESERVE_TYPE)) {
        /* VMM heap: VMMA -> map_node -> blk_type */
        struct devmm_vmma_struct *vmma = devmm_vmma_get(&heap->vmma_mng, mapped_va);
        if (vmma != NULL) {
            struct devmm_share_id_map_node *map_node = devmm_share_id_map_node_get(svm_process, vmma->info.devid,
                                                                                   vmma->info.phy_addr_blk_id);
            if (map_node != NULL) {
                /* IMPORT_TYPE: imported from another process -> shared
                 * EXPORT_TYPE / NORMAL_TYPE: created by this process -> self */
                info->valid_range_is_shared = (map_node->blk_type == SVM_PYH_ADDR_BLK_IMPORT_TYPE) ? 1 : 0;
                devmm_share_id_map_node_put(map_node);
            }
            devmm_vmma_put(vmma);
        }
    } else {
        /* Non-VMM heap: only IPC open (bit 0) indicates shared */
        info->valid_range_is_shared = (bitmap_val & DEVMM_PAGE_IPC_MEM_OPEN_MASK) ? 1 : 0;
    }
}

/* Check if a record already exists for the given device. */
static bool devmm_pagefault_info_has_record(struct devmm_svm_process *svm_process, u32 devid)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_process->priv_data;
    struct devmm_pagefault_info_slot *slot = &master_data->pagefault_info_records[devid];
    bool has = false;

    ka_task_read_lock(&slot->rwlock);
    has = slot->has_record;
    ka_task_read_unlock(&slot->rwlock);
    return has;
}

/* Write svmPagefaultInfo into per-device record slot. */
static void devmm_pagefault_info_write_record(struct devmm_svm_process *svm_process, u32 devid,
                                              const struct svmPagefaultInfo *info)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_process->priv_data;
    struct devmm_pagefault_info_slot *slot = &master_data->pagefault_info_records[devid];

    ka_task_write_lock(&slot->rwlock);
    slot->info = *info;
    slot->has_record = true;
    ka_task_write_unlock(&slot->rwlock);
}

void devmm_pagefault_info_record_access(struct devmm_svm_process *svm_process, u32 devid, u64 fault_va)
{
    struct svmPagefaultInfo info = {0};
    u64 seg_start_va = 0;
    u64 seg_end_va = 0;
    u32 mapped_bitmap = 0;
    int seg_ret;

    if (devid >= SVM_MAX_AGENT_NUM) {
        devmm_drv_warn("Pagefault record devid out of range, skip. (devid=%u; va=0x%llx)\n", devid, fault_va);
        return;
    }

    /* Only record the first failure. Skip if a record already exists --
     * user must clear it via halMemCtl(CLEAR) before a new one can be recorded. */
    if (devmm_pagefault_info_has_record(svm_process, devid)) {
        return;
    }

    info.fault_va = fault_va;

    /* heap_sem read lock protects heap/page_bitmap/vmma access
     * (same pattern as devmm_chan_check_va_d2h_process at devmm_channel.c:349) */
    ka_task_down_read(&svm_process->heap_sem);

    /* Find left-nearest valid segment (allocated + page-table-built). */
    seg_ret = devmm_pagefault_find_left_valid_seg(svm_process, fault_va, &seg_start_va, &seg_end_va, &mapped_bitmap);
    if (seg_ret == 0) {
        /* Found valid segment. Use mapped_bitmap (the found mapped page's bitmap value,
         * NOT the segment first page's bitmap -- segment first page may not be mapped). */
        info.valid_range_start = seg_start_va;
        info.valid_range_size = seg_end_va - seg_start_va + 1;

        /* Side + devid from mapped page's bitmap bits 3/4/21-31.
         * mapped_bitmap is guaranteed to have bit 3 or bit 4 set
         * (devmm_find_mapped_seg_from_end only returns pages that are mapped). */
        if (mapped_bitmap & DEVMM_PAGE_HOST_MAPPED_MASK) {
            info.valid_range_side = MEM_HOST_SIDE;
            info.valid_range_devid = 0;
        } else {
            info.valid_range_side = MEM_DEV_SIDE;
            info.valid_range_devid = (mapped_bitmap >> DEVMM_PAGE_DEVID_SHIT) & DEVMM_PAGE_DEVID_MASK;
        }

        /* Shared judgment: only IPC open (bit 0) + VMM import (via VMMA->map_node).
         * NOT checking bit 2 (REMOTE_MAPPED/NOSYNC_FLG multiplexed bit). */
        devmm_pagefault_info_extract_shared(svm_process, seg_end_va, mapped_bitmap, &info);
    }

    ka_task_up_read(&svm_process->heap_sem);

    /* Write record to per-device slot -- no heap access needed, outside heap_sem */
    devmm_pagefault_info_write_record(svm_process, devid, &info);
}

void devmm_pagefault_info_clear_by_devid(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_svm_proc_master *master_data = NULL;
    struct devmm_pagefault_info_slot *slot = NULL;

    if (devid >= SVM_MAX_AGENT_NUM) {
        return;
    }

    master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    slot = &master_data->pagefault_info_records[devid];
    ka_task_write_lock(&slot->rwlock);
    (void)memset_s(&slot->info, sizeof(slot->info), 0, sizeof(slot->info));
    slot->has_record = false;
    ka_task_write_unlock(&slot->rwlock);
}

int devmm_ioctl_pagefault_info_query(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_pagefault_info_ioctl_para *para = &arg->data.pagefault_info_para;
    u32 devid = arg->head.devid;
    struct devmm_svm_proc_master *master_data = NULL;
    struct devmm_pagefault_info_slot *slot = NULL;

    if (devid >= SVM_MAX_AGENT_NUM) {
        devmm_drv_err("Pagefault info query devid out of range. (devid=%u)\n", devid);
        return -EINVAL;
    }

    master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    slot = &master_data->pagefault_info_records[devid];
    ka_task_read_lock(&slot->rwlock);
    if (!slot->has_record) {
        ka_task_read_unlock(&slot->rwlock);
        para->has_record = 0;
        return 0;
    }
    para->info = slot->info;
    para->has_record = 1;
    ka_task_read_unlock(&slot->rwlock);
    return 0;
}

int devmm_ioctl_pagefault_info_clear(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    u32 devid = arg->head.devid;
    struct devmm_svm_proc_master *master_data = NULL;
    struct devmm_pagefault_info_slot *slot = NULL;

    if (devid >= SVM_MAX_AGENT_NUM) {
        devmm_drv_err("Pagefault info clear devid out of range. (devid=%u)\n", devid);
        return -EINVAL;
    }

    master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    slot = &master_data->pagefault_info_records[devid];
    ka_task_write_lock(&slot->rwlock);
    (void)memset_s(&slot->info, sizeof(slot->info), 0, sizeof(slot->info));
    slot->has_record = false;
    ka_task_write_unlock(&slot->rwlock);
    return 0;
}

void devmm_pagefault_info_proc_init(struct devmm_svm_process *svm_proc)
{
    struct devmm_svm_proc_master *master_data = NULL;
    u32 i;

    if (svm_proc == NULL || svm_proc->priv_data == NULL) {
        return;
    }

    master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;
    for (i = 0; i < SVM_MAX_AGENT_NUM; i++) {
        ka_task_rwlock_init(&master_data->pagefault_info_records[i].rwlock);
        master_data->pagefault_info_records[i].has_record = false;
    }
}
