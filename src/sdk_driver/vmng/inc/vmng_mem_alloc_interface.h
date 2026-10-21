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

#ifndef VMNG_MEM_ALLOC_INTERFACE_H
#define VMNG_MEM_ALLOC_INTERFACE_H

#include "ka_memory_pub.h"
#include "pbl/pbl_ka_memory.h"
#include "ascend_hal_define.h"

#define vmng_kmalloc(size, flags) ka_kmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_kzalloc(size, flags) ka_kzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_kcalloc(size, flags) ka_kcalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_kfree(addr) ka_kfree(addr, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))

#define vmng_vzalloc(size) ka_vzalloc(size, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define __vmng_vmalloc(size, gfp_mask, prot) \
    __ka_vmalloc(size, gfp_mask, prot, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_vfree(addr) ka_vfree(addr, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))

#define vmng_get_free_pages(gfp_mask, order) \
    ka_get_free_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_alloc_pages(gfp_mask, order) \
    ka_alloc_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_free_pages(addr, order) \
    ka_free_pages(addr, order, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))

#define vmng_mg_sp_alloc_nodemask(size, sp_flags, spg_id, nodemask) \
    ka_mg_sp_alloc_nodemask(size, sp_flags, spg_id, nodemask,       \
                            ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_mg_sp_alloc(size, sp_flags, spg_id) \
    ka_mg_sp_alloc(size, sp_flags, spg_id, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_mg_sp_free(addr, id) ka_mg_sp_free(addr, id, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))

#define vmng_alloc_pages_exact(size, gfp_mask) \
    ka_alloc_pages_exact(size, gfp_mask, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_free_pages_exact(virt, size) \
    ka_free_pages_exact(virt, size, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))

#define vmng_alloc_pages_node(nid, gfp_mask, order) \
    ka_alloc_pages_node(nid, gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define __vmng_free_pages(page, order) \
    __ka_free_pages(page, order, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))

#define vmng_kvzalloc(size, flags) \
    ka_kvzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_kvmalloc_node(size, flags, node) \
    ka_kvmalloc_node(size, flags, node, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_kvmalloc(size, flags) \
    ka_kvmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_kvfree(addr) ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))

#define vmng_ka_dma_alloc_coherent(dev, size, dma_handle, gfp) \
    ka_dma_alloc_coherent(dev, size, dma_handle, gfp, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))
#define vmng_ka_dma_free_coherent(dev, size, cpu_addr, dma_handle) \
    ka_dma_free_coherent(dev, size, cpu_addr, dma_handle, ka_get_module_id(HAL_MODULE_TYPE_VMNG, KA_SUB_MODULE_TYPE_0))

#endif