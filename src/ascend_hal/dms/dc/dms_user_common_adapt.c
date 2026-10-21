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

#ifndef __linux
#pragma comment(lib, "libc_sec.lib")
#include "devdrv_manager_win.h"
#define PTHREAD_MUTEX_INITIALIZER NULL
#define FdIsValid(fd) fd != (mmProcess)DEVDRV_INVALID_FD_OR_INDEX
#else
#include <errno.h>
#include <sys/ioctl.h>

#define FdIsValid(fd) ((fd) >= 0)
#endif

#include "securec.h"
#include "mmpa_api.h"
#include "davinci_interface.h"
#include "dmc_user_interface.h"
#include "dms/dms_devdrv_info_comm.h"

int dms_ioctl_open(int fd)
{
    struct davinci_intf_close_arg arg = {0};
    int ret;

    if (!FdIsValid(fd)) {
        return DRV_ERROR_INVALID_VALUE;
    }

#ifndef CFG_FEATURE_SRIOV
    if (dmanage_check_module_init("asdrv_vdms") == 0) {
        dms_set_virt_flag(true);
        return 0;
    }
#endif

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_INTF_MODULE_URD);
    if (ret != 0) {
        DMS_ERR("strcpy_s failed, ret(%d).\n", ret);
        return ret;
    }

    ret = ioctl(fd, DAVINCI_INTF_IOCTL_OPEN, &arg);
    if (ret != 0) {
        DMS_ERR("call devdrv_device ioctl open failed, ret(%d) errno(%d).\n", ret, errno);
        return ret;
    }

    return 0;
}

bool dms_ioctl_cmd_check(unsigned long ioctl_cmd, unsigned int main_cmd, unsigned int sub_cmd, const char *filter)
{
    (void)ioctl_cmd;
    (void)main_cmd;
    (void)sub_cmd;
    (void)filter;
    return true;
}
