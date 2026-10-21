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
#ifndef RMO_SLAB_H
#define RMO_SLAB_H

#include "ka_fs_pub.h"
#include "ka_memory_pub.h"
#include "pbl_ka_memory.h"
#include "ascend_hal_define.h"

#define rmo_kmalloc(size, flags) \
    ka_kmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))
#define rmo_kzalloc(size, flags) \
    ka_kzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))
#define rmo_kcalloc(n, size, flags) \
    ka_kcalloc(n, size, flags, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))
#define rmo_kfree(addr) ka_kfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))

#define rmo_vzalloc(size) ka_vzalloc(size, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))

#define __rmo_vmalloc(size, gfp_mask, prot) \
    __ka_vmalloc(size, gfp_mask, prot, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))

#define rmo_vfree(addr) ka_vfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))

#define rmo_get_free_pages(gfp_mask, order) \
    ka_get_free_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))
#define rmo_alloc_pages(gfp_mask, order) \
    ka_alloc_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))
#define rmo_free_pages(addr, order) \
    ka_free_pages(addr, order, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))

#define rmo_alloc_pages_exact(size, gfp_mask) \
    ka_alloc_pages_exact(size, gfp_mask, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))
#define rmo_free_pages_exact(virt, size) \
    ka_free_pages_exact(virt, size, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))

#define rmo_kvzalloc(size, flags) \
    ka_kvzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))
#define rmo_kvmalloc(size, flags) \
    ka_kvmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))
#define rmo_kvfree(addr) ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_DP_PROC_MNG, KA_SUB_MODULE_TYPE_0))

#endif
