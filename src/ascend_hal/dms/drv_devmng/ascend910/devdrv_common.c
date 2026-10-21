/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "dmc_user_interface.h"
#include "dms_user_common.h"
#include "devmng_common.h"

int dmanage_run_proc(char **arg)
{
    return dms_run_proc((const char **)arg);
}

drvError_t halCtl(int cmd, void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret)
{
    uint32_t cmpt = 0;
    int ret;

    switch (cmd) {
        case HAL_CTL_REGISTER_LOG_OUT_HANDLE:
            cmpt = 0;
            ret = drv_log_out_handle_register((struct log_out_handle *)param_value, param_value_size, cmpt);
            break;
        case HAL_CTL_UNREGISTER_LOG_OUT_HANDLE:
            ret = drv_log_out_handle_unregister();
            break;
        case HAL_CTL_REGISTER_RUN_LOG_OUT_HANDLE:
            cmpt = 1;
            ret = drv_log_out_handle_register((struct log_out_handle *)param_value, param_value_size, cmpt);
            break;
        default:
            ret = DRV_ERROR_NOT_SUPPORT;
            break;
    }
    (void)(out_value);
    (void)(out_size_ret);
    return ret;
}
