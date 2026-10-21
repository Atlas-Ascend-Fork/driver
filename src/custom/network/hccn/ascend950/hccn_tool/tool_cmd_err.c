/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tls.h"
#include "hccn_err.h"
#include "tool_comm.h"
#include "tool_log.h"
#include "tool_param.h"
#include "tool_cmd.h"
#include "tool_cmd_err.h"

void tool_print_help_prompt(int argc, char **argv, int err_help_level)
{
    switch (err_help_level) {
        case HELP_LEVEL_NONE:
            break;
        case HELP_LEVEL_GLOBAL:
            TOOL_PRINT_ERR("Enter [ hccn_tool -h ] for more details.");
            break;
        case HELP_LEVEL_MODE:
            TOOL_PRINT_ERR("Enter [ hccn_tool -h %s ] for more details.", argv[ARGS_MODE_INDEX]);
            break;
        case HELP_LEVEL_CMD:
            TOOL_PRINT_ERR("Enter [ hccn_tool -h %s ] for more details.", argv[ARGS_MAIN_CMD_INDEX]);
            break;
        default:
            break;
    }
    return;
}

void tool_print_npu_info(unsigned int npu_info)
{
    switch (npu_info) {
        case DEV_PORT_TYPE:
            TOOL_PRINT_OPT("-i <dev_id>", "device id");
            TOOL_PRINT_OPT("-u <udie_id>", "udie id");
            TOOL_PRINT_OPT("-p <port_id>", "port id");
            break;
        case DEV_DEV_ATTR:
            TOOL_PRINT_OPT("-i <dev_id>", "device id");
            break;
        case DEV_NPU_ATTR:
            TOOL_PRINT_OPT("-i <dev_id>", "device id");
            break;
        case DEV_GLOBAL_ATTR:
            TOOL_PRINT_OPT("-all", "all devices");
            break;
        case DEV_OPTICAL_ATTR:
            TOOL_PRINT_OPT("-i <dev_id>", "device id");
            TOOL_PRINT_OPT("-optical_id <optical_id>", "optical id");
            break;
        default:
            break;
    }
}

void tool_print_dev_info(unsigned int dev_info)
{
    switch (dev_info) {
        case DEV_UDMA_DEV:
            TOOL_PRINT_OPT("-d <udma_dev_name>", "udma dev name");
            break;
        case DEV_NET_DEV:
            TOOL_PRINT_OPT("-d <ndev_name>", "netdev name");
            break;
        case DEV_BOND_DEV:
            TOOL_PRINT_OPT("-d <bond_ndev_name>", "bond netdev name");
            break;
        default:
            break;
    }
}

STATIC void tool_print_help_global(void)
{
    const char *modes[] = {"-g", "-s", "-t"};
    unsigned int idx = 0;

    TOOL_PRINT_INFO("Usage");
    TOOL_PRINT_INFO("\thccn_tool -mode -cmd -npu_info [-options]");
    TOOL_PRINT_INFO("mode:");
    for (idx = 0; idx < ARRAY_SIZE_COMPUTE(modes); idx++) {
        TOOL_PRINT_INFO("\t%s\tFor -cmd and -npu_info, enter [ hccn_tool -h %s ] for more details.", modes[idx],
                        modes[idx]);
    }
    TOOL_PRINT_INFO("example:");
    TOOL_PRINT_INFO("\thccn_tool -g -link -i 0 -u 1 -p 0\tquery link status");
}

STATIC void tool_print_help_by_mode(struct command_context *ctx)
{
    if (ctx->match_cmd == NULL || ctx->match_cmd->exec == NULL) {
        tool_print_help_prompt(0, NULL, HELP_LEVEL_GLOBAL);
        return;
    }

    (void)ctx->match_cmd->exec(ctx);
}

STATIC void tool_help_exec_by_mode(struct command_context *ctx, unsigned int mode)
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
            if (cmds[cmd_idx].print_help == NULL) {
                continue;
            }
            cmds[cmd_idx].print_help(ctx, mode);
        }
    }
}

STATIC void tool_print_help_by_cmd(struct command_context *ctx)
{
    if (ctx->match_cmd == NULL || ctx->match_cmd->print_help == NULL) {
        tool_print_help_prompt(0, NULL, HELP_LEVEL_GLOBAL);
        return;
    }

    ctx->match_cmd->print_help(ctx, MODE_ALL);
    return;
}

void tool_exe_help(struct command_context *ctx)
{
    switch (ctx->help_level) {
        case HELP_LEVEL_NONE:
            break;
        case HELP_LEVEL_GLOBAL:
            tool_print_help_global();
            break;
        case HELP_LEVEL_MODE:
            tool_print_help_by_mode(ctx);
            break;
        case HELP_LEVEL_CMD:
            tool_print_help_by_cmd(ctx);
            break;
        default:
            break;
    }
    return;
}

STATIC void tool_handle_err_comm(struct command_context *ctx, int exe_ret, struct tool_err_info *err_list,
                                 unsigned int size, int *err_help_level)
{
    struct tool_err_info *err_info = NULL;
    unsigned int i;

    for (i = 0; i < size; i++) {
        if (err_list[i].err_code != exe_ret) {
            continue;
        }
        err_info = &err_list[i];
        break;
    }

    if (err_info == NULL) {
        TOOL_PRINT_ERR("Cmd execute failed!");
        return;
    }

    if (err_info->err_str != NULL) {
        if (err_info->err_level == PRINT_LEVEL_INFO) {
            TOOL_PRINT_INFO("%s", err_info->err_str);
        } else {
            TOOL_PRINT_ERR("%s", err_info->err_str);
        }
    }

    *err_help_level = err_info->help_level;
}

STATIC void tool_handle_user_input_err(struct command_context *ctx, int ret, int *err_help_level)
{
    static struct tool_err_info err_list[] = {
        {TOOL_PUBLIC_MODE_EMPTY_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_GLOBAL, "Please specify a mode."},
        {TOOL_PUBLIC_MODE_TYPE_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_GLOBAL, "Please specify a valid mode."},
        {TOOL_PUBLIC_CMD_EMPTY_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_MODE, "Please specify a cmd."},
        {TOOL_PUBLIC_CMD_TYPE_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_MODE, "Please specify a valid cmd."},
        {TOOL_PUBLIC_CMD_MODE_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, NULL},
        {TOOL_PUBLIC_ARGS_PARSE_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, "Parse cmd option failed."},
        {TOOL_PUBLIC_ARGS_ID_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, NULL},
        {TOOL_PUBLIC_ARGS_RANGE_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, NULL},
        {TOOL_PUBLIC_ARGS_REPEAT_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, NULL},
        {TOOL_PUBLIC_ARGS_MISMATCH_CMD_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, "Parse cmd option failed."},
        {TOOL_PUBLIC_ARGS_CHIP_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, "target chip is invalid, please check."},
        {TOOL_PUBLIC_ARGS_UDIE_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, "target udie is invalid, please check."},
        {TOOL_PUBLIC_ARGS_PORT_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, "target port is invalid, please check."},
        {TOOL_PUBLIC_ARGS_DEV_NAME_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, NULL},
        {TOOL_PRIVATE_ARGS_NUM_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, "The total count of options is wrong."},
        {TOOL_PRIVATE_ARGS_PARSE_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, "Parse cmd option failed."},
        {TOOL_PRIVATE_ARGS_REPEAT_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, NULL},
        {TOOL_PRIVATE_ARGS_CONFLICT_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, NULL},
        {TOOL_PRIVATE_ARGS_MISMATCH_MODE_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, NULL},
        {TOOL_PRIVATE_ARGS_VALUE_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, NULL},
        {TOOL_EXE_USER_ABORT_CMD_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "User aborts command."},
    };

    tool_handle_err_comm(ctx, ret, err_list, ARRAY_SIZE_COMPUTE(err_list), err_help_level);
}

STATIC void tool_handle_tls_err(struct command_context *ctx, int ret, int *err_help_level)
{
    static struct tool_err_info err_list[] = {
        {TLS_INPUT_CERT_COUNT_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD, "The total count of options is wrong."},
        {
            TLS_PWD_COMPLEXITY_ERR,
            PRINT_LEVEL_INFO,
            HELP_LEVEL_NONE,
            "The complexity of password you input is too low, please choose another one!",
        },
        {
            TLS_PWD_LEN_INVALID_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE,
            "Your input password length is illegal, length must between [8] and [15]", // PWD_MIN_LEN PWD_MAX_LEN
        },
        {TLS_NO_WEAK_PWD_DICT, PRINT_LEVEL_INFO, HELP_LEVEL_NONE, NULL},
        {TLS_WEAK_PWD_DICT_NULL, PRINT_LEVEL_INFO, HELP_LEVEL_NONE, NULL},
        {TLS_WEAK_PWD_DICT_TOO_MANY, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, NULL},
        {TLS_WEAK_PWD_DICT_CHECK_FAIL, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, NULL},
        {TLS_PWD_TOO_WEAK_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, NULL},
        {TLS_NO_CERT_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "No certificate found, please set certs firstly!"},
        {TLS_CERT_EXPIRED_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "The Certificate has expired!"},
        {TLS_CERT_ILLEGAL_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "The certificates are illegal!"},
        {
            TLS_CERT_VERIFY_FAIL,
            PRINT_LEVEL_ERR,
            HELP_LEVEL_NONE,
            "Failed to verify the certificate chain, make sure the certificate chain is correct!",
        },
        {TLS_CERT_LEN_INVALID_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "The certificate length is invalid!"},
        {TLS_CERT_DISCONTINOUS_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "The certificate chain is not continuous!"},
        {TLS_CA_WITHOUT_PRI_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "No pri key, cannot assign ca without pri key!"},
    };

    tool_handle_err_comm(ctx, ret, err_list, ARRAY_SIZE_COMPUTE(err_list), err_help_level);
}

STATIC void tool_handle_dev_exe_err(struct command_context *ctx, int ret, int *err_help_level)
{
    static struct tool_err_info err_list[] = {
        {DEV_SYS_BUSY_ERR, PRINT_LEVEL_INFO, HELP_LEVEL_NONE, "hccn_tool is busy, please try again."},
        {
            TOOL_PUBLIC_ROOT_UNSUPPORT,
            PRINT_LEVEL_ERR,
            HELP_LEVEL_NONE,
            "permission denied: pls check this cmd run permission.",
        },
        {
            TOOL_PUBLIC_ENV_UNSUPPORT,
            PRINT_LEVEL_ERR,
            HELP_LEVEL_NONE,
            "permission denied: pls check this cmd run environment.",
        },
        {DEV_EXE_CONFLICT_IP_ADDR_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "The ip address configured is conflicted!"},
        {DEV_EXE_GATEWAY_NOT_PRESET_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE,
         "Get gateway failed, because no gateway was preset there!"},
        {DEV_EXE_IP_ADDRESS_NOT_PRESET_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE,
         "Get ip failed, because no ip was preset there!"},
        {DEV_EXE_DIFF_SEGMT_GTWY_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE,
         "The segment of configured gateway doesn't match that of the configured ip address!"},
        {TOOL_DHCPC_PROCESS_EXIST_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "udhcpc running, don't run again!"},
        {TOOL_DHCPC_CONFIG_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "Cmd execute failed, no ip and router set."},
        {TOOL_TROUTE_PROCESS_EXIST_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_CMD,
         "Traceroute is running, please kill the running process and start a new one."},
        {DEV_EXE_TRACEROUTE_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "Traceroute execution failed."},
        {DEV_EXE_TRACEROUTE_TIMEOUT, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "Traceroute execution timeout."},
        {TOOL_EXE_DEV_INFO_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, "Get device info failed."},
        {DEV_EXE_ETH_TEST_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, NULL},
        {DEV_QOS_MAP_CONFLICT_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, NULL},
        {DEV_QOS_UNMAP_REPEAT_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE, NULL},
        {TOOL_GATEWAY_OR_ARP_RECOVERY_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE,
         "Link up executed, but recovery failed, please check '/var/log/hccn_tool/hccn_config.log' for details."},
        {TOOL_WRITE_CONF_ERR, PRINT_LEVEL_ERR, HELP_LEVEL_NONE,
         "Cmd executed successfully, but persistent configuration failed."},
    };

    tool_handle_err_comm(ctx, ret, err_list, ARRAY_SIZE_COMPUTE(err_list), err_help_level);
}

void tool_handle_cmd_err(int argc, char **argv, struct command_context *ctx, int ret)
{
    int err_help_level = HELP_LEVEL_NONE;
    unsigned int err_type;

    if (ret == TOOL_EXE_SUCCESS) {
        if (ctx->mode != MODE_GET && ctx->mode != MODE_HELP) {
            TOOL_PRINT_INFO("Cmd executed successfully!");
        }
        LOG_PROMPT(ctx, "hccn tool execution succeeded!");
        return;
    } else {
        LOG_PROMPT(ctx, "hccn tool execution failed! ret=0x%x", ret);
    }

    err_type = ((unsigned int)ret & HCCN_ERR_MASK);
    switch (err_type) {
        case TOOL_USER_INPUT_PARAM_ERR:
            tool_handle_user_input_err(ctx, ret, &err_help_level);
            break;
        case DSMI_EXE_ERR:
            break;
        case DEV_EXE_ERR:
            tool_handle_dev_exe_err(ctx, ret, &err_help_level);
            break;
        case TOOL_TLS_ERR:
            tool_handle_tls_err(ctx, ret, &err_help_level);
            break;
        default:
            TOOL_PRINT_ERR("Cmd execute failed!");
    }

    tool_print_help_prompt(argc, argv, err_help_level);
    return;
}

STATIC int tool_help_exec_by_get_mode(struct command_context *ctx)
{
    tool_help_exec_by_mode(ctx, MODE_GET);
    return 0;
}

STATIC int tool_help_exec_by_set_mode(struct command_context *ctx)
{
    tool_help_exec_by_mode(ctx, MODE_SET);
    return 0;
}

STATIC int tool_help_exec_by_test_mode(struct command_context *ctx)
{
    tool_help_exec_by_mode(ctx, MODE_TEST);
    return 0;
}

STATIC struct cmd_entry g_help_cmds[] = {
    {"-g", NULL, MODE_HELP, NULL, NULL, tool_help_exec_by_get_mode},
    {"-s", NULL, MODE_HELP, NULL, NULL, tool_help_exec_by_set_mode},
    {"-t", NULL, MODE_HELP, NULL, NULL, tool_help_exec_by_test_mode},
};

void tool_get_help_cmds(struct cmd_entry **cmds, unsigned int *count)
{
    *cmds = g_help_cmds;
    *count = ARRAY_SIZE_COMPUTE(g_help_cmds);
}

int tool_double_check(const char *info)
{
    int echo;

    TOOL_PRINT_INFO("%s(y/n)", info);
    echo = getchar();
    if ((echo != 'y') && (echo != 'Y')) {
        return TOOL_EXE_USER_ABORT_CMD_ERR;
    }
    echo = getchar();
    if (echo != '\n') {
        return TOOL_EXE_USER_ABORT_CMD_ERR;
    }

    return 0;
}
