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
#include "dms_ioctl.h"

/* offset in byte */
#define FEAT_BIT(feat_id) (1ULL << ((feat_id) % 64))

static const u64 dms_features_cloud_v4[] = {
    FEAT_BIT(DMS_RECAL_LPM_POWER),
};

static const u64 dms_features_cloud_v5[] = {
    FEAT_BIT(DMS_L2_BUFF_INFO) | FEAT_BIT(DMS_MEMORY_INFO),
};

BEGIN_MODULE_FEATURE_DECLARATION(dms, DMS_FEATURE_MAX)
MODULE_FEATURE_MAP(HISI_CLOUD_V4, dms_features_cloud_v4)
MODULE_FEATURE_MAP(HISI_CLOUD_V5, dms_features_cloud_v5)
MODULE_FEATURE_MAP(HISI_CLOUD_V6, dms_features_cloud_v5)
MODULE_FEATURE_MAP(HISI_CLOUD_V7, dms_features_cloud_v5)
MODULE_FEATURE_TABLE_END(dms)
MODULE_FEATURE_DESCRIPTOR(dms, DMS_FEATURE_MAX)
MODULE_FEATURE_HAS_FEATURE_DEF(dms)
MODULE_FEATURE_INIT_BY_DEVID(dms)
MODULE_FEATURE_URD_HANDLER(dms)
MODULE_FEATURE_NOTIFIER(dms)
MODULE_FEATURE_CHECK_INIT(dms)
MODULE_FEATURE_CHECK_UNINIT(dms)

DECLAER_FEATURE_AUTO_INIT(dms_feature_check_init, FEATURE_LOADER_STAGE_5);
DECLAER_FEATURE_AUTO_UNINIT(dms_feature_check_uninit, FEATURE_LOADER_STAGE_5);
