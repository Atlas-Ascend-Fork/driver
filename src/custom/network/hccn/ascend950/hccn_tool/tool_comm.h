/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TOOL_COMM_H
#define TOOL_COMM_H

#include <stdio.h>
#include "ds_data.h"
#include "hccn_log.h"

#define OUTBUF_LEN_16 16
#define OUTBUF_LEN_64 64
#define OUTBUF_LEN_256 256
#define OUTBUF_LEN_1024 1024
#define OUTBUF_LEN_2048 2048
#define OUTBUF_LEN_4096 4096
#define OUTBUF_LEN_5120 5120
#define OUTBUF_LEN_32K 32768

#define TOOL_CHECK_PTR_VALID_RETURN_VAL(p, ret) \
    do {                                        \
        if ((p) == NULL) {                      \
            hccn_err("ptr is NULL!");           \
            return ret;                         \
        }                                       \
    } while (0)

#ifdef CONFIG_LLT
#define TOOL_PRINT_INFO(fmt, args...)      \
    do {                                   \
        fprintf(stdout, fmt "\n", ##args); \
    } while (0)

#define TOOL_PRINT_WARN(fmt, args...)      \
    do {                                   \
        fprintf(stdout, fmt "\n", ##args); \
    } while (0)

#define TOOL_PRINT_ERR(fmt, args...)       \
    do {                                   \
        fprintf(stderr, fmt "\n", ##args); \
    } while (0)

#else
#define TOOL_PRINT_INFO(fmt, args...)               \
    do {                                            \
        fprintf(stdout, fmt "\n", ##args);          \
        NET_INFO(HAL_MODULE_TYPE_NET, fmt, ##args); \
    } while (0)

#define TOOL_PRINT_WARN(fmt, args...)               \
    do {                                            \
        fprintf(stdout, fmt "\n", ##args);          \
        NET_WARN(HAL_MODULE_TYPE_NET, fmt, ##args); \
    } while (0)

#define TOOL_PRINT_ERR(fmt, args...)               \
    do {                                           \
        fprintf(stderr, fmt "\n", ##args);         \
        NET_ERR(HAL_MODULE_TYPE_NET, fmt, ##args); \
    } while (0)
#endif

#define PRINT_SPACE_NUM 25U
#define TOOL_PRINT_CMD(mode, cmd, desc) TOOL_PRINT_INFO("hccn_tool %s %-*s\t: %s", mode, PRINT_SPACE_NUM, cmd, desc)
#define TOOL_PRINT_OPT(opt, desc) TOOL_PRINT_INFO("%13s%-*s\t: %s", "", PRINT_SPACE_NUM, opt, desc)

int hccn_check_usr_identify(void);
char *hccn_get_g_usr_name(void);
int hccn_get_usr_name(struct command_context *ctx);
int hccn_get_usr_ip(struct command_context *ctx);

#endif // TOOL_COMM_H
