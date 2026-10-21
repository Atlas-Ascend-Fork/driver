/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifdef CFG_FEATURE_CAN_INFO
#include <sys/ioctl.h>
#include "securec.h"
#include "dmc_user_interface.h"
#include "dms_device_info.h"
#endif
#include "dms_user_common.h"
#include "dms_can.h"

#ifdef CFG_FEATURE_CAN_INFO
int DmsGetCanStatus(u32 dev_id, const char *name, u32 name_len, struct dsmi_can_status_stru *can_status_data)
{
    int ret;
    struct can_status_stru can_status = {0};
    struct dms_ioctl_arg ioarg = {0};

    if ((dev_id != 0) || (name == NULL) || (can_status_data == NULL)) {
        DMS_ERR("Invalid dev_id or name is NULL. (dev_id=%u; name_is_null=%d; data_is_null=%d)\n", dev_id,
                (name == NULL), (can_status_data == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_CAN;
    ioarg.sub_cmd = DMS_SUBCMD_GET_CAN_STATUS;
    ioarg.filter_len = 0;
    ioarg.input = (void *)name;
    ioarg.input_len = name_len;
    ioarg.output = (void *)&can_status;
    ioarg.output_len = sizeof(struct can_status_stru);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_ERR("DmsIoctl failed. (ret=%d; can_name=%.*s)\n", ret, (int)name_len, name);
        return errno_to_user_errno(ret);
    }

    ret = memcpy_s(can_status_data, sizeof(struct dsmi_can_status_stru), &can_status, sizeof(struct can_status_stru));
    if (ret != 0) {
        DMS_ERR("Memcpy_s failed. (ret=%d).\n", ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    return DRV_ERROR_NONE;
}

int DmsSetCanCfg(u32 dev_id, u32 sub_cmd, void *buf, u32 size)
{
    return DmsSetDeviceInfo(dev_id, DMS_MAIN_CMD_CAN, sub_cmd, buf, size);
}

int DmsGetCanCfg(u32 dev_id, u32 vfid, u32 sub_cmd, void *buf, u32 *size)
{
    return DmsGetDeviceInfo(dev_id, DMS_MAIN_CMD_CAN, sub_cmd, buf, size);
}
#else
int DmsGetCanStatus(u32 dev_id, const char *name, u32 name_len, struct dsmi_can_status_stru *can_status_data)
{
    (void)dev_id;
    (void)name;
    (void)name_len;
    (void)can_status_data;

    return DRV_ERROR_NOT_SUPPORT;
}

int DmsSetCanCfg(u32 dev_id, u32 sub_cmd, void *buf, u32 size)
{
    (void)dev_id;
    (void)sub_cmd;
    (void)buf;
    (void)size;

    return DRV_ERROR_NOT_SUPPORT;
}

int DmsGetCanCfg(u32 dev_id, u32 vfid, u32 sub_cmd, void *buf, u32 *size)
{
    (void)dev_id;
    (void)vfid;
    (void)sub_cmd;
    (void)buf;
    (void)size;

    return DRV_ERROR_NOT_SUPPORT;
}
#endif