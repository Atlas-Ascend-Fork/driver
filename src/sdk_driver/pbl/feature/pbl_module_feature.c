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

#include "pbl/pbl_module_feature_decl.h"
#include "pbl_module_feature.h"

/* Calculate the byte position by yourself, and then use the FEAT_SIT macro to calculate the bit position within the
 * byte  */
#define FEAT_BIT(feat_id) (1ULL << ((feat_id) % 64))

static const u64 pbl_features_cloud_v4[] = {
    FEAT_BIT(PBL_UDA_CHAR_NUMBERING_BY_LOGIC_ID),
};

static const u64 pbl_features_cloud_v5[] = {
    FEAT_BIT(PBL_UDA_CHAR_NUMBERING_BY_LOGIC_ID),
};

BEGIN_MODULE_FEATURE_DECLARATION(pbl, PBL_FEATURE_MAX)
MODULE_FEATURE_MAP(HISI_CLOUD_V4, pbl_features_cloud_v4)
MODULE_FEATURE_MAP(HISI_CLOUD_V5, pbl_features_cloud_v5)
MODULE_FEATURE_TABLE_END(pbl)
MODULE_FEATURE_DESCRIPTOR(pbl, PBL_FEATURE_MAX)
MODULE_FEATURE_HAS_FEATURE_DECL(pbl)
MODULE_FEATURE_HAS_FEATURE_DEF(pbl)
MODULE_FEATURE_INIT_BY_DEVID(pbl)
MODULE_FEATURE_URD_HANDLER(pbl)
MODULE_FEATURE_NOTIFIER(pbl)
MODULE_FEATURE_CHECK_INIT(pbl)
MODULE_FEATURE_CHECK_UNINIT(pbl)

DECLAER_FEATURE_AUTO_INIT(pbl_feature_check_init, FEATURE_LOADER_STAGE_5);
DECLAER_FEATURE_AUTO_UNINIT(pbl_feature_check_uninit, FEATURE_LOADER_STAGE_5);
