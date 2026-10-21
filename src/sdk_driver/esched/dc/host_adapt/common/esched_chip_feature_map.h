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

#ifndef ESCHED_CHIP_FEATURE_MAP_H
#define ESCHED_CHIP_FEATURE_MAP_H

#include "esched_feature_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* hisi_mini_v1 - HISI_MINI_V1 (0) */
static const enum esched_feature_id hisi_mini_v1_esched_features[] = {

};

/* hisi_cloud_v1 - HISI_CLOUD_V1 (1) */
static const enum esched_feature_id hisi_cloud_v1_esched_features[] = {

};

/* hisi_mini_v2 - HISI_MINI_V2 (2) */
static const enum esched_feature_id hisi_mini_v2_esched_features[] = {

};

/* hisi_cloud_v2 - HISI_CLOUD_V2 (3) */
static const enum esched_feature_id hisi_cloud_v2_esched_features[] = {
    ESCHED_FEATURE_HARDWARE_SCHED,
    ESCHED_FEATURE_HARDWARE_MIA,
    ESCHED_FEATURE_SOFT_NON_SCHED_ONLY,
    ESCHED_FEATURE_REMOTE_PUB_HARD_SCHED,
};

/* hisi_mini_v3 - HISI_MINI_V3 (4) */
static const enum esched_feature_id hisi_mini_v3_esched_features[] = {
    ESCHED_FEATURE_HARDWARE_SCHED,
    ESCHED_FEATURE_SOFT_NON_SCHED_ONLY,
    ESCHED_FEATURE_SIA_MAP_TOPIC_TABLE,
    ESCHED_FEATURE_REMOTE_PUB_HARD_SCHED,
};

/* hisi_cloud_v4 - HISI_CLOUD_V4 (6) */
static const enum esched_feature_id hisi_cloud_v4_esched_features[] = {
    ESCHED_FEATURE_HARDWARE_SCHED,
    ESCHED_FEATURE_HARDWARE_MIA,
    ESCHED_FEATURE_SOFT_NON_SCHED_ONLY,
    ESCHED_SOC_PLATFORM_CLOUD_V4,
};

/* hisi_cloud_v5 - HISI_CLOUD_V5 (7) */
static const enum esched_feature_id hisi_cloud_v5_esched_features[] = {
    ESCHED_FEATURE_HARDWARE_SCHED,
    ESCHED_FEATURE_HARDWARE_MIA,
    ESCHED_FEATURE_SOFT_NON_SCHED_ONLY,
    ESCHED_SOC_PLATFORM_CLOUD_V4,
};

/* hisi_mini_v4 - HISI_MINI_V4 (10) */
static const enum esched_feature_id hisi_mini_v4_esched_features[] = {
    ESCHED_FEATURE_HARDWARE_SCHED,
    ESCHED_FEATURE_HARDWARE_MIA,
    ESCHED_FEATURE_SOFT_NON_SCHED_ONLY,
    ESCHED_SOC_PLATFORM_CLOUD_V4,
};

#ifdef __cplusplus
}
#endif

#endif /* ESCHED_CHIP_FEATURE_MAP_H */
