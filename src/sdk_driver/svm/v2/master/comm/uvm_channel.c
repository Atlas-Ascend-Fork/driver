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
#include "devmm_addr_mng.h"
#include "comm_kernel_interface.h"
#include "uvm_heap_mng.h"
#include "dms/dms_devdrv_manager_comm.h"
#include "uvm_channel.h"
#include "svm_msg_client.h"
#include "svm_kernel_msg.h"
#include "devmm_common.h"
#include "devmm_mem_alloc_interface.h"
#include "devmm_page_cache.h"
#include "svm_proc_mng.h"
#include "svm_dma.h"
#include "svm_heap_mng.h"
#include "svm_master_remote_map.h"
#include "svm_master_mem_share.h"
#include "svm_define.h"
#include "uvm_master_prefetch.h"
#include "svm_phy_addr_blk_mng.h"

static int uvm_chan_page_fault_process_h2d_dma_copy(struct devmm_chan_uvm_page_fault *fault_msg, uint64_t src_dma_addr)
{
    struct devdrv_dma_node dma_nodes = {0};
    int ret = 0;
    dma_nodes.src_addr = src_dma_addr;
    dma_nodes.dst_addr = fault_msg->dst_addr;
    dma_nodes.size = DEVMM_UVM_PAGE_SIZE;
    dma_nodes.direction = DEVDRV_DMA_HOST_TO_DEVICE;

#ifndef EMU_ST
    ret = devmm_dma_sync_link_copy(fault_msg->head.dev_id, fault_msg->head.process_id.vfid, &dma_nodes, 1);
#endif
    if (ret != 0) {
        devmm_drv_err("Dma sync link copy failed. (va=0x%llx; hostpid=%d; devid=%u; ret=%d)\n", fault_msg->va,
                      fault_msg->head.process_id.hostpid, fault_msg->head.dev_id, ret);
    }

    return ret;
}

static int uvm_chan_device_page_fault_host_data_sync(struct devmm_chan_uvm_page_fault *fault_msg, uint64_t pa)
{
    ka_page_t *pg = NULL;
    ka_device_t *dev = NULL;
    int ret;
    uint64_t src_dma_addr;

    dev = devmm_device_get_by_devid(fault_msg->head.dev_id);
    if (dev == NULL) {
        devmm_drv_err("Dev is NULL. (dev_id=%d)\n", fault_msg->head.dev_id);
        return -ENODEV;
    }

    pg = devmm_pa_to_page(pa);
#ifndef EMU_ST
    ka_mm_get_page(pg);
#endif
    src_dma_addr = hal_kernel_devdrv_dma_map_page(dev, pg, 0, DEVMM_UVM_PAGE_SIZE, KA_DMA_BIDIRECTIONAL);
    ret = ka_mm_dma_mapping_error(dev, src_dma_addr);
    if (ret != 0) {
        devmm_drv_err("Dma map page failed. (va=0x%llx; hostpid=%d; devid=%u; ret=%d)\n", fault_msg->va,
                      fault_msg->head.process_id.hostpid, fault_msg->head.dev_id, ret);
        goto unmap_dma;
    }

    ret = uvm_chan_page_fault_process_h2d_dma_copy(fault_msg, src_dma_addr);
    if (ret != 0) {
        devmm_drv_err("Dma failed. (va=0x%llx; hostpid=%d; devid=%u; ret=%d)\n", fault_msg->va,
                      fault_msg->head.process_id.hostpid, fault_msg->head.dev_id, ret);
    }

    hal_kernel_devdrv_dma_unmap_page(dev, src_dma_addr, DEVMM_UVM_PAGE_SIZE, KA_DMA_BIDIRECTIONAL);
unmap_dma:
    devmm_device_put_by_devid(fault_msg->head.dev_id);
#ifndef EMU_ST
    ka_mm_put_page(pg);
#endif
    pg = NULL;

    return ret;
}

static int devmm_addr_phy_to_dma(struct devmm_chan_uvm_page_fault *fault_msg, int physical_id, uint64_t *dst_dma)
{
    int ret;

    ret = devdrv_devmem_addr_d2h(fault_msg->head.dev_id, fault_msg->pa, dst_dma);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Devdrv_devmem_addr_d2h failed. (dst_dev_id=%u)\n", fault_msg->head.dev_id);
#endif
        return ret;
    }
    ret = devdrv_devmem_addr_bar_to_dma(physical_id, fault_msg->head.dev_id, *dst_dma, dst_dma);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Devdrv_devmem_addr_bar_to_dma failed. (src_dev_id=%u, dst_dev_id=%u)\n", fault_msg->head.dev_id,
                      physical_id);
#endif
        return ret;
    }
    return ret;
}

static int uvm_chan_device_page_fault_device_data_sync(struct devmm_svm_process *svm_proc,
                                                       struct uvm_page_mapping *page_map,
                                                       struct devmm_chan_uvm_page_fault *fault_msg)
{
    struct devmm_uvm_chan_device_data *device_msg = NULL;
    int ret = 0;
    uint64_t dst_dma;
    int physical_id;
    physical_id = devmm_get_phyid_devid_from_svm_process(svm_proc, page_map->devid);
    if (devdrv_get_connect_protocol(fault_msg->head.dev_id) == CONNECT_PROTOCOL_PCIE) {
        /* Convert the target device's physical pages to bar addresses, then DMA map them. */
        ret = devmm_addr_phy_to_dma(fault_msg, physical_id, &dst_dma);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("Devdrv_devmem_addr_phy_to_dma failed. (src_dev_id=%u, dst_dev_id=%u)\n",
                          fault_msg->head.dev_id, physical_id);
#endif
            return ret;
        }
    } else {
        dst_dma = fault_msg->pa;
    }

    device_msg = (struct devmm_uvm_chan_device_data *)ka_mm_kvmalloc(sizeof(struct devmm_uvm_chan_device_data),
                                                                     KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (device_msg == NULL) {
        devmm_drv_err("Kzalloc for struct devmm_uvm_chan_device_data failed.\n");
        return -ENOMEM;
    }

    device_msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    device_msg->head.process_id.vfid = svm_proc->process_id.vfid;
    device_msg->head.msg_id = DEVMM_CHAN_UVM_PAGE_FAULT_D2D_ID;
    device_msg->head.dev_id = physical_id;
    device_msg->va = fault_msg->va;
    device_msg->src_pa = page_map->pa_addr;
    device_msg->dst_pa = dst_dma;
    device_msg->dst_dev_id = fault_msg->head.dev_id;

    ret = devmm_chan_msg_send(device_msg, sizeof(*device_msg), 0);
    if (ret != 0) {
        devmm_drv_err("Device copy data p2p failed. (ret=%d; src_dev_id=%u; dst_dev_id=%u; va=0x%llx)\n", ret,
                      physical_id, fault_msg->head.dev_id, fault_msg->va);
    }

    ka_mm_kfree(device_msg);
    return ret;
}

static int uvm_chan_page_fault_device_process_normal(struct devmm_svm_process *svm_proc,
                                                     struct uvm_page_info *page_info,
                                                     struct devmm_chan_uvm_page_fault *fault_msg)
{
    int ret = 0;

    fault_msg->set_flag = PAGE_PTE_SET_NONE;
    if (!page_bitmap_get_alloced(page_info->page_bitmap)) {
        devmm_drv_err("Page is not allocated. (va=0x%llx; dev_id=%d)\n", fault_msg->va, fault_msg->head.dev_id);
        return -EINVAL;
    }

    if (page_is_no_mapped(page_info->page_bitmap)) {
        page_bitmap_set_device_mapped(page_info->page_bitmap);
        goto normal_exit;
    }

    /* If the card triggering the page fault already contains the corresponding physical memory, no further page fault
     * processing is required. */
    if (page_info->page_map->devid == fault_msg->head.logical_devid) {
        return UVM_ALREADY_SATISFIED_VAL;
    }

    if (page_is_mapped_host(page_info->page_bitmap, page_info->page_map)) {
        ret = uvm_chan_device_page_fault_host_data_sync(fault_msg, page_info->page_map->pa_addr);
        if (ret) {
            devmm_drv_err("Device page fault copy host data failed. (va=0x%llx; dev_id=%d)\n", fault_msg->va,
                          fault_msg->head.dev_id);
            return ret;
        }
        /* Copy successful */
        devmm_unmap_pages(svm_proc, fault_msg->va, 1);
        page_bitmap_unset_host_mapped(page_info->page_bitmap);
        page_bitmap_set_device_mapped(page_info->page_bitmap);
        goto normal_exit;
    }

    if (page_is_mapped_other_device(fault_msg->head.logical_devid, page_info->page_bitmap, page_info->page_map)) {
        ret = uvm_chan_device_page_fault_device_data_sync(svm_proc, page_info->page_map, fault_msg);
        if (ret) {
            devmm_drv_err("Device page fault copy other device data failed. (va=0x%llx; dev_id=%d)\n", fault_msg->va,
                          fault_msg->head.dev_id);
            return ret;
        }
        /* Copy successful */
        clear_device_present(page_info->dev_bitmap, page_info->page_map->devid);
        goto normal_exit;
    }

#ifndef EMU_ST
    devmm_drv_err("Page mapping info error. (va=0x%llx; dev_id=%d; page_mapping->dev_id=%d;"
                  "is_host_mapped=%d, is_device_mapped=%d)\n",
                  fault_msg->va, fault_msg->head.dev_id, page_info->page_map->devid,
                  page_bitmap_get_host_mapped(page_info->page_bitmap),
                  page_bitmap_get_device_mapped(page_info->page_bitmap));
#endif
    return -EINVAL;

normal_exit:
    set_page_mapping(page_info->page_map, fault_msg->pa, fault_msg->head.logical_devid);
    set_device_present(page_info->dev_bitmap, fault_msg->head.logical_devid);
    return ret;
}

static int uvm_chan_page_fault_device_process_read_mostly_write(struct devmm_svm_process *svm_proc,
                                                                struct uvm_page_info *page_info,
                                                                struct devmm_chan_uvm_page_fault *fault_msg)
{
    int ret = 0;
    uint16_t i, count;
    int *device_ids = NULL;

    device_ids = devmm_kzalloc_ex(sizeof(int) * BITMAP_BITS_COUNT, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (device_ids == NULL) {
        devmm_drv_err("Kmalloc for device_ids failed. (va=0x%llx)\n", fault_msg->va);
        return -ENOMEM;
    }

    count = get_present_devices(page_info->dev_bitmap, device_ids, BITMAP_BITS_COUNT);
    for (i = 0; i < count; i++) {
        if (device_ids[i] == fault_msg->head.logical_devid) {
            continue;
        }
        ret = uvm_free_device_page_by_id(svm_proc, page_info->dev_bitmap, device_ids[i], fault_msg->va);
        if (ret) {
            goto free_device_ids;
        }
    }
    devmm_unmap_pages(svm_proc, fault_msg->va, 1);
    page_bitmap_unset_host_mapped(page_info->page_bitmap);
    page_bitmap_unset_read_mostly(page_info->page_bitmap);
    page_bitmap_set_device_mapped(page_info->page_bitmap);
    set_page_mapping(page_info->page_map, fault_msg->pa, fault_msg->head.logical_devid);

    set_device_present(page_info->dev_bitmap, fault_msg->head.logical_devid);

free_device_ids:
    devmm_kfree_ex(device_ids);
    device_ids = NULL;

    return ret;
}

static int uvm_chan_page_fault_device_process_read_mostly_read(struct uvm_page_info *page_info,
                                                               struct devmm_chan_uvm_page_fault *fault_msg)
{
    page_bitmap_set_device_mapped(page_info->page_bitmap);
    set_device_present(page_info->dev_bitmap, fault_msg->head.logical_devid);
    return 0;
}

static int uvm_chan_page_fault_device_process_read_mostly(struct devmm_svm_process *svm_proc,
                                                          struct uvm_page_info *page_info,
                                                          struct devmm_chan_uvm_page_fault *fault_msg)
{
    int ret = 0;

    if (!page_bitmap_get_alloced(page_info->page_bitmap)) {
        devmm_drv_err("Page is not allocated. (va=0x%llx; dev_id=%d)\n", fault_msg->va, fault_msg->head.dev_id);
        return -EINVAL;
    }

    if (!page_is_mapped_host(page_info->page_bitmap, page_info->page_map)) {
        devmm_drv_err("Page mapping info error, read mostly page must have a mapping on host. (va=0x%llx; dev_id=%d)\n",
                      fault_msg->va, fault_msg->head.dev_id);
        return -EINVAL;
    }

    if (is_device_present(page_info->dev_bitmap, fault_msg->head.dev_id) &&
        (fault_msg->fault_flag & UVM_CHAN_MSG_DEVICE_FAULT_FLAG_READ)) {
        return UVM_ALREADY_SATISFIED_VAL;
    }

    if (!(fault_msg->fault_flag & UVM_CHAN_MSG_DEVICE_FAULT_MAPPED)) {
        ret = uvm_chan_device_page_fault_host_data_sync(fault_msg, page_info->page_map->pa_addr);
        if (ret) {
            devmm_drv_err("Device page fault copy host data failed. (va=0x%llx; dev_id=%d)\n", fault_msg->va,
                          fault_msg->head.dev_id);
            return -EFAULT;
        }
    }

    if (fault_msg->fault_flag & UVM_CHAN_MSG_DEVICE_FAULT_FLAG_WRITE) {
        fault_msg->set_flag = PAGE_PTE_SET_READWRITE;
        ret = uvm_chan_page_fault_device_process_read_mostly_write(svm_proc, page_info, fault_msg);
    } else {
        fault_msg->set_flag = PAGE_PTE_SET_READONLY;
        ret = uvm_chan_page_fault_device_process_read_mostly_read(page_info, fault_msg);
    }

    return ret;
}

#ifndef BUILD_UT
static
#endif
    int
    uvm_chan_page_fault_device_process(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *heap,
                                       struct devmm_chan_uvm_page_fault *fault_msg, uint32_t *ack_len)
{
    int ret = 0;
    struct uvm_page_info page_info;

    ret = uvm_get_page_info_by_va(heap, fault_msg->va, &page_info);
    if (ret != 0) {
        devmm_drv_err("uvm_get_page_info_by_va failed. (va=0x%llx;)\n", fault_msg->va);
        return ret;
    }

    /* Apply the lock directly to prevent simultaneous acquisition of the read lock by both prefetch and pagefault,
     * which would violate mutual exclusion. */
    ka_task_down_write(page_info.page_rwlock);

    if (page_bitmap_get_read_mostly(page_info.page_bitmap)) {
        ret = uvm_chan_page_fault_device_process_read_mostly(svm_proc, &page_info, fault_msg);
    } else {
        ret = uvm_chan_page_fault_device_process_normal(svm_proc, &page_info, fault_msg);
    }

    ka_task_up_write(page_info.page_rwlock);

    *ack_len = sizeof(struct devmm_chan_uvm_page_fault);
    if (ret) {
        fault_msg->set_flag = PAGE_PTE_SET_NONE;
    }
    return ret;
}

int devmm_uvm_chan_page_fault_d2h_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *msg,
                                          u32 *ack_len)
{
    struct devmm_chan_uvm_page_fault *fault_msg = (struct devmm_chan_uvm_page_fault *)msg;
    struct devmm_uvm_heap *heap_tmp = NULL;
    int ret = 0;

    heap_tmp = svm_proc->uvm_heap;
    if (heap_tmp == NULL) {
        devmm_drv_err("Va is not alloced. (hostpid=%d; va=0x%llx; devid=%u)\n", svm_proc->process_id.hostpid,
                      fault_msg->va, fault_msg->head.dev_id);
        return -EFAULT;
    }

    if (!devmm_va_is_in_uvm_range(fault_msg->va)) {
        devmm_drv_err("Va is not in uvm range. (va=0x%llx; hostpid=%d; devid=%d)\n", fault_msg->va,
                      fault_msg->head.process_id.hostpid, fault_msg->head.dev_id);
        return -EINVAL;
    }

    ret = uvm_chan_page_fault_device_process(svm_proc, heap_tmp, fault_msg, ack_len);
    if (ret < 0) {
        devmm_drv_err("Device page fault process in host failed. (va=0x%llx; hostpid=%d; devid=%d)\n", fault_msg->va,
                      fault_msg->head.process_id.hostpid, fault_msg->head.dev_id);
    }
    return ret;
}

int devmm_uvm_chan_prefetch_ack_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *msg,
                                        u32 *ack_len)
{
    struct devmm_chan_uvm_ack *prefetch_ack = (struct devmm_chan_uvm_ack *)msg;
    struct prefetching_task *task = NULL;
    struct uvm_page_info page_info = {0};
    uint32_t i, j;
    uint32_t fail_cnt = 0;
    uint64_t va;

    if (!prefetch_ack) {
#ifndef EMU_ST
        devmm_drv_err("prefetch_ack is NULL.\n");
        return -EFAULT;
#endif
    }

    task = uvm_hnode_find_from_prefetching_task_hashtable(prefetch_ack->id);
    if (!task) {
#ifndef EMU_ST
        devmm_drv_err("uvm_hnode_find_from_prefetching_task_hashtable failed. (id=%llu)\n", prefetch_ack->id);
        return -EFAULT;
#endif
    }

    for (i = 0; i < task->seg_cnt; i++) {
        for (va = task->va_segs[i].start_va, j = 0; j < task->va_segs[i].cnt; va += task->chunk_page_size, j++) {
            if (uvm_get_page_info_by_va(task->uvm_heap, va, &page_info) != 0) {
                fail_cnt++;
#ifndef EMU_ST
                devmm_drv_err("uvm_get_page_info_by_va failed at prefetch ack. (va=0x%llx)\n", va);
#endif
                continue;
            }
            page_bitmap_set_device_mapped(page_info.page_bitmap);
            set_device_present(page_info.dev_bitmap, task->dst_devid);
        }
    }

#ifndef EMU_ST
    if (fail_cnt > 0) {
        devmm_drv_err("prefetch ack encountered %u page info lookup failures. (id=%llu)\n", fail_cnt, prefetch_ack->id);
    }
#endif

    uvm_hnode_del_from_prefetching_task_hashtable(task);
    uvm_prefetch_task_cleanup(task);

    devmm_kfree_ex(task);
    return 0;
}

int devmm_uvm_chan_free_mem_d2h_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *msg,
                                        u32 *ack_len)
{
    struct devmm_chan_msg_head *free_msg = (struct devmm_chan_msg_head *)msg;
    devmm_drv_debug("Handle mem free since device process exit. (hostpid=%d;devid=%u)\n", svm_proc->process_id.hostpid,
                    free_msg->dev_id);
    devmm_destory_all_uvm_mem_by_proc(svm_proc);
    return 0;
}

static int devmm_uvm_chan_swap_d2h_process_dma_copy(struct devmm_chan_uvm_swap *swap_msg,
                                                    const struct devmm_addr_info *addr_info, uint32_t page_size)
{
    struct devdrv_dma_node dma_node = {0};
    int ret = 0;
    int page_num = 1;

    dma_node.src_addr = swap_msg->src_dma_addr;
    dma_node.dst_addr = addr_info->addr;
    dma_node.size = DEVMM_UVM_PAGE_SIZE;
    dma_node.direction = DEVDRV_DMA_DEVICE_TO_HOST;
#ifndef EMU_ST
    ret = devmm_dma_sync_link_copy(swap_msg->head.dev_id, swap_msg->head.process_id.vfid, &dma_node, page_num);
#endif
    if (ret != 0) {
        devmm_drv_err("Device process sync copy page fail. (hostpid=%d, devid=%d, va=0x%llx, ret=%d)\n",
                      swap_msg->head.process_id.hostpid, swap_msg->head.dev_id, swap_msg->va, ret);
    }
    return ret;
}

static int devmm_uvm_chan_swap_d2h_alloc_pages(struct devmm_svm_process *svm_proc, struct devmm_phy_addr_attr *attr,
                                               ka_page_t ***pages)
{
    int ret;

    *pages = (ka_page_t **)devmm_kzalloc_ex(sizeof(ka_page_t *), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (*pages == NULL)
        return -ENOMEM;

    devmm_phy_addr_attr_pack(svm_proc, DEVMM_HUGE_PAGE_TYPE, DEVMM_DDR_MEM, false, attr);
    ret = devmm_master_alloc_huge_pages(attr, *pages, 1);
    if (ret != 0) {
        devmm_kfree_ex(*pages);
        *pages = NULL;
    }
    return ret;
}

static int devmm_uvm_chan_swap_d2h_data_trans(struct devmm_chan_uvm_swap *swap_msg, ka_page_t *page)
{
    struct devmm_addr_info addr_info = {0};
    int ret;

    ret = devmm_dma_map_page(swap_msg->head.dev_id, page, DEVMM_UVM_PAGE_SIZE, NULL, &addr_info);
    if (ret != 0) {
        devmm_drv_err("Host page fault dma map page failed. (dev_id=%u; va=0x%llx)\n", swap_msg->head.dev_id,
                      swap_msg->va);
        return ret;
    }

    ret = devmm_uvm_chan_swap_d2h_process_dma_copy(swap_msg, &addr_info, DEVMM_UVM_PAGE_SIZE);
    if (ret != 0) {
        devmm_drv_err("devmm_uvm_chan_swap_d2h_process_dma_copy failed");
    }

    devmm_dma_unmap_page(NULL, &addr_info);

    return ret;
}

int devmm_uvm_chan_swap_d2h_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *msg,
                                    u32 *ack_len)
{
    struct devmm_chan_uvm_swap *swap_msg = (struct devmm_chan_uvm_swap *)msg;
    struct devmm_phy_addr_attr attr = {0};
    struct uvm_page_info page_info = {0};
    ka_page_t **pages = NULL;
    int ret = 0;
    ka_vm_area_struct_t *vma = NULL;
    uint64_t pa;

    ret = devmm_uvm_chan_swap_d2h_alloc_pages(svm_proc, &attr, &pages);
    if (ret != 0) {
        return ret;
    }

    ret = uvm_get_page_info_by_va(svm_proc->uvm_heap, swap_msg->va, &page_info);
    if (ret != 0) {
        goto free_pa;
    }

    ka_task_down_write(page_info.page_rwlock);
    vma = devmm_find_vma(svm_proc, swap_msg->va);
    if (vma == NULL) {
        devmm_drv_err("Can not find vma. (vaddr=0x%llx; hostpid=%d; devid=%d; vfid=%d)\n", swap_msg->va,
                      svm_proc->process_id.hostpid, svm_proc->process_id.devid, svm_proc->process_id.vfid);
        ret = -EINVAL;
        goto out_unlock;
    }

    if (!page_bitmap_get_alloced(page_info.page_bitmap)) {
#ifndef EMU_ST
        ret = -EINVAL;
        goto out_unlock;
#endif
    }

    ret = devmm_va_to_pa(vma, swap_msg->va, &pa);
    if (ret == 0) {
        devmm_free_pages(&attr, pages, 1);
        goto out_unlock;
    }

    ret = devmm_uvm_chan_swap_d2h_data_trans(swap_msg, pages[0]);
    if (ret != 0) {
        devmm_drv_err("devmm_uvm_chan_swap_d2h_data_trans failed. (va=0x%llx)\n", swap_msg->va);
        goto out_unlock;
    }

    ret = devmm_pages_remap(svm_proc, swap_msg->va, 1, pages, 0);
    if (ret != 0) {
        devmm_drv_err("Can not insert_pages_vma . (va=0x%llx; ret=%d)\n", swap_msg->va, ret);
        goto out_unlock;
    }

    clear_device_present(page_info.dev_bitmap, swap_msg->head.logical_devid);
    page_bitmap_unset_device_mapped(page_info.page_bitmap);
    uvm_set_page_mapping(page_info.page_map, pages[0], UVM_HOST_ID);
    uvm_set_bitmap_mapped(page_info.page_bitmap, UVM_HOST_ID);

out_unlock:
    ka_task_up_write(page_info.page_rwlock);
free_pa:
    if (ret != 0)
        devmm_free_pages(&attr, pages, 1);
    devmm_kfree_ex(pages);

    return ret;
}

int devmm_uvm_chan_release_d2h_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *msg,
                                       u32 *ack_len)
{
    struct devmm_chan_uvm_release *release_msg = (struct devmm_chan_uvm_release *)msg;
    struct uvm_page_info page_info = {0};
    int ret = 0;
    int count;

    ret = uvm_get_page_info_by_va(svm_proc->uvm_heap, release_msg->va, &page_info);
    if (ret != 0) {
        devmm_drv_err("Fail to get page info.\n");
        return ret;
    }
    ka_task_down_write(page_info.page_rwlock);
    clear_device_present(page_info.dev_bitmap, release_msg->head.logical_devid);
    count = get_present_devices_count(page_info.dev_bitmap);
    if (count == 0) {
        page_bitmap_unset_device_mapped(page_info.page_bitmap);
    }
    ka_task_up_write(page_info.page_rwlock);
    return ret;
}
