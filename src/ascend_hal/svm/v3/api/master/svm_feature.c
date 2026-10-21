/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <pthread.h>

#include "ascend_hal.h"
#include "pbl_urd_sub_cmd_common.h"
#include "pbl_urd_main_cmd_def.h"
#include "pbl_urd_user.h"
#include "bitmap.h"

#include "svm_log.h"
#include "svm_sys_cmd.h"
#include "svm_init_pri.h"
#include "dbi_def.h"
#include "dbi_ioctl.h"

struct svm_feature_trans {
    drvFeature_t feature;
    u32 feature_bit;
};

static struct svm_feature_trans g_svm_feature_trans[] = {
    {FEATURE_MAX, SVM_DEV_CAP_BIT_MAX},
};

static pthread_mutex_t g_feature_mutex = PTHREAD_MUTEX_INITIALIZER;
static svm_dev_cap_t g_feature_bitmap[SVM_MAX_DEV_NUM] = {0};
static bool g_feature_inited[SVM_MAX_DEV_NUM] = {false};

static int svm_dev_feature_query(u32 devid, svm_dev_cap_t *bitmap)
{
    struct svm_dbi_cap_query_para para = {.devid = devid, .bitmap_size = sizeof(svm_dev_cap_t)};
    struct urd_cmd_para cmd_para = {
        .input = (void *)&para, .input_len = sizeof(para), .output = (void *)&para, .output_len = sizeof(para)};
    struct urd_cmd cmd = {
        .main_cmd = DMS_MAIN_CMD_SVM, .sub_cmd = DMS_SUBCMD_QUERY_SVM_FEATURE, .filter = NULL, .filter_len = 0};
    int ret;

    ret = urd_dev_usr_cmd(devid, &cmd, &cmd_para);
    if (ret != 0) {
        svm_run_info("Feature ioctl query completed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    ret = memcpy_s(bitmap, sizeof(svm_dev_cap_t), para.cap_bitmap, sizeof(svm_dev_cap_t));
    if (ret != 0) {
        return DRV_ERROR_INVALID_VALUE;
    }
    return 0;
}

int svm_dev_feature_init(u32 devid)
{
    int ret;

    (void)pthread_mutex_lock(&g_feature_mutex);
    if (!g_feature_inited[devid]) {
        ret = svm_dev_feature_query(devid, &g_feature_bitmap[devid]);
        if (ret != 0) {
            (void)pthread_mutex_unlock(&g_feature_mutex);
            svm_run_info("Svm feature dev init completed. (ret=%d; devid=%u)\n", ret, devid);
            return ret;
        }
        g_feature_inited[devid] = true;
    }
    (void)pthread_mutex_unlock(&g_feature_mutex);

    return 0;
}

static bool _svm_feature_is_support(u32 devid, drvFeature_t type)
{
    struct svm_feature_trans *trans = NULL;
    u32 i;

    for (i = 0; i < (u32)(sizeof(g_svm_feature_trans) / sizeof(g_svm_feature_trans[0])); i++) {
        trans = &g_svm_feature_trans[i];
        if (trans->feature == type) {
            return svm_dev_cap_is_support(g_feature_bitmap[devid], trans->feature_bit);
        }
    }

    return false;
}

bool svm_feature_is_support(uint32_t devid, drvFeature_t type)
{
    if ((devid >= SVM_MAX_DEV_NUM) || (type >= FEATURE_MAX)) {
        svm_run_info("Para check completed. (devid=%u; type=%u; dev_max=%u; feature_max=%u)\n", devid, (u32)type,
                     (u32)SVM_MAX_DEV_NUM, (u32)FEATURE_MAX);
        return false;
    }

    if (!g_feature_inited[devid]) {
        if (svm_dev_feature_init(devid) != 0) {
            svm_run_info("Svm feature uninit record. (devid=%u)\n", devid);
            return false;
        }
    }

    return _svm_feature_is_support(devid, type);
}

void __attribute__((constructor(SVM_INIT_PRI_FISRT))) svm_feature_init(void)
{
    int ret;

    ret = svm_register_ioctl_dev_init_post_handle(svm_dev_feature_init);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register svm dev_feature init post handle completed. (ret=%d)\n", ret);
    }
}
