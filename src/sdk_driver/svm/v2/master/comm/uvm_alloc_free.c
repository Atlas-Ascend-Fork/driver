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
#include "uvm_heap_mng.h"
#include "svm_msg_client.h"
#include "devmm_mem_alloc_interface.h"
#include "devmm_common.h"
#include "svm_heap_mng.h"

page_bitmap_t *devmm_get_page_bitmap_with_uvm_heap(struct devmm_uvm_heap *heap, u64 va)
{
    unsigned long pfn;
    int ret;

    ret = devmm_get_virt_pfn_by_uvm_heap(heap, va, &pfn);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Fail to get virt pfn of va. (va=0x%llx)\n", va);
        return NULL;
#endif
    }

    return &heap->page_bitmaps[pfn];
}

STATIC page_bitmap_t *devmm_get_fst_alloc_bitmap_by_uvm_heap(struct devmm_svm_process *svm_process,
                                                             struct devmm_uvm_heap *heap, u64 va)
{
    page_bitmap_t *page_bitmap = NULL;

    page_bitmap = devmm_get_page_bitmap_with_uvm_heap(heap, va);
    if (page_bitmap == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Get page bitmap failed for uvm heap. (va=0x%llx)\n", va);
        return NULL;
#endif
    }

    if (!page_bitmap_get_alloced(page_bitmap) || !page_bitmap_get_first_page(page_bitmap)) {
        devmm_drv_err("Va isn't first page. (va=0x%llx; page_bitmap=0x%x)\n", va, page_bitmap_read(page_bitmap));
        return NULL;
    }

    return page_bitmap;
}

STATIC void devmm_uvm_max_used_pages_update(struct devmm_uvm_heap *heap, u64 va, u64 page_cnt, bool is_free)
{
    unsigned long pfn;
    int ret;

    ret = devmm_get_virt_pfn_by_uvm_heap(heap, va, &pfn);
    if (ret != 0) {
        devmm_drv_err("Fail to get virt pfn of va. (va=0x%llx)\n", va);
        return;
    }
    ka_task_write_lock(&heap->uvm_heap_lock);
    if (!is_free) {
        if (heap->max_used_page < pfn + page_cnt) {
            heap->max_used_page = pfn + page_cnt;
        }
    } else if (heap->max_used_page == pfn + page_cnt) {
        heap->max_used_page -= page_cnt;
    }
    ka_task_write_unlock(&heap->uvm_heap_lock);
}

int uvm_free_host_page(struct devmm_svm_process *svm_pro, struct uvm_page_info *page_info, uint64_t addr_idx)
{
    if (!page_bitmap_get_host_mapped(page_info->page_bitmap + addr_idx)) {
        return 0;
    }

    devmm_unmap_pages(svm_pro, page_info->va_align + DEVMM_UVM_PAGE_SIZE * addr_idx, 1);
    clear_page_mapping(page_info->page_map + addr_idx);
    page_bitmap_unset_host_mapped(page_info->page_bitmap + addr_idx);
    devmm_drv_debug("Uvm_free_host_page success. va= %llx.\n", page_info->va_align + DEVMM_UVM_PAGE_SIZE * addr_idx);
    return 0;
}

int uvm_free_device_page_by_id(struct devmm_svm_process *svm_pro, device_bitmap *dev_bitmap, uint32_t dev_id, u64 va)
{
    struct devmm_chan_free_pages free_page_info = {{{0}}};
    int ret = 0;
    free_page_info.head.dev_id = devmm_get_phyid_devid_from_svm_process(svm_pro, dev_id);
    free_page_info.head.msg_id = DEVMM_CHAN_UVM_FREE_PAGE_H2D_ID;
    free_page_info.head.process_id.hostpid = svm_pro->process_id.hostpid;
    free_page_info.va = va;
    ret = devmm_chan_msg_send(&free_page_info, sizeof(free_page_info), 0);
    if (ret) {
#ifndef EMU_ST
        devmm_drv_err("Failed to send free page msg, va = 0x%llx ret = %d\n", va, ret);
        return -EFAULT;
#endif
    }

    clear_device_present(dev_bitmap, dev_id);

    return 0;
}

int uvm_free_device_page(struct devmm_svm_process *svm_pro, struct uvm_page_info *page_info, uint64_t addr_idx,
                         bool no_return)
{
    int ret;
    uint16_t i, count;
    int *device_ids;
    struct devmm_uvm_heap *uvm_heap = NULL;

    uvm_heap = svm_pro->uvm_heap;
    if (uvm_heap == NULL) {
        return -EINVAL;
    }

    if (!page_bitmap_get_device_mapped(page_info->page_bitmap + addr_idx)) {
        return 0;
    }

    device_ids = devmm_kvzalloc(sizeof(int) * BITMAP_BITS_COUNT);
    if (!device_ids) {
        return -ENOMEM;
    }

    count = get_present_devices(page_info->dev_bitmap + addr_idx, device_ids, BITMAP_BITS_COUNT);
    for (i = 0; i < count; i++) {
        if (is_device_present(&uvm_heap->invalid_devices, device_ids[i])) {
#ifndef EMU_ST
            devmm_drv_debug("Device is invalid. (va=0x%llx, device_id = %d)\n",
                            page_info->va_align + DEVMM_UVM_PAGE_SIZE * addr_idx, device_ids[i]);
            continue;
#endif
        }

        ret = uvm_free_device_page_by_id(svm_pro, page_info->dev_bitmap + addr_idx, device_ids[i],
                                         page_info->va_align + DEVMM_UVM_PAGE_SIZE * addr_idx);
        if (ret) {
#ifndef EMU_ST
            if (!no_return) {
                devmm_drv_err("Device failed to free va. (va=0x%llx, device_id = %d)\n",
                              page_info->va_align + DEVMM_UVM_PAGE_SIZE * addr_idx, device_ids[i]);
                devmm_kvfree_ex(device_ids);
                return ret;
            }
            set_device_present(&uvm_heap->invalid_devices, device_ids[i]);
#endif
        }
    }
    page_bitmap_unset_device_mapped(page_info->page_bitmap + addr_idx);
    devmm_kvfree_ex(device_ids);
    return 0;
}

int uvm_free_one_page(struct devmm_svm_process *svm_pro, struct uvm_page_info *page_info, uint64_t addr_idx,
                      bool no_return)
{
    int ret;

    ret = uvm_free_host_page(svm_pro, page_info, addr_idx);
    if (!no_return && ret) {
#ifndef EMU_ST
        devmm_drv_err("Host failed to free va. (va=0x%llx)\n", page_info->va_align + DEVMM_UVM_PAGE_SIZE * addr_idx);
        return ret;
#endif
    }
    ret = uvm_free_device_page(svm_pro, page_info, addr_idx, no_return);
    if (!no_return && ret) {
#ifndef EMU_ST
        devmm_drv_err("Device failed to free va. (va=0x%llx)\n", page_info->va_align + DEVMM_UVM_PAGE_SIZE * addr_idx);
        return ret;
#endif
    }

    return 0;
}

STATIC inline void page_bitmap_flag_clear(struct uvm_page_info page_info, u64 i)
{
    page_bitmap_unset_read_mostly(page_info.page_bitmap + i);
    page_bitmap_unset_preferred_loc(page_info.page_bitmap + i);
    page_bitmap_unset_preferred_loc_type(page_info.page_bitmap + i);
    clear_all_devices(page_info.dev_bitmap + i);
    clear_all_devices(page_info.access_by_bitmap + i);
    page_bitmap_unset_last_prefetch_loc(page_info.page_bitmap + i);
}

int devmm_free_page_process_uvm(struct devmm_svm_process *svm_pro, struct devmm_uvm_heap *heap, u64 va)
{
    page_bitmap_t *page_bitmap = NULL;
    struct uvm_page_info page_info;
    u64 i, page_num;
    int ret;
    u32 stamp = (u32)ka_jiffies;

    if (va != ka_base_round_down(va, heap->chunk_page_size)) {
#ifndef EMU_ST
        return -EADDRNOTAVAIL;
#endif
    }

    page_bitmap = devmm_get_fst_alloc_bitmap_by_uvm_heap(svm_pro, heap, va);
    if (page_bitmap == NULL) {
#ifndef EMU_ST
        return -EINVAL;
#endif
    }

    page_num = page_bitmap_get_alloc_len(page_bitmap);
    if (page_num == 0) {
#ifndef EMU_ST
        return -EADDRNOTAVAIL;
#endif
    }

    ret = uvm_get_page_info_by_va(heap, va, &page_info);
    if (ret) {
#ifndef EMU_ST
        devmm_drv_err("Failed to free va. (va=0x%llx; page_num=%llu)\n", va, page_num);
        return ret;
#endif
    }

    for (i = 0; i < page_num; i++) {
        ka_task_down_write(page_info.page_rwlock + i);
        ret = uvm_free_one_page(svm_pro, &page_info, i, false);
        if (ret) {
#ifndef EMU_ST
            ka_task_up_write(page_info.page_rwlock + i);
            devmm_drv_err("Failed to free va. (va=0x%llx)\n", page_info.va_align + DEVMM_UVM_PAGE_SIZE * i);
            break;
#endif
        }
        /* clear bitmap */
        page_bitmap_flag_clear(page_info, i);
        page_bitmap_clean(page_bitmap + i);
        ka_task_up_write(page_info.page_rwlock + i);
        devmm_try_cond_resched(&stamp);
    }

    devmm_uvm_max_used_pages_update(heap, va, page_num, true);

    return 0;
}

int devmm_uvm_ioctl_alloc(struct devmm_svm_process *svm_process, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_alloc_para *alloc_para = &arg->data.alloc_uvm_para;
    struct devmm_uvm_heap *heap = NULL;
    page_bitmap_t *page_bitmap = NULL;
    u64 chunk_cnt, i, j;
    u32 stamp = (u32)ka_jiffies;

    heap = devmm_uvm_get_heap(svm_process, (unsigned long)alloc_para->p);
    if ((heap == NULL) || (devmm_check_va_add_size_by_uvm_heap(heap, alloc_para->p, alloc_para->size) != 0)) {
#ifndef EMU_ST
        devmm_drv_err("Can't find heap. (va=0x%llx; size=%lu)\n", alloc_para->p, alloc_para->size);
        return -EADDRNOTAVAIL;
#endif
    }

    page_bitmap = devmm_get_page_bitmap_with_uvm_heap(heap, alloc_para->p);
    if (page_bitmap == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Can't find page_bitmap. (va=0x%llx; size=%lu)\n", alloc_para->p, alloc_para->size);
        return -EINVAL;
#endif
    }

    chunk_cnt = devmm_get_pagecount_by_size(alloc_para->p, alloc_para->size, heap->chunk_page_size);
    if (chunk_cnt == 0) {
#ifndef EMU_ST
        devmm_drv_err("Count error. (va=0x%llx; size=%lu)\n", alloc_para->p, alloc_para->size);
        return -EINVAL;
#endif
    }

    for (i = 0; i < chunk_cnt; i++) {
        if (page_bitmap_get_alloced(page_bitmap + i)) {
#ifndef EMU_ST
            devmm_drv_err("Already alloced i. (i=%llu; va=0x%llx; size=%lu)\n", i, alloc_para->p, alloc_para->size);
            goto alloc_fail_handle;
#endif
        }
        page_bitmap_set_alloced(page_bitmap + i);
        devmm_try_cond_resched(&stamp);
    }

    page_bitmap_set_first_page(page_bitmap);
    page_bitmap_set_alloc_len(page_bitmap, chunk_cnt);
    devmm_uvm_max_used_pages_update(heap, alloc_para->p, chunk_cnt, false);

    return 0;

alloc_fail_handle:
    for (j = 0; j < i; j++) {
        page_bitmap_clean(page_bitmap + j);
        devmm_try_cond_resched(&stamp);
    }

    return -EINVAL;
}

void devmm_destory_all_uvm_mem_by_proc(struct devmm_svm_process *svm_process)
{
    struct devmm_uvm_heap *uvm_heap = NULL;
    int ret;
    struct uvm_page_info page_info;
    u64 i;
    u32 stamp = (u32)ka_jiffies;

    uvm_heap = svm_process->uvm_heap;
    if (uvm_heap == NULL) {
        return;
    }

    ret = uvm_get_page_info_by_va(uvm_heap, DEVMM_UVM_MEM_START, &page_info);
    if (ret) {
#ifndef EMU_ST
        devmm_drv_err("Failed to get_page_info of va. (va=0x%llx)\n", DEVMM_UVM_MEM_START);
        return;
#endif
    }

    for (i = 0; i < uvm_heap->max_used_page; i++) {
        ka_task_down_write(page_info.page_rwlock + i);
        if (!page_bitmap_get_alloced(page_info.page_bitmap + i)) {
            ka_task_up_write(page_info.page_rwlock + i);
            continue;
        }

        ret = uvm_free_one_page(svm_process, &page_info, i, true);
        if (ret) {
            devmm_drv_err("Failed to free va. (va=0x%llx)\n", page_info.va_align + DEVMM_UVM_PAGE_SIZE * i);
        }
        ka_task_up_write(page_info.page_rwlock + i);
        devmm_try_cond_resched(&stamp);
    }
}