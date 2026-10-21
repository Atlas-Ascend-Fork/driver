/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/ioctl.h>

#include "ascend_hal.h"

#include "svm_pub.h"
#include "svm_log.h"
#include "svm_ub_register_to_master.h"
#include "svm_init_pri.h"
#include "svm_sys_cmd.h"
#include "svm_dbi.h"

static int svm_ub_share_register(u32 devid)
{
    u32 hd_connect_type;

    hd_connect_type = svm_get_device_connect_type(devid);
    if (hd_connect_type == HOST_DEVICE_CONNECT_TYPE_UB) {
        svm_ub_register_to_master_ops_register(svm_get_host_devid(), devid);
        svm_ub_register_to_master_ops_register(devid, svm_get_host_devid());
    }

    svm_ub_register_to_master_ops_register(devid, devid);

    return DRV_ERROR_NONE;
}

static int svm_ub_share_unregister(u32 devid)
{
    u32 hd_connect_type;

    hd_connect_type = svm_get_device_connect_type(devid);
    if (hd_connect_type == HOST_DEVICE_CONNECT_TYPE_UB) {
        svm_ub_register_to_master_ops_unregister(svm_get_host_devid(), devid);
    }

    svm_ub_register_to_master_ops_unregister(devid, devid);

    return DRV_ERROR_NONE;
}

static void __attribute__((constructor(SVM_INIT_PRI_FISRT))) svm_ub_share_init(void)
{
    int ret;

    ret = svm_register_ioctl_dev_init_post_handle(svm_ub_share_register);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev init post handle failed.\n");
    }

    ret = svm_register_ioctl_dev_uninit_pre_handle(svm_ub_share_unregister);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev uninit pre handle failed.\n");
    }
}
