/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DRV_COMMON_INTERFACE_H
#define DRV_COMMON_INTERFACE_H

#include <linux/ioctl.h>
#include "ascend_hal.h"
#include "ascend_hal_error.h"
#include "ascend_inpackage_hal.h"
#include "dmc_user_interface.h"
#include "esched_user_interface.h"
#include "queue_user_interface.h"
#include "svm_user_interface.h"
#include "tsdrv_user_interface.h"
#include "apm_user_interface.h"
#include "pbl/pbl_urd_user.h"
#include "dms_user_common.h"
#include "soc_resmng_ioctl.h"

#define DRV_COMM_ERR(fmt, ...) DRV_ERR(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DRV_COMM_WARN(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DRV_COMM_INFO(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DRV_COMM_DEBUG(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DRV_COMM_EVENT(fmt, ...) DRV_RUN_INFO(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)

#define DRV_COMM_EX_NOTSUPPORT_ERR(ret, fmt, ...)                                  \
    do {                                                                           \
        if (((ret) != (int)DRV_ERROR_NOT_SUPPORT) && ((ret) != (int)EOPNOTSUPP)) { \
            DRV_COMM_ERR(fmt, ##__VA_ARGS__);                                      \
        }                                                                          \
    } while (0)

#define DRV_COMM_KERNEL_ERR(err)                      \
    ((DVresult)(((err) == 0) ? DRV_ERROR_IOCRL_FAIL : \
                               (((err) != ESRCH) ? errno_to_user_errno(err) : DRV_ERROR_PROCESS_EXIT)))

typedef enum {
    ESCHED_DEV_OPERATION,
    MEM_DEV_OPERATION,
    TSDRV_DEV_OPERATION,
    BUFF_DEV_OPERATION,
    QUEUE_DEV_OPERATION,
    URD_DEV_OPERATION,
    DMS_DEV_OPERATION,
    APM_DEV_OPERATION,
    UDA_DEV_OPERATION,
    MAX_DEV_OPERATION,
} DRV_DEV_OPERATION;

// clang-format off
static drvError_t (*drv_open_handlers[MAX_DEV_OPERATION])(uint32_t devid, halDevOpenIn *in, halDevOpenOut *out) = {
    [ESCHED_DEV_OPERATION] = esched_device_open,
    [MEM_DEV_OPERATION] = drvMemDeviceOpenInner,
    [TSDRV_DEV_OPERATION] = drvDeviceOpenInner,
    [BUFF_DEV_OPERATION] = NULL,
    [QUEUE_DEV_OPERATION] = queue_device_open,
};

static drvError_t (*drv_close_handlers[MAX_DEV_OPERATION])(uint32_t devid, halDevCloseIn *in) = {
    [ESCHED_DEV_OPERATION] = esched_device_close,
    [MEM_DEV_OPERATION] = drvMemDeviceCloseInner,
    [TSDRV_DEV_OPERATION] = drvDeviceCloseInner,
    [BUFF_DEV_OPERATION] = NULL,
    [QUEUE_DEV_OPERATION] = queue_device_close,
};

static drvError_t (*drv_close_host_user_handlers[MAX_DEV_OPERATION])(uint32_t devid, halDevCloseIn *in) = {
    [ESCHED_DEV_OPERATION] = esched_device_close_user,
    [MEM_DEV_OPERATION] = drvMemDeviceCloseUserRes,
    [TSDRV_DEV_OPERATION] = drvTrsDeviceCloseUserResInner,
    [BUFF_DEV_OPERATION] = NULL,
    [QUEUE_DEV_OPERATION] = queue_device_close_user,
    [URD_DEV_OPERATION] = urdCloseRestoreHandler,
    [DMS_DEV_OPERATION] = dmsCloseRestoreHandler,
    [APM_DEV_OPERATION] = apm_device_close_user,
    [UDA_DEV_OPERATION] = udaCloseRestoreHandler,
};

static drvError_t (*drv_proc_res_backup_handlers[MAX_DEV_OPERATION])(halProcResBackupInfo *info) = {
    [ESCHED_DEV_OPERATION] = NULL,
    [MEM_DEV_OPERATION] = drvMemProcResBackup,
    [TSDRV_DEV_OPERATION] = trsMemProcResBackup,
    [BUFF_DEV_OPERATION] = NULL,
    [QUEUE_DEV_OPERATION] = NULL,
};

static drvError_t (*drv_proc_res_restore_handlers[MAX_DEV_OPERATION])(halProcResRestoreInfo *info) = {
    [ESCHED_DEV_OPERATION] = NULL,
    [MEM_DEV_OPERATION] = drvMemProcResRestore,
    [TSDRV_DEV_OPERATION] = NULL,
    [BUFF_DEV_OPERATION] = NULL,
    [QUEUE_DEV_OPERATION] = NULL,
    [URD_DEV_OPERATION] = NULL,
    [DMS_DEV_OPERATION] = dmsProcResRestoreHandler,
};
// clang-format on

u32 soc_res_get_ver(u32 udevid, enum soc_ver_type type, u32 *ver);

#endif
