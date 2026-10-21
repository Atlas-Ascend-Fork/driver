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
#include "prof_adapt.h"

drvError_t prof_adapt_check_cc_mode(unsigned int dev_id)
{
    HAL_CC_INFO cc_info = {0};
    int size = sizeof(HAL_CC_INFO);
    drvError_t ret = halGetDeviceInfoByBuff(dev_id, MODULE_TYPE_CC, INFO_TYPE_CC, &cc_info, &size);
    if (ret != DRV_ERROR_NOT_SUPPORT) {
        if (ret != 0) {
            PROF_ERR("Failed to halGetDeviceInfoByBuff. (ret = %u)\n", ret);
            return PROF_ERROR;
        }
        if (cc_info.cc_cfg_info.cc_mode == HAL_CC_MODE_NORMAL) {
            PROF_INFO("CPU is currently in confidential computing mode, and prof drv is disabled.\n");
            return PROF_NOT_SUPPORT;
        }
    }
    return DRV_ERROR_NONE;
}

bool prof_is_v4_extra_channel(unsigned int chan_id)
{
    return (chan_id == CHANNEL_NPU_APP_MEM);
}

bool prof_is_support_host_sample(void)
{
    return true;
}
