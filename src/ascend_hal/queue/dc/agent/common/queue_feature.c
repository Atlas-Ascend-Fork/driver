/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "securec.h"
#include "ascend_hal_base.h"

#include "queue_ioctl.h"
#include "queue_kernel_api.h"

#include "queue_feature.h"
#include "queue_client_base.h"
static unsigned char g_queue_dev_feature[QUEUE_FEATURE_DEV_NUM][QUEUE_FEATURE_MAX] = {{0}};
static bool g_queue_feature_inited = false;
static pthread_mutex_t g_feature_init_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Initialize device feature bitmaps from kernel.
 *
 * Opens the queue device fd if not already open, queries all features,
 * and caches the result. Thread-safe via DCL (double-checked locking).
 */
static drvError_t queue_dev_feature_init_all(void)
{
    static struct queue_ioctl_feature all_features;
    drvError_t ret;

    pthread_mutex_lock(&g_feature_init_mutex);
    if (g_queue_feature_inited) {
        pthread_mutex_unlock(&g_feature_init_mutex);
        return DRV_ERROR_NONE;
    }

    if (queue_open_dev_base() < 0) {
        pthread_mutex_unlock(&g_feature_init_mutex);
        QUEUE_LOG_ERR("queue open dev base failed.\n");
        return DRV_ERROR_IOCRL_FAIL;
    }

    ret = queue_get_all_feature(&all_features);
    if (ret != DRV_ERROR_NONE) {
        pthread_mutex_unlock(&g_feature_init_mutex);
        QUEUE_LOG_ERR("queue_get_all_feature failed.\n");
        return DRV_ERROR_IOCRL_FAIL;
    }
    (void)memcpy_s(g_queue_dev_feature, sizeof(g_queue_dev_feature), all_features.feature_bitmaps,
                   sizeof(all_features.feature_bitmaps));

    g_queue_feature_inited = true;
    if (g_queue_tgid == -1) {
        g_queue_tgid = (int)drvDeviceGetBareTgid();
        if (g_queue_tgid == -1) {
            QUEUE_LOG_ERR("drvDeviceGetBareTgid failed.\n");
        }
    }
    pthread_mutex_unlock(&g_feature_init_mutex);
    QUEUE_RUN_LOG_INFO("Queue dev feature init all from kernel.\n");
    return DRV_ERROR_NONE;
}

bool queue_has_dev_feature(unsigned int dev_id, unsigned int feature_id)
{
    if (queue_dev_feature_init_all() != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue dev feature init failed.\n");
        return false;
    }

    if (dev_id >= QUEUE_FEATURE_DEV_NUM) {
        QUEUE_LOG_ERR("Invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, QUEUE_FEATURE_DEV_NUM);
        return false;
    }
    if (feature_id >= QUEUE_FEATURE_MAX) {
        QUEUE_LOG_ERR("Invalid feature_id. (feature_id=%u; max=%u)\n", feature_id, QUEUE_FEATURE_MAX);
        return false;
    }
    return g_queue_dev_feature[dev_id][feature_id] != 0;
}

#ifndef CFG_FEATURE_QUE_SUPPORT_UB
/*
 * Version-package feature mapping table.
 * Each entry maps a drvFeature_t to a queue_feature_id.
 * Lookup is done by iterating this table - add entries as needed.
 */
struct queue_version_feature_map {
    drvFeature_t drv_feature;
    enum queue_feature_id queue_feature;
};

static const struct queue_version_feature_map g_queue_feature_map[] = {
    /* Add version feature mappings here in the future */
};
#endif

bool queue_has_ver_feature(unsigned int dev_id, drvFeature_t feature)
{
#ifdef CFG_FEATURE_QUE_SUPPORT_UB
    (void)dev_id;
    (void)feature;
    return false;
#else
    unsigned int i;
    unsigned int table_size = sizeof(g_queue_feature_map) / sizeof(struct queue_version_feature_map);

    if (queue_dev_feature_init_all() != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue dev feature init failed.\n");
        return false;
    }

    if (dev_id >= QUEUE_FEATURE_DEV_NUM) {
        QUEUE_LOG_ERR("Invalid dev_id. (dev_id=%u; max=%u)\n", dev_id, QUEUE_FEATURE_DEV_NUM);
        return false;
    }
    if (feature >= FEATURE_MAX) {
        return false;
    }

    for (i = 0; i < table_size; i++) {
        if (g_queue_feature_map[i].drv_feature == feature) {
            return g_queue_dev_feature[dev_id][g_queue_feature_map[i].queue_feature] != 0;
        }
    }
    return false;
#endif
}
