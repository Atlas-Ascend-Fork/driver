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

#include "queue_feature_check.h"
#include "queue_module.h"
#include "securec.h"
#include "pbl/pbl_uda.h"
#include "queue_chip_feature_map.h"
#include "ascend_hal_error.h"

/*
 * ============================================================================
 * Chip version feature table definitions (using arrays from shared header file)
 * ============================================================================
 */

static const struct queue_chip_features g_queue_hisi_mini_v1_features = {
    .features = hisi_mini_v1_queue_features,
    .feature_count = sizeof(hisi_mini_v1_queue_features) / sizeof(hisi_mini_v1_queue_features[0])};

static const struct queue_chip_features g_queue_hisi_cloud_v1_features = {
    .features = hisi_cloud_v1_queue_features,
    .feature_count = sizeof(hisi_cloud_v1_queue_features) / sizeof(hisi_cloud_v1_queue_features[0])};

static const struct queue_chip_features g_queue_hisi_mini_v2_features = {
    .features = hisi_mini_v2_queue_features,
    .feature_count = sizeof(hisi_mini_v2_queue_features) / sizeof(hisi_mini_v2_queue_features[0])};

static const struct queue_chip_features g_queue_hisi_cloud_v2_features = {
    .features = hisi_cloud_v2_queue_features,
    .feature_count = sizeof(hisi_cloud_v2_queue_features) / sizeof(hisi_cloud_v2_queue_features[0])};

static const struct queue_chip_features g_queue_hisi_mini_v3_features = {
    .features = hisi_mini_v3_queue_features,
    .feature_count = sizeof(hisi_mini_v3_queue_features) / sizeof(hisi_mini_v3_queue_features[0])};

static const struct queue_chip_features g_queue_hisi_cloud_v4_features = {
    .features = hisi_cloud_v4_queue_features,
    .feature_count = sizeof(hisi_cloud_v4_queue_features) / sizeof(hisi_cloud_v4_queue_features[0])};

static const struct queue_chip_features g_queue_hisi_cloud_v5_features = {
    .features = hisi_cloud_v5_queue_features,
    .feature_count = sizeof(hisi_cloud_v5_queue_features) / sizeof(hisi_cloud_v5_queue_features[0])};

static const struct queue_chip_features g_queue_hisi_mini_v4_features = {
    .features = hisi_mini_v4_queue_features,
    .feature_count = sizeof(hisi_mini_v4_queue_features) / sizeof(hisi_mini_v4_queue_features[0])};

/*
 * ============================================================================
 * Chip feature lookup table (indexed by chip_type enum)
 * ============================================================================
 */
#define QUEUE_CHIP_FEATURE_TABLE_SIZE (HISI_MINI_V4 + 1)

static const struct queue_chip_features *g_queue_chip_feature_table[QUEUE_CHIP_FEATURE_TABLE_SIZE] = {
    [HISI_MINI_V1] = &g_queue_hisi_mini_v1_features,   [HISI_CLOUD_V1] = &g_queue_hisi_cloud_v1_features,
    [HISI_MINI_V2] = &g_queue_hisi_mini_v2_features,   [HISI_CLOUD_V2] = &g_queue_hisi_cloud_v2_features,
    [HISI_MINI_V3] = &g_queue_hisi_mini_v3_features,   [HISI_CLOUD_V4] = &g_queue_hisi_cloud_v4_features,
    [HISI_CLOUD_V5] = &g_queue_hisi_cloud_v5_features, [HISI_MINI_V4] = &g_queue_hisi_mini_v4_features,
};

/*
 * ============================================================================
 * Per-device chip type cache (fixed-size array, indexed by dev_id)
 * ============================================================================
 */

static unsigned int g_queue_dev_chip_map[QUEUE_FEATURE_DEV_NUM];

/*
 * ============================================================================
 * Basic function implementations
 * ============================================================================
 */

/*
 * Initialize the per-device chip type cache
 * Must be called before any other feature functions
 */
void queue_feature_system_init(void)
{
    unsigned int i;

    for (i = 0; i < QUEUE_FEATURE_DEV_NUM; i++) {
        g_queue_dev_chip_map[i] = HISI_CHIP_UNKNOWN;
    }

    queue_info("Queue feature system initialized. (max_dev=%u)\n", QUEUE_FEATURE_DEV_NUM);
}

/*
 * Get chip feature configuration for specified device.
 * Returns NULL if dev_id is invalid or device not initialized.
 */
const struct queue_chip_features *queue_get_dev_features(unsigned int dev_id)
{
    unsigned int chip_type;

    if (dev_id >= QUEUE_FEATURE_DEV_NUM) {
        queue_err("Invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, QUEUE_FEATURE_DEV_NUM);
        return NULL;
    }

    chip_type = g_queue_dev_chip_map[dev_id];
    if (chip_type == HISI_CHIP_UNKNOWN) {
        queue_debug("Unknown chip type. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    return queue_get_chip_features(chip_type);
}

int queue_has_feature(unsigned int dev_id, enum queue_feature_id feature)
{
    const struct queue_chip_features *feat = queue_get_dev_features(dev_id);
    if (feat == NULL) {
        return 0;
    }
    return queue_feature_array_has(feat->features, feat->feature_count, feature);
}

/*
 * Get feature configuration by chip type
 */
const struct queue_chip_features *queue_get_chip_features(unsigned int chip_type)
{
    /* Use lookup table for O(1) access instead of switch-case */
    if (chip_type >= QUEUE_CHIP_FEATURE_TABLE_SIZE) {
        return NULL;
    }
    return g_queue_chip_feature_table[chip_type];
}

/*
 * Deinitialize feature system (reset per-device chip type cache)
 */
void queue_feature_uninit(void)
{
    unsigned int i;

    for (i = 0; i < QUEUE_FEATURE_DEV_NUM; i++) {
        g_queue_dev_chip_map[i] = HISI_CHIP_UNKNOWN;
    }

    queue_info("Queue feature system uninitialized.\n");
}

/*
 * Auto-detect chip type by dev_id via UDA interface and populate chip type cache
 */
drvError_t queue_feature_init_by_devid(unsigned int dev_id)
{
    unsigned int chip_type;
    const struct queue_chip_features *chip_features;

    if (dev_id >= QUEUE_FEATURE_DEV_NUM) {
        queue_err("Invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, QUEUE_FEATURE_DEV_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    chip_type = uda_get_chip_type(dev_id);
    if (chip_type == HISI_CHIP_UNKNOWN) {
        queue_err("Unknown chip type. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    chip_features = queue_get_chip_features(chip_type);
    if (chip_features == NULL) {
        queue_err("No feature config for chip_type=%u (dev_id=%u)\n", chip_type, dev_id);
        return DRV_ERROR_NOT_SUPPORT;
    }

    /* Skip if already initialized for this dev_id */
    if (g_queue_dev_chip_map[dev_id] != HISI_CHIP_UNKNOWN) {
        queue_info("Feature already cached. (dev_id=%u; chip_type=%u)\n", dev_id, chip_type);
        return DRV_ERROR_NONE;
    }

    /* Populate per-device chip type cache */
    g_queue_dev_chip_map[dev_id] = chip_type;

    queue_info("Feature cache populated. (dev_id=%u; chip_type=%u; feature_count=%u)\n", dev_id, chip_type,
               chip_features->feature_count);

    return DRV_ERROR_NONE;
}
