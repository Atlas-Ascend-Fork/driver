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

#include "dms_user_common.h"
#include "devmng_user_common.h"
#include "devdrv_user_common.h"
#include "devdrv_ioctl.h"
#include "dms/dms_devdrv_info_comm.h"
#include "dms/dms_misc_interface.h"
#include "ascend_dev_num.h"

int dms_power_hotreset_common(unsigned int dev_id, int ioctl_sub_cmd)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    ioarg.main_cmd = DMS_MAIN_CMD_HOTRESET;
    ioarg.sub_cmd = (unsigned int)ioctl_sub_cmd;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) && (dev_id != ALL_DEVICE_RESET_FLAG)) {
        DMS_ERR("Invalid devid. (devId=%u)\n", dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms hotreset common failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        if (ret == ENODEV) {
            ret = DRV_ERROR_CONNECT; /* H2D communication is abnormal */
        }
        return ret;
    }

    return DRV_ERROR_NONE;
}
