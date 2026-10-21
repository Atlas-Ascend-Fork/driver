/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DMS_USER_COMMON_H
#define __DMS_USER_COMMON_H

#include "drv_type.h"
#include "ascend_hal_error.h"
#include "ascend_inpackage_hal.h"
#include "dmc_user_interface.h"
#include "dms_cmd_def.h"
#include "pbl_user_interface.h"
#include "urd_cmd.h"
#include "securec.h"
#ifndef ERESTARTSYS
#define ERESTARTSYS 512
#endif

#define MODULE_DMS "dms"
#define DMS_ERR(fmt, ...) DRV_ERR(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DMS_WARN(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DMS_INFO(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DMS_DEBUG(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DMS_EVENT(fmt, ...) DRV_RUN_INFO(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)

#if defined(CFG_FEATURE_DRV_EVENT_LOG)
#define FAULT_DMS_EVENT(fmt, ...) DRV_EVENT(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#endif

#define DMS_EX_NOTSUPPORT_ERR(ret, fmt, ...)                                       \
    do {                                                                           \
        if (((ret) != (int)DRV_ERROR_NOT_SUPPORT) && ((ret) != (int)EOPNOTSUPP)) { \
            DMS_ERR(fmt, ##__VA_ARGS__);                                           \
        }                                                                          \
    } while (0)

#define DMS_MAKE_UP_FILTER_DEVICE_INFO(f, main_cmd)                                                             \
    do {                                                                                                        \
        (f)->filter_len = (unsigned int)sprintf_s((f)->filter, sizeof((f)->filter), "main_cmd=0x%x", main_cmd); \
    } while (0)

#define DMS_MAKE_UP_FILTER_DEVICE_INFO_EX(f, main_cmd, sub_cmd)                                                   \
    do {                                                                                                          \
        (f)->filter_len = (unsigned int)sprintf_s((f)->filter, sizeof((f)->filter), "main_cmd=0x%x,sub_cmd=0x%x", \
                                                  main_cmd, sub_cmd);                                             \
    } while (0)

#define PROC_MOUDULE_FILE_NAME "/proc/modules"
#define DEV_MODULE_INIT_INFO_LEN 1024
#define MAX_FOPEN_RETRY_TIMES 5

int DmsIoctl(int cmd, struct dms_ioctl_arg *ioarg);
drvError_t DmsIoctlConvertErrno(unsigned long cmd, struct urd_ioctl_arg *ioarg);
int DmsGetVirtFlag(void);
int dms_set_virt_flag(int env_virt);
int dms_run_proc(const char **arg);
int drvGetKlogBuf(uint32_t devId, const char *path, unsigned int *pSize);
int dms_ioctl_open(int fd);
drvError_t ioctl_errno_convert(int ret, int errno_param);
bool dms_ioctl_cmd_check(unsigned long ioctl_cmd, unsigned int main_cmd, unsigned int sub_cmd,
                         const char *filter); /* internal use only */

#define DMS_VIRT_ADAPT_FUNC(virt_func, phy_func) ((DmsGetVirtFlag() == true) ? (virt_func) : (phy_func))

drvError_t dmsCloseRestoreHandler(uint32_t devid, halDevCloseIn *in);
drvError_t dmsProcResRestoreHandler(halProcResRestoreInfo *info);
drvError_t dms_res_enable_p2p(u32 dev_num);
drvError_t udaCloseRestoreHandler(uint32_t devid, halDevCloseIn *in);
void hal_report_not_support(const char *func_name, const char *reason);
void hal_report_invalid_dev_id(const char *func_name, uint32_t devId, uint32_t max_devid);
void hal_report_device_occupied(uint32_t devId);
#endif
