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
#include "ka_ioctl_pub.h"
#include "ka_fs_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"

#include "kernel_version_adapt.h"
#include "pbl/pbl_davinci_api.h"
#include "dpa/dpa_apm_kernel.h"

#include "svm_kern_log.h"
#include "svm_ioctl_ex.h"
#include "va_mng.h"
#include "svm_mem_recycle.h"
#include "svm_mmu_notifier.h"
#include "framework_vma.h"
#include "svm_pgtable.h"
#include "svm_mmap_fops.h"

#define VMA_PRIVATE_STATUS_OFFSET 32

int svm_vma_may_split(ka_vm_area_struct_t *area, unsigned long addr);
int svm_vma_split(ka_vm_area_struct_t *area, unsigned long addr);

static void svm_parse_tgid_from_vma_priv_data(ka_vm_area_struct_t *vma, int *tgid)
{
    struct svm_mmu_notifier_ctx *mn_ctx = (struct svm_mmu_notifier_ctx *)ka_mm_get_vm_private_data(vma);
    *tgid = mn_ctx->tgid;
}

void svm_set_vma_status(ka_vm_area_struct_t *vma, int status)
{
    if (ka_mm_get_vm_private_data(vma) != NULL) {
        struct svm_mmu_notifier_ctx *mn_ctx = (struct svm_mmu_notifier_ctx *)ka_mm_get_vm_private_data(vma);
        mn_ctx->status = status;
    }
}

static void svm_invalid_vma(ka_vm_area_struct_t *vma)
{
    struct svm_mmu_notifier_ctx *mn_ctx = (struct svm_mmu_notifier_ctx *)ka_mm_get_vm_private_data(vma);
    svm_mmu_notifier_unregister(mn_ctx);
    ka_mm_set_vm_private_data(vma, NULL);
    ka_mm_set_vm_ops(vma, NULL);
}

static u64 svm_vma_mem_recycle(ka_vm_area_struct_t *vma)
{
    u64 recycle_size;
    int tgid;

    svm_parse_tgid_from_vma_priv_data(vma, &tgid);
    recycle_size = svm_mem_recycle(vma, tgid);
    svm_invalid_vma(vma);

    return recycle_size;
}

/*
 * Vm_operations_struct.open will called when split vma or copy vma.
 * The caller held mm semaphore already.
 * Vma has not yet been added to mm->vma_tree.
 */
static void svm_vma_open(ka_vm_area_struct_t *vma)
{
    ka_vm_area_struct_t *old_vma = NULL;
    u64 recycle_size;
    int ret;

    old_vma = ka_mm_find_vma(vma->vm_mm, ka_mm_get_vm_start(vma));
    if (old_vma == NULL) {
        svm_err("User improperly unmap, need to unmap memory, but not find old vma. (vm_start=0x%lx; vm_end=0x%lx)\n",
                ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma));
        return;
    }

    ret = svm_check_vma(old_vma, ka_mm_get_vm_start(old_vma), ka_mm_get_vm_end(old_vma) - ka_mm_get_vm_start(old_vma));
    if (ret != 0) {
        svm_err("User improperly unmap, need to unmap memory, but old vma check failed. "
                "(vm_start=0x%lx; vm_end=0x%lx; old vma: vm_start=0x%lx; vm_end=0x%lx)\n",
                ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma), ka_mm_get_vm_start(old_vma), ka_mm_get_vm_end(old_vma));
        return;
    }

    recycle_size = svm_vma_mem_recycle(old_vma);

    svm_err("User improperly unmap, recycle. (vm_start=0x%lx; vm_end=0x%lx;"
            " old vma: vm_start=0x%lx; vm_end=0x%lx; recycle_size=0x%llx)\n",
            ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma), ka_mm_get_vm_start(old_vma), ka_mm_get_vm_end(old_vma),
            recycle_size);

    /* Vma split will open new_vma, new_vma will inherit old_vma->vm_privat_data and vm_ops. */
    ka_mm_set_vm_private_data(vma, NULL);
    ka_mm_set_vm_ops(vma, NULL);
}

/*
 * The caller held mm semaphore already.
 * Vma is already del from mm->vma_tree.
 * The pgtable has already been released.
 */
static void svm_vma_close(ka_vm_area_struct_t *vma)
{
    svm_info("Vma close. (vm_start=0x%lx; vm_end=0x%lx)\n", ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma));
    if (ka_mm_get_vm_private_data(vma) != NULL) {
        svm_invalid_vma(vma);
    }
}

int svm_vma_may_split(ka_vm_area_struct_t *area, unsigned long addr)
{
    return -EINVAL;
}

int svm_vma_split(ka_vm_area_struct_t *area, unsigned long addr)
{
    return -EINVAL;
}

/* avoid mremap */
static int svm_vma_mremap(ka_vm_area_struct_t *area)
{
    return -EINVAL;
}

#define SVM_PRINT_TIME_INTERVAL_MS 1000 /* 1s */
static void svm_vma_fault_print_err(ka_vm_fault_struct_t *vmf)
{
    static unsigned long pre_stamp = 0;

    if (ka_system_jiffies_to_msecs(ka_jiffies - pre_stamp) > SVM_PRINT_TIME_INTERVAL_MS) {
        /* The log cannot be modified, because in the failure mode library. */
        svm_err("Svm_vma_fault. (fault_va=0x%llx; flags=0x%x)\n",
                (u64)(ka_mm_get_vm_start(vmf->vma) + (vmf->pgoff << KA_MM_PAGE_SHIFT)), vmf->flags);
        pre_stamp = ka_jiffies;
    }
}

#define SVM_FAULT_HANDLE_NUM 2
svm_fault_handle g_fault_handle[SVM_FAULT_HANDLE_NUM];

static void svm_vma_fault_handle_uninit(void)
{
    int i;

    for (i = 0; i < SVM_FAULT_HANDLE_NUM; i++) {
        g_fault_handle[i] = NULL;
    }
}

void svm_register_vma_fault_handle(svm_fault_handle handle)
{
    int i;

    for (i = 0; i < SVM_FAULT_HANDLE_NUM; i++) {
        if (g_fault_handle[i] == NULL) {
            g_fault_handle[i] = handle;
            return;
        }
    }

    svm_warn("Overflow.\n");
}

static ka_vm_fault_t svm_vma_fault_handle(ka_vm_area_struct_t *vma, ka_vm_fault_struct_t *vmf, int huge_fault_flag)
{
    int i;

    for (i = 0; i < SVM_FAULT_HANDLE_NUM; i++) {
        if (g_fault_handle[i] != NULL) {
            ka_vm_fault_t ret = g_fault_handle[i](vma, vmf, huge_fault_flag);
            if ((ret == SVM_FAULT_OK) || (ret == SVM_FAULT_RETRY)) {
                return ret;
            }
        }
    }

    svm_vma_fault_print_err(vmf);
    return SVM_FAULT_ERROR;
}

static ka_vm_fault_t _svm_vma_fault(ka_vm_area_struct_t *vma, ka_vm_fault_struct_t *vmf)
{
    return svm_vma_fault_handle(vma, vmf, 0);
}

KA_DEFINE_VM_OPS_FAULT_FUNC(_svm_vma_fault);

KA_DEFINE_VM_OPS_HUGE_FAULT_FUNC(svm_vma_fault_handle)

static int _svm_mkwrite(ka_vm_area_struct_t *vma, ka_vm_fault_struct_t *vmf)
{
    return svm_vma_fault_handle(vma, vmf, 0);
}

KA_DEFINE_VM_OPS_PFN_MKWRITE_FUNC(_svm_mkwrite)

static ka_vm_operations_struct_t _svm_vma_ops = {
    .open = svm_vma_open,
    .close = svm_vma_close,
    ka_vm_ops_init_may_split(svm_vma_may_split) ka_vm_ops_init_split(svm_vma_split)
        ka_vm_ops_init_mremap(svm_vma_mremap) ka_vm_ops_init_huge_fault(svm_vma_fault_handle)
            ka_vm_ops_init_fault(_svm_vma_fault) ka_vm_ops_init_pfn_mkwrite(_svm_mkwrite)};

bool svm_is_svm_vma(ka_vm_area_struct_t *vma)
{
    return (ka_mm_get_vm_ops(vma) == &_svm_vma_ops);
}

static int _svm_get_va_type(ka_vm_area_struct_t *vma, u64 va, u64 size, int *va_type)
{
    /* non svm va may has multi vmas, so not check size */
    if (svm_is_svm_vma(vma)) {
        if ((va >= ka_mm_get_vm_start(vma)) && (size <= (ka_mm_get_vm_end(vma) - ka_mm_get_vm_start(vma))) &&
            ((va + size) <= ka_mm_get_vm_end(vma))) {
            *va_type = VA_TYPE_SVM;
            return 0;
        }
    } else {
        if ((va >= ka_mm_get_vm_start(vma)) && (va < ka_mm_get_vm_end(vma))) {
            *va_type = VA_TYPE_NON_SVM;
            return 0;
        }
    }

    return -EINVAL;
}

int svm_check_vma(ka_vm_area_struct_t *vma, u64 va, u64 size)
{
    int va_type, ret;

    ret = _svm_get_va_type(vma, va, size, &va_type);
    if (ret != 0) {
        return ret;
    }

    return (va_type == VA_TYPE_SVM) ? 0 : -EINVAL;
}

int svm_get_current_task_va_type(u64 va, u64 size, int *va_type)
{
    ka_vm_area_struct_t *vma = NULL;
    int ret = -EINVAL;

    ka_task_down_read(get_mmap_sem(ka_task_get_current_mm()));
    vma = ka_mm_find_vma(ka_task_get_current_mm(), va);
    if (vma != NULL) {
        ret = _svm_get_va_type(vma, va, size, va_type);
    } else {
        svm_err("Find vma failed. (va=0x%llx; size=0x%llx)\n", va, size);
    }
    ka_task_up_read(get_mmap_sem(ka_task_get_current_mm()));

    return ret;
}

static int _svm_mmap(ka_file_t *file, ka_vm_area_struct_t *vma)
{
    u64 start, size, vma_start, vma_end;

    vma_start = ka_mm_get_vm_start(vma);
    vma_end = ka_mm_get_vm_end(vma);

    svm_get_va_range(&start, &size);

    if (svm_check_va_range(vma_start, vma_end - vma_start, start, start + size) != 0) {
        svm_get_pcie_th_va_range(&start, &size);
        if (svm_check_va_range(vma_start, vma_end - vma_start, start, start + size) != 0) {
            svm_info("Svm map va not in range. (vm_start=0x%llx; vm_end=0x%llx; vm_pgoff=0x%lx; vm_flags=0x%lx)\n",
                     vma_start, vma_end, vma->vm_pgoff, ka_mm_get_vm_flags(vma));
            return -EINVAL;
        }
    }

    ka_vm_flags_set(vma, ka_mm_get_vm_flags(vma) | KA_VM_DONTEXPAND | KA_VM_DONTDUMP | KA_VM_DONTCOPY | KA_VM_PFNMAP |
                             KA_VM_LOCKED | KA_VM_WRITE | KA_VM_IO);

    if ((SVM_IS_ALIGNED(vma_start, SVM_VA_RESERVE_ALIGN)) && (SVM_IS_ALIGNED(vma_end, SVM_VA_RESERVE_ALIGN))) {
        struct svm_mmu_notifier_ctx *mn_ctx = NULL;
        int ret = svm_mmu_notifier_register(ka_task_get_current_mm(), vma, &mn_ctx);
        if (ret != 0) {
            svm_err("Register failed. (vm_start=0x%llx; vm_end=0x%llx; ret=%d)\n", vma_start, vma_end, ret);
            return ret;
        }

        ka_mm_set_vm_private_data(vma, (void *)mn_ctx);
        ka_mm_set_vm_ops(vma, &_svm_vma_ops);
        svm_info("Mmap. (vm_start=0x%llx; vm_end=0x%llx)\n", vma_start, vma_end);
    } else {
        svm_info("Mmap not align. (vm_start=0x%llx; vm_end=0x%llx)\n", vma_start, vma_end);
    }

    return 0;
}

static int _svm_open(ka_inode_t *inode, ka_file_t *file)
{
    return 0;
}

static int _svm_release(ka_inode_t *inode, ka_file_t *file)
{
    return 0;
}

static long _svm_ioctl(ka_file_t *file, u32 cmd, unsigned long arg)
{
    return -EOPNOTSUPP;
}

static ka_file_operations_t svm_mmap_fops = {
    .owner = KA_THIS_MODULE,
    .open = _svm_open,
    .release = _svm_release,
    .unlocked_ioctl = _svm_ioctl,
    .mmap = _svm_mmap,
};

int svm_mmap_fops_init(void)
{
    int ret;

    ret = drv_davinci_register_sub_module(SVM_MMAP_CHAR_DEV_NAME, &svm_mmap_fops);
    if (ret != 0) {
        svm_err("Module register fail. (ret=%d)\n", ret);
        return ret;
    }

    ret = hal_kernel_apm_vma_check_register(SVM_MODULE, svm_is_svm_vma);
    if (ret != 0) {
        svm_err("Vma check register fail. (ret=%d)\n", ret);
        (void)drv_ascend_unregister_sub_module(SVM_MMAP_CHAR_DEV_NAME);
        return ret;
    }

    return 0;
}

void svm_mmap_fops_uninit(void)
{
    hal_kernel_apm_vma_check_unregister(SVM_MODULE);
    (void)drv_ascend_unregister_sub_module(SVM_MMAP_CHAR_DEV_NAME);
    svm_vma_fault_handle_uninit();
}