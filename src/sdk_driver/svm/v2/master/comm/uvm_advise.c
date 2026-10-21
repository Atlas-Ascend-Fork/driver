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

#include "devmm_proc_mem_copy.h"
#include "devmm_common.h"
#include "uvm_master_common.h"
#include "uvm_heap_mng.h"
#include "svm_msg_client.h"
#include "svm_kernel_msg.h"
#include "uvm_advise.h"
#include "svm_phy_addr_blk_mng.h"
#include "ka_base_pub.h"

int uvm_check_location(int type, int id, bool numaEnable)
{
    switch (type) {
        case DRV_UVM_LOCATION_TYPE_HOST:
            break;

        case DRV_UVM_LOCATION_TYPE_DEVICE:
            if (id < 0 || id > UVM_MAX_DEVICE_ID) {
                devmm_drv_err("Invalid device id. (id=%d)\n", id);
                return -EINVAL;
            }
            break;

        case DRV_UVM_LOCATION_TYPE_HOST_NUMA:
#ifndef EMU_ST
            if (!numaEnable) {
                return -EOPNOTSUPP;
            }
            if (id <= 0 || id > ka_mm_num_possible_nodes() || !ka_mm_node_online(id - 1)) {
                devmm_drv_err("Invalid host numa id. (id=%d)\n", id - 1);
                return -EINVAL;
            }
#endif
            break;

        default:
            devmm_drv_err("Unknown advise type. (type=%d)\n", type);
            return -EINVAL;
    }

    return EOK;
}

static int uvm_sync_host_data_to_device(struct devmm_svm_process *svm_proc, struct uvm_page_info *page_info, int devid,
                                        uint64_t *dev_pa, bool readmostly)
{
    struct devmm_chan_uvm_prefetch *prefetch_msg = NULL;
    struct devmm_addr_info addr_info = {0};
    ka_page_t *pg = NULL;
    uint64_t pfn;
    int ret;

    prefetch_msg = (struct devmm_chan_uvm_prefetch *)devmm_kzalloc_ex(sizeof(struct devmm_chan_uvm_prefetch),
                                                                      KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
#ifndef EMU_ST
    if (prefetch_msg == NULL) {
        devmm_drv_err("Kzalloc error. (va=0x%lx)\n", page_info->va_align);
        return -ENOMEM;
    }
#endif
    pfn = KA_MM_PFN_DOWN(page_info->page_map->pa_addr);
    pg = ka_mm_pfn_to_page(pfn);
    ret = devmm_dma_map_page(devmm_get_phyid_devid_from_svm_process(svm_proc, devid), pg, DEVMM_UVM_PAGE_SIZE, NULL,
                             &addr_info);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("devmm_dma_map_page failed. (dev_id=%u; va=0x%llx)\n",
                      devmm_get_phyid_devid_from_svm_process(svm_proc, devid), page_info->va_align);
        goto free_message;
    }
#endif
    prefetch_msg->head.dev_id = devmm_get_phyid_devid_from_svm_process(svm_proc, devid);
    prefetch_msg->head.msg_id = DEVMM_CHAN_UVM_PREFETCH_H2D_ID;
    prefetch_msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    prefetch_msg->head.process_id.vfid = svm_proc->process_id.vfid;
    prefetch_msg->is_read_mostly = readmostly;
    prefetch_msg->va = page_info->va_align;
    prefetch_msg->cnt = 1;
    prefetch_msg->src_addr[0] = addr_info.addr;

    ret = devmm_chan_msg_send(prefetch_msg, sizeof(*prefetch_msg), sizeof(*prefetch_msg));
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Device copy data process failed. (ret=%d; dev_id=%u; va=0x%llx)\n", ret, devid,
                      page_info->va_align);
        goto dma_unmmap;
    }
#endif
    *dev_pa = prefetch_msg->dst_addr[0];

dma_unmmap:
    devmm_dma_unmap_page(NULL, &addr_info);
free_message:
    devmm_kfree_ex(prefetch_msg);

    return ret;
}

static int uvm_prefer_host_default(struct devmm_svm_process *svm_proc, struct uvm_page_info *page_info,
                                   int preferred_numa)
{
    int ret;
    u16 preferred_dev_id = page_bitmap_get_preferred_loc(page_info->page_bitmap);

    if (page_bitmap_get_host_mapped(page_info->page_bitmap)) {
        return 0;
    }
#ifndef EMU_ST
    if (page_bitmap_get_device_mapped(page_info->page_bitmap)) {
        if (page_info->page_map->devid != preferred_dev_id) {
            devmm_drv_warn("Cannot prealloc page for host, page already mapped on device "
                           "(non-read-mostly). (va=0x%llx)\n",
                           page_info->va_align);
        }
        return 0;
    }
#endif

    ret = uvm_host_alloc_and_map_page(svm_proc, page_info, page_info->va_align, preferred_numa, false);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Uvm_prealloc_page_host failed. (ret=%d, va=0x%llx)\n", ret, page_info->va_align);
        return ret;
    }
#endif
    page_bitmap_set_host_mapped(page_info->page_bitmap);
    return ret;
}

int uvm_prealloc_one_page_device(struct devmm_svm_process *svm_proc, struct uvm_page_info *page_info, u16 dev_id,
                                 u64 *out_dev_pa)
{
    struct devmm_chan_alloc_map_req msg = {0};
    int ret;

    msg.head.msg_id = DEVMM_CHAN_UVM_PREALLOC_PAGE_DEVICE_ID;
    msg.head.process_id.vfid = svm_proc->process_id.vfid;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.dev_id = devmm_get_phyid_devid_from_svm_process(svm_proc, dev_id);
    msg.va = page_info->va_align;
    ret = devmm_chan_msg_send(&msg, sizeof(msg), sizeof(msg));
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Uvm_prealloc_one_page_device failed. (ret=%d; dev_id=%u; va=0x%llx)\n", ret, dev_id,
                      page_info->va_align);
        return ret;
    }
#endif
    if (out_dev_pa) {
        *out_dev_pa = msg.dev_pa;
    }

    return ret;
}

static int uvm_prefer_device_readmostly(struct devmm_svm_process *svm_proc, struct uvm_page_info *page_info)
{
    int ret;
    u16 preferred_dev_id;
    u64 dev_pa = 0;

    preferred_dev_id = page_bitmap_get_preferred_loc(page_info->page_bitmap);
#ifndef EMU_ST
    if (is_device_present(page_info->dev_bitmap, preferred_dev_id)) {
        return 0;
    }
#endif
    ret = uvm_sync_host_data_to_device(svm_proc, page_info, preferred_dev_id, &dev_pa, true);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Uvm_sync_host_data_to_device failed. (ret=%d, va=0x%llx)\n", ret, page_info->va_align);
        return ret;
    }
#endif
    set_device_present(page_info->dev_bitmap, preferred_dev_id);
    page_bitmap_set_device_mapped(page_info->page_bitmap);
    return ret;
}

static int uvm_prefer_device_default(struct devmm_svm_process *svm_proc, struct uvm_page_info *page_info)
{
    int ret;
    u16 preferred_dev_id;
    u64 dev_pa = 0;
#ifndef EMU_ST
    if (page_bitmap_get_host_mapped(page_info->page_bitmap)) {
        devmm_drv_warn("Cannot prealloc page for device, page already mapped on host (non-read-mostly). (va=0x%llx)\n",
                       page_info->va_align);
        return 0;
    }
    if (page_bitmap_get_device_mapped(page_info->page_bitmap)) {
        return 0;
    }
#endif
    preferred_dev_id = page_bitmap_get_preferred_loc(page_info->page_bitmap);

    ret = uvm_prealloc_one_page_device(svm_proc, page_info, preferred_dev_id, &dev_pa);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Uvm_prealloc_page_device failed. (ret=%d, va=0x%llx)\n", ret, page_info->va_align);
        return ret;
    }
#endif
    set_page_mapping(page_info->page_map, dev_pa, preferred_dev_id);
    set_device_present(page_info->dev_bitmap, preferred_dev_id);
    page_bitmap_set_device_mapped(page_info->page_bitmap);
    return ret;
}

static int uvm_apply_page_policy(void *svm_proc, struct uvm_page_info *page_info, uint32_t target_loc,
                                 int preferred_numa)
{
    int ret;
    bool is_readmostly = page_bitmap_get_read_mostly(page_info->page_bitmap);
    if (target_loc == UVM_HOST_ID) {
        if (is_readmostly) {
            ret = 0;
        } else {
            ret = uvm_prefer_host_default(svm_proc, page_info, preferred_numa);
        }
    } else {
        if (is_readmostly) {
            ret = uvm_prefer_device_readmostly(svm_proc, page_info);
        } else {
            ret = uvm_prefer_device_default(svm_proc, page_info);
        }
    }
    return ret;
}

static int uvm_set_preferred_loc_one_page(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                          u64 set_addr, u16 preferred_loc, int preferred_loc_type)
{
    int ret = 0;
    struct uvm_page_info page_info;
    bool should_prealloc;
    int preferred_numa = INVALID_NUMA;
    u16 target_loc;
    ret = uvm_get_page_info_by_va(uvm_heap, set_addr, &page_info);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Fail to get page info in uvm_set_preferred_loc_one_page.\n");
        return -EINVAL;
    }

    if (!page_bitmap_get_alloced(page_info.page_bitmap)) {
        devmm_drv_err("Va is not alloced in uvm_set_preferred_loc_one_page, va = 0x%llx.\n", set_addr);
        return -EINVAL;
    }
#endif
    ka_task_down_write(page_info.page_rwlock);
    if (page_bitmap_get_preferred_loc_type(page_info.page_bitmap) == preferred_loc_type &&
        page_bitmap_get_preferred_loc(page_info.page_bitmap) == preferred_loc) {
        ka_task_up_write(page_info.page_rwlock);
        return 0;
    }
    page_bitmap_set_preferred_loc_type(page_info.page_bitmap, preferred_loc_type);
    page_bitmap_set_preferred_loc(page_info.page_bitmap, preferred_loc);

    target_loc = preferred_loc;
    if (preferred_loc_type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) {
        preferred_numa = preferred_loc;
        target_loc = UVM_HOST_ID;
    }

    should_prealloc = (target_loc != UVM_INVALID_DEVICE_ID) &&
                      is_device_present(page_info.access_by_bitmap, target_loc);
    if (!should_prealloc) {
        ka_task_up_write(page_info.page_rwlock);
        return 0;
    }

    ret = uvm_apply_page_policy(svm_proc, &page_info, target_loc, preferred_numa);

    ka_task_up_write(page_info.page_rwlock);

    return ret;
}

static int uvm_set_preferred_loc_process(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                         struct devmm_mem_managed_advise_para *arg)
{
    int ret = 0;
    uint64_t set_addr;
    int preferred_loc_type = arg->type;
    int preferred_loc = arg->id;

    ret = uvm_check_location(arg->type, arg->id, true);
    if (ret != 0) {
        return ret;
    }

    if (preferred_loc_type == DRV_UVM_LOCATION_TYPE_HOST) {
        preferred_loc = UVM_HOST_ID;
    }

    for (set_addr = arg->ptr; set_addr < arg->ptr + arg->size; set_addr += DEVMM_UVM_PAGE_SIZE) {
        ret = uvm_set_preferred_loc_one_page(svm_proc, uvm_heap, set_addr, (u16)preferred_loc, preferred_loc_type);
#ifndef EMU_ST
        if (ret != 0) {
            devmm_drv_err("Uvm set task preferred_loc one page failed, addr = 0x%llx\n", set_addr);
            break;
        }
#endif
    }

    return ret;
}

static int uvm_set_access_by_one_page(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap, u64 set_addr,
                                      u16 access_by)
{
    int ret = 0;
    struct uvm_page_info page_info;
    bool should_prealloc;
    u16 preferred_loc, preferred_loc_type;
    int preferred_numa = INVALID_NUMA;
    ret = uvm_get_page_info_by_va(uvm_heap, set_addr, &page_info);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Fail to get page info.\n");
        return -EINVAL;
    }

    if (!page_bitmap_get_alloced(page_info.page_bitmap)) {
        devmm_drv_err("Va is not alloced, va = 0x%llx.\n", set_addr);
        return -EINVAL;
    }
#endif
    ka_task_down_write(page_info.page_rwlock);
    if (is_device_present(page_info.access_by_bitmap, access_by)) {
        ka_task_up_write(page_info.page_rwlock);
        return 0;
    }
    set_device_present(page_info.access_by_bitmap, access_by);

    preferred_loc_type = page_bitmap_get_preferred_loc_type(page_info.page_bitmap);
    preferred_loc = page_bitmap_get_preferred_loc(page_info.page_bitmap);
    if (preferred_loc_type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) {
        preferred_numa = preferred_loc;
        preferred_loc = UVM_HOST_ID;
    } else if (preferred_loc_type == DRV_UVM_LOCATION_TYPE_INVALID) {
        ka_task_up_write(page_info.page_rwlock);
        return 0;
    }
    should_prealloc = ((access_by != UVM_INVALID_DEVICE_ID) && (access_by == preferred_loc));
    if (!should_prealloc) {
        ka_task_up_write(page_info.page_rwlock);
        return 0;
    }

    ret = uvm_apply_page_policy(svm_proc, &page_info, access_by, preferred_numa);

    ka_task_up_write(page_info.page_rwlock);

    return ret;
}

static int uvm_set_access_by_process(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                     struct devmm_mem_managed_advise_para *arg)
{
    int ret = 0;
    uint64_t set_addr;

    ret = uvm_check_location(arg->type, arg->id, false);
    if (ret != 0) {
        return ret;
    }

    if (arg->type == DRV_UVM_LOCATION_TYPE_HOST) {
        arg->id = UVM_HOST_ID;
    }

    for (set_addr = arg->ptr; set_addr < arg->ptr + arg->size; set_addr += DEVMM_UVM_PAGE_SIZE) {
        ret = uvm_set_access_by_one_page(svm_proc, uvm_heap, set_addr, (u16)arg->id);
#ifndef EMU_ST
        if (ret != 0) {
            devmm_drv_err("Uvm set task access_by one page failed, addr = 0x%llx, arg->id = %d\n", set_addr, arg->id);
            break;
        }
#endif
    }
    return ret;
}

static int uvm_unset_preferred_loc_one_page(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                            u64 set_addr)
{
    int ret = 0;
    struct uvm_page_info page_info;
    ret = uvm_get_page_info_by_va(uvm_heap, set_addr, &page_info);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Fail to get page info.\n");
        return -EINVAL;
    }

    if (!page_bitmap_get_alloced(page_info.page_bitmap)) {
        devmm_drv_err("Va is not alloced, va = 0x%llx.\n", set_addr);
        return -EINVAL;
    }
#endif
    ka_task_down_write(page_info.page_rwlock);
    page_bitmap_unset_preferred_loc_type(page_info.page_bitmap);
    page_bitmap_unset_preferred_loc(page_info.page_bitmap);
    ka_task_up_write(page_info.page_rwlock);

    return ret;
}

static int uvm_unset_preferred_loc_process(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                           struct devmm_mem_managed_advise_para *arg)
{
    int ret = 0;
    uint64_t set_addr;
    for (set_addr = arg->ptr; set_addr < arg->ptr + arg->size; set_addr += DEVMM_UVM_PAGE_SIZE) {
        ret = uvm_unset_preferred_loc_one_page(svm_proc, uvm_heap, set_addr);
#ifndef EMU_ST
        if (ret != 0) {
            devmm_drv_err("Uvm unset task preferred_loc one page failed, addr = 0x%llx\n", set_addr);
            break;
        }
#endif
    }
    return ret;
}

static int uvm_unset_access_by_one_page(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                        u64 set_addr, u16 access_by)
{
    int ret = 0;
    struct uvm_page_info page_info;
    ret = uvm_get_page_info_by_va(uvm_heap, set_addr, &page_info);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Fail to get page info.\n");
        return -EINVAL;
    }

    if (!page_bitmap_get_alloced(page_info.page_bitmap)) {
        devmm_drv_err("Va is not alloced, va = 0x%llx.\n", set_addr);
        return -EINVAL;
    }
#endif
    ka_task_down_write(page_info.page_rwlock);
    clear_device_present(page_info.access_by_bitmap, access_by);
    ka_task_up_write(page_info.page_rwlock);
    return ret;
}

static int uvm_unset_access_by_process(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                       struct devmm_mem_managed_advise_para *arg)
{
    int ret = 0;
    uint64_t set_addr;
    int access_by = arg->id;

    ret = uvm_check_location(arg->type, arg->id, false);
    if (ret != 0) {
        return ret;
    }

    for (set_addr = arg->ptr; set_addr < arg->ptr + arg->size; set_addr += DEVMM_UVM_PAGE_SIZE) {
        ret = uvm_unset_access_by_one_page(svm_proc, uvm_heap, set_addr, (u16)access_by);
#ifndef EMU_ST
        if (ret != 0) {
            devmm_drv_err("Uvm set task access_by one page failed, addr = 0x%llx\n", set_addr);
            break;
        }
#endif
    }
    return ret;
}

static int uvm_alloc_phy_mem_for_va_host_and_sync(struct devmm_svm_process *svm_proc, ka_vm_area_struct_t *vma,
                                                  struct uvm_page_info *page_info, uint64_t va, bool is_read_only)
{
    int ret = 0;
    struct devmm_phy_addr_attr attr = {0};
    ka_page_t **pages = NULL;
    uint64_t page_num = 1;
    u32 prot;

    prot = is_read_only ? DEVMM_PAGE_READONLY_FLG : 0;
    pages = (ka_page_t **)devmm_kvalloc(sizeof(ka_page_t *) * page_num,
                                        KA_GFP_KERNEL | __KA_GFP_ACCOUNT | __KA_GFP_ZERO);
#ifndef EMU_ST
    if (pages == NULL) {
        devmm_drv_err("Kmalloc for pages failed. (va=0x%llx, page_num=%llu)\n", va, page_num);
        return -ENOMEM;
    }
#endif
    devmm_phy_addr_attr_pack(svm_proc, DEVMM_HUGE_PAGE_TYPE, DEVMM_DDR_MEM, false, &attr);
    ret = devmm_master_alloc_huge_pages(&attr, pages, page_num);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Can not alloc pages. (ret=%d; pg_num=%llu)\n", ret, page_num);
        goto free_tmp_pages;
    }
#endif
    ret = uvm_host_sync_device_data(svm_proc, page_info, va, pages);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Sync device data error. (ret=%d, set_addr=0x%llx, page_num=%llu\n)", ret, va, page_num);
        goto free_pa;
    }
#endif
    ret = devmm_pages_remap(svm_proc, va, 1, pages, prot);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Can not insert_pages_vma . (va=0x%llx; ret=%d)\n", va, ret);
        goto free_pa;
    }
#endif
    uvm_set_page_mapping(page_info->page_map, pages[0], UVM_HOST_ID);
    uvm_set_bitmap_mapped(page_info->page_bitmap, UVM_HOST_ID);
    goto free_tmp_pages;

free_pa:
    devmm_free_pages(&attr, pages, page_num);
free_tmp_pages:
    devmm_kvfree(pages);
    pages = NULL;

    return ret;
}

int uvm_reset_ptes_of_hugepage(struct devmm_svm_process *svm_proc, ka_vm_area_struct_t *vma, uint64_t addr,
                               bool is_read_only)
{
    ka_page_t *page;
    uint64_t pa_addr;
    uint64_t pfn;
    int ret = 0;
    u32 prot;

    prot = is_read_only ? DEVMM_PAGE_READONLY_FLG : 0;

    ret = devmm_va_to_pa(vma, addr, &pa_addr);
    if (ret != 0) {
        devmm_drv_err("UVM reset ptes of hugepage failed, can not find pa of va. (vaddr=0x%llx)\n", addr);
        return ret;
    }
#ifndef EMU_ST
    pfn = pa_addr >> KA_MM_PAGE_SHIFT;
#else
    pfn = pa_addr;
#endif
    page = ka_mm_pfn_to_page(pfn);
    devmm_zap_vma_ptes(vma, addr, DEVMM_UVM_PAGE_SIZE);
    ret = devmm_pages_remap(svm_proc, addr, 1, &page, prot);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_info("Can not insert_pages_vma . (va=0x%llx; ret=%d)\n", addr, ret);
    }
#endif
    return ret;
}

static int uvm_set_page_readonly_process(struct devmm_svm_process *svm_proc, struct uvm_page_info *page_info)
{
    ka_vm_area_struct_t *vma = NULL;
    int ret = 0;

    vma = devmm_find_vma(svm_proc, page_info->va_align);
#ifndef EMU_ST
    if (vma == NULL) {
        devmm_drv_err("Can not find vma. (vaddr=0x%llx; hostpid=%d; devid=%d; vfid=%d)\n", page_info->va_align,
                      svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid);
        return -EINVAL;
    }
#endif
    ka_mm_mmap_write_lock(vma->vm_mm);
    if (page_is_mapped_host(page_info->page_bitmap, page_info->page_map)) {
        ret = uvm_reset_ptes_of_hugepage(svm_proc, vma, page_info->va_align, true);
#ifndef EMU_ST
        if (ret != 0) {
            devmm_drv_err("UVM uvm_reset_ptes_of_hugepage failed. (vaddr=0x%llx)\n", page_info->va_align);
        }
#endif
    } else if (page_is_no_mapped(page_info->page_bitmap)) {
        ret = uvm_host_alloc_and_map_page(svm_proc, page_info, page_info->va_align, -1, true);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("Uvm_set_readonly_one_page for no mapped failed. (ret=%d, va=0x%llx)\n", ret,
                          page_info->va_align);
#endif
        } else {
            uvm_set_bitmap_mapped(page_info->page_bitmap, UVM_HOST_ID);
        }
    } else if (page_is_mapped_device(page_info->page_bitmap, page_info->page_map)) {
        ret = uvm_alloc_phy_mem_for_va_host_and_sync(svm_proc, vma, page_info, page_info->va_align, true);
#ifndef EMU_ST
        if (ret != 0) {
            devmm_drv_err("Sync device data error. (ret=%d, va=0x%llx)\n", ret, page_info->va_align);
        }
#endif
    } else {
#ifndef EMU_ST
        ret = -EINVAL;
        devmm_drv_err("Unexpected error occurred while processing uvm_set_readonly_one_page.");
#endif
    }
    ka_mm_mmap_write_unlock(vma->vm_mm);
    return ret;
}

static int uvm_set_readonly_one_page(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                     uint64_t set_addr)
{
    int ret = 0;
    struct uvm_page_info page_info;
    ret = uvm_get_page_info_by_va(uvm_heap, set_addr, &page_info);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Fail to get page info.\n");
        return -EINVAL;
    }

    if (!page_bitmap_get_alloced(page_info.page_bitmap)) {
        devmm_drv_err("Va is not alloced, va = 0x%llx.\n", set_addr);
        return -EINVAL;
    }
#endif
    ka_task_down_write(page_info.page_rwlock);
    if (page_bitmap_get_read_mostly(page_info.page_bitmap)) {
        ka_task_up_write(page_info.page_rwlock);
        return 0;
    }

    page_bitmap_set_read_mostly(page_info.page_bitmap);

    ret = uvm_set_page_readonly_process(svm_proc, &page_info);
#ifndef EMU_ST
    if (ret) {
        devmm_drv_err("Uvm_set_page_readonly_process error, addr = 0x%llx\n", set_addr);
        page_bitmap_unset_read_mostly(page_info.page_bitmap);
    }
#endif
    ka_task_up_write(page_info.page_rwlock);
    return ret;
}

static int uvm_set_readmostly_process(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                      struct devmm_mem_managed_advise_para *arg)
{
    int ret = 0;
    uint64_t set_addr;

    for (set_addr = arg->ptr; set_addr < arg->ptr + arg->size; set_addr += DEVMM_UVM_PAGE_SIZE) {
        ret = uvm_set_readonly_one_page(svm_proc, uvm_heap, set_addr);
#ifndef EMU_ST
        if (ret) {
            devmm_drv_err("Uvm set task readonly one page error, addr = 0x%llx\n", set_addr);
            break;
        }
#endif
    }

    return ret;
}

static int uvm_free_other_device_page_process(struct devmm_svm_process *svm_proc, struct uvm_page_info *page_info,
                                              int id)
{
    int ret = 0;
    int16_t i, count;
    int *device_ids;

    device_ids = devmm_kzalloc_ex(sizeof(int) * BITMAP_BITS_COUNT, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
#ifndef EMU_ST
    if (!device_ids) {
        devmm_drv_err("Failed to alloc mem for device ids\n");
        return -ENOMEM;
    }
#endif

    count = get_present_devices(page_info->dev_bitmap, device_ids, BITMAP_BITS_COUNT);
    for (i = 0; i < count; i++) {
#ifndef EMU_ST
        if (device_ids[i] != id) {
            ret = uvm_free_device_page_by_id(svm_proc, page_info->dev_bitmap, device_ids[i], page_info->va_align);
            if (ret) {
                devmm_drv_err("Device failed to free va. (va=0x%llx, device_id = %d)\n", page_info->va_align,
                              device_ids[i]);
                goto free_device_ids;
            }
        }
#endif
    }

free_device_ids:
    devmm_kfree_ex(device_ids);

    return ret;
}

static int uvm_unset_page_readonly_host_process(struct devmm_svm_process *svm_proc, ka_vm_area_struct_t *vma,
                                                struct uvm_page_info *page_info)
{
    int ret = 0;
    ret = uvm_free_device_page(svm_proc, page_info, 0, false);
#ifndef EMU_ST
    if (ret) {
        devmm_drv_err("Failed to free device va. (va=0x%llx)\n", page_info->va_align);
        return ret;
    }
#endif

    ret = uvm_reset_ptes_of_hugepage(svm_proc, vma, page_info->va_align, false);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("UVM uvm_reset_ptes_of_hugepage failed. (vaddr=0x%llx)\n", page_info->va_align);
    }
#endif
    return ret;
}

static int uvm_unset_page_readonly_device_process(struct devmm_svm_process *svm_proc, ka_vm_area_struct_t *vma,
                                                  struct uvm_page_info *page_info, int id)
{
    int ret = 0;
    uint64_t pa;
    struct devmm_chan_set_page set_page_info;

    ret = uvm_free_other_device_page_process(svm_proc, page_info, id);
    if (ret) {
        return ret;
    }

    if (page_bitmap_get_device_mapped(page_info->page_bitmap) && is_device_present(page_info->dev_bitmap, id)) {
        set_page_info.head.dev_id = devmm_get_phyid_devid_from_svm_process(svm_proc, id);
        set_page_info.head.msg_id = DEVMM_CHAN_UVM_SET_PAGE_H2D_ID;
        set_page_info.head.process_id.hostpid = svm_proc->process_id.hostpid;
        set_page_info.head.process_id.vfid = svm_proc->process_id.vfid;
        set_page_info.va = page_info->va_align;
        set_page_info.set_flag = PAGE_PTE_SET_READWRITE;
        ret = devmm_chan_msg_send(&set_page_info, sizeof(set_page_info), sizeof(set_page_info));
#ifndef EMU_ST
        if (ret) {
            devmm_drv_err("Failed change pte of page, (va=0x%llx)\n", page_info->va_align);
            return ret;
        }
#endif
        pa = set_page_info.pa;
    } else {
        ret = uvm_sync_host_data_to_device(svm_proc, page_info, id, &pa, false);
#ifndef EMU_ST
        if (ret != 0) {
            devmm_drv_err("Uvm_sync_host_data_to_device failed. (dev_id = %d, va=0x%llx)\n", id, page_info->va_align);
            return ret;
        }
#endif
    }

    ret = uvm_free_host_page(svm_proc, page_info, 0);
#ifndef EMU_ST
    if (ret) {
        devmm_drv_err("Host failed to free va. (va=0x%llx)\n", page_info->va_align);
        return ret;
    }
#endif
    set_device_present(page_info->dev_bitmap, id);
    set_page_mapping(page_info->page_map, pa, id);
    page_bitmap_set_device_mapped(page_info->page_bitmap);

    return ret;
}

static int uvm_unset_page_readonly_process(struct devmm_svm_process *svm_proc, struct uvm_page_info *page_info,
                                           int type, int id)
{
    int ret = 0;
    ka_vm_area_struct_t *vma = NULL;
    u16 preferred_loc_type;
    int actual_id = id;
    int target_type = type;

    vma = devmm_find_vma(svm_proc, page_info->va_align);
#ifndef EMU_ST
    if (vma == NULL) {
        devmm_drv_err("Can not find vma. (vaddr=0x%llx; hostpid=%d; devid=%d; vfid=%d)\n", page_info->va_align,
                      svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid);
        ret = -EINVAL;
        return ret;
    }
#endif
    preferred_loc_type = page_bitmap_get_preferred_loc_type(page_info->page_bitmap);
    if (preferred_loc_type == DRV_UVM_LOCATION_TYPE_HOST || preferred_loc_type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) {
        target_type = DRV_UVM_LOCATION_TYPE_HOST;
    } else if (preferred_loc_type == DRV_UVM_LOCATION_TYPE_DEVICE) {
        target_type = DRV_UVM_LOCATION_TYPE_DEVICE;
        actual_id = page_bitmap_get_preferred_loc(page_info->page_bitmap);
    }

    if (target_type == DRV_UVM_LOCATION_TYPE_INVALID || target_type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) {
        target_type = DRV_UVM_LOCATION_TYPE_HOST;
    }

    if (target_type == DRV_UVM_LOCATION_TYPE_HOST) {
        ret = uvm_unset_page_readonly_host_process(svm_proc, vma, page_info);
    } else {
        ret = uvm_unset_page_readonly_device_process(svm_proc, vma, page_info, actual_id);
    }

    if (ret == 0) {
        page_bitmap_unset_read_mostly(page_info->page_bitmap);
    }

    return ret;
}

static int uvm_unset_readonly_one_pte(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                      uint64_t set_addr, int type, int id)
{
    int ret = 0;
    struct uvm_page_info page_info;
    ret = uvm_get_page_info_by_va(uvm_heap, set_addr, &page_info);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Fail to get page info.\n");
        return -EINVAL;
    }

    if (!page_bitmap_get_alloced(page_info.page_bitmap)) {
        devmm_drv_err("Va is not alloced, va = 0x%llx.\n", set_addr);
        return -EINVAL;
    }
#endif
    ka_task_down_write(page_info.page_rwlock);
#ifndef EMU_ST
    if (!page_bitmap_get_read_mostly(page_info.page_bitmap)) {
        ka_task_up_write(page_info.page_rwlock);
        return 0;
    }
#endif
    ret = uvm_unset_page_readonly_process(svm_proc, &page_info, type, id);
#ifndef EMU_ST
    if (ret) {
        devmm_drv_err("Uvm_unset_page_readonly_process error, addr = 0x%llx\n", set_addr);
    }
#endif
    ka_task_up_write(page_info.page_rwlock);
    return ret;
}

static int uvm_unset_readmostly_process(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                        struct devmm_mem_managed_advise_para *arg)
{
    int ret = 0;
    uint64_t set_addr;

    if (arg->type == DRV_UVM_LOCATION_TYPE_HOST_NUMA || arg->type == DRV_UVM_LOCATION_TYPE_DEVICE) {
        ret = uvm_check_location(arg->type, arg->id, true);
#ifndef EMU_ST
        if (ret != 0) {
            return ret;
        }
#endif
    }

    for (set_addr = arg->ptr; set_addr < arg->ptr + arg->size; set_addr += DEVMM_UVM_PAGE_SIZE) {
        ret = uvm_unset_readonly_one_pte(svm_proc, uvm_heap, set_addr, arg->type, arg->id);
        if (ret) {
            break;
        }
    }
    return ret;
}

typedef int (*uvm_advise_func)(struct devmm_svm_process *, struct devmm_uvm_heap *,
                               struct devmm_mem_managed_advise_para *);

struct uvm_advise_entry {
    int advise_type;
    uvm_advise_func func;
    const char *error_msg;
};

static const struct uvm_advise_entry uvm_advise_table[] = {
    {READ_MOSTLY, uvm_set_readmostly_process, "Uvm set readmostly error"},
    {UNSET_READ_MOSTLY, uvm_unset_readmostly_process, "Uvm unset readmostly error"},
    {PREFER_LOCATION, uvm_set_preferred_loc_process, "Uvm set preferred_loc error"},
    {UNSET_PREFER_LOCATION, uvm_unset_preferred_loc_process, "Uvm unset preferred_loc error"},
    {ACCESS_BY_LOCATION, uvm_set_access_by_process, "Uvm set access_by error"},
    {UNSET_ACCESS_BY_LOCATION, uvm_unset_access_by_process, "Uvm unset access_by error"},
};

static int uvm_advise(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                      struct devmm_mem_managed_advise_para *arg)
{
    size_t i;
    int ret = -EINVAL;

    for (i = 0; i < KA_BASE_ARRAY_SIZE(uvm_advise_table); i++) {
        if (uvm_advise_table[i].advise_type == arg->advise) {
            ret = uvm_advise_table[i].func(svm_proc, uvm_heap, arg);
#ifndef EMU_ST
            if (ret != 0) {
                devmm_drv_err("%s, addr = 0x%llx\n", uvm_advise_table[i].error_msg, arg->ptr);
            }
#endif
            return ret;
        }
    }

    devmm_drv_err("Uvm advise type error, addr = 0x%llx\n", arg->ptr);
    return ret;
}

int devmm_uvm_ioctl_advise(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_managed_advise_para *advise_para = &arg->data.mem_managed_advise_para;
    struct devmm_uvm_heap *uvm_heap = NULL;

    uvm_heap = devmm_uvm_get_heap(svm_proc, advise_para->ptr);
#ifndef EMU_ST
    if (uvm_heap == NULL) {
        devmm_drv_err("UVM Heap is NULL or error. (heap_is_null=%d; ptr=0x%llx)\n", (uvm_heap == NULL),
                      advise_para->ptr);
        return -EADDRNOTAVAIL;
    }
#endif

    advise_para->ptr = ka_base_round_down(advise_para->ptr, uvm_heap->chunk_page_size);
    advise_para->size = KA_DRIVER_ALIGN(advise_para->size, uvm_heap->chunk_page_size);
#ifndef EMU_ST
    if (!devmm_vaddr_and_size_is_in_uvm_range(advise_para->ptr, advise_para->size)) {
        devmm_drv_err("Advise addr is not in uvm range. (ptr=0x%llx)\n", advise_para->ptr);
        return -EFAULT;
    }
#endif
    if (advise_para->type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) {
        advise_para->id += 1;
    }
    return uvm_advise(svm_proc, uvm_heap, advise_para);
}

static int devmm_get_read_mostly_attr(struct uvm_page_info page_info, size_t num_pages, int32_t *mem_attr,
                                      size_t data_size)
{
    size_t pade_idx;
    int start_idx = 0;
    page_bitmap_t *page_bitmap = page_info.page_bitmap;
    u32 stamp = (u32)ka_jiffies;

    mem_attr[start_idx] = 1;
    for (pade_idx = 0; pade_idx < num_pages; pade_idx++) {
        if (!page_bitmap_get_read_mostly(&page_bitmap[pade_idx])) {
            mem_attr[start_idx] = 0;
            break;
        }
        devmm_try_cond_resched(&stamp);
    }
    return 0;
}

static int devmm_get_preferred_loc_type_attr(struct uvm_page_info page_info, size_t num_pages, int32_t *mem_attr,
                                             size_t data_size)
{
    int32_t location_dev;
    page_bitmap_t *page_bitmap = page_info.page_bitmap;
    size_t page_idx;
    u32 stamp = (u32)ka_jiffies;

    location_dev = page_bitmap_get_preferred_loc_type(&page_bitmap[0]);
    for (page_idx = 0; page_idx < num_pages; page_idx++) {
        if (location_dev != page_bitmap_get_preferred_loc_type(&page_bitmap[page_idx])) {
            location_dev = DRV_UVM_LOCATION_TYPE_INVALID;
            break;
        }
        devmm_try_cond_resched(&stamp);
    }
    *mem_attr = location_dev;
    return 0;
}

static int devmm_get_prefer_loc_attr_detail(struct uvm_page_info page_info, size_t num_pages, size_t data_size,
                                            int32_t *location_dev, int32_t *location_type)
{
    int32_t loc_dev, loc_type;
    size_t page_idx = 0;
    page_bitmap_t *page_bitmap = page_info.page_bitmap;
    u32 stamp = (u32)ka_jiffies;

    (void)devmm_get_preferred_loc_type_attr(page_info, num_pages, &loc_type, data_size);
    loc_dev = page_bitmap_get_preferred_loc(&page_bitmap[page_idx]);
    for (; page_idx < num_pages; page_idx++) {
        if (loc_dev != page_bitmap_get_preferred_loc(&page_bitmap[page_idx])) {
            loc_dev = UVM_INVALID_DEVICE_ID;
            break;
        }
        devmm_try_cond_resched(&stamp);
    }
    *location_dev = loc_dev;
    *location_type = loc_type;
    return 0;
}

static int devmm_get_prefer_loc_attr(struct uvm_page_info page_info, size_t num_pages, int32_t *mem_attr,
                                     size_t data_size)
{
    bool loc_type_same = true, loc_type_host = true;
    int32_t location_dev, location_dev_tmp, location_type;
    page_bitmap_t *page_bitmap = page_info.page_bitmap;
    size_t page_idx;
    u32 stamp = (u32)ka_jiffies;

    location_dev = page_bitmap_get_preferred_loc_type(&page_bitmap[0]);
    for (page_idx = 0; page_idx < num_pages; page_idx++) {
        location_dev_tmp = page_bitmap_get_preferred_loc_type(&page_bitmap[page_idx]);
        if (loc_type_same && (location_dev != location_dev_tmp)) {
            loc_type_same = false;
        }
        if ((location_dev_tmp != DRV_UVM_LOCATION_TYPE_HOST_NUMA) && (location_dev_tmp != DRV_UVM_LOCATION_TYPE_HOST)) {
            loc_type_host = false;
        }
        devmm_try_cond_resched(&stamp);
    }

    if (loc_type_host) {
        location_dev = MEM_RANGE_CPU_DEVICE_ID;
    } else if (!loc_type_same) {
        location_dev = MEM_RANGE_INVALID_DEVICE_ID;
    } else {
        (void)devmm_get_prefer_loc_attr_detail(page_info, num_pages, data_size, &location_dev, &location_type);
        if (location_type == DRV_UVM_LOCATION_TYPE_INVALID) {
            location_dev = MEM_RANGE_INVALID_DEVICE_ID;
        }
    }

    *mem_attr = location_dev;

    return 0;
}

static int devmm_set_access_by_loc_attr(struct uvm_page_info page_info, size_t num_pages, int32_t *mem_attr,
                                        size_t data_size, bool *dev_access_by, bool *dev_cur_access_by,
                                        int32_t *device_per_page)
{
    int32_t dev_max_num = UVM_INVALID_DEVICE_ID + 1;
    device_bitmap *access_by_bitmap = page_info.access_by_bitmap;
    int32_t dev_num_per_page, page_idx, dev_idx, dev_idx_access_by;
    u32 stamp = (u32)ka_jiffies;

    for (dev_idx = 0; dev_idx < dev_max_num; dev_idx++) {
        dev_access_by[dev_idx] = true;
        dev_cur_access_by[dev_idx] = false;
    }

    for (page_idx = 0; page_idx < num_pages; page_idx++) {
        dev_num_per_page = get_present_devices(&access_by_bitmap[page_idx], device_per_page, BITMAP_BITS_COUNT);
        for (dev_idx = 0; dev_idx < dev_num_per_page; dev_idx++) {
            dev_cur_access_by[device_per_page[dev_idx]] = true;
        }
        for (dev_idx = 0; dev_idx < dev_max_num; dev_idx++) {
            dev_access_by[dev_idx] &= dev_cur_access_by[dev_idx];
            dev_cur_access_by[dev_idx] = false;
        }
        devmm_try_cond_resched(&stamp);
    }
    for (dev_idx = 0, dev_idx_access_by = 0; (dev_idx < dev_max_num) && (dev_idx_access_by < data_size); dev_idx++) {
        if (dev_access_by[dev_idx]) {
            mem_attr[dev_idx_access_by] = dev_idx;
            if (dev_idx == UVM_HOST_ID) {
                mem_attr[dev_idx_access_by] = MEM_RANGE_CPU_DEVICE_ID;
            }
            if (dev_idx == UVM_INVALID_DEVICE_ID) {
                mem_attr[dev_idx_access_by] = MEM_RANGE_INVALID_DEVICE_ID;
            }
            dev_idx_access_by++;
        }
    }
    for (; dev_idx_access_by < data_size; dev_idx_access_by++) {
        mem_attr[dev_idx_access_by] = MEM_RANGE_INVALID_DEVICE_ID;
    }

    return 0;
}

static int devmm_get_access_by_loc_attr(struct uvm_page_info page_info, size_t num_pages, int32_t *mem_attr,
                                        size_t data_size)
{
    int32_t dev_max_num = UVM_INVALID_DEVICE_ID + 1;
    bool *dev_access_by, *dev_cur_access_by;
    int32_t *device_per_page;
    int32_t ret = 0;

    device_per_page = devmm_kzalloc_ex(sizeof(int32_t) * dev_max_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (device_per_page == NULL) {
        devmm_drv_err("Kvzalloc fail. (num=%u)\n", sizeof(int32_t) * dev_max_num);
        return -ENOMEM;
    }
    dev_cur_access_by = devmm_kzalloc_ex(sizeof(bool) * dev_max_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    dev_access_by = devmm_kzalloc_ex(sizeof(bool) * dev_max_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if ((dev_cur_access_by == NULL) || (dev_access_by == NULL)) {
        devmm_drv_err("Kvzalloc fail. (num=%u)\n", sizeof(bool) * dev_max_num);
        ret = -ENOMEM;
        goto mem_free;
    }

    ret = devmm_set_access_by_loc_attr(page_info, num_pages, mem_attr, data_size, dev_access_by, dev_cur_access_by,
                                       device_per_page);

mem_free:
    devmm_kfree_ex(device_per_page);
    if (dev_cur_access_by != NULL) {
        devmm_kfree_ex(dev_cur_access_by);
    }
    if (dev_access_by != NULL) {
        devmm_kfree_ex(dev_access_by);
    }
    return ret;
}

static int devmm_get_preferred_loc_id_attr(struct uvm_page_info page_info, size_t num_pages, int32_t *mem_attr,
                                           size_t data_size)
{
    int32_t location_dev, location_type;

    (void)devmm_get_prefer_loc_attr_detail(page_info, num_pages, data_size, &location_dev, &location_type);

    if (location_dev == UVM_INVALID_DEVICE_ID ||
        ((location_type != DRV_UVM_LOCATION_TYPE_DEVICE) && (location_type != DRV_UVM_LOCATION_TYPE_HOST_NUMA))) {
        *mem_attr = MEM_RANGE_INVALID_DEVICE_ID;
        return 0;
    }
    /* adapt runtime numa_id will add 1 */
    if (location_type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) {
        --location_dev;
    }

    *mem_attr = location_dev;

    return 0;
}

static int devmm_get_last_prefetch_loc_type_attr(struct uvm_page_info page_info, size_t num_pages, int32_t *mem_attr,
                                                 size_t data_size)
{
    int32_t location_dev, start_idx = 0;
    page_bitmap_t *page_bitmap = page_info.page_bitmap;
    size_t page_idx;
    u32 stamp = (u32)ka_jiffies;

    location_dev = page_bitmap_get_last_prefetch_loc_type(&page_bitmap[0]);
    for (page_idx = 0; page_idx < num_pages; page_idx++) {
        if (location_dev != page_bitmap_get_last_prefetch_loc_type(&page_bitmap[page_idx])) {
            location_dev = DRV_UVM_LOCATION_TYPE_INVALID;
            break;
        }
        devmm_try_cond_resched(&stamp);
    }
    mem_attr[start_idx] = location_dev;

    return 0;
}

static int devmm_get_last_prefetch_loc_attr_detail(struct uvm_page_info page_info, size_t num_pages, size_t data_size,
                                                   int32_t *location_dev, int32_t *location_type)
{
    int32_t loc_dev, loc_type;
    size_t page_idx = 0;
    page_bitmap_t *page_bitmap = page_info.page_bitmap;
    u32 stamp = (u32)ka_jiffies;

    (void)devmm_get_last_prefetch_loc_type_attr(page_info, num_pages, &loc_type, data_size);
    loc_dev = page_bitmap_get_last_prefetch_loc(&page_bitmap[page_idx]);
    for (; page_idx < num_pages; page_idx++) {
        if (loc_dev != page_bitmap_get_last_prefetch_loc(&page_bitmap[page_idx])) {
            loc_dev = UVM_INVALID_DEVICE_ID;
            break;
        }
        devmm_try_cond_resched(&stamp);
    }
    *location_dev = loc_dev;
    *location_type = loc_type;

    return 0;
}

static int devmm_get_last_prefetch_loc_attr(struct uvm_page_info page_info, size_t num_pages, int32_t *mem_attr,
                                            size_t data_size)
{
    bool loc_type_same = true, loc_type_host = true;
    int32_t last_location_dev, last_location_dev_tmp, last_location_type;
    page_bitmap_t *page_bitmap = page_info.page_bitmap;
    size_t page_idx;
    u32 stamp = (u32)ka_jiffies;

    last_location_dev = page_bitmap_get_last_prefetch_loc_type(&page_bitmap[0]);
    for (page_idx = 0; page_idx < num_pages; page_idx++) {
        last_location_dev_tmp = page_bitmap_get_last_prefetch_loc_type(&page_bitmap[page_idx]);
        if (loc_type_same && (last_location_dev != last_location_dev_tmp)) {
            loc_type_same = false;
        }
        if ((last_location_dev_tmp != DRV_UVM_LOCATION_TYPE_HOST_NUMA) &&
            (last_location_dev_tmp != DRV_UVM_LOCATION_TYPE_HOST)) {
            loc_type_host = false;
        }
        devmm_try_cond_resched(&stamp);
    }

    if (loc_type_host) {
        last_location_dev = MEM_RANGE_CPU_DEVICE_ID;
    } else if (!loc_type_same) {
        last_location_dev = MEM_RANGE_INVALID_DEVICE_ID;
    } else {
        (void)devmm_get_last_prefetch_loc_attr_detail(page_info, num_pages, data_size, &last_location_dev,
                                                      &last_location_type);
        if (last_location_type == DRV_UVM_LOCATION_TYPE_INVALID) {
            last_location_dev = MEM_RANGE_INVALID_DEVICE_ID;
        }
    }

    *mem_attr = last_location_dev;

    return 0;
}

static int devmm_get_last_prefetch_loc_id_attr(struct uvm_page_info page_info, size_t num_pages, int32_t *mem_attr,
                                               size_t data_size)
{
    int32_t last_location_dev, last_location_type;

    (void)devmm_get_last_prefetch_loc_attr_detail(page_info, num_pages, data_size, &last_location_dev,
                                                  &last_location_type);

    if (last_location_dev == UVM_INVALID_DEVICE_ID || ((last_location_type != DRV_UVM_LOCATION_TYPE_DEVICE) &&
                                                       (last_location_type != DRV_UVM_LOCATION_TYPE_HOST_NUMA))) {
        *mem_attr = MEM_RANGE_INVALID_DEVICE_ID;
        return 0;
    }
    /* adapt runtime numa_id will add 1 */
    if (last_location_type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) {
        --last_location_dev;
    }

    *mem_attr = last_location_dev;

    return 0;
}

typedef int32_t (*devmm_get_mem_range_attr)(struct uvm_page_info page_info, size_t num_pages, int32_t *mem_attr,
                                            size_t data_size);
static const devmm_get_mem_range_attr handle_mem_range_attr[MEM_RANGE_ATTR_MAX] = {
    [MEM_RANGE_ATTR_READ_MOSTLY] = devmm_get_read_mostly_attr,
    [MEM_RANGE_ATTR_PREFERRED_LOC] = devmm_get_prefer_loc_attr,
    [MEM_RANGE_ATTR_ACCESS_BY] = devmm_get_access_by_loc_attr,
    [MEM_RANGE_ATTR_PREFERRED_LOC_TYPE] = devmm_get_preferred_loc_type_attr,
    [MEM_RANGE_ATTR_PREFERRED_LOC_ID] = devmm_get_preferred_loc_id_attr,
    [MEM_RANGE_ATTR_LAST_PREFETCH_LOC] = devmm_get_last_prefetch_loc_attr,
    [MEM_RANGE_ATTR_LAST_PREFETCH_LOC_TYPE] = devmm_get_last_prefetch_loc_type_attr,
    [MEM_RANGE_ATTR_LAST_PREFETCH_LOC_ID] = devmm_get_last_prefetch_loc_id_attr,
};
static int devmm_uvm_mem_attr_query(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                    struct devmm_mem_range_attribute *attr_arg)
{
    struct uvm_page_info page_info = {};
    size_t num_pages;
    int32_t *mem_attr;
    uint32_t chunk_page_size = uvm_heap->chunk_page_size;
    int ret = 0, page_idx;
    page_bitmap_t *page_bitmap;
    u32 stamp = (u32)ka_jiffies;

    if ((attr_arg->attribute >= MEM_RANGE_ATTR_MAX) || (handle_mem_range_attr[attr_arg->attribute] == NULL)) {
        devmm_drv_err("Invalid attribute type. (attribute=%u)\n", attr_arg->attribute);
        return -EINVAL;
    }

    num_pages = (attr_arg->size + chunk_page_size - 1) / chunk_page_size;
    ret = uvm_get_page_info_by_va(uvm_heap, attr_arg->ptr, &page_info);
    if (ret) {
        return ret;
    }
    /* check */
    page_bitmap = page_info.page_bitmap;
    for (page_idx = 0; page_idx < num_pages; page_idx++) {
        if (!page_bitmap_get_alloced(page_bitmap + page_idx)) {
            devmm_drv_err("Uvm mem not alloced.\n");
            return -EINVAL;
        }
        devmm_try_cond_resched(&stamp);
    }

    mem_attr = devmm_kzalloc_ex(attr_arg->data_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (mem_attr == NULL) {
        devmm_drv_err("Kvzalloc fail. (num=%u)\n", attr_arg->data_size);
        return -ENOMEM;
    }

    ret = handle_mem_range_attr[attr_arg->attribute](page_info, num_pages, mem_attr,
                                                     attr_arg->data_size / sizeof(int32_t));
    if (ret) {
        devmm_drv_err("Uvm get mem attribute failed. (ret=%d)\n", ret);
        goto mem_attr_free;
    }

    if (ka_base_copy_to_user(attr_arg->data, mem_attr, attr_arg->data_size) != 0) {
        devmm_drv_err("Uvm copy_to_user fail.\n");
        goto mem_attr_free;
    }

mem_attr_free:
    devmm_kfree_ex(mem_attr);
    return ret;
}

static int mem_managed_attrs_get_size(struct devmm_mem_range_attributes *attrs_arg, uint32_t *mem_attrs_size,
                                      uint32_t *data_num_size, uint32_t *data_size)
{
    if ((attrs_arg->attribute_num > KA_U32_MAX / sizeof(uint32_t)) ||
        (attrs_arg->attribute_num > KA_U32_MAX / sizeof(size_t)) ||
        (attrs_arg->attribute_num > KA_U32_MAX / sizeof(uint64_t))) {
        devmm_drv_err("Attribute number is too large. (num=%u; max=%u)\n", attrs_arg->attribute_num,
                      (u32)(KA_U32_MAX / sizeof(uint64_t)));
        return -EINVAL;
    }
    *mem_attrs_size = (uint32_t)(attrs_arg->attribute_num * sizeof(uint32_t));
    *data_num_size = (uint32_t)(attrs_arg->attribute_num * sizeof(size_t));
    *data_size = (uint32_t)(attrs_arg->attribute_num * sizeof(uint64_t)); /* store addr */
    return 0;
}

static int mem_managed_attrs_alloc(uint32_t mem_attrs_size, uint32_t data_num_size, uint32_t data_size,
                                   uint32_t **attributes, size_t **data_sizes, uint64_t **data)
{
    *attributes = devmm_kzalloc_ex(mem_attrs_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (*attributes == NULL) {
        devmm_drv_err("Uvm attributes Kvzalloc fail. (num=%u)\n", mem_attrs_size);
        return -ENOMEM;
    }
    *data_sizes = devmm_kzalloc_ex(data_num_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
#ifndef EMU_ST
    if (*data_sizes == NULL) {
        devmm_drv_err("Uvm data_sizes Kvzalloc fail. (num=%u)\n", data_num_size);
        devmm_kfree_ex(*attributes);
        return -ENOMEM;
    }
#endif
    *data = devmm_kzalloc_ex(data_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (*data == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Uvm data Kvzalloc fail. (num=%u)\n", data_size);
        devmm_kfree_ex(*attributes);
        devmm_kfree_ex(*data_sizes);
#endif
        return -ENOMEM;
    }
    return 0;
}

static int mem_managed_attrs_copy_from_usr(struct devmm_mem_range_attributes *attrs_arg, uint32_t **attributes,
                                           size_t **data_sizes, uint64_t **data)
{
    uint32_t mem_attrs_size, data_num_size, data_size;
    int ret;

    ret = mem_managed_attrs_get_size(attrs_arg, &mem_attrs_size, &data_num_size, &data_size);
    if (ret != 0) {
        return ret;
    }
    ret = mem_managed_attrs_alloc(mem_attrs_size, data_num_size, data_size, attributes, data_sizes, data);
    if (ret != 0) {
        return ret;
    }

    /* data init */
    if (ka_base_copy_from_user(*attributes, (void __ka_user *)attrs_arg->attributes, mem_attrs_size) != 0) {
        devmm_drv_err("Uvm attributes copy from user fail. (num=%u)\n", mem_attrs_size);
        ret = -EINVAL;
        goto out;
    }
    if (ka_base_copy_from_user(*data_sizes, (void __ka_user *)attrs_arg->data_sizes, data_num_size) != 0) {
        devmm_drv_err("Uvm data_sizes copy from user fail. (num=%u)\n", data_num_size);
        ret = -EINVAL;
        goto out;
    }
    if (ka_base_copy_from_user(*data, (void __ka_user *)(attrs_arg->data), data_size) != 0) {
        devmm_drv_err("Uvm data copy from user fail. (num=%u)\n", data_size);
        ret = -EINVAL;
        goto out;
    }

out:
    if (ret != 0) {
#ifndef EMU_ST
        devmm_kfree_ex(*data);
        devmm_kfree_ex(*data_sizes);
        devmm_kfree_ex(*attributes);
#endif
    }
    return ret;
}

static int mem_managed_attrs_init(struct devmm_mem_range_attributes *attrs_arg)
{
    int ret = 0;
    uint32_t *attributes = NULL;
    uint64_t *data = NULL;
    size_t *data_sizes = NULL;

    ret = mem_managed_attrs_copy_from_usr(attrs_arg, &attributes, &data_sizes, &data);
    if (ret != 0) {
#ifndef EMU_ST
        return ret;
#endif
    }

    attrs_arg->attributes = attributes;
    attrs_arg->data_sizes = data_sizes;
    attrs_arg->data = (void **)data;

    return ret;
}

static int mem_managed_attrs_uninit(struct devmm_mem_range_attributes *attrs_arg)
{
    devmm_kfree_ex(attrs_arg->attributes);
    devmm_kfree_ex(attrs_arg->data_sizes);
    devmm_kfree_ex(attrs_arg->data);
    return 0;
}

static int devmm_uvm_mem_attrs_query(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                     struct devmm_mem_range_attributes *attrs_arg)
{
    struct devmm_mem_range_attribute single_mem_attr = {};
    uint32_t mem_attr_num = attrs_arg->attribute_num, mem_attr_idx = 0;
    u32 stamp = (u32)ka_jiffies;
    int ret;

    ret = mem_managed_attrs_init(attrs_arg);
    if (ret) {
        devmm_drv_err("Mem attribute init failed. (ret=%d)\n", ret);
        return ret;
    }

    for (; mem_attr_idx < mem_attr_num; mem_attr_idx++) {
        devmm_drv_debug("Uvm memory kernel attribute get after init. (attr_idx=%d, attribute=%d, ptr=0x%llx, "
                        "ptr_size=%lx, data_ptr=0x%llx, data_size=%lx) \n",
                        mem_attr_idx, attrs_arg->attributes[mem_attr_idx], attrs_arg->ptr, attrs_arg->size,
                        (uint64_t)(attrs_arg->data)[mem_attr_idx], attrs_arg->data_sizes[mem_attr_idx]);
        single_mem_attr.attribute = attrs_arg->attributes[mem_attr_idx];
        single_mem_attr.ptr = attrs_arg->ptr;
        single_mem_attr.size = attrs_arg->size;
        single_mem_attr.data = (int32_t *)(attrs_arg->data)[mem_attr_idx];
        single_mem_attr.data_size = attrs_arg->data_sizes[mem_attr_idx];
        ret = devmm_uvm_mem_attr_query(svm_proc, uvm_heap, &single_mem_attr);
        if (ret) {
            devmm_drv_err("get mem managed range attribute failed.\n");
            goto mem_attrs_free;
        }
        devmm_try_cond_resched(&stamp);
    }
mem_attrs_free:
    mem_managed_attrs_uninit(attrs_arg);
    return ret;
}

int devmm_uvm_ioctl_get_mem_range_attributes(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_range_attributes *attrs_arg = &arg->data.mem_range_attribute_paras;
    struct devmm_uvm_heap *uvm_heap = NULL;

    uvm_heap = devmm_uvm_get_heap(svm_proc, (unsigned long)attrs_arg->ptr);
    if (uvm_heap == NULL) {
        devmm_drv_err("UVM Heap is NULL or error.\n");
        return -EADDRNOTAVAIL;
    }
    attrs_arg->ptr = ka_base_round_down(attrs_arg->ptr, uvm_heap->chunk_page_size);
    attrs_arg->size = KA_DRIVER_ALIGN(attrs_arg->size, uvm_heap->chunk_page_size);

    /* check uvm mem range */
    if (!devmm_vaddr_and_size_is_in_uvm_range(attrs_arg->ptr, attrs_arg->size)) {
        devmm_drv_err("Uvm addr is not in uvm range. (ptr=%llx)\n", attrs_arg->ptr);
        return -EFAULT;
    }

    if (attrs_arg->attribute_num <= 0 || attrs_arg->attribute_num >= MEM_RANGE_ATTR_MAX) {
#ifndef EMU_ST
        devmm_drv_err("Invalid attribute_num. (num=%u)\n", attrs_arg->attribute_num);
        return -EINVAL;
#endif
    }

    return devmm_uvm_mem_attrs_query(svm_proc, uvm_heap, attrs_arg);
}
