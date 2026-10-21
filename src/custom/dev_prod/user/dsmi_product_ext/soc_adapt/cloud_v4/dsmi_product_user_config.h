/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DSMI_PRODUCT_USER_CONFIG_H
#define __DSMI_PRODUCT_USER_CONFIG_H

#include "devdrv_user_config_common.h"

#define UC_PROD_ITEM_MAX_NUM 2 // 新增配置项时，需要同步修改

#define GUID_INFO "guid_info"
#define GUID_INFO_BLOCK_OFFSET 0x2140000 // 具体范围查看 g_flash_ctrl
#define GUID_INFO_CONFIG_SIZE 32

#define DEFAULT_EXPIERD_THRESHOLD ("0x5A")    // default: 90 days
#define MB_CONFIG_MAIN_BLOCK_OFFSET 0x21D0000 // 具体范围查看 g_flash_ctrl
#define MB_CONFIG_EXPIRED_OFFSET 0

#ifdef DSMI_PRODUCT_USER_INTERFACE
int cert_expired_config_para_check(unsigned int config_cmd, unsigned char *buf, unsigned int buf_size);
#define CERT_EXPIRED_CONFIG_PARA_CHECK cert_expired_config_para_check
#else
#define CERT_EXPIRED_CONFIG_PARA_CHECK NULL
#endif

const struct user_config_item_product g_user_config_info_product[UC_PROD_ITEM_MAX_NUM] = {
    {GUID_INFO, NULL, FLASH_BLOCK_COMMON, GUID_INFO_BLOCK_OFFSET, 0, GUID_INFO_CONFIG_SIZE,
     SUPPORT_SET_USER_CFG | SUPPORT_GET_USER_CFG, UC_PRO_AUTHORITY_USER_WR, UC_SUPPORT_ALL, NULL, NULL},
    {CERT_EXPIRED_THRESHOLD, NULL, FLASH_BLOCK_MB, MB_CONFIG_MAIN_BLOCK_OFFSET, MB_CONFIG_EXPIRED_OFFSET,
     MB_CONFIG_EXPIRED_SIZE, SUPPORT_ALL_CFG_CFG, UC_PRO_AUTHORITY_USER_WR, UC_SUPPORT_ALL, DEFAULT_EXPIERD_THRESHOLD,
     CERT_EXPIRED_CONFIG_PARA_CHECK},
};

#endif