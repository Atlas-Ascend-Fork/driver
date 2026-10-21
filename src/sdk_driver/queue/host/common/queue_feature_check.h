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

#ifndef QUEUE_FEATURE_CHECK_H
#define QUEUE_FEATURE_CHECK_H

#include "queue_feature_kernel.h"
#include "ascend_hal_error.h"

/*
 * ============================================================================
 * Chip version feature configuration structure
 * Each chip version corresponds to a feature array
 * ============================================================================
 */
struct queue_chip_features {
    const enum queue_feature_id *features;
    unsigned int feature_count;
};

/*
 * ============================================================================
 * Basic function declarations
 * ============================================================================
 */

/*
 * Initialize the feature check system
 * Must be called once before using any other feature functions
 */
void queue_feature_system_init(void);

/*
 * Get chip feature configuration for specified device.
 * @param dev_id: device ID
 * @return: pointer to chip feature configuration, NULL if dev_id is invalid or device not initialized
 */
const struct queue_chip_features *queue_get_dev_features(unsigned int dev_id);

/*
 * Get feature configuration by chip type
 * @param chip_type: UDA chip type, e.g., HISI_CLOUD_V2, HISI_CLOUD_V5
 * @return: pointer to corresponding feature configuration, NULL if not found
 */
const struct queue_chip_features *queue_get_chip_features(unsigned int chip_type);

/*
 * Check if specified device supports a given feature
 * @param dev_id: device ID
 * @param feature: feature ID to check
 * @return: 1 if feature exists, 0 if not
 */
int queue_has_feature(unsigned int dev_id, enum queue_feature_id feature);
void queue_feature_uninit(void);
drvError_t queue_feature_init_by_devid(unsigned int dev_id);

/* ============================================================================
 * All chip version feature table declarations
 * ============================================================================
 *
 * Chip type mapping (code and config tables use UDA chip_type enum values):
 * HISI_MINI_V1  (0)  -> queue_get_chip_features(HISI_MINI_V1)
 * HISI_CLOUD_V1 (1)  -> queue_get_chip_features(HISI_CLOUD_V1)
 * HISI_MINI_V2  (2)  -> queue_get_chip_features(HISI_MINI_V2)
 * HISI_CLOUD_V2 (3)  -> queue_get_chip_features(HISI_CLOUD_V2)
 * HISI_MINI_V3  (4)  -> queue_get_chip_features(HISI_MINI_V3)
 * HISI_CLOUD_V4 (6)  -> queue_get_chip_features(HISI_CLOUD_V4)
 * HISI_CLOUD_V5 (7)  -> queue_get_chip_features(HISI_CLOUD_V5)
 * HISI_MINI_V4  (10) -> queue_get_chip_features(HISI_MINI_V4)
 *
 * Note: Use queue_get_chip_features() function to access chip features.
 *       Do not access global variables directly.
 */

#endif /* QUEUE_FEATURE_CHECK_H */
