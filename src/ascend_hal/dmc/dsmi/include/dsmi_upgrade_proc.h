/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef __DSMI_UPGRADE_PROC_H__
#define __DSMI_UPGRADE_PROC_H__
#include "dsmi_common_interface.h"

#define UPGRADE_FIRMWARE_CFG_FILE_PATH "/firmware/tools/conf/upgrade.cfg"
#define UPGRADE_FIRMWARE_PATH_PREFIX "/firmware/tools/"

#define DEVICE_RETURN_UPGRADE_STATUS_IS_SYNCHRONIZING (DRV_ERROR_INNER_ERR - 20)

int upgrade_all_component(int device_id, const char *file_name, DSMI_COMPONENT_TYPE *component_list,
                          unsigned int component_num, DSMI_COMPONENT_TYPE component_type);

int upgrade_single_component(int device_id, const char *file_name, DSMI_COMPONENT_TYPE *component_list,
                             unsigned int component_num, DSMI_COMPONENT_TYPE component_type);
DLLEXPORT int transmit_file_to_device(int device_id, const char *src_file, char *dst_file,
                                      DSMI_COMPONENT_TYPE component_type);
int upgrade_trans_patch(int device_id, const char *file_name);
int upgrade_trans_mami_patch(int device_id, const char *file_name);

#endif
