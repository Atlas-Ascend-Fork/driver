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
#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_barrier_pub.h"
#include "ka_sched_pub.h"

#include "ascend_hal_define.h"
#include "kernel_version_adapt.h"
#include "comm_kernel_interface.h"
#include "pbl_uda.h"
#include "dpa_kernel_interface.h"

#include "svm_pub.h"
#include "va_mng.h"
#include "framework_dev.h"
#include "svm_addr_desc.h"
#include "svm_kern_log.h"
#include "svm_pgtable.h"
#include "svm_smp.h"
#include "ksvmm.h"
#include "mwl.h"
#include "svm_slab.h"
#include "dma_copy.h"
#include "dbi_kern.h"
#include "pmq_client.h"
#include "copy_task.h"

#define SVM_COPY_TIMEOUT_MSECS (240ULL * 1000ULL)
static ka_atomic_t svm_instance = KA_BASE_ATOMIC_INIT(0);

static void svm_copy_task_release(ka_kref_t *kref)
{
    struct svm_copy_task *copy_task = ka_container_of(kref, struct svm_copy_task, ref);

    svm_kvfree(copy_task);
}

static void svm_copy_task_get(struct svm_copy_task *copy_task)
{
    ka_base_kref_get(&copy_task->ref);
}

static void svm_copy_task_put(struct svm_copy_task *copy_task)
{
    ka_base_kref_put(&copy_task->ref, svm_copy_task_release);
}

static bool copy_subtask_flag_is_sync(u32 flag)
{
    return ((flag & SVM_COPY_SUBTASK_SYNC) != 0);
}

static bool copy_subtask_flag_is_auto_recycle(u32 flag)
{
    return ((flag & SVM_COPY_SUBTASK_AUTO_RECYCLE) != 0);
}

static bool svm_copy_res_is_cc_host_mem(const struct svm_copy_res *res)
{
    return (res->va_info.udevid == uda_get_host_id()) && svm_dbi_kern_is_cc_vm();
}

static int _svm_copy_res_dma_addr_init(u32 udevid, struct svm_copy_res *res)
{
    u32 map_flag = (res->is_src) ? 0 : SVM_DMA_MAP_ACCESS_WRITE;
    u64 dst_page_size;
    int ret;

    if (res->va_info.udevid != uda_get_host_id()) {
        ret = hal_kernel_apm_query_slave_tgid_by_master(res->host_tgid, res->va_info.udevid, 0, &res->va_info.tgid);
        if (ret != 0) {
            svm_err("Get slave tgid failed. (udevid=%u)\n", res->va_info.udevid);
            return ret;
        }
    } else {
        res->va_info.tgid = res->host_tgid;
    }

    res->dma_handle = NULL;
    ret = svm_dma_addr_get(udevid, res->host_tgid, &res->va_info, &res->dma_info, &res->dma_handle);
    if (ret == 0) {
        if ((res->dma_info.is_write == false) && (res->is_src == false)) {
            svm_dma_addr_put_by_handle(res->dma_handle);
            res->dma_handle = NULL;
        } else {
            return 0;
        }
    }

    ret = svm_dbi_kern_query_npage_size(res->va_info.udevid, &dst_page_size);
    if (ret != 0) {
        svm_err("Get page size failed. (udevid=%u)\n", res->va_info.udevid);
        return ret;
    }

    ret = svm_dma_map_addr(udevid, res->host_tgid, &res->va_info, dst_page_size, map_flag, &res->dma_handle);
    if (ret != 0) {
        svm_err("Dma map failed. (udevid=%u)\n", res->va_info.udevid);
        return ret;
    }

    ret = svm_dma_addr_query_by_handle(res->dma_handle, &res->va_info, &res->dma_info);
    if (ret != 0) {
        svm_dma_addr_put_by_handle(res->dma_handle);
        res->dma_handle = NULL;
        svm_err("Query Dma info failed. (udevid=%u)\n", res->va_info.udevid);
        return ret;
    }

    return 0;
}

static int svm_copy_res_dma_addr_init(u32 udevid, struct svm_copy_res *res)
{
    int ret;

    ret = _svm_copy_res_dma_addr_init(udevid, res);
    if (ret != 0) {
        return ret;
    }

    if (res->is_src && svm_copy_res_is_cc_host_mem(res)) {
        /* Flush CPU memory to the bounce buffer before DMA. */
        svm_dma_addr_sync_for_device(udevid, &res->dma_info);
    }

    return 0;
}

static void _svm_copy_res_dma_addr_uninit(u32 udevid, struct svm_copy_res *res)
{
    svm_dma_addr_put_by_handle(res->dma_handle);
    res->dma_handle = NULL;

    res->dma_info.dma_addr_seg_num = 0;
    res->dma_info.seg = NULL;
}

static void svm_copy_res_dma_addr_uninit(u32 udevid, struct svm_copy_res *res)
{
    if (!res->is_src && svm_copy_res_is_cc_host_mem(res)) {
        /* Flush the bounce buffer back to CPU memory after DMA. */
        svm_dma_addr_sync_for_cpu(udevid, &res->dma_info);
    }

    _svm_copy_res_dma_addr_uninit(udevid, res);
}

static int svm_copy_res_sva_addr_init(struct svm_copy_res *res)
{
    u64 va = res->va_info.va;
    u64 size = res->va_info.size;
    int ret;

    ret = svm_smp_check_mem_exists(res->va_info.udevid, res->host_tgid, va, size);
    if (ret != 0) {
        ret = ksvmm_check_range(res->va_info.udevid, res->host_tgid, va, size);
        if (ret != 0) {
            if (!va_is_in_sp_range(va, size) &&
                svm_mwl_check_mem(res->va_info.udevid, res->host_tgid, MWL_ID_NO_USE, va, size) != 0) {
                svm_err("Invalid para. (udevid=%u; host_tgid=%d; va=0x%llx; size=0x%llx)\n", res->va_info.udevid,
                        res->host_tgid, va, size);
                return -EINVAL;
            }
        }
    }

    res->dma_info.seg = svm_kvzalloc(sizeof(struct svm_dma_addr_seg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (res->dma_info.seg == NULL) {
        return -ENOMEM;
    }

    res->dma_info.dma_addr_seg_num = 1;
    res->dma_info.first_seg_offset = 0;
    res->dma_info.last_seg_len = size;
    res->dma_info.seg->dma_addr = va;
    res->dma_info.seg->size = size;

    return 0;
}

static void svm_copy_res_sva_addr_uninit(struct svm_copy_res *res)
{
    res->ssid = 0;
    svm_kvfree(res->dma_info.seg);
    res->dma_info.dma_addr_seg_num = 0;
    res->dma_info.seg = NULL;
}

static int svm_src_copy_res_init(struct svm_copy_subtask *subtask)
{
    struct svm_copy_res *res = &subtask->src_res;
    struct copy_va_info *va_info = &subtask->va_info;
    u32 udevid = subtask->copy_task->udevid;
    /* D2D will trans to D2H. H2D src is host, D2H src is device */
    u32 src_udevid = (subtask->dir == SVM_H2D_CPY) ? uda_get_host_id() : va_info->src_udevid;
    u32 ssid;
    int ret;

    res->copy_use_va = svm_dbi_kern_is_support_sva(src_udevid);
    res->ssid = 0;
    res->host_tgid = (va_info->src_host_tgid != 0) ? va_info->src_host_tgid : ka_task_get_current_tgid();
    res->is_src = true;

    svm_global_va_pack(va_info->src_udevid, 0, va_info->src_va, va_info->size, &res->va_info);

    if (res->copy_use_va) {
        ret = hal_kernel_apm_query_slave_ssid_by_master(res->va_info.udevid, res->host_tgid, PROCESS_CP1, &ssid);
        if (ret != 0) {
            svm_err("Get slave ssid failed. (udevid=%u; host_tgid=%d)\n", res->va_info.udevid, res->host_tgid);
            return ret;
        }
        res->ssid = (int)ssid;
        return svm_copy_res_sva_addr_init(res);
    } else {
        return svm_copy_res_dma_addr_init(udevid, res);
    }
}

static void svm_src_copy_res_uninit(struct svm_copy_subtask *subtask)
{
    struct svm_copy_res *res = &subtask->src_res;
    struct copy_va_info *va_info = &subtask->va_info;
    u32 udevid = (subtask->dir == SVM_H2D_CPY) ? va_info->dst_udevid : va_info->src_udevid;

    if (res->copy_use_va) {
        svm_copy_res_sva_addr_uninit(res);
    } else {
        svm_copy_res_dma_addr_uninit(udevid, res);
    }
}

static int svm_dst_copy_res_init(struct svm_copy_subtask *subtask)
{
    struct svm_copy_res *res = &subtask->dst_res;
    struct copy_va_info *va_info = &subtask->va_info;
    u32 udevid = subtask->copy_task->udevid;
    /* D2D will trans to D2H. H2D dst is device, D2H dst is host */
    u32 dst_udevid = (subtask->dir == SVM_D2H_CPY) ? uda_get_host_id() : va_info->dst_udevid;
    u32 ssid;
    int ret;

    res->copy_use_va = svm_dbi_kern_is_support_sva(dst_udevid);
    res->ssid = 0;
    res->host_tgid = (va_info->dst_host_tgid != 0) ? va_info->dst_host_tgid : ka_task_get_current_tgid();
    res->is_src = false;

    svm_global_va_pack(va_info->dst_udevid, 0, va_info->dst_va, va_info->size, &res->va_info);

    if (res->copy_use_va) {
        ret = hal_kernel_apm_query_slave_ssid_by_master(res->va_info.udevid, res->host_tgid, PROCESS_CP1, &ssid);
        if (ret != 0) {
            svm_err("Get slave ssid failed. (udevid=%u; host_tgid=%d)\n", res->va_info.udevid, res->host_tgid);
            return ret;
        }
        res->ssid = (int)ssid;
        return svm_copy_res_sva_addr_init(res);
    } else {
        return svm_copy_res_dma_addr_init(udevid, res);
    }
}

static void svm_dst_copy_res_uninit(struct svm_copy_subtask *subtask)
{
    struct svm_copy_res *res = &subtask->dst_res;
    struct copy_va_info *va_info = &subtask->va_info;
    u32 udevid = (subtask->dir == SVM_H2D_CPY) ? va_info->dst_udevid : va_info->src_udevid;

    if (res->copy_use_va) {
        svm_copy_res_sva_addr_uninit(res);
    } else {
        svm_copy_res_dma_addr_uninit(udevid, res);
    }
}

static u64 svm_fill_dma_nodes(struct svm_copy_res *src_res, struct svm_copy_res *dst_res, enum svm_cpy_dir dir,
                              struct devdrv_dma_node *dma_nodes, u64 max_num)
{
    struct svm_dma_addr_info *src_dma = &src_res->dma_info;
    struct svm_dma_addr_info *dst_dma = &dst_res->dma_info;
    u64 i, curr_size, src_size, dst_size, src_index, dst_index, src_offset, dst_offset;
    u64 src_last_seg_offset, dst_last_seg_offset;
    u64 no_aligned_num = 0;

    src_offset = src_dma->first_seg_offset;
    dst_offset = dst_dma->first_seg_offset;

    src_last_seg_offset = (src_dma->dma_addr_seg_num == 1) ? src_dma->first_seg_offset : 0;
    dst_last_seg_offset = (dst_dma->dma_addr_seg_num == 1) ? dst_dma->first_seg_offset : 0;

    for (i = 0, src_index = 0, dst_index = 0;
         ((i < max_num) && (src_index < src_dma->dma_addr_seg_num) && (dst_index < dst_dma->dma_addr_seg_num)); i++) {
        src_size = (src_index < src_dma->dma_addr_seg_num - 1) ?
                       src_dma->seg[src_index].size - src_offset :
                       src_dma->last_seg_len + src_last_seg_offset - src_offset;
        dst_size = (dst_index < dst_dma->dma_addr_seg_num - 1) ?
                       dst_dma->seg[dst_index].size - dst_offset :
                       dst_dma->last_seg_len + dst_last_seg_offset - dst_offset;
        curr_size = ka_base_min(src_size, dst_size);

        dma_nodes[i].src_addr = src_dma->seg[src_index].dma_addr + src_offset;
        dma_nodes[i].dst_addr = dst_dma->seg[dst_index].dma_addr + dst_offset;
        dma_nodes[i].size = curr_size;
        dma_nodes[i].direction = (dir == SVM_H2D_CPY) ? DEVDRV_DMA_HOST_TO_DEVICE : DEVDRV_DMA_DEVICE_TO_HOST;
        dma_nodes[i].loc_passid = (dir == SVM_H2D_CPY) ? (u32)dst_res->ssid : (u32)src_res->ssid;

        src_offset += curr_size;
        dst_offset += curr_size;

        if (curr_size == src_size) {
            src_index++;
            src_offset = 0;
        }

        if (curr_size == dst_size) {
            dst_index++;
            dst_offset = 0;
        }

        /* If there is no 128 byte alignment, dma copy will be slow. */
        no_aligned_num += ((SVM_IS_ALIGNED(dma_nodes[i].src_addr, 128) == false) ||
                           (SVM_IS_ALIGNED(dma_nodes[i].dst_addr, 128) == false)) ?
                              1 :
                              0;
    }

    svm_debug("Fill dma_nodes succ. (src_va=0x%llx; dst_va=0x%llx; dma_node_num=%llu; no_aligned_num=%llu)\n",
              src_res->va_info.va, dst_res->va_info.va, i, no_aligned_num);

    return i;
}

static u64 svm_get_dma_node_max_num(struct svm_copy_res *src_res, struct svm_copy_res *dst_res)
{
    u64 src_max_num = src_res->copy_use_va ? 1 : src_res->dma_info.dma_addr_seg_num;
    u64 dst_max_num = dst_res->copy_use_va ? 1 : dst_res->dma_info.dma_addr_seg_num;

    return (ka_base_max_t(u64, src_max_num, dst_max_num) * 2); /* 2 * (src)dst max_num */
}

static int svm_dma_nodes_create(struct svm_copy_res *src_res, struct svm_copy_res *dst_res, enum svm_cpy_dir dir,
                                struct devdrv_dma_node **dma_nodes_out, u64 *dma_node_num_out)
{
    struct devdrv_dma_node *dma_nodes = NULL;
    u64 max_num = svm_get_dma_node_max_num(src_res, dst_res);
    u64 actual_num;

    dma_nodes = svm_kvzalloc(max_num * sizeof(struct devdrv_dma_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (dma_nodes == NULL) {
        svm_err("svm_kvzalloc dma_nodes failed. (size=%llu)\n", max_num * sizeof(struct devdrv_dma_node));
        return -ENOMEM;
    }

    actual_num = svm_fill_dma_nodes(src_res, dst_res, dir, dma_nodes, max_num);

    *dma_nodes_out = dma_nodes;
    *dma_node_num_out = actual_num;
    return 0;
}

static void svm_dma_nodes_destroy(struct devdrv_dma_node *dma_nodes)
{
    svm_kvfree(dma_nodes);
}

static void svm_copy_subtask_insert(struct svm_copy_task *copy_task, struct svm_copy_subtask *subtask)
{
    ka_task_spin_lock_bh(&copy_task->subtasks_list.spinlock);
    copy_task->subtasks_list.num++;
    ka_list_add_tail(&subtask->node, &copy_task->subtasks_list.head);
    ka_task_spin_unlock_bh(&copy_task->subtasks_list.spinlock);
}

static void svm_copy_subtask_erase(struct svm_copy_subtask *subtask)
{
    struct svm_copy_task *copy_task = subtask->copy_task;

    ka_task_spin_lock_bh(&copy_task->subtasks_list.spinlock);
    ka_list_del(&subtask->node);
    copy_task->subtasks_list.num--;
    ka_task_spin_unlock_bh(&copy_task->subtasks_list.spinlock);
}

static struct svm_copy_subtask *_svm_copy_subtask_create(struct svm_copy_task *copy_task, struct copy_va_info *info,
                                                         u32 flag)
{
    struct svm_copy_subtask *subtask = NULL;
    int ret;

    subtask = svm_kvzalloc(sizeof(struct svm_copy_subtask), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (subtask == NULL) {
        svm_err("svm_kvzalloc copy_sub_task failed. (size=%lu)\n", sizeof(struct svm_copy_subtask));
        return NULL;
    }
    KA_INIT_LIST_HEAD(&subtask->node);
    subtask->copy_task = copy_task;
    subtask->va_info = *info;
    subtask->flag = flag;
    subtask->dir = (info->src_udevid == copy_task->udevid) ? SVM_D2H_CPY :
                                                             SVM_H2D_CPY; /* copy dir decide by dma engine devid. */

    ret = svm_src_copy_res_init(subtask);
    if (ret != 0) {
        svm_err("Src copy res init failed. (ret=%d; src=0x%llx; size=%llu)\n", ret, info->src_va, info->size);
        goto free_subtask;
    }

    ret = svm_dst_copy_res_init(subtask);
    if (ret != 0) {
        svm_err("Dst copy res init failed. (ret=%d; dst=0x%llx; size=%llu)\n", ret, info->dst_va, info->size);
        goto uninit_src_res;
    }

    ret = svm_dma_nodes_create(&subtask->src_res, &subtask->dst_res, subtask->dir, &subtask->dma_nodes,
                               &subtask->dma_node_num);
    if (ret != 0) {
        svm_err("Create dma nodes failed. (ret=%d)\n", ret);
        goto uninit_dst_res;
    }

    return subtask;

uninit_dst_res:
    svm_dst_copy_res_uninit(subtask);
uninit_src_res:
    svm_src_copy_res_uninit(subtask);
free_subtask:
    svm_kvfree(subtask);
    return NULL;
}

static void _svm_copy_subtask_destroy(struct svm_copy_subtask *subtask)
{
    svm_dma_nodes_destroy(subtask->dma_nodes);
    svm_dst_copy_res_uninit(subtask);
    svm_src_copy_res_uninit(subtask);
    svm_kvfree(subtask);
}

struct svm_copy_subtask *svm_copy_subtask_create(struct svm_copy_task *copy_task, struct copy_va_info *info, u32 flag)
{
    struct svm_copy_subtask *subtask = NULL;

    svm_copy_task_get(copy_task);
    subtask = _svm_copy_subtask_create(copy_task, info, flag);
    if (subtask == NULL) {
        svm_copy_task_put(copy_task);
        return NULL;
    }

    if (!copy_subtask_flag_is_auto_recycle(subtask->flag)) {
        svm_copy_subtask_insert(copy_task, subtask);
    }

    return subtask;
}

void svm_copy_subtask_destroy(struct svm_copy_subtask *subtask)
{
    struct svm_copy_task *copy_task = subtask->copy_task;

    if (!copy_subtask_flag_is_auto_recycle(subtask->flag)) {
        svm_copy_subtask_erase(subtask);
    }

    _svm_copy_subtask_destroy(subtask);
    svm_copy_task_put(copy_task);
}

/* will call by pcie irq tasklet, can not sleep */
static void svm_dma_async_callback(void *data, u32 trans_id, u32 status)
{
    struct svm_copy_subtask *subtask = (struct svm_copy_subtask *)data;
    struct svm_copy_task *copy_task = subtask->copy_task;

    if (copy_subtask_flag_is_auto_recycle(subtask->flag)) {
        svm_copy_subtask_destroy(subtask);
    }
    copy_task->async_subtasks_ret.dma_ret |= status;

    ka_wmb();
    ka_task_up(&copy_task->async_subtasks_ret.sem);
    svm_copy_task_put(copy_task); /* Paired with submit async copy */
}

int svm_copy_subtask_submit(struct svm_copy_subtask *subtask)
{
    struct svm_copy_task *copy_task = subtask->copy_task;
    int ret;

    if (copy_subtask_flag_is_sync(subtask->flag)) {
        ret = svm_dma_sync_cpy(copy_task->udevid, subtask->dma_nodes, subtask->dma_node_num, copy_task->instance);
    } else {
        svm_copy_task_get(copy_task);
        ret = svm_dma_async_cpy(copy_task->udevid, subtask->dma_nodes, subtask->dma_node_num, svm_dma_async_callback,
                                (void *)subtask, copy_task->instance);
        if (ret != 0) {
            svm_copy_task_put(copy_task);
            return ret;
        }

        ka_base_atomic64_inc(&copy_task->async_subtasks_ret.to_wait_num);
    }

    return ret;
}

int svm_copy_task_wait(struct svm_copy_task *copy_task)
{
    struct svm_copy_async_subtask_ret *async_subtasks_ret = &copy_task->async_subtasks_ret;
    u64 num = ka_base_atomic64_read(&async_subtasks_ret->to_wait_num);
    u64 i;
    int ret;

    if (svm_dev_status_is_link_abnormal(copy_task->udevid)) {
        return 0;
    }

    for (i = 0; i < num; i++) {
        ret = ka_task_down_timeout(&async_subtasks_ret->sem, ka_system_msecs_to_jiffies(SVM_COPY_TIMEOUT_MSECS));
        if (ret != 0) {
            svm_warn("Wait copy task finish timeout. (ret=%d; i=%llu; num=%llu)\n", ret, i, num);
            return -ETIMEDOUT;
        }

        ka_base_atomic64_dec(&async_subtasks_ret->to_wait_num);
    }

    if (async_subtasks_ret->dma_ret != 0) {
        svm_warn("Async sub copy return err. (dma_ret=%d)\n", async_subtasks_ret->dma_ret);
        async_subtasks_ret->dma_ret = 0;
        return -EINVAL;
    }

    return 0;
}

int svm_copy_task_submit(struct svm_copy_task *copy_task)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    struct svm_copy_subtask *subtask = NULL;
    struct svm_copy_subtask *n = NULL;
    int ret;

    /* No need lock, SVM_DMA_DESC_TASK_MODE callback won't access list. */
    ka_list_for_each_entry_safe(subtask, n, &copy_task->subtasks_list.head, node)
    {
        ka_try_cond_resched(&stamp);
        ret = svm_copy_subtask_submit(subtask);
        if (ret != 0) {
            svm_err("Submit subtask failed. (ret=%d)\n", ret);
            (void)svm_copy_task_wait(copy_task);
            return ret;
        }
    }

    return 0;
}

struct svm_copy_task *svm_copy_task_create(u32 udevid)
{
    struct svm_copy_task *copy_task = NULL;
    int ret;

    copy_task = svm_kvzalloc(sizeof(struct svm_copy_task), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (copy_task == NULL) {
        svm_err("svm_kvzalloc copy_task failed. (size=%lu)\n", sizeof(struct svm_copy_task));
        return NULL;
    }

    copy_task->dev = uda_get_device(udevid);
    if (copy_task->dev == NULL) {
        svm_err("uda_get_device failed. (udevid=%u)\n", udevid);
        svm_kvfree(copy_task);
        return NULL;
    }

    ret = svm_dbi_kern_query_npage_size(udevid, &copy_task->dev_page_size);
    if (ret != 0) {
        svm_err("Get page size failed. (udevid=%u)\n", udevid);
        svm_kvfree(copy_task);
        return NULL;
    }

    ka_base_kref_init(&copy_task->ref);
    copy_task->udevid = udevid;

    ka_task_spin_lock_init(&copy_task->subtasks_list.spinlock);
    KA_INIT_LIST_HEAD(&copy_task->subtasks_list.head);
    copy_task->subtasks_list.num = 0;

    ka_base_atomic64_set(&copy_task->async_subtasks_ret.to_wait_num, 0);
    ka_task_sema_init(&copy_task->async_subtasks_ret.sem, 0);
    copy_task->async_subtasks_ret.dma_ret = 0;
    copy_task->instance = (u32)ka_base_atomic_inc_return(&svm_instance);

    return copy_task;
}

void svm_copy_task_destroy(struct svm_copy_task *copy_task)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    struct svm_copy_subtask *subtask = NULL;
    struct svm_copy_subtask *n = NULL;

    ka_list_for_each_entry_safe(subtask, n, &copy_task->subtasks_list.head, node)
    {
        ka_try_cond_resched(&stamp);
        ka_list_del(&subtask->node);
        copy_task->subtasks_list.num--;
        _svm_copy_subtask_destroy(subtask);
        svm_copy_task_put(copy_task);
    }

    ka_base_kref_put(&copy_task->ref, svm_copy_task_release);
}

u32 svm_copy_task_get_dma_node_num(struct svm_copy_task *copy_task)
{
    struct svm_copy_subtask *subtask = NULL;
    struct svm_copy_subtask *n = NULL;
    u32 num = 0;

    ka_list_for_each_entry_safe(subtask, n, &copy_task->subtasks_list.head, node)
    {
        num += subtask->dma_node_num;
    }

    return num;
}
