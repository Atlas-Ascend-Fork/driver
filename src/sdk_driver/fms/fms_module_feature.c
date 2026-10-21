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

#include "pbl/pbl_module_feature_decl.h"
#include "fms_module_feature.h"

/* Calculate the byte position by yourself, and then use the FEAT_SIT macro to calculate the bit position within the
 * byte  */
static const u64 fms_features_cloud_v4[] = {
    0,
};

static const u64 fms_features_cloud_v5[] = {
    0,
};

BEGIN_MODULE_FEATURE_DECLARATION(fms, FMS_FEATURE_MAX)
MODULE_FEATURE_MAP(HISI_CLOUD_V4, fms_features_cloud_v4)
MODULE_FEATURE_MAP(HISI_CLOUD_V5, fms_features_cloud_v5)
MODULE_FEATURE_TABLE_END(fms)
MODULE_FEATURE_DESCRIPTOR(fms, FMS_FEATURE_MAX)
MODULE_FEATURE_INIT_BY_DEVID(fms)
MODULE_FEATURE_URD_HANDLER(fms)
MODULE_FEATURE_NOTIFIER(fms)
MODULE_FEATURE_CHECK_INIT(fms)
MODULE_FEATURE_CHECK_UNINIT(fms)

DECLAER_FEATURE_AUTO_INIT(fms_feature_check_init, FEATURE_LOADER_STAGE_5);
DECLAER_FEATURE_AUTO_UNINIT(fms_feature_check_uninit, FEATURE_LOADER_STAGE_5);
