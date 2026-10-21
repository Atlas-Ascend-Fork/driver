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
#include "dmc_user_interface.h"
#include "dms/dms_misc_interface.h"
#include "ascend_dev_num.h"

drvError_t dms_get_device_topology(unsigned int dev_id1, unsigned int dev_id2, int *topology_type)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_get_dev_topology_in in = {0};

    if (topology_type == NULL) {
        DMS_ERR("Input type is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    in.dev_id1 = dev_id1;
    in.dev_id2 = dev_id2;
    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_DEV_TOPOLOGY;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_get_dev_topology_in);
    ioarg.output = (void *)topology_type;
    ioarg.output_len = sizeof(int);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get device topology failed. (dev_id1=%u; dev_id2=%u; ret=%d)\n", dev_id1, dev_id2,
                              ret);
        return ret;
    }

    DMS_DEBUG("Get device topology success. (dev_id1=%u; dev_id2=%u; type=%d)\n", dev_id1, dev_id2, *topology_type);
    return DRV_ERROR_NONE;
}

drvError_t dms_get_phy_devices_topology(unsigned int dev_id1, unsigned int dev_id2, int *topology_type)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_get_dev_topology_in in = {0};

    if (topology_type == NULL) {
        DMS_ERR("Input type is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

#ifndef CFG_FEATURE_ERR_CODE_NOT_OPTIMIZATION
    if (dev_id1 >= ASCEND_DEV_MAX_NUM || dev_id2 >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("Invalid device ID. (dev_id1=%u; dev_id2=%u; max_dev_num=%d)\n", dev_id1, dev_id2, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_PARA_ERROR;
    }
#endif

    in.dev_id1 = dev_id1;
    in.dev_id2 = dev_id2;
    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_DEVICES_TOPOLOGY;
    ioarg.filter_len = 0;
    ioarg.input = &in;
    ioarg.input_len = sizeof(struct dms_get_dev_topology_in);
    ioarg.output = topology_type;
    ioarg.output_len = sizeof(int);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        if (ret == DRV_ERROR_NOT_SUPPORT) {
            hal_report_not_support("halGetPairPhyDevicesInfo", "This API cannot be called in the split scenario");
        }
        DMS_EX_NOTSUPPORT_ERR(ret,
                              "Get physical devices topology failed from kernel. (dev_id1=%u; dev_id2=%u; ret=%d)\n",
                              dev_id1, dev_id2, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
