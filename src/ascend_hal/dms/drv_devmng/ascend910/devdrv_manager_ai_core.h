/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DEVDRV_MANAGER_CPU_H
#define DEVDRV_MANAGER_CPU_H

#include "ascend_hal.h"

drvError_t drv_get_ctrl_cpu_info(uint32_t devId, int32_t info_type, int64_t *value);
drvError_t drv_get_data_cpu_info(uint32_t devId, int32_t info_type, int64_t *value);
drvError_t drv_get_ai_core_level(uint32_t devId, int32_t info_type, int64_t *value);
drvError_t drvGetAiCoreInfo(uint32_t devId, int32_t info_type, int64_t *value);
drvError_t drv_get_vector_core_info(uint32_t devId, int32_t info_type, int64_t *value);
drvError_t drv_get_ts_cpu_info(uint32_t devId, int32_t info_type, int64_t *value);
drvError_t drv_get_device_computing_power(uint32_t devId, int32_t info_type, int64_t *value);
drvError_t drv_get_host_aicpu_info(uint32_t devId, int32_t info_type, int64_t *value);
drvError_t drv_get_hcom_cpu_info(uint32_t dev_id, int32_t info_type, int64_t *value);
drvError_t drv_get_ai_core_info_by_buff(uint32_t devId, int32_t info_type, void *buf, unsigned int *size);
drvError_t drv_get_ai_cpu_info(uint32_t devId, int32_t info_type, int64_t *value);

#endif /* DEVDRV_MANAGER_CPU_H */
