/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdint.h>
#include <stdbool.h>
#include "devmm_virt_comm.h"
#include "svm_ioctl.h"
#include "devmm_virt_interface.h"
#include "devmm_svm_init.h"
#include "devmm_svm.h"
#include "devmm_virt_dvpp_heap.h"
#include "devmm_virt_read_only_heap.h"
#include "devmm_virt_com_heap.h"
#include "devmm_virt_base_heap.h"
#include "svm_mem_statistics.h"

STATIC virt_addr_t devmm_virt_base_heap_alloc(struct devmm_virt_com_heap *heap, virt_addr_t alloc_ptr,
                                              size_t alloc_size, DVmem_advise advise)
{
    (void)heap;
    (void)alloc_size;
    (void)advise;
    DEVMM_DRV_SWITCH("Base alloc. (alloc_ptr=0x%lx; alloc_size=%lu)\n", alloc_ptr, alloc_size);
    return alloc_ptr;
}

STATIC DVresult devmm_virt_base_heap_free(struct devmm_virt_com_heap *heap, virt_addr_t ptr)
{
    (void)heap;
    (void)ptr;
    DEVMM_DRV_SWITCH("Base free. (alloc_ptr=0x%lx; start=0x%lx; heap_size=%llu; heap_idx=%u)\n", ptr, heap->start,
                     heap->heap_size, heap->heap_idx);

    return DRV_ERROR_NONE;
}

STATIC struct devmm_com_heap_ops g_base_heap_ops = {devmm_virt_base_heap_alloc, devmm_virt_base_heap_free};

/* decouple here, call allocation submodule's interface */
DVresult devmm_virt_free_mem_to_base(struct devmm_virt_heap_mgmt *mgmt, virt_addr_t ptr)
{
    uint64_t free_len;
    if (ptr >= mgmt->start && ptr < mgmt->end) {
        return devmm_free_mem(ptr, &mgmt->heap_queue.base_heap, &free_len);
    }
    if (ptr >= mgmt->host_pin_start && ptr < mgmt->host_pin_end) {
        return devmm_free_mem(ptr, &mgmt->heap_queue.host_base_heap, &free_len);
    }
    return DRV_ERROR_PARA_ERROR;
}

virt_addr_t devmm_virt_alloc_mem_from_base(struct devmm_virt_heap_mgmt *mgmt, size_t alloc_size, DVmem_advise advise,
                                           virt_addr_t alloc_ptr)
{
    virt_addr_t va = ALIGN_DOWN(alloc_ptr, DEVMM_HEAP_SIZE);
    if (devmm_alloc_mem(&va, alloc_size, advise, &mgmt->heap_queue.base_heap) != DRV_ERROR_NONE) {
        return (alloc_ptr == 0) ? DEVMM_INVALID_STOP : DEVMM_ADDR_BUSY;
    }

    return va;
}

virt_addr_t devmm_virt_alloc_from_host_base(struct devmm_virt_heap_mgmt *mgmt, size_t alloc_size, DVmem_advise advise,
                                            virt_addr_t alloc_ptr)
{
    virt_addr_t va = ALIGN_DOWN(alloc_ptr, DEVMM_2M_PAGE_SIZE);
    size_t size = ALIGN_UP(alloc_size + (va - alloc_ptr), DEVMM_2M_PAGE_SIZE);
    if (devmm_is_host_pin_memory_map_failed()) {
        DEVMM_DRV_ERR("host pin memory map failed, cannot alloc.\n");
        return DEVMM_ADDR_BUSY;
    }
    if (devmm_alloc_mem(&va, size, advise, &mgmt->heap_queue.host_base_heap) != DRV_ERROR_NONE) {
        return (alloc_ptr == 0) ? DEVMM_INVALID_STOP : DEVMM_ADDR_BUSY;
    }

    return va;
}

STATIC struct devmm_virt_com_heap *devmm_virt_alloc_heap(struct devmm_virt_heap_mgmt *mgmt,
                                                         struct devmm_virt_heap_type *heap_type, virt_addr_t alloc_ptr,
                                                         size_t alloc_size, DVmem_advise advise)
{
    struct devmm_virt_com_heap *heap_set = NULL;
    uint32_t heap_idx;
    bool is_host_pin = devmm_is_in_host_pin_range(alloc_ptr);

    if ((is_host_pin) && (heap_type->heap_sub_type != SUB_RESERVE_TYPE)) {
        /* halMemAlloc uva: return host_base_heap directly */
        return &mgmt->heap_queue.host_base_heap;
    }

    /* reserve va in host_pin range goes through the same path as non-uva */
    heap_idx = devmm_va_to_heap_idx(mgmt, alloc_ptr, is_host_pin);
    heap_set = devmm_virt_get_heap_from_queue(mgmt, heap_idx, alloc_size, is_host_pin);
    if (heap_set == NULL) {
        DEVMM_DRV_ERR("Base alloc heap failed. (idx=%u; ptr=0x%llx; size=%lu)\n", heap_idx, alloc_ptr, alloc_size);
        return NULL;
    }
    devmm_virt_normal_heap_update_info(mgmt, heap_set, heap_type, NULL, alloc_size);
    heap_set->kernel_page_size = ((advise & DV_ADVISE_GIANTPAGE) != 0) ? DEVMM_GIANT_PAGE_SIZE :
                                                                         heap_set->kernel_page_size;
    return heap_set;
}

static void devmm_virt_free_heap(struct devmm_virt_heap_mgmt *mgmt, struct devmm_virt_com_heap *heap)
{
    struct devmm_virt_com_heap **heap_array = NULL;
    uint32_t i, heap_num;

    if (heap == &mgmt->heap_queue.host_base_heap) {
        return;
    }

    heap_num = (uint32_t)(heap->heap_size / DEVMM_HEAP_SIZE);
    heap_array = heap->is_host_pin ? mgmt->heap_queue.host_pin_heaps : mgmt->heap_queue.heaps;
    for (i = 0; i < heap_num; i++) {
        heap_array[(uint64_t)heap->heap_idx + i] = NULL;
    }

    (void)pthread_mutex_destroy(&heap->tree_lock);
    (void)pthread_rwlock_destroy(&heap->heap_rw_lock);
    free(heap);
}

static void devmm_primary_heap_module_mem_stats_inc(struct devmm_virt_com_heap *heap, uint32_t module_id, uint64_t size)
{
    uint32_t mem_val = devmm_heap_sub_type_to_mem_val(heap->heap_sub_type);
    uint32_t page_type = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? DEVMM_HUGE_PAGE_TYPE : DEVMM_NORMAL_PAGE_TYPE;
    uint32_t phy_memtype = heap->heap_mem_type;
    uint32_t devid = devmm_heap_device_by_list_type(heap->heap_list_type);
    struct svm_mem_stats_type type;

    svm_mem_stats_type_pack(&type, mem_val, page_type, phy_memtype);
    if (heap->heap_sub_type != SUB_RESERVE_TYPE) {
        svm_module_alloced_size_inc(&type, devid, module_id, size);
        heap->module_id = module_id;
        heap->devid = devid;
    }
}

void devmm_primary_heap_module_mem_stats_dec(struct devmm_virt_com_heap *heap, uint64_t size)
{
    uint32_t mem_val = devmm_heap_sub_type_to_mem_val(heap->heap_sub_type);
    uint32_t page_type = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? DEVMM_HUGE_PAGE_TYPE : DEVMM_NORMAL_PAGE_TYPE;
    uint32_t phy_memtype = heap->heap_mem_type;
    uint32_t devid = devmm_heap_device_by_list_type(heap->heap_list_type);
    uint32_t module_id = heap->module_id;
    struct svm_mem_stats_type type;

    svm_mem_stats_type_pack(&type, mem_val, page_type, phy_memtype);
    /* heap->module_id is for large heap (>=512M) */
    if ((heap->heap_sub_type != SUB_RESERVE_TYPE) && (module_id < SVM_MAX_MODULE_ID)) {
        svm_module_alloced_size_dec(&type, devid, module_id, size);
        if (heap->start != DEVMM_HOST_PIN_START) { /* uva alloc mem from host_base_heap, need to dec size */
            heap->module_id = SVM_MAX_MODULE_ID;
        }
    }
}

STATIC virt_addr_t devmm_virt_set_alloced_mem_struct(struct devmm_virt_heap_mgmt *mgmt, virt_addr_t alloc_ptr,
                                                     size_t alloc_size, struct devmm_virt_heap_type *heap_type,
                                                     DVmem_advise advise)
{
    uint32_t module_id = devmm_get_module_id_by_advise(advise);
    struct devmm_heap_list *heap_list = NULL;
    struct devmm_virt_com_heap *heap = NULL;
    size_t real_alloc_size;
    virt_addr_t ret_ptr;
    bool is_reserve_va = (heap_type->heap_sub_type == SUB_RESERVE_TYPE);
    bool need_enable_and_list = ((advise & DV_ADVISE_HOST_UVA) == 0) || (is_reserve_va);
    DVresult ret;

    if (devmm_get_heap_list_by_type(mgmt, heap_type, &heap_list) != DRV_ERROR_NONE) {
        return DEVMM_INVALID_STOP;
    }

    /* alloc large mem para is addr of heap_type */
    real_alloc_size = align_up(alloc_size, DEVMM_HEAP_SIZE);
    heap = devmm_virt_alloc_heap(mgmt, heap_type, alloc_ptr, real_alloc_size, advise);
    if (heap == NULL) {
        DEVMM_DRV_ERR("Devmm alloc heap failed. (alloc_ptr=0x%llx; alloc_size=%lu)\n", alloc_ptr, real_alloc_size);
        return DEVMM_INVALID_STOP;
    }

    if (need_enable_and_list) {
        ret = devmm_ioctl_enable_heap(heap->heap_idx, heap_type->heap_type, heap_type->heap_sub_type, heap->heap_size,
                                      heap_type->heap_list_type, heap->is_host_pin);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Devmm update heap failed. (alloc_ptr=0x%llx; alloc_size=%lu)\n", alloc_ptr, real_alloc_size);
            devmm_virt_free_heap(mgmt, heap);
            return DEVMM_INVALID_STOP;
        }
        real_alloc_size = (heap->kernel_page_size == DEVMM_GIANT_PAGE_SIZE) ?
                              align_up(alloc_size, heap->kernel_page_size) :
                              alloc_size;
    } else {
        real_alloc_size = align_up(alloc_size, heap->chunk_size);
    }

    heap->advise = advise;
    ret_ptr = devmm_virt_heap_alloc_ops(heap, alloc_ptr, real_alloc_size, advise);
    if ((ret_ptr < DEVMM_SVM_MEM_START) && (!devmm_is_in_host_pin_range(ret_ptr))) {
        DEVMM_RUN_INFO("Alloc ptr completed. (ret_ptr=0x%lx; alloc_ptr=0x%llx; alloc_size=%lu; advise=%u)\n", ret_ptr,
                       alloc_ptr, real_alloc_size, advise);
        if (need_enable_and_list) {
            (void)devmm_ioctl_disable_heap(heap->heap_idx, heap->heap_type, heap->heap_sub_type, heap->heap_size,
                                           heap->is_host_pin);
            devmm_virt_free_heap(mgmt, heap);
        }
        return ret_ptr;
    }

    devmm_primary_heap_module_mem_stats_inc(heap, module_id, real_alloc_size);
    if (need_enable_and_list) {
        (void)pthread_rwlock_wrlock(&heap_list->list_lock);
        devmm_virt_list_add(&heap->list, &heap_list->heap_list);
        heap_list->heap_cnt++;
        (void)__sync_fetch_and_add(&heap_list->version, 1);
        (void)pthread_rwlock_unlock(&heap_list->list_lock);
    }
    DEVMM_DRV_SWITCH("Devmm alloc heap. (ret_ptr=0x%lx; alloc_ptr=0x%lx; alloc_size=%lu; real_alloc_size=%lu)\n",
                     ret_ptr, alloc_ptr, alloc_size, real_alloc_size);
    return ret_ptr;
}

virt_addr_t devmm_alloc_from_base_heap(struct devmm_virt_heap_mgmt *mgmt, size_t alloc_size,
                                       struct devmm_virt_heap_type *heap_type, DVmem_advise advise, virt_addr_t va)
{
    virt_addr_t alloc_ptr, ret_ptr;

    if ((advise & DV_ADVISE_HOST_UVA) != 0) {
        alloc_ptr = devmm_virt_alloc_from_host_base(mgmt, alloc_size, 0, va);
    } else {
        alloc_ptr = devmm_virt_alloc_mem_from_base(mgmt, alloc_size, 0, va);
    }
    if (alloc_ptr < DEVMM_HOST_PIN_START) {
        if (devmm_is_specified_va_alloc(va) == false) {
            DEVMM_RUN_INFO("Alloc memory from base heap not success. (alloc_ptr=0x%lx; alloc_size=%lu; va=0x%llx)\n",
                           alloc_ptr, alloc_size, va);
        }
        return alloc_ptr;
    }
    ret_ptr = devmm_virt_set_alloced_mem_struct(mgmt, alloc_ptr, alloc_size, heap_type, advise);
    if (ret_ptr < DEVMM_SVM_MEM_START && !devmm_is_in_host_pin_range(ret_ptr)) {
        DEVMM_RUN_INFO("Can not alloc physical memory from base heap. "
                       "(ret_ptr=0x%lx; va=0x%lx; alloc_size=%lu; "
                       "va=0x%llx)\n",
                       ret_ptr, alloc_ptr, alloc_size, va);
        (void)devmm_virt_free_mem_to_base(mgmt, alloc_ptr);
    }

    return ret_ptr;
}

/* check whether ptr is legally assigned from base_heap */
STATIC DVresult devmm_virt_check_va_alloced_from_base(struct devmm_virt_com_heap *heap, virt_addr_t ptr)
{
    if (ptr != heap->start) {
#ifndef EMU_ST
        DEVMM_DRV_ERR("Not allocated from base by user. (ptr=0x%lx; heap start=0x%lx)\n", ptr, heap->start);
#endif
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static DVresult devmm_free_to_host_pin_heap(struct devmm_virt_heap_mgmt *mgmt, struct devmm_virt_com_heap *heap,
                                            virt_addr_t ptr)
{
    DVresult ret;
    uint64_t free_len;

    if (!devmm_is_mem_allocated(ptr, heap)) {
        DEVMM_DRV_ERR("host pin memory is not allocated(ptr=0x%lx)\n", ptr);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (devmm_virt_heap_free_ops(heap, ptr) != 0) {
        DEVMM_DRV_ERR("Free ptr error. (ptr=0x%lx)\n", ptr);
        return DRV_ERROR_IOCRL_FAIL;
    }

    ret = devmm_free_mem(ptr, &mgmt->heap_queue.host_base_heap, &free_len);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("devmm_free_mem ptr error. (ptr=0x%lx)\n", ptr);
        return ret;
    }

    devmm_primary_heap_module_mem_stats_dec(heap, free_len);
    return DRV_ERROR_NONE;
}

DVresult devmm_free_to_base_heap(struct devmm_virt_heap_mgmt *mgmt, struct devmm_virt_com_heap *heap, virt_addr_t ptr)
{
    struct devmm_heap_list *heap_list = NULL;
    struct devmm_virt_heap_type heap_type;

    if (heap == &mgmt->heap_queue.host_base_heap) {
        return devmm_free_to_host_pin_heap(mgmt, heap, ptr);
    }

    if ((ptr == DEVMM_SVM_MEM_START) || ((ptr & (mgmt->heap_queue.base_heap.chunk_size - 1)) != 0) ||
        (devmm_virt_check_va_alloced_from_base(heap, ptr) != 0)) {
        DEVMM_DRV_ERR("Ptr wasn't allocated by user. (ptr=0x%lx)\n", ptr);
        return DRV_ERROR_INVALID_VALUE;
    }

    heap_type.heap_type = heap->heap_type;
    heap_type.heap_list_type = heap->heap_list_type;
    heap_type.heap_sub_type = heap->heap_sub_type;
    heap_type.heap_mem_type = heap->heap_mem_type;

    if (devmm_get_heap_list_by_type(mgmt, &heap_type, &heap_list) != DRV_ERROR_NONE) {
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_rwlock_wrlock(&heap_list->list_lock);
    if (devmm_virt_list_empty_careful(&heap->list)) {
        (void)pthread_rwlock_unlock(&heap_list->list_lock);
        DEVMM_DRV_ERR("Ptr wasn't allocated by user. (ptr=0x%lx)\n", ptr);
        return DRV_ERROR_INVALID_VALUE;
    }
    devmm_virt_list_del_init(&heap->list);
    heap_list->heap_cnt--;
    (void)__sync_fetch_and_add(&heap_list->version, 1);
    (void)pthread_rwlock_unlock(&heap_list->list_lock);

    if (devmm_virt_heap_free_ops(heap, ptr) != 0) {
        DEVMM_DRV_ERR("Free ptr error. (ptr=0x%lx)\n", ptr);
        goto heap_list_add;
    }
    if (devmm_virt_destroy_heap(mgmt, heap, true) != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Destroy ptr error. (ptr=0x%lx)\n", ptr);
        goto heap_list_add;
    }
    return DRV_ERROR_NONE;
heap_list_add:
    (void)pthread_rwlock_wrlock(&heap_list->list_lock);
    devmm_virt_list_add(&heap->list, &heap_list->heap_list);
    heap_list->heap_cnt++;
    (void)pthread_rwlock_unlock(&heap_list->list_lock);
    return DRV_ERROR_IOCRL_FAIL;
}

static inline size_t devmm_virt_get_base_heap_reserve_size(void)
{
    return DEVMM_DVPP_HEAP_TOTAL_SIZE + DEVMM_READ_ONLY_HEAP_TOTAL_SIZE + DEVMM_DEV_READ_ONLY_HEAP_TOTAL_SIZE;
}

DVresult devmm_virt_init_base_heap(struct devmm_virt_heap_mgmt *mgmt)
{
    struct devmm_virt_heap_type heap_type = {0};
    struct devmm_virt_heap_para heap_info;
    struct devmm_virt_com_heap *heap = NULL;
    virt_addr_t reserve_ptr = 0;
    DVresult ret;
    int i;

    heap = &mgmt->heap_queue.base_heap;
    heap_info.start = DEVMM_SVM_MEM_START;
    heap_info.heap_size = DEVMM_MAX_MAPPED_RANGE;
    heap_info.page_size = DEVMM_HEAP_SIZE;
    heap_info.kernel_page_size = 0; /* not used in base heap */
    heap_info.map_size = 0;         /* not used in base heap */
    for (i = 0; i < (int)DEVMM_MEMTYPE_MAX; i++) {
        heap_info.need_cache_thres[i] = 0; /* not used in base heap */
    }
    heap_info.is_limited = true; /* means base heap can not be expanded */
    heap_info.is_host_pin = false;
    heap_info.is_base_heap = true;
#ifndef UVM_OPEN
    heap_info.is_uvm_heap = false;
#endif
    devmm_virt_status_init(heap);
    ret = devmm_virt_init_com_base_heap(heap, &heap_type, &g_base_heap_ops, &heap_info);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Init com base heap failed.\n");
        return ret;
    }

    heap = &mgmt->heap_queue.host_base_heap;
    heap_info.start = DEVMM_HOST_PIN_START;
    heap_info.heap_size = DEVMM_HOST_PIN_SIZE;
    heap_info.page_size = DEVMM_2M_PAGE_SIZE;
    heap_info.is_host_pin = true;
    devmm_virt_status_init(heap);

    heap_type.heap_type = DEVMM_HEAP_PINNED_HOST;
    heap_type.heap_list_type = HOST_LIST;
    heap_type.heap_sub_type = SUB_HOST_TYPE;
    heap_type.heap_mem_type = DEVMM_DDR_MEM;
    ret = devmm_virt_init_com_base_heap(heap, &heap_type, &g_base_heap_ops, &heap_info);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Init com host pin base heap failed.\n");
        return ret;
    }

    /* alloc max device reserve heap */
    if (!devmm_is_snapshot_state()) {
        reserve_ptr = devmm_virt_alloc_mem_from_base(mgmt, devmm_virt_get_base_heap_reserve_size(), 0, reserve_ptr);
        if (reserve_ptr != DEVMM_SVM_MEM_START) {
            DEVMM_DRV_ERR("Alloc for reserve failed. (reserve_ptr=0x%lx)\n", reserve_ptr);
            return DRV_ERROR_INNER_ERR;
        }
    }

    DEVMM_DRV_SWITCH("Alloc for reserve succeeded. (reserve_ptr=0x%lx; reserve_heaps_total_size=%llu; snapshot=%u)\n",
                     reserve_ptr, devmm_virt_get_base_heap_reserve_size(), devmm_is_snapshot_state());
    return DRV_ERROR_NONE;
}
