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

#ifndef ESCHED_FEATURE_H
#define ESCHED_FEATURE_H

#include <stdbool.h>
#include "ascend_hal_define.h"
#include "esched_ioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Per-device feature bitmap cache, populated by esched_dev_feature_init_all */
extern unsigned char g_esched_dev_feature[ESCHED_FEATURE_DEV_NUM][ESCHED_FEATURE_MAX];

/*
 * Send ESCHED_FEATURE_GET ioctl with the given fd.
 */
drvError_t esched_feature_ioctl_get_all(int fd, struct esched_feature_get_arg *all_features);

/*
 * Initialize the per-device feature bitmap by querying the kernel.
 * Must be called once after the esched device is opened.
 * Returns DRV_ERROR_NONE on success.
 */
drvError_t esched_dev_feature_init_all(int fd);

/*
 * Check if a specific feature is available for the given device.
 * @param dev_id: device ID (0 ~ ESCHED_FEATURE_DEV_NUM - 1)
 * @param feature_id: feature ID to check (0 ~ ESCHED_FEATURE_MAX - 1)
 * @return: true if the feature is supported, false otherwise
 */
bool esched_has_dev_feature(unsigned int dev_id, unsigned int feature_id);

/*
 * Version-package feature interface.
 * Query whether a drvFeature_t is supported by the current driver version package.
 * Mapping entries are defined in esched_feature.c (g_esched_feature_map[]).
 */
bool esched_has_ver_feature(unsigned int dev_id, drvFeature_t feature);

#ifdef __cplusplus
}
#endif

#endif /* ESCHED_FEATURE_H */
