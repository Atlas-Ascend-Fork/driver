/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/**
 * brief description about this document.
 * points to focus on.
 */

#ifndef _DMS_SENSORHUB_INFO_H
#define _DMS_SENSORHUB_INFO_H

#include "dsmi_common_interface.h"

int dms_get_sensor_hub_status(int device_id, struct dsmi_sensorhub_status_stru *sensorhub_status_data);

int dms_get_sensor_hub_config(int device_id, struct dsmi_sensorhub_config_stru *sensorhub_config_data);

#endif
