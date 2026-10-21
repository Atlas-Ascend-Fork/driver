/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <string.h>
#include <pthread.h>
#include "securec.h"
#include "buff_feature.h"
#include "buff_manage_kernel_api.h"
#include "drv_buff_log.h"

static unsigned char g_buff_dev_feature[BUFF_FEATURE_DEV_NUM][BUFF_FEATURE_MAX] = {{0}};
static bool g_buff_feature_inited = false;
static pthread_mutex_t g_feature_init_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Initialize device feature bitmaps from kernel.
 * Idempotent and thread-safe: only the first call queries the kernel,
 * later calls return immediately. Safe to call on every feature query.
 */
drvError_t buff_dev_feature_init_all(void)
{
    static struct xsm_feature_get_arg all_features;
    drvError_t ret;

    pthread_mutex_lock(&g_feature_init_mutex);
    if (g_buff_feature_inited) {
        pthread_mutex_unlock(&g_feature_init_mutex);
        return DRV_ERROR_NONE;
    }

    ret = buff_pool_feature_get_all(&all_features);
    if (ret != DRV_ERROR_NONE) {
        pthread_mutex_unlock(&g_feature_init_mutex);
        buff_err("buff_dev_feature_init_all: query all features failed. (ret=%d)\n", ret);
        return ret;
    }
    (void)memcpy_s(g_buff_dev_feature, sizeof(g_buff_dev_feature), all_features.feature_bitmaps,
                   sizeof(all_features.feature_bitmaps));
    g_buff_feature_inited = true;
    pthread_mutex_unlock(&g_feature_init_mutex);
    return DRV_ERROR_NONE;
}

bool buff_has_dev_feature(unsigned int dev_id, unsigned int feature_id)
{
    if (buff_dev_feature_init_all() != DRV_ERROR_NONE) {
        buff_err("buff dev feature init failed.\n");
        return false;
    }

    if (dev_id >= BUFF_FEATURE_DEV_NUM) {
        buff_err("Invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, BUFF_FEATURE_DEV_NUM);
        return false;
    }
    if (feature_id >= BUFF_FEATURE_MAX) {
        buff_err("Invalid feature_id. (feature_id=%u; max=%u)\n", feature_id, BUFF_FEATURE_MAX);
        return false;
    }
    return g_buff_dev_feature[dev_id][feature_id] != 0;
}

/*
 * Version-package feature mapping table.
 * Each entry maps a drvFeature_t to a buff_feature_id.
 * Lookup is done by iterating this table - add entries as needed.
 */
struct buff_version_feature_map {
    drvFeature_t drv_feature;
    enum buff_feature_id buff_feature;
};

static const struct buff_version_feature_map g_buff_feature_map[] = {
    /* Add version feature mappings here in the future */
};

bool buff_has_ver_feature(unsigned int dev_id, drvFeature_t feature)
{
    unsigned int i;
    unsigned int table_size = sizeof(g_buff_feature_map) / sizeof(struct buff_version_feature_map);

    if (buff_dev_feature_init_all() != DRV_ERROR_NONE) {
        buff_err("buff dev feature init failed.\n");
        return false;
    }

    if (dev_id >= BUFF_FEATURE_DEV_NUM) {
        buff_err("Invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, BUFF_FEATURE_DEV_NUM);
        return false;
    }
    if (feature >= FEATURE_MAX) {
        return false;
    }

    for (i = 0; i < table_size; i++) {
        if (g_buff_feature_map[i].drv_feature == feature) {
            return g_buff_dev_feature[dev_id][g_buff_feature_map[i].buff_feature] != 0;
        }
    }
    return false;
}
