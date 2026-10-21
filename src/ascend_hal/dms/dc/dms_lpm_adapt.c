/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "securec.h"
#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "dms_user_common.h"
#include "dms/dms_lpm_interface.h"
#include "dms_device_info.h"
#include "dms/dms_devdrv_info_comm.h"
#include "ascend_hal_external.h"
#include "devmng_user_common.h"

int DmsGetInfoFromLpAdapter(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                            unsigned int *buf_size)
{
    int ret;
    int chip_type = 0;

    ret = DmsGetInfoFromLp(dev_id, vfid, sub_cmd, out_buf, buf_size);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get device info from LP failed. (type=0x%X; ret=%d).\n", chip_type, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

int DmsSetTemperature(unsigned int dev_id, unsigned int sub_cmd, void *in_buf, unsigned int buf_size)
{
    int ret;

    switch (sub_cmd) {
        case DSMI_TEMP_SUB_CMD_DDR_THOLD:
        case DSMI_TEMP_SUB_CMD_SOC_THOLD:
        case DSMI_TEMP_SUB_CMD_SOC_MIN_THOLD:
            ret = dms_set_temperature_threshold(dev_id, sub_cmd, in_buf, buf_size);
            if (ret != 0) {
                DMS_ERR("dms_set_temperature_threshold failed, devid(%u), ret(%d).\n", dev_id, ret);
                return ret;
            }
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

int DmsGetTemperatureAdapter(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                             unsigned int *buf_size)
{
    int ret;
    int chip_type = 0;

    ret = DmsGetTemperature(dev_id, vfid, sub_cmd, out_buf, buf_size);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get temperature from LP failed. (ret=%d, type=0x%X).\n", ret, chip_type);
        return ret;
    }
    return DRV_ERROR_NONE;
}