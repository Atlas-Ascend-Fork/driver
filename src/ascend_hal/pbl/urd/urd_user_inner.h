/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef URD_USER_INNER_H
#define URD_USER_INNER_H

#ifdef CFG_FEATURE_SYSLOG
#include <syslog.h>
#define urd_err(fmt, ...) syslog(LOG_ERR, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define urd_warn(fmt, ...) syslog(LOG_WARNING, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define urd_info(fmt, ...) syslog(LOG_INFO, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define urd_debug(fmt, ...) syslog(LOG_DEBUG, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else

#ifndef EMU_ST
#include "dmc_user_interface.h"
#else
#include "ascend_inpackage_hal.h"
#include "ut_log.h"
#endif
#define urd_err(fmt, ...) DRV_ERR(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define urd_warn(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define urd_info(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define urd_debug(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)

#endif
#define urd_ex_err(ret, fmt, ...)                                                                          \
    do {                                                                                                   \
        if (((ret) != DRV_ERROR_NOT_SUPPORT) && ((ret) != EOPNOTSUPP) && ((ret) != DRV_ERROR_TRY_AGAIN)) { \
            urd_err(fmt, ##__VA_ARGS__);                                                                   \
        }                                                                                                  \
    } while (0)

bool urd_ioctl_cmd_check(unsigned long ioctl_cmd, unsigned int main_cmd, unsigned int sub_cmd, const char *filter);

#endif
