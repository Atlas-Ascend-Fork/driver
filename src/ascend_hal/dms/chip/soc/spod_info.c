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
#include "dms_device_info.h"
#include "ascend_hal.h"

drvError_t dms_get_spod_info(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                             unsigned int *size)
{
    int ret;

    if (size == NULL) {
        DMS_ERR("Size is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }
    if (buf == NULL) {
        DMS_ERR("Buffer is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }
    if (*size < sizeof(struct dsmi_spod_info)) {
        DMS_ERR("Wrong buffer length. (dev_id=%u, len=%u, min=%zu)\n", dev_id, *size, sizeof(struct dsmi_spod_info));
        return DRV_ERROR_PARA_ERROR;
    }
    ret = halDmsGetDeviceInfoEx(dev_id, main_cmd, sub_cmd, buf, size);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Failed to obtain the device information. (dev_id=%u; ret=%d)", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t dms_get_spod_item(uint32_t devId, int32_t infoType, int64_t *value)
{
    int ret;
    struct dsmi_spod_info info = {0};
    unsigned int size = sizeof(struct dsmi_spod_info);

    ret = halDmsGetDeviceInfoEx(devId, DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_SPOD_INFO, &info, &size);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Failed to obtain the device info. (dev_id=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    if (infoType == INFO_TYPE_SDID) {
        *value = info.sdid;
    } else if (infoType == INFO_TYPE_SERVER_ID) {
        *value = info.server_id;
    } else if (infoType == INFO_TYPE_SCALE_TYPE) {
        *value = info.scale_type;
    } else if (infoType == INFO_TYPE_SUPER_POD_ID) {
        *value = info.super_pod_id;
    } else if (infoType == INFO_TYPE_CHASSIS_ID) {
        *value = info.chassis_id;
    } else if (infoType == INFO_TYPE_SUPER_POD_TYPE) {
        *value = info.super_pod_type;
    } else if (infoType == INFO_TYPE_SUPER_POD_INTERCON_TYPE) {
        *value = info.super_pod_intercon_type;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

#ifdef CFG_FEATURE_SUPPORT_GET_SPOD_PING_INFO
drvError_t dms_get_spod_ping_info(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                                  unsigned int *size)
{
    int ret;

    if ((size == NULL) || (buf == NULL)) {
        DMS_ERR("Invalid parameter. (dev_id=%u; size_is_null=%d; buf_is_null=%d)\n", dev_id, size == NULL, buf == NULL);
        return DRV_ERROR_PARA_ERROR;
    }
    if (*size < sizeof(unsigned int)) {
        DMS_ERR("Invalid parameter. (dev_id=%u, size_invalid=%u; min=%zu)\n", dev_id, *size, sizeof(unsigned int));
        return DRV_ERROR_PARA_ERROR;
    }
    ret = halDmsGetDeviceInfoEx(dev_id, main_cmd, sub_cmd, buf, size);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Failed to obtain the device information. (dev_id=%u; ret=%d)", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
#endif

drvError_t dms_get_spod_node_status(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                                    unsigned int *size)
{
    int ret;
    struct dms_filter_st filter = {0};
    struct dms_get_device_info_in in = {0};
    struct dms_get_device_info_out out = {0};
    struct urd_ioctl_arg ioarg = {0};

    if ((buf == NULL) || (size == NULL)) {
        DMS_ERR("Invalid parameter. (dev_id=%u; buf_is_null=%d; size_is_null=%d;)\n", dev_id, buf == NULL,
                size == NULL);
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_DEVICE_INFO_EX(&filter, main_cmd, sub_cmd);
    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = *size;

    ioarg.cmd.main_cmd = DMS_GET_GET_DEVICE_INFO_CMD;
    ioarg.cmd.sub_cmd = ZERO_CMD;
    ioarg.cmd.filter = &filter.filter[0];
    ioarg.cmd.filter_len = filter.filter_len;
    ioarg.cmd_para.input = (void *)&in;
    ioarg.cmd_para.input_len = sizeof(struct dms_get_device_info_in);
    ioarg.cmd_para.output = (void *)&out;
    ioarg.cmd_para.output_len = sizeof(struct dms_get_device_info_out);

    ret = DmsIoctlConvertErrno(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret,
                              "get spod node info ioctl failed. (dev_id=%u; main_cmd=0x%x; sub_cmd=0x%x; ret=%d)\n",
                              dev_id, main_cmd, sub_cmd, ret);
        return ret;
    }

    *size = out.out_size;

    return DRV_ERROR_NONE;
}

drvError_t dms_set_spod_node_status(unsigned int dev_id, unsigned int sub_cmd, const void *buf, unsigned int size)
{
    int ret;
    struct dms_filter_st filter = {0};
    struct dms_set_device_info_in in = {0};
    struct urd_ioctl_arg ioarg = {0};

    DMS_MAKE_UP_FILTER_DEVICE_INFO_EX(&filter, DSMI_MAIN_CMD_CHIP_INF, sub_cmd);
    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = size;

    ioarg.cmd.main_cmd = DMS_GET_SET_DEVICE_INFO_CMD;
    ioarg.cmd.sub_cmd = ZERO_CMD;
    ioarg.cmd.filter = &filter.filter[0];
    ioarg.cmd.filter_len = filter.filter_len;
    ioarg.cmd_para.input = (void *)&in;
    ioarg.cmd_para.input_len = sizeof(struct dms_get_device_info_in);
    ioarg.cmd_para.output = NULL;
    ioarg.cmd_para.output_len = 0;

    ret = DmsIoctlConvertErrno(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Set spod node info ioctl failed. (dev_id=%u; sub_cmd=0x%x; ret=%d)\n", dev_id,
                              sub_cmd, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
