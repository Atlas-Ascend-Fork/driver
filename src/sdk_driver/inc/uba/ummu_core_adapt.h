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

#ifndef __UMMU_CORE_ADAPT_H__
#define __UMMU_CORE_ADAPT_H__

#include <linux/ummu_core.h>
#ifdef DRV_HOST
#include <linux/hisi_ummu.h>
#endif
#include "module_type.h"

typedef struct tdev_attr uba_tdev_attr;
#ifdef DRV_HOST
typedef struct hisi_ummu_tdev_info uba_hisi_ummu_tdev_info;
#endif

#define UBA_UMMU_INVALID_TID UMMU_INVALID_TID
#define uba_tdev_attr_init(attr) tdev_attr_init(attr)

struct device *uba_ummu_core_alloc_tdev(enum uba_module_type module_type, uba_tdev_attr *attr, u32 *ptid);
void uba_ummu_core_free_tdev(enum uba_module_type module_type, struct device *tdev);

#endif
