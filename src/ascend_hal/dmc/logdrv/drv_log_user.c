/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdio.h>
#include "dmc/dmc_log_user.h"
#include "drv_log_user_kernel_api.h"

#ifdef DRV_HOST
static void __attribute__((constructor)) drv_log_init(void)
{
    drvError_t ret;
    uint32_t drv_log_rsyslog_console_level_tmp = LOG_ERR;
    ret = drvMngGetConsoleLogLevel(&drv_log_rsyslog_console_level_tmp);
    if (ret != DRV_ERROR_NONE) {
        (void)printf("Failed to obtain the console log level. The possible causes are as follows:\n"
                     " 1. Different containers share the same device;\n"
                     " 2. Failed to open the character device when interacting with the OS kernel;\n"
                     " 3. Failed to obtain the number of devices.\n");
    }
    drv_log_rsyslog_console_level_set(drv_log_rsyslog_console_level_tmp);
}
#endif