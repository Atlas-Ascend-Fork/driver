/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DAVINCI_INTERFACE_H__
#define __DAVINCI_INTERFACE_H__

#include <unistd.h>

#define DAVINIC_MODULE_NAME_MAX 256

#define DAVINCI_INTF_DEV_NAME "davinci_manager"
#define DAVINCI_INTF_DEV_PATH "/dev/davinci_manager"
#define DAVINCI_INTF_NPU_DEV_CUST_PATH "/dev/npu_device_cust"
#define DAVINCI_INTF_DEV_PATH_NAME "/dev/" DAVINCI_INTF_DEV_NAME
#define DAVINCI_INTF_MODULE_TSDRV "TSDRV"
#define DAVINCI_INTF_MODULE_URD "URD"

/* Use 'Z' as magic number */
#define DAVINCI_INTF_IOC_MAGIC 'Z'
#define DAVINCI_INTF_IOCTL_OPEN _IO(DAVINCI_INTF_IOC_MAGIC, 0)
#define DAVINCI_INTF_IOCTL_CLOSE _IO(DAVINCI_INTF_IOC_MAGIC, 1)
#define DAVINCI_INTF_IOCTL_GET_MODULE_STATUS _IO(DAVINCI_INTF_IOC_MAGIC, 2)

#define DAVINCI_INTF_IOCTL_CMD_MAX_NR 3

struct davinci_intf_open_arg {
    char module_name[DAVINIC_MODULE_NAME_MAX];
    int device_id;
};

struct davinci_intf_close_arg {
    char module_name[DAVINIC_MODULE_NAME_MAX];
    int device_id;
};

struct davinci_intf_check_no_use_arg {
    char module_name[DAVINIC_MODULE_NAME_MAX];
    unsigned int status; /* 0--FALSE ,1 --TRUE means not used */
};

enum {
    DAVINCI_STATUS_TYPE_PROCESS,
    DAVINCI_STATUS_TYPE_DEVICE
};

/* device status */
#define DAVINCI_INTF_DEVICE_CLEAR_ALL_STATUS 0xffffffff
#define DAVINCI_INTF_DEVICE_CLEAR_STATUS (1U << 31)
#define DAVINCI_INTF_DEVICE_STATUS_TS_DOWN (1U << 0)
#define DAVINCI_INTF_DEVICE_STATUS_HEARTBIT_LOST (1U << 1)

#define DAVINCI_INTF_PROCESS_CLEAR_ALL_STATUS 0xffffffff
#define DAVINCI_INTF_PROCESS_CLEAR_STATUS (1U << 31)
#define DAVINCI_INTF_PROCESS_SVM_HUNG (1U << 0)
#define DAVINCI_INTF_PROCESS_HDC_CONNECT_CLOSE (1U << 1)

static inline const char *davinci_intf_get_dev_path(void)
{
#if (defined DRV_HOST) || (defined CFG_FEATURE_NO_NPU_DEV_CUST_PATH)
    return DAVINCI_INTF_DEV_PATH;
#else
    if (access(DAVINCI_INTF_DEV_PATH, F_OK | R_OK | W_OK) == 0) {
        return DAVINCI_INTF_DEV_PATH;
    } else {
        return DAVINCI_INTF_NPU_DEV_CUST_PATH;
    }
#endif
}

#endif
