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

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "securec.h"
#include "esched_feature.h"
#include "event_sched.h"
#include "davinci_interface.h"

/* Per-device feature bitmap cache (populated by esched_dev_feature_init_all) */
unsigned char g_esched_dev_feature[ESCHED_FEATURE_DEV_NUM][ESCHED_FEATURE_MAX] = {{0}};

/*
 * Internal helper: send ESCHED_FEATURE_GET ioctl with the given fd.
 */
drvError_t esched_feature_ioctl_get_all(int fd, struct esched_feature_get_arg *all_features)
{
    int ret;

    ret = esched_ioctl(fd, ESCHED_FEATURE_GET, all_features);
    if (ret != 0) {
        sched_err("esched_feature_ioctl_get_all: ioctl ESCHED_FEATURE_GET failed. (ret=%d, err=%d)\n", ret, errno);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

bool esched_has_dev_feature(unsigned int dev_id, unsigned int feature_id)
{
    if (esched_dev_init(dev_id) != DRV_ERROR_NONE) {
        sched_err("esched dev feature init failed. (dev_id=%u)\n", dev_id);
        return false;
    }
    if (dev_id >= ESCHED_FEATURE_DEV_NUM) {
        sched_err("esched_has_dev_feature: invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, ESCHED_FEATURE_DEV_NUM);
        return false;
    }
    if (feature_id >= ESCHED_FEATURE_MAX) {
        sched_err("esched_has_dev_feature: invalid feature_id. (feature_id=%u; max=%u)\n", feature_id,
                  ESCHED_FEATURE_MAX);
        return false;
    }
    return g_esched_dev_feature[dev_id][feature_id] != 0;
}

/*
 * Version-package feature mapping table.
 * Each entry maps a drvFeature_t to an esched_feature_id.
 * Lookup is done by iterating this table - add entries as needed.
 */
struct esched_version_feature_map {
    drvFeature_t drv_feature;
    enum esched_feature_id esched_feature;
};

static const struct esched_version_feature_map g_esched_feature_map[] = {
    /* Add version feature mappings here in the future */
};

bool esched_has_ver_feature(unsigned int dev_id, drvFeature_t feature)
{
    unsigned int i;
    unsigned int table_size = sizeof(g_esched_feature_map) / sizeof(struct esched_version_feature_map);
    if (esched_dev_init(dev_id) != DRV_ERROR_NONE) {
        sched_err("esched dev feature init failed. (dev_id=%u)\n", dev_id);
        return false;
    }
    if (dev_id >= ESCHED_FEATURE_DEV_NUM) {
        sched_err("esched_has_ver_feature: invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, ESCHED_FEATURE_DEV_NUM);
        return false;
    }
    if (feature >= FEATURE_MAX) {
        return false;
    }

    for (i = 0; i < table_size; i++) {
        if (g_esched_feature_map[i].drv_feature == feature) {
            return g_esched_dev_feature[dev_id][g_esched_feature_map[i].esched_feature] != 0;
        }
    }
    return false;
}
