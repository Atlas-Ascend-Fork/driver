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
#include "dms_user_common.h"
#include "dms/dms_misc_interface.h"
#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "dms/dms_devdrv_info_comm.h"
#include "dsmi_common_interface.h"
#include "dms_cmd_def.h"
#include "dmc_user_interface.h"
#include "ascend_dev_num.h"

drvError_t DmsGetAllDeviceList(int device_ids[], int count)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    struct urd_probe_dev_info probe_devinfo = {0};

    if ((device_ids == NULL) || (count > ASCEND_DEV_MAX_NUM) || (count <= 0)) {
        DMS_ERR("Input param error. (device_ids_is_NULL=%d; count=%d)\n", (int)(device_ids == NULL), count);
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_ALL_DEV_LIST;
    ioarg.filter_len = 0;
    ioarg.input = NULL;
    ioarg.input_len = 0;
    ioarg.output = (void *)&probe_devinfo;
    ioarg.output_len = sizeof(struct urd_probe_dev_info);

    ret = errno_to_user_errno(DmsIoctl(DMS_GET_ALL_DEVICE_LIST, &ioarg));
    if (ret == DRV_ERROR_NO_DEVICE) {
        DMS_WARN("IOCTL no dev, please retry. (ret=%d)\n", ret);
        return DRV_ERROR_NO_DEVICE;
    } else if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "IOCTL failed. (ret=%d)\n", ret);
        return (drvError_t)ret;
    }
    if (probe_devinfo.num_dev > (unsigned int)count) {
        DMS_ERR("Input param count is invalid. (real=%d; count=%d)\n", probe_devinfo.num_dev, count);
        return DRV_ERROR_INNER_ERR;
    }
    ret = memcpy_s((void *)device_ids, sizeof(u32) * probe_devinfo.num_dev, probe_devinfo.devids,
                   sizeof(u32) * probe_devinfo.num_dev);
    if (ret != 0) {
        DMS_ERR("copy to user failed. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}
