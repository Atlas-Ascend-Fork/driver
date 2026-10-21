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

#include "kernel_version_adapt.h"
#include "hdcdrv_cmd.h"
#include "hdcdrv_core_com_ub.h"
#include "hdcdrv_mem_com_ub.h"

STATIC int hdcdrv_follow_pfn_check(const void *ctx, ka_vm_area_struct_t *vma, unsigned long long user_va, u32 dev_id)
{
    unsigned long size = HDCDRV_UB_MEM_POOL_LEN;
    unsigned long end = user_va + KA_MM_PAGE_ALIGN(size);
    unsigned long va_check;
    unsigned long pfn;

    for (va_check = user_va; va_check < end; va_check += KA_MM_PAGE_SIZE) {
        if (ka_mm_follow_pfn(vma, va_check, &pfn) == 0) {
            hdcdrv_err("va_check is invalid. (ddr=%pK; size=%lu; va_check=%lx)\n", (void *)(uintptr_t)user_va, size,
                       va_check);
            return HDCDRV_PARA_ERR;
        }
    }

    return HDCDRV_OK;
}

int hdcdrv_check_va(const void *ctx, ka_vm_area_struct_t *vma, unsigned long long user_va, u32 dev_id)
{
    unsigned long size = HDCDRV_UB_MEM_POOL_LEN;
    unsigned long end = user_va + KA_MM_PAGE_ALIGN(size);

    if (ka_mm_get_vm_private_data(vma) != ctx) {
        hdcdrv_err("addr %pK ka_mm_get_vm_private_data() %pK ctx %pK\n", (void *)(uintptr_t)user_va,
                   ka_mm_get_vm_private_data(vma), ctx);
        return HDCDRV_PARA_ERR;
    }

    if ((user_va & (KA_MM_PAGE_SIZE - 1)) != 0) {
        hdcdrv_err("Input parameter is invalid. (user_va=0x%llx;)\n", user_va);
        return HDCDRV_PARA_ERR;
    }

    if ((user_va < ka_mm_get_vm_start(vma)) || (user_va > ka_mm_get_vm_end(vma)) || (end > ka_mm_get_vm_end(vma)) ||
        (user_va >= end)) {
        hdcdrv_err("Input parameter is invalid. (user_va=0x%llx;dev_id=%u;)\n", user_va, dev_id);
        return HDCDRV_PARA_ERR;
    }

    return HDCDRV_OK;
}

int hdcdrv_remap_mem_pool_va(const void *ctx, struct hdcdrv_mem_pool *mem_pool, int dev_id, unsigned long long user_va)
{
    int ret;
    ka_vm_area_struct_t *vma = NULL;

    ka_task_down_write(get_mmap_sem(ka_task_get_current_mm()));
#ifndef EMU_ST
    vma = ka_mm_find_vma(ka_task_get_current_mm(), user_va);
#else
    vma = hdc_find_vma_from_stub(ka_task_get_current_mm(), user_va);
#endif
    if (vma == NULL) {
        ka_task_up_write(get_mmap_sem(ka_task_get_current_mm()));
        hdcdrv_err("Find vma failed. (devid=%d)\n", dev_id);
        return HDCDRV_FIND_VMA_FAIL;
    }

    ret = hdcdrv_check_va(ctx, vma, user_va, dev_id);
    if (ret != HDCDRV_OK) {
        ka_task_up_write(get_mmap_sem(ka_task_get_current_mm()));
        return ret;
    }

    ret = hdcdrv_follow_pfn_check(ctx, vma, user_va, dev_id);
    if (ret != HDCDRV_OK) {
        ka_task_up_write(get_mmap_sem(ka_task_get_current_mm()));
        return ret;
    }

    ka_mm_set_vm_flags(vma, KA_VM_IO | KA_VM_SHARED);
    ka_mm_set_vm_pgprot(vma);

#ifndef EMU_ST
    ret = ka_mm_remap_pfn_range(vma, user_va, ka_mm_page_to_pfn(mem_pool->page), HDCDRV_UB_MEM_POOL_LEN,
                                *(ka_mm_get_vm_pgprot(vma)));
#else
    ret = hdc_remap_pfn_range_stub(vma, user_va, ka_mm_page_to_pfn(mem_pool->page), HDCDRV_UB_MEM_POOL_LEN,
                                   *(ka_mm_get_vm_pgprot(vma)));
#endif
    if (ret != 0) {
        ka_task_up_write(get_mmap_sem(ka_task_get_current_mm()));
        return HDCDRV_DMA_MPA_FAIL;
    }

    mem_pool->va = user_va;
    ka_task_up_write(get_mmap_sem(ka_task_get_current_mm()));
    return 0;
}

int hdcdrv_unmap_mem_pool_va(const void *ctx, struct hdcdrv_mem_pool *mem_pool, u32 dev_id)
{
    ka_vm_area_struct_t *vma = NULL;
    ka_struct_pid_t *pro_pid = NULL;
    ka_task_struct_t *task = NULL;
    ka_mm_struct_t *task_mm = NULL;
    int ret = 0;

    pro_pid = ka_task_find_get_pid(mem_pool->vnr);
    if (pro_pid == NULL) {
        hdcdrv_err("ka_task_find_get_pid failed.(dev_id=%u; vnr=%d; pid=%llu)\n", dev_id, mem_pool->vnr, mem_pool->pid);
        return HDCDRV_ERR;
    }

    task = ka_task_get_pid_task(pro_pid, KA_PIDTYPE_PID);
    if (task == NULL) {
        ret = HDCDRV_ERR;
        hdcdrv_err("ka_task_get_pid_task failed.(dev_id=%u)\n", dev_id);
        goto unmap_put_pid;
    }

    task_mm = ka_task_get_task_mm(task);
    if (task_mm == NULL) {
        ret = HDCDRV_ERR;
        hdcdrv_err("ka_task_get_task_mm failed.(dev_id=%u)\n", dev_id);
        goto unmap_put_task_struct;
    }

    ka_task_down_read(get_mmap_sem(task_mm));
#ifndef EMU_ST
    vma = ka_mm_find_vma(task_mm, mem_pool->va);
#else
    vma = hdc_find_vma_from_stub(task_mm, mem_pool->va);
#endif
    if (vma == NULL) {
        ret = HDCDRV_ERR;
        hdcdrv_err("ka_mm_find_vma is NULL.(dev_id=%u)\n", dev_id);
        goto unmap_up_read;
    }

    ret = hdcdrv_check_va(ctx, vma, mem_pool->va, dev_id);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("hdcdrv_check_va failed.(dev_id=%u)\n", dev_id);
        goto unmap_up_read;
    }

    (void)ka_mm_zap_vma_ptes(vma, mem_pool->va, HDCDRV_UB_MEM_POOL_LEN);

unmap_up_read:
    ka_task_up_read(get_mmap_sem(task_mm));
    ka_mm_mmput(task_mm);
unmap_put_task_struct:
    ka_task_put_task_struct(task);
unmap_put_pid:
    ka_task_put_pid(pro_pid);

    return ret;
}
