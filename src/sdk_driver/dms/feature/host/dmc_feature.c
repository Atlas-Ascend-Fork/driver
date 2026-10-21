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
#include "dmc_feature_id_def.h"

/* offset in DWORD */
#define FEAT_BIT(feat_id) (1ULL << ((feat_id) % 64))

static const u64 dmc_features_cloud_v4[] = {
    0,
};

static const u64 dmc_features_cloud_v5[] = {
    FEAT_BIT(DMC_CFG_FEATURE_SUPPORT_UBMEM_RETRY_FLAG),
};

BEGIN_MODULE_FEATURE_DECLARATION(dmc, DMC_FEATURE_MAX)
MODULE_FEATURE_MAP(HISI_CLOUD_V4, dmc_features_cloud_v4)
MODULE_FEATURE_MAP(HISI_CLOUD_V5, dmc_features_cloud_v5)
MODULE_FEATURE_TABLE_END(dmc)
MODULE_FEATURE_DESCRIPTOR(dmc, DMC_FEATURE_MAX)
MODULE_FEATURE_INIT_BY_DEVID(dmc)
MODULE_FEATURE_URD_HANDLER(dmc)
MODULE_FEATURE_NOTIFIER(dmc)
MODULE_FEATURE_CHECK_INIT(dmc)
MODULE_FEATURE_CHECK_UNINIT(dmc)

DECLAER_FEATURE_AUTO_INIT(dmc_feature_check_init, FEATURE_LOADER_STAGE_5);
DECLAER_FEATURE_AUTO_UNINIT(dmc_feature_check_uninit, FEATURE_LOADER_STAGE_5);
