/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "securec.h"
#include "devmng_common.h"
#include "dms/dms_devdrv_info_comm.h"
#include "dms_device_info.h"
#include "dms_user_common.h"

#define SET_LP_FREQ_VOLT_MAX 512
#define SET_LP_FREQ_VOLT_PRE_LEN (2 * sizeof(unsigned int))

drvError_t drv_get_lp_info(uint32_t devId, int32_t info_type, void *buf, unsigned int *size)
{
    int ret;

    switch (info_type) {
        case INFO_TYPE_LP_AIC:
        case INFO_TYPE_LP_BUS:
            ret = DmsHalGetDeviceInfoEx(devId, MODULE_TYPE_LP, info_type, buf, size);
            break;

        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get low power info. (dev_id=%u, infoType=%d, ret=%d)\n", devId,
                                     info_type, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

STATIC drvError_t drv_set_lp_freq_volt(unsigned int dev_id, const void *buf, unsigned int size)
{
    unsigned char set_buf[SET_LP_FREQ_VOLT_MAX] = {0};
    int ret;

    ret = memcpy_s(set_buf + SET_LP_FREQ_VOLT_PRE_LEN, sizeof(set_buf) - SET_LP_FREQ_VOLT_PRE_LEN, buf, size);
    if (ret != 0) {
        DEVDRV_DRV_ERR("memcpy_s fail. (dev_id=%u;size=%u)\n", dev_id, size);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DmsHalSetDeviceInfoEx(dev_id, MODULE_TYPE_LP, INFO_TYPE_LP_FREQ_VOLT, set_buf,
                                 size + SET_LP_FREQ_VOLT_PRE_LEN);
}

drvError_t drv_set_lp_info(uint32_t devId, int32_t info_type, void *buf, unsigned int size)
{
    int ret;

    switch (info_type) {
        case INFO_TYPE_LP_FREQ_VOLT:
            ret = drv_set_lp_freq_volt(devId, buf, size);
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to set low power info. (dev_id=%u, infoType=%d, ret=%d)\n", devId,
                                     info_type, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}
