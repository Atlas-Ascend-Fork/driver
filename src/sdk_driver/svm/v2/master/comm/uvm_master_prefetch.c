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
#include "uvm_master_prefetch.h"

#include "devmm_chan_handlers.h"
#include "devmm_common.h"
#include "devmm_proc_mem_copy.h"
#include "svm_dev_res_mng.h"
#include "svm_gfp.h"
#include "svm_kernel_msg.h"
#include "svm_msg_client.h"
#include "svm_phy_addr_blk_mng.h"
#include "svm_proc_mng.h"
#include "uvm_advise.h"
#include "uvm_heap_mng.h"
#include "uvm_master_common.h"

#define UVM_HLIST_TABLE_BIT 13
#define UVM_HLIST_TABLE_MASK ((0x1UL << UVM_HLIST_TABLE_BIT) - 1)

#define UVM_PREFETCH_ENABLE_DEBUG_LOG

#define MAX_PREFETCH_COUNT 4096
#define MAX_PREFETCH_LOCS 64

#define UVM_PREFETCH_ADDR_LIST_GROWTH_FACTOR 2

static KA_DEFINE_HASHTABLE(prefetching_task_hashtable, UVM_HLIST_TABLE_BIT);

ka_task_spinlock_t prefetching_task_lock;
KA_TASK_DEFINE_SPINLOCK(prefetching_task_lock);

static ka_atomic64_t uvm_uuid_counter = KA_BASE_ATOMIC64_INIT(0);

static uint64_t uvm_get_uuid(void)
{
    return (uint64_t)ka_base_atomic64_inc_return(&uvm_uuid_counter);
}

void uvm_hnode_add_to_prefetching_task_hashtable(struct prefetching_task *task)
{
    uint32_t key;
    key = ka_base_jhash(&task->id, sizeof(task->id), 0) & UVM_HLIST_TABLE_MASK;
    ka_task_spin_lock(&prefetching_task_lock);
    ka_hash_add(prefetching_task_hashtable, &task->hnode, key);
    ka_task_spin_unlock(&prefetching_task_lock);
}

struct prefetching_task *uvm_hnode_find_from_prefetching_task_hashtable(uint64_t id)
{
    struct prefetching_task *task = NULL;
    uint32_t key;

    key = ka_base_jhash(&id, sizeof(id), 0) & UVM_HLIST_TABLE_MASK;

    ka_task_spin_lock(&prefetching_task_lock);
    ka_hash_for_each_possible(prefetching_task_hashtable, task, hnode, key)
    {
        if (task->id == id) {
            ka_task_spin_unlock(&prefetching_task_lock);
            return task;
        }
    }
    ka_task_spin_unlock(&prefetching_task_lock);
    return NULL;
}

void uvm_hnode_del_from_prefetching_task_hashtable(struct prefetching_task *task)
{
    ka_task_spin_lock(&prefetching_task_lock);
    ka_hash_del(&task->hnode);
    ka_task_spin_unlock(&prefetching_task_lock);
}

bool uvm_prefetch_task_try_append_addr(uint64_t id, const struct devmm_addr_info *addr_info, uint32_t cnt)
{
    struct prefetching_task *task = NULL;
    struct devmm_addr_info *new_arr = NULL;
    uint32_t key;
    uint32_t new_cap;
    bool appended = false;

    key = ka_base_jhash(&id, sizeof(id), 0) & UVM_HLIST_TABLE_MASK;
    ka_task_spin_lock(&prefetching_task_lock);
    ka_hash_for_each_possible(prefetching_task_hashtable, task, hnode, key)
    {
        if (task->id != id) {
            continue;
        }

        if (task->addr_cnt + cnt > task->addr_cap) {
            new_cap = task->addr_cap ? task->addr_cap * UVM_PREFETCH_ADDR_LIST_GROWTH_FACTOR : MAX_CONT_NUM;
            new_cap = (new_cap < task->addr_cnt + cnt) ? (task->addr_cnt + cnt) : new_cap;
            new_arr = (struct devmm_addr_info *)devmm_kmalloc_ex(new_cap * sizeof(struct devmm_addr_info),
                                                                 KA_GFP_ATOMIC);
            if (new_arr == NULL) {
                break;
            }
            if (task->addr_cnt > 0) {
                if (memcpy_s(new_arr, new_cap * sizeof(struct devmm_addr_info), task->addr_infos,
                             task->addr_cnt * sizeof(struct devmm_addr_info)) != EOK) {
                    devmm_kfree_ex(new_arr);
                    break;
                }
            }
            if (task->addr_infos != NULL) {
                devmm_kfree_ex(task->addr_infos);
            }
            task->addr_infos = new_arr;
            task->addr_cap = new_cap;
        }

        if (memcpy_s(task->addr_infos + task->addr_cnt,
                     (task->addr_cap - task->addr_cnt) * sizeof(struct devmm_addr_info), addr_info,
                     cnt * sizeof(struct devmm_addr_info)) != EOK) {
            break;
        }
        task->addr_cnt += cnt;
        appended = true;
        break;
    }
    ka_task_spin_unlock(&prefetching_task_lock);

    return appended;
}

bool uvm_prefetch_task_try_append_va_seg(struct prefetching_task *task, uint64_t start_va, uint32_t cnt)
{
    struct prefetching_va_seg *new_arr = NULL;
    uint32_t new_cap;
    bool appended = false;

    if ((task == NULL) || (cnt == 0)) {
        return false;
    }

    ka_task_spin_lock(&prefetching_task_lock);
    if (task->seg_cnt >= task->seg_cap) {
        new_cap = task->seg_cap ? task->seg_cap * UVM_PREFETCH_ADDR_LIST_GROWTH_FACTOR : MAX_CONT_NUM;
        new_arr = (struct prefetching_va_seg *)devmm_kmalloc_ex(new_cap * sizeof(struct prefetching_va_seg),
                                                                KA_GFP_ATOMIC);
        if (new_arr == NULL) {
            ka_task_spin_unlock(&prefetching_task_lock);
            return false;
        }
        if (task->seg_cnt > 0) {
            if (memcpy_s(new_arr, new_cap * sizeof(struct prefetching_va_seg), task->va_segs,
                         task->seg_cnt * sizeof(struct prefetching_va_seg)) != EOK) {
                devmm_kfree_ex(new_arr);
                ka_task_spin_unlock(&prefetching_task_lock);
                return false;
            }
        }
        devmm_kfree_ex(task->va_segs);
        task->va_segs = new_arr;
        task->seg_cap = new_cap;
    }

    task->va_segs[task->seg_cnt].start_va = start_va;
    task->va_segs[task->seg_cnt].cnt = cnt;
    task->seg_cnt++;
    appended = true;
    ka_task_spin_unlock(&prefetching_task_lock);

    return appended;
}

void uvm_prefetch_task_cleanup(struct prefetching_task *task)
{
    uint32_t i;

    if (task == NULL) {
        return;
    }

    for (i = 0; i < task->addr_cnt; i++) {
        devmm_dma_unmap_page(NULL, &task->addr_infos[i]);
    }
    devmm_kfree_ex(task->addr_infos);
    task->addr_infos = NULL;
    task->addr_cnt = 0;
    task->addr_cap = 0;

    devmm_kfree_ex(task->va_segs);
    task->va_segs = NULL;
    task->seg_cnt = 0;
    task->seg_cap = 0;
}

static int uvm_hnode_wait_for_removal(uint64_t id)
{
    struct prefetching_task *task = NULL;
    uint32_t key;
    unsigned long stamp, timeout = ka_system_msecs_to_jiffies(DEVMM_UVM_TIMEOUT);
    bool is_find;

    key = ka_base_jhash(&id, sizeof(id), 0) & UVM_HLIST_TABLE_MASK;
    stamp = ka_jiffies;

    while (!ka_system_time_after(ka_jiffies, stamp + timeout)) {
        is_find = false;

        ka_task_spin_lock(&prefetching_task_lock);
        ka_hash_for_each_possible(prefetching_task_hashtable, task, hnode, key)
        {
            if (task->id == id) {
                is_find = true;
                break;
            }
        }
        ka_task_spin_unlock(&prefetching_task_lock);

        if (!is_find) {
            return 0;
        }

        ka_system_usleep_range(1000, 2000); /* 1000us to 2000us */
    }

    /* Exiting the loop indicates that the timeout has occurred, and the timeout cleanup logic is executed. */
    task = uvm_hnode_find_from_prefetching_task_hashtable(id);
    if (!task) {
        devmm_drv_err("uvm_hnode_find_from_prefetching_task_hashtable failed. (id=%llu)\n", id);
        return -EFAULT;
    }

    uvm_hnode_del_from_prefetching_task_hashtable(task);
    uvm_prefetch_task_cleanup(task);
    devmm_kfree_ex(task);

    return -ETIMEDOUT;
}

enum uvm_prefetch_lock_type {
    UVM_PREFETCH_LOCK_READ,
    UVM_PREFETCH_LOCK_WRITE
};

struct uvm_prefetch_page_lock {
    ka_rw_semaphore_t *rwlock;
    enum uvm_prefetch_lock_type type;
};

static void uvm_prefetch_acquire_page_read_lock(struct uvm_page_info *page_info,
                                                struct uvm_prefetch_page_lock *page_lock)
{
    ka_task_down_read(page_info->page_rwlock);
    page_lock->rwlock = page_info->page_rwlock;
    page_lock->type = UVM_PREFETCH_LOCK_READ;
}

static void uvm_prefetch_upgrade_page_lock(struct uvm_page_info *page_info, struct uvm_prefetch_page_lock *page_lock)
{
    if (!page_bitmap_get_host_mapped(page_info->page_bitmap) &&
        !page_bitmap_get_device_mapped(page_info->page_bitmap)) {
#ifndef EMU_ST
        ka_task_up_read(page_lock->rwlock);
        ka_task_down_write(page_lock->rwlock);
        page_lock->type = UVM_PREFETCH_LOCK_WRITE;
#endif
        return;
    }

    while (!page_bitmap_get_read_mostly(page_info->page_bitmap)) {
        ka_task_up_read(page_lock->rwlock);
        ka_task_down_write(page_lock->rwlock);
        page_lock->type = UVM_PREFETCH_LOCK_WRITE;
        if (!page_bitmap_get_read_mostly(page_info->page_bitmap)) {
            return;
        }

        ka_task_up_write(page_lock->rwlock);
        ka_task_down_read(page_lock->rwlock);
        page_lock->type = UVM_PREFETCH_LOCK_READ;
    }
}

static void uvm_prefetch_release_page_lock(struct uvm_prefetch_page_lock *page_lock)
{
    if (page_lock->type == UVM_PREFETCH_LOCK_WRITE) {
        ka_task_up_write(page_lock->rwlock);
    } else {
        ka_task_up_read(page_lock->rwlock);
    }
}

/* Applicable only when the location.type does not have the MEM_HOST_NUMA_SIDE type. */
static bool is_uvm_location_same(struct drv_uvm_location location1, struct drv_uvm_location location2)
{
    if (location1.type == DRV_UVM_LOCATION_TYPE_INVALID || location2.type == DRV_UVM_LOCATION_TYPE_INVALID) {
        return false;
    }
    if (location1.type != location2.type) {
        return false;
    }
    if (location1.type == DRV_UVM_LOCATION_TYPE_HOST) {
        return true;
    }
    if (location1.id == location2.id) {
        return true;
    }
    return false;
}

static int uvm_prefetch_h2d_common_data_trans_dma_map(struct devmm_svm_process *svm_proc,
                                                      struct devmm_uvm_heap *uvm_heap,
                                                      struct uvm_prefetch_batch_info *batch_info, uint64_t *host_pa,
                                                      struct devmm_addr_info *addr_info)
{
    uint64_t pfn = 0;
    ka_page_t *pg = NULL;
    int i = 0, ret = 0;

    for (i = 0; i < batch_info->cnt; i++) {
        pfn = KA_MM_PFN_DOWN(host_pa[i]);
        pg = ka_mm_pfn_to_page(pfn);
        ret = devmm_dma_map_page(devmm_get_phyid_devid_from_svm_process(svm_proc, batch_info->dst_location.id), pg,
                                 DEVMM_UVM_PAGE_SIZE, NULL, &addr_info[i]);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("devmm_dma_map_page failed. (dev_id=%u; va=0x%llx)\n", batch_info->dst_location.id,
                          batch_info->va + i * DEVMM_UVM_PAGE_SIZE);
            for (i--; i >= 0; i--) {
                devmm_dma_unmap_page(NULL, &addr_info[i]);
            }
            return ret;
#endif
        }
    }

    return ret;
}

static int uvm_prefetch_h2d_common_data_trans(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                              struct uvm_prefetch_batch_info *batch_info, uint64_t *host_pa,
                                              uint64_t *dev_pa, uint64_t msg_cnt)
{
    struct devmm_chan_uvm_prefetch *prefetch_msg = NULL;
    struct devmm_addr_info addr_info[MAX_CONT_NUM] = {0};
    int i = 0, ret = 0;

    prefetch_msg = (struct devmm_chan_uvm_prefetch *)devmm_kzalloc_ex(sizeof(struct devmm_chan_uvm_prefetch),
                                                                      KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (prefetch_msg == NULL) {
        devmm_drv_err("Kzalloc error. (va=0x%lx)\n", batch_info->va);
        return -ENOMEM;
    }

    ret = uvm_prefetch_h2d_common_data_trans_dma_map(svm_proc, uvm_heap, batch_info, host_pa, addr_info);
    if (ret != 0) {
        ret = -EIO;
        goto msg_send_err;
    }

    prefetch_msg->head.dev_id = devmm_get_phyid_devid_from_svm_process(svm_proc, batch_info->dst_location.id);
    prefetch_msg->head.msg_id = batch_info->is_readmostly ? DEVMM_CHAN_UVM_PREFETCH_H2D_READMOSTLY_ID :
                                                            DEVMM_CHAN_UVM_PREFETCH_H2D_ID;
    prefetch_msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    prefetch_msg->head.process_id.vfid = svm_proc->process_id.vfid;
    prefetch_msg->is_read_mostly = batch_info->is_readmostly;
    prefetch_msg->va = batch_info->va;
    prefetch_msg->cnt = batch_info->cnt;
    prefetch_msg->msg_cnt = msg_cnt;
    prefetch_msg->id = batch_info->is_readmostly ? batch_info->id : DEVMM_UVM_SYNC_ID;
    for (i = 0; i < batch_info->cnt; i++) {
        prefetch_msg->src_addr[i] = addr_info[i].addr;
    }

    if (batch_info->is_readmostly) {
        ret = devmm_chan_msg_send(prefetch_msg, sizeof(*prefetch_msg), 0);
        if (ret != 0) {
            devmm_drv_err("Device copy data process failed. (ret=%d; dev_id=%u; va=0x%llx)\n", ret,
                          batch_info->dst_location.id, batch_info->va);
            goto msg_send_err;
        }

        if (uvm_prefetch_task_try_append_addr(batch_info->id, addr_info, batch_info->cnt)) {
            devmm_kfree_ex(prefetch_msg);
            return ret;
        }
#ifndef EMU_ST
        devmm_drv_err("prefetching task append failed, unmap early. (id=%llu; va=0x%llx)\n", batch_info->id,
                      batch_info->va);
#endif
    } else {
        ret = devmm_chan_msg_send(prefetch_msg, sizeof(*prefetch_msg), sizeof(*prefetch_msg));
        if (ret != 0) {
            devmm_drv_err("Device copy data process failed. (ret=%d; dev_id=%u; va=0x%llx)\n", ret,
                          batch_info->dst_location.id, batch_info->va);
            goto msg_send_err;
        }
        memcpy_s(dev_pa, MAX_CONT_NUM * sizeof(uint64_t), prefetch_msg->dst_addr, batch_info->cnt * sizeof(uint64_t));
    }

msg_send_err:
    for (i = 0; i < batch_info->cnt; i++) {
        devmm_dma_unmap_page(NULL, &addr_info[i]);
    }
    devmm_kfree_ex(prefetch_msg);

    return ret;
}

static int uvm_prefetch_h2d_common(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                   struct uvm_prefetch_batch_info *batch_info, uint64_t msg_cnt,
                                   struct prefetching_task *prefetch_task)
{
    struct uvm_page_info page_info = {0};
    uint64_t cur_va;
    uint64_t src_pa[MAX_CONT_NUM] = {0};
    uint64_t dst_pa[MAX_CONT_NUM] = {0};
    int i, ret = 0;
    cur_va = batch_info->va;
    for (i = 0; i < batch_info->cnt; cur_va += uvm_heap->chunk_page_size, i++) {
        ret = uvm_get_page_info_by_va(uvm_heap, cur_va, &page_info);
        if (ret) {
#ifndef EMU_ST
            devmm_drv_err("uvm_get_page_info_by_va failed. (va=0x%llx)\n", cur_va);
            return ret;
#endif
        }
        src_pa[i] = page_info.page_map->pa_addr;
    }

    ret = uvm_prefetch_h2d_common_data_trans(svm_proc, uvm_heap, batch_info, src_pa, dst_pa, msg_cnt);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("uvm_prefetch_h2d_common_data_trans failed.\n");
        return ret;
#endif
    }

    cur_va = batch_info->va;
    for (i = 0; i < batch_info->cnt; cur_va += uvm_heap->chunk_page_size, i++) {
        ret = uvm_get_page_info_by_va(uvm_heap, cur_va, &page_info);
        if (ret) {
#ifndef EMU_ST
            devmm_drv_err("uvm_get_page_info_by_va failed. (va=0x%llx)\n", cur_va);
            return ret;
#endif
        }
        if (!batch_info->is_readmostly) {
            devmm_unmap_pages(svm_proc, cur_va, 1);
            page_bitmap_unset_host_mapped(page_info.page_bitmap);
            set_page_mapping(page_info.page_map, dst_pa[i], batch_info->dst_location.id);
            page_bitmap_set_device_mapped(page_info.page_bitmap);
            set_device_present(page_info.dev_bitmap, batch_info->dst_location.id);
        }
    }

    if (batch_info->is_readmostly && prefetch_task != NULL) {
        (void)uvm_prefetch_task_try_append_va_seg(prefetch_task, batch_info->va, batch_info->cnt);
    }
    return ret;
}

static int uvm_prefetch_h2d_readmostly(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                       struct uvm_prefetch_batch_info *batch_info,
                                       ka_atomic_t *h2d_readmoslty_msg_send_cnt)
{
    uint64_t msg_cnt = 0;
    int ret;
    bool task_created = false;
    struct prefetching_task *prefetch_task = NULL;

    prefetch_task = uvm_hnode_find_from_prefetching_task_hashtable(batch_info->id);
    if (prefetch_task == NULL) {
        prefetch_task = (struct prefetching_task *)devmm_kzalloc_ex(sizeof(*prefetch_task),
                                                                    KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (!prefetch_task) {
#ifndef EMU_ST
            devmm_drv_err("Kzalloc for prefetching_task failed.\n");
            return -ENOMEM;
#endif
        }
        prefetch_task->id = batch_info->id;
        uvm_hnode_add_to_prefetching_task_hashtable(prefetch_task);
        task_created = true;
    }
    prefetch_task->uvm_heap = uvm_heap;
    prefetch_task->chunk_page_size = uvm_heap->chunk_page_size;
    prefetch_task->dst_devid = batch_info->dst_location.id;
    ka_base_atomic_inc(h2d_readmoslty_msg_send_cnt);

    ret = uvm_prefetch_h2d_common(svm_proc, uvm_heap, batch_info, msg_cnt, prefetch_task);
    if (ret != 0) {
#ifndef EMU_ST
        if (task_created) {
            (void)uvm_hnode_wait_for_removal(prefetch_task->id);
        }
        devmm_drv_err("uvm_prefetch_h2d_common failed. (va=0x%llx, src_dev_id=%u, dst_dev_id=%u)\n", batch_info->va,
                      batch_info->src_location.id, batch_info->dst_location.id);
#endif
    }
    return ret;
}

static int uvm_prefetch_h2h_batch_pages(struct devmm_uvm_heap *uvm_heap, struct uvm_prefetch_batch_info *batch_info,
                                        ka_page_t **dst_pages)
{
    struct uvm_page_info page_info = {0};
    ka_page_t *src_page = NULL;
    phys_addr_t src_pa;
    uint64_t src_pfn;
    int i, ret;

    for (i = 0; i < batch_info->cnt; i++) {
        ret = uvm_get_page_info_by_va(uvm_heap, batch_info->va + i * DEVMM_UVM_PAGE_SIZE, &page_info);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("uvm_get_page_info_by_va failed. (va=0x%llx)\n", batch_info->va + i * DEVMM_UVM_PAGE_SIZE);
            return ret;
#endif
        }

        src_pa = page_info.page_map->pa_addr;
#ifndef EMU_ST
        src_pfn = KA_MM_PFN_DOWN(src_pa);
        src_page = ka_mm_pfn_to_page(src_pfn);
        ret = memcpy_s(ka_mm_page_address(dst_pages[i]), DEVMM_UVM_PAGE_SIZE, ka_mm_page_address(src_page),
                       DEVMM_UVM_PAGE_SIZE);
        if (ret != 0) {
            devmm_drv_err("memcpy_s failed. (va=0x%llx)\n", batch_info->va + i * DEVMM_UVM_PAGE_SIZE);
            return ret;
        }
#endif
    }

    return 0;
}

static int uvm_remap_and_update_mapping(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                        struct uvm_prefetch_batch_info *batch_info, ka_page_t **dst_pages)
{
    struct uvm_page_info page_info = {0};
    uint32_t prot;
    int i, ret;

    devmm_unmap_pages(svm_proc, batch_info->va, batch_info->cnt);

    prot = batch_info->is_readmostly ? DEVMM_PAGE_READONLY_FLG : 0;
    ret = devmm_pages_remap(svm_proc, batch_info->va, batch_info->cnt, dst_pages, prot);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Insert pages vma error. (ret=%d, va=0x%llx, page_num=%llu)\n", ret, batch_info->va,
                      batch_info->cnt);
#endif
        return ret;
    }

    for (i = 0; i < batch_info->cnt; i++) {
        ret = uvm_get_page_info_by_va(uvm_heap, batch_info->va + i * DEVMM_UVM_PAGE_SIZE, &page_info);
        if (ret) {
#ifndef EMU_ST
            devmm_drv_err("uvm_get_page_info_by_va failed. (va=0x%llx)\n", batch_info->va);
#endif
            return ret;
        }
        uvm_set_page_mapping(page_info.page_map, dst_pages[i], UVM_HOST_ID);
    }

    return 0;
}

static int uvm_prefetch_h2h(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                            struct uvm_prefetch_batch_info *batch_info)
{
    struct devmm_phy_addr_attr attr = {0};
    ka_page_t **dst_pages = NULL;
    int ret = 0;

#ifndef EMU_ST
    if (batch_info->dst_location.type == DRV_UVM_LOCATION_TYPE_HOST_NUMA &&
        (batch_info->dst_location.id <= 0 || !node_online(batch_info->dst_location.id - 1))) {
        devmm_drv_err("Invalid host numa id. (id=%d)\n", batch_info->dst_location.id);
        return -EINVAL;
    }
#endif

    dst_pages = (ka_page_t **)devmm_kzalloc_ex(sizeof(ka_page_t *) * batch_info->cnt, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (dst_pages == NULL) {
#ifndef EMU_ST
        devmm_drv_err("devmm_kzalloc_ex for dst_pages failed. (va=0x%llx, page_num=%llu)\n", batch_info->va,
                      batch_info->cnt);
        return -ENOMEM;
#endif
    }

    devmm_phy_addr_attr_pack(svm_proc, DEVMM_HUGE_PAGE_TYPE, DEVMM_DDR_MEM, false, &attr);

    attr.numa_id = batch_info->dst_location.id;
    ret = devmm_master_alloc_huge_pages(&attr, dst_pages, batch_info->cnt);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("devmm_master_alloc_huge_pages failed. (ret=%d; pg_num=%llu)\n", ret, batch_info->cnt);
        goto free_tmp_pages;
#endif
    }

    ret = uvm_prefetch_h2h_batch_pages(uvm_heap, batch_info, dst_pages);
    if (ret != 0) {
        goto free_pa;
    }

    ret = uvm_remap_and_update_mapping(svm_proc, uvm_heap, batch_info, dst_pages);
    if (ret != 0) {
        goto free_pa;
    }

    goto free_tmp_pages;

free_pa:
    devmm_free_pages(&attr, dst_pages, batch_info->cnt);
free_tmp_pages:
    devmm_kvfree(dst_pages);
    dst_pages = NULL;

    return ret;
}

static int uvm_prefetch_d2h_get_page_info(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                          struct uvm_prefetch_batch_info *batch_info, uint64_t *dev_pa)
{
    uint64_t va = batch_info->va;
    struct uvm_page_info page_info = {0};
    int i = 0, ret = 0;

    for (i = 0; i < batch_info->cnt; va += uvm_heap->chunk_page_size, i++) {
        ret = uvm_get_page_info_by_va(uvm_heap, va, &page_info);
        if (ret != 0) {
            devmm_drv_err("Uvm_get_page_info_by_va failed. (va=0x%llx)\n", va);
            return -EINVAL;
        }

        if (page_info.page_map->devid == UVM_HOST_ID) {
            devmm_drv_info("Va is already mapped in host. (va=0x%llx)\n", va);
            return ret;
        }

        if (!(page_bitmap_get_device_mapped(page_info.page_bitmap) &&
              (page_info.page_map->devid == batch_info->src_location.id))) {
            devmm_drv_err("Va is not mapped in device. (va=0x%llx, src_dev_id=%d, real_dev=%d)\n", va,
                          batch_info->src_location.id, page_info.page_map->devid);
            return -EFAULT;
        }

        dev_pa[i] = page_info.page_map->pa_addr;
    }

    return ret;
}

static int uvm_prefetch_d2h_alloc_pages(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                        struct uvm_prefetch_batch_info *batch_info, ka_page_t ***pages,
                                        struct devmm_phy_addr_attr *attr)
{
    uint64_t va = batch_info->va;
    int ret = 0;

    *pages = (ka_page_t **)devmm_kzalloc_ex(sizeof(ka_page_t *) * batch_info->cnt, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (*pages == NULL) {
        devmm_drv_err("Devmm_kzalloc_ex for pages failed. (va=0x%llx, page_num=%hu)\n", va, batch_info->cnt);
        return -ENOMEM;
    }

    devmm_phy_addr_attr_pack(svm_proc, DEVMM_HUGE_PAGE_TYPE, DEVMM_DDR_MEM, false, attr);

    if (batch_info->dst_location.type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) {
        attr->numa_id = batch_info->dst_location.id;
        ret = devmm_master_alloc_huge_pages(attr, *pages, batch_info->cnt);
        if (ret != 0) {
            devmm_drv_err("Devmm_master_alloc_huge_pages failed. (va=0x%llx, page_num=%hu)\n", va, batch_info->cnt);
        }
    } else if (batch_info->dst_location.type == DRV_UVM_LOCATION_TYPE_HOST) {
        ret = devmm_master_alloc_huge_pages(attr, *pages, batch_info->cnt);
        if (ret != 0) {
            devmm_drv_err("Devmm_alloc_pages failed. (va=0x%llx, page_num=%hu)\n", va, batch_info->cnt);
        }
    } else {
        devmm_drv_err("Unsupported destination location type: %u.\n", (uint32_t)batch_info->dst_location.type);
        ret = -EINVAL;
    }

    return ret;
}

static int uvm_prefetch_h2d(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                            struct uvm_prefetch_batch_info *batch_info, struct uvm_prefetch_type *type,
                            ka_atomic_t *h2d_readmoslty_msg_send_cnt)
{
    uint64_t msg_cnt = 0;
    int ret = 0;

    if (batch_info->is_readmostly) {
        type->h2d_readmostly = true;
        ret = uvm_prefetch_h2d_readmostly(svm_proc, uvm_heap, batch_info, h2d_readmoslty_msg_send_cnt);
    } else {
        ret = uvm_prefetch_h2d_common(svm_proc, uvm_heap, batch_info, msg_cnt, NULL);
    }

    if (ret != 0) {
        devmm_drv_err("uvm_prefetch_h2d failed. (va=0x%llx, cnt=%u, dst_location.id=%u, is_readmostly=%d)\n",
                      batch_info->va, batch_info->cnt, batch_info->dst_location.id, batch_info->is_readmostly);
    }
    return ret;
}

static int uvm_prefetch_d2h_sync(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                 struct uvm_prefetch_batch_info *batch_info, uint64_t *dev_pa, struct page **pages)
{
    struct devmm_chan_uvm_prefetch *prefetch_msg = NULL;
    struct devmm_addr_info dma_addr[MAX_CONT_NUM] = {0};
    uint32_t dev_phyid = 0;
    int ret = 0, i = 0, j = 0;

    dev_phyid = devmm_get_phyid_devid_from_svm_process(svm_proc, batch_info->src_location.id);

    prefetch_msg = (struct devmm_chan_uvm_prefetch *)devmm_kzalloc_ex(sizeof(struct devmm_chan_uvm_prefetch),
                                                                      KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (prefetch_msg == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Devmm_kvalloc for struct uvm_chan_prefetch failed. (va=0x%llx)\n", batch_info->va);
        return -ENOMEM;
#endif
    }

    for (i = 0; i < batch_info->cnt; i++) {
        ret = devmm_dma_map_page(dev_phyid, pages[i], DEVMM_UVM_PAGE_SIZE, NULL, &dma_addr[i]);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("devmm_dma_map_page failed. (dev_phyid=%u, current_va=0x%llx)\n", dev_phyid,
                          batch_info->va + i * DEVMM_UVM_PAGE_SIZE);
            ret = -EIO;
            goto dma_unmap_page;
#endif
        }
    }

    prefetch_msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    prefetch_msg->head.process_id.vfid = svm_proc->process_id.vfid;
    prefetch_msg->head.msg_id = DEVMM_CHAN_UVM_PREFETCH_D2H_ID;
    prefetch_msg->head.dev_id = dev_phyid;
    prefetch_msg->va = batch_info->va;
    prefetch_msg->cnt = batch_info->cnt;
    prefetch_msg->is_read_mostly = batch_info->is_readmostly;
    for (j = 0; j < batch_info->cnt; j++) {
        prefetch_msg->src_addr[j] = dev_pa[j];
        prefetch_msg->dst_addr[j] = dma_addr[j].addr;
    }

    ret = devmm_chan_msg_send(prefetch_msg, sizeof(*prefetch_msg), 0);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Device copy data process failed. (ret=%d; dst_location.id=%u; va=0x%llx)\n", ret,
                      batch_info->dst_location.id, batch_info->va);
        goto dma_unmap_page;
#endif
    }

dma_unmap_page:
    for (i--; i >= 0; i--) {
        devmm_dma_unmap_page(NULL, &dma_addr[i]);
    }
    devmm_kvfree(prefetch_msg);

    return ret;
}

static int uvm_prefetch_d2h_set_page_info(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                          struct uvm_prefetch_batch_info *batch_info, ka_page_t **pages)
{
    uint64_t va = batch_info->va;
    struct uvm_page_info page_info = {0};
    ka_vm_area_struct_t *vma = NULL;
    uint32_t page_prot = 0;
    int i = 0, ret = 0;

    vma = devmm_find_vma(svm_proc, va);
    if (!vma) {
#ifndef EMU_ST
        devmm_drv_err("VMA is NULL.\n");
        return -EINVAL;
#endif
    }
    if (vma->vm_start > va || vma->vm_end < va + DEVMM_UVM_PAGE_SIZE) {
        devmm_drv_err("Invalid VMA range. (vm_start=%llx, vm_end=%llx)\n", vma->vm_start, vma->vm_end);
        return -EINVAL;
    }

    ret = devmm_pages_remap(svm_proc, va, batch_info->cnt, pages, page_prot);
    if (ret != 0) {
        devmm_drv_err("Insert pages vma error. (ret=%d, va=0x%llx, page_num=%hu)\n", ret, va, batch_info->cnt);
        return -EIO;
    }
    for (i = 0; i < batch_info->cnt; va += uvm_heap->chunk_page_size, i++) {
        ret = uvm_get_page_info_by_va(uvm_heap, va, &page_info);
        if (ret != 0) {
            devmm_drv_err("Get page info by va failed. (va=0x%llx)\n", va);
            return -EINVAL;
        }
        page_bitmap_set_host_mapped(page_info.page_bitmap);
        uvm_set_page_mapping(page_info.page_map, pages[i], UVM_HOST_ID);
        page_bitmap_unset_device_mapped(page_info.page_bitmap);
        clear_device_present(page_info.dev_bitmap, batch_info->src_location.id);
    }

    return ret;
}

static int uvm_prefetch_d2h(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                            struct uvm_prefetch_batch_info *batch_info)
{
    uint64_t va = batch_info->va;        /* Virtual Address */
    uint64_t dev_pa[MAX_CONT_NUM] = {0}; /* DEVice Physical Address */
    ka_page_t **pages = NULL;            /* host */
    struct devmm_phy_addr_attr attr = {0};
    int ret = 0;

#ifndef EMU_ST
    if (batch_info->dst_location.type == DRV_UVM_LOCATION_TYPE_HOST_NUMA &&
        (batch_info->dst_location.id <= 0 || !node_online(batch_info->dst_location.id - 1))) {
        devmm_drv_err("Invalid host numa id. (id=%d)\n", batch_info->dst_location.id);
        return -EINVAL;
    }
#endif

    if (batch_info->is_readmostly) {
        devmm_drv_err("Uvm do not support prefetch d2h in readmostly scene.\n");
        return -EFAULT;
    }

    ret = uvm_prefetch_d2h_get_page_info(svm_proc, uvm_heap, batch_info, dev_pa);
    if (ret != 0) {
        devmm_drv_err("Get page info failed.\n");
        return ret;
    }

    ret = uvm_prefetch_d2h_alloc_pages(svm_proc, uvm_heap, batch_info, &pages, &attr);
    if (ret == -ENOMEM) {
        devmm_drv_err("Alloc pages failed.\n");
        return ret;
    } else if (ret != 0) {
        devmm_drv_err("Prefetch d2h alloc pages failed. (ret=%d)\n", ret);
        goto free_tmp_pages;
    }

    ret = uvm_prefetch_d2h_sync(svm_proc, uvm_heap, batch_info, dev_pa, pages);
    if (ret != 0) {
        devmm_drv_err("Sync device data error. (ret=%d, va=0x%llx, page_num=%hu\n)", ret, va, batch_info->cnt);
        goto free_pa;
    }

    ret = uvm_prefetch_d2h_set_page_info(svm_proc, uvm_heap, batch_info, pages);
    if (ret == -EIO) {
        devmm_drv_err("Set page info failed.\n");
        goto free_pa;
    } else if (ret != 0) {
        devmm_drv_err("Prefetch d2h set page info failed. (ret=%d)\n", ret);
    }

    goto free_tmp_pages;

free_pa:
    devmm_free_pages(&attr, pages, batch_info->cnt);
free_tmp_pages:
    devmm_kvfree(pages);
    pages = NULL;

    return ret;
}

static int uvm_prefetch_d2d(void)
{
    return 0;
}

static inline bool is_h2h_direction(struct uvm_prefetch_batch_info *batch_info)
{
    return (batch_info->src_location.type == DRV_UVM_LOCATION_TYPE_HOST &&
            batch_info->dst_location.type == DRV_UVM_LOCATION_TYPE_HOST_NUMA);
}

static inline bool is_h2d_direction(struct uvm_prefetch_batch_info *batch_info)
{
    return (batch_info->src_location.type == DRV_UVM_LOCATION_TYPE_HOST &&
            batch_info->dst_location.type == DRV_UVM_LOCATION_TYPE_DEVICE);
}

static inline bool is_d2h_direction(struct uvm_prefetch_batch_info *batch_info)
{
    return (batch_info->src_location.type == DRV_UVM_LOCATION_TYPE_DEVICE &&
            (batch_info->dst_location.type == DRV_UVM_LOCATION_TYPE_HOST ||
             batch_info->dst_location.type == DRV_UVM_LOCATION_TYPE_HOST_NUMA));
}

static inline bool is_d2d_direction(struct uvm_prefetch_batch_info *batch_info)
{
    return (batch_info->src_location.type == DRV_UVM_LOCATION_TYPE_DEVICE &&
            batch_info->dst_location.type == DRV_UVM_LOCATION_TYPE_DEVICE);
}

static int uvm_prefetch_all_direction(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                                      struct uvm_prefetch_batch_info *batch_info, struct uvm_prefetch_type *type,
                                      ka_atomic_t *h2d_readmoslty_msg_send_cnt)
{
    int ret = 0;

    if (is_h2h_direction(batch_info)) {
        ret = uvm_prefetch_h2h(svm_proc, uvm_heap, batch_info);
        if (ret != 0) {
            devmm_drv_err("uvm_prefetch_h2h failed.\n");
        }
    } else if (is_h2d_direction(batch_info)) {
        ret = uvm_prefetch_h2d(svm_proc, uvm_heap, batch_info, type, h2d_readmoslty_msg_send_cnt);
        if (ret != 0) {
            devmm_drv_err("uvm_prefetch_h2d failed.\n");
        }
    } else if (is_d2h_direction(batch_info)) {
        ret = uvm_prefetch_d2h(svm_proc, uvm_heap, batch_info);
        if (ret != 0) {
            devmm_drv_err("uvm_prefetch_d2h failed.\n");
        }
    } else if (is_d2d_direction(batch_info)) {
        ret = uvm_prefetch_d2d();
        if (ret != 0) {
            devmm_drv_err("uvm_prefetch_d2d failed.\n");
        }
    } else {
#ifndef EMU_ST
        return -EOPNOTSUPP;
#endif
    }

    return ret;
}

static int uvm_prefetch_h2d_readmostly_last_msg(struct devmm_svm_process *svm_proc,
                                                struct uvm_prefetch_batch_info *batch_info,
                                                ka_atomic_t *h2d_readmoslty_msg_send_cnt)
{
    struct devmm_chan_uvm_prefetch *prefetch_msg = NULL;
    int ret;

    prefetch_msg = (struct devmm_chan_uvm_prefetch *)devmm_kzalloc_ex(sizeof(struct devmm_chan_uvm_prefetch),
                                                                      KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (prefetch_msg == NULL) {
        devmm_drv_err("Kzalloc error. (va=0x%lx)\n", batch_info->va);
        return -ENOMEM;
    }

    prefetch_msg->head.dev_id = devmm_get_phyid_devid_from_svm_process(svm_proc, batch_info->dst_location.id);
    prefetch_msg->head.msg_id = DEVMM_CHAN_UVM_PREFETCH_H2D_READMOSTLY_ID;
    prefetch_msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    prefetch_msg->head.process_id.vfid = svm_proc->process_id.vfid;
    prefetch_msg->is_read_mostly = true;
    prefetch_msg->va = 0;
    prefetch_msg->cnt = 0;
    prefetch_msg->msg_cnt = ka_base_atomic_read(h2d_readmoslty_msg_send_cnt);
    prefetch_msg->id = batch_info->id;

    /* sync send msg:device todo copy data process */
    ret = devmm_chan_msg_send(prefetch_msg, sizeof(*prefetch_msg), 0);
    if (ret != 0) {
        devmm_drv_err("Device copy data process failed. (ret=%d; dev_id=%u; va=0x%llx)\n", ret,
                      batch_info->dst_location.id, prefetch_msg->va);
    }

    devmm_kfree_ex(prefetch_msg);

    return ret;
}

static void uvm_prefetch_get_page_location(struct uvm_page_info *page_info, struct drv_uvm_location *location)
{
    if (page_info->page_map->devid == UVM_HOST_ID) {
        location->type = DRV_UVM_LOCATION_TYPE_HOST;
    } else {
        location->type = DRV_UVM_LOCATION_TYPE_DEVICE;
        location->id = page_info->page_map->devid;
    }
}

static bool uvm_prefetch_batch_try_add(struct uvm_prefetch_batch_info *batch_info, struct uvm_page_info *page_info,
                                       uint64_t cur_va, struct drv_uvm_location *src_location,
                                       struct drv_uvm_location *dst_location)
{
    bool is_readmostly = page_bitmap_get_read_mostly(page_info->page_bitmap);

    uvm_prefetch_get_page_location(page_info, src_location);
    if (batch_info->cnt == 0) {
        batch_info->va = cur_va;
        batch_info->src_location = *src_location;
        batch_info->dst_location = *dst_location;
        batch_info->is_readmostly = is_readmostly;
    }

    if (!is_uvm_location_same(*src_location, batch_info->src_location) || is_readmostly != batch_info->is_readmostly ||
        batch_info->cnt >= MAX_CONT_NUM) {
        return false;
    }

    batch_info->cnt++;
    return true;
}

struct uvm_prefetch_batch_ctx {
    struct devmm_svm_process *svm_proc;
    struct devmm_uvm_heap *uvm_heap;
    struct uvm_prefetch_batch_info *batch_info;
    struct uvm_prefetch_type *type;
    struct drv_uvm_location *src_location;
    struct drv_uvm_location *dst_location;
    struct uvm_mem_range *mem_range;
    ka_atomic_t *h2d_readmoslty_msg_send_cnt;
};

enum uvm_prefetch_page_state {
    UVM_PREFETCH_PAGE_VALID,
    UVM_PREFETCH_PAGE_SKIP,
    UVM_PREFETCH_PAGE_INVALID
};

struct uvm_prefetch_locked_batch {
    struct uvm_prefetch_page_lock page_locks[MAX_CONT_NUM];
    uint32_t cnt;
};

static int uvm_prefetch_get_page_info(struct uvm_prefetch_batch_ctx *ctx, uint64_t cur_va,
                                      struct uvm_page_info *page_info)
{
    int ret = uvm_get_page_info_by_va(ctx->uvm_heap, cur_va, page_info);
    if (ret != 0) {
        devmm_drv_err("Get page info by va failed. (va=0x%llx)\n", cur_va);
        return -EINVAL;
    }
    return 0;
}

static enum uvm_prefetch_page_state uvm_prefetch_check_page(struct uvm_page_info *page_info, uint64_t cur_va)
{
#ifndef EMU_ST
    if (!page_bitmap_get_alloced(page_info->page_bitmap)) {
        devmm_drv_err("Va is not alloced, va = 0x%llx.\n", cur_va);
        return UVM_PREFETCH_PAGE_INVALID;
    }
    if (!page_bitmap_get_host_mapped(page_info->page_bitmap) &&
        !page_bitmap_get_device_mapped(page_info->page_bitmap)) {
        return UVM_PREFETCH_PAGE_SKIP;
    }
#endif
    return UVM_PREFETCH_PAGE_VALID;
}

static void uvm_prefetch_release_batch(struct uvm_prefetch_batch_info *batch_info,
                                       struct uvm_prefetch_locked_batch *locked_batch)
{
    while (locked_batch->cnt > 0) {
        locked_batch->cnt--;
        uvm_prefetch_release_page_lock(&locked_batch->page_locks[locked_batch->cnt]);
    }
    batch_info->cnt = 0;
}

static int uvm_prefetch_execute_batch(struct uvm_prefetch_batch_ctx *ctx)
{
    int ret = uvm_prefetch_all_direction(ctx->svm_proc, ctx->uvm_heap, ctx->batch_info, ctx->type,
                                         ctx->h2d_readmoslty_msg_send_cnt);
    if (ret != 0) {
        devmm_drv_err("uvm_prefetch_all_direction failed. (va=0x%llx)\n", ctx->batch_info->va);
    }
    return ret;
}

/* Allocate physical memory at the destination location and establish the mapping for a page that is
 * allocated but not yet mapped on either the host or the device.
 */
#ifndef EMU_ST
static int uvm_prefetch_alloc_and_map_dst_page(struct uvm_prefetch_batch_ctx *ctx, struct uvm_page_info *page_info,
                                               uint64_t cur_va)
{
    int ret;

    if (ctx->dst_location->type == DRV_UVM_LOCATION_TYPE_HOST ||
        ctx->dst_location->type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) {
        int numa_id = (ctx->dst_location->type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) ? ctx->dst_location->id :
                                                                                     INVALID_NUMA;
        ret = uvm_host_alloc_and_map_page(ctx->svm_proc, page_info, cur_va, numa_id, false);
        if (ret != 0) {
            devmm_drv_err("Uvm_host_alloc_and_map_page failed. (ret=%d, va=0x%llx)\n", ret, cur_va);
            return ret;
        }
        page_bitmap_set_host_mapped(page_info->page_bitmap);
    } else if (ctx->dst_location->type == DRV_UVM_LOCATION_TYPE_DEVICE) {
        u64 dev_pa = 0;
        ret = uvm_prealloc_one_page_device(ctx->svm_proc, page_info, ctx->dst_location->id, &dev_pa);
        if (ret != 0) {
            devmm_drv_err("Uvm_prealloc_one_page_device failed. (ret=%d, va=0x%llx)\n", ret, cur_va);
            return ret;
        }
        set_page_mapping(page_info->page_map, dev_pa, ctx->dst_location->id);
        set_device_present(page_info->dev_bitmap, ctx->dst_location->id);
        page_bitmap_set_device_mapped(page_info->page_bitmap);
    } else {
        return -EOPNOTSUPP;
    }

    return 0;
}
#endif

/*
 * Handle a page whose state is not UVM_PREFETCH_PAGE_VALID.
 * Releases the current page_lock internally.
 * Returns: 0 on success (caller must release locked_batch and advance to next page),
 * -EINVAL for an invalid page, or another negative errno from uvm_prefetch_alloc_and_map_dst_page.
 */
static int uvm_prefetch_handle_nonvalid_page(struct uvm_prefetch_batch_ctx *ctx, struct uvm_page_info *page_info,
                                             struct uvm_prefetch_page_lock *page_lock,
                                             enum uvm_prefetch_page_state page_state, uint64_t cur_va)
{
    int ret = 0;

    if (page_state == UVM_PREFETCH_PAGE_INVALID) {
#ifndef EMU_ST
        uvm_prefetch_release_page_lock(page_lock);
#endif
        return -EINVAL;
    }

    /* Page is allocated but not mapped on host or device: allocate physical
     * memory at the destination location and establish the mapping. */
#ifndef EMU_ST
    ret = uvm_prefetch_alloc_and_map_dst_page(ctx, page_info, cur_va);
    uvm_prefetch_release_page_lock(page_lock);
    return ret;
#endif
}

static int uvm_prefetch_process_batch(struct uvm_prefetch_batch_ctx *ctx)
{
    struct uvm_prefetch_locked_batch locked_batch = {0};
    struct uvm_page_info page_info = {0};
    uint64_t cur_va = ctx->mem_range->aligned_va;
    int i = 0, ret = 0;

    while (i < ctx->mem_range->aligned_cnt) {
        struct uvm_prefetch_page_lock page_lock = {0};
        enum uvm_prefetch_page_state page_state;

        ret = uvm_prefetch_get_page_info(ctx, cur_va, &page_info);
        if (ret != 0) {
            break;
        }
        uvm_prefetch_acquire_page_read_lock(&page_info, &page_lock);
        uvm_prefetch_upgrade_page_lock(&page_info, &page_lock);
        page_state = uvm_prefetch_check_page(&page_info, cur_va);
        if (page_state != UVM_PREFETCH_PAGE_VALID) {
#ifndef EMU_ST
            ret = uvm_prefetch_handle_nonvalid_page(ctx, &page_info, &page_lock, page_state, cur_va);
#endif
            if (ret != 0) {
                break;
            }
            uvm_prefetch_release_batch(ctx->batch_info, &locked_batch);
            i++;
            cur_va += ctx->uvm_heap->chunk_page_size;
            continue;
        }
        if (!uvm_prefetch_batch_try_add(ctx->batch_info, &page_info, cur_va, ctx->src_location, ctx->dst_location)) {
            uvm_prefetch_release_page_lock(&page_lock);
        } else {
            locked_batch.page_locks[locked_batch.cnt++] = page_lock;
            i++;
            cur_va += ctx->uvm_heap->chunk_page_size;
            if (i < ctx->mem_range->aligned_cnt && ctx->batch_info->cnt < MAX_CONT_NUM) {
                continue;
            }
        }
        ret = uvm_prefetch_execute_batch(ctx);
        uvm_prefetch_release_batch(ctx->batch_info, &locked_batch);
        if (ret != 0) {
            return ret;
        }
    }

    uvm_prefetch_release_batch(ctx->batch_info, &locked_batch);
    return ret;
}

static int uvm_prefetch_set_mem_attr(struct devmm_uvm_heap *uvm_heap, struct uvm_mem_range *mem_range,
                                     struct drv_uvm_location *dst_location)
{
    struct uvm_page_info page_info = {0};
    uint64_t cur_va = 0;
    int i = 0, ret = 0;

    for (i = 0, cur_va = mem_range->aligned_va; i < mem_range->aligned_cnt; i++, cur_va += uvm_heap->chunk_page_size) {
        ret = uvm_get_page_info_by_va(uvm_heap, cur_va, &page_info);
        if (ret) {
            devmm_drv_err("Get page info by va failed. (va=0x%llx)\n", cur_va);
            return -EINVAL;
        }
        page_bitmap_set_last_prefetch_loc(page_info.page_bitmap, dst_location->id);
        page_bitmap_set_last_prefetch_loc_type(page_info.page_bitmap, dst_location->type);
    }

    return ret;
}

static int uvm_prefetch(struct devmm_svm_process *svm_proc, struct devmm_uvm_heap *uvm_heap,
                        struct devmm_uvm_prefetch_para *prefetch_para)
{
    struct uvm_prefetch_batch_info batch_info = {0};
    struct uvm_mem_range mem_range = {0};
    struct uvm_prefetch_type type = {0};
    struct drv_uvm_location src_location = {0}, dst_location = {0};
    ka_atomic_t h2d_readmoslty_msg_send_cnt;
    struct uvm_prefetch_batch_ctx batch_ctx = {0};
    uint64_t aligned_size;
    int ret = 0;

#ifndef EMU_ST
    batch_info.id = uvm_get_uuid();
#endif
    ka_base_atomic_set(&h2d_readmoslty_msg_send_cnt, 0);
    dst_location.id = prefetch_para->location.id;
    dst_location.type = prefetch_para->location.type;

    if (!devmm_acquire_aligned_addr_and_cnt(prefetch_para->va, prefetch_para->size, true, &mem_range.aligned_va,
                                            &aligned_size)) {
        devmm_drv_err("Acquire aligned addr and cnt failed. (dev_ptr=0x%llx; byte_count=%llx)\n", prefetch_para->va,
                      prefetch_para->size);
        return -EINVAL;
    }
    mem_range.aligned_cnt = aligned_size / uvm_heap->chunk_page_size;

    batch_ctx.svm_proc = svm_proc;
    batch_ctx.uvm_heap = uvm_heap;
    batch_ctx.batch_info = &batch_info;
    batch_ctx.type = &type;
    batch_ctx.src_location = &src_location;
    batch_ctx.dst_location = &dst_location;
    batch_ctx.mem_range = &mem_range;
    batch_ctx.h2d_readmoslty_msg_send_cnt = &h2d_readmoslty_msg_send_cnt;

    ret = uvm_prefetch_process_batch(&batch_ctx);
    if (ret != 0) {
        devmm_drv_err("Process batch failed.\n");
        return ret;
    }

    if (type.h2d_readmostly) {
        ret = uvm_prefetch_h2d_readmostly_last_msg(svm_proc, &batch_info, &h2d_readmoslty_msg_send_cnt);
        if (ret != 0) {
            devmm_drv_err("uvm_prefetch_h2d_readmostly_last_msg failed. (va=0x%llx)\n", mem_range.aligned_va);
            return ret;
        }
    }

    uvm_hnode_wait_for_removal(batch_info.id);

    ret = uvm_prefetch_set_mem_attr(uvm_heap, &mem_range, &dst_location);
    if (ret != 0) {
        devmm_drv_err("Set mem attr failed.\n");
    }

    return ret;
}

int devmm_uvm_ioctl_prefetch(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_uvm_prefetch_para *prefetch_para = &arg->data.uvm_prefetch_para;
    struct devmm_uvm_heap *uvm_heap = NULL;
    int ret = 0;

    uvm_heap = devmm_uvm_get_heap(svm_proc, prefetch_para->va);
    if (uvm_heap == NULL) {
#ifndef EMU_ST
        devmm_drv_err("UVM Heap is NULL or error. (heap_is_null=%d; va=0x%llx)\n", (uvm_heap == NULL),
                      prefetch_para->va);
        return -EADDRNOTAVAIL;
#endif
    }

    if (!devmm_vaddr_and_size_is_in_uvm_range(prefetch_para->va, prefetch_para->size)) {
#ifndef EMU_ST
        devmm_drv_err("Prefetch addr and size is not in uvm range. (va=0x%llx, size=%llu)\n", prefetch_para->va,
                      prefetch_para->size);
        return -EADDRNOTAVAIL;
#endif
    }
    if (prefetch_para->location.type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) {
        prefetch_para->location.id += 1;
    }

    ret = uvm_check_location(prefetch_para->location.type, prefetch_para->location.id, true);
    if (ret != 0) {
        return ret;
    }

    return uvm_prefetch(svm_proc, uvm_heap, prefetch_para);
}

static int uvm_prefetch_batch_copy_from_user_data(struct devmm_uvm_prefetch_batch_para *para, uint64_t *vas,
                                                  uint64_t *sizes, struct drv_uvm_location *locs, uint64_t *loc_ids)
{
    int ret = 0;
    ret += ka_base_copy_from_user(vas, (void __ka_user *)para->vas, para->count * sizeof(uint64_t));
    ret += ka_base_copy_from_user(sizes, (void __ka_user *)para->sizes, para->count * sizeof(uint64_t));
    ret += ka_base_copy_from_user(locs, (void __ka_user *)para->locations,
                                  para->num_loc * sizeof(struct drv_uvm_location));
    ret += ka_base_copy_from_user(loc_ids, (void __ka_user *)para->loc_ids, para->num_loc * sizeof(uint64_t));
    return ret;
}

static int uvm_prefetch_batch_prefetch_for_one(struct devmm_svm_process *svm_proc,
                                               struct devmm_uvm_prefetch_batch_para *para, uint64_t *vas,
                                               uint64_t *sizes, struct drv_uvm_location *locs, uint64_t *loc_ids,
                                               uint64_t i, int *loc_ids_index)
{
    struct devmm_uvm_prefetch_para prefetch_para = {0};
    struct devmm_uvm_heap *uvm_heap = NULL;
    int ret = 0;

    prefetch_para.va = vas[i];
    prefetch_para.size = sizes[i];
    if (*loc_ids_index + 1 < para->num_loc && i >= loc_ids[*loc_ids_index + 1]) {
        (*loc_ids_index)++;
    }
    prefetch_para.location = locs[*loc_ids_index];
    prefetch_para.flags = para->flags;

    if (prefetch_para.location.type == DRV_UVM_LOCATION_TYPE_HOST_NUMA) {
        prefetch_para.location.id += 1;
    }

    uvm_heap = devmm_uvm_get_heap(svm_proc, prefetch_para.va);
    if (uvm_heap == NULL) {
        devmm_drv_err("UVM Heap is NULL or error. (heap_is_null=%d; va=0x%llx)\n", (uvm_heap == NULL),
                      prefetch_para.va);
        return -EADDRNOTAVAIL;
    }

    if (!devmm_vaddr_and_size_is_in_uvm_range(prefetch_para.va, prefetch_para.size)) {
        devmm_drv_err("Prefetch addr and size is not in uvm range. (va=0x%llx, size=%llu)\n", prefetch_para.va,
                      prefetch_para.size);
        return -EADDRNOTAVAIL;
    }

    ret = uvm_prefetch(svm_proc, uvm_heap, &prefetch_para);
    if (ret != 0) {
        devmm_drv_err("uvm_prefetch failed. (ret=%d)\n", ret);
    }

    return ret;
}

int devmm_uvm_ioctl_prefetch_batch(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_uvm_prefetch_batch_para *para = &arg->data.uvm_prefetch_batch_para;
    uint64_t *vas, *sizes, *loc_ids;
    struct drv_uvm_location *locs;
    int ret = 0, loc_ids_index = -1;
    uint64_t i;
    uint64_t count = para->count;

    if (para->count > MAX_PREFETCH_COUNT || para->num_loc > MAX_PREFETCH_LOCS) {
#ifndef EMU_ST
        devmm_drv_err("Count or num_loc is too large. (count=%llu; num_loc=%llu; max_count=%llu; max_num_loc=%llu)\n",
                      para->count, para->num_loc, (u64)MAX_PREFETCH_COUNT, (u64)MAX_PREFETCH_LOCS);
        return -EINVAL;
#endif
    }

    vas = devmm_kzalloc_ex(para->count * sizeof(uint64_t), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    sizes = devmm_kzalloc_ex(para->count * sizeof(uint64_t), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    locs = devmm_kzalloc_ex(para->num_loc * sizeof(struct drv_uvm_location), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    loc_ids = devmm_kzalloc_ex(para->num_loc * sizeof(uint64_t), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (!vas || !sizes || !locs || !loc_ids) {
        ret = -ENOMEM;
        goto out_free;
    }

    ret = uvm_prefetch_batch_copy_from_user_data(para, vas, sizes, locs, loc_ids);
    if (ret != 0) {
        devmm_drv_err("Copy from user data failed, err ret=%d\n", ret);
        ret = -EFAULT;
        goto out_free;
    }

    for (i = 0; i < count; i++) {
        ret = uvm_prefetch_batch_prefetch_for_one(svm_proc, para, vas, sizes, locs, loc_ids, i, &loc_ids_index);
        if (ret != 0) {
            goto out_free;
        }
    }

out_free:
    devmm_kfree_ex(vas);
    devmm_kfree_ex(sizes);
    devmm_kfree_ex(locs);
    devmm_kfree_ex(loc_ids);
    return ret;
}
