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

#ifndef _HDCDRV_FEATURE_H_
#define _HDCDRV_FEATURE_H_

#include <stdbool.h>

typedef unsigned long long hdcdrv_feature_bitmap_t;

#define HDCDRV_FEATURE_ENABLE 1
#define HDCDRV_FEATURE_DISABLE 0

enum hdcdrv_feature {
    HDCDRV_FEATURE_TSD_NOTIFY = 0,
    HDCDRV_FEATURE_FAST_CHANNEL,
    HDCDRV_FEATURE_MAX
};

#define FEATURE_BIT(feat) (1ULL << (feat))

struct hdcdrv_dev_chip_info {
    u32 chip_type;
    u32 status;
};

int hdcdrv_feature_get_chip_info(unsigned int dev_id, struct hdcdrv_dev_chip_info *chip_info);
int hdcdrv_feature_get_chip_features(u32 chip_type, hdcdrv_feature_bitmap_t *chip_features);
int hdcdrv_feature_is_support(unsigned int dev_id, enum hdcdrv_feature feature, u32 *is_support);
int hdcdrv_feature_init_by_chip(unsigned int devid);
int hdcdrv_feature_uninit_by_chip(unsigned int devid);
int hdcdrv_feature_printf_chip_info(char *buf, u32 buf_len);
u32 hdcdrv_get_chip_type(u32 devid);

#endif /* _HDCDRV_FEATURE_H_ */
