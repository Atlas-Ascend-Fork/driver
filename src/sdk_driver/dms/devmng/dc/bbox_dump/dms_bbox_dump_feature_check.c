/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd. All rights reserved.
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

#include "dms_bbox_dump_feature_check.h"
#include "dms_bbox_dump.h"
#include "dms_bbox_dump_chip_feature_map.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_dev_identity.h"
#include "pbl/pbl_feature_loader.h"
#include "dms_template.h"

#define DMS_MODULE_BBOX_FEATURE "dms_bbox_feature"

int dms_get_bbox_feature(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_BBOX_FEATURE)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_BBOX_FEATURE, DMS_MAIN_CMD_BBOX, DMS_SUBCMD_GET_BBOX_FEATURE, NULL, NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT, dms_get_bbox_feature)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

static const struct bbox_chip_features g_bbox_hisi_cloud_v2_features = {
    .features = hisi_cloud_v2_bbox_features,
    .feature_count = sizeof(hisi_cloud_v2_bbox_features) / sizeof(hisi_cloud_v2_bbox_features[0])};

static const struct bbox_chip_features g_bbox_hisi_mini_v3_features = {
    .features = hisi_mini_v3_bbox_features,
    .feature_count = sizeof(hisi_mini_v3_bbox_features) / sizeof(hisi_mini_v3_bbox_features[0])};

static const struct bbox_chip_features g_bbox_hisi_cloud_v4_features = {
    .features = hisi_cloud_v4_bbox_features,
    .feature_count = sizeof(hisi_cloud_v4_bbox_features) / sizeof(hisi_cloud_v4_bbox_features[0])};

static const struct bbox_chip_features g_bbox_hisi_cloud_v5_features = {
    .features = hisi_cloud_v5_bbox_features,
    .feature_count = sizeof(hisi_cloud_v5_bbox_features) / sizeof(hisi_cloud_v5_bbox_features[0])};

static const struct bbox_chip_features g_bbox_hisi_mini_v4_features = {
    .features = hisi_mini_v4_bbox_features,
    .feature_count = sizeof(hisi_mini_v4_bbox_features) / sizeof(hisi_mini_v4_bbox_features[0])};

#define BBOX_CHIP_FEATURE_TABLE_SIZE (HISI_MINI_V4 + 1)

static const struct bbox_chip_features *g_bbox_chip_feature_table[BBOX_CHIP_FEATURE_TABLE_SIZE] = {
    [HISI_CLOUD_V2] = &g_bbox_hisi_cloud_v2_features, [HISI_MINI_V3] = &g_bbox_hisi_mini_v3_features,
    [HISI_CLOUD_V4] = &g_bbox_hisi_cloud_v4_features, [HISI_CLOUD_V5] = &g_bbox_hisi_cloud_v5_features,
    [HISI_MINI_V4] = &g_bbox_hisi_mini_v4_features,
};

static unsigned int g_bbox_dev_chip_map[ASCEND_DEV_MAX_NUM];

void bbox_feature_reset(void)
{
    unsigned int i;

    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        g_bbox_dev_chip_map[i] = HISI_CHIP_UNKNOWN;
    }
}

const struct bbox_chip_features *bbox_get_chip_features(unsigned int chip_type)
{
    if (chip_type >= BBOX_CHIP_FEATURE_TABLE_SIZE) {
        return NULL;
    }
    return g_bbox_chip_feature_table[chip_type];
}

static const struct bbox_chip_features *bbox_get_verified_features(unsigned int dev_id)
{
    unsigned int chip_type;
    const struct bbox_chip_features *feat;

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        dms_err("Invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, ASCEND_DEV_MAX_NUM);
        return NULL;
    }
    chip_type = g_bbox_dev_chip_map[dev_id];
    if (chip_type == HISI_CHIP_UNKNOWN) {
        dms_debug("Feature not initialized. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    feat = bbox_get_chip_features(chip_type);
    if (feat == NULL) {
        dms_err("No feature config for chip_type=%u (dev_id=%u)\n", chip_type, dev_id);
        return NULL;
    }
    return feat;
}

int bbox_has_feature(unsigned int dev_id, enum bbox_feature_id feature)
{
    const struct bbox_chip_features *feat = bbox_get_verified_features(dev_id);
    if (feat == NULL) {
        return 0;
    }
    return bbox_feature_array_has(feat->features, feat->feature_count, feature);
}

unsigned long long bbox_get_feature_bitmap(unsigned int dev_id)
{
    const struct bbox_chip_features *feat = bbox_get_verified_features(dev_id);
    unsigned long long bitmap = 0;
    unsigned int i;

    if (feat == NULL) {
        return 0;
    }
    for (i = 0; i < feat->feature_count; i++) {
        bitmap |= (1ULL << feat->features[i]);
    }
    return bitmap;
}

drvError_t bbox_feature_init_by_devid(unsigned int dev_id)
{
    unsigned int chip_type;
    const struct bbox_chip_features *chip_features;

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        dms_err("Invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    chip_type = uda_get_chip_type(dev_id);
    if (chip_type == HISI_CHIP_UNKNOWN) {
        dms_err("Unknown chip type. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    chip_features = bbox_get_chip_features(chip_type);
    if (chip_features == NULL) {
        dms_err("No feature config for chip_type=%u (dev_id=%u)\n", chip_type, dev_id);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (g_bbox_dev_chip_map[dev_id] != HISI_CHIP_UNKNOWN) {
        dms_info("Feature already cached. (dev_id=%u; chip_type=%u)\n", dev_id, g_bbox_dev_chip_map[dev_id]);
        return DRV_ERROR_NONE;
    }

    g_bbox_dev_chip_map[dev_id] = chip_type;

    dms_info("Feature cache populated. (dev_id=%u; chip_type=%u; feature_count=%u)\n", dev_id, chip_type,
             chip_features->feature_count);

    return DRV_ERROR_NONE;
}

static int bbox_common_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= ASCEND_DEV_MAX_NUM) {
        dms_err("Invalid para. (dev_id=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = bbox_feature_init_by_devid(udevid);
        if (ret != DRV_ERROR_NONE) {
            dms_err("Feature init failed. (ret=%d; udevid=%u)\n", ret, udevid);
            return ret;
        }
    }

    dms_info("Bbox feature notifier action. (dev_id=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}

int dms_get_bbox_feature(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    unsigned int i;

    UNUSED(feature);
    UNUSED(in);
    UNUSED(in_len);

    if (out == NULL || out_len < sizeof(unsigned long long) * ASCEND_DEV_MAX_NUM) {
        dms_err("Invalid output for batch query.\n");
        return -EINVAL;
    }
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        ((unsigned long long *)out)[i] = bbox_get_feature_bitmap(i);
    }
    dms_info("Bbox feature bitmap batch queried for all devices.\n");
    return 0;
}

static int dms_bbox_dump_feature_check_init(void)
{
    struct uda_dev_type type;
    int ret;

    bbox_feature_reset();

    CALL_INIT_MODULE(DMS_MODULE_BBOX_FEATURE);
    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(DMS_MODULE_BBOX_FEATURE, &type, UDA_PRI2, bbox_common_notifier_func);
    if (ret != 0) {
        CALL_EXIT_MODULE(DMS_MODULE_BBOX_FEATURE);
        dms_err("Failed to register bbox feature notifier. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_bbox_dump_feature_check_init, FEATURE_LOADER_STAGE_5);

static void dms_bbox_dump_feature_check_uninit(void)
{
    struct uda_dev_type type;

    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(DMS_MODULE_BBOX_FEATURE, &type);
    CALL_EXIT_MODULE(DMS_MODULE_BBOX_FEATURE);
    bbox_feature_reset();
}
DECLAER_FEATURE_AUTO_UNINIT(dms_bbox_dump_feature_check_uninit, FEATURE_LOADER_STAGE_5);
