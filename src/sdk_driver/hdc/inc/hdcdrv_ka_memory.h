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

#ifndef _HDCDRV_KA_MEMORY_H_
#define _HDCDRV_KA_MEMORY_H_

#include "pbl/pbl_ka_memory.h"
#include "ascend_hal_define.h"

/*
    KA_SUB_MODULE_TYPE_0: for init/uninit when probe/remove or instance/uninstance
    KA_SUB_MODULE_TYPE_1: for normal channel, session link, process open/release
    KA_SUB_MODULE_TYPE_2: for fast channel(pcie) or mem_pool(ub)
    KA_SUB_MODULE_TYPE_3: for epoll
    KA_SUB_MODULE_TYPE_4: for sysfs
*/
#define hdcdrv_kvzalloc(size, flags, level) ka_kvzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_ka_kvfree(addr, level) ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))

#define hdcdrv_kmalloc(size, flags, level) ka_kmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_kzalloc(size, flags, level) ka_kzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_kfree(addr, level) ka_kfree(addr, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))

// replace vzalloc
#define hdcdrv_vzalloc(size, level) ka_vzalloc(size, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
// replace vmalloc
#define hdcdrv_vmalloc(size, gfp_mask, prot, level) \
    __ka_vmalloc(size, gfp_mask, prot, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_vfree(addr, level) ka_vfree(addr, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))

#define hdcdrv_alloc_pages(gfp_mask, order, level) \
    ka_alloc_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_free_pages(addr, order, level) ka_free_pages(addr, order, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))

#define hdcdrv_alloc_pages_node_ex(nid, gfp_mask, order, level) \
    ka_alloc_pages_node(nid, gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_free_pages_ex(page, order, level) \
    __ka_free_pages(page, order, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))

#define hdcdrv_kzalloc_node(size, flags, node, level) \
    ka_kzalloc_node(size, flags, node, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
// use hdcdrv_kfree for hdcdrv_kzalloc_node

#define hdcdrv_hugetlb_alloc_hugepage(nid, flag, level) \
    ka_hugetlb_alloc_hugepage(nid, flag, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_alloc_hugetlb_folio_size(nid, size, level) \
    ka_alloc_hugetlb_folio_size(nid, size, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))
#define hdcdrv_hugetlb_free_hugepage(page, level) \
    ka_hugetlb_free_hugepage(page, ka_get_module_id(HAL_MODULE_TYPE_HDC, level))

#endif /* _HDCDRV_KA_MEMORY_H_ */
