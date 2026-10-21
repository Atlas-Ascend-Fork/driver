/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ascend_hal.h"
#include "devmm_virt_com_heap.h"
#include "devmm_virt_comm.h"
#include "devmm_virt_interface.h"
#include "devmm_svm_init.h"
#include "devmm_virt_soma_heap.h"

STATIC struct devmm_com_heap_ops g_soma_heap_ops = {NULL, NULL};

static void devmm_fill_soma_heap_info(struct devmm_virt_heap_para *heap_info)
{
    heap_info->start = DEVMM_SOMA_MEM_START;
    heap_info->heap_size = DEVMM_SOMA_MEM_SIZE;
    heap_info->page_size = DEVMM_SOMA_PAGE_SIZE;
    heap_info->kernel_page_size = DEVMM_SOMA_PAGE_SIZE;
    heap_info->map_size = 0;

    for (int i = 0; i < (int)DEVMM_MEMTYPE_MAX; i++) {
        heap_info->need_cache_thres[i] = 0;
    }

    heap_info->is_limited = true;
    heap_info->is_host_pin = false;
    heap_info->is_base_heap = false;
    heap_info->is_soma_heap = true;
}

DVresult devmm_virt_init_soma_heap(struct devmm_virt_heap_mgmt *mgmt)
{
    struct devmm_virt_heap_type heap_type = {0};
    struct devmm_virt_heap_para heap_info;
    struct devmm_virt_com_heap *heap = NULL;
    DVresult ret;

    heap = &mgmt->heap_queue.soma_heap;
    devmm_fill_soma_heap_info(&heap_info);

    heap_type.heap_list_type = SOMA_LIST;
    heap_type.heap_sub_type = SUB_SOMA_TYPE;
    heap_type.heap_mem_type = DEVMM_SOMA_MEM;
    heap_type.heap_type = DEVMM_HEAP_HUGE_PAGE;

    devmm_virt_status_init(heap);
    ret = devmm_virt_init_com_base_heap(heap, &heap_type, &g_soma_heap_ops, &heap_info);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Init com soma heap failed.\n");
        return ret;
    }

    return DRV_ERROR_NONE;
}