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

#include "ka_kernel_def_pub.h"
#include "ka_compiler_pub.h"

#include "securec.h"
#include "pbl_uda.h"
#include "pbl_dev_identity.h"
#include "trs_pub_def.h"
#include "trsbase_feature.h"

static trsbase_feat_bitmap_t g_trsbase_chip_features[HISI_CHIP_NUM] = {
    [HISI_CLOUD_V2] = {BIT(TRSBASE_FEATURE_MAX)},
    [HISI_MINI_V3] = {BIT(TRSBASE_FEATURE_MAX)},
    [HISI_CLOUD_V4] = {BIT(TRSBASE_FEATURE_MAX)},
    [HISI_CLOUD_V5] = {BIT(TRSBASE_FEATURE_MAX)},
};

static trsbase_feat_bitmap_t g_trsbase_dev_features[TRS_DEV_MAX_NUM] = {0};

int trsbase_feature_init(u32 devid)
{
    u32 chip_type;

    chip_type = uda_get_chip_type(devid);
    if (chip_type >= HISI_CHIP_UNKNOWN) {
        trs_err("Invalid chip type. (devid=%u; chip_type=%u)\n", devid, chip_type);
        return -EINVAL;
    }

    if (memcpy_s(g_trsbase_dev_features[devid], sizeof(trsbase_feat_bitmap_t), g_trsbase_chip_features[chip_type],
                 sizeof(trsbase_feat_bitmap_t)) != 0) {
        return -EFAULT;
    }

    trs_info("Trsbase feature init. (devid=%u; chip_type=%u; bitmap=0x%llx)\n", devid, chip_type,
             g_trsbase_dev_features[devid][0]);

    return 0;
}
KA_EXPORT_SYMBOL(trsbase_feature_init);

void trsbase_feature_uninit(u32 devid)
{
    (void)memset_s(g_trsbase_dev_features[devid], sizeof(trsbase_feat_bitmap_t), 0, sizeof(trsbase_feat_bitmap_t));
}
KA_EXPORT_SYMBOL(trsbase_feature_uninit);
