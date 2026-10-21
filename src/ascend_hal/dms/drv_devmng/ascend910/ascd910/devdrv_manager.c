/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ascend_hal.h"
#include "devmng_common.h"
#include "devmng_user.h"
#include "dms/dms_misc_interface.h"
#include "ascend_dev_num.h"

#define PLAT_GET_CHIP(type) ((type >> 8) & 0xff)

drvError_t halGetChipCapability(uint32_t devId, struct halCapabilityInfo *info)
{
    drvError_t drvRet;
    int64_t hardwareVersion = 0;

    if (devId >= ASCEND_DEV_MAX_NUM || info == NULL) {
        DEVDRV_DRV_ERR("invalid devId(%u) or info is NULL\n", devId);
        return DRV_ERROR_INVALID_DEVICE;
    }

    /* SDMA reduce Capability */
    info->sdma_reduce_support = CAP_BIT_CLEAR;
    info->sdma_reduce_support |= CAP_SDMA_REDUCE_FP32; // ascd910 only support FP32 reduce

    /* HBM Capability & L2Buffer Capability */
    info->memory_support = CAP_BIT_CLEAR;
    info->memory_support |= CAP_MEM_SUPPORT_HBM; // ascd910 support HBM

    info->ts_group_number = CAP_BIT_CLEAR;
    info->sdma_reduce_kind = CAP_BIT_CLEAR;
    info->sdma_reduce_kind |= CAP_SDMA_REDUCE_KIND_ADD;

    drvRet = halGetDeviceInfo(devId, MODULE_TYPE_SYSTEM, INFO_TYPE_VERSION, &hardwareVersion);
    if (drvRet != DRV_ERROR_NONE) {
        DEVDRV_DRV_ERR("Failed to get hardware version. (ret=%d)\n", drvRet);
        return drvRet;
    }

    if (PLAT_GET_CHIP((uint64_t)hardwareVersion) == CHIP_CLOUD_V2) {
        info->sdma_reduce_kind |= CAP_SDMA_REDUCE_KIND_MAX;
        info->sdma_reduce_kind |= CAP_SDMA_REDUCE_KIND_MIN;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_INT8;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_INT16;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_INT32;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_FP16;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_FP32;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_BFP16;
    } else if ((PLAT_GET_CHIP((uint64_t)hardwareVersion) == CHIP_CLOUD_V4) ||
               (PLAT_GET_CHIP((uint64_t)hardwareVersion) == CHIP_CLOUD_V5) ||
               (PLAT_GET_CHIP((uint64_t)hardwareVersion) == CHIP_MINI_V4)) {
        info->sdma_reduce_kind |= CAP_SDMA_REDUCE_KIND_MAX;
        info->sdma_reduce_kind |= CAP_SDMA_REDUCE_KIND_MIN;
        info->sdma_reduce_kind |= CAP_SDMA_REDUCE_KIND_EQUAL;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_INT8;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_INT16;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_INT32;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_FP16;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_FP32;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_BFP16;
        info->sdma_reduce_support |= CAP_SDMA_REDUCE_UINT32;
        info->memory_support |= CAP_MEM_SUPPORT_HiBAILUV100;
    }

    drvRet = memset_s(info->res, sizeof(info->res), 0, sizeof(info->res));
    if (drvRet != 0) {
        DEVDRV_DRV_ERR(" memset_s error (%d)\n", drvRet);
        return drvRet;
    }
    return 0;
}

drvError_t halGetCapabilityGroupInfo(int device_id, int ts_id, int group_id, struct capability_group_info *group_info,
                                     int group_count)
{
    (void)device_id;
    (void)ts_id;
    (void)group_id;
    (void)group_info;
    (void)group_count;
    return DRV_ERROR_NOT_SUPPORT;
}
