/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tool_lib.h"

STATIC char str_to_off_conversion(const char *pe, u32 radix)
{
    char c = 0;

    if ((*pe >= '0') && (*pe <= '9')) {
        c = *pe - '0';
    } else if ((*pe >= 'a') && (*pe <= 'f')) {
        if (radix == DECIMAL) {
            roce_err("Data format error.");
            return -1;
        }
        c = (*pe - 'a') + DECIMAL;
    } else if ((*pe >= 'A') && (*pe <= 'F')) {
        if (radix == DECIMAL) {
            roce_err("Data format error.");
            return -1;
        }
        c = (*pe - 'A') + DECIMAL;
    } else {
        roce_err("Data format error.");
        return -1;
    }
    return c;
}

int str_to_off_t(const char *str, off_t *data)
{
    const char *pe = NULL;
    const char *ps = NULL;
    off_t tmp = 0;
    off_t c_val;
    u32 c_num;
    u32 radix;
    char c;
    u32 i;
    u32 j;

    if (str == NULL) {
        roce_err("str is NULL");
        return -1;
    }

    if ((*str == '0') && ((*(str + 1) == 'x') || (*(str + 1) == 'X'))) {
        radix = HEXADECIMAL;
        ps = str + 0x2;
        c_num = strlen(ps);
        pe = ps + c_num - 1;
    } else {
        radix = DECIMAL;
        ps = str;
        c_num = strlen(ps);
        pe = ps + c_num - 1;
    }

    for (i = 0; i < c_num; i++) {
        c = str_to_off_conversion(pe, radix);
        if (c < 0) {
            return c;
        }

        c_val = c;
        for (j = 0; j < i; j++) {
            c_val *= (long)radix;
        }

        tmp += c_val;
        pe--;
        if (tmp < 0) {
            roce_err("Data has overflown.");
            return -1;
        }
    }

    *data = tmp;
    return 0;
}

STATIC int check_option_param(cmd_option_t *option, char *arg)
{
    if (option == NULL || (option->have_param && (!arg))) {
        return UDA_PARAM_INVALID_ERR;
    }

    return UDA_EXE_SUCCESS;
}

STATIC int cmp_option(major_cmd_t *cmd, char **argv, int *input, int option_count)
{
    cmd_option_t *option = NULL;
    char *arg = NULL;
    int ret;

    arg = argv[*input];
    option = &cmd->options[option_count];
    if (((option->little != NULL) && (strncmp(arg, option->little, strlen(option->little) + 1) == 0)) ||
        ((option->large != NULL) && (strncmp(arg, option->large, strlen(option->large) + 1) == 0))) {
        if (cmd->options_repeat_flag[option_count] != 0) {
            roce_err("repeat input %s.", option->little);
            return UDA_PARAM_INVALID_ERR;
        }
        cmd->options_repeat_flag[option_count] = 1;
        ret = check_option_param(option, argv[*input + 1]);
        if (ret) {
            roce_err("check_option_param error");
            return ret;
        } else {
            (*input)++;
            arg = argv[*input];
        }

        ret = option->record(cmd, arg);
        if (ret != UDA_EXE_SUCCESS) {
            return ret;
        }
        return IS_MATCH;
    }

    return NOT_MATCH;
}

int command_parse(major_cmd_t *cmd, int argc, char **argv)
{
    int ret;
    int i;
    int j;

    for (i = 0; i < argc; i++) {
        for (j = 0; j < cmd->option_count; j++) {
            ret = cmp_option(cmd, argv, &i, j);
            if (ret == IS_MATCH) {
                break;
            } else if (ret == NOT_MATCH) {
                continue;
            } else {
                return ret;
            }
        }
        if (j == cmd->option_count) {
            roce_err("Do not find the options");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }

    return UDA_EXE_SUCCESS;
}

int major_command_option(major_cmd_t *cmd, char *little, char *large, int have_param, command_record_t record)
{
    cmd_option_t *option = NULL;

    if ((cmd == NULL) || ((little == NULL) && (large == NULL)) || (record == NULL)) {
        roce_err("Invalid input parameter");
        return UDA_PARAM_INVALID_ERR;
    }

    if (cmd->option_count >= COMMAND_MAX_OPTIONS) {
        roce_err("Do not support more than %d options", cmd->option_count);
        return UDA_PARAM_INVALID_ERR;
    }

    option = &cmd->options[cmd->option_count];
    cmd->options_repeat_flag[cmd->option_count] = 0;
    cmd->option_count++;

    option->record = record;
    option->little = little;
    option->large = large;
    option->have_param = (unsigned int)have_param;

    return UDA_EXE_SUCCESS;
}