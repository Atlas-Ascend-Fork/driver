/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "pbl_feature_user.h"
#include "drv_comm_intf.h"
#include "ascend_dev_num.h"
#include "pbl/pbl_module_feature_msg.h"
#include "pbl/pbl_module_feature.h"
#include "pbl/pbl_urd_user.h"
#include "pbl/pbl_urd_main_cmd_def.h"
#include "pbl/pbl_urd_sub_cmd_common.h"

int drv_query_module_features(unsigned int dev_id, const char *module_name, unsigned long long *bitmap,
                              unsigned int *bitmap_len)
{
    int ret = 0;
    struct pbl_module_feature_arg arg = {0};
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    unsigned int bitmap_array_len = 0;

    if (module_name == NULL || bitmap == NULL || bitmap_len == NULL) {
        DRV_COMM_ERR("drv_query_module_features invalid null param. (module_name_is_null=%d; bitmap_is_null=%d; "
                     "bitmap_len_is_null=%d)\n",
                     (module_name == NULL), (bitmap == NULL), (bitmap_len == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }
    if (*bitmap_len == 0 || *bitmap_len > PBL_QUERY_FEATURE_BITMAP_MAX_WORDS) {
        DRV_COMM_ERR("drv_query_module_features invalid bitmap_len. (bitmap_len=%u; max=%d)\n", *bitmap_len,
                     PBL_QUERY_FEATURE_BITMAP_MAX_WORDS);
        return DRV_ERROR_INVALID_VALUE;
    }

    bitmap_array_len = (dev_id == PBL_QUERY_FEATURE_ALL_DEV) ? (ASCEND_DEV_MAX_NUM * (*bitmap_len)) : *bitmap_len;

    arg.dev_id = dev_id;
    arg.max_dev_num = ASCEND_DEV_MAX_NUM;
    arg.bitmap_buff = bitmap;
    arg.bitmap_buff_len = bitmap_array_len;
    arg.per_dev_bitmap_len = bitmap_len;

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_MODULE_FEATURE, module_name,
                     (unsigned int)strlen(module_name));
    urd_usr_cmd_para_fill(&cmd_para, &arg, (unsigned int)sizeof(arg), NULL, 0);

    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret != 0) {
        DRV_COMM_EX_NOTSUPPORT_ERR(ret, "Failed to get feature bitmap. (ret=%d; module=%s; dev_id=0x%x)\n", ret,
                                   module_name, dev_id);
    }
    return ret;
}

#define PBL_FEATURE_MODULE_NAME "pbl"
#define PBL_FEATURE_BITMAP_WORDS (((unsigned int)PBL_FEATURE_MAX + 63U) / 64U)

static unsigned long long g_pbl_feature_bitmap[ASCEND_DEV_MAX_NUM][PBL_FEATURE_BITMAP_WORDS];
static unsigned int g_pbl_feature_bitmap_words = 0;

static void __attribute__((constructor)) drv_module_feature_init(void)
{
    unsigned int bitmap_len = PBL_FEATURE_BITMAP_WORDS;
    int ret = drv_query_module_features(PBL_QUERY_FEATURE_ALL_DEV, PBL_FEATURE_MODULE_NAME, &g_pbl_feature_bitmap[0][0],
                                        &bitmap_len);
    if (ret == 0) {
        g_pbl_feature_bitmap_words = bitmap_len;
    }
}

bool pbl_support_feature(unsigned int dev_id, unsigned int feature_id)
{
    bool is_support_feature = false;

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        DRV_COMM_ERR("Invalid parameter. (dev_id=%u)\n", dev_id);
        return false;
    }

    /* Need to map feature_id to the corresponding pbl feature. No new features are added yet, so return false
     * directly. */

    return is_support_feature;
}
