/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "pbl/pbl_feature_user.h"
#include "ascend_hal_define.h"
#include "ascend_dev_num.h"
#include "dms_user_common.h"
#include "dms_ioctl.h"
#include "dms_feature.h"

#define DMS_BITMAP_BITS_PER_ELEM (sizeof(unsigned long long) * CHAR_BIT)
#define DMS_BITMAP_MAX_LEN \
    (uint32_t)(((uint32_t)(DMS_FEATURE_MAX) + DMS_BITMAP_BITS_PER_ELEM - 1U) / DMS_BITMAP_BITS_PER_ELEM)
static unsigned long long g_dms_feature_bitmap[ASCEND_DEV_MAX_NUM][DMS_BITMAP_MAX_LEN] = {0};
static uint32_t g_dms_feature_bitmap_len = DMS_BITMAP_MAX_LEN;
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define DMS_FEATURE_QUERY_ALL_DEV 0xFFFFFFFF

struct dms_feature_type {
    drvFeature_t type;
    enum dms_feature_id dms_feature;
};
STATIC struct dms_feature_type g_dms_feature_type[] = {{FEATURE_MAX, DMS_FEATURE_MAX}};

STATIC int dms_feature_trans(drvFeature_t type, enum dms_feature_id *dms_feature)
{
    uint32_t i;

    for (i = 0; i < ARRAY_SIZE(g_dms_feature_type); i++) {
        if (g_dms_feature_type[i].type == type) {
            *dms_feature = g_dms_feature_type[i].dms_feature;
            return 0;
        }
    }
    return -1;
}

/* Used in dms onebin framework, queries dms feature by dms-specific feature id. */
bool dms_has_feature_by_dms_feature_id(uint32_t devid, enum dms_feature_id dms_feature)
{
    uint32_t feat_col_idx, bit_idx, bit;

    if (devid >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("Invalid devid. (devid=%u; max_devid=%u)\n", devid, ASCEND_DEV_MAX_NUM);
        return false;
    }

    if (dms_feature >= DMS_FEATURE_MAX) {
        DMS_INFO("Not support dms feature, over max type. (devid=%u; dms_feature=0x%x; max_type=0x%x)\n", devid,
                 dms_feature, DMS_FEATURE_MAX);
        return false;
    }

    feat_col_idx = (uint32_t)dms_feature / DMS_BITMAP_BITS_PER_ELEM;
    bit_idx = (uint32_t)dms_feature % DMS_BITMAP_BITS_PER_ELEM;
    bit = (g_dms_feature_bitmap[devid][feat_col_idx] >> bit_idx) & (0x1);

    return (bit == 1);
}

bool dms_has_feature(uint32_t devid, drvFeature_t type)
{
    enum dms_feature_id dms_feature = 0;

    if (devid >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("Invalid devid. (devid=%u; max_devid=%u)\n", devid, ASCEND_DEV_MAX_NUM);
        return false;
    }

    if (type >= FEATURE_MAX) {
        DMS_INFO("Not support type, over max type. (devid=%u; type=0x%x; max_type=0x%x)\n", devid, type, FEATURE_MAX);
        return false;
    }

    if (dms_feature_trans(type, &dms_feature) != 0) {
        DMS_INFO("Not support type. (devid=%u; type=0x%x)\n", devid, type);
        return false;
    }

    return dms_has_feature_by_dms_feature_id(devid, dms_feature);
}

STATIC void __attribute__((constructor)) dms_query_feature_init(void)
{
    int ret;

    ret = drv_query_module_features(DMS_FEATURE_QUERY_ALL_DEV, "dms", (unsigned long long *)g_dms_feature_bitmap,
                                    &g_dms_feature_bitmap_len);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "query module features fail. (ret=%d)\n", ret);
    }
}
