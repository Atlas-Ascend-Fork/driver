/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef URD_USER_H
#define URD_USER_H

#include <stdint.h>
#include "pbl_urd_common.h"
#ifndef DMS_UT
#include "ascend_hal.h"
#endif
#ifndef UNUSED
#define UNUSED(x)  \
    do {           \
        (void)(x); \
    } while (0)
#endif

#ifdef __linux
#define DLLEXPORT __attribute__((visibility("default")))
#else
#define DLLEXPORT _declspec(dllexport)
#endif

int urd_dev_usr_cmd_ex(uint32_t devid, unsigned long ioctl_cmd, struct urd_cmd *cmd, struct urd_cmd_para *cmd_para);
DLLEXPORT int urd_dev_usr_cmd(uint32_t devid, struct urd_cmd *cmd, struct urd_cmd_para *cmd_para);
int urd_usr_cmd_ex(unsigned long ioctl_cmd, struct urd_cmd *cmd, struct urd_cmd_para *cmd_para);
int urd_usr_cmd(struct urd_cmd *cmd, struct urd_cmd_para *cmd_para);
static inline void urd_usr_cmd_fill(struct urd_cmd *cmd, uint32_t main_cmd, uint32_t sub_cmd, const char *filter,
                                    uint32_t filter_len)
{
    cmd->main_cmd = main_cmd;
    cmd->sub_cmd = sub_cmd;
    cmd->filter = filter;
    cmd->filter_len = filter_len;
}

static inline void urd_usr_cmd_para_fill(struct urd_cmd_para *cmd_para, void *input, uint32_t input_len, void *output,
                                         uint32_t output_len)
{
    cmd_para->input = input;
    cmd_para->input_len = input_len;
    cmd_para->output = output;
    cmd_para->output_len = output_len;
}
#ifndef DMS_UT
drvError_t urdCloseRestoreHandler(uint32_t devid, halDevCloseIn *in);
#endif
#endif
