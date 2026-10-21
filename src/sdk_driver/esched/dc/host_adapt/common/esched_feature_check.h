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

#ifndef ESCHED_FEATURE_CHECK_H
#define ESCHED_FEATURE_CHECK_H

#include "esched_feature_kernel.h"
#include "ascend_hal_error.h"

/*
 * ============================================================================
 * Chip version feature configuration structure
 * Each chip version corresponds to a feature array
 * ============================================================================
 */
struct esched_chip_features {
    const enum esched_feature_id *features;
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
void esched_feature_system_init(void);

/*
 * Get feature configuration by chip type
 * @param chip_type: UDA chip type, e.g., HISI_CLOUD_V2, HISI_CLOUD_V5
 * @return: pointer to corresponding feature configuration, NULL if not found
 */
const struct esched_chip_features *esched_get_chip_features(unsigned int chip_type);

/*
 * Get the feature configuration for a specific device
 * @param dev_id: device ID (0 ~ SCHED_MAX_CHIP_NUM - 1)
 * @return: pointer to device's feature configuration, NULL if device not found
 */
const struct esched_chip_features *esched_get_dev_features(unsigned int dev_id);

int esched_has_feature(unsigned int dev_id, enum esched_feature_id feature);
void esched_feature_uninit(void);
drvError_t esched_feature_init_by_devid(unsigned int dev_id);

#endif /* ESCHED_FEATURE_CHECK_H */
