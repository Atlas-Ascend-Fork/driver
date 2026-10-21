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

#include "virtmnghost_feature.h"
#include "virtmng_res_cloud_v1.h"
#include "virtmng_res_cloud_v2.h"
#include "virtmng_res_cloud_v4.h"
#include "virtmng_res_mini_v2.h"
#include "virtmng_public_def.h"
#include "virtmnghost_unit.h"
#include "comm_kernel_interface.h"

STATIC vmng_feature_bitmap_t (*vmngh_feature_init_func[HISI_CHIP_NUM])(void) = {
    [HISI_MINI_V1] = NULL,
    [HISI_CLOUD_V1] = vmngh_feature_init_cloud_v1,
    [HISI_MINI_V2] = vmngh_feature_init_mini_v2,
    [HISI_CLOUD_V2] = vmngh_feature_init_cloud_v2,
    [HISI_MINI_V3] = NULL,
    [HISI_CLOUD_V4] = vmngh_feature_init_cloud_v4,
    [HISI_CLOUD_V5] = vmngh_feature_init_cloud_v4,
    [HISI_MINI_V4] = vmngh_feature_init_cloud_v4,
};

vmng_feature_bitmap_t vmngh_feature_init(u32 dev_id)
{
    u32 chip_type;

    chip_type = uda_get_chip_type(dev_id);
    if ((chip_type < HISI_CHIP_NUM) && (vmngh_feature_init_func[chip_type] != NULL)) {
        return vmngh_feature_init_func[chip_type]();
    }

    return 0;
}

bool vmngh_feature_not_support(u32 dev_id, enum vmng_feature feat)
{
    struct vmngh_pci_dev *pdev = NULL;

    if (feat >= VMNG_FEATURE_MAX) {
        vmng_err("Input parameter is error. (feat=%d)\n", feat);
        return true;
    }

    pdev = vmngh_get_pdev_from_unit(dev_id);
    if (pdev == NULL) {
        vmng_err("Get pdev failed. (dev_id=%u)\n", dev_id);
        return true;
    }

    return ((pdev->features & (1ULL << feat)) != 0) ? false : true;
}
