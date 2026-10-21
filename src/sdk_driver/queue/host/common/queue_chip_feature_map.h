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

#ifndef QUEUE_CHIP_FEATURE_MAP_H
#define QUEUE_CHIP_FEATURE_MAP_H

#include "queue_feature_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* chip key: hisi_mini_v1 - HISI_MINI_V1 (0) */
static const enum queue_feature_id hisi_mini_v1_queue_features[] = {
    QUEUE_FEATURE_SHARE_LOG,
};

/* chip key: hisi_cloud_v1 - HISI_CLOUD_V1 (1) */
static const enum queue_feature_id hisi_cloud_v1_queue_features[] = {
    QUEUE_FEATURE_SHARE_LOG,
    QUEUE_FEATURE_KA_ALLOC_INTERFACE,
    QUEUE_FEATURE_HOST_LOG,
    QUEUE_FEATURE_HOST_SUPPORT_LOCAL_QUEUE,
};

/* chip key: hisi_mini_v2 - HISI_MINI_V2 (2) */
static const enum queue_feature_id hisi_mini_v2_queue_features[] = {
    QUEUE_FEATURE_SHARE_LOG,
    QUEUE_FEATURE_KA_ALLOC_INTERFACE,
    QUEUE_FEATURE_HOST_LOG,
    QUEUE_FEATURE_HOST_SUPPORT_LOCAL_QUEUE,
};

/* chip key: hisi_cloud_v2 - HISI_CLOUD_V2 (3) */
static const enum queue_feature_id hisi_cloud_v2_queue_features[] = {
    QUEUE_FEATURE_SHARE_LOG,     QUEUE_FEATURE_KA_ALLOC_INTERFACE,       QUEUE_FEATURE_HOST_LOG,
    QUEUE_FEATURE_SUPPORT_SRIOV, QUEUE_FEATURE_HOST_SUPPORT_LOCAL_QUEUE,
};

/* chip key: hisi_mini_v3 - HISI_MINI_V3 (4) */
static const enum queue_feature_id hisi_mini_v3_queue_features[] = {
    QUEUE_FEATURE_SHARE_LOG,
    QUEUE_FEATURE_KA_ALLOC_INTERFACE,
    QUEUE_FEATURE_HOST_LOG,
    QUEUE_SOC_PLATFORM_960,
};

/* chip key: hisi_cloud_v4 - HISI_CLOUD_V4 (6) */
static const enum queue_feature_id hisi_cloud_v4_queue_features[] = {
    QUEUE_FEATURE_SHARE_LOG,     QUEUE_FEATURE_KA_ALLOC_INTERFACE,       QUEUE_FEATURE_HOST_LOG,
    QUEUE_FEATURE_SUPPORT_SRIOV, QUEUE_FEATURE_HOST_SUPPORT_LOCAL_QUEUE,
};

/* chip key: hisi_cloud_v5 - HISI_CLOUD_V5 (7), CHIP_CLOUD_V5 */
static const enum queue_feature_id hisi_cloud_v5_queue_features[] = {
    QUEUE_FEATURE_SHARE_LOG,
    QUEUE_FEATURE_KA_ALLOC_INTERFACE,
    QUEUE_FEATURE_HOST_LOG,
    QUEUE_FEATURE_SUPPORT_SRIOV,
    QUEUE_FEATURE_HOST_SUPPORT_LOCAL_QUEUE, // only for host
};

/* chip key: hisi_mini_v4 - HISI_MINI_V4 (10) */
static const enum queue_feature_id hisi_mini_v4_queue_features[] = {
    QUEUE_FEATURE_SHARE_LOG,     QUEUE_FEATURE_KA_ALLOC_INTERFACE,       QUEUE_FEATURE_HOST_LOG,
    QUEUE_FEATURE_SUPPORT_SRIOV, QUEUE_FEATURE_HOST_SUPPORT_LOCAL_QUEUE,
};

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_CHIP_FEATURE_MAP_H */
