/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "securec.h"
#include "ascend_hal.h"
#include "dsmi_common_interface.h"
#include "hccn_err.h"
#include "hccn_comm.h"
#include "ds_data.h"
#include "tool_comm.h"
#include "tool_param.h"
#include "tool_cmd.h"
#include "tool_log.h"
#include "tool_cmd_err.h"
#include "tool_product.h"
#include "tool_cmd_parse.h"

STATIC int parse_sub_options(int argc, char **argv, int idx, struct command_context *ctx)
{
    struct sub_option_entry *match_opt = NULL;
    struct sub_option_entry *sub_opts = NULL;
    struct cmd_entry *cmd = ctx->match_cmd;
    unsigned int j, sub_opt_count = 0;
    value_union type_value = {0};
    const char *val = NULL;
    char *sub_opt = NULL;
    int i, ret;

    TOOL_CHECK_PTR_VALID_RETURN_VAL(cmd->get_sub_opts, -EINVAL);
    cmd->get_sub_opts(&sub_opts, &sub_opt_count);

    for (i = idx; i < argc; i++) {
        CHK_PRT_RETURN(argv[i][0] != '-', hccn_err("index %d %s needs input -opt rather than value", i, argv[i]),
                       TOOL_PRIVATE_ARGS_PARSE_ERR);

        sub_opt = argv[i];
        val = (i + 1 < argc) && (argv[i + 1][0] != '-' || is_valid_integer(argv[i + 1])) ? argv[i + 1] : NULL;

        for (j = 0; j < sub_opt_count; j++) {
            if (strcmp(sub_opt, sub_opts[j].name) != 0) {
                continue;
            }

            CHK_PRT_RETURN((ctx->sub_opt_mask & HCCN_BIT(j)) != 0, TOOL_PRINT_ERR("Option %s duplicated.", sub_opt),
                           TOOL_PRIVATE_ARGS_REPEAT_ERR);

            match_opt = &sub_opts[j];
            CHK_PRT_RETURN((match_opt->param_mode_mask & ctx->mode) == 0,
                           TOOL_PRINT_ERR("Option %s not allowed in mode %s.", sub_opt, argv[ARGS_MODE_INDEX]),
                           TOOL_PRIVATE_ARGS_MISMATCH_MODE_ERR);

            TOOL_CHECK_PTR_VALID_RETURN_VAL(match_opt->parse, -EINVAL);
            ret = match_opt->parse(match_opt, val, &type_value);
            CHK_PRT_RETURN(ret != 0, hccn_err("%s convert failed, ret:%d", sub_opt, ret), ret);

            CHK_PRT_RETURN(match_opt->has_range && !is_value_in_range(match_opt, &type_value),
                           hccn_err("option %s value is out of range", sub_opt), TOOL_PRIVATE_ARGS_VALUE_ERR);

            ret = cmd->assign(ctx->cmd_args, match_opt, &type_value);
            CHK_PRT_RETURN(ret != 0, hccn_err("%s cmd->assign failed, ret:%d", sub_opt, ret), ret);

            ctx->sub_opt_mask |= HCCN_BIT(j);
            (void)memset_s(&type_value, sizeof(value_union), 0, sizeof(value_union));
            break;
        }

        CHK_PRT_RETURN(j == sub_opt_count, TOOL_PRINT_ERR("Unknown option %s.", sub_opt),
                       TOOL_PRIVATE_ARGS_MISMATCH_MODE_ERR);

        if (val) {
            i++;
        }
    }

    for (j = 0; j < sub_opt_count; j++) {
        CHK_PRT_RETURN(
            (sub_opts[j].param_required_mask & ctx->mode) != 0 && (ctx->sub_opt_mask & HCCN_BIT(j)) == 0,
            TOOL_PRINT_ERR("Option %s is required in mode %s but missing.", sub_opts[j].name, argv[ARGS_MODE_INDEX]),
            TOOL_PRIVATE_ARGS_MISMATCH_MODE_ERR);
    }

    return 0;
}

STATIC bool check_dev_combination(struct cmd_entry *cmd, unsigned int dev_mask)
{
    unsigned int i;

    for (i = 0; i < DEV_COMBINATION_NUM; i++) {
        if (cmd->supported_dev_mask[i] == dev_mask) {
            return true;
        }
    }

    return false;
}

STATIC int assign_device_id(struct dev_opt_entry *dev_entry, char *argv_opt, char *argv_val)
{
    int int_val, int_min, int_max;
    char *endptr = NULL;
    int ret = 0;

    switch (dev_entry->val_type) {
        case ARG_TYPE_INT:
            *dev_entry->value_int = (int)strtol(argv_val, &endptr, 0);
            CHK_PRT_RETURN(endptr != NULL && *endptr != '\0',
                           hccn_err("strtol failed, argv[%s %s] is invalid", argv_opt, argv_val),
                           TOOL_PUBLIC_ARGS_PARSE_ERR);
            CHK_PRT_RETURN(*dev_entry->value_int < 0,
                           TOOL_PRINT_ERR("Option [%s %s] is invalid, %s < 0.", argv_opt, argv_val, argv_val),
                           TOOL_PUBLIC_ARGS_ID_ERR);

            // no need to check range if no range exist
            if (!dev_entry->has_range) {
                break;
            }
            int_min = dev_entry->range.int_range.min;
            int_max = dev_entry->range.int_range.max;
            int_val = *dev_entry->value_int;
            CHK_PRT_RETURN(
                ((int_val < int_min) || (int_val > int_max)),
                TOOL_PRINT_ERR("Option [%s %s] is out of range [%d, %d].", argv_opt, argv_val, int_min, int_max),
                TOOL_PUBLIC_ARGS_RANGE_ERR);
            break;
        case ARG_TYPE_FLAG:
            *dev_entry->value_flag = true;
            break;
        case ARG_TYPE_STR:
            ret = strncpy_s(dev_entry->value_str, dev_entry->value_size, argv_val, strlen(argv_val));
            CHK_PRT_RETURN(ret != 0, hccn_err("strncpy_s argv[%s] failed, ret:%d", argv_val, ret),
                           TOOL_PUBLIC_ARGS_PARSE_ERR);
            break;
        default:
            hccn_err("unknown val_type:%d, argv[%s]", dev_entry->val_type, argv_opt);
            return TOOL_PUBLIC_ARGS_PARSE_ERR;
    }

    return ret;
}

int get_other_id_from_logic_id(struct command_context *ctx)
{
    struct dsmi_board_info_stru pboard_info = {0};
    int ret = 0;

    ret = dsmi_get_phyid_from_logicid((unsigned int)ctx->npu_info.logic_id, (unsigned int *)(void *)&ctx->dev_id);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("dsmi_get_phyid_from_logicid failed, logic_id:%d ret:%d", ctx->npu_info.logic_id, ret),
                   TOOL_PUBLIC_ARGS_CHIP_ERR);

    ret = dsmi_get_board_info(ctx->npu_info.logic_id, &pboard_info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi_get_board_info failed, logic_id:%d ret:%d", ctx->npu_info.logic_id, ret),
                   TOOL_PUBLIC_ARGS_CHIP_ERR);
    ctx->slot_id = (int)pboard_info.slot_id;

    hccn_info("get phy_id & slot_id success! logic_id:%d phy_id:%d slot_id:%d", ctx->npu_info.logic_id, ctx->dev_id,
              ctx->slot_id);

    ret = tool_get_product_other_info(ctx);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("tool_get_product_other_info failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
                   TOOL_PUBLIC_ARGS_CHIP_ERR);

    return 0;
}

STATIC int parse_device_ids(int argc, char **argv, struct command_context *ctx, int *i_result)
{
    struct dev_opt_entry dev_entry[] = {
        {"-i", DEV_I, ARG_TYPE_INT, {.value_int = &ctx->npu_info.logic_id}},
        {"-u", DEV_U, ARG_TYPE_INT, {.value_int = &ctx->udie_id}, sizeof(int), true, .range.int_range = {0, 1}},
        {"-p", DEV_P, ARG_TYPE_INT, {.value_int = &ctx->port_id}, sizeof(int), true, .range.int_range = {0, 9}},
        {"-d", DEV_D, ARG_TYPE_STR, {.value_str = ctx->dev_name}, sizeof(ctx->dev_name)},
        {"-optical_id",
         DEV_OPTICAL,
         ARG_TYPE_INT,
         {.value_int = &ctx->optical_id},
         sizeof(int),
         true,
         .range.int_range = {0, 4}},
        {"-all", DEV_ALL, ARG_TYPE_FLAG, {.value_flag = &ctx->global_flag}},
        {"-global", DEV_ALL, ARG_TYPE_FLAG, {.value_flag = &ctx->global_flag}},
    };
    int entry_size = ARRAY_SIZE_COMPUTE(dev_entry);
    int arg_idx = ARGS_MAIN_CMD_INDEX + 1;
    char *argv_opt = NULL;
    char *argv_val = NULL;
    int entry_idx = 0;
    int ret;

    for (; arg_idx < argc; arg_idx++) {
        for (entry_idx = 0; entry_idx < entry_size; entry_idx++) {
            if (strcmp(argv[arg_idx], dev_entry[entry_idx].name) != 0) {
                continue;
            }

            argv_opt = argv[arg_idx];
            if (dev_entry[entry_idx].val_type != ARG_TYPE_FLAG) {
                arg_idx++;
                CHK_PRT_RETURN(arg_idx >= argc, hccn_err("argv[%s] must specify a value", argv_opt),
                               TOOL_PUBLIC_ARGS_PARSE_ERR);
            }
            argv_val = argv[arg_idx];
            ENSURE_PARAM_UNIQUE(ctx->dev_mask, dev_entry[entry_idx].dev_type, argv_opt);

            ret = assign_device_id(&dev_entry[entry_idx], argv_opt, argv_val);
            CHK_PRT_RETURN(ret != 0, hccn_err("assign_device_id failed"), ret);
            break;
        }

        // arg could be private, no need to parse the remaining args
        if (entry_idx == entry_size) {
            break;
        }
    }

    if (ctx->dev_mask == 0 || !check_dev_combination(ctx->match_cmd, ctx->dev_mask)) {
        hccn_err("device combination match or check failed, ctx->dev_mask:0x%x", ctx->dev_mask);
        return TOOL_PUBLIC_ARGS_MISMATCH_CMD_ERR;
    }

    if (CHECK_HCCN_BIT(ctx->dev_mask, DEV_I)) {
        ret = get_other_id_from_logic_id(ctx);
        CHK_PRT_RETURN(ret != 0,
                       hccn_err("get_other_id_from_logic_id failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
                       ret);
    }

    *i_result = arg_idx;

    return 0;
}

STATIC struct cmd_entry *find_match_cmd(struct command_context *ctx, const char *opt)
{
    tool_get_cmds_func *cmds_func = NULL;
    unsigned int func_count = 0;
    struct cmd_entry *cmds = NULL;
    unsigned int cmd_count = 0;
    int func_idx = 0;
    int cmd_idx = 0;

    // non-help cmds
    tool_get_main_cmds_funcs(&cmds_func, &func_count);
    for (func_idx = 0; func_idx < func_count; func_idx++) {
        cmds_func[func_idx](&cmds, &cmd_count);
        for (cmd_idx = 0; cmd_idx < cmd_count; cmd_idx++) {
            if (strcmp(opt, cmds[cmd_idx].name) == 0) {
                return &cmds[cmd_idx];
            }
        }
    }

    if (ctx->mode != MODE_HELP) {
        return NULL;
    }

    // help cmds
    tool_get_help_cmds(&cmds, &cmd_count);
    for (cmd_idx = 0; cmd_idx < cmd_count; cmd_idx++) {
        if (strcmp(opt, cmds[cmd_idx].name) == 0) {
            return &cmds[cmd_idx];
        }
    }
    return NULL;
}

STATIC int parse_verb_args(int argc, char **argv, struct command_context *ctx)
{
    struct mode_opt_entry mode_entry[] = {
        {"-g", MODE_GET}, {"-s", MODE_SET}, {"-t", MODE_TEST}, {"-h", MODE_HELP}, {"--help", MODE_HELP},
    };
    int entry_size = ARRAY_SIZE_COMPUTE(mode_entry);
    int idx = 0;
    char *opt;

    TOOL_CHECK_PTR_VALID_RETURN_VAL(argv, TOOL_PUBLIC_MODE_EMPTY_ERR);
    CHK_PRT_RETURN(argc < ARGS_MODE_INDEX + 1, hccn_err("argc invalid, argc:%d < %d", argc, ARGS_MODE_INDEX + 1),
                   TOOL_PUBLIC_MODE_EMPTY_ERR);

    opt = argv[ARGS_MODE_INDEX];
    for (idx = 0; idx < entry_size; idx++) {
        if (strcmp(opt, mode_entry[idx].name) == 0) {
            ctx->mode = mode_entry[idx].mode_type;
            break;
        }
    }
    CHK_PRT_RETURN(idx == entry_size, hccn_err("invalid mode:%s", opt), TOOL_PUBLIC_MODE_TYPE_ERR);

    // hccn_tool -h
    CHK_PRT_RETURN(ctx->mode == MODE_HELP && argc == ARGS_MODE_INDEX + 1, ctx->help_level = HELP_LEVEL_GLOBAL, 0);
    CHK_PRT_RETURN(argc < ARGS_MAIN_CMD_INDEX + 1,
                   hccn_err("argc invalid, argc:%d < %d", argc, ARGS_MAIN_CMD_INDEX + 1), TOOL_PUBLIC_CMD_EMPTY_ERR);

    opt = argv[ARGS_MAIN_CMD_INDEX];
    ctx->match_cmd = find_match_cmd(ctx, opt);
    CHK_PRT_RETURN(ctx->mode == MODE_HELP && ctx->match_cmd == NULL, hccn_err("invalid main_cmd:%s", opt),
                   TOOL_PUBLIC_MODE_TYPE_ERR);
    CHK_PRT_RETURN(ctx->match_cmd == NULL, hccn_err("invalid main_cmd:%s", opt), TOOL_PUBLIC_CMD_TYPE_ERR);
    // hccn_tool -h -cmd
    CHK_PRT_RETURN(ctx->mode == MODE_HELP && argc > ARGS_MAIN_CMD_INDEX + 1,
                   hccn_err("input invalid option in help mode, argc:%d > %d", argc, ARGS_MAIN_CMD_INDEX + 1),
                   TOOL_PRIVATE_ARGS_NUM_ERR);
    CHK_PRT_RETURN(ctx->mode == MODE_HELP && !CHECK_HCCN_BIT(ctx->match_cmd->mode_mask, ctx->mode),
                   ctx->help_level = HELP_LEVEL_CMD, 0);

    CHK_PRT_RETURN(!CHECK_HCCN_BIT(ctx->match_cmd->mode_mask, ctx->mode),
                   TOOL_PRINT_ERR("The cmd %s does not support mode %s", ctx->match_cmd->name, argv[ARGS_MODE_INDEX]),
                   TOOL_PUBLIC_CMD_MODE_ERR);

    // hccn_tool -h -mode
    CHK_PRT_RETURN(ctx->mode == MODE_HELP, ctx->help_level = HELP_LEVEL_MODE, 0);
    return 0;
}

STATIC bool is_mode_required_in_sub_opts(struct command_context *ctx)
{
    struct cmd_entry *match_cmd = ctx->match_cmd;
    struct sub_option_entry *sub_opts = NULL;
    unsigned int sub_opt_count = 0;
    unsigned int i = 0;

    TOOL_CHECK_PTR_VALID_RETURN_VAL(match_cmd->get_sub_opts, -EINVAL);
    match_cmd->get_sub_opts(&sub_opts, &sub_opt_count);

    for (i = 0; i < sub_opt_count; i++) {
        if ((sub_opts[i].param_required_mask & ctx->mode) != 0) {
            return true;
        }
    }
    return false;
}

STATIC int parse_private_args(int argc, char **argv, struct command_context *ctx, int idx)
{
    struct cmd_entry *main_cmd = ctx->match_cmd;
    int ret = 0;
    int i = 0;

    if (main_cmd->child != NULL) {
        for (; i < main_cmd->child_count; i++) {
            if (strcmp(argv[idx], main_cmd->child[i].name) == 0) {
                ctx->match_cmd = &main_cmd->child[i];
                break;
            }
        }

        if (i == main_cmd->child_count) {
            if (main_cmd->child[0].name == NULL) { // 可能存在子命令名为NULL的项
                ctx->match_cmd = &main_cmd->child[0];
            } else {
                hccn_err("Invalid child_cmd %s for command %s in mode:%s", argv[idx], main_cmd->name,
                         argv[ARGS_MODE_INDEX]);
                return TOOL_USER_INPUT_PARAM_ERR;
            }
        }
    }

    if (idx == argc) {
        CHK_PRT_RETURN(ctx->match_cmd->alloc_args != NULL && is_mode_required_in_sub_opts(ctx),
                       hccn_err("match_cmd needs sub options to be input, idx:%d argc:%d", idx, argc),
                       TOOL_PRIVATE_ARGS_NUM_ERR);
        return 0;
    }
    CHK_PRT_RETURN(ctx->match_cmd->alloc_args == NULL, hccn_err("alloc_args is NULL, idx:%d argc:%d", idx, argc),
                   TOOL_PRIVATE_ARGS_NUM_ERR);

    ctx->cmd_args = ctx->match_cmd->alloc_args();
    CHK_PRT_RETURN(ctx->cmd_args == NULL, hccn_err("%s alloc_args failed", ctx->match_cmd->name), -EINVAL);

    ret = parse_sub_options(argc, argv, idx, ctx);
    CHK_PRT_RETURN(ret != 0, hccn_err("parse_sub_options failed, ret:%d", ret), ret);

    return 0;
}

int parse_args(int argc, char **argv, struct command_context *ctx)
{
    int index = 0;
    int ret;

    // 解析公共部分
    ret = parse_verb_args(argc, argv, ctx);
    CHK_PRT_RETURN(ret != 0, hccn_err("parse_verb_args failed! ret:%d", ret), ret);
    CHK_PRT_RETURN(ctx->mode == MODE_HELP, tool_exe_help(ctx), TOOL_EXE_HELP);

    ret = parse_device_ids(argc, argv, ctx, &index);
    CHK_PRT_RETURN(ret != 0, hccn_err("parse_device_ids failed! ret:%d", ret), ret);

    // 若存在私有部分，解析私有部分
    ret = parse_private_args(argc, argv, ctx, index);
    CHK_PRT_RETURN(ret != 0, hccn_err("parse_private_args failed! ret:%d", ret), ret);

    return 0;
}

void dump_prompt_log(int argc, char **argv, struct command_context *ctx)
{
    int i;

    LOG_PROMPT(ctx, "input parameters: argc[%d]", argc);
    for (i = 0; i < argc; i++) {
        LOG_PROMPT(ctx, "parse [%d]th argv[%s]", i, argv[i]);
    }
}

// ------------------ 执行 ------------------
int execute(struct command_context *ctx)
{
    int (*tool_exec)(struct command_context *ctx) = ctx->match_cmd->exec;
    int ret;

    CHK_PRT_RETURN(tool_exec == NULL, hccn_err("tool_exec is NULL"), -EINVAL);
    ret = tool_exec(ctx);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool execute failed! ret:%d", ret), ret);

    return 0;
}
