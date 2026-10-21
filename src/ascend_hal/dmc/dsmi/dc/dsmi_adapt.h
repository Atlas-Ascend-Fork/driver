/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DSMI_ADAPT_H__
#define DSMI_ADAPT_H__

#include "dsmi_common_interface.h"
#define MAX_EVENT_COUNT_OF_GET_FAULT_EVENT (128)

int _dsmi_get_fault_inject_info(unsigned int device_id, unsigned int max_info_cnt, DSMI_FAULT_INJECT_INFO *info_buf,
                                unsigned int *real_info_cnt);
int _dsmi_check_partitions(const char *config_xml_path);
int _dsmi_get_device_utilization_rate(int device_id, int device_type, unsigned int *putilization_rate);
int _dsmi_get_ecc_enable(int device_id, DSMI_DEVICE_TYPE device_type, int *enable_flag);
int _dsmi_get_flash_content(int device_id, DSMI_FLASH_CONTENT content_info);
int _dsmi_set_flash_content(int device_id, DSMI_FLASH_CONTENT content_info);
int _dsmi_get_device_state(int device_id, DSMI_DEV_NODE_STATE *node_state, unsigned int max_num, unsigned int *num);
int _dsmi_set_bist_info(int device_id, DSMI_BIST_CMD cmd, const void *buf, unsigned int buf_size);
int _dsmi_get_bist_info(int device_id, DSMI_BIST_CMD cmd, void *buf, unsigned int *size);
int _dsmi_set_upgrade_attr(int device_id, DSMI_COMPONENT_TYPE component_type, DSMI_UPGRADE_ATTR attr);
int _dsmi_set_power_state(int device_id, DSMI_POWER_STATE type);
int _dsmi_get_ufs_status(int device_id, struct dsmi_ufs_status_stru *ufs_status_data);
int _dsmi_set_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                          unsigned int buf_size);
int _dsmi_get_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                          unsigned int *size);
int _dsmi_get_last_bootstate(int device_id, BOOT_TYPE boot_type, unsigned int *state);
int _dsmi_get_centre_notify_info(int device_id, int index, int *value);
int _dsmi_set_centre_notify_info(int device_id, int index, int value);
int _dsmi_ctrl_device_node(int device_id, struct dsmi_dtm_node_s dtm_node, DSMI_DTM_OPCODE opcode, IN_OUT_BUF buf);
int _dsmi_get_all_device_node(int device_id, DEV_DTM_CAP capability, struct dsmi_dtm_node_s node_info[],
                              unsigned int *size);
int _dsmi_fault_inject(DSMI_FAULT_INJECT_INFO fault_inject_info);
bool dsmi_get_device_info_is_critical(DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd);
int dsmi_cmd_get_flash_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                            unsigned int *size);
int dsmi_upgrade_cmd_send(int device_id, DSMI_COMPONENT_TYPE component_type, const char *file_name);

#endif