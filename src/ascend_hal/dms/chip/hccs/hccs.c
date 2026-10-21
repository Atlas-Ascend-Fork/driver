/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "dms/dms_misc_interface.h"
#include "dms_device_info.h"

int DmsGetHccsInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    (void)vfid;

    return DmsGetDeviceInfo(dev_id, DSMI_MAIN_CMD_HCCS, sub_cmd, buf, size);
}
