/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ascend_dev_num.h"
#include "drv_comm_intf.h"
#include "drv_repair_fault.h"
#include "pbl/pbl_ubmm.h"

static drvRepairFaultHandler g_drv_repair_fault_handlers[HAL_REPAIR_FAULT_TYPE_MAX] = {
    [HAL_REPAIR_FAULT_TYPE_UBMEM] = ubmemRepairFault,
};

drvError_t halRepairFault(uint32_t devid, halRepairFaultInfo *info)
{
    drvRepairFaultHandler handler = NULL;

    if (info == NULL) {
        DRV_COMM_ERR("Info is null. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (devid >= ASCEND_DEV_MAX_NUM) {
        DRV_COMM_ERR("Devid is invalid. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((uint32_t)info->fault_type >= (uint32_t)HAL_REPAIR_FAULT_TYPE_MAX) {
        DRV_COMM_EVENT("Fault type is not supported. (devid=%u; type=%d)\n", devid, info->fault_type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    handler = g_drv_repair_fault_handlers[info->fault_type];
    if (handler == NULL) {
        DRV_COMM_EVENT("Fault type is not supported. (devid=%u; type=%d)\n", devid, info->fault_type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return handler(devid, info);
}