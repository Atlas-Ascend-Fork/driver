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

#ifndef ASDRV_UB_MEM_ALLOC_H
#define ASDRV_UB_MEM_ALLOC_H

#include "pbl/pbl_ka_memory.h"
#include "ascend_hal_define.h"

#define ubdrv_vmalloc(size) \
    __ka_vmalloc(size, KA_GFP_KERNEL, KA_PAGE_KERNEL, ka_get_module_id(HAL_MODULE_TYPE_ASDRV_UB, KA_SUB_MODULE_TYPE_0))

#define ubdrv_kzalloc(size, flags) \
    ka_kzalloc(size, flags, ka_get_module_id(HAL_MODULE_TYPE_ASDRV_UB, KA_SUB_MODULE_TYPE_0))
#define ubdrv_vzalloc(size) ka_vzalloc(size, ka_get_module_id(HAL_MODULE_TYPE_ASDRV_UB, KA_SUB_MODULE_TYPE_0))

#define ubdrv_kfree(addr) ka_kfree(addr, ka_get_module_id(HAL_MODULE_TYPE_ASDRV_UB, KA_SUB_MODULE_TYPE_0))
#define ubdrv_vfree(addr) ka_vfree(addr, ka_get_module_id(HAL_MODULE_TYPE_ASDRV_UB, KA_SUB_MODULE_TYPE_0))

#endif