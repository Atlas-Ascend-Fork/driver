/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SRC_DRIVER_NETWORK_HCCN_HCCN_TOOL_SRC_TOOL_LIB_H
#define SRC_DRIVER_NETWORK_HCCN_HCCN_TOOL_SRC_TOOL_LIB_H

#include <unistd.h>
#include <string.h>
#include "ossl_user_linux.h"

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#ifndef CONFIG_LLT
#define STATIC static
#else
#define STATIC
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif
#define DECIMAL 10
#define HEXADECIMAL 16
#define COMMAND_MAX_OPTIONS 64
#define IS_MATCH 0
#define NOT_MATCH 1

struct tag_cmd_option_t;
struct tag_major_cmd_t;

typedef int (*command_record_t)(struct tag_major_cmd_t *major, const char *param);

typedef struct tag_cmd_option_t {
    char *little;
    char *large;
    unsigned int have_param;
    command_record_t record;
} cmd_option_t;

typedef struct tag_major_cmd_t {
    char *name;
    int option_count;
    cmd_option_t options[COMMAND_MAX_OPTIONS];
    int options_repeat_flag[COMMAND_MAX_OPTIONS];
} major_cmd_t;

struct dfx_u32_parse {
    char *dfx_name;
    unsigned int minv;
    unsigned int maxv;
};

int major_command_option(major_cmd_t *cmd, char *little, char *large, int have_param, command_record_t record);
int command_parse(major_cmd_t *cmd, int argc, char **argv);
int str_to_off_t(const char *str, off_t *data);
int dfx_parse_uinteger_param(int argc, char **argv, int *argc_index, unsigned int *value,
                             const struct dfx_u32_parse *dfx);
#endif
