/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef UVM_OPEN
#include <stdint.h>
#include <stdbool.h>
#include <sys/ioctl.h>

#include "devmm_virt_comm.h"
#include "svm_ioctl.h"
#include "devmm_virt_interface.h"
#include "devmm_svm_init.h"
#include "devmm_svm.h"
#include "devmm_virt_com_heap.h"
#include "svm_mem_statistics.h"
#include "devmm_virt_uvm_heap.h"
STATIC DVresult devmm_uvm_ioctl_alloc(DVdeviceptr p, size_t size)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.data.alloc_uvm_para.p = p;
    arg.data.alloc_uvm_para.size = size;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_UVM_ALLOC, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Ioctl alloc error. (ret=%d; size=%lu; ptr=0x%llx)\n", ret, size, p);
        return ret;
    }

    return DRV_ERROR_NONE;
}
STATIC virt_addr_t devmm_virt_heap_alloc_uvm(struct devmm_virt_com_heap *heap, virt_addr_t ret_val, size_t alloc_size,
                                             DVmem_advise advise)
{
    (void)advise;
    if (devmm_uvm_ioctl_alloc(ret_val, alloc_size) != DRV_ERROR_NONE) {
        return DEVMM_INVALID_STOP;
    }

    devmm_mem_mapped_size_inc(heap, alloc_size);
    return ret_val;
}

DVresult devmm_virt_heap_free_pages_uvm(struct devmm_virt_com_heap *heap, virt_addr_t ptr)
{
    DVresult ret;
    ret = devmm_ioctl_free_pages(ptr);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Devmm_ioctl_free failed. (ptr=0x%llx; heap_type=%u)\n", ptr, heap->heap_type);
        return ret;
    }
    return DRV_ERROR_NONE;
}

STATIC struct devmm_com_heap_ops g_uvm_heap_ops = {devmm_virt_heap_alloc_uvm, devmm_virt_heap_free_pages_uvm};

DVresult devmm_virt_init_uvm_heap(struct devmm_virt_heap_mgmt *mgmt)
{
    struct devmm_virt_heap_type heap_type = {0};
    struct devmm_virt_heap_para heap_info;
    struct devmm_virt_com_heap *heap = NULL;
    DVresult ret;
    int i;

    heap = &mgmt->heap_queue.uvm_heap;
    heap_info.start = DEVMM_UVM_MEM_START;
    heap_info.heap_size = DEVMM_UVM_MEM_SIZE;
    heap_info.page_size = DEVMM_UVM_PAGE_SIZE;
    heap_info.kernel_page_size = DEVMM_UVM_PAGE_SIZE; /* UVM use 2M page */
    heap_info.map_size = 0;
    for (i = 0; i < (int)DEVMM_MEMTYPE_MAX; i++) {
        heap_info.need_cache_thres[i] = 0; /* not used in uvm heap */
    }
    heap_info.is_limited = true; /* means base heap can not be expanded */
    heap_info.is_host_pin = false;
    heap_info.is_base_heap = false;
    heap_info.is_uvm_heap = true;

    heap_type.heap_list_type = UVM_LIST;
    heap_type.heap_sub_type = SUB_UVM_TYPE;
    heap_type.heap_mem_type = DEVMM_UVM_MEM;
    heap_type.heap_type = DEVMM_HEAP_HUGE_PAGE;

    devmm_virt_status_init(heap);
    ret = devmm_virt_init_com_base_heap(heap, &heap_type, &g_uvm_heap_ops, &heap_info);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Init com uvm heap failed.\n");
        return ret;
    }

    return DRV_ERROR_NONE;
}

#endif