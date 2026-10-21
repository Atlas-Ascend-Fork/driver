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
#ifndef APM_SLAB_H
#define APM_SLAB_H

#include "ka_fs_pub.h"

#include "pbl_ka_memory.h"
#include "ascend_hal_define.h"

#define apm_kmalloc(size, flags) ka_kmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))
#define apm_kzalloc(size, flags) ka_kzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))
#define apm_kcalloc(n, size, flags) \
    ka_kcalloc(n, size, flags, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))
#define apm_kfree(addr) ka_kfree(addr, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))

#define apm_vzalloc(size) ka_vzalloc(size, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))

#define apm_vmalloc(size, gfp_mask, prot) \
    __ka_vmalloc(size, gfp_mask, prot, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))

#define apm_vfree(addr) ka_vfree(addr, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))

#define apm_get_free_pages(gfp_mask, order) \
    ka_get_free_pages(gfp_mask, order, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))

#define apm_alloc_pages_exact(size, gfp_mask) \
    ka_alloc_pages_exact(size, gfp_mask, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))
#define apm_free_pages_exact(virt, size) \
    ka_free_pages_exact(virt, size, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))

#ifdef EMU_ST
/* UT: ka_kvzalloc/ka_kvfree route to static inline kvzalloc(kvfree) which stubs can't hook */
#define apm_kvzalloc(size, flags) apm_vzalloc(size)
#define apm_kvfree(addr) apm_vfree(addr)
#else
#define apm_kvzalloc(size, flags) ka_kvzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))
#define apm_kvfree(addr) ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))
#endif

#define apm_kvmalloc(size, flags) ka_kvmalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_0))

#define apm_kvmalloc_node(size, flags, node) \
    ka_kvmalloc_node(size, flags, node, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_1))
#define apm_kvfree_node(addr) ka_kvfree(addr, ka_get_module_id(HAL_MODULE_TYPE_APM, KA_SUB_MODULE_TYPE_1))
#endif
