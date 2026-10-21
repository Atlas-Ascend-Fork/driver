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

#include "uvm_master_memset.h"
#include "svm_log.h"
#include "uvm_heap_mng.h"
#include "devmm_proc_info.h"
#include "svm_msg_client.h"
#include "uvm_master_common.h"
#include "svm_kernel_msg.h"
#include "devmm_common.h"

struct uvm_memory_attributes {
    bool is_uvm_host;
    bool is_uvm_device;
    bool is_uvm_non_page;
    bool is_readmostly;
    uint64_t va;
    uint64_t pa;
    uint32_t devid;
    int device_count;
    int device_ids[BITMAP_BITS_COUNT];
};

static int uvm_memset_send_device_msg(struct devmm_svm_process *svm_proc, struct devmm_mem_memset_para *memset_para,
                                      uint32_t devid)
{
    int ret;
    struct devmm_chan_memset devmm_chan_memset_dev = {0};

    devmm_chan_memset_dev.head.dev_id = devmm_get_phyid_devid_from_svm_process(svm_proc, devid);
    devmm_chan_memset_dev.head.msg_id = DEVMM_CHAN_UVM_CHAN_MEMSET_H2D_ID;
    devmm_chan_memset_dev.head.process_id.hostpid = svm_proc->process_id.hostpid;
    devmm_chan_memset_dev.value = memset_para->value;
    devmm_chan_memset_dev.dst = memset_para->dst;
    devmm_chan_memset_dev.count = memset_para->count;

    ret = devmm_chan_msg_send(&devmm_chan_memset_dev, sizeof(devmm_chan_memset_dev), sizeof(devmm_chan_memset_dev));
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Device memset error. (ret=%d; dst=0x%llx; count=%lu)\n", ret, memset_para->dst,
                      memset_para->count);
        return ret;
    }
#endif
    return ret;
}

static int uvm_memset_device_process(struct devmm_svm_process *svm_proc, struct devmm_mem_memset_para *memset_para,
                                     struct uvm_memory_attributes *attr)
{
    uint64_t va_start;
    unsigned long size;
    unsigned long va_end;
    int ret, i;

    va_start = memset_para->dst;
    size = memset_para->count;
    va_end = va_start + size;

    if (attr->is_readmostly) {
        for (i = 0; i < attr->device_count; i++) {
            attr->devid = attr->device_ids[i];
            ret = uvm_memset_send_device_msg(svm_proc, memset_para, attr->devid);
#ifndef EMU_ST
            if (ret != 0) {
                devmm_drv_err("memset dma h2d failed. (dev_id=%u; va=0x%llx)\n", attr->devid, memset_para->dst);
                return ret;
            }
#endif
        }
    } else {
        ret = uvm_memset_send_device_msg(svm_proc, memset_para, attr->devid);
#ifndef EMU_ST
        if (ret != 0) {
            devmm_drv_err("Send memset device message failed. (dst=0x%llx; count=%lu)\n", memset_para->dst,
                          memset_para->count);
            return ret;
        }
#endif
    }

    return 0;
}

static int uvm_memset_alloc_host_page(struct devmm_svm_process *svm_proc, struct uvm_memory_attributes *attr,
                                      struct uvm_page_info *page_info, uint64_t va_start, uint64_t memset_count,
                                      uint64_t total_memset_size)
{
    int ret;

    ret = uvm_host_alloc_and_map_page(svm_proc, page_info, page_info->va_align, -1, false);
    if (ret != 0) {
#ifndef EMU_ST
        struct ka_sysinfo mem_info = {0};
        ka_si_meminfo(&mem_info);
        devmm_drv_err("OOM: alloc huge pages fail, out of memory. (ret=%d; hostpid=%d; vfid=%u; "
                      "total_memset_size=0x%llx; free_mem=0x%llx; va=0x%llx)\n",
                      ret, svm_proc->process_id.hostpid, svm_proc->process_id.vfid, total_memset_size,
                      (uint64_t)mem_info.freeram << KA_MM_PAGE_SHIFT, va_start);
#else
        (void)memset_count;
#endif
        return ret;
    }

    uvm_set_bitmap_mapped(page_info->page_bitmap, UVM_HOST_ID);
    attr->pa = page_info->page_map->pa_addr;
    devmm_drv_debug("Memset alloc host page succeeded. (dst=0x%llx; byte_count=%llu)\n", va_start, memset_count);
    return 0;
}

static int uvm_memset_host_process(struct devmm_svm_process *svm_proc, struct devmm_mem_memset_para *memset_para,
                                   struct uvm_memory_attributes *attr, struct uvm_page_info *page_info,
                                   uint64_t total_memset_size)
{
    int ret;
    void *kvaddr;
    uint64_t memset_size, paddr;
    uint64_t va_start, va_end, page_offset;
    uint8_t value;
    ka_vm_area_struct_t *vma;

    va_start = memset_para->dst;
    va_end = va_start + memset_para->count;
    value = (uint8_t)memset_para->value;

    page_offset = va_start & (DEVMM_UVM_PAGE_SIZE - 1);

    vma = devmm_find_vma(svm_proc, va_start);
    if (vma == NULL) {
        devmm_drv_err("Failed to find vma. (va=0x%llx)\n", va_start);
        return -EINVAL;
    }

    if (attr->is_uvm_non_page) {
        devmm_zap_vma_ptes(vma, page_info->va_align, DEVMM_UVM_PAGE_SIZE);
        ret = uvm_memset_alloc_host_page(svm_proc, attr, page_info, va_start, memset_para->count, total_memset_size);
        if (ret != 0) {
            return ret;
        }
    }

    paddr = attr->pa;
    memset_size = memset_para->count;

#ifndef EMU_ST
    kvaddr = ka_mm_phys_to_virt(paddr + page_offset);
#else
    paddr = (paddr + page_offset) >> KA_MM_PAGE_SHIFT;
    kvaddr = ka_mm_phys_to_virt(paddr);
#endif
#ifndef EMU_ST
    if (kvaddr == NULL) {
        devmm_drv_err("Phys_to_virt failed. (size=0x%llx)\n", memset_size);
        return -EINVAL;
    }

    ret = memset_s(kvaddr, DEVMM_UVM_PAGE_SIZE - page_offset, value, memset_size);
    if (ret != 0) {
        devmm_drv_err("Memset_s failed. (ret=%d; size=0x%llx)\n", ret, (uint64_t)(DEVMM_UVM_PAGE_SIZE - page_offset));
        return ret;
    }
#endif

    return 0;
}

static int uvm_get_memory_attributes(struct devmm_uvm_heap *uvm_heap, uint64_t addr, struct uvm_memory_attributes *attr,
                                     struct uvm_page_info *page_info)
{
    page_bitmap_t *page_bitmap;
    struct uvm_page_mapping *page_map;
    device_bitmap *device_bitmap;
    int ret;

    if (page_info == NULL || page_info->page_map == NULL) {
        ret = uvm_get_page_info_by_va(uvm_heap, addr, page_info);
#ifndef EMU_ST
        if (ret != 0) {
            devmm_drv_err("Fail to get page info.\n");
            return -EFAULT;
        }
#endif
    }

    page_map = page_info->page_map;
    page_bitmap = page_info->page_bitmap;
    device_bitmap = page_info->dev_bitmap;

    *attr = (struct uvm_memory_attributes){0};
    attr->va = addr;
    attr->is_uvm_host = page_bitmap_get_host_mapped(page_bitmap);
    attr->is_uvm_device = page_bitmap_get_device_mapped(page_bitmap);
    attr->is_readmostly = page_bitmap_get_read_mostly(page_bitmap);
    attr->devid = page_map->devid;
    attr->pa = page_map->pa_addr;
    attr->device_count = get_present_devices(device_bitmap, attr->device_ids, BITMAP_BITS_COUNT);
    attr->is_uvm_non_page = (!attr->is_uvm_host && !attr->is_uvm_device);

    return 0;
}

static int devmm_memset_page_processing(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                        uint64_t vaddr, uint64_t memset_size, uint64_t value,
                                        struct uvm_page_info *page_info, uint64_t total_memset_size)
{
    int ret;
    struct uvm_memory_attributes attr = {};
    struct devmm_mem_memset_para current_para = {};

    ret = uvm_get_memory_attributes(uvm_heap, vaddr, &attr, page_info);
#ifndef EMU_ST
    if (ret != 0) {
        devmm_drv_err("Get memory attributes failed. (va=0x%llx)\n", vaddr);
        return ret;
    }
#endif

    current_para.dst = vaddr;
    current_para.count = memset_size;
    current_para.value = value;

    if (attr.is_uvm_non_page || attr.is_uvm_host) {
        ret = uvm_memset_host_process(svm_proc, &current_para, &attr, page_info, total_memset_size);
#ifndef EMU_ST
        if (ret != 0) {
            devmm_drv_err("uvm_memset_host_process failed. (ret=%d; dst=0x%llx)\n", ret, current_para.dst);
            return ret;
        }
#endif
    }

    if (attr.is_uvm_device) {
        ret = uvm_memset_device_process(svm_proc, &current_para, &attr);
#ifndef EMU_ST
        if (ret != 0) {
            devmm_drv_err("uvm_memset_device_process failed. (ret=%d; dst=0x%llx)\n", ret, current_para.dst);
            return ret;
        }
#endif
    }

    return 0;
}

#ifndef EMU_ST
static void devmm_memset_rollback_alloced(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                          uint64_t *alloced_vaddrs, uint32_t alloced_count)
{
    uint32_t i;
    struct uvm_page_info rb_info;

    for (i = 0; i < alloced_count; i++) {
        if (uvm_get_page_info_by_va(uvm_heap, alloced_vaddrs[i], &rb_info) != 0) {
            devmm_drv_err("Rollback: failed to get page_info. (va=0x%llx)\n", alloced_vaddrs[i]);
            continue;
        }
        ka_task_down_write(rb_info.page_rwlock);
        (void)uvm_free_host_page(svm_proc, &rb_info, 0);
        ka_task_up_write(rb_info.page_rwlock);
    }
}
#endif

static int devmm_memset_partial_proc_one(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                         uint64_t vaddr, uint64_t memset_size, uint64_t value,
                                         struct uvm_page_info *page_info, bool *was_alloced, uint64_t total_memset_size)
{
    int ret;
    bool was_no_mapped;

    ret = uvm_get_page_info_by_va(uvm_heap, vaddr, page_info);
#ifndef EMU_ST
    if (ret) {
        devmm_drv_err("Failed to get page_info for va=0x%llx\n", vaddr);
        return ret;
    }
#endif
    ka_task_down_write(page_info->page_rwlock);

    if (!page_bitmap_get_alloced(page_info->page_bitmap)) {
#ifndef EMU_ST
        devmm_drv_err("Va is not alloced, va = 0x%llx.\n", vaddr);
#endif
        ka_task_up_write(page_info->page_rwlock);
        return -EINVAL;
    }

    was_no_mapped = page_is_no_mapped(page_info->page_bitmap);

    ret = devmm_memset_page_processing(svm_proc, uvm_heap, vaddr, memset_size, value, page_info, total_memset_size);

    ka_task_up_write(page_info->page_rwlock);

    if (ret != 0) {
        return ret;
    }

    *was_alloced = was_no_mapped;
    return 0;
}

static int devmm_memset_partial(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                struct devmm_mem_memset_para *memset_para, uint64_t total_memset_size)
{
    int ret;
    uint64_t vaddr, left_size, offset_size, page_size, memset_size;
    uint64_t max_pages;
    uint32_t alloced_count = 0;
    uint64_t *alloced_vaddrs = NULL;
    bool was_alloced = false;
    struct uvm_page_info page_info = {};
    u32 stamp = (u32)ka_jiffies;

    devmm_drv_debug("Memset parameter. (dst=0x%llx; value=0x%llx; count=%llu)\n", memset_para->dst, memset_para->value,
                    memset_para->count);

    left_size = memset_para->count;
    page_size = DEVMM_UVM_PAGE_SIZE;
    vaddr = memset_para->dst;

    max_pages = (memset_para->count + page_size - 1) / page_size + 1;
    alloced_vaddrs = (uint64_t *)devmm_kvalloc(sizeof(uint64_t) * max_pages, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (alloced_vaddrs == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Failed to alloc rollback tracking array. (max_pages=%llu)\n", max_pages);
        return -ENOMEM;
#endif
    }

    do {
        offset_size = vaddr & (page_size - 1);
        memset_size = page_size - offset_size;
        memset_size = ka_base_min(memset_size, left_size);
        if (memset_size == 0) {
            break;
        }

        devmm_try_cond_resched(&stamp);

        ret = devmm_memset_partial_proc_one(svm_proc, uvm_heap, vaddr, memset_size, memset_para->value, &page_info,
                                            &was_alloced, total_memset_size);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_memset_rollback_alloced(svm_proc, uvm_heap, alloced_vaddrs, alloced_count);
            devmm_kvfree(alloced_vaddrs);
#endif
            return ret;
        }

        if (was_alloced) {
            alloced_vaddrs[alloced_count++] = page_info.va_align;
        }

        vaddr += memset_size;
        left_size -= memset_size;
    } while (left_size > 0);

    devmm_kvfree(alloced_vaddrs);
    return 0;
}

static int devmm_memset_partial_range(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                      struct devmm_mem_memset_para *memset_para, uint64_t vaddr, uint64_t size,
                                      uint64_t total_memset_size)
{
    if (size == 0) {
        return 0;
    }
    memset_para->dst = vaddr;
    memset_para->count = size;
    return devmm_memset_partial(svm_proc, uvm_heap, memset_para, total_memset_size);
}

int devmm_uvm_ioctl_memset_process(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    int ret;
    uint64_t head_size, middle_size, tail_size;
    uint64_t vaddr = arg->data.memset_para.dst;
    struct devmm_mem_memset_para memset_para;
    struct devmm_uvm_heap *uvm_heap = NULL;

    uvm_heap = devmm_uvm_get_heap(svm_proc, (unsigned long)vaddr);
#ifndef EMU_ST
    if (uvm_heap == NULL) {
        devmm_drv_err("UVM Heap is NULL or error for va=0x%llx.\n", vaddr);
        return -EADDRNOTAVAIL;
    }
#endif
    memset_para.dst = arg->data.memset_para.dst;
    memset_para.value = arg->data.memset_para.value;
    memset_para.count = arg->data.memset_para.count;
    if (memset_para.count > DEVMM_UVM_MEM_SIZE) {
        devmm_drv_err("Memset count exceeds UVM range. (count=%llu; max=%llu)\n", memset_para.count,
                      (u64)DEVMM_UVM_MEM_SIZE);
        return -EINVAL;
    }

    head_size = (DEVMM_UVM_PAGE_SIZE - (arg->data.memset_para.dst & ~UVM_PAGE_MASK)) % DEVMM_UVM_PAGE_SIZE;
    if (arg->data.memset_para.count < head_size) {
        head_size = arg->data.memset_para.count;
    }
    tail_size = (arg->data.memset_para.count - head_size) & ~UVM_PAGE_MASK;
    middle_size = arg->data.memset_para.count - head_size - tail_size;

    ret = devmm_memset_partial_range(svm_proc, uvm_heap, &memset_para, vaddr, head_size, arg->data.memset_para.count);
    if (ret != 0) {
        return ret;
    }
    vaddr += head_size;

    ret = devmm_memset_partial_range(svm_proc, uvm_heap, &memset_para, vaddr, middle_size, arg->data.memset_para.count);
    if (ret != 0) {
        return ret;
    }
    vaddr += middle_size;

    return devmm_memset_partial_range(svm_proc, uvm_heap, &memset_para, vaddr, tail_size, arg->data.memset_para.count);
}
