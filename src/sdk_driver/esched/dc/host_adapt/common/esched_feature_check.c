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

#include "esched_feature_check.h"
#include "esched.h"
#include "esched_log.h"
#include "pbl/pbl_uda.h"
#include "esched_chip_feature_map.h"
#include "ascend_hal_error.h"
#include "esched_ioctl.h"

/*
 * ============================================================================
 * Chip version feature table definitions (using arrays from shared header file)
 * ============================================================================
 */

static const struct esched_chip_features g_esched_hisi_mini_v1_features = {
    .features = hisi_mini_v1_esched_features,
    .feature_count = sizeof(hisi_mini_v1_esched_features) / sizeof(hisi_mini_v1_esched_features[0])};

static const struct esched_chip_features g_esched_hisi_cloud_v1_features = {
    .features = hisi_cloud_v1_esched_features,
    .feature_count = sizeof(hisi_cloud_v1_esched_features) / sizeof(hisi_cloud_v1_esched_features[0])};

static const struct esched_chip_features g_esched_hisi_mini_v2_features = {
    .features = hisi_mini_v2_esched_features,
    .feature_count = sizeof(hisi_mini_v2_esched_features) / sizeof(hisi_mini_v2_esched_features[0])};

static const struct esched_chip_features g_esched_hisi_cloud_v2_features = {
    .features = hisi_cloud_v2_esched_features,
    .feature_count = sizeof(hisi_cloud_v2_esched_features) / sizeof(hisi_cloud_v2_esched_features[0])};

static const struct esched_chip_features g_esched_hisi_mini_v3_features = {
    .features = hisi_mini_v3_esched_features,
    .feature_count = sizeof(hisi_mini_v3_esched_features) / sizeof(hisi_mini_v3_esched_features[0])};

static const struct esched_chip_features g_esched_hisi_cloud_v4_features = {
    .features = hisi_cloud_v4_esched_features,
    .feature_count = sizeof(hisi_cloud_v4_esched_features) / sizeof(hisi_cloud_v4_esched_features[0])};

static const struct esched_chip_features g_esched_hisi_cloud_v5_features = {
    .features = hisi_cloud_v5_esched_features,
    .feature_count = sizeof(hisi_cloud_v5_esched_features) / sizeof(hisi_cloud_v5_esched_features[0])};

static const struct esched_chip_features g_esched_hisi_mini_v4_features = {
    .features = hisi_mini_v4_esched_features,
    .feature_count = sizeof(hisi_mini_v4_esched_features) / sizeof(hisi_mini_v4_esched_features[0])};

/*
 * ============================================================================
 * Chip feature lookup table (indexed by chip_type enum)
 * ============================================================================
 */
#define ESCHED_CHIP_FEATURE_TABLE_SIZE (HISI_MINI_V4 + 1)

static const struct esched_chip_features *g_esched_chip_feature_table[ESCHED_CHIP_FEATURE_TABLE_SIZE] = {
    [HISI_MINI_V1] = &g_esched_hisi_mini_v1_features,   [HISI_CLOUD_V1] = &g_esched_hisi_cloud_v1_features,
    [HISI_MINI_V2] = &g_esched_hisi_mini_v2_features,   [HISI_CLOUD_V2] = &g_esched_hisi_cloud_v2_features,
    [HISI_MINI_V3] = &g_esched_hisi_mini_v3_features,   [HISI_CLOUD_V4] = &g_esched_hisi_cloud_v4_features,
    [HISI_CLOUD_V5] = &g_esched_hisi_cloud_v5_features, [HISI_MINI_V4] = &g_esched_hisi_mini_v4_features,
};

/*
 * ============================================================================
 * Per-device chip type cache (fixed-size array, indexed by dev_id)
 * ============================================================================
 */

static unsigned int g_esched_dev_chip_map[ESCHED_FEATURE_DEV_NUM];

/*
 * ============================================================================
 * Basic function implementations
 * ============================================================================
 */

/*
 * Initialize the per-device chip type cache
 * Must be called before any other feature functions
 */
void esched_feature_system_init(void)
{
    unsigned int i;

    for (i = 0; i < ESCHED_FEATURE_DEV_NUM; i++) {
        g_esched_dev_chip_map[i] = HISI_CHIP_UNKNOWN;
    }
}

/*
 * ============================================================================
 * Public API: per-device feature configuration lookup
 * ============================================================================
 */

const struct esched_chip_features *esched_get_dev_features(unsigned int dev_id)
{
    unsigned int chip_type;
    const struct esched_chip_features *features;

    if (dev_id >= ESCHED_FEATURE_DEV_NUM) {
        sched_err("Invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, ESCHED_FEATURE_DEV_NUM);
        return NULL;
    }
    chip_type = g_esched_dev_chip_map[dev_id];
    if (chip_type == HISI_CHIP_UNKNOWN) {
        sched_debug("Feature not initialized. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    features = esched_get_chip_features(chip_type);
    if (features == NULL) {
        sched_err("esched_get_dev_features(dev_id=%u) -> chip_type=%u, no feature table\n", dev_id, chip_type);
    }
    return features;
}

int esched_has_feature(unsigned int dev_id, enum esched_feature_id feature)
{
    const struct esched_chip_features *feat = esched_get_dev_features(dev_id);

    if (feat == NULL) {
        return 0;
    }
    return esched_feature_array_has(feat->features, feat->feature_count, feature);
}

/*
 * Get feature configuration by chip type
 */
const struct esched_chip_features *esched_get_chip_features(unsigned int chip_type)
{
    if (chip_type >= ESCHED_CHIP_FEATURE_TABLE_SIZE) {
        return NULL;
    }
    return g_esched_chip_feature_table[chip_type];
}

/*
 * Deinitialize feature system (reset per-device chip type cache)
 */
void esched_feature_uninit(void)
{
    unsigned int i;

    for (i = 0; i < ESCHED_FEATURE_DEV_NUM; i++) {
        g_esched_dev_chip_map[i] = HISI_CHIP_UNKNOWN;
    }
}

/*
 * Auto-detect chip type by dev_id via UDA interface and populate chip type cache
 */
drvError_t esched_feature_init_by_devid(unsigned int dev_id)
{
    unsigned int chip_type;
    const struct esched_chip_features *chip_features;

    if (dev_id >= ESCHED_FEATURE_DEV_NUM) {
        sched_err("Invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, ESCHED_FEATURE_DEV_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* Skip if already initialized for this dev_id */
    if (g_esched_dev_chip_map[dev_id] != HISI_CHIP_UNKNOWN) {
        sched_debug("Feature already cached. (dev_id=%u; chip_type=%u)\n", dev_id, g_esched_dev_chip_map[dev_id]);
        return DRV_ERROR_NONE;
    }

    chip_type = uda_get_chip_type(dev_id);
    if (chip_type == HISI_CHIP_UNKNOWN) {
        sched_err("Unknown chip type. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    chip_features = esched_get_chip_features(chip_type);
    if (chip_features == NULL) {
        sched_err("No feature config for chip_type=%u (dev_id=%u)\n", chip_type, dev_id);
        return DRV_ERROR_NOT_SUPPORT;
    }

    /* Populate per-device chip type cache */
    g_esched_dev_chip_map[dev_id] = chip_type;

    sched_info("Feature cache populated. (dev_id=%u; chip_type=%u; feature_count=%u)\n", dev_id, chip_type,
               chip_features->feature_count);

    return DRV_ERROR_NONE;
}
