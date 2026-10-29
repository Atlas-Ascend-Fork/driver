/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/ioctl.h>
#include "ascend_hal.h"
#include "devmm_virt_comm.h"
#include "devmm_svm_init.h"
#include "svm_mem_statistics.h"
#include "devmm_svm.h"
#include "devmm_virt_com_heap.h"

#define DEVMM_ONE_HUNDRED 100
SVM_DECLARE_MODULE_NAME(svm_module_name);

STATIC void devmm_virt_com_heap_update_info(struct devmm_virt_com_heap *heap, struct devmm_virt_heap_type *heap_type,
                                            struct devmm_com_heap_ops *ops, struct devmm_virt_heap_para *heap_info)
{
    int i;
    heap->ops = ops;
    heap->heap_type = heap_type->heap_type;
    heap->heap_list_type = heap_type->heap_list_type;
    heap->heap_sub_type = heap_type->heap_sub_type;
    heap->heap_mem_type = heap_type->heap_mem_type;

    heap->is_base_heap = heap_info->is_base_heap;
#ifndef UVM_OPEN
    heap->is_uvm_heap = heap_info->is_uvm_heap;
#endif
    heap->is_host_pin = heap_info->is_host_pin;

    heap->start = heap_info->start;
    heap->end = heap->start + heap_info->heap_size - 1;
    heap->heap_size = heap_info->heap_size;
    heap->map_size = heap_info->map_size;
    heap->chunk_size = heap_info->page_size;
    heap->kernel_page_size = heap_info->kernel_page_size;
    heap->is_limited = heap_info->is_limited;
    heap->is_cache = true;

    heap->module_id = SVM_MAX_MODULE_ID;
    heap->devid = DEVMM_MAX_DEVICE_NUM;
    heap->mapped_size = 0;
    heap->reserve_size = 0;

    heap->sys_mem_alloced = 0;
    heap->sys_mem_freed = 0;
    heap->sys_mem_alloced_num = 0;
    heap->sys_mem_freed_num = 0;
    for (i = 0; i < (int)DEVMM_MEMTYPE_MAX; i++) {
        heap->need_cache_thres[i] = heap_info->need_cache_thres[i];
        heap->cur_cache_mem[i] = 0;
        heap->cur_alloc_cache_mem[i] = 0;
        heap->peak_alloc_cache_mem[i] = 0;
        heap->cache_mem_thres[i] = 0;
    }

    DEVMM_DRV_SWITCH("Heap update info. (heap_type=0x%x; sub_type=0x%x; size=%llu; "
                     "page_size=%u; start_addr=0x%lx; end_addr=0x%lx; cache=%u; map_size=%u; is_limited=%u)\n",
                     heap_type->heap_type, heap_type->heap_sub_type, heap->heap_size, heap->chunk_size, heap->start,
                     heap->end, heap->need_cache_thres[DEVMM_MEM_NORMAL], heap->map_size, heap->is_limited);
}

STATIC DVresult devmm_init_rbtree_queue(struct devmm_virt_com_heap *heap, uint32_t cache_numsize)
{
    struct devmm_rbtree_node *node = NULL;
    DVresult ret;

    if (devmm_is_snapshot_state()) {
        return DRV_ERROR_NONE;
    }

    ret = devmm_rbtree_alloc(&heap->rbtree_queue, cache_numsize);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Alloc rbtree failed.\n");
        return ret;
    }

    node = devmm_alloc_rbtree_node(&heap->rbtree_queue);
    if (node == NULL) {
        DEVMM_DRV_ERR("Alloc init rbtree node failed. (heap_idx=%u)\n", heap->heap_idx);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    devmm_assign_rbtree_node_data(heap->start, heap->heap_size, heap->heap_size, DEVMM_NODE_FIRST_VA_FLG, 0, node);
    ret = devmm_rbtree_insert_idle_size_tree(node, &heap->rbtree_queue);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Insert node to free size tree fail. (heap_idx=%u)\n", heap->heap_idx);
        goto devmm_insert_fail;
    }
    ret = devmm_rbtree_insert_idle_va_tree(node, &heap->rbtree_queue);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Insert node to free unmapped va tree fail. (heap_idx=%u)\n", heap->heap_idx);
        (void)devmm_rbtree_erase_idle_size_tree(node, &heap->rbtree_queue);
        goto devmm_insert_fail;
    }

    DEVMM_DRV_SWITCH("Insert node to DEVMM_IDLE_SIZE_TREE and DEVMM_IDLE_VA_TREE. (heap_idx=%u; "
                     "node=0x%llx; size=%llu; va=0x%llx; flag=%u; total=%llu)\n",
                     heap->heap_idx, (uint64_t)node, node->data.size, node->data.va, node->data.flag, node->data.total);

    return DRV_ERROR_NONE;

devmm_insert_fail:
    devmm_free_rbtree_node(node, &heap->rbtree_queue);
    return DRV_ERROR_INNER_ERR;
}

STATIC uint32_t devmm_get_rbtree_nodecache_numsize(struct devmm_virt_heap_type *heap_type)
{
    uint32_t heap_sub_type = heap_type->heap_sub_type;

    if (heap_sub_type == SUB_DVPP_TYPE) {
        return DEVMM_DVPP_HEAP_CACHE_NODE_NUM;
    } else {
        return DEVMM_COMM_HEAP_CACHE_NODE_NUM;
    }
}

DVresult devmm_virt_init_com_heap(struct devmm_virt_com_heap *heap, struct devmm_virt_heap_type *heap_type,
                                  struct devmm_com_heap_ops *ops, struct devmm_virt_heap_para *heap_info)
{
    DVresult ret;

    (void)pthread_rwlock_wrlock(&heap->heap_rw_lock);
    /* to ensure only init one time */
    if (heap->inited == 1) {
        (void)pthread_rwlock_unlock(&heap->heap_rw_lock);
        return DRV_ERROR_NONE;
    }
    devmm_virt_com_heap_update_info(heap, heap_type, ops, heap_info);

    ret = devmm_init_rbtree_queue(heap, devmm_get_rbtree_nodecache_numsize(heap_type));
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Init free size tree fail.\n");
        (void)pthread_rwlock_unlock(&heap->heap_rw_lock);
        return ret;
    }

    /* update heap info to kernel. */
    ret = devmm_ioctl_enable_heap(heap->heap_idx, heap->heap_type, heap->heap_sub_type, heap->heap_size,
                                  heap->heap_list_type, heap->is_host_pin);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Devmm_virt_ioctl_update_heap error. (ret_val=%d)\n", ret);
        devmm_rbtree_destory(&heap->rbtree_queue);
        (void)pthread_rwlock_unlock(&heap->heap_rw_lock);
        return ret;
    }
    heap->inited = 1;
    (void)pthread_rwlock_unlock(&heap->heap_rw_lock);

    return DRV_ERROR_NONE;
}

DVresult devmm_virt_init_com_base_heap(struct devmm_virt_com_heap *heap, struct devmm_virt_heap_type *heap_type,
                                       struct devmm_com_heap_ops *ops, struct devmm_virt_heap_para *heap_info)
{
    DVresult ret;

    /* to ensure only init one time */
    if (heap->inited == 1) {
        return DRV_ERROR_NONE;
    }

    (void)pthread_mutex_init(&heap->tree_lock, NULL);
    (void)pthread_rwlock_init(&heap->heap_rw_lock, NULL);

    devmm_virt_com_heap_update_info(heap, heap_type, ops, heap_info);

    ret = devmm_init_rbtree_queue(heap, DEVMM_BASE_HEAP_CACHE_NODE_NUM);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Init free size tree fail.\n");
        return ret;
    }

    heap->inited = 1;
#ifndef UVM_OPEN
    if (heap->is_uvm_heap) {
        heap->is_cache = false;
    }
#endif
    return DRV_ERROR_NONE;
}

static inline void devmm_node_flag_set_value(uint32_t *flag, uint32_t shift, uint32_t wide, uint32_t value)
{
    uint32_t msk = ((1U << wide) - 1);
    uint32_t val = (msk & value);

    (*flag) &= (uint32_t)(~(msk << shift));
    (*flag) |= (uint32_t)(val << shift);
}

STATIC INLINE uint32_t devmm_node_flag_get_value(uint32_t flag, uint32_t shift, uint32_t wide)
{
    uint32_t msk = ((1U << wide) - 1);
    uint32_t val = flag >> shift;

    return (val & msk);
}

STATIC INLINE void devmm_node_flag_set_side(uint32_t *flag, uint32_t side)
{
    devmm_node_flag_set_value(flag, DEVMM_NODE_SIDE_SHIFT, DEVMM_NODE_SIDE_WID, side);
}

STATIC INLINE uint32_t devmm_node_flag_get_side(uint32_t flag)
{
    return devmm_node_flag_get_value(flag, DEVMM_NODE_SIDE_SHIFT, DEVMM_NODE_SIDE_WID);
}

STATIC INLINE void devmm_node_flag_set_nocache(uint32_t *flag)
{
    devmm_node_flag_set_value(flag, DEVMM_NODE_NOCACHE_SHIFT, DEVMM_NODE_NOCACHE_WID, 1);
}

STATIC INLINE bool devmm_node_flag_is_nocache(uint32_t flag)
{
    return (bool)(devmm_node_flag_get_value(flag, DEVMM_NODE_NOCACHE_SHIFT, DEVMM_NODE_NOCACHE_WID) == 1);
}

STATIC INLINE void devmm_node_flag_set_devid(uint32_t *flag, uint32_t devid)
{
    devmm_node_flag_set_value(flag, DEVMM_NODE_DEVID_SHIFT, DEVMM_NODE_DEVID_WID, devid);
}

STATIC INLINE uint32_t devmm_node_flag_get_devid(uint32_t flag)
{
    return devmm_node_flag_get_value(flag, DEVMM_NODE_DEVID_SHIFT, DEVMM_NODE_DEVID_WID);
}

STATIC INLINE void devmm_node_flag_set_module_id(uint32_t *flag, uint32_t module_id)
{
    devmm_node_flag_set_value(flag, DEVMM_NODE_MODULE_ID_SHIFT, DEVMM_NODE_MODULE_ID_WID, module_id);
}

STATIC INLINE uint32_t devmm_node_flag_get_module_id(uint32_t flag)
{
    return devmm_node_flag_get_value(flag, DEVMM_NODE_MODULE_ID_SHIFT, DEVMM_NODE_MODULE_ID_WID);
}

STATIC INLINE void devmm_node_set_flag(struct devmm_rbtree_node *node, uint32_t flag)
{
    node->data.flag |= flag;
}

STATIC INLINE void devmm_node_flag_clear_memtype(uint32_t *flag)
{
    devmm_node_flag_set_value(flag, DEVMM_NODE_MEMTYPE_SHIFT, DEVMM_NODE_MEMTYPE_WID, 0);
}

STATIC INLINE void devmm_unmap_node_set_flag(struct devmm_rbtree_node *node, uint32_t flag)
{
    devmm_node_flag_clear_memtype(&node->data.flag);
    devmm_node_set_flag(node, flag);
}

STATIC INLINE void devmm_node_flag_set_mem_val(uint32_t *flag, uint32_t mem_val)
{
    devmm_node_flag_set_value(flag, DEVMM_NODE_MEM_VAL_SHIFT, DEVMM_NODE_MEM_VAL_WID, mem_val);
}

STATIC INLINE uint32_t devmm_node_flag_get_mem_val(uint32_t flag)
{
    return devmm_node_flag_get_value(flag, DEVMM_NODE_MEM_VAL_SHIFT, DEVMM_NODE_MEM_VAL_WID);
}

STATIC INLINE void devmm_node_flag_set_phy_memtype(uint32_t *flag, uint32_t phy_memtype)
{
    devmm_node_flag_set_value(flag, DEVMM_NODE_PHY_MEMTYPE_SHIFT, DEVMM_NODE_PHY_MEMTYPE_WID, phy_memtype);
}

STATIC INLINE uint32_t devmm_node_flag_get_phy_memtype(uint32_t flag)
{
    return devmm_node_flag_get_value(flag, DEVMM_NODE_PHY_MEMTYPE_SHIFT, DEVMM_NODE_PHY_MEMTYPE_WID);
}

STATIC INLINE void devmm_node_flag_set_page_type(uint32_t *flag, uint32_t page_type)
{
    devmm_node_flag_set_value(flag, DEVMM_NODE_PAGE_TYPE_SHIFT, DEVMM_NODE_PAGE_TYPE_WID, page_type);
}

STATIC INLINE uint32_t devmm_node_flag_get_page_type(uint32_t flag)
{
    return devmm_node_flag_get_value(flag, DEVMM_NODE_PAGE_TYPE_SHIFT, DEVMM_NODE_PAGE_TYPE_WID);
}

STATIC INLINE bool devmm_node_flag_is_mapped(struct devmm_rbtree_node *node)
{
    return (bool)devmm_node_flag_get_value(node->data.flag, DEVMM_NODE_MAPPED_BIT, 1);
}

STATIC INLINE bool devmm_node_flag_is_first_va(struct devmm_rbtree_node *node)
{
    if (node == NULL) {
        return false;
    }
    return (bool)devmm_node_flag_get_value(node->data.flag, DEVMM_NODE_FIRST_VA_BIT, 1);
}

bool devmm_node_is_need_restore(struct devmm_rbtree_node *node)
{
    return devmm_node_flag_is_first_va(node);
}

STATIC INLINE uint32_t devmm_node_flag_get_memtype(uint32_t flag)
{
    return devmm_node_flag_get_value(flag, DEVMM_NODE_MEMTYPE_SHIFT, DEVMM_NODE_MEMTYPE_WID);
}

STATIC INLINE void devmm_node_flag_set_readonly(uint32_t *flag)
{
    devmm_node_flag_set_value(flag, DEVMM_NODE_MEMTYPE_SHIFT, DEVMM_NODE_MEMTYPE_WID, DEVMM_MEM_RDONLY);
}

STATIC INLINE bool devmm_node_flag_is_readonly(uint32_t flag)
{
    return (bool)(devmm_node_flag_get_value(flag, DEVMM_NODE_MEMTYPE_SHIFT, DEVMM_NODE_MEMTYPE_WID) ==
                  DEVMM_MEM_RDONLY);
}

STATIC INLINE void devmm_add_cur_alloc_cache_mem(struct devmm_virt_com_heap *heap, uint64_t size, uint32_t memtype)
{
    heap->cur_alloc_cache_mem[memtype] += (size <= heap->need_cache_thres[memtype]) ? size : 0;
}

STATIC INLINE void devmm_node_clear_flag(struct devmm_rbtree_node *node, uint32_t flag)
{
    node->data.flag &= (~flag);
}

STATIC INLINE void devmm_add_cur_cache_mem(struct devmm_virt_com_heap *heap, uint64_t size, uint32_t memtype)
{
    heap->cur_cache_mem[memtype] += size;
}

STATIC INLINE void devmm_sub_cur_alloc_cache_mem(struct devmm_virt_com_heap *heap, uint64_t size, uint32_t memtype)
{
    uint64_t sub_size = (size <= heap->need_cache_thres[memtype]) ? size : 0;

    if (heap->cur_alloc_cache_mem[memtype] < sub_size) {
        DEVMM_DRV_ERR("Cache count abnormal. (memtype=%u; cur_alloc_cache_mem=%llu; sub_size=%llu; heap_idx=%u; "
                      "cur_cache_mem=%llu; cache_mem_thres=%llu; peak_alloc_cache_mem=%llu)\r\n",
                      memtype, heap->cur_alloc_cache_mem[memtype], sub_size, heap->heap_idx,
                      heap->cur_cache_mem[memtype], heap->cache_mem_thres[memtype],
                      heap->peak_alloc_cache_mem[memtype]);
        return;
    }

    heap->cur_alloc_cache_mem[memtype] -= sub_size;
}

STATIC INLINE void devmm_sub_cur_cache_mem(struct devmm_virt_com_heap *heap, uint64_t size, uint32_t memtype)
{
    if (heap->cur_cache_mem[memtype] < size) {
        DEVMM_DRV_ERR("Cache pool exhausted. (memtype=%u; cur_cache_mem=%llu; size=%llu; heap_idx=%u; "
                      "cur_alloc_cache_mem=%llu; cache_mem_thres=%llu; peak_alloc_cache_mem=%llu)\r\n",
                      memtype, heap->cur_cache_mem[memtype], size, heap->heap_idx, heap->cur_alloc_cache_mem[memtype],
                      heap->cache_mem_thres[memtype], heap->peak_alloc_cache_mem[memtype]);
        return;
    }

    heap->cur_cache_mem[memtype] -= size;
}

uint32_t devmm_get_module_id_by_advise(DVmem_advise advise)
{
    return ((advise >> DV_ADVISE_MODULE_ID_BIT) & DV_ADVISE_MODULE_ID_MASK);
}

void devmm_mem_mapped_size_inc(struct devmm_virt_com_heap *heap, uint64_t size)
{
    uint32_t mem_val = devmm_heap_sub_type_to_mem_val(heap->heap_sub_type);
    uint32_t page_type = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? DEVMM_HUGE_PAGE_TYPE : DEVMM_NORMAL_PAGE_TYPE;
    uint32_t phy_memtype = heap->heap_mem_type;
    uint32_t devid = devmm_heap_device_by_list_type(heap->heap_list_type);
    uint64_t inc_size = devmm_is_snapshot_state() ? 0 : size;
    struct svm_mem_stats_type type;

    if (heap->heap_sub_type != SUB_RESERVE_TYPE) {
        svm_mem_stats_type_pack(&type, mem_val, page_type, phy_memtype);
        svm_mapped_size_inc(&type, devid, size);
        heap->mapped_size += inc_size;
    }
}

void devmm_mem_mapped_size_dec(struct devmm_virt_com_heap *heap, uint64_t size)
{
    uint32_t mem_val = devmm_heap_sub_type_to_mem_val(heap->heap_sub_type);
    uint32_t page_type = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? DEVMM_HUGE_PAGE_TYPE : DEVMM_NORMAL_PAGE_TYPE;
    uint32_t phy_memtype = heap->heap_mem_type;
    uint32_t devid = devmm_heap_device_by_list_type(heap->heap_list_type);
    struct svm_mem_stats_type type;

    if (heap->heap_sub_type != SUB_RESERVE_TYPE) {
        svm_mem_stats_type_pack(&type, mem_val, page_type, phy_memtype);
        svm_mapped_size_dec(&type, devid, size);
        heap->mapped_size -= size;
    }
}

static void devmm_module_mem_stats_inc(struct devmm_virt_com_heap *heap, uint32_t module_id,
                                       struct devmm_rbtree_node *node)
{
    uint32_t mem_val = devmm_heap_sub_type_to_mem_val(heap->heap_sub_type);
    uint32_t page_type = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? DEVMM_HUGE_PAGE_TYPE : DEVMM_NORMAL_PAGE_TYPE;
    uint32_t phy_memtype = heap->heap_mem_type;
    uint32_t devid = devmm_heap_device_by_list_type(heap->heap_list_type);
    struct svm_mem_stats_type type;

    devmm_node_flag_set_module_id(&node->data.flag, module_id);
    devmm_node_flag_set_mem_val(&node->data.flag, mem_val);
    devmm_node_flag_set_phy_memtype(&node->data.flag, phy_memtype);
    devmm_node_flag_set_page_type(&node->data.flag, page_type);
    if ((mem_val != MEM_SVM_VAL) && (mem_val != MEM_HOST_VAL) && (mem_val != MEM_RESERVE_VAL)) {
        /* Reserve mem hasn't map, do not know devid */
        devmm_node_flag_set_devid(&node->data.flag, devid);
    }

    if (heap->heap_sub_type != SUB_RESERVE_TYPE) {
        svm_mem_stats_type_pack(&type, mem_val, page_type, phy_memtype);
        svm_module_alloced_size_inc(&type, devid, module_id, node->data.size);
    }
}

void devmm_module_mem_stats_dec(struct devmm_rbtree_node *node)
{
    uint32_t mem_val = devmm_node_flag_get_mem_val(node->data.flag);
    uint32_t page_type = devmm_node_flag_get_page_type(node->data.flag);
    uint32_t phy_memtype = devmm_node_flag_get_phy_memtype(node->data.flag);
    uint32_t module_id = devmm_node_flag_get_module_id(node->data.flag);
    uint32_t devid = devmm_node_flag_get_devid(node->data.flag);
    struct svm_mem_stats_type type;

    if (mem_val != MEM_RESERVE_VAL) {
        svm_mem_stats_type_pack(&type, mem_val, page_type, phy_memtype);
        svm_module_alloced_size_dec(&type, devid, module_id, node->data.size);
    }
}

STATIC void devmm_merge_unmap_free_node_to_va_node(uint64_t va, struct devmm_rbtree_node *node,
                                                   struct devmm_heap_rbtree *rbtree_queue)
{
    struct devmm_rbtree_node *node_tmp = NULL;

    /* try to merge va to node */
    node_tmp = devmm_rbtree_get_idle_va_node_in_range(va, rbtree_queue);
#ifndef UVM_OPEN
    if ((node_tmp != NULL) &&
        ((devmm_node_flag_is_first_va(node_tmp) && !devmm_node_flag_is_mapped(node_tmp)) || devmm_va_is_uvm(va))) {
        /* merge left node need update va */
        node->data.va = node->data.va > node_tmp->data.va ? node_tmp->data.va : node->data.va;
        node->data.size += node_tmp->data.size;
        node->data.total += node_tmp->data.total;
        (void)devmm_rbtree_erase_idle_va_tree(node_tmp, rbtree_queue);
        (void)devmm_rbtree_erase_idle_size_tree(node_tmp, rbtree_queue);
        devmm_free_rbtree_node(node_tmp, rbtree_queue);
    }
#else
    if ((node_tmp != NULL) && devmm_node_flag_is_first_va(node_tmp) && !devmm_node_flag_is_mapped(node_tmp)) {
        /* merge left node need update va */
        node->data.va = node->data.va > node_tmp->data.va ? node_tmp->data.va : node->data.va;
        node->data.size += node_tmp->data.size;
        node->data.total += node_tmp->data.total;
        (void)devmm_rbtree_erase_idle_va_tree(node_tmp, rbtree_queue);
        (void)devmm_rbtree_erase_idle_size_tree(node_tmp, rbtree_queue);
        devmm_free_rbtree_node(node_tmp, rbtree_queue);
    }
#endif
}

STATIC void devmm_merge_unmapped_free_node(struct devmm_rbtree_node *node, struct devmm_heap_rbtree *rbtree_queue)
{
    uint64_t size = node->data.size;
    uint64_t va = node->data.va;

    /* try to merge left node */
    devmm_merge_unmap_free_node_to_va_node(va - 1, node, rbtree_queue);
    /* try to merge right node */
    devmm_merge_unmap_free_node_to_va_node(va + size, node, rbtree_queue);
}

STATIC DVresult devmm_free_phymem_heap_oper(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node)
{
    int ret;

    /* ioctl to kernel to free pa */
    ret = heap->ops->heap_free(heap, node->data.va);
    if (ret != 0) {
        DEVMM_DRV_ERR("Heap ops failed. (ret=%d; va=0x%llx)\n", ret, node->data.va);
        return (DVresult)ret;
    }

    heap->sys_mem_freed_num++;

    return DRV_ERROR_NONE;
}

STATIC DVresult devmm_free_phymem_to_os(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node,
                                        uint64_t nocache_size)
{
    DVresult ret;
    (void)nocache_size;

    DEVMM_DRV_SWITCH("Free physical memory. (va=0x%llx; size=%llu; total=%llu)\r\n", node->data.va, node->data.size,
                     node->data.total);
    ret = devmm_free_phymem_heap_oper(heap, node);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    heap->sys_mem_freed += node->data.total;

    if (heap->is_base_heap == true) {
        /* base_heap (non-host-pin) never does mapped_size_inc, skip dec */
        /* host_base_heap without MAPPED_FLG: alloc op failed before inc, skip dec */
        if ((heap->is_host_pin == true) && devmm_node_flag_is_mapped(node)) {
            devmm_mem_mapped_size_dec(heap, node->data.total);
        }
    } else {
        devmm_mem_mapped_size_dec(heap, node->data.total);
    }

    devmm_node_clear_flag(node, DEVMM_NODE_MAPPED_FLG);
    return DRV_ERROR_NONE;
}

STATIC void devmm_try_merge_idle_unmap_tree(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node)
{
    /* try to merge node in free size tree */
    devmm_merge_unmapped_free_node(node, &heap->rbtree_queue);
    (void)devmm_rbtree_insert_idle_size_tree(node, &heap->rbtree_queue);
    (void)devmm_rbtree_insert_idle_va_tree(node, &heap->rbtree_queue);
}

STATIC void devmm_insert_to_idle_mapped_tree(struct devmm_rbtree_node *node, struct devmm_virt_com_heap *heap)
{
    (void)devmm_rbtree_insert_idle_mapped_tree(node, &heap->rbtree_queue);
    (void)devmm_rbtree_insert_idle_va_tree(node, &heap->rbtree_queue);
}

STATIC void devmm_erase_from_idle_mapped_tree(struct devmm_rbtree_node *node, struct devmm_virt_com_heap *heap)
{
    (void)devmm_rbtree_erase_idle_mapped_tree(node, &heap->rbtree_queue);
    (void)devmm_rbtree_erase_idle_va_tree(node, &heap->rbtree_queue);
}

STATIC DVresult devmm_shrink_updata_node_to_trees(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node,
                                                  uint32_t memtype)
{
    /* node in mapped tree may also in free tree */
    devmm_erase_from_idle_mapped_tree(node, heap);
    devmm_sub_cur_cache_mem(heap, node->data.size, memtype);
    devmm_try_merge_idle_unmap_tree(heap, node);

    return DRV_ERROR_NONE;
}

STATIC bool devmm_need_shrink_cache(struct devmm_virt_com_heap *heap, uint32_t memtype)
{
    /* judge contract_cache */
    if ((heap->cur_cache_mem[memtype] < (heap->cache_mem_thres[memtype] * 2ul)) &&         /* 2 just for twice size */
        (heap->cur_cache_mem[memtype] < (heap->cache_mem_thres[memtype] + 0x8000000ul))) { /* 0x8000000ul 128M more */
        return false;
    }
    return true;
}

STATIC void devmm_shrink_cache_force(struct devmm_virt_com_heap *heap)
{
    struct devmm_rbtree_node *node_mapped = NULL;
    uint64_t node_num, i;
    uint32_t j;

    for (j = 0; j < DEVMM_MAPPED_TREE_TYPE_MAX; j++) {
        node_num = devmm_rbtree_get_mapped_len(&heap->rbtree_queue, j);
        if (node_num == 0) {
            continue;
        }

        for (i = 0; i < node_num; i++) {
            DVresult ret;
            node_mapped = devmm_rbtree_get_full_mapped_node(&heap->rbtree_queue, j);
            if (node_mapped == NULL) {
                break;
            }
            DEVMM_DRV_SWITCH("Map node details. (heap_idx=%u; cur_alloc_cache_mem=%llu; "
                             "cur_cache_mem=%llu; cache_mem_thres=%llu; va=%llx; size=%llu; total=%llu)\r\n",
                             heap->heap_idx, heap->cur_alloc_cache_mem[j], heap->cur_cache_mem[j],
                             heap->cache_mem_thres[j], node_mapped->data.va, node_mapped->data.size,
                             node_mapped->data.total);

            /* node not all free, can not free */
            if (node_mapped->data.total != node_mapped->data.size) {
                break;
            }
            ret = devmm_free_phymem_to_os(heap, node_mapped, 0);
            if (ret != DRV_ERROR_NONE) {
                break;
            }
            ret = devmm_shrink_updata_node_to_trees(heap, node_mapped, j);
            if (ret != DRV_ERROR_NONE) {
                DEVMM_DRV_WARN("Free fail. (va=0x%llx; ret=%d)\n", node_mapped->data.va, ret);
                break;
            }
        }
    }
}

STATIC DVresult devmm_check_node_allowed_free(struct devmm_virt_com_heap *heap, uint32_t memtype,
                                              struct devmm_rbtree_node *node_mapped)
{
    if (heap->cur_cache_mem[memtype] < node_mapped->data.size) {
        DEVMM_DRV_ERR("Cache pool exhausted. (heap_idx=%u; memtype=%u; cur_alloc_cache_mem=%llu; "
                      "cur_cache_mem=%llu; cache_mem_thres=%llu; va=%llx; size=%llu; total=%llu)\r\n",
                      heap->heap_idx, memtype, heap->cur_alloc_cache_mem[memtype], heap->cur_cache_mem[memtype],
                      heap->cache_mem_thres[memtype], node_mapped->data.va, node_mapped->data.size,
                      node_mapped->data.total);
        return DRV_ERROR_INNER_ERR;
    }

    if ((heap->cur_cache_mem[memtype] - node_mapped->data.size) <= heap->cache_mem_thres[memtype]) {
        return DRV_ERROR_INNER_ERR;
    }

    /* node not all free, can not free */
    if (node_mapped->data.total != node_mapped->data.size) {
        return DRV_ERROR_INNER_ERR;
    }
    return DRV_ERROR_NONE;
}

static inline uint32_t memtype_to_mapped_tree_type(uint32_t memtype)
{
    return (memtype == DEVMM_MEM_RDONLY) ? DEVMM_MAPPED_RDONLY_TREE : DEVMM_MAPPED_RW_TREE;
}

STATIC void devmm_shrink_cache(struct devmm_virt_com_heap *heap, uint32_t memtype)
{
    uint32_t mapped_tree_type = memtype_to_mapped_tree_type(memtype);
    struct devmm_rbtree_node *node_mapped = NULL;
    uint64_t node_num, i;

    (void)pthread_mutex_lock(&heap->tree_lock);
    /* judge contract_cache */
    if (devmm_need_shrink_cache(heap, memtype) == false) {
        goto devmm_shrink_stop;
    }
    node_num = devmm_rbtree_get_mapped_len(&heap->rbtree_queue, mapped_tree_type);
    (void)pthread_mutex_unlock(&heap->tree_lock);

    for (i = 0; i < node_num; i++) {
        DVresult ret;
        (void)pthread_mutex_lock(&heap->tree_lock);
        node_mapped = devmm_rbtree_get_full_mapped_node(&heap->rbtree_queue, mapped_tree_type);
        if (node_mapped == NULL) {
            goto devmm_shrink_stop;
        }
        DEVMM_DRV_SWITCH("Map node details. (heap_idx=%u; cur_alloc_cache_mem=%llu; "
                         "cur_cache_mem=%llu; cache_mem_thres=%llu; va=%llx; size=%llu; total=%llu)\r\n",
                         heap->heap_idx, heap->cur_alloc_cache_mem, heap->cur_cache_mem, heap->cache_mem_thres,
                         node_mapped->data.va, node_mapped->data.size, node_mapped->data.total);

        ret = devmm_check_node_allowed_free(heap, memtype, node_mapped);
        if (ret != DRV_ERROR_NONE) {
            goto devmm_shrink_stop;
        }

        devmm_erase_from_idle_mapped_tree(node_mapped, heap);
        devmm_sub_cur_cache_mem(heap, node_mapped->data.size, memtype);
        (void)pthread_mutex_unlock(&heap->tree_lock);

        ret = devmm_free_phymem_to_os(heap, node_mapped, 0);
        if (ret != DRV_ERROR_NONE) {
            (void)pthread_mutex_lock(&heap->tree_lock);
            devmm_insert_to_idle_mapped_tree(node_mapped, heap);
            devmm_add_cur_cache_mem(heap, node_mapped->data.size, memtype);
            goto devmm_shrink_stop;
        }
        (void)pthread_mutex_lock(&heap->tree_lock);
        devmm_try_merge_idle_unmap_tree(heap, node_mapped);
        (void)pthread_mutex_unlock(&heap->tree_lock);
    }
    return;

devmm_shrink_stop:
    (void)pthread_mutex_unlock(&heap->tree_lock);
    return;
}

STATIC void devmm_segment_node(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *map_node,
                               uint64_t alloc_size, uint64_t remain_size, uint32_t memtype)
{
    struct devmm_rbtree_node *new_seg_node = NULL;

    new_seg_node = devmm_alloc_rbtree_node(&heap->rbtree_queue);
    if (new_seg_node == NULL) {
        DEVMM_DRV_ERR("Alloc new_seg_node failed. (heap_idx=%u)\n", heap->heap_idx);
        return;
    }

    devmm_assign_rbtree_node_data(map_node->data.va + alloc_size, remain_size, map_node->data.total, 0,
                                  map_node->data.advise, new_seg_node);
    devmm_node_set_flag(new_seg_node, map_node->data.flag);
    devmm_node_clear_flag(new_seg_node, DEVMM_NODE_FIRST_VA_FLG);
    devmm_insert_to_idle_mapped_tree(new_seg_node, heap);
    DEVMM_DRV_SWITCH("Insert seg node to DEVMM_IDLE_VA_TREE and DEVMM_IDLE_SIZE_TREE. (heap_idx=%u; "
                     "seg_node=0x%llx; (free size key)size=%llu; (free va key)va=0x%llx; flag=%u; total=%llu)\n",
                     heap->heap_idx, (uint64_t)new_seg_node, new_seg_node->data.size, new_seg_node->data.va,
                     new_seg_node->data.flag, new_seg_node->data.total);

    map_node->data.size -= remain_size;
    devmm_add_cur_cache_mem(heap, remain_size, memtype);
}

STATIC bool devmm_node_needs_to_seg(struct devmm_virt_com_heap *heap, uint64_t mapped_size, uint64_t remain_size,
                                    uint32_t memtype, bool is_nocache_node)
{
    uint64_t remain_precent;

    if (is_nocache_node) {
        return false;
    }

    if ((mapped_size == 0) || (remain_size == 0) ||
        ((memtype == DEVMM_MEM_NORMAL) && (mapped_size > heap->need_cache_thres[memtype]))) {
        return false;
    }
    if (memtype == DEVMM_MEM_RDONLY) {
        return true;
    }

    remain_precent = (uint64_t)remain_size * DEVMM_ONE_HUNDRED / mapped_size;
    if ((remain_size >= DEVMM_SEG_THRES_SIZE) && (remain_precent >= DEVMM_SEG_THRES_PERCENT)) { /* seg node */
        return true;
    }
    return false;
}

STATIC void devmm_update_idle_mapped_node(struct devmm_rbtree_node *node, struct devmm_virt_com_heap *heap,
                                          uint64_t alloc_size)
{
    node->data.size -= alloc_size;
    node->data.va += alloc_size;
    if (devmm_node_flag_is_first_va(node)) {
        devmm_node_clear_flag(node, DEVMM_NODE_FIRST_VA_FLG);
    }
    devmm_insert_to_idle_mapped_tree(node, heap);
}
#ifndef UVM_OPEN
STATIC DVresult devmm_alloc_node(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node, uint64_t map_size,
                                 DVmem_advise advise)
{
    virt_addr_t ret_val;

    ret_val = heap->ops->heap_alloc(heap, node->data.va, map_size, advise);
    if (ret_val < DEVMM_UVM_MEM_START) {
        DEVMM_DRV_ERR("Can not alloc virtual address. (va=0x%llx; ret_val=0x%lx)\n", node->data.va, ret_val);
        return ptr_to_errcode(ret_val);
    }

    heap->sys_mem_alloced += node->data.total;
    heap->sys_mem_alloced_num++;
    DEVMM_DRV_SWITCH("Ioctl to alloc success. (va=0x%llx; ret_val=0x%lx; heap_idx=%u)\n", node->data.va, ret_val,
                     heap->heap_idx);

    return DRV_ERROR_NONE;
}
#endif
STATIC DVresult devmm_map_node(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node, uint64_t map_size,
                               DVmem_advise advise)
{
    virt_addr_t ret_val;

    ret_val = heap->ops->heap_alloc(heap, node->data.va, map_size, advise);
    if ((ret_val < DEVMM_SVM_MEM_START && ret_val > DEVMM_HOST_PIN_END) || (ret_val < DEVMM_HOST_PIN_START)) {
        DEVMM_DRV_INFO("Can not alloc physical address. (va=0x%llx; ret_val=0x%lx)\n", node->data.va, ret_val);
        return ptr_to_errcode(ret_val);
    }

    heap->sys_mem_alloced += node->data.total;
    heap->sys_mem_alloced_num++;
    DEVMM_DRV_SWITCH("Ioctl to map success. (va=0x%llx; ret_val=0x%lx; heap_idx=%u)\n", node->data.va, ret_val,
                     heap->heap_idx);

    return DRV_ERROR_NONE;
}

STATIC DVresult devmm_alloc_from_mapped_node(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node,
                                             uint64_t alloc_size, DVmem_advise advise, uint32_t memtype)
{
    uint32_t module_id = devmm_get_module_id_by_advise(advise);
    struct devmm_rbtree_node *node_new = NULL;
    uint32_t flag = node->data.flag;
    uint64_t va = node->data.va;

    if (node->data.size > alloc_size) {
        node_new = devmm_alloc_rbtree_node(&heap->rbtree_queue);
        if (node_new == NULL) {
            DEVMM_DRV_ERR("Out of memory, malloc node_new fail.\n");
            return DRV_ERROR_OUT_OF_MEMORY;
        }
        /* update original node */
        devmm_erase_from_idle_mapped_tree(node, heap);
        devmm_update_idle_mapped_node(node, heap, alloc_size);
        devmm_assign_rbtree_node_data(va, alloc_size, node->data.total, flag, advise, node_new);
    } else if (node->data.size == alloc_size) { /* reuse node */
        devmm_erase_from_idle_mapped_tree(node, heap);
        node_new = node;
    } else {
        DEVMM_DRV_ERR("Node size abnormal. (size=%llu; alloc_size=%llu)\n", node->data.size, alloc_size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    devmm_sub_cur_cache_mem(heap, alloc_size, memtype);
    devmm_module_mem_stats_inc(heap, module_id, node_new);
    (void)devmm_rbtree_insert_alloced_tree(node_new, &heap->rbtree_queue);
    DEVMM_DRV_SWITCH("Insert alloc node to alloc tree. (heap_idx=%u; node=0x%llx; size=%llu; va=0x%llx; "
                     "flag=%u; total=%llu)\n",
                     heap->heap_idx, (uint64_t)node_new, node_new->data.size, node_new->data.va, node_new->data.flag,
                     node_new->data.total);
    devmm_add_cur_alloc_cache_mem(heap, node_new->data.size, memtype);

    return DRV_ERROR_NONE;
}

STATIC uint64_t devmm_alloc_get_map_size(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node,
                                         uint64_t alloc_size, uint32_t memtype)
{
    uint64_t align_size = align_up(alloc_size, heap->kernel_page_size);
    uint64_t map_size;

    if (memtype == DEVMM_MEM_RDONLY) {
        map_size = align_size;
    } else {
        /* Improve utilization of memory: in scenario DVPP, the image size is unchanged every time */
        map_size = (align_size < heap->map_size) ? (heap->map_size / align_size) * align_size : align_size;
    }
#ifndef UVM_OPEN
    if (heap->heap_sub_type == SUB_UVM_TYPE) {
        map_size = align_size;
    }
#endif
    /* map size larger than node size, map node size */
    map_size = (map_size > node->data.size) ? node->data.size : map_size;

    return map_size;
}

STATIC void devmm_update_unmapped_node(struct devmm_rbtree_node *node, struct devmm_virt_com_heap *heap, uint64_t va,
                                       uint64_t size, uint32_t flag)
{
    devmm_assign_rbtree_node_data(va, size, size, flag, 0, node);
    (void)devmm_rbtree_insert_idle_size_tree(node, &heap->rbtree_queue);
    (void)devmm_rbtree_insert_idle_va_tree(node, &heap->rbtree_queue);
}

STATIC uint32_t devmm_set_node_flag_from_advise(DVmem_advise advise)
{
    uint32_t node_flag = 0;
    if ((advise & DV_ADVISE_READONLY) != 0) {
        devmm_node_flag_set_readonly(&node_flag);
    }
    if ((advise & DV_ADVISE_NOCACHE) != 0) {
        devmm_node_flag_set_nocache(&node_flag);
    }
    return node_flag;
}
#ifndef UVM_OPEN
STATIC DVresult devmm_alloc_from_unmapped_node_uvm(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node,
                                                   uint64_t alloc_size_usr, DVmem_advise advise)
{
    struct devmm_rbtree_node *alloc_node = NULL;
    uint64_t va, alloc_size;
    int reuse_flag = 0;
    DVresult ret;

    alloc_size = align_up(alloc_size_usr, heap->kernel_page_size);
    va = node->data.va;
    DEVMM_DRV_SWITCH("Size info. (alloc_size=%llu)\n", alloc_size);

    (void)devmm_rbtree_erase_idle_size_tree(node, &heap->rbtree_queue);
    (void)devmm_rbtree_erase_idle_va_tree(node, &heap->rbtree_queue);
    if (node->data.size > alloc_size) { /* need to update original node */
        devmm_update_unmapped_node(node, heap, node->data.va + alloc_size, node->data.size - alloc_size,
                                   node->data.flag);
    } else if (node->data.size == alloc_size) { /* reuse original node as map node */
        alloc_node = node;
        alloc_node->data.advise = advise;
        reuse_flag = 1;
    } else { /* for code maintenance */
        DEVMM_DRV_ERR("Invalid node size check map size.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (alloc_node == NULL) {
        alloc_node = devmm_alloc_rbtree_node(&heap->rbtree_queue);
        if (alloc_node == NULL) {
            DEVMM_DRV_ERR("Out of memory, malloc alloc_node fail.\n");
            return DRV_ERROR_OUT_OF_MEMORY;
        }
        devmm_assign_rbtree_node_data(va, alloc_size, alloc_size, node->data.flag, advise, alloc_node);
    }

    ret = devmm_alloc_node(heap, alloc_node, alloc_size, advise);
    if (ret != DRV_ERROR_NONE) {
        if (reuse_flag == 0) {
            devmm_free_rbtree_node(alloc_node, &heap->rbtree_queue);
        }
        return ret;
    }
    (void)devmm_rbtree_insert_alloced_tree(alloc_node, &heap->rbtree_queue);

    DEVMM_DRV_SWITCH("Node info. (alloc_size=%llu; va=0x%llx; total=%llu; alloc_num=%llu)\n", alloc_node->data.size,
                     alloc_node->data.va, alloc_node->data.total, heap->sys_mem_alloced_num);
    return DRV_ERROR_NONE;
}
#endif

STATIC DVresult devmm_alloc_from_unmapped_node(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node,
                                               uint64_t alloc_size, DVmem_advise advise, uint32_t memtype)
{
    uint32_t module_id = devmm_get_module_id_by_advise(advise);
    struct devmm_rbtree_node *map_node = NULL;
    uint64_t map_size, remain_size, va;
    uint32_t node_flag;
    int reuse_flag = 0;
    DVresult ret;

    map_size = devmm_alloc_get_map_size(heap, node, alloc_size, memtype);
    remain_size = map_size - alloc_size;
    va = node->data.va;
    DEVMM_DRV_SWITCH("Size info. (map_size=%llu; remain_size=%llu)\n", map_size, remain_size);

    (void)devmm_rbtree_erase_idle_size_tree(node, &heap->rbtree_queue);
    (void)devmm_rbtree_erase_idle_va_tree(node, &heap->rbtree_queue);
    if (node->data.size > map_size) { /* need to update original node */
        devmm_update_unmapped_node(node, heap, node->data.va + map_size, node->data.size - map_size, node->data.flag);
    } else if (node->data.size == map_size) { /* reuse original node as map node */
        map_node = node;
        map_node->data.advise = advise;
        reuse_flag = 1;
    } else { /* for code maintenance */
        DEVMM_DRV_ERR("Invalid node size check map size.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (map_node == NULL) {
        map_node = devmm_alloc_rbtree_node(&heap->rbtree_queue);
        if (map_node == NULL) {
            DEVMM_DRV_ERR("Out of memory, malloc map_node fail.\n");
            return DRV_ERROR_OUT_OF_MEMORY;
        }
        devmm_assign_rbtree_node_data(va, map_size, map_size, node->data.flag, advise, map_node);
    }

    ret = devmm_map_node(heap, map_node, map_size, advise);
    if (ret != DRV_ERROR_NONE) {
        if (reuse_flag == 0) {
            devmm_free_rbtree_node(map_node, &heap->rbtree_queue);
        }
        return ret;
    }
    node_flag = devmm_set_node_flag_from_advise(advise);
    if ((heap->is_base_heap == true) && (heap->is_host_pin == true)) {
        /* host_base_heap: MAPPED_FLG is set in alloc ops after physical map succeeds */
        devmm_unmap_node_set_flag(map_node, node_flag);
    } else {
        devmm_unmap_node_set_flag(map_node, node_flag | DEVMM_NODE_MAPPED_FLG);
    }

    if (devmm_node_needs_to_seg(heap, map_size, remain_size, memtype, advise_is_nocache(advise)) == true) {
        devmm_segment_node(heap, map_node, alloc_size, remain_size, memtype);
    }
    devmm_add_cur_alloc_cache_mem(heap, map_node->data.size, memtype);
    if (heap->is_base_heap == false) {
        devmm_module_mem_stats_inc(heap, module_id, map_node);
    }
    (void)devmm_rbtree_insert_alloced_tree(map_node, &heap->rbtree_queue);
    DEVMM_DRV_SWITCH("Node info. (heap_idx=%u; memtype=%u; buff_size=%llu; cur_cache_mem=%llu; thres=%llu; "
                     "peak_alloc_cache_mem=%llu; alloc_size=%llu; va=0x%llx; flag=%u; total=%llu; alloc_num=%llu)\n",
                     heap->heap_idx, memtype, heap->cur_alloc_cache_mem[memtype], heap->cur_cache_mem[memtype],
                     heap->cache_mem_thres[memtype], heap->peak_alloc_cache_mem[memtype], map_node->data.size,
                     map_node->data.va, map_node->data.flag, map_node->data.total, heap->sys_mem_alloced_num);
    return DRV_ERROR_NONE;
}

static void devmm_separate_node_by_va(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node, uint64_t va)
{
    struct devmm_rbtree_node *new_node = NULL;
    uint64_t new_node_size, node_size;

    new_node = devmm_alloc_rbtree_node(&heap->rbtree_queue);
    if (new_node == NULL) {
        DEVMM_DRV_INFO("Out of memory, cannot malloc new_node.\n");
        return;
    }

    /* Use va as the dividing line to separate node. */
    (void)devmm_rbtree_erase_idle_size_tree(node, &heap->rbtree_queue);
    (void)devmm_rbtree_erase_idle_va_tree(node, &heap->rbtree_queue);

    new_node_size = va - node->data.va;
    devmm_update_unmapped_node(new_node, heap, node->data.va, new_node_size, node->data.flag);

    node_size = node->data.size - new_node_size;
    devmm_update_unmapped_node(node, heap, va, node_size, node->data.flag);
}

static struct devmm_rbtree_node *devmm_get_node_from_idle_va_tree(struct devmm_virt_com_heap *heap, size_t alloc_size,
                                                                  uint64_t va)
{
    struct devmm_rbtree_node *node = NULL;

    node = devmm_rbtree_get_idle_va_node_in_range(va, &heap->rbtree_queue);
    if (node == NULL) {
        node = devmm_rbtree_get_alloced_node_in_range(va, &heap->rbtree_queue);
        if (node == NULL) {
#ifndef EMU_ST
            DEVMM_DRV_INFO("Cannot find va in allocated tree. (va=0x%llx; alloc_size=%lu)\n", va, alloc_size);
#endif
        } else {
            DEVMM_DRV_INFO("Va is allocated. (va=0x%llx; alloc_size=%lu; node_va=0x%llx; node_size=%llu; total=%llu; "
                           "flag=0x%x; is_base_heap=%u)\n",
                           va, alloc_size, node->data.va, node->data.size, node->data.total, node->data.flag,
                           heap->is_base_heap);
        }
        return NULL;
    }

    if ((va + alloc_size) > (node->data.va + node->data.size)) {
        DEVMM_DRV_INFO("Alloc size too large. (va=0x%llx; alloc_size=%lu; node_va=0x%llx; node_size=%llu; total=%llu; "
                       "flag=0x%x)\n",
                       va, alloc_size, node->data.va, node->data.size, node->data.total, node->data.flag);
        return NULL;
    }

    if (node->data.va != va) {
        devmm_separate_node_by_va(heap, node, va);
    }
    return node;
}

static struct devmm_rbtree_node *_devmm_alloc_mem_get_node(struct devmm_virt_com_heap *heap, size_t alloc_size,
                                                           uint64_t va)
{
    return devmm_is_specified_va_alloc(va) ? devmm_get_node_from_idle_va_tree(heap, alloc_size, va) :
                                             devmm_rbtree_get_from_idle_size_tree(alloc_size, &heap->rbtree_queue);
}

STATIC struct devmm_rbtree_node *devmm_alloc_mem_get_node(struct devmm_virt_com_heap *heap, size_t alloc_size,
                                                          uint32_t mapped_tree_type, uint32_t memtype, uint64_t va)
{
    struct devmm_rbtree_node *node = NULL;

    if (alloc_size <= heap->need_cache_thres[memtype]) {
        /* try get node from mapped tree first; if cannot get node, rollback get node from idle size tree */
        node = devmm_rbtree_get_idle_mapped_node(alloc_size, &heap->rbtree_queue, mapped_tree_type);
        if (node != NULL) {
            return node;
        }
    }
    node = _devmm_alloc_mem_get_node(heap, alloc_size, va);
    if (node == NULL) {
        if (alloc_size <= heap->need_cache_thres[memtype]) {
            if (heap->is_limited == true) { /* record err */
                DEVMM_RUN_INFO("Out of virtual memory, please check memory usage. "
                               "(size=%lu; memtype=%u; heap_size=%llu; "
                               "need_cache_thres=%lu; heap_mem_alloced=%llu; freed=%llu; cur_cache=%llu)\n",
                               alloc_size, memtype, heap->heap_size, heap->need_cache_thres[memtype],
                               heap->sys_mem_alloced, heap->sys_mem_freed, heap->cur_cache_mem[memtype]);
            }
            return NULL;
        }
        if (heap->is_limited == true) {
            /* shrink the heap's cache forcibly, when the heap is not enough for nocache's allocation */
            devmm_shrink_cache_force(heap);
            node = _devmm_alloc_mem_get_node(heap, alloc_size, va);
            if (node == NULL) {
                if (devmm_is_specified_va_alloc(va) == false) {
                    DEVMM_RUN_INFO("Out of virtual memory to alloc large memory, please check memory usage. "
                                   "(size=%lu; memtype=%u; heap_size=%llu; "
                                   "need_cache_thres=%lu; heap_mem_alloced=%llu; freed=%llu; cur_cache=%llu)\n",
                                   alloc_size, memtype, heap->heap_size, heap->need_cache_thres[memtype],
                                   heap->sys_mem_alloced, heap->sys_mem_freed, heap->cur_cache_mem[memtype]);
                }
                return NULL;
            }
        }
    }

    return node;
}

STATIC void devmm_update_peak_cache_mem(struct devmm_virt_com_heap *heap, uint32_t mem_type)
{
    if (heap->peak_alloc_cache_mem[mem_type] < heap->cur_alloc_cache_mem[mem_type]) {
        heap->peak_alloc_cache_time[mem_type] = time(NULL);
        heap->peak_alloc_cache_mem[mem_type] = heap->cur_alloc_cache_mem[mem_type];
    }
}

DVresult devmm_alloc_mem(uint64_t *pp, size_t bytesize, DVmem_advise advise, struct devmm_virt_com_heap *heap)
{
    struct devmm_rbtree_node *node = NULL;
    uint32_t mapped_tree_type = advise_to_mapped_tree_type(advise);
    uint32_t memtype = advise_to_memtype(advise);
    uint64_t alloc_size, va;
    DVresult ret;

    if ((heap == NULL) || (bytesize > heap->heap_size) || (bytesize == 0)) {
        DEVMM_DRV_ERR("Heap is NULL or alloc memory too large. (bytesize=%lu)\n", bytesize);
        return DRV_ERROR_INVALID_VALUE;
    }

    alloc_size = align_up(bytesize, heap->chunk_size);
    (void)pthread_rwlock_rdlock(&heap->heap_rw_lock);
    (void)pthread_mutex_lock(&heap->tree_lock);
    node = devmm_alloc_mem_get_node(heap, alloc_size, mapped_tree_type, memtype, *pp);
    if (node == NULL) {
        ret = DRV_ERROR_OUT_OF_MEMORY;
        goto alloc_out;
    }

    va = node->data.va;
    DEVMM_DRV_SWITCH("Get node. (alloc_size=%llu; node=0x%llx; node_size=%llu; node_va=0x%llx)\n", alloc_size, node,
                     node->data.size, node->data.va);
    if (devmm_node_flag_is_mapped(node)) {
        ret = devmm_alloc_from_mapped_node(heap, node, alloc_size, advise, memtype);
    } else {
        ret = devmm_alloc_from_unmapped_node(heap, node, alloc_size, advise, memtype);
    }
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_INFO("Alloc memory completed. (ret=%d; bytesize=%lu; alloc_size=%llu)\n", ret, bytesize, alloc_size);
        goto alloc_out;
    }

    *pp = va;
    devmm_update_peak_cache_mem(heap, memtype);

alloc_out:
    (void)pthread_mutex_unlock(&heap->tree_lock);
    (void)pthread_rwlock_unlock(&heap->heap_rw_lock);

    return ret;
}

STATIC void devmm_erase_seg_node_from_rbtree(struct devmm_rbtree_node **node_addr,
                                             struct devmm_heap_rbtree *rbtree_queue)
{
    (void)devmm_rbtree_erase_idle_va_tree(*node_addr, rbtree_queue);
    (void)devmm_rbtree_erase_idle_mapped_tree(*node_addr, rbtree_queue);
    DEVMM_DRV_SWITCH("Erase node from free tree. (size=%llu; va=0x%llx; flag=%u; total=%llu)\n",
                     (*node_addr)->data.size, (*node_addr)->data.va, (*node_addr)->data.flag, (*node_addr)->data.total);

    devmm_free_rbtree_node(*node_addr, rbtree_queue);
}

STATIC void devmm_merge_mapped_free_node(uint64_t va, struct devmm_rbtree_node *node,
                                         struct devmm_heap_rbtree *rbtree_queue)
{
    struct devmm_rbtree_node *node_tmp = NULL;
    uint64_t size = node->data.size;

    if (devmm_node_flag_is_first_va(node)) { /* first node, just need to check its right node */
        DEVMM_DRV_SWITCH("Node is first virtual address, try to merge right node.\n");
        node_tmp = devmm_rbtree_get_idle_va_node_in_range(va + size, rbtree_queue);
        if ((node_tmp == NULL) || !devmm_node_flag_is_mapped(node_tmp) || devmm_node_flag_is_first_va(node_tmp)) {
            /* not found right node, maybe in use, not merge full. */
            return;
        }
        node->data.size += node_tmp->data.size;
        devmm_erase_seg_node_from_rbtree(&node_tmp, rbtree_queue);
        return;
    }

    /* given node is not first node, first try to merge left node */
    node_tmp = devmm_rbtree_get_idle_va_node_in_range(va - 1, rbtree_queue);
    if ((node_tmp != NULL) && devmm_node_flag_is_mapped(node_tmp)) { /* found left node */
        /* merge left node to the given node */
        node->data.va = node_tmp->data.va;
        node->data.flag = node_tmp->data.flag;
        node->data.size += node_tmp->data.size;
        devmm_erase_seg_node_from_rbtree(&node_tmp, rbtree_queue);
    }

    /* given node is not first node and not merge full, need to check right node */
    node_tmp = devmm_rbtree_get_idle_va_node_in_range(va + size, rbtree_queue);
    if ((node_tmp != NULL) && !devmm_node_flag_is_first_va(node_tmp) && devmm_node_flag_is_mapped(node_tmp)) {
        node->data.size += node_tmp->data.size;
        devmm_erase_seg_node_from_rbtree(&node_tmp, rbtree_queue);
    }
    return;
}

STATIC uint64_t devmm_get_cache_thres(struct devmm_virt_com_heap *heap, uint32_t memtype)
{
    uint64_t twice_cur_size;
    uint64_t thres_size;
    struct devmm_virt_heap_type heap_type;
    uint64_t need_cache_thres;

    heap_type.heap_type = heap->heap_type;
    heap_type.heap_list_type = heap->heap_list_type;
    heap_type.heap_sub_type = heap->heap_sub_type;
    heap_type.heap_mem_type = heap->heap_mem_type;

    need_cache_thres = devmm_virt_get_cache_size_by_heap_type(&heap_type);
    thres_size = (heap->peak_alloc_cache_mem[memtype] + heap->cur_alloc_cache_mem[memtype]) /
                 2;                                          /* 2 just divided by two */
    twice_cur_size = heap->cur_alloc_cache_mem[memtype] * 2; /* 2 just twice size */
    if ((thres_size > need_cache_thres) && (twice_cur_size > need_cache_thres)) {
        thres_size = (thres_size < twice_cur_size) ? thres_size : twice_cur_size;
    } else {
        thres_size = need_cache_thres;
    }
    return thres_size;
}

STATIC bool devmm_node_needs_try_shrink(struct devmm_virt_com_heap *heap, uint64_t size, uint32_t memtype)
{
    /* some node bigger than need_cache_thres */
    if (size > heap->need_cache_thres[memtype]) {
        return true;
    }
    DEVMM_DRV_SWITCH(
        "Node info. (heap_idx=%u; memtype=%u; allocated_size=%llu; cache=%llu; thres=%llu; peak=%llu; "
        "time=%lu; sys_mem_alloced_num=%llu; sys_mem_freed_num=%llu; sys_mem_alloced=%llu; sys_mem_freed=%llu)\n",
        heap->heap_idx, memtype, heap->cur_alloc_cache_mem[memtype], heap->cur_cache_mem[memtype],
        heap->cache_mem_thres[memtype], heap->peak_alloc_cache_mem[memtype], heap->peak_alloc_cache_time[memtype],
        heap->sys_mem_alloced_num, heap->sys_mem_freed_num, heap->sys_mem_alloced, heap->sys_mem_freed);

    return (heap->cur_cache_mem[memtype] > heap->cache_mem_thres[memtype]) ? true : false;
}

STATIC void devmm_update_cache_thres(struct devmm_virt_com_heap *heap, uint32_t memtype)
{
    time_t last_time = heap->peak_alloc_cache_time[memtype];
    time_t cur_time = time(NULL);
    uint64_t thres_size;

    if ((cur_time - last_time) > 1) {                         /* 1 one second */
        thres_size = heap->peak_alloc_cache_mem[memtype] / 8; /* peak bigger than cur 1/8 refresh cache thres */
        if ((heap->peak_alloc_cache_mem[memtype] - thres_size) > heap->cur_alloc_cache_mem[memtype]) {
            heap->peak_alloc_cache_mem[memtype] = heap->peak_alloc_cache_mem[memtype] - thres_size;
            heap->peak_alloc_cache_time[memtype] = cur_time;
        }
    }
    heap->cache_mem_thres[memtype] = devmm_get_cache_thres(heap, memtype);
}

STATIC DVresult devmm_free_cache_mem_process(uint64_t va, struct devmm_virt_com_heap *heap,
                                             struct devmm_rbtree_node *node, uint32_t memtype)
{
    bool is_merge_full;
    uint64_t free_size;
    bool try_shrink_cache = false;

    (void)pthread_mutex_lock(&heap->tree_lock);
    free_size = node->data.size;
    /* merge node */
    devmm_merge_mapped_free_node(va, node, &heap->rbtree_queue);

    DEVMM_DRV_SWITCH("Free node to free tree. (heap_idx=%u; node=0x%llx; size=%llu; "
                     "va=0x%llx; flag=%u; total=%llu)\n",
                     heap->heap_idx, (uint64_t)node, node->data.size, node->data.va, node->data.flag, node->data.total);

    devmm_sub_cur_alloc_cache_mem(heap, free_size, memtype);
    devmm_insert_to_idle_mapped_tree(node, heap);
    devmm_add_cur_cache_mem(heap, free_size, memtype);

    is_merge_full = node->data.size == node->data.total;
    if (is_merge_full == true) {
        devmm_update_cache_thres(heap, memtype);

        if (devmm_node_needs_try_shrink(heap, node->data.size, memtype) == true) {
            try_shrink_cache = true;
        }
    }
    (void)pthread_mutex_unlock(&heap->tree_lock);

    if (try_shrink_cache) {
        devmm_shrink_cache(heap, memtype);
    }

    return DRV_ERROR_NONE;
}

STATIC struct devmm_rbtree_node *devmm_get_and_erase_alloced_mem_node(struct devmm_virt_com_heap *heap, uint64_t va)
{
    struct devmm_rbtree_node *node = NULL;

    (void)pthread_mutex_lock(&heap->tree_lock);
    node = devmm_rbtree_get_alloced_node(va, &heap->rbtree_queue);
    if (node == NULL) {
        DEVMM_DRV_ERR("Virtual address is not allocated, please check. (va=0x%llx)\n", va);
        (void)pthread_mutex_unlock(&heap->tree_lock);
        return NULL;
    }

    if (heap->is_base_heap == false) {
        devmm_module_mem_stats_dec(node);
    }
    /* free virt addr */
    (void)devmm_rbtree_erase_alloced_tree(node, &heap->rbtree_queue);
    (void)pthread_mutex_unlock(&heap->tree_lock);

    return node;
}

STATIC DVresult devmm_free_nocache_mem_process(uint64_t va, struct devmm_virt_com_heap *heap,
                                               struct devmm_rbtree_node *node, uint32_t memtype)
{
    DVresult ret;

    DEVMM_DRV_SWITCH("Free memory. (va=0x%llx; size=%llu; total=%llu)\r\n", va, node->data.size, node->data.total);
    /* ioctl to kernel to free pa */
    ret = devmm_free_phymem_to_os(heap, node, node->data.size);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Free error. (va=0x%llx; size=%llu; total=%llu; ret=%d)\n", va, node->data.size, node->data.total,
                      ret);
        return ret;
    }

    (void)pthread_mutex_lock(&heap->tree_lock);
#ifndef UVM_OPEN
    if (!devmm_va_is_uvm(va)) {
        devmm_sub_cur_alloc_cache_mem(heap, node->data.size, memtype);
    }
#else
    devmm_sub_cur_alloc_cache_mem(heap, node->data.size, memtype);
#endif
    devmm_try_merge_idle_unmap_tree(heap, node);
    (void)pthread_mutex_unlock(&heap->tree_lock);

    return DRV_ERROR_NONE;
}

STATIC void devmm_rollback_mem_node_to_alloced_tree(struct devmm_virt_com_heap *heap, struct devmm_rbtree_node *node)
{
    uint32_t module_id = devmm_node_flag_get_module_id(node->data.flag);

    (void)pthread_mutex_lock(&heap->tree_lock);
    devmm_module_mem_stats_inc(heap, module_id, node);
    (void)devmm_rbtree_insert_alloced_tree(node, &heap->rbtree_queue);
    (void)pthread_mutex_unlock(&heap->tree_lock);
}

DVresult devmm_free_mem(uint64_t va, struct devmm_virt_com_heap *heap, uint64_t *free_len)
{
    struct devmm_rbtree_node *node = NULL;
    uint32_t memtype;
    DVresult ret;

    DEVMM_DRV_SWITCH("Free memory. (va=0x%llx)\n", va);
    if (heap == NULL) {
        DEVMM_DRV_ERR("Heap is NULL, is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    (void)pthread_rwlock_rdlock(&heap->heap_rw_lock);
    if (heap->heap_type == DEVMM_HEAP_IDLE) {
        (void)pthread_rwlock_unlock(&heap->heap_rw_lock);
        DEVMM_DRV_ERR("Heap is destroy, is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    node = devmm_get_and_erase_alloced_mem_node(heap, va);
    if (node == NULL) {
        (void)pthread_rwlock_unlock(&heap->heap_rw_lock);
        /* The log cannot be modified, because in the failure mode library. */
        DEVMM_DRV_ERR("Virtual address is not alloced, please check. (va=0x%llx)\n", va);
        return DRV_ERROR_INVALID_VALUE;
    }
    *free_len = node->data.size;
    memtype = devmm_node_flag_get_memtype(node->data.flag);
#ifndef UVM_OPEN
    if (((node->data.size == node->data.total) &&
         ((node->data.total > heap->need_cache_thres[memtype]) || devmm_node_flag_is_nocache(node->data.flag))) ||
        heap->is_uvm_heap) {
        ret = devmm_free_nocache_mem_process(va, heap, node, memtype);
        if (ret != DRV_ERROR_NONE) {
            devmm_rollback_mem_node_to_alloced_tree(heap, node);
        }
    } else {
        ret = devmm_free_cache_mem_process(va, heap, node, memtype);
    }
#else
    if ((node->data.size == node->data.total) &&
        ((node->data.total > heap->need_cache_thres[memtype]) || devmm_node_flag_is_nocache(node->data.flag))) {
        ret = devmm_free_nocache_mem_process(va, heap, node, memtype);
        if (ret != DRV_ERROR_NONE) {
            devmm_rollback_mem_node_to_alloced_tree(heap, node);
        }
    } else {
        ret = devmm_free_cache_mem_process(va, heap, node, memtype);
    }
#endif
    (void)pthread_rwlock_unlock(&heap->heap_rw_lock);
    return ret;
}

bool devmm_is_mem_allocated(uint64_t va, struct devmm_virt_com_heap *heap)
{
    struct devmm_rbtree_node *node = NULL;

    (void)pthread_rwlock_rdlock(&heap->heap_rw_lock);
    (void)pthread_mutex_lock(&heap->tree_lock);
    node = devmm_rbtree_get_alloced_node(va, &heap->rbtree_queue);
    (void)pthread_mutex_unlock(&heap->tree_lock);
    (void)pthread_rwlock_unlock(&heap->heap_rw_lock);

    return (node != NULL);
}

DVresult devmm_rbtree_insert_idle_mapped_tree(struct devmm_rbtree_node *rbtree_node,
                                              struct devmm_heap_rbtree *rbtree_queue)
{
    if (devmm_node_flag_is_readonly(rbtree_node->data.flag)) {
        return devmm_rbtree_insert_idle_readonly_mapped_tree(rbtree_node, rbtree_queue);
    } else {
        return devmm_rbtree_insert_idle_rw_mapped_tree(rbtree_node, rbtree_queue);
    }
}

DVresult devmm_rbtree_erase_idle_mapped_tree(struct devmm_rbtree_node *rbtree_node,
                                             struct devmm_heap_rbtree *rbtree_queue)
{
    if (devmm_node_flag_is_readonly(rbtree_node->data.flag)) {
        return devmm_rbtree_erase_idle_readonly_mapped_tree(rbtree_node, rbtree_queue);
    } else {
        return devmm_rbtree_erase_idle_rw_mapped_tree(rbtree_node, rbtree_queue);
    }
}

uint32_t devmm_virt_get_cache_size_by_heap_type(struct devmm_virt_heap_type *virt_heap_type)
{
    uint32_t heap_type = virt_heap_type->heap_type;
    uint32_t heap_sub_type = virt_heap_type->heap_sub_type;
    uint32_t heap_mem_type = virt_heap_type->heap_mem_type;

    if (heap_mem_type == DEVMM_TS_DDR_MEM) {
        return DEVMM_CACHE_TSDDR_THRES_SIZE;
    }

    if ((heap_sub_type == SUB_SVM_TYPE) || (heap_sub_type == SUB_RESERVE_TYPE)) {
        return 0;
    } else if (heap_sub_type == SUB_DVPP_TYPE) {
        return DEVMM_CACHE_MIN_THRES_SIZE;
    } else {
        if (heap_type == DEVMM_HEAP_HUGE_PAGE) {
            return DEVMM_CACHE_MIN_THRES_SIZE;
        } else {
            return DEVMM_CACHE_COMM_THRES_SIZE;
        }
    }
}

static DVdeviceptr devmm_alloc_from_mapped_tree(struct devmm_virt_com_heap *heap, size_t size, DVmem_advise advise,
                                                uint64_t va)
{
    (void)va;
    struct devmm_rbtree_node *node = NULL;
    uint32_t mapped_tree_type = advise_to_mapped_tree_type(advise);
    uint32_t memtype = advise_to_memtype(advise);
    DVdeviceptr ptr;
    DVresult ret;

    if (size > heap->need_cache_thres[memtype]) {
        return DEVMM_INVALID_STOP;
    }

    node = devmm_rbtree_get_idle_mapped_node(size, &heap->rbtree_queue, mapped_tree_type);
    if (node == NULL) {
        return DEVMM_OUT_OF_VIRT_MEM;
    }

    ptr = node->data.va;
    ret = devmm_alloc_from_mapped_node(heap, node, size, advise, memtype);
    if (ret != DRV_ERROR_NONE) {
        return DEVMM_OUT_OF_VIRT_MEM;
    }

    return ptr;
}

DVdeviceptr devmm_alloc_from_size_tree(struct devmm_virt_com_heap *heap, size_t size, DVmem_advise advise, uint64_t va)
{
    struct devmm_rbtree_node *node = NULL;
    DVdeviceptr ptr;
    DVresult ret;

    node = _devmm_alloc_mem_get_node(heap, size, va);
    if (node == NULL) {
        if (heap->is_limited == true) {
            /* shrink the heap's cache forcibly, when the heap is not enough for nocache's allocation */
            devmm_shrink_cache_force(heap);
            node = _devmm_alloc_mem_get_node(heap, size, va);
            if (node == NULL) {
                return DEVMM_OUT_OF_VIRT_MEM;
            }
        } else {
            return DEVMM_OUT_OF_VIRT_MEM;
        }
    }

    ptr = node->data.va;
#ifndef UVM_OPEN
    if (heap->heap_sub_type == SUB_UVM_TYPE) {
        ret = devmm_alloc_from_unmapped_node_uvm(heap, node, size, DEVMM_MEM_NORMAL);
    } else {
        ret = devmm_alloc_from_unmapped_node(heap, node, size, advise, advise_to_memtype(advise));
    }
#else
    ret = devmm_alloc_from_unmapped_node(heap, node, size, advise, advise_to_memtype(advise));
#endif
    if (ret != DRV_ERROR_NONE) {
        return errcode_to_ptr(ret, DEVMM_OUT_OF_PHYS_MEM);
    }
    if (va != 0) {
        heap->is_cache = false;
    }

    return ptr;
}

static DVdeviceptr (*alloc_from_tree[DEVMM_TREE_TYPE_MAX])(struct devmm_virt_com_heap *heap, size_t size,
                                                           DVmem_advise advise, uint64_t va) = {
    [DEVMM_IDLE_SIZE_TREE] = devmm_alloc_from_size_tree,
    [DEVMM_IDLE_MAPPED_TREE] = devmm_alloc_from_mapped_tree,
};

DVdeviceptr devmm_alloc_from_tree(struct devmm_virt_com_heap *heap, size_t bytesize, DVmem_advise advise,
                                  uint32_t tree_type, uint64_t va)
{
    uint64_t alloc_size = align_up(bytesize, heap->chunk_size);
    DVdeviceptr ptr = DEVMM_ERR_PTR;

    (void)pthread_rwlock_rdlock(&heap->heap_rw_lock);
    (void)pthread_mutex_lock(&heap->tree_lock);

    if (alloc_from_tree[tree_type] != NULL) {
        ptr = alloc_from_tree[tree_type](heap, alloc_size, advise, va);
        if (ptr_is_valid(ptr)) {
            devmm_update_peak_cache_mem(heap, advise_to_memtype(advise));
        }
    }

    (void)pthread_mutex_unlock(&heap->tree_lock);
    (void)pthread_rwlock_unlock(&heap->heap_rw_lock);
    return ptr;
}

int devmm_get_reserve_addr_info(uint64_t va, struct devmm_mem_info *mem_info)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    arg.data.resv_addr_info_query_para.va = va;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_RESERVE_ADDR_INFO_QUERY, &arg);
    if (ret == DRV_ERROR_NONE) {
        mem_info->start = arg.data.resv_addr_info_query_para.start;
        mem_info->end = arg.data.resv_addr_info_query_para.end;
        mem_info->module_id = arg.data.resv_addr_info_query_para.module_id;
        mem_info->devid = arg.data.resv_addr_info_query_para.devid;
    }

    return ret;
}

static void _devmm_get_addr_info(struct devmm_virt_com_heap *heap, uint64_t va, struct devmm_mem_info *mem_info)
{
    struct devmm_rbtree_node *node = NULL;

    (void)pthread_rwlock_rdlock(&heap->heap_rw_lock);
    (void)pthread_mutex_lock(&heap->tree_lock);

    node = devmm_rbtree_get_alloced_node_in_range(va, &heap->rbtree_queue);
    if (node != NULL) {
        mem_info->start = node->data.va;
        mem_info->end = node->data.va + node->data.size - 1;
        mem_info->module_id = devmm_node_flag_get_module_id(node->data.flag);
        mem_info->devid = devmm_node_flag_get_devid(node->data.flag);
    }

    (void)pthread_mutex_unlock(&heap->tree_lock);
    (void)pthread_rwlock_unlock(&heap->heap_rw_lock);
}

STATIC void devmm_get_addr_info(uint64_t va, struct devmm_mem_info *mem_info)
{
    struct devmm_virt_com_heap *heap = NULL;

    if (va >= DEVMM_MAX_DYN_ALLOC_BASE) {
        (void)devmm_get_reserve_addr_info(va, mem_info);
        return;
    }

    heap = devmm_va_to_heap(va);
    if ((heap == NULL) || (heap->heap_type == DEVMM_HEAP_IDLE)) {
        return;
    }

    if (devmm_virt_heap_is_primary(heap) && (heap->heap_sub_type != SUB_RESERVE_TYPE)) {
        mem_info->start = heap->start;
        mem_info->end = heap->end;
        mem_info->module_id = heap->module_id;
        mem_info->devid = heap->devid;
        return;
    }

    if (heap->heap_sub_type == SUB_RESERVE_TYPE) {
        (void)devmm_get_reserve_addr_info(va, mem_info);
    } else {
        _devmm_get_addr_info(heap, va, mem_info);
    }
}

void devmm_get_addr_module_id(uint64_t va, uint32_t *module_id, uint64_t *module_id_size)
{
    struct devmm_mem_info mem_info = {
        .start = 0, .end = 0, .module_id = SVM_INVALID_MODULE_ID, .devid = SVM_MAX_AGENT_NUM};

    devmm_get_addr_info(va, &mem_info);
    *module_id_size = sizeof(uint32_t);
    *module_id = mem_info.module_id;
}

void devmm_print_svm_va_info(uint64_t va, DVresult ret)
{
    struct devmm_mem_info mem_info = {
        .start = 0, .end = 0, .module_id = SVM_INVALID_MODULE_ID, .devid = SVM_MAX_AGENT_NUM};

    if ((ret == DRV_ERROR_NOT_SUPPORT) || (devmm_va_is_in_svm_range(va) == false)) {
        return;
    }

    devmm_get_addr_info(va, &mem_info);
    if (mem_info.start == 0) {
        DEVMM_DRV_ERR("Va is not allocated. (va=0x%llx)\n", va);
    } else {
        DEVMM_DRV_ERR("Va info. (va=0x%llx; start=0x%llx; end=0x%llx; module_name=%s; devid=%u)\n", va, mem_info.start,
                      mem_info.end, SVM_GET_MODULE_NAME(svm_module_name, mem_info.module_id), mem_info.devid);
    }
}

void devmm_rbtree_free_node_resources(struct devmm_rbtree_node *rbtree_node)
{
    if (devmm_node_flag_is_first_va(rbtree_node)) {
        if (devmm_node_flag_get_mem_val(rbtree_node->data.flag) == MEM_HOST_VAL) {
            devmm_host_pin_pre_register_release(rbtree_node->data.va);
        }
    }
}

void devmm_get_svm_va_info(uint64_t va, struct devmm_mem_info *mem_info)
{
    devmm_get_addr_info(va, mem_info);
}
