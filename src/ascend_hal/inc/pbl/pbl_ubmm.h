/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef PBL_UBMM_H
#define PBL_UBMM_H

#include <stdint.h>

#include "ascend_hal.h"

#define UBMM_INVALID_SERVER_ID 0x3FFU

drvError_t ubmemRepairFault(uint32_t devid, halRepairFaultInfo *info);

/*
 * map_route: define in ascend_hal_define.h MEM_MAP_UB_ONE_PORT_PATH...
 * if map route not exist, will return DRV_ERROR_NOT_EXIST.
 */
drvError_t ubmemMapRouteCheck(uint32_t dst_devid, uint32_t src_server_id, uint32_t src_mem_id, uint32_t map_route);
#endif
