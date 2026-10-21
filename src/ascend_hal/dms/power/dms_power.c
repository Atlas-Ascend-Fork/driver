/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/ioctl.h>
#include "securec.h"
#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "dms/dms_lpm_interface.h"
#include "dms_user_common.h"
#include "dsmi_common_interface.h"
#include "dms_cmd_def.h"

struct drvdev_power_state_info {
    unsigned int dev_id;
    struct dsmi_power_state_info_stru power_info;
};

drvError_t DmsSetPowerState(unsigned int dev_id, struct dsmi_power_state_info_stru *power_info)
{
    struct drvdev_power_state_info info = {};
    struct dms_ioctl_arg ioarg = {};
    int ret = 0;

    info.dev_id = dev_id;
    ret = memcpy_s(&(info.power_info), sizeof(struct dsmi_power_state_info_stru), power_info,
                   sizeof(struct dsmi_power_state_info_stru));
    if (ret != 0) {
        DMS_ERR("memcpy_s failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_GET_SET_POWER_STATE_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&info;
    ioarg.input_len = sizeof(struct drvdev_power_state_info);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = errno_to_user_errno(DmsIoctl(URD_CONFIG_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Set power state failed. (devid=%u; type=%u; mode=%u; value=%u; ret=%d)\n", dev_id,
                              power_info->type, power_info->mode, power_info->value, ret);
        return ret;
    }
    DMS_DEBUG("Set power state success. (devid=%u; type=%u; mode=%u; value=%u; ret=%d)\n", dev_id, power_info->type,
              power_info->mode, power_info->value, ret);
    return DRV_ERROR_NONE;
}
