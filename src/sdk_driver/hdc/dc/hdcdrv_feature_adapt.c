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

#include "hdcdrv_log.h"
#include "hdcdrv_feature.h"

int hdcdrv_feature_is_support(unsigned int dev_id, enum hdcdrv_feature feature, u32 *is_support)
{
    hdcdrv_feature_bitmap_t chip_features = 0;
    int ret;
    struct hdcdrv_dev_chip_info chip_info = {0};

    if (is_support == NULL) {
        hdcdrv_err("Input parameter is error. (dev_id=%u, feature=%d)\n", dev_id, feature);
        return HDCDRV_ERR;
    }

    *is_support = HDCDRV_FEATURE_DISABLE;

    if (feature >= HDCDRV_FEATURE_MAX) {
        hdcdrv_err("Invalid hdcdrv feature. (feature=%d;feature_max_num=%d;)\n", feature, HDCDRV_FEATURE_MAX - 1);
        return HDCDRV_ERR;
    }

    ret = hdcdrv_feature_get_chip_info(dev_id, &chip_info);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    if (chip_info.status == 0) {
        hdcdrv_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return HDCDRV_ERR;
    }

    ret = hdcdrv_feature_get_chip_features(chip_info.chip_type, &chip_features);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    *is_support = ((chip_features & FEATURE_BIT(feature)) != 0) ? HDCDRV_FEATURE_ENABLE : HDCDRV_FEATURE_DISABLE;
    return HDCDRV_OK;
}
