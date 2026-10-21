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
#include "dms_user_common.h"
#include "dmc_user_interface.h"
#include "securec.h"

int dms_fault_inject(unsigned int dev_id, unsigned int vfid, unsigned int main_cmd, unsigned int sub_cmd,
                     DMS_FAULT_INJECT_STRU *para)
{
    int ret;
    struct dms_fault_inject_in fault_inject_in;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_filter_st filter = {0};

    if (para == NULL) {
        DMS_ERR("Parameter is null. (dev_id=%u; vfid=%u)\n", dev_id, vfid);
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_DEVICE_INFO(&filter, main_cmd);
    fault_inject_in.dev_id = dev_id;
    fault_inject_in.vfid = vfid;
    fault_inject_in.buff = (void *)para;
    fault_inject_in.buff_size = sizeof(DMS_FAULT_INJECT_STRU);

    ioarg.main_cmd = DMS_FAULT_INJECT_CMD;
    ioarg.sub_cmd = sub_cmd;
    ioarg.filter = &filter.filter[0];
    ioarg.filter_len = filter.filter_len;
    ioarg.input = (void *)&fault_inject_in;
    ioarg.input_len = sizeof(struct dms_fault_inject_in);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms fault inject failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return errno_to_user_errno(ret);
    }

    DMS_DEBUG("Fault inject success.\n");
    return DRV_ERROR_NONE;
}
