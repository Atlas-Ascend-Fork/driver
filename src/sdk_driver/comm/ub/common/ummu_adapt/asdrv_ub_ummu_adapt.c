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

#include "asdrv_ub_ummu_adapt.h"
#include "adapt_ops.h"
#include "ummu_core_adapt.h"
#include "asdrv_ub_common.h"

STATIC struct device *uba_ubdrv_alloc_tdev(uba_tdev_attr *attr, u32 *ptid)
{
    return ummu_core_alloc_tdev(attr, ptid);
}

STATIC void uba_ubdrv_free_tdev(struct device *tdev)
{
    ummu_core_free_tdev(tdev);
}

STATIC struct uba_ummu_adapt_ops g_ummu_adapt_ops = {
    .alloc_tdev = uba_ubdrv_alloc_tdev,
    .free_tdev = uba_ubdrv_free_tdev,
};

STATIC struct uba_dev_ops g_ummu_dev_ops = {
    .type = UBA_OPS_UMMU,
    .ops.ummu = &g_ummu_adapt_ops,
};

struct uba_dev_ops *ubdrv_get_ummu_dev_ops(void)
{
    return &g_ummu_dev_ops;
}
