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
#include "dms/dms_misc_interface.h"
#include "devdrv_user_common.h"
#include "dms_device_info.h"
#include "dms_cmd_def.h"
#include "ascend_kernel_hal.h"
#include "dms_user_common.h"
#include "dms/dms_qos_interface.h"

int DmsGetQosInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    (void)vfid;
    return DmsGetDeviceInfo(dev_id, DSMI_MAIN_CMD_QOS, sub_cmd, buf, size);
}

int DmsSetQosInfo(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size)
{
    return DmsSetDeviceInfo(dev_id, DSMI_MAIN_CMD_QOS, sub_cmd, buf, size);
}

int drv_get_qos_config(uint32_t devId, void *buf, unsigned int *size)
{
    int ret = 0;
    struct QosProfileInfo *qos_info = NULL;
    struct dms_ioctl_arg ioarg = {0};

    if (devId >= DEVDRV_PF_DEV_MAX_NUM || buf == NULL || size == NULL) {
        DMS_ERR("Invalid parameters. (dev_id=%u, buf%s, size%s)\n", devId, buf == NULL ? "=NULL" : "!=NULL",
                size == NULL ? "=NULL" : "!=NULL");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (*size != sizeof(struct QosProfileInfo)) {
        DMS_ERR("Invalid parameters. (dev_id=%u, *size=%u)\n", devId, *size);
        return DRV_ERROR_INVALID_VALUE;
    }

    qos_info = (struct QosProfileInfo *)buf;
    qos_info->dev_id = devId;

    ioarg.main_cmd = DMS_MAIN_CMD_QOS;
    ioarg.sub_cmd = DMS_SUBCMD_GET_CONFIG_INFO;
    ioarg.filter_len = 0;
    ioarg.input = qos_info;
    ioarg.input_len = sizeof(struct QosProfileInfo);
    ioarg.output = qos_info;
    ioarg.output_len = *size;
    ret = errno_to_user_errno(DmsIoctl(DMS_GET_QOS_CONFIG_INFO, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "drv_get_qos_config failed. (dev_id=%u, ret=%d)", devId, ret);
    }
    return ret;
}
