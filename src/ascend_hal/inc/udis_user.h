/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef UDIS_USER_H
#define UDIS_USER_H
#include "ascend_inpackage_hal.h"

typedef enum {
    UDIS_MODULE_DEVMNG = 0,
    UDIS_MODULE_MEMORY,
    UDIS_MODULE_LP,
    UDIS_MODULE_SVM,
    UDIS_MODULE_NETWORK,
    UDIS_MODULE_TS,
    UDIS_MODULE_APM,
    UDIS_MODULE_MAX
} UDIS_MODULE_TYPE;

#define UDIS_MAX_NAME_LEN 16
#define UDIS_INFO_BLOCK_SIZE 256
#define UDIS_INFO_BLOCK_HEAD_LEN 48
#define UDIS_MAX_DATA_LEN (UDIS_INFO_BLOCK_SIZE - UDIS_INFO_BLOCK_HEAD_LEN)

struct udis_dev_info {
    unsigned int module_type;
    char name[UDIS_MAX_NAME_LEN];
    unsigned int acc_ctrl;
    unsigned long long last_update_time;
    unsigned int data_len;
    char data[UDIS_MAX_DATA_LEN];
    unsigned char reserved[12];
};

struct base_mem_info {
    unsigned long long total_size;
    unsigned long long free_size;
    unsigned long long resv[6];
};

struct udis_mem_info {
    struct base_mem_info sys_mem_info;
    struct base_mem_info service_mem_info;
    struct base_mem_info medium_mem_info;
};

struct udis_core_util_info {
    unsigned int core_num;
    unsigned char core[128];
};

__attribute__((visibility("default"))) int udis_get_device_info(unsigned int dev_id, struct udis_dev_info *info);
__attribute__((visibility("default"))) int udis_set_device_info(unsigned int dev_id, const struct udis_dev_info *info);
#endif