/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "pbl/pbl_feature_user.h"
#include "ascend_hal_define.h"
#include "ascend_dev_num.h"
#include "dev_mon_log.h"
#include "dmc_feature_user.h"

#define DMC_BITMAP_BITS_PER_ELEM (sizeof(unsigned long long) * CHAR_BIT)
#define DMC_BITMAP_MAX_LEN \
    (uint32_t)(((uint32_t)(DMC_FEATURE_MAX) + DMC_BITMAP_BITS_PER_ELEM - 1U) / DMC_BITMAP_BITS_PER_ELEM)
static unsigned long long g_dmc_feature_bitmap[ASCEND_DEV_MAX_NUM][DMC_BITMAP_MAX_LEN] = {0};
static uint32_t g_dmc_feature_bitmap_len = DMC_BITMAP_MAX_LEN;
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define DMC_FEATURE_QUERY_ALL_DEV 0xFFFFFFFF

struct dmc_feature_type {
    drvFeature_t type;
    enum dmc_feature_id feature_id;
};
STATIC struct dmc_feature_type g_dmc_feature_type[] = {{FEATURE_MAX, DMC_FEATURE_MAX}};

STATIC int dmc_feature_trans(drvFeature_t type, enum dmc_feature_id *feature_id)
{
    uint32_t i;

    for (i = 0; i < ARRAY_SIZE(g_dmc_feature_type); i++) {
        if (g_dmc_feature_type[i].type == type) {
            *feature_id = g_dmc_feature_type[i].feature_id;
            return 0;
        }
    }
    return -1;
}

bool dmc_has_feature_id(uint32_t devid, enum dmc_feature_id feature_id)
{
    uint32_t feat_col_idx, bit_idx, bit;

    if (devid >= ASCEND_DEV_MAX_NUM) {
        DEV_MON_ERR("Invalid devid. (devid=%u; max_devid=%u)\n", devid, ASCEND_DEV_MAX_NUM);
        return false;
    }

    if (feature_id >= DMC_FEATURE_MAX) {
        DEV_MON_INFO("Not support type, over max type. (devid=%u; feature_id=0x%x; max_type=0x%x)\n", devid, feature_id,
                     DMC_FEATURE_MAX);
        return false;
    }

    feat_col_idx = (uint32_t)feature_id / DMC_BITMAP_BITS_PER_ELEM;
    bit_idx = (uint32_t)feature_id % DMC_BITMAP_BITS_PER_ELEM;
    bit = (g_dmc_feature_bitmap[devid][feat_col_idx] >> bit_idx) & (0x1);

    return bit == 1;
}

bool dmc_has_feature_type(uint32_t devid, drvFeature_t type)
{
    enum dmc_feature_id feature_id = 0;

    if (devid >= ASCEND_DEV_MAX_NUM) {
        DEV_MON_ERR("Invalid devid. (devid=%u; max_devid=%u)\n", devid, ASCEND_DEV_MAX_NUM);
        return false;
    }
    if (type >= FEATURE_MAX) {
        DEV_MON_INFO("Not support type, over max type. (devid=%u; type=0x%x; max_type=0x%x)\n", devid, type,
                     FEATURE_MAX);
        return false;
    }

    if (dmc_feature_trans(type, &feature_id) != 0) {
        DEV_MON_INFO("Not support type. (devid=%u; type=0x%x)\n", devid, type);
        return false;
    }

    return dmc_has_feature_id(devid, feature_id);
}

static void __attribute__((constructor)) dmc_query_feature_init(void)
{
    int ret;

    ret = drv_query_module_features(DMC_FEATURE_QUERY_ALL_DEV, "dmc", (unsigned long long *)g_dmc_feature_bitmap,
                                    &g_dmc_feature_bitmap_len);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "query module features fail. (ret=%d)\n", ret);
    }
}
