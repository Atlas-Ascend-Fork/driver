/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd. All rights reserved.
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

#ifndef DMS_BBOX_DUMP_CHIP_FEATURE_MAP_H
#define DMS_BBOX_DUMP_CHIP_FEATURE_MAP_H

#include "dms_bbox_dump_feature.h"

#ifdef __cplusplus
extern "C" {
#endif

/* chip key: hisi_cloud_v2 - HISI_CLOUD_V2 (3) */
static const enum bbox_feature_id hisi_cloud_v2_bbox_features[] = {
    BBOX_FEATURE_SOC_PLATFORM_CLOUD_V2,
    BBOX_FEATURE_KERNEL_LOG_ADAPT,
};

/* chip key: hisi_mini_v3 - HISI_MINI_V3 (4) */
static const enum bbox_feature_id hisi_mini_v3_bbox_features[] = {
    BBOX_FEATURE_SOC_PLATFORM_RC,
    BBOX_FEATURE_KERNEL_LOG_ADAPT,
};

/* chip key: hisi_cloud_v4 - HISI_CLOUD_V4 (6) */
static const enum bbox_feature_id hisi_cloud_v4_bbox_features[] = {
    BBOX_FEATURE_PCIE_BBOX_DUMP,
    BBOX_FEATURE_SOC_PLATFORM_CLOUD_V4,
    BBOX_FEATURE_KERNEL_LOG_ADAPT,
};

/* chip key: hisi_cloud_v5 - HISI_CLOUD_V5 (7), CHIP_CLOUD_V5 */
static const enum bbox_feature_id hisi_cloud_v5_bbox_features[] = {
    BBOX_FEATURE_PCIE_BBOX_DUMP,
    BBOX_FEATURE_SOC_PLATFORM_CLOUD_V4,
};

/* chip key: hisi_mini_v4 - HISI_MINI_V4 (10) */
static const enum bbox_feature_id hisi_mini_v4_bbox_features[] = {
    BBOX_FEATURE_PCIE_BBOX_DUMP,
    BBOX_FEATURE_SOC_PLATFORM_CLOUD_V4,
};

#ifdef __cplusplus
}
#endif

#endif /* DMS_BBOX_DUMP_CHIP_FEATURE_MAP_H */
