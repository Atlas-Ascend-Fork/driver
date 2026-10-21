/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_MON_OPS_H
#define DEV_MON_OPS_H

#include "dsmi_common_interface.h"
#include "dm_common.h"
#include "drvdmp_adapt.h"
#define DEV_MON_DOCKER_SCENES 1
#define DEV_MON_VIRTUAL_SCENES 2

int dev_mon_get_device_info(DM_RECV_ST *msg, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                            unsigned int *size);

int dev_mon_set_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                            unsigned int size, DMANAGE_OPERATE_DEVICE_INFO_FLAG type);

int dev_mon_set_detect_info(unsigned int device_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                            unsigned int size);

int dev_mon_get_detect_info(unsigned int device_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                            unsigned int *size);

#endif
