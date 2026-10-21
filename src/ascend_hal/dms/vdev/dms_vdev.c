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
#include "dms_user_common.h"
#include "dms/dms_misc_interface.h"
#include "dms_vdev.h"

int dms_get_vdevice_info(unsigned int dev_id, unsigned int vf_id, unsigned int *total_core, unsigned int *core_count,
                         unsigned long *mem_size)
{
    struct dms_ioctl_arg ioarg = {0};
    struct dms_get_vdevice_info_in in = {0};
    struct dms_get_vdevice_info_out out = {0};
    int ret;
    if ((total_core == NULL) || (core_count == NULL) || (mem_size == NULL)) {
        DMS_ERR("Invalid parameter\n");
        return DRV_ERROR_PARA_ERROR;
    }

    in.dev_id = dev_id;
    in.vfid = vf_id;
    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_GET_VDEVICE_INFO;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_get_vdevice_info_in);
    ioarg.output = (void *)&out;
    ioarg.output_len = sizeof(struct dms_get_vdevice_info_out);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "IOCTL failed. (ret=%d)\n", ret);
        return ret;
    }
    *total_core = out.total_core;
    *core_count = out.core_num;
    *mem_size = out.mem_size;
    return DRV_ERROR_NONE;
}

#ifdef CFG_FEATURE_SRIOV
drvError_t dms_set_sriov_switch(unsigned int dev_id, unsigned int sub_cmd, const void *buf, unsigned int buf_size)
{
    (void)sub_cmd;
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_sriov_switch_in in = {0};

    if (buf == NULL || buf_size < sizeof(int)) {
        DMS_ERR("Invalid parameter. (buf_size=%u)\n", buf_size);
        return DRV_ERROR_PARA_ERROR;
    }

    in.dev_id = dev_id;
    in.sriov_switch = *(const int *)buf;
    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_SRIOV_SWITCH;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_sriov_switch_in);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret == EOPNOTSUPP) {
        return DRV_ERROR_NOT_SUPPORT;
    } else if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "IOCTL failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
#endif
