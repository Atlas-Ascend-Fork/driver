/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DSMI_PRODUCT_H__
#define __DSMI_PRODUCT_H__
#include "dsmi_common.h"
#include "dev_mon_iam_type.h"

#define DSMI_PRODUCT_MAIN_CMD_START 0x8000U
#define DSMI_PRODUCT_MAIN_CMD_END 0x8FFFU

int dsmi_product_set_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                                 unsigned int buf_size);
int dsmi_product_get_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                                 unsigned int *size);
int dsmi_cmd_get_i2c_heartbeat_status(int device_id, unsigned char *status, unsigned int *disconn_cnt);

int dsmi_cmd_passthru_mcu(int device_id, unsigned char pass_para, struct passthru_message_stru *passthru_message);

bool is_product_user_config_item_by_name(const char *config_name, unsigned char config_name_len);
int dsmi_product_set_user_config(int device_id, const char *config_name, unsigned int buf_size, unsigned char *buf);
int dsmi_product_get_user_config(int device_id, const char *config_name, unsigned int buf_size, unsigned char *buf);
int dsmi_product_clear_user_config(int device_id, const char *config_name);
#endif /* __DSMI_PRODUCT_H__ */
