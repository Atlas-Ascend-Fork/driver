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
#include "pbl/pbl_uda.h"

#define CHIP_FEAT_MINI_V1 0
#define CHIP_FEAT_CLOUD_V1 FEATURE_BIT(HDCDRV_FEATURE_TSD_NOTIFY) | FEATURE_BIT(HDCDRV_FEATURE_FAST_CHANNEL)
#define CHIP_FEAT_MINI_V2 FEATURE_BIT(HDCDRV_FEATURE_TSD_NOTIFY) | FEATURE_BIT(HDCDRV_FEATURE_FAST_CHANNEL)
#define CHIP_FEAT_CLOUD_V2 0
#define CHIP_FEAT_MINI_V3 FEATURE_BIT(HDCDRV_FEATURE_TSD_NOTIFY) | FEATURE_BIT(HDCDRV_FEATURE_FAST_CHANNEL)
#define CHIP_FEAT_RESERVED_1 FEATURE_BIT(HDCDRV_FEATURE_FAST_CHANNEL)
#define CHIP_FEAT_CLOUD_V4 0
#define CHIP_FEAT_CLOUD_V5 0
#define CHIP_FEAT_RESERVED_2 FEATURE_BIT(HDCDRV_FEATURE_FAST_CHANNEL)
#define CHIP_FEAT_RESERVED_3 FEATURE_BIT(HDCDRV_FEATURE_FAST_CHANNEL)
#define CHIP_FEAT_MINI_V4 0

static const hdcdrv_feature_bitmap_t g_chip_features[HISI_CHIP_NUM] = {
    CHIP_FEAT_MINI_V1,    CHIP_FEAT_CLOUD_V1,   CHIP_FEAT_MINI_V2,  CHIP_FEAT_CLOUD_V2,
    CHIP_FEAT_MINI_V3,    CHIP_FEAT_RESERVED_1, CHIP_FEAT_CLOUD_V4, CHIP_FEAT_CLOUD_V5,
    CHIP_FEAT_RESERVED_2, CHIP_FEAT_RESERVED_3, CHIP_FEAT_MINI_V4};

static const char *g_chip_type_str[HISI_CHIP_NUM] = {"MINI_V1",    "CLOUD_V1",   "MINI_V2",  "CLOUD_V2",
                                                     "MINI_V3",    "RESERVED_1", "CLOUD_V4", "CLOUD_V5",
                                                     "RESERVED_2", "RESERVED_3", "MINI_V4"};

static const char *g_feature_str[HDCDRV_FEATURE_MAX] = {"TSD_NOTIFY", "FAST_CHANNEL"};

static struct hdcdrv_dev_chip_info g_chip_info[HDCDRV_SUPPORT_MAX_DEV] = {
    [0 ... HDCDRV_SUPPORT_MAX_DEV - 1] = {.chip_type = HISI_CHIP_UNKNOWN, .status = 0}};

int hdcdrv_feature_get_chip_info(unsigned int dev_id, struct hdcdrv_dev_chip_info *chip_info)
{
    if (chip_info == NULL) {
        hdcdrv_err("Input parameter is error. (dev_id=%u)\n", dev_id);
        return HDCDRV_ERR;
    }

    if (dev_id >= HDCDRV_SUPPORT_MAX_DEV) {
        hdcdrv_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return HDCDRV_ERR;
    }

    *chip_info = g_chip_info[dev_id];
    return HDCDRV_OK;
}

int hdcdrv_feature_get_chip_features(u32 chip_type, hdcdrv_feature_bitmap_t *chip_features)
{
    if (chip_features == NULL) {
        hdcdrv_err("Input parameter is error. (chip_type=%u)\n", chip_type);
        return HDCDRV_ERR;
    }

    if (chip_type >= HISI_CHIP_NUM) {
        hdcdrv_err("Invalid chip type. (chip_type=%u)\n", chip_type);
        return HDCDRV_ERR;
    }

    *chip_features = g_chip_features[chip_type];
    return HDCDRV_OK;
}

int hdcdrv_feature_init_by_chip(unsigned int devid)
{
    unsigned int chip_type = 0;

    if (devid >= (u32)hdcdrv_get_max_support_dev()) {
        hdcdrv_err("Input parameter is error. (devid=%u; max=%d)\n", devid, hdcdrv_get_max_support_dev());
        return HDCDRV_ERR;
    }

    chip_type = hdcdrv_get_chip_type(devid);
    if (chip_type >= HISI_CHIP_NUM) {
        hdcdrv_err("Invalid chip type. (devid=%u, chip_type=%u)\n", devid, chip_type);
        return HDCDRV_ERR;
    }

    g_chip_info[devid].chip_type = chip_type;
    g_chip_info[devid].status = 1;
    return 0;
}

int hdcdrv_feature_uninit_by_chip(unsigned int devid)
{
    if (devid >= (u32)hdcdrv_get_max_support_dev()) {
        hdcdrv_err("Input parameter is error. (devid=%u; max=%d)\n", devid, hdcdrv_get_max_support_dev());
        return HDCDRV_ERR;
    }

    g_chip_info[devid].chip_type = HISI_CHIP_UNKNOWN;
    g_chip_info[devid].status = 0;
    return 0;
}

int hdcdrv_feature_printf_chip_info(char *buf, u32 buf_len)
{
    ssize_t offset = 0;
    u32 dev_id;
    int ret;
    int i;

    if (buf == NULL) {
        hdcdrv_err("Input parameter is error.\n");
        return HDCDRV_ERR;
    }

    ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1),
                     "dev_id\tchip_type\tfeature\n");
    if (ret >= 0) {
        offset += ret;
    }

    for (dev_id = 0; dev_id < HDCDRV_SUPPORT_MAX_DEV; dev_id++) {
        if (g_chip_info[dev_id].status == 0) {
            continue;
        }

        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1), "%u\t%u(%s)\t",
                         dev_id, g_chip_info[dev_id].chip_type, g_chip_type_str[g_chip_info[dev_id].chip_type]);
        if (ret >= 0) {
            offset += ret;
        }

        for (i = 0; i < HDCDRV_FEATURE_MAX; i++) {
            if ((g_chip_features[g_chip_info[dev_id].chip_type] & FEATURE_BIT(i)) == 0) {
                continue;
            }
            ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1), "%s ",
                             g_feature_str[i]);
            if (ret >= 0) {
                offset += ret;
            }
        }

        ret = snprintf_s(buf + offset, (size_t)(buf_len - offset), (size_t)(buf_len - offset - 1), "\n");
        if (ret >= 0) {
            offset += ret;
        }
    }

    return (int)offset;
}
