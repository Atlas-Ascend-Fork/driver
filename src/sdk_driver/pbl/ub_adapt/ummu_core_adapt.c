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

#include "ummu_core_adapt.h"
#include "adapt_ops.h"
#include "ka_kernel_def_pub.h"

struct device *uba_ummu_core_alloc_tdev(enum uba_module_type module_type, uba_tdev_attr *attr, u32 *ptid)
{
    struct device *tdev = NULL;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UMMU);

    if (dev_ops == NULL) {
        return NULL;
    }
    if ((dev_ops->ops.ummu == NULL) || (dev_ops->ops.ummu->alloc_tdev == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return NULL;
    }

    tdev = dev_ops->ops.ummu->alloc_tdev(attr, ptid);
    UBA_ADD_MODULE_ON_SUCC(tdev != NULL, module_type, UBA_FUNC_ALLOC_TDEV);
    uba_sub_adapt_ops_ref(dev_ops);
    return tdev;
}
KA_EXPORT_SYMBOL(uba_ummu_core_alloc_tdev);

void uba_ummu_core_free_tdev(enum uba_module_type module_type, struct device *tdev)
{
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UMMU);

    if (dev_ops == NULL) {
        return;
    }
    if ((dev_ops->ops.ummu == NULL) || (dev_ops->ops.ummu->free_tdev == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return;
    }

    dev_ops->ops.ummu->free_tdev(tdev);
    UBA_ADD_MODULE_ON_SUCC(true, module_type, UBA_FUNC_FREE_TDEV);
    uba_sub_adapt_ops_ref(dev_ops);
}
KA_EXPORT_SYMBOL(uba_ummu_core_free_tdev);
