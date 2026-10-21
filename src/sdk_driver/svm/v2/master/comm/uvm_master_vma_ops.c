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

#include "devmm_proc_info.h"
#include "svm_proc_mng.h"
#include "devmm_common.h"
#include "svm_kernel_msg.h"
#include "devmm_page_cache.h"
#include "devmm_addr_mng.h"
#include "svm_heap_mng.h"
#include "svm_proc_gfp.h"
#include "svm_dynamic_addr.h"
#include "svm_define.h"
#include "svm_msg_client.h"
#include "uvm_heap_mng.h"
#include "uvm_master_common.h"
#include "svm_phy_addr_blk_mng.h"

#ifndef EMU_ST
static void uvm_log_oom_info(struct devmm_svm_process *svm_proc, uint64_t start, uint64_t page_num)
{
    struct ka_sysinfo mem_info = {0};
    ka_si_meminfo(&mem_info);
    devmm_drv_err("OOM: Kmalloc for pages failed, out of memory. (hostpid=%d; vfid=%u; va=0x%llx; "
                  "page_num=%llu; total_mem=0x%llx; free_mem=0x%llx; shared_mem=0x%llx)\n",
                  svm_proc->process_id.hostpid, svm_proc->process_id.vfid, start, page_num,
                  (uint64_t)mem_info.totalram << KA_MM_PAGE_SHIFT, (uint64_t)mem_info.freeram << KA_MM_PAGE_SHIFT,
                  (uint64_t)mem_info.sharedram << KA_MM_PAGE_SHIFT);
}
#endif

int uvm_host_alloc_and_map_page(struct devmm_svm_process *svm_proc, struct uvm_page_info *page_info, uint64_t start,
                                int numa_id, bool is_read_only)
{
    struct devmm_phy_addr_attr attr = {0};
    ka_page_t **pages = NULL;
    uint64_t page_num = 1;
    int ret;
    u32 prot;

    prot = is_read_only ? DEVMM_PAGE_READONLY_FLG : 0;
    pages = (ka_page_t **)devmm_kvalloc(sizeof(ka_page_t *) * page_num,
                                        KA_GFP_KERNEL | __KA_GFP_ACCOUNT | __KA_GFP_ZERO);
    if (pages == NULL) {
#ifndef EMU_ST
        uvm_log_oom_info(svm_proc, start, page_num);
        return -ENOMEM;
#endif
    }

    devmm_phy_addr_attr_pack(svm_proc, DEVMM_HUGE_PAGE_TYPE, DEVMM_DDR_MEM, false, &attr);
    attr.numa_id = numa_id;
    ret = devmm_master_alloc_huge_pages(&attr, pages, page_num);
    if (ret != 0) {
#ifndef EMU_ST
        uvm_log_oom_info(svm_proc, start, page_num);
        goto free_tmp_pages;
#endif
    }

    ret = devmm_pages_remap(svm_proc, start, page_num, pages, prot);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Insert pages vma error. (ret=%d, va=0x%llx, page_num=%llu)\n", ret, start, page_num);
        goto free_pa;
#endif
    }
    uvm_set_page_mapping(page_info->page_map, pages[0], UVM_HOST_ID);
    goto free_tmp_pages;

free_pa:
    devmm_free_pages(&attr, pages, page_num);
free_tmp_pages:
    devmm_kvfree(pages);
    pages = NULL;

    return ret;
}

static int uvm_page_fault_h2d_sync(struct devmm_svm_process *svm_proc, uint16_t dev_id, ka_page_t **pages, uint64_t va,
                                   uint64_t src_addr, struct uvm_page_info *page_info)
{
    struct devmm_chan_uvm_page_fault *fault_msg = NULL;
    int ret;
    struct devmm_addr_info addr_info = {0};

    fault_msg = (struct devmm_chan_uvm_page_fault *)ka_mm_kvmalloc(sizeof(struct devmm_chan_uvm_page_fault),
                                                                   KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (fault_msg == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Kzalloc for struct devmm_chan_uvm_page_fault failed.\n");
        return -ENOMEM;
#endif
    }

    ret = devmm_dma_map_page(dev_id, pages[0], DEVMM_UVM_PAGE_SIZE, NULL, &addr_info);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Host page fault dma map page failed. (dev_id=%u; va=0x%llx)\n", dev_id, va);
        ret = -EIO;
        goto host_page_fault_msg_free;
#endif
    }

    if (page_bitmap_get_read_mostly(page_info->page_bitmap)) {
        fault_msg->set_flag = PAGE_PTE_SET_READONLY;
    } else {
        fault_msg->set_flag = PAGE_PTE_SET_NONE;
    }

    fault_msg->head.dev_id = dev_id;
    fault_msg->head.msg_id = DEVMM_CHAN_UVM_PAGE_FAULT_H2D_ID;
    fault_msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    fault_msg->head.process_id.vfid = svm_proc->process_id.vfid;
    fault_msg->va = va;
    fault_msg->src_addr = src_addr;
    fault_msg->dst_addr = addr_info.addr;
    ret = devmm_chan_msg_send(fault_msg, sizeof(*fault_msg), 0);
    if (ret != 0) {
        devmm_drv_err("Device copy data process failed. (ret=%d; dev_id=%u; va=0x%llx)\n", ret, dev_id, va);
    }
    devmm_dma_unmap_page(NULL, &addr_info);
host_page_fault_msg_free:
    ka_mm_kfree(fault_msg);
    fault_msg = NULL;

    return ret;
}

int uvm_host_sync_device_data(struct devmm_svm_process *svm_proc, struct uvm_page_info *page_info, uint64_t start,
                              ka_page_t **pages)
{
    uint16_t dev_id;
    uint64_t src_addr;
    int ret;
    struct uvm_page_mapping *page_mapping = page_info->page_map;
    src_addr = get_page_mapping_pa(page_mapping);

    dev_id = devmm_get_phyid_devid_from_svm_process(svm_proc, get_page_mapping_devid(page_mapping));

    ret = uvm_page_fault_h2d_sync(svm_proc, dev_id, pages, start, src_addr, page_info);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Uvm_page_fault_h2d_sync failed. (ret=%d, dev_id=%u)\n", ret, dev_id);
        return ret;
#endif
    }

    return 0;
}

static int uvm_vm_fault_host_proc_device_mapped(struct devmm_svm_process *svm_proc, ka_vm_area_struct_t *vma,
                                                struct uvm_page_info *page_info)
{
    struct devmm_phy_addr_attr attr = {0};
    ka_page_t **pages = NULL;
    uint32_t dev_id;
    int ret;
    struct uvm_page_mapping *page_mapping = page_info->page_map;
    uint64_t start = page_info->va_align;
    u64 page_num = 1;
    u32 page_prot = 0;

    pages = (ka_page_t **)ka_mm_kvmalloc(sizeof(ka_page_t *) * page_num,
                                         KA_GFP_KERNEL | __KA_GFP_ACCOUNT | __KA_GFP_ZERO);
    if (pages == NULL) {
        devmm_drv_err("Kmalloc for pages failed. (va=0x%llx)\n", start);
        return -ENOMEM;
    }

    devmm_phy_addr_attr_pack(svm_proc, DEVMM_HUGE_PAGE_TYPE, DEVMM_DDR_MEM, false, &attr);
    ret = devmm_master_alloc_huge_pages(&attr, pages, page_num);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Uvm_alloc_pages failed. (ka_task_get_current_tgid()=%u, va=0x%llx)\n",
                      svm_proc->process_id.hostpid, start);
        goto free_tmp_pages;
#endif
    }

    ret = uvm_host_sync_device_data(svm_proc, page_info, start, pages);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Sync device data error. (ret=%d, va=0x%llx)\n", ret, start);
        goto free_pa;
#endif
    }

    ret = devmm_pages_remap(svm_proc, start, page_num, pages, page_prot);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Insert pages vma error. (ret=%d, va=0x%llx)\n", ret, start);
        goto free_pa;
#endif
    }

    dev_id = get_page_mapping_devid(page_mapping);
    clear_device_present(page_info->dev_bitmap, dev_id);
    uvm_set_page_mapping(page_mapping, pages[0], UVM_HOST_ID);
    goto free_tmp_pages;

free_pa:
    devmm_free_pages(&attr, pages, page_num);
free_tmp_pages:
    ka_mm_kfree(pages);
    pages = NULL;

    return ret;
}

static int uvm_vm_fault_host_proc(struct devmm_svm_process *svm_proc, ka_vm_area_struct_t *vma,
                                  struct uvm_page_info *page_info, ka_vm_fault_struct_t *vmf)
{
    int ret = 0;
    uint64_t pa_addr;

    if (!page_bitmap_get_alloced(page_info->page_bitmap)) {
        return -EINVAL;
    }

    ka_task_down_write(page_info->page_rwlock);

    ret = devmm_va_to_pa(vma, page_info->va_align, &pa_addr);
    if (ret == 0) {
        goto UVM_PAGE_UNLOCK;
    }
    if (page_is_no_mapped(page_info->page_bitmap)) {
        /* There are no physical pages on either the host or the card. */
        ret = uvm_host_alloc_and_map_page(svm_proc, page_info, page_info->va_align, -1, false);
        if (ret != 0) {
#ifndef EMU_ST
            goto UVM_PAGE_UNLOCK;
#endif
        }
        goto UVM_SET_BITMAP_MAPPED;
    } else if (page_is_mapped_device(page_info->page_bitmap, page_info->page_map)) {
        /* The card has physical pages */
        ret = uvm_vm_fault_host_proc_device_mapped(svm_proc, vma, page_info);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("Uvm_vm_fault_host_proc_device_mapped failed. (ret=%d, va=0x%llx)\n", ret,
                          page_info->va_align);
            goto UVM_PAGE_UNLOCK;
#endif
        }
        page_bitmap_unset_device_mapped(page_info->page_bitmap);
        goto UVM_SET_BITMAP_MAPPED;
    } else if (page_is_mapped_host(page_info->page_bitmap, page_info->page_map)) {
#ifndef EMU_ST
        devmm_drv_err("Unexpected error occurred while page is mapped in host.\n");
        ret = -EINVAL;
        goto UVM_PAGE_UNLOCK;
#endif
    } else {
#ifndef EMU_ST
        devmm_drv_err("Unexpected error occurred while processing uvm_vm_fault_host_proc.\n");
        ret = -EINVAL;
        goto UVM_PAGE_UNLOCK;
#endif
    }

UVM_SET_BITMAP_MAPPED:
    uvm_set_bitmap_mapped(page_info->page_bitmap, UVM_HOST_ID);

UVM_PAGE_UNLOCK:
    ka_task_up_write(page_info->page_rwlock);

    return ret;
}

STATIC int devmm_uvm_vm_fault_host(ka_vm_area_struct_t *vma, ka_vm_fault_struct_t *vmf)
{
    struct uvm_page_info page_info;
    struct devmm_svm_process *svm_proc = NULL;
    struct devmm_uvm_heap *uvm_heap = NULL;
    u64 start = ka_mm_get_vm_start(vma) + (vmf->pgoff << KA_MM_PAGE_SHIFT);
    int ret = 0;

    devmm_drv_debug("Host enter vm fault. (start=0x%llx)\n", start);

    svm_proc = devmm_get_svm_proc_by_mm(vma->vm_mm);
    if (svm_proc == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Can't find process by current pid.\n");
        return DEVMM_FAULT_ERROR;
#endif
    }

    uvm_heap = svm_proc->uvm_heap;
    if (uvm_heap == NULL) {
#ifndef EMU_ST
        devmm_svm_proc_put(svm_proc);
        devmm_drv_err("Uvm heap is not initialized.\n");
        return DEVMM_FAULT_ERROR;
#endif
    }

    ret = uvm_get_page_info_by_va(uvm_heap, start, &page_info);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_svm_proc_put(svm_proc);
        devmm_drv_err("Fail to get page info.\n");
        return DEVMM_FAULT_ERROR;
#endif
    }

    ret = uvm_vm_fault_host_proc(svm_proc, vma, &page_info, vmf);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_svm_proc_put(svm_proc);
        devmm_drv_err("Uvm_vm_fault_host_proc failed. (ret=%d, va=0x%llx)\n", ret, page_info.va_align);
        return DEVMM_FAULT_ERROR;
#endif
    }

    devmm_drv_debug("Host page fault success. (ret=%d, vaddr=0x%llx)\n", ret, page_info.va_align);
    devmm_svm_proc_put(svm_proc);
    return (ret == 0) ? DEVMM_FAULT_OK : DEVMM_FAULT_ERROR;
}

int uvm_reset_ptes_of_hugepage_host(struct devmm_svm_process *svm_proc, ka_vm_area_struct_t *vma, uint64_t addr,
                                    bool is_read_only, bool need_lock)
{
    u64 paddr;
    ka_page_t *page;
    uint64_t pfn;
    int ret = 0;
    ka_pgprot_t prot;

    ret = devmm_va_to_pa(vma, addr, &paddr);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("UVM reset ptes of hugepage failed, can not find pa of va. (vaddr=0x%llx)\n", addr);
        return ret;
#endif
    }
#ifndef EMU_ST
    pfn = paddr >> KA_MM_PAGE_SHIFT;
#else
    pfn = paddr;
#endif
    page = ka_mm_pfn_to_page(pfn);
    if (need_lock) {
        ka_mm_mmap_write_lock(vma->vm_mm);
    }
    devmm_zap_vma_ptes(vma, addr, KA_HPAGE_SIZE);

    prot = is_read_only ? KA_PAGE_READONLY : KA_PAGE_SHARED;
    ret = devmm_insert_pages_to_vma_owner(vma, addr, 1, &page, prot);
    if (ret != 0) {
        devmm_drv_err("Can not insert_pages_vma . (va=0x%llx; ret=%d)\n", addr, ret);
    }
    if (need_lock) {
        ka_mm_mmap_write_unlock(vma->vm_mm);
    }
    return ret;
}

int set_page_pte_readwrite(struct devmm_svm_process *svm_proc, ka_vm_area_struct_t *vma,
                           struct uvm_page_info *page_info, bool need_lock)
{
    int ret = 0;
    u64 start = page_info->va_align;
    ret = KA_MM_UVM_RESET_PTES_OF_HUGEPAGE_HOST(svm_proc, vma, start, false, false);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Remap huge pages error. (ret=%d, va=0x%llx)\n", ret, start);
        return DEVMM_FAULT_ERROR;
#endif
    }
    page_bitmap_unset_read_mostly(page_info->page_bitmap);
    ret = uvm_free_device_page(svm_proc, page_info, 0, false);
    if (ret) {
#ifndef EMU_ST
        devmm_drv_err("Failed to free device va. (va=0x%llx)\n", start);
        return DEVMM_FAULT_ERROR;
#endif
    }

    return ret;
}
STATIC int devmm_uvm_vm_pfn_mkwrite_host(ka_vm_area_struct_t *vma, ka_vm_fault_struct_t *vmf)
{
    struct uvm_page_info page_info;
    struct devmm_svm_process *svm_proc = NULL;
    struct devmm_uvm_heap *uvm_heap = NULL;
    u64 start = ka_mm_get_vm_start(vma) + (vmf->pgoff << KA_MM_PAGE_SHIFT);
    int ret = 0;

    devmm_drv_debug("Host enter pfn mkwrite . (start=0x%llx)\n", start);

    svm_proc = devmm_get_svm_proc_by_mm(vma->vm_mm);
    if (svm_proc == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Can't find process by current pid in pfn mkwrite.\n");
        return DEVMM_FAULT_ERROR;
#endif
    }
    uvm_heap = svm_proc->uvm_heap;
    if (uvm_heap == NULL) {
#ifndef EMU_ST
        devmm_svm_proc_put(svm_proc);
        devmm_drv_err("Uvm heap is not initialized in devmm_uvm_vm_pfn_mkwrite_host.\n");
        return DEVMM_FAULT_ERROR;
#endif
    }
    ret = uvm_get_page_info_by_va(uvm_heap, start, &page_info);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_svm_proc_put(svm_proc);
        devmm_drv_err("Fail to get page info in devmm_uvm_vm_pfn_mkwrite_host.\n");
        return DEVMM_FAULT_ERROR;
#endif
    }
    ret = set_page_pte_readwrite(svm_proc, vma, &page_info, true);
    if (ret) {
        devmm_svm_proc_put(svm_proc);
        return DEVMM_FAULT_ERROR;
    }
    devmm_svm_proc_put(svm_proc);
    return KA_MM_UVM_FAULT_ERROR;
}

#ifndef EMU_ST
static int devmm_mremap(ka_vm_area_struct_t *area)
{
    return -EACCES;
}
#endif

KA_DEFINE_VM_OPS_FAULT_FUNC(devmm_uvm_vm_fault_host)
KA_UVM_DEFINE_VM_OPS_PFN_MKWRITE_FUNC(devmm_uvm_vm_pfn_mkwrite_host)

static ka_vm_operations_struct_t uvm_master_vma_ops = {ka_vm_ops_init_fault(devmm_uvm_vm_fault_host)
                                                           ka_vm_ops_init_pfn_mkwrite(devmm_uvm_vm_pfn_mkwrite_host)
#ifndef EMU_ST
                                                               ka_vm_ops_init_mremap(devmm_mremap)
#endif
};

void devmm_uvm_setup_vma_ops(ka_vm_area_struct_t *vma)
{
    ka_mm_set_vm_ops(vma, &uvm_master_vma_ops);
}
