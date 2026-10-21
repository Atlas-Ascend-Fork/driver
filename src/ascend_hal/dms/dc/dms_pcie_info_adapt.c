/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "dms_user_common.h"
#include "dms/dms_misc_interface.h"
#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devmng_common.h"
#include "dsmi_common_interface.h"
#include "dms_device_info.h"
#include "dms_pcie.h"

int dms_get_pcie_link_info(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                           unsigned int *size)
{
    (void)dev_id;
    (void)vfid;
    (void)sub_cmd;
    (void)out_buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
}

int DmsGetPcieInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    (void)vfid;
    if (sub_cmd >= DSMI_PCIE_SUB_CMD_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return halDmsGetDeviceInfoEx(dev_id, DSMI_MAIN_CMD_PCIE, sub_cmd, buf, size);
}