/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <string.h>
#include "securec.h"
#include "ds_net_interface.h"
#include "dsmi_common_interface.h"
#include "file_opt.h"
#include "hccn_err.h"
#include "hccn_comm.h"
#include "ds_data.h"
#include "shell_cmd_data.h"
#include "dev_info_data.h"
#include "tool_param.h"
#include "tool_cmd_err.h"
#include "tool_cfg_ops.h"
#include "tool_dev_info.h"
#include "tool_bw_ops.h"
#include "tool_log.h"
#include "tool_udis.h"
#include "tool_cfg_ops.h"
#include "tool_shell_ops.h"

STATIC struct sub_option_entry context_opts_param[] = {
    {"-aeqc", ARG_TYPE_INT, parse_int, MODE_GET, 0, true, .range.int_range = {0, 0}},
    {"-ceqc", ARG_TYPE_INT, parse_int, MODE_GET, 0, true, .range.int_range = {0, 0}},
};

STATIC void get_context_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = context_opts_param;
    *count = ARRAY_SIZE_COMPUTE(context_opts_param);
}

STATIC void *alloc_context_args_init(void)
{
    return calloc(1, sizeof(struct context_cmd_args));
}

STATIC int context_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct context_cmd_args *ctx_args = (struct context_cmd_args *)args;

    if (strcmp(opt->name, "-aeqc") == 0 && ctx_args->type == DS_TYPE_NONE) {
        ctx_args->type = DS_TYPE_AEQC;
    } else if (strcmp(opt->name, "-ceqc") == 0 && ctx_args->type == DS_TYPE_NONE) {
        ctx_args->type = DS_TYPE_CEQC;
    } else {
        TOOL_PRINT_ERR("Option -aeqc and -ceqc conflict with each other.");
        return TOOL_PRIVATE_ARGS_CONFLICT_ERR;
    }
    ctx_args->index = type_val->int_val;

    return 0;
}

STATIC int tool_context_exec(struct command_context *ctx)
{
    unsigned int size = OUTBUF_LEN_256 - 1;
    char ctx_res[OUTBUF_LEN_256] = {0};
    struct dsmi_info info = {0};
    int ret = 0;

    CHK_PRT_RETURN(ctx->cmd_args == NULL, hccn_err("ctx->cmd_args is NULL"), TOOL_PRIVATE_ARGS_NUM_ERR);

    ctx->result = (char *)ctx_res;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)ctx->cmd_args,
                   sizeof(struct context_cmd_args), ctx->result, &size);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_CONTEXT, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get context failed, ret:%d", ret), ret);

    TOOL_PRINT_INFO("%s", ctx->result);
    return 0;
}

STATIC void tool_context_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_context_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-context", "get the context info");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_UDMA_DEV);
            TOOL_PRINT_OPT("-aeqc <aeq_id>", "get the specified aeqc context info, range [0, 0]");

            TOOL_PRINT_CMD("-g", "-context", "get the context info");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_UDMA_DEV);
            TOOL_PRINT_OPT("-ceqc <ceq_id>", "get the specified ceqc context info, range [0, 0]");
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry show_res_opts_param[] = {
    {"-resource_type", ARG_TYPE_INT, parse_int, MODE_GET, MODE_GET, true,
     .range.int_range = {5, 11}}, // range decided by urma_admin, MIN:5(JFS), MAX:11(SEG)
    {"-key_id", ARG_TYPE_INT, parse_int, MODE_GET, MODE_GET}};

STATIC void get_show_res_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = show_res_opts_param;
    *count = ARRAY_SIZE_COMPUTE(show_res_opts_param);
}

STATIC void *alloc_show_res_args_init(void)
{
    return calloc(1, sizeof(struct show_res_cmd_args));
}

STATIC int show_res_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct show_res_cmd_args *res_args = (struct show_res_cmd_args *)args;

    if (strcmp(opt->name, "-resource_type") == 0) {
        res_args->res_type = type_val->int_val;
    } else {
        res_args->key_id = type_val->int_val;
    }

    return 0;
}

STATIC int tool_show_res_exec(struct command_context *ctx)
{
    char show_res_out[OUTBUF_LEN_2048] = {0};
    unsigned int size = OUTBUF_LEN_2048 - 1;
    struct dsmi_info info = {0};
    int ret = 0;

    ctx->result = (char *)show_res_out;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), ctx->cmd_args, sizeof(struct show_res_cmd_args),
                   ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_RES_ATTR, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get resource attr failed, ret:%d", ret), ret);

    TOOL_PRINT_INFO("%s", ctx->result);

    return 0;
}

STATIC void tool_show_res_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_show_res_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-show_res", "get attributes of queue resource");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_UDMA_DEV);
            TOOL_PRINT_OPT("-resource_type <type_id>", "resource type id");
            TOOL_PRINT_OPT("-key_id <key_id>", "resource key id");
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry set_link_opts_param[] = {{"-up", ARG_TYPE_NONE, parse_none, MODE_SET, 0},
                                                        {"-down", ARG_TYPE_NONE, parse_none, MODE_SET, 0}};

STATIC void get_link_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = set_link_opts_param;
    *count = ARRAY_SIZE_COMPUTE(set_link_opts_param);
}

STATIC void *alloc_link_args_init(void)
{
    return calloc(1, sizeof(struct set_link_args));
}

STATIC int set_link_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct set_link_args *link_args = (struct set_link_args *)args;

    if (strcmp(opt->name, "-up") == 0 && link_args->type == SET_LINK_NONE) {
        link_args->type = SET_LINK_UP;
    } else if (strcmp(opt->name, "-down") == 0 && link_args->type == SET_LINK_NONE) {
        link_args->type = SET_LINK_DOWN;
    } else {
        TOOL_PRINT_ERR("Option -up and -down conflict with each other.");
        return TOOL_PRIVATE_ARGS_CONFLICT_ERR;
    }

    return 0;
}

STATIC int tool_get_link_exec(struct command_context *ctx)
{
    unsigned int size = sizeof(bool);
    struct dsmi_info info = {0};
    bool link_status;
    int ret = 0;

    ctx->result = (char *)&link_status;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_INSPECT_LINK_STATUS, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get link status failed, ret:%d", ret), ret);

    if (link_status) {
        TOOL_PRINT_INFO("link status: UP");
    } else {
        TOOL_PRINT_INFO("link status: DOWN");
    }

    return 0;
}

STATIC int tool_set_link_up_recovery(struct command_context *ctx, struct set_link_args *link_args)
{
    struct command_context recovery_ctx = {0};
    int ret = 0;

    (void)memcpy_s(&recovery_ctx, sizeof(struct command_context), ctx, sizeof(struct command_context));
    recovery_ctx.cmd_args = NULL;

    ret = tool_cfg_gateway_recovery(&recovery_ctx);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool gateway recovery failed."), ret);

    ret = tool_cfg_arp_recovery(&recovery_ctx);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool arp recovery failed."), ret);

    return 0;
}

STATIC int tool_set_link_exec(struct command_context *ctx, bool skip_double_check)
{
    struct set_link_args *link_args = (struct set_link_args *)ctx->cmd_args;
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    CHK_PRT_RETURN(link_args == NULL, hccn_err("link_args is NULL"), TOOL_PRIVATE_ARGS_NUM_ERR);

    LOG_PROMPT(ctx, "start to set link status:%s, logic_id:%d dev_name:%s",
               ((link_args->type == SET_LINK_UP) ? "up" : "down"), ctx->npu_info.logic_id, ctx->dev_name);

    if (link_args->type == SET_LINK_DOWN && !skip_double_check) {
        ret = tool_double_check("Running this command will result in the loss of the gateway and arp.\n"
                                "Are you sure you want to continue?");
        CHK_PRT_RETURN(ret != 0, hccn_info("User aborts command"), ret);
    }

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    CHK_PRT_RETURN(!hccn_check_is_uboe_dev(ctx->dev_name), hccn_err("only eth_dev or bond_ndev is allowed"), -EINVAL);

    if (hccn_check_is_bond_dev(ctx->dev_name)) {
        CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);
    }

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), ctx->cmd_args, sizeof(struct set_link_args), NULL,
                   &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_LINK_STATUS, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("set link status failed, ret:%d", ret), ret);

    if (link_args->type == SET_LINK_UP) {
        ret = tool_set_link_up_recovery(ctx, link_args);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool gateway or arp recovery failed, ret:%d", ret),
                       TOOL_GATEWAY_OR_ARP_RECOVERY_ERR);
    }

    return 0;
}

STATIC int tool_link_exec(struct command_context *ctx)
{
    if (ctx->mode == MODE_GET) {
        CHK_PRT_RETURN(ctx->dev_mask != DEV_PORT_TYPE,
                       hccn_err("invalid param, dev_mask:0x%x != 0x%x", ctx->dev_mask, DEV_PORT_TYPE),
                       TOOL_PUBLIC_ARGS_MISMATCH_CMD_ERR);
        return tool_get_link_exec(ctx);
    } else {
        CHK_PRT_RETURN(ctx->dev_mask != DEV_DEV_ATTR,
                       hccn_err("invalid param, dev_mask:0x%x != 0x%x", ctx->dev_mask, DEV_DEV_ATTR),
                       TOOL_PUBLIC_ARGS_MISMATCH_CMD_ERR);
        return tool_set_link_exec(ctx, false);
    }
}

STATIC void tool_link_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_link_print_help(ctx, MODE_GET);
            tool_link_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-link", "get port link status, UP or DOWN");
            tool_print_npu_info(DEV_PORT_TYPE);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-link", "link up or shut down net device");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_NET_DEV);
            TOOL_PRINT_OPT("-up", "link up net device");

            TOOL_PRINT_CMD("-s", "-link", "link up or shut down net device");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_NET_DEV);
            TOOL_PRINT_OPT("-down", "shut down net device");
            break;
        default:
            break;
    }
}

STATIC int tool_get_netdev_interface_status(struct command_context *ctx, int *interface_status)
{
    unsigned int size = sizeof(int);
    struct dsmi_info info = {0};
    int ret = 0;

    ctx->result = (char *)interface_status;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_INTERFACE_STATUS, &info);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("dsmi get net_dev interface status failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    return 0;
}

STATIC int tool_set_link_for_exec_func(struct command_context *ctx, int interface_status, enum link_ops_type link_op)
{
    struct set_link_args link_args = {.type = link_op};
    struct command_context link_ctx = {0};

    if (interface_status == INTERFACE_STATUS_DOWN) {
        hccn_info("logic_id:%d, net_dev's original interface status is down, no need to execute link op:%d",
                  ctx->npu_info.logic_id, link_op);
        return 0;
    }

    (void)memcpy_s(&link_ctx, sizeof(struct command_context), ctx, sizeof(struct command_context));
    link_ctx.cmd_args = &link_args;

    return tool_set_link_exec(&link_ctx, true);
}

int tool_exec_with_link_status(struct command_context *ctx, int (*exec_func)(struct command_context *ctx),
                               bool double_check_flag)
{
    int interface_status, cmd_ret, ret;

    ret = tool_get_netdev_interface_status(ctx, &interface_status);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("tool get netdev interface failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    if (double_check_flag && interface_status != INTERFACE_STATUS_DOWN) {
        ret = tool_double_check("Running this command will trigger bond_dev link down and then recover the link.\n"
                                "Are you sure you want to continue?");
        CHK_PRT_RETURN(ret != 0, hccn_info("User aborts command"), ret);
    }

    ret = tool_set_link_for_exec_func(ctx, interface_status, SET_LINK_DOWN);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("set net_dev link down failed before executing func, ret:%d, logic_id:%d", ret,
                            ctx->npu_info.logic_id),
                   ret);

    cmd_ret = exec_func(ctx);
    if (cmd_ret != 0) {
        hccn_err("cmd:%s execute failed, dev_name:%s , cmd_ret:%d, logic_id:%d", ctx->match_cmd->name, ctx->dev_name,
                 cmd_ret, ctx->npu_info.logic_id);
    }

    ret = tool_set_link_for_exec_func(ctx, interface_status, SET_LINK_UP);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("set net_dev link up failed after executing func, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id),
        ret);

    return cmd_ret;
}

STATIC int tool_process_exec(struct command_context *ctx)
{
    unsigned int size = sizeof(bool);
    struct dsmi_info info = {0};
    bool dev_proc_exist = false;
    int ret = 0;

    ctx->result = (char *)&dev_proc_exist;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_PROCESS, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get process status failed, ret:%d", ret), ret);

    if (dev_proc_exist) {
        TOOL_PRINT_INFO("device hccp process: exist");
    } else {
        TOOL_PRINT_INFO("device hccp process: not exist");
    }

    return 0;
}

STATIC void tool_process_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_process_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-process", "get device process info");
            tool_print_npu_info(DEV_NPU_ATTR);
            break;
        default:
            break;
    }
}

STATIC int tool_bit_err_exec(struct command_context *ctx)
{
    unsigned int size = sizeof(struct bit_err_res);
    struct bit_err_res res = {0};
    struct dsmi_info info = {0};
    int ret = 0;

    ctx->result = (char *)&res;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_BIT_ERR, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get bit err failed, ret:%d", ret), ret);

    TOOL_PRINT_INFO("%s", "-------------------------- dl-bit_err --------------------------");
    TOOL_PRINT_INFO("port_id: 0x%x", ctx->port_id);
    TOOL_PRINT_INFO("st_fec_decoding_fail_num: 0x%llx", res.decoded_fail_block_num);
    TOOL_PRINT_INFO("st_fec_err_bit_num: 0x%llx", res.fec_err);

    return 0;
}

STATIC void tool_bit_err_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_bit_err_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-bit_err", "get bit_err");
            tool_print_npu_info(DEV_PORT_TYPE);
            break;
        default:
            break;
    }
}

STATIC int tool_show_list_exec(struct command_context *ctx)
{
    char show_list_out[OUTBUF_LEN_1024] = {0};
    unsigned int size = OUTBUF_LEN_1024 - 1;
    struct dsmi_info info = {0};
    int ret = 0;

    ctx->result = (char *)show_list_out;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_RES_LIST, &info);
    CHK_PRT_RETURN((ret != 0), hccn_err("get resource list failed, ret:%d", ret), ret);

    TOOL_PRINT_INFO("%s", ctx->result);

    return 0;
}

STATIC void tool_show_list_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_show_list_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-show_list", "get lists of queue resources");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_UDMA_DEV);
            break;
        default:
            break;
    }
}

STATIC int tool_version_exec(struct command_context *ctx)
{
    char firmware_ver[OUTBUF_LEN_64] = {0};
    unsigned int size = OUTBUF_LEN_64 - 1;
    char driver_ver[OUTBUF_LEN_64] = {0};
    struct dsmi_info info = {0};
    unsigned int ver_len = 0;
    int ret;

    ctx->result = (char *)firmware_ver;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_FIRMWARE_VERSION, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get firmware version failed, ret:%d", ret), ret);

    ret = dsmi_get_version(ctx->npu_info.logic_id, driver_ver, sizeof(driver_ver), &ver_len);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi_get_version failed, ret:%d", ret), ret);

    driver_ver[OUTBUF_LEN_64 - 1] = '\0';
    TOOL_PRINT_INFO("Network Driver Version:    %s", driver_ver);
    TOOL_PRINT_INFO("Network Firmware Version:  %s", firmware_ver);

    return ret;
}

STATIC void tool_version_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_version_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-version", "get driver and firmware version");
            tool_print_npu_info(DEV_NPU_ATTR);
            break;
        default:
            break;
    }
}

STATIC int tool_get_netdev_status(struct command_context *ctx)
{
    unsigned int size_out = OUTBUF_LEN_2048 - 1;
    char outbuf[OUTBUF_LEN_2048] = {0};
    struct dsmi_info info = {0};
    int ret;

    ctx->result = (char *)outbuf;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size_out);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_NETDEV_STATUS, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get netdev status failed, ret:%d name:%s", ret, ctx->dev_name), ret);

    while (size_out != 0 && (outbuf[size_out - 1] == '\n')) {
        outbuf[size_out - 1] = '\0';
        size_out = (unsigned int)strlen(outbuf);
    }

    TOOL_PRINT_INFO("%s", ctx->result);

    return 0;
}

STATIC int tool_status_exec(struct command_context *ctx)
{
    struct netdev_info ndev_info = {0};
    int ret, i;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    ret = tool_get_bond_slaves(ctx, &ndev_info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get bond slaves failed, ret:%d", ret), ret);

    TOOL_PRINT_INFO("%s", "Netdev status:");
    ret = tool_get_netdev_status(ctx);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get netdev status failed, ret:%d name:%s", ret, ctx->dev_name), ret);

    for (i = 0; i < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; i++) {
        (void)memset_s(ctx->dev_name, MAX_DEV_NAME_LEN, 0, MAX_DEV_NAME_LEN);
        ret = strncpy_s(ctx->dev_name, MAX_DEV_NAME_LEN, ndev_info.bond_info.slaves_ifname[i],
                        strlen(ndev_info.bond_info.slaves_ifname[i]));
        CHK_PRT_RETURN(ret != 0, hccn_err("copy slave_name to ctx->dev_name failed, ret:%d", ret), ret);

        ret = tool_get_netdev_status(ctx);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool get netdev status failed, ret:%d name:%s", ret, ctx->dev_name), ret);
    }

    return 0;
}

STATIC void tool_status_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_status_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-status", "get netdev status");
            tool_print_npu_info(DEV_NPU_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        default:
            break;
    }
}

STATIC int tool_port_info_exec(struct command_context *ctx)
{
    unsigned int size = OUTBUF_LEN_4096 - 1;
    struct dsmi_info info = {0};
    char *port_info = NULL;
    int ret = 0;

    port_info = (char *)calloc(OUTBUF_LEN_4096, sizeof(char));
    CHK_PRT_RETURN(port_info == NULL, hccn_err("port_info calloc failed."), -ENOMEM);

    ctx->result = (char *)port_info;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_PORT_INFO, &info);
    if (ret != 0) {
        hccn_err("dsmi_exe_get_info failed, ret:%d", ret);
        goto out;
    }

    TOOL_PRINT_INFO("%s", ctx->result);

out:
    free(port_info);
    port_info = NULL;
    return ret;
}

STATIC void tool_port_info_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_port_info_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-port_info", "get port info");
            tool_print_npu_info(DEV_PORT_TYPE);
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry eth_test_opts_param[] = {
    {"-offline", ARG_TYPE_NONE, parse_none, MODE_TEST, MODE_TEST},
};

STATIC void get_eth_test_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = eth_test_opts_param;
    *count = ARRAY_SIZE_COMPUTE(eth_test_opts_param);
}

STATIC void *alloc_eth_test_init(void)
{
    return calloc(1, sizeof(struct eth_test_cmd_args));
}

STATIC int eth_test_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct eth_test_cmd_args *eth_test_args = (struct eth_test_cmd_args *)args;

    if (strcmp(opt->name, "-offline") == 0) {
        eth_test_args->mode = ETH_TEST_MODE_OFFLINE;
    }

    return 0;
}

STATIC int tool_get_eth_test(struct command_context *ctx)
{
    unsigned int size_out = OUTBUF_LEN_2048 - 1;
    char outbuf[OUTBUF_LEN_2048] = {0};
    struct dsmi_info info = {0};
    int ret;

    ctx->result = (char *)outbuf;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size_out);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_ETH_TEST_INFO, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get eth test info failed, ret:%d name:%s", ret, ctx->dev_name), ret);

    size_out = (unsigned int)strlen(outbuf);
    while (size_out != 0 && (outbuf[size_out - 1] == '\n')) {
        outbuf[size_out - 1] = '\0';
        size_out = (unsigned int)strlen(outbuf);
    }

    TOOL_PRINT_INFO("[dev_name: %s] get eth_test result:", ctx->dev_name);
    TOOL_PRINT_INFO("%s", outbuf);

    return 0;
}

STATIC int tool_eth_test_exec(struct command_context *ctx)
{
    struct netdev_info ndev_info = {0};
    unsigned int err_count = 0;
    int i = 0, ret;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    ret = tool_double_check("Running this command may cause errors to other programs"
                            " that are using the ETH network port. Ensure that all other programs"
                            " are stopped before proceeding with this command.\n"
                            "Are you sure you want to continue?");
    CHK_PRT_RETURN(ret != 0, hccn_info("User aborts command"), ret);

    ret = tool_get_bond_slaves(ctx, &ndev_info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get bond slaves failed, ret:%d", ret), ret);

    for (i = 0; i < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; i++) {
        (void)memset_s(ctx->dev_name, MAX_DEV_NAME_LEN, 0, MAX_DEV_NAME_LEN);
        ret = strncpy_s(ctx->dev_name, MAX_DEV_NAME_LEN, ndev_info.bond_info.slaves_ifname[i],
                        strlen(ndev_info.bond_info.slaves_ifname[i]));
        CHK_PRT_RETURN(ret != 0, hccn_err("copy slave_name to ctx->dev_name failed, ret:%d", ret), ret);

        ret = tool_get_eth_test(ctx);
        if (ret != 0) {
            err_count++;
            hccn_err("tool get eth_test result failed, ret:%d name:%s err_count:%u", ret, ctx->dev_name, err_count);
            TOOL_PRINT_ERR("[dev_name:%s] get eth_test result failed!", ctx->dev_name);
        }
    }

    return (err_count == DEV_INFO_DEFAULT_BOND_SLAVE_NUM) ? ret : 0;
}

STATIC void tool_eth_test_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_eth_test_print_help(ctx, MODE_TEST);
            break;
        case MODE_TEST:
            TOOL_PRINT_CMD("-t", "-eth_test", "get the self-check result of eth ports");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-offline", "mode, now only offline is supported");
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry bond_attr_opts_param[] = {
    {"-lacp_rate", ARG_TYPE_INT, parse_int, MODE_SET, MODE_SET, true, .range.int_range = {0, 1}},
};

STATIC void get_bond_attr_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = bond_attr_opts_param;
    *count = ARRAY_SIZE_COMPUTE(bond_attr_opts_param);
}

STATIC void *alloc_bond_attr_args_init(void)
{
    return calloc(1, sizeof(struct bond_attr_cmd_args));
}

STATIC int bond_attr_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct bond_attr_cmd_args *bond_attr_args = (struct bond_attr_cmd_args *)args;

    if (strcmp(opt->name, "-lacp_rate") == 0) {
        bond_attr_args->lacp_rate = type_val->int_val;
    }

    return 0;
}

STATIC int tool_bond_attr_get(struct command_context *ctx)
{
    unsigned int size = MAX_TRANS_DATA_LEN - 1;
    struct dsmi_info info = {0};
    int ret = 0;

    ctx->result = (char *)calloc(MAX_TRANS_DATA_LEN, sizeof(char));
    CHK_PRT_RETURN(ctx->result == NULL, hccn_err("bond attr result buffer calloc failed"), -ENOMEM);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_BOND_ATTR, &info);
    if (ret != 0) {
        hccn_err("dsmi get bond attr failed, ret:%d", ret);
        goto out;
    }

    TOOL_PRINT_INFO("bond attr:");
    TOOL_PRINT_INFO("%s", ctx->result);
out:
    FREE_BUF(ctx->result);
    return ret;
}

int tool_bond_attr_set(struct command_context *ctx)
{
    struct bond_attr_cmd_args *bond_attr_args = (struct bond_attr_cmd_args *)ctx->cmd_args;
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    CHK_PRT_RETURN(bond_attr_args == NULL, hccn_err("bond_attr_args is NULL"), TOOL_PRIVATE_ARGS_NUM_ERR);
    CHK_PRT_RETURN(hccn_check_usr_identify() != 0,
                   hccn_err("permission denied: only root user is allowed, logic_id:%d", ctx->npu_info.logic_id),
                   TOOL_PUBLIC_ROOT_UNSUPPORT);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)bond_attr_args,
                   sizeof(struct bond_attr_cmd_args), NULL, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_BOND_ATTR, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi set bond attr failed, ret:%d", ret), ret);
    return 0;
}

STATIC int tool_update_bond_attr_cfg(struct command_context *ctx)
{
    struct bond_attr_cmd_args *bond_attr_args = (struct bond_attr_cmd_args *)ctx->cmd_args;
    int ret;

    ret = tool_write_conf(ctx->slot_id, "bond_attr", (bond_attr_args->lacp_rate == 0) ? "0" : "1");
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("write cfg error, ret:%d slot_id:%d logic_id:%d)", ret, ctx->slot_id, ctx->npu_info.logic_id),
        TOOL_WRITE_CONF_ERR);

    return 0;
}

STATIC int tool_bond_attr_exec(struct command_context *ctx)
{
    int ret;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    if (ctx->mode == MODE_GET) {
        return tool_bond_attr_get(ctx);
    } else {
        ret = tool_exec_with_link_status(ctx, tool_bond_attr_set, true);
        if (ret == 0) {
            ret = tool_update_bond_attr_cfg(ctx);
            CHK_PRT_RETURN(ret != 0,
                           hccn_err("update bond attr cfg failed, ret:%d, slot_id:%d, logic_id:%d", ret, ctx->slot_id,
                                    ctx->npu_info.logic_id),
                           ret);
        }
        return ret;
    }
}

STATIC void tool_bond_attr_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_bond_attr_print_help(ctx, MODE_GET);
            tool_bond_attr_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-bond_attr", "get bond attr");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-bond_attr", "set bond attr, only allow set when bond device is down");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-lacp_rate <lacp_rate_mode>", "specify lacp rate mode, 0:slow, 1:fast");
            break;
        default:
            break;
    }
}

STATIC int tool_get_lldp_eth_info(struct command_context *ctx)
{
    unsigned int size_out = MAX_CMD_PAYLOAD_LEN - 1;
    char lldp_info[MAX_CMD_PAYLOAD_LEN] = {0};
    struct dsmi_info info = {0};
    int ret = 0;

    ctx->result = lldp_info;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size_out);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_LLDP_NEIGHBOR_INFO, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get lldp info failed, ret:%d", ret), ret);

    TOOL_PRINT_INFO("[dev_name:%s] LLDP neighbor info:", ctx->dev_name);
    TOOL_PRINT_INFO("%s", ctx->result);
    return 0;
}

STATIC int tool_lldp_exec(struct command_context *ctx)
{
    struct netdev_info ndev_info = {0};
    unsigned int err_count = 0;
    int ret = 0;
    int i;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    ret = tool_get_bond_slaves(ctx, &ndev_info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get bond slaves failed, ret:%d", ret), ret);

    for (i = 0; i < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; i++) {
        (void)memset_s(ctx->dev_name, MAX_DEV_NAME_LEN, 0, MAX_DEV_NAME_LEN);
        ret = strncpy_s(ctx->dev_name, MAX_DEV_NAME_LEN, ndev_info.bond_info.slaves_ifname[i],
                        strlen(ndev_info.bond_info.slaves_ifname[i]));
        CHK_PRT_RETURN(ret != 0, hccn_err("copy slave_name to ctx->dev_name failed, ret:%d", ret), ret);

        ret = tool_get_lldp_eth_info(ctx);
        if (ret != 0) {
            err_count++;
            hccn_err("tool get lldp info failed, ret:%d dev_name:%s err_count:%u", ret, ctx->dev_name, err_count);
            TOOL_PRINT_ERR("[dev_name:%s] failed to get the lldp result!", ctx->dev_name);
        }
    }

    return (err_count == DEV_INFO_DEFAULT_BOND_SLAVE_NUM) ? ret : 0;
}

STATIC void tool_lldp_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_lldp_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-lldp", "get lldp cfg");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry stat_opts_param[] = {
    {"-clear", ARG_TYPE_NONE, parse_none, MODE_SET, MODE_SET},
};

STATIC void get_stat_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = stat_opts_param;
    *count = ARRAY_SIZE_COMPUTE(stat_opts_param);
}

STATIC void *alloc_stat_args_init(void)
{
    return calloc(1, sizeof(struct stat_cmd_args));
}

STATIC int stat_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct stat_cmd_args *stat_args = (struct stat_cmd_args *)args;

    if (strcmp(opt->name, "-clear") == 0) {
        stat_args->mode = STAT_SET_MODE_CLEAR;
    }

    return 0;
}

STATIC void tool_display_stat_info(const struct ds_port_stat_info *stat_info)
{
    TOOL_PRINT_INFO("packet statistics:");
    TOOL_PRINT_INFO("port_id: %u", stat_info->port_id);
    TOOL_PRINT_INFO("is_uboe_port: %u", stat_info->is_uboe_port);
    TOOL_PRINT_INFO("ub_ipv4_pkt_cnt_rx: %llu", stat_info->ub_ipv4_pkt_cnt_rx);
    TOOL_PRINT_INFO("ub_ipv6_pkt_cnt_rx: %llu", stat_info->ub_ipv6_pkt_cnt_rx);
    TOOL_PRINT_INFO("unic_ipv4_pkt_cnt_rx: %llu", stat_info->unic_ipv4_pkt_cnt_rx);
    TOOL_PRINT_INFO("unic_ipv6_pkt_cnt_rx: %llu", stat_info->unic_ipv6_pkt_cnt_rx);
    TOOL_PRINT_INFO("ub_compact_pkt_cnt_rx: %llu", stat_info->ub_compact_pkt_cnt_rx);
    TOOL_PRINT_INFO("ub_umoc_ctph_cnt_rx: %llu", stat_info->ub_umoc_ctph_cnt_rx);
    TOOL_PRINT_INFO("ub_umoc_ntph_cnt_rx: %llu", stat_info->ub_umoc_ntph_cnt_rx);
    TOOL_PRINT_INFO("ub_mem_pkt_cnt_rx: %llu", stat_info->ub_mem_pkt_cnt_rx);
    TOOL_PRINT_INFO("unknown_pkt_cnt_rx: %llu", stat_info->unknown_pkt_cnt_rx);
    TOOL_PRINT_INFO("drop_ind_cnt_rx: %llu", stat_info->drop_ind_cnt_rx);
    TOOL_PRINT_INFO("err_ind_cnt_rx: %llu", stat_info->err_ind_cnt_rx);
    TOOL_PRINT_INFO("to_host_pkt_cnt_rx: %llu", stat_info->to_host_pkt_cnt_rx);
    TOOL_PRINT_INFO("to_imp_pkt_cnt_rx: %llu", stat_info->to_imp_pkt_cnt_rx);
    TOOL_PRINT_INFO("to_mar_pkt_cnt_rx: %llu", stat_info->to_mar_pkt_cnt_rx);
    TOOL_PRINT_INFO("to_link_pkt_cnt_rx: %llu", stat_info->to_link_pkt_cnt_rx);
    TOOL_PRINT_INFO("to_noc_pkt_cnt_rx: %llu", stat_info->to_noc_pkt_cnt_rx);
    TOOL_PRINT_INFO("route_err_cnt_rx: %llu", stat_info->route_err_cnt_rx);
    TOOL_PRINT_INFO("out_err_cnt_rx: %llu", stat_info->out_err_cnt_rx);
    TOOL_PRINT_INFO("length_err_cnt_rx: %llu", stat_info->length_err_cnt_rx);
    TOOL_PRINT_INFO("rx_busi_flit_num: %llu", stat_info->rx_busi_flit_num);
    TOOL_PRINT_INFO("rx_send_ack_flit: %llu", stat_info->rx_send_ack_flit);
    TOOL_PRINT_INFO("ub_ipv4_pkt_cnt_tx: %llu", stat_info->ub_ipv4_pkt_cnt_tx);
    TOOL_PRINT_INFO("ub_ipv6_pkt_cnt_tx: %llu", stat_info->ub_ipv6_pkt_cnt_tx);
    TOOL_PRINT_INFO("unic_ipv4_pkt_cnt_tx: %llu", stat_info->unic_ipv4_pkt_cnt_tx);
    TOOL_PRINT_INFO("unic_ipv6_pkt_cnt_tx: %llu", stat_info->unic_ipv6_pkt_cnt_tx);
    TOOL_PRINT_INFO("ub_compact_pkt_cnt_tx: %llu", stat_info->ub_compact_pkt_cnt_tx);
    TOOL_PRINT_INFO("ub_umoc_ctph_cnt_tx: %llu", stat_info->ub_umoc_ctph_cnt_tx);
    TOOL_PRINT_INFO("ub_umoc_ntph_cnt_tx: %llu", stat_info->ub_umoc_ntph_cnt_tx);
    TOOL_PRINT_INFO("ub_mem_pkt_cnt_tx: %llu", stat_info->ub_mem_pkt_cnt_tx);
    TOOL_PRINT_INFO("unknown_pkt_cnt_tx: %llu", stat_info->unknown_pkt_cnt_tx);
    TOOL_PRINT_INFO("drop_ind_cnt_tx: %llu", stat_info->drop_ind_cnt_tx);
    TOOL_PRINT_INFO("err_ind_cnt_tx: %llu", stat_info->err_ind_cnt_tx);
    TOOL_PRINT_INFO("lpbk_ind_cnt_tx: %llu", stat_info->lpbk_ind_cnt_tx);
    TOOL_PRINT_INFO("out_err_cnt_tx: %llu", stat_info->out_err_cnt_tx);
    TOOL_PRINT_INFO("length_err_cnt_tx: %llu", stat_info->length_err_cnt_tx);
    TOOL_PRINT_INFO("tx_busi_flit_num: %llu", stat_info->tx_busi_flit_num);
    TOOL_PRINT_INFO("tx_recv_ack_flit: %llu", stat_info->tx_recv_ack_flit);
    TOOL_PRINT_INFO("retry_req_sum: %llu", stat_info->retry_req_sum);
    TOOL_PRINT_INFO("retry_ack_sum: %llu", stat_info->retry_ack_sum);
    TOOL_PRINT_INFO("crc_error_sum: %llu", stat_info->crc_error_sum);

    if (stat_info->is_uboe_port == PROTOCOL_TYPR_UBOE) {
        TOOL_PRINT_INFO("core_mib_rxpausepkts: %llu", stat_info->uboe.core_mib_rxpausepkts);
        TOOL_PRINT_INFO("core_mib_txpausepkts: %llu", stat_info->uboe.core_mib_txpausepkts);
        TOOL_PRINT_INFO("core_mib_rxpfcpkts: %llu", stat_info->uboe.core_mib_rxpfcpkts);
        TOOL_PRINT_INFO("core_mib_txpfcpkts: %llu", stat_info->uboe.core_mib_txpfcpkts);
        TOOL_PRINT_INFO("core_mib_rxbadpkts: %llu", stat_info->uboe.core_mib_rxbadpkts);
        TOOL_PRINT_INFO("core_mib_txbadpkts: %llu", stat_info->uboe.core_mib_txbadpkts);
        TOOL_PRINT_INFO("core_mib_rxbadoctets: %llu", stat_info->uboe.core_mib_rxbadoctets);
        TOOL_PRINT_INFO("core_mib_txbadoctets: %llu", stat_info->uboe.core_mib_txbadoctets);
    }
}

STATIC int tool_stat_exec(struct command_context *ctx)
{
    unsigned int size = sizeof(struct ds_port_stat_info);
    struct ds_port_stat_info stat_info = {0};
    struct dsmi_info info = {0};
    int ret = 0;

    ctx->result = (char *)&stat_info;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    if (ctx->mode == MODE_GET) {
        ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_PACKET_STATISTICS, &info);
        CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get stat failed, ret:%d", ret), ret);

        tool_display_stat_info(&stat_info);
    } else {
        ret = hccn_check_usr_identify();
        CHK_PRT_RETURN(
            ret != 0,
            hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
            TOOL_PUBLIC_ROOT_UNSUPPORT);

        ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_CLEAR_PACKET_STATISTICS, &info);
        CHK_PRT_RETURN(ret != 0, hccn_err("dsmi clear stat failed, ret:%d", ret), ret);
    }

    return 0;
}

STATIC void tool_stat_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_stat_print_help(ctx, MODE_GET);
            tool_stat_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-stat", "get packet statistics");
            tool_print_npu_info(DEV_PORT_TYPE);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-stat", "set packet statistics");
            tool_print_npu_info(DEV_PORT_TYPE);
            TOOL_PRINT_OPT("-clear", "clear packet statistics");
            break;
        default:
            break;
    }
}

struct sub_option_entry mtu_opts_param[] = {
    {"-size", ARG_TYPE_INT, parse_int, MODE_SET, MODE_SET, true, .range.int_range = {68, 4454}},
};

STATIC void get_mtu_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = mtu_opts_param;
    *count = ARRAY_SIZE_COMPUTE(mtu_opts_param);
}

STATIC void *alloc_mtu_args_init(void)
{
    return calloc(1, sizeof(struct mtu_cmd_args));
}

STATIC int set_mtu_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct mtu_cmd_args *mtu_args = (struct mtu_cmd_args *)args;

    if (strcmp(opt->name, "-size") == 0) {
        mtu_args->mtu_size = type_val->int_val;
    }

    return 0;
}

STATIC struct sub_option_entry gateway_opts_param[] = {
    {"-address", ARG_TYPE_IP, parse_ip, MODE_SET, MODE_SET},
};

STATIC void get_gateway_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = gateway_opts_param;
    *count = ARRAY_SIZE_COMPUTE(gateway_opts_param);
}

STATIC void *alloc_gateway_args_init(void)
{
    // Use IPv4 address by default
    struct gateway_cmd_args *gw_args = (struct gateway_cmd_args *)calloc(1, sizeof(struct gateway_cmd_args));
    return (void *)gw_args;
}

STATIC int gateway_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct gateway_cmd_args *gw_args = (struct gateway_cmd_args *)args;
    int ret = 0;

    if (strcmp(opt->name, "-address") == 0) {
        ret = memcpy_s(&gw_args->gw_addr, sizeof(gw_args->gw_addr), &type_val->ip, sizeof(type_val->ip));
    } else {
        return -EINVAL;
    }

    return ret;
}

STATIC int tool_get_mtu_exec(struct command_context *ctx)
{
    unsigned int size = OUTBUF_LEN_16 - 1;
    char mtu_size[OUTBUF_LEN_16] = {0};
    struct dsmi_info info = {0};
    char *newline_pos = NULL;
    int ret = 0;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, mtu_size, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_MTU_SIZE, &info);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("get mtu size failed, logic_id:%d dev_name:%s ret:%d", ctx->npu_info.logic_id, ctx->dev_name, ret),
        ret);

    newline_pos = strchr(mtu_size, '\n');
    if (newline_pos != NULL) {
        *newline_pos = '\0';
    }

    TOOL_PRINT_INFO("mtu:%s", mtu_size);

    return 0;
}

int tool_set_mtu_exec(struct command_context *ctx)
{
    struct mtu_cmd_args *mtu_args = (struct mtu_cmd_args *)ctx->cmd_args;
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    LOG_PROMPT(ctx, "start to set mtu:%d, logic_id:%d", mtu_args->mtu_size, ctx->npu_info.logic_id);

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d dev_name:%s", ret,
                            ctx->npu_info.logic_id, ctx->dev_name),
                   TOOL_PUBLIC_ROOT_UNSUPPORT);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)mtu_args, sizeof(struct mtu_cmd_args),
                   NULL, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_MTU_SIZE, &info);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("set mtu failed, logic_id:%d dev_name:%s ret:%d", ctx->npu_info.logic_id, ctx->dev_name, ret), ret);

    return 0;
}

STATIC int tool_mtu_exec(struct command_context *ctx)
{
    struct mtu_cmd_args *mtu_args = (struct mtu_cmd_args *)ctx->cmd_args;
    char mtu_val[MTU_VAL_STRING_LEN] = {0};
    int ret = 0;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    if (ctx->mode == MODE_GET) {
        return tool_get_mtu_exec(ctx);
    } else {
        ret = tool_exec_with_link_status(ctx, tool_set_mtu_exec, true);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool_exec_with_link_status failed, ret:%d", ret), ret);

        ret = snprintf_s(mtu_val, MTU_VAL_STRING_LEN, MTU_VAL_STRING_LEN - 1, "%d", mtu_args->mtu_size);
        CHK_PRT_RETURN(ret <= 0, hccn_err("snprintf_s mtu val failed, ret:%d", ret), -EIO);

        ret = tool_write_conf(ctx->slot_id, "mtu", mtu_val);
        CHK_PRT_RETURN(
            ret != 0,
            hccn_err("write cfg error, ret:%d slot_id:%d logic_id:%d)", ret, ctx->slot_id, ctx->npu_info.logic_id),
            TOOL_WRITE_CONF_ERR);

        return 0;
    }
}

STATIC void tool_mtu_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_mtu_print_help(ctx, MODE_GET);
            tool_mtu_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-mtu", "get netdev mtu size");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-mtu", "set netdev mtu size");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-size <mtu_size>", "mtu size, range [68, 4454] Bytes");
            break;
        default:
            break;
    }
}

STATIC int tool_get_gateway_exec(struct command_context *ctx)
{
    char gw_str[MAX_IP_VAL_LEN] = {0};
    struct dsmi_info info = {0};
    unsigned int gw_addr;
    unsigned int size;
    int ret = 0;

    size = sizeof(unsigned int);
    ctx->result = (char *)&gw_addr;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_DEFAULT_GATEWAY, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get default gateway info failed, ret:%d", ret), ret);

    ret = tool_ipv4_int_to_str(gw_addr, gw_str, MAX_IP_VAL_LEN);
    CHK_PRT_RETURN(ret != 0, hccn_err("ipv4_int_to_str failed! ret[%d] gateway[0x%x]", ret, gw_addr), ret);

    TOOL_PRINT_INFO("default gateway:%s", gw_str);

    return ret;
}

STATIC int tool_del_gateway_exec(struct command_context *ctx, unsigned int gw_ip)
{
    unsigned int gw_addr = gw_ip;
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret;

    DSMI_FILL_INFO(info, (char *)&gw_addr, sizeof(unsigned int), NULL, 0, NULL, &size);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_DEL_DEFAULT_GATEWAY, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi del default gateway failed, ret:%d", ret), ret);

    return ret;
}

int tool_inet_pton_ipv4_address(const char *str, unsigned int *ip)
{
    struct in_addr addr;
    int ret;

    TOOL_CHECK_PTR_VALID_RETURN_VAL(str, -EINVAL);
    TOOL_CHECK_PTR_VALID_RETURN_VAL(ip, -EINVAL);

    ret = inet_pton(AF_INET, str, &addr);
    if (ret <= 0) {
        hccn_err("\"%s\" is an invalid IPv4 address! ret:%d, errno[%d]", str, ret, errno);
        return -1;
    }

    *ip = addr.s_addr;
    return 0;
}

int tool_check_ip_net_segment(const char *sip, const char *dip, const char *netmask)
{
    unsigned int src_ip;
    unsigned int dst_ip;
    unsigned int mask;
    int ret;

    hccn_info("check net segment! sip[%s] dip[%s] netmask[%s]", sip, dip, netmask);
    ret = tool_inet_pton_ipv4_address(sip, &src_ip);
    if (ret == -1) {
        TOOL_PRINT_ERR("The IP address is invalid!");
        return ret;
    } else if (ret) {
        hccn_err("check and inet pton source ip failed, ret:%d", ret);
        return ret;
    }

    ret = tool_inet_pton_ipv4_address(dip, &dst_ip);
    if (ret == -1) {
        TOOL_PRINT_ERR("The IP address is invalid!");
        return ret;
    } else if (ret) {
        hccn_err("check and inet pton dest ip failed, ret:%d", ret);
        return ret;
    }

    ret = tool_inet_pton_ipv4_address(netmask, &mask);
    if (ret == -1) {
        TOOL_PRINT_ERR("The netmask is invalid!");
        return ret;
    } else if (ret) {
        hccn_err("check and inet pton netmask failed, ret:%d", ret);
        return ret;
    }

    if ((src_ip & mask) != (dst_ip & mask)) {
        hccn_err("src_ip[0x%x] and dst_ip[0x%x] are not the same net segment!", src_ip, dst_ip);
        return DEV_EXE_DIFF_SEGMT_GTWY_ERR;
    } else {
        return 0;
    }
}

STATIC int tool_ip_get_gateway_conf(struct command_context *ctx, unsigned int *gateway)
{
    char gw[MAX_IP_VAL_LEN] = {0};
    unsigned int gw_ip = 0;
    int ret;

    ret = tool_read_conf(ctx->slot_id, "gateway", gw, sizeof(gw));
    if (ret) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND) {
            return FILE_OPT_SYS_RD_FILE_NOT_FOUND;
        }
        hccn_err("tool_read_conf failed! slot_id:%d logic_id:%d ret:%d len:%zu", ctx->slot_id, ctx->npu_info.logic_id,
                 ret, sizeof(gw));
        return ret;
    }

    ret = tool_inet_pton_ipv4_address(gw, &gw_ip);
    if (ret) {
        TOOL_PRINT_ERR("The gateway is invalid!");
        hccn_err("tool_inet_pton_ipv4_address failed! ret:%d logic_id:%d gw:%s gw_ip:0x%x", ret, ctx->npu_info.logic_id,
                 gw, gw_ip);
        return ret;
    }

    *gateway = gw_ip;

    return 0;
}

STATIC int tool_ip_sync_handle_cfg_conf(bool has_conf, int slot_id, const unsigned int *gateway, unsigned int gw_conf)
{
    int ret = 0;

    if (has_conf) {
        // if hw cfg and conf diff, del old and add hw cfg
        hccn_warn("hw gateway[0x%x], diff conf gw[0x%x] slot_id[%d]", *gateway, gw_conf, slot_id);
        ret = tool_del_conf(slot_id, "gateway");
        if (ret) {
            hccn_err("del cfg error! ret[%d] slot_id[%d]", ret, slot_id);
            return ret;
        }
    } else {
        // hw has something, and conf nothing, LOG ERR and continue
        hccn_err("hw gateway[0x%x], conf nothing", *gateway);
    }

    return ret;
}

STATIC int tool_ip_sync_hw_gateway_to_conf(struct command_context *ctx, unsigned int *gateway)
{
    char gw_str[MAX_IP_VAL_LEN] = {0};
    struct dsmi_info info = {0};
    unsigned int gw_conf = 0;
    unsigned int gw_addr;
    bool has_conf = true;
    unsigned int size;
    int ret;

    *gateway = INVALID_GW;

    ret = tool_ip_get_gateway_conf(ctx, &gw_conf);
    if (ret != 0) {
        if (ret != FILE_OPT_SYS_RD_FILE_NOT_FOUND) {
            hccn_err("tool get gw conf failed[%d] logic_id[%d]", ret, ctx->npu_info.logic_id);
            return ret;
        }
        has_conf = false;
    }

    size = sizeof(unsigned int);
    ctx->result = (char *)&gw_addr;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);
    // first, sync conf and config. then check if need cfg new gateway
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_DEFAULT_GATEWAY, &info);
    // hardware without default gateway
    if (ret == DEV_EXE_GATEWAY_NOT_PRESET_ERR) {
        // if conf has gateway, delete it
        if (has_conf) {
            ret = tool_del_conf(ctx->slot_id, "gateway");
            if (ret) {
                hccn_err("del cfg error! ret:%d slot_id:%d logic_id:%d", ret, ctx->slot_id, ctx->npu_info.logic_id);
                return ret;
            }
        }
        // return ok, because without hw gw entry
        return 0;
    }

    ret = tool_ipv4_int_to_str(gw_addr, gw_str, MAX_IP_VAL_LEN);
    CHK_PRT_RETURN(ret != 0, hccn_err("ipv4_int_to_str failed! ret[%d] gateway[0x%x]", ret, gw_addr), ret);

    *gateway = gw_addr;

    if (has_conf && (gw_addr == gw_conf)) {
        return 0;
    }

    ret = tool_ip_sync_handle_cfg_conf(has_conf, ctx->slot_id, gateway, gw_conf);
    if (ret) {
        hccn_err("handle cfg and conf failed!, ret[%d]", ret);
        return ret;
    }

    ret = tool_write_conf(ctx->slot_id, "gateway", gw_str);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("write cfg error, ret:%d slot_id:%d logic_id:%d)", ret, ctx->slot_id, ctx->npu_info.logic_id), ret);

    return ret;
}

int tool_ip_set_gateway(struct command_context *ctx)
{
    struct gateway_cmd_args *gw_args = (struct gateway_cmd_args *)ctx->cmd_args;
    int logic_id = ctx->npu_info.logic_id;
    struct dsmi_info info = {0};
    unsigned int gw_ip = 0;
    unsigned int size = 0;
    bool hw_has_gw = true;
    int ret;

    LOG_PROMPT(ctx, "start to set default gateway addr:%s, logic_id:%d", gw_args->gw_addr.str_ip,
               ctx->npu_info.logic_id);

    ret = tool_ip_sync_hw_gateway_to_conf(ctx, &gw_ip);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("sync hw gateway to conf failed logic_id[%d] slot_id[%d] ret[%d]", logic_id, ctx->slot_id, ret), ret);

    if (gw_ip == INVALID_GW) {
        hw_has_gw = false;
    }

    if (hw_has_gw && gw_ip == gw_args->gw_addr.ip_addr) {
        hccn_info("cfg logic_id[%d] gw[0x%x] exists", logic_id, gw_args->gw_addr.ip_addr);
        return 0;
    }

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)ctx->cmd_args,
                   sizeof(struct gateway_cmd_args), NULL, &size);

    ret = dsmi_exe_get_info(logic_id, DS_SET_DEFAULT_GATEWAY, &info);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("set default gateway failed, logic_id:%d dev_name:%s ret:%d", ctx->npu_info.logic_id,
                            ctx->dev_name, ret),
                   ret);

    if (hw_has_gw) {
        ret = tool_del_gateway_exec(ctx, gw_ip);
        if (ret) {
            return ret;
        }
    }

    return ret;
}

int tool_set_gateway_exec(struct command_context *ctx)
{
    struct gateway_cmd_args *gw_args = (struct gateway_cmd_args *)ctx->cmd_args;
    char *gateway = gw_args->gw_addr.str_ip;
    char mask[MAX_IP_VAL_LEN] = {0};
    char ip[MAX_IP_VAL_LEN] = {0};
    int slot_id = ctx->slot_id;
    int ret = 0;

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    ret = tool_read_conf(slot_id, "address", ip, sizeof(ip));
    CHK_PRT_RETURN(
        ret != 0, hccn_err("read address failed! ret:%d slot_id:%d logic_id:%d:", ret, slot_id, ctx->npu_info.logic_id),
        ret);

    ret = tool_read_conf(slot_id, "netmask", mask, sizeof(mask));
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("read netmask failed! ret:%d slot_id:%d logic_id:%d", ret, slot_id, ctx->npu_info.logic_id),
                   ret);

    ret = tool_check_ip_net_segment(ip, gateway, mask);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("not the same net segment! ret:%d logic_id:%d ip:%s gateway:%s mask:%s", ret,
                            ctx->npu_info.logic_id, ip, gateway, mask),
                   ret);

    ret = tool_ip_set_gateway(ctx);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("tool_ip_set_gateway failed! ret:%d logic_id:%d gateway:%s", ret, ctx->npu_info.logic_id, gateway),
        ret);

    if (!tool_check_conf_udhcpc_enable(slot_id)) {
        ret = tool_write_conf(slot_id, "gateway", gateway);
        CHK_PRT_RETURN(
            ret != 0,
            hccn_err("write cfg error, ret:%d slot_id:%d logic_id:%d)", ret, ctx->slot_id, ctx->npu_info.logic_id),
            TOOL_WRITE_CONF_ERR);
    }

    return ret;
}

STATIC void tool_gateway_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_gateway_print_help(ctx, MODE_GET);
            tool_gateway_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-gateway", "get gateway address");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-gateway", "set gateway address");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-address <gateway>", "set gateway, such as x.x.x.x");
            break;
        default:
            break;
    }
}

STATIC int tool_gateway_exec(struct command_context *ctx)
{
    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    if (ctx->mode == MODE_GET) {
        return tool_get_gateway_exec(ctx);
    } else {
        return tool_set_gateway_exec(ctx);
    }
}

STATIC struct sub_option_entry lb_opts_param[] = {
    {"-mode", ARG_TYPE_STR, parse_str, MODE_SET, 0},
    {"-clear", ARG_TYPE_NONE, parse_none, MODE_SET, 0},
};

STATIC void get_lb_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = lb_opts_param;
    *count = ARRAY_SIZE_COMPUTE(lb_opts_param);
}

STATIC void *alloc_lb_args_init(void)
{
    return calloc(1, sizeof(struct lb_cmd_args));
}

STATIC int lb_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct lb_cmd_args *lb_args = (struct lb_cmd_args *)args;
    int ret = 0;

    if (strcmp(opt->name, "-clear") == 0 && lb_args->mode == LOOPBACK_SET_NONE) {
        lb_args->mode = LOOPBACK_SET_CLEAR;
    } else if (strcmp(opt->name, "-mode") == 0 && lb_args->mode == LOOPBACK_SET_NONE) {
        lb_args->mode = LOOPBACK_SET_ENABLE;
        ret = strncpy_s(lb_args->lb_mode_name, MAX_LOOPBACK_MODE_NAME - 1, type_val->str_val,
                        strlen(type_val->str_val));
        CHK_PRT_RETURN(ret != 0,
                       hccn_err("strncpy_s lb_mode_name failed! ret:%d, str_len:%zu", ret, strlen(type_val->str_val)),
                       ret);

        CHK_PRT_RETURN(strcmp(lb_args->lb_mode_name, RXMAC2TXMAC) != 0 &&
                           strcmp(lb_args->lb_mode_name, TXMAC2RXMAC) != 0 &&
                           strcmp(lb_args->lb_mode_name, TXPCS2RXPCS) != 0,
                       TOOL_PRINT_ERR("Option -mode <loopback_mode> supports one of the following loopback modes: "
                                      "%s, %s, %s.",
                                      RXMAC2TXMAC, TXMAC2RXMAC, TXPCS2RXPCS),
                       TOOL_PRIVATE_ARGS_VALUE_ERR);
    } else {
        TOOL_PRINT_ERR("Option -mode and -clear conflict with each other.");
        return TOOL_PRIVATE_ARGS_CONFLICT_ERR;
    }

    return 0;
}

STATIC int tool_get_lb_exec(struct command_context *ctx)
{
    unsigned int size = sizeof(struct lb_state);
    struct lb_state lb_results = {0};
    struct dsmi_info info = {0};
    int ret = 0;

    ctx->result = (char *)&lb_results;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_LOOPBACK_STATE, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get lb state failed, ret:%d", ret), ret);

    TOOL_PRINT_INFO("%s: %d", RXMAC2TXMAC, lb_results.is_rxmac2txmac_en);
    TOOL_PRINT_INFO("%s: %d", TXMAC2RXMAC, lb_results.is_txmac2rxmac_en);
    TOOL_PRINT_INFO("%s: %d", TXPCS2RXPCS, lb_results.is_txpcs2rxpcs_en);

    return 0;
}

STATIC int tool_set_lb_exec(struct command_context *ctx)
{
    struct lb_cmd_args *lb_args = (struct lb_cmd_args *)ctx->cmd_args;
    enum ds_cmd_opcode cmd_opcode;
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    CHK_PRT_RETURN(lb_args == NULL, TOOL_PRINT_ERR("Option -clear or -mode <loopback_mode> is required but missing."),
                   TOOL_PRIVATE_ARGS_VALUE_ERR);

    cmd_opcode = (lb_args->mode == LOOPBACK_SET_ENABLE) ? DS_SET_LOOPBACK_STATE : DS_CLEAR_LOOPBACK_STATE;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)ctx->cmd_args, sizeof(struct lb_cmd_args),
                   NULL, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, cmd_opcode, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi set lb state failed, ret:%d", ret), ret);

    return 0;
}

STATIC int tool_lb_exec(struct command_context *ctx)
{
    int ret;

    if (ctx->mode == MODE_GET) {
        ret = tool_get_lb_exec(ctx);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool_get_lb_exec failed, ret:%d", ret), ret);
    } else {
        ret = hccn_check_usr_identify();
        CHK_PRT_RETURN(
            ret != 0,
            hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
            TOOL_PUBLIC_ROOT_UNSUPPORT);

        ret = tool_set_lb_exec(ctx);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool_set_lb_exec failed, ret:%d", ret), ret);
    }

    return 0;
}

STATIC void tool_lb_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_lb_print_help(ctx, MODE_GET);
            tool_lb_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-loopback", "get loopback enable state");
            tool_print_npu_info(DEV_PORT_TYPE);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-loopback", "set loopback state");
            tool_print_npu_info(DEV_PORT_TYPE);
            TOOL_PRINT_OPT("-mode <loopback_mode>", "enable loopback mode: rxmac2txmac, txmac2rxmac, txpcs2rxpcs");

            TOOL_PRINT_CMD("-s", "-loopback", "set loopback state");
            tool_print_npu_info(DEV_PORT_TYPE);
            TOOL_PRINT_OPT("-clear", "clear loopback state");
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry arp_opts_param[] = {
    {"-add", ARG_TYPE_NONE, parse_none, MODE_SET, 0},
    {"-del", ARG_TYPE_NONE, parse_none, MODE_SET, 0},
    {"-ip", ARG_TYPE_IP, parse_ip, MODE_SET, MODE_SET},
    {"-mac", ARG_TYPE_MAC, parse_mac, MODE_SET, 0},
};

STATIC void get_arp_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = arp_opts_param;
    *count = ARRAY_SIZE_COMPUTE(arp_opts_param);
}

STATIC void *alloc_arp_args_init(void)
{
    return calloc(1, sizeof(struct arp_cmd_args));
}

STATIC int arp_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct arp_cmd_args *arp_args = (struct arp_cmd_args *)args;

    if (strcmp(opt->name, "-add") == 0) {
        if (arp_args->mode != ARP_SET_MODE_NONE) {
            goto arp_set_cmd_conflict;
        }
        arp_args->mode = ARP_SET_MODE_ADD;
        return 0;
    }

    if (strcmp(opt->name, "-del") == 0) {
        if (arp_args->mode != ARP_SET_MODE_NONE) {
            goto arp_set_cmd_conflict;
        }
        arp_args->mode = ARP_SET_MODE_DEL;
        return 0;
    }

    if (strcmp(opt->name, "-ip") == 0) {
        (void)memcpy_s(&arp_args->ip, sizeof(arp_args->ip), &type_val->ip, sizeof(type_val->ip));
        return 0;
    }

    if (strcmp(opt->name, "-mac") == 0) {
        (void)memcpy_s(&arp_args->mac, sizeof(arp_args->mac), &type_val->mac, sizeof(type_val->mac));
        return 0;
    }
    return 0;

arp_set_cmd_conflict:
    TOOL_PRINT_ERR("Option -add and -del conflict with each other.");
    return TOOL_PRIVATE_ARGS_CONFLICT_ERR;
}

STATIC int tool_arp_get(struct command_context *ctx)
{
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    size = MAX_TRANS_DATA_LEN - 1;
    ctx->result = (char *)calloc(MAX_TRANS_DATA_LEN, sizeof(char));
    CHK_PRT_RETURN(ctx->result == NULL, hccn_err("arp table result buffer calloc failed"), -ENOMEM);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_ARP_TABLE, &info);
    if (ret != 0) {
        hccn_err("dsmi get arp table config failed, ret:%d", ret);
        goto out;
    }

    TOOL_PRINT_INFO("arp table:");
    TOOL_PRINT_INFO("%s", ctx->result);
out:
    FREE_BUF(ctx->result);
    return ret;
}

STATIC int tool_prepare_arp_table_cfg(char match_list[][CONFLINE_LENGTH], struct command_context *ctx,
                                      struct arp_cmd_args *arp_args)
{
    int ret = 0;

    ret = sprintf_s(match_list[ARGC_ID_0], CONFLINE_LENGTH, "arp_%d=", ctx->slot_id);
    CHK_PRT_RETURN(ret <= 0, hccn_err("update arp conf sprintf slot_id failed, ret:%d", ret), -EIO);

    ret = sprintf_s(match_list[ARGC_ID_1], CONFLINE_LENGTH, "%s ", arp_args->ip.str_ip);
    CHK_PRT_RETURN(ret <= 0, hccn_err("update arp conf sprintf ip failed, ret:%d", ret), -EIO);

    return 0;
}

STATIC int tool_fill_add_arp_conf_cmd(char cmd_buf[], unsigned int cmd_buf_len, struct command_context *ctx,
                                      struct arp_cmd_args *arp_args)
{
    char *cmd_arg_buf = NULL;
    int ret = 0;

    cmd_arg_buf = (char *)calloc(CFG_INFO_PAYLOAD_LEN, sizeof(char));
    CHK_PRT_RETURN(cmd_arg_buf == NULL, hccn_err("tool add arp calloc in buf failed"), -ENOMEM);

    ret = sprintf_s(cmd_arg_buf, INFO_PAYLOAD_LEN, "-s %s %02x:%02x:%02x:%02x:%02x:%02x", arp_args->ip.str_ip,
                    MAC_ADDR_ARGS(arp_args->mac.mac_pieces));
    if (ret <= 0) {
        hccn_err("tool add arp construct cmdline failed, ret:%d", ret);
        ret = -EIO;
        goto out;
    }

    ret = sprintf_s(cmd_buf, cmd_buf_len, "arp_%d=%s\n", ctx->slot_id, cmd_arg_buf);
    if (ret <= 0) {
        hccn_err("tool add arp conf sprintf cmd_buf failed, ret:%d", ret);
        ret = -EIO;
        goto out;
    }

    ret = 0;
out:
    FREE_BUF(cmd_arg_buf);
    return ret;
}

STATIC int tool_update_arp_table_cfg(struct command_context *ctx)
{
    struct arp_cmd_args *arp_args = (struct arp_cmd_args *)ctx->cmd_args;
    char match_list[ARP_MATCH_CNT][CONFLINE_LENGTH] = {0};
    char cmd_buf[CONFLINE_LENGTH] = {0};
    int ret = 0;

    if (arp_args->mode == ARP_SET_MODE_ADD && tool_check_conf_udhcpc_enable(ctx->slot_id)) {
        return 0;
    }
    ret = tool_prepare_arp_table_cfg(match_list, ctx, arp_args);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool prepare arp table cfg failed"), ret);

    if (arp_args->mode == ARP_SET_MODE_ADD) {
        ret = tool_fill_add_arp_conf_cmd(cmd_buf, CONFLINE_LENGTH, ctx, arp_args);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool fill arp add cfg failed, ret:%d", ret), ret);

        ret = tool_update_conf(cmd_buf, FILE_OPT_TYPE_MATCH_REPLACES, match_list, ARP_MATCH_CNT);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool add arp cfg failed, ret:%d, cmd=\"%s\"", ret, cmd_buf), ret);
    } else {
        ret = tool_update_conf(NULL, FILE_OPT_TYPE_MATCH_DEL, match_list, ARP_MATCH_CNT);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool del arp cfg failed, ret:%d", ret), ret);
    }

    return 0;
}

STATIC bool is_valid_arp_set_args(struct command_context *ctx, struct arp_cmd_args *arp_args)
{
    if (!is_valid_ip(arp_args->ip.str_ip, AF_INET)) {
        TOOL_PRINT_ERR("IP type matching failed: expected IPv4, got IPv6, ip_address: %s.", arp_args->ip.str_ip);
        return false;
    }
    if (arp_args->mode == ARP_SET_MODE_ADD && (ctx->sub_opt_mask & SUB_OPT_MAC_MASK) == 0) {
        TOOL_PRINT_ERR("Option -mac <mac_address> is required but missing.");
        return false;
    }
    if (arp_args->mode == ARP_SET_MODE_DEL && (ctx->sub_opt_mask & SUB_OPT_MAC_MASK) != 0) {
        TOOL_PRINT_ERR("Option -mac <mac_address> is not required but was provided.");
        return false;
    }
    if (arp_args->mode == ARP_SET_MODE_NONE) {
        TOOL_PRINT_ERR("Option -add or -del is required but missing.");
        return false;
    }
    return true;
}

int tool_arp_set(struct command_context *ctx)
{
    struct arp_cmd_args *arp_args = (struct arp_cmd_args *)ctx->cmd_args;
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    CHK_PRT_RETURN(arp_args == NULL, hccn_err("arp_args is NULL"), TOOL_PRIVATE_ARGS_NUM_ERR);
    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);
    if (!is_valid_arp_set_args(ctx, arp_args)) {
        return TOOL_PRIVATE_ARGS_VALUE_ERR;
    }

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)arp_args, sizeof(struct arp_cmd_args),
                   NULL, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_ARP_TABLE, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi set arp table config failed, ret:%d", ret), ret);

    return 0;
}

STATIC int tool_arp_exec(struct command_context *ctx)
{
    int ret;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    if (ctx->mode == MODE_GET) {
        return tool_arp_get(ctx);
    }

    ret = tool_arp_set(ctx);
    if (ret == 0) {
        ret = tool_update_arp_table_cfg(ctx);
        CHK_PRT_RETURN(ret != 0,
                       hccn_err("update arp table cfg failed, ret:%d, slot_id:%d, logic_id:%d", ret, ctx->slot_id,
                                ctx->npu_info.logic_id),
                       ret);
    }
    return ret;
}

STATIC void tool_arp_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_arp_print_help(ctx, MODE_GET);
            tool_arp_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-arp", "get arp table");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-arp", "set arp table");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-add", "add arp table");
            TOOL_PRINT_OPT("-ip <ip_address>", "specify ip address for arp table, such as x.x.x.x");
            TOOL_PRINT_OPT("-mac <mac_address>", "specify mac address for arp table, such as xx:xx:xx:xx:xx:xx");

            TOOL_PRINT_CMD("-s", "-arp", "set arp table");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-del", "delete arp table");
            TOOL_PRINT_OPT("-ip <ip_address>", "specify ip address for arp table, such as x.x.x.x");
            break;
        default:
            break;
    }
}

struct sub_option_entry mac_opts_param[] = {
    {"-mac", ARG_TYPE_MAC, parse_mac, MODE_SET, MODE_SET},
};

STATIC void get_mac_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = mac_opts_param;
    *count = ARRAY_SIZE_COMPUTE(mac_opts_param);
}

STATIC void *alloc_mac_args_init(void)
{
    return calloc(1, sizeof(struct mac_val));
}

STATIC int set_mac_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct mac_val *mac_args = (struct mac_val *)args;

    if (strcmp(opt->name, "-mac") == 0) {
        (void)memcpy_s(mac_args, sizeof(struct mac_val), &type_val->mac, sizeof(type_val->mac));
    }

    return 0;
}

STATIC int tool_get_mac_exec(struct command_context *ctx)
{
    unsigned int size = sizeof(struct mac_val);
    struct mac_val mac_address = {0};
    struct dsmi_info info = {0};
    int ret = 0;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, (char *)&mac_address, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_MAC_ADDR, &info);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("get mac addr failed, logic_id:%d dev_name:%s ret:%d", ctx->npu_info.logic_id, ctx->dev_name, ret),
        ret);

    TOOL_PRINT_INFO("mac addr: %02x:%02x:%02x:%02x:%02x:%02x", MAC_ADDR_ARGS(mac_address.mac_pieces));
    return 0;
}

STATIC int tool_set_mac_exec(struct command_context *ctx)
{
    struct mac_val *mac_args = (struct mac_val *)ctx->cmd_args;
    char mac_addr_string[MAC_STR_LEN + 1] = {0};
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    LOG_PROMPT(ctx, "start to set mac addr:%02x:%02x:%02x:%02x:%02x:%02x, logic_id:%d",
               MAC_ADDR_ARGS(mac_args->mac_pieces), ctx->npu_info.logic_id);

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)ctx->cmd_args, sizeof(struct mac_val),
                   NULL, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_MAC_ADDR, &info);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("set mac addr failed, logic_id:%d dev_name:%s ret:%d", ctx->npu_info.logic_id, ctx->dev_name, ret),
        ret);

    ret = snprintf_s(mac_addr_string, MAC_STR_LEN + 1, MAC_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
                     MAC_ADDR_ARGS(mac_args->mac_pieces));
    CHK_PRT_RETURN(ret <= 0, hccn_err("snprintf_s mac_addr_string failed"), -EIO);

    ret = tool_write_conf(ctx->slot_id, "mac_addr", mac_addr_string);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("write cfg error, ret:%d slot_id:%d logic_id:%d)", ret, ctx->slot_id, ctx->npu_info.logic_id),
        TOOL_WRITE_CONF_ERR);

    return 0;
}

STATIC int tool_mac_exec(struct command_context *ctx)
{
    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    if (ctx->mode == MODE_GET) {
        return (tool_get_mac_exec(ctx));
    } else {
        return (tool_set_mac_exec(ctx));
    }
}

STATIC void tool_mac_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_mac_print_help(ctx, MODE_GET);
            tool_mac_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-mac", "get bond netdev mac address");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-mac", "set mac address for bond netdev");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-mac <mac_address>", "mac address, such as xx:xx:xx:xx:xx:xx");
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry ip_opts_param[] = {
    {"-address", ARG_TYPE_IP, parse_ip, MODE_SET, MODE_SET},
    {"-netmask", ARG_TYPE_IP, parse_ip, MODE_SET, MODE_SET},
};

STATIC void get_ip_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = ip_opts_param;
    *count = ARRAY_SIZE_COMPUTE(ip_opts_param);
}

STATIC void *alloc_ip_args_init(void)
{
    // Use IPv4 address by default
    struct ip_cmd_args *ip_args = (struct ip_cmd_args *)calloc(1, sizeof(struct ip_cmd_args));
    return (void *)ip_args;
}

STATIC int ip_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct ip_cmd_args *ip_args = (struct ip_cmd_args *)args;
    int ret = 0;

    if (strcmp(opt->name, "-address") == 0) {
        ret = memcpy_s(&ip_args->ip, sizeof(ip_args->ip), &type_val->ip, sizeof(type_val->ip));
    } else if (strcmp(opt->name, "-netmask") == 0) {
        ret = memcpy_s(&ip_args->netmask, sizeof(ip_args->netmask), &type_val->ip, sizeof(type_val->ip));
    } else {
        return -EINVAL;
    }

    return ret;
}

STATIC int tool_get_ip_exec(struct command_context *ctx)
{
    char netmask_str[MAX_IP_VAL_LEN] = {0};
    char ip_str[MAX_IP_VAL_LEN] = {0};
    struct ip_netmask_val ip_netmask;
    struct dsmi_info info = {0};
    unsigned int size;
    int ret = 0;

    size = sizeof(struct ip_netmask_val);
    ctx->result = (char *)&ip_netmask;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_IP, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get ip info failed, ret:%d", ret), ret);

    ret = tool_ipv4_int_to_str(ip_netmask.ip, ip_str, MAX_IP_VAL_LEN);
    CHK_PRT_RETURN(ret != 0, hccn_err("ipv4_int_to_str failed! ret:%d ip:0x%x", ret, ip_netmask.ip), ret);

    ret = tool_ipv4_int_to_str(ip_netmask.netmask, netmask_str, MAX_IP_VAL_LEN);
    CHK_PRT_RETURN(ret != 0, hccn_err("ipv4_int_to_str failed! ret:%d netmask:0x%x", ret, ip_netmask.netmask), ret);

    TOOL_PRINT_INFO("ipaddr:%s", ip_str);
    TOOL_PRINT_INFO("netmask:%s", netmask_str);

    return ret;
}

STATIC int tool_write_ip_conf(struct command_context *ctx, const char *address, const char *netmask)
{
    int ret;

    ret = tool_write_conf(ctx->slot_id, "address", address);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("write address cfg error, ret:%d slot_id:%d logic_id:%d", ret, ctx->slot_id, ctx->npu_info.logic_id),
        TOOL_WRITE_CONF_ERR);

    ret = tool_write_conf(ctx->slot_id, "netmask", netmask);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("write netmask cfg error, ret:%d slot_id:%d logic_id:%d", ret, ctx->slot_id, ctx->npu_info.logic_id),
        TOOL_WRITE_CONF_ERR);

    return 0;
}

static bool is_dev_ipaddr_conflict(int logic_id, int port_type, int port_id, unsigned int ipaddr, unsigned int mask)
{
    unsigned int ip_addr, mask_addr;
    ip_addr_t mask_address = {0};
    ip_addr_t ip_address = {0};
    int ret;

    ip_address.ip_type = IPADDR_TYPE_V4;
    mask_address.ip_type = IPADDR_TYPE_V4;
    ret = dsmi_get_device_ip_address(logic_id, port_type, port_id, &ip_address, &mask_address);
    if (ret) {
        hccn_err("dsmi_get_device_ip_address failed ret:%d, logic_id:%d", ret, logic_id);
        return false;
    }

    ip_addr = *(unsigned int *)&ip_address.u_addr;
    mask_addr = *(unsigned int *)&mask_address.u_addr;
    if ((ip_addr & mask_addr) == (ipaddr & mask_addr)) {
        return true;
    }

    return false;
}

bool is_ipaddr_conflict(struct command_context *ctx)
{
    struct ip_cmd_args *ip_args = (struct ip_cmd_args *)ctx->cmd_args;
    unsigned int netmask = ip_args->netmask.ip_addr;
    const char *vnic_host_addr = "192.168.1.199";
    const char *vnic_host_mask = "255.255.255.0";
    unsigned int ip_addr = ip_args->ip.ip_addr;
    int logic_id = ctx->npu_info.logic_id;
    unsigned int vnic_h_ipaddr;
    unsigned int vnic_h_mask;

    if (is_dev_ipaddr_conflict(logic_id, DSMI_VNIC_PORT, 0, ip_addr, netmask)) {
        return true;
    }

    vnic_h_ipaddr = inet_addr(vnic_host_addr);
    vnic_h_mask = inet_addr(vnic_host_mask);
    /* check if there has confliction that ip with device vnic's host_ip */
    if ((ip_addr & vnic_h_mask) == (vnic_h_ipaddr & vnic_h_mask)) {
        return true;
    }

    return false;
}

STATIC int tool_set_ip_exec(struct command_context *ctx)
{
    struct ip_cmd_args *ip_args = (struct ip_cmd_args *)ctx->cmd_args;
    char *mask_str = ip_args->netmask.str_ip;
    char *ip_str = ip_args->ip.str_ip;
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    LOG_PROMPT(ctx, "start to set ip addr:%s and netmask_ip:%s, logic_id:%d", ip_args->ip.str_ip,
               ip_args->netmask.str_ip, ctx->npu_info.logic_id);

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d dev_name:%s", ret,
                            ctx->npu_info.logic_id, ctx->dev_name),
                   TOOL_PUBLIC_ROOT_UNSUPPORT);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)ip_args, sizeof(struct ip_cmd_args), NULL,
                   &size);

    if (is_ipaddr_conflict(ctx)) {
        hccn_err("ipaddr conflict, address[%s] netmask[%s]", ip_str, mask_str);
        return DEV_EXE_CONFLICT_IP_ADDR_ERR;
    }

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_IP, &info);
    CHK_PRT_RETURN(
        ret != 0, hccn_err("set ip failed, logic_id:%d dev_name:%s ret:%d", ctx->npu_info.logic_id, ctx->dev_name, ret),
        ret);

    if (!tool_check_conf_udhcpc_enable(ctx->slot_id)) {
        ret = tool_write_ip_conf(ctx, ip_str, mask_str);
        CHK_PRT_RETURN(ret != 0,
                       hccn_err("write ip cfg error! ret:%d logic_id:%d addr:%s netmask:%s", ret,
                                ctx->npu_info.logic_id, ip_str, mask_str),
                       ret);
    }

    return 0;
}

STATIC int tool_ip_exec(struct command_context *ctx)
{
    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    if (ctx->mode == MODE_GET) {
        return tool_get_ip_exec(ctx);
    } else {
        return tool_set_ip_exec(ctx);
    }
}

STATIC void tool_ip_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_ip_print_help(ctx, MODE_GET);
            tool_ip_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-ip", "get ip address and netmask");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-ip", "set ip cfg");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-address <ip>", "set IPv4 address, such as x.x.x.x");
            TOOL_PRINT_OPT("-netmask <netmask>", "set IPv4 netmask, such as x.x.x.x");
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry pfc_opts_param[] = {
    {"-bitmap", ARG_TYPE_STR, parse_str, MODE_SET, MODE_SET},
};

STATIC void get_pfc_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = pfc_opts_param;
    *count = ARRAY_SIZE_COMPUTE(pfc_opts_param);
}

STATIC void *alloc_pfc_args_init(void)
{
    return calloc(1, sizeof(struct pfc_cmd_args));
}

STATIC int pfc_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct pfc_cmd_args *pfc_args = (struct pfc_cmd_args *)args;
    int ret = 0;

    if (strcmp(opt->name, "-bitmap") == 0) {
        CHK_PRT_RETURN(
            strlen(type_val->str_val) != PFC_CFG_STR_LEN,
            hccn_err("input pfc_cfg bitmap length %zu, expect %d", strlen(type_val->str_val), PFC_CFG_STR_LEN),
            TOOL_PRIVATE_ARGS_NUM_ERR);

        ret = strncpy_s(pfc_args->pfc_enabled, PFC_CFG_STR_LEN + 1, type_val->str_val, strlen(type_val->str_val));
        CHK_PRT_RETURN(ret != 0,
                       hccn_err("strncpy_s pfc_enabled failed! ret:%d, str_len:%zu", ret, strlen(type_val->str_val)),
                       TOOL_PRIVATE_ARGS_PARSE_ERR);
    }
    return 0;
}

STATIC int tool_get_pfc_info(struct command_context *ctx)
{
    char pfc_enabled[PRIORITY_NUM] = {0};
    unsigned int size_out = PRIORITY_NUM;
    struct dsmi_info info = {0};
    int ret;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, pfc_enabled, &size_out);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_PFC_CFG, &info);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("dsmi get pfc info failed, logic_id:%d dev_name:%s ret:%d", ctx->npu_info.logic_id,
                            ctx->dev_name, ret),
                   ret);

    TOOL_PRINT_INFO("[dev_name:%s]", ctx->dev_name);
    TOOL_PRINT_INFO("priority   0 1 2 3 4 5 6 7");
    TOOL_PRINT_INFO("enabled    %d %d %d %d %d %d %d %d", PRIORITY_ARGS(pfc_enabled));

    return 0;
}

STATIC int tool_get_pfc_exec(struct command_context *ctx, struct netdev_info *ndev_info)
{
    unsigned int i, err_count = 0;
    int ret = 0;

    TOOL_PRINT_INFO("PFC configuration:");

    for (i = 0; i < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; i++) {
        (void)memset_s(ctx->dev_name, MAX_DEV_NAME_LEN, 0, MAX_DEV_NAME_LEN);
        ret = strncpy_s(ctx->dev_name, MAX_DEV_NAME_LEN, ndev_info->bond_info.slaves_ifname[i],
                        strlen(ndev_info->bond_info.slaves_ifname[i]));
        CHK_PRT_RETURN(ret != 0, hccn_err("copy slave_name to ctx->dev_name failed, ret:%d", ret), ret);

        ret = tool_get_pfc_info(ctx);
        if (ret != 0) {
            err_count++;
            hccn_err("tool get pfc info failed, ret:%d dev_name:%s err_count:%u", ret, ctx->dev_name, err_count);
            TOOL_PRINT_ERR("[dev_name:%s] get the pfc result failed!", ctx->dev_name);
        }
    }

    return (err_count == DEV_INFO_DEFAULT_BOND_SLAVE_NUM) ? ret : 0;
}

STATIC int tool_pfc_str2char(const char pfc_str[], char pfc_enabled[PRIORITY_NUM])
{
#define PFC_CONF_INDEX 2
    int priority_i = 0;
    int str_i = 0;

    for (str_i = 0; str_i < PFC_CFG_STR_LEN && priority_i < PRIORITY_NUM; str_i++) {
        if (str_i % PFC_CONF_INDEX == 1) {
            if (pfc_str[str_i] == ',') {
                continue;
            }
            CHK_PRT_RETURN((pfc_str[str_i] >= '0') && (pfc_str[str_i] <= '9'),
                           TOOL_PRINT_ERR("Option -bitmap value range [0, 1]"), TOOL_PRIVATE_ARGS_VALUE_ERR);
            CHK_PRT_RETURN(pfc_str[str_i] != ',', TOOL_PRINT_ERR("Option -bitmap must be separated by comma"),
                           TOOL_PRIVATE_ARGS_VALUE_ERR);
        } else if ((str_i % PFC_CONF_INDEX == 0) && (pfc_str[str_i] != '0') && (pfc_str[str_i] != '1')) {
            CHK_PRT_RETURN(pfc_str[str_i] == ',', TOOL_PRINT_ERR("Option -bitmap contains redundant comma"),
                           TOOL_PRIVATE_ARGS_VALUE_ERR);

            TOOL_PRINT_ERR("Option -bitmap value range [0, 1], [%c] is invalid", pfc_str[str_i]);
            return TOOL_PRIVATE_ARGS_VALUE_ERR;
        } else {
            pfc_enabled[priority_i++] = pfc_str[str_i] - '0';
        }
    }

    return 0;
}

int tool_set_pfc_exec(struct command_context *ctx, struct netdev_info *ndev_info)
{
    char pfc_enabled[PRIORITY_NUM] = {0};
    struct dsmi_info info = {0};
    unsigned int size_out = 0;
    unsigned int i = 0;
    int ret = 0;

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    ret = tool_pfc_str2char((char *)ctx->cmd_args, pfc_enabled);
    CHK_PRT_RETURN(ret != 0, hccn_err("invalid input pfc cfg"), ret);

    for (i = 0; i < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; i++) {
        size_out = 0;
        (void)memset_s(ctx->dev_name, MAX_DEV_NAME_LEN, 0, MAX_DEV_NAME_LEN);
        ret = strncpy_s(ctx->dev_name, MAX_DEV_NAME_LEN, ndev_info->bond_info.slaves_ifname[i],
                        strlen(ndev_info->bond_info.slaves_ifname[i]));
        CHK_PRT_RETURN(ret != 0, hccn_err("copy slave_name to ctx->dev_name failed, ret:%d", ret), ret);

        DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), pfc_enabled, PRIORITY_NUM, NULL, &size_out);

        ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_PFC_CFG, &info);
        CHK_PRT_RETURN(ret != 0, hccn_err("[dev_name:%s] set the pfc_cfg failed!", ctx->dev_name), ret);
    }

    return 0;
}

STATIC int tool_pfc_exec(struct command_context *ctx)
{
    struct pfc_cmd_args *pfc_args = (struct pfc_cmd_args *)ctx->cmd_args;
    struct netdev_info ndev_info = {0};
    int ret = 0;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    ret = tool_get_bond_slaves(ctx, &ndev_info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get bond slaves failed, ret:%d", ret), ret);

    if (ctx->mode == MODE_GET) {
        return tool_get_pfc_exec(ctx, &ndev_info);
    } else {
        ret = tool_set_pfc_exec(ctx, &ndev_info);
        CHK_PRT_RETURN(ret != 0, TOOL_PRINT_ERR("[dev_name:%s] set the pfc_cfg failed!", ctx->dev_name), ret);

        ret = tool_write_conf(ctx->slot_id, "bitmap", pfc_args->pfc_enabled);
        CHK_PRT_RETURN(
            ret != 0,
            hccn_err("write cfg error, ret:%d slot_id:%d logic_id:%d)", ret, ctx->slot_id, ctx->npu_info.logic_id),
            TOOL_WRITE_CONF_ERR);
    }

    return 0;
}

STATIC void tool_pfc_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_pfc_print_help(ctx, MODE_GET);
            tool_pfc_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-pfc", "get pfc cfg");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-pfc", "set pfc cfg");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-bitmap <bit_arr>", "each bit corresponds to one priority, such as 0,0,0,0,1,0,0,0");
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry prio_tc_opts_param[] = {
    {"-map", ARG_TYPE_STR, parse_str, MODE_SET, MODE_SET},
};

STATIC void get_prio_tc_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = prio_tc_opts_param;
    *count = ARRAY_SIZE_COMPUTE(prio_tc_opts_param);
}

STATIC void *alloc_prio_tc_args_init(void)
{
    return calloc(1, sizeof(struct prio_tc_cmd_args));
}

STATIC int prio_tc_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct prio_tc_cmd_args *prio_tc_args = (struct prio_tc_cmd_args *)args;
    int ret = 0;

    if (strcmp(opt->name, "-map") == 0) {
        CHK_PRT_RETURN(strlen(type_val->str_val) != PFC_CFG_STR_LEN,
                       hccn_err("input prio_tc map length %zu, expect %d", strlen(type_val->str_val), PFC_CFG_STR_LEN),
                       TOOL_PRIVATE_ARGS_NUM_ERR);

        ret = strncpy_s(prio_tc_args->map, PFC_CFG_STR_LEN + 1, type_val->str_val, strlen(type_val->str_val));
        CHK_PRT_RETURN(ret != 0,
                       hccn_err("strncpy_s prio_tc map failed! ret:%d, str_len:%zu", ret, strlen(type_val->str_val)),
                       TOOL_PRIVATE_ARGS_PARSE_ERR);
    }
    return 0;
}

STATIC int tool_get_prio_tc_info(struct command_context *ctx)
{
    unsigned int size_out = PRIORITY_NUM;
    char prio_tc[PRIORITY_NUM] = {0};
    struct dsmi_info info = {0};
    int ret;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, prio_tc, &size_out);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_PRIO_TC_INFO, &info);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("get prio_tc info failed, logic_id:%d dev_name:%s ret:%d", ctx->npu_info.logic_id, ctx->dev_name, ret),
        ret);

    TOOL_PRINT_INFO("[dev_name:%s]", ctx->dev_name);
    TOOL_PRINT_INFO("priority   0 1 2 3 4 5 6 7");
    TOOL_PRINT_INFO("TC         %d %d %d %d %d %d %d %d", PRIORITY_ARGS(prio_tc));

    return 0;
}

STATIC int tool_get_prio_tc_exec(struct command_context *ctx, struct netdev_info *ndev_info)
{
    unsigned int i, err_count = 0;
    int ret = 0;

    TOOL_PRINT_INFO("Priority mapping:");

    for (i = 0; i < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; i++) {
        (void)memset_s(ctx->dev_name, MAX_DEV_NAME_LEN, 0, MAX_DEV_NAME_LEN);
        ret = strncpy_s(ctx->dev_name, MAX_DEV_NAME_LEN, ndev_info->bond_info.slaves_ifname[i],
                        strlen(ndev_info->bond_info.slaves_ifname[i]));
        CHK_PRT_RETURN(ret != 0, hccn_err("copy slave_name to ctx->dev_name failed, ret:%d", ret), ret);

        ret = tool_get_prio_tc_info(ctx);
        if (ret != 0) {
            err_count++;
            hccn_err("tool_get_prio_tc_info failed, ret:%d dev_name:%s err_count:%u", ret, ctx->dev_name, err_count);
            TOOL_PRINT_ERR("[dev_name:%s] get the mapping between priority and TC failed!", ctx->dev_name);
        }
    }

    return (err_count == DEV_INFO_DEFAULT_BOND_SLAVE_NUM) ? ret : 0;
}

STATIC int tool_prio_tc_str2char(const char prio_tc_str[], char prio_tc[PRIORITY_NUM])
{
#define PRIO_TC_CONF_INDEX 2
#define MIN_TC_INDEX 0
#define MAX_TC_INDEX 3
    int priority_i = 0;
    int str_i;

    for (str_i = 0; str_i < PFC_CFG_STR_LEN && priority_i < PRIORITY_NUM; str_i++) {
        if (str_i % PRIO_TC_CONF_INDEX == 0) {
            CHK_PRT_RETURN(prio_tc_str[str_i] == ',', TOOL_PRINT_ERR("Option -map contains redundant comma"),
                           TOOL_PRIVATE_ARGS_VALUE_ERR);

            CHK_PRT_RETURN((prio_tc_str[str_i] < '0') || (prio_tc_str[str_i] > '9'),
                           TOOL_PRINT_ERR("Option -map contains invalid char[%c]", prio_tc_str[str_i]),
                           TOOL_PRIVATE_ARGS_VALUE_ERR);

            CHK_PRT_RETURN(prio_tc_str[str_i] > '3',
                           TOOL_PRINT_ERR("Option -map value range [%d, %d], [%c] is invalid", MIN_TC_INDEX,
                                          MAX_TC_INDEX, prio_tc_str[str_i]),
                           TOOL_PRIVATE_ARGS_VALUE_ERR);

            prio_tc[priority_i++] = prio_tc_str[str_i];
        } else if (str_i % PRIO_TC_CONF_INDEX == 1) {
            if (prio_tc_str[str_i] == ',') {
                continue;
            }
            CHK_PRT_RETURN((prio_tc_str[str_i] >= '0') && (prio_tc_str[str_i] <= '9'),
                           TOOL_PRINT_ERR("Option -map value is out of range [%d, %d]", MIN_TC_INDEX, MAX_TC_INDEX),
                           TOOL_PRIVATE_ARGS_VALUE_ERR);

            CHK_PRT_RETURN(prio_tc_str[str_i] != ',', TOOL_PRINT_ERR("Option -map must be separated by comma"),
                           TOOL_PRIVATE_ARGS_VALUE_ERR);
        }
    }

    return 0;
}

int tool_set_prio_tc_exec(struct command_context *ctx, struct netdev_info *ndev_info)
{
    char prio_tc[PRIORITY_NUM] = {0};
    struct dsmi_info info = {0};
    unsigned int size_out = 0;
    unsigned int i = 0;
    int ret = 0;

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    ret = tool_prio_tc_str2char((char *)ctx->cmd_args, prio_tc);
    CHK_PRT_RETURN(ret != 0, hccn_err("invalid input prio_tc cfg"), ret);

    for (i = 0; i < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; i++) {
        size_out = 0;
        (void)memset_s(ctx->dev_name, MAX_DEV_NAME_LEN, 0, MAX_DEV_NAME_LEN);
        ret = strncpy_s(ctx->dev_name, MAX_DEV_NAME_LEN, ndev_info->bond_info.slaves_ifname[i],
                        strlen(ndev_info->bond_info.slaves_ifname[i]));
        CHK_PRT_RETURN(ret != 0, hccn_err("copy slave_name to ctx->dev_name failed, ret:%d", ret), ret);

        DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), prio_tc, PRIORITY_NUM, NULL, &size_out);

        ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_PRIO_TC_INFO, &info);
        CHK_PRT_RETURN(ret != 0, hccn_err("[dev_name:%s] set the prio_tc map failed!", ctx->dev_name), ret);
    }

    return 0;
}

STATIC int tool_prio_tc_exec(struct command_context *ctx)
{
    struct prio_tc_cmd_args *prio_tc_args = (struct prio_tc_cmd_args *)ctx->cmd_args;
    struct netdev_info ndev_info = {0};
    int ret = 0;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    ret = tool_get_bond_slaves(ctx, &ndev_info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get bond slaves failed, ret:%d", ret), ret);

    if (ctx->mode == MODE_GET) {
        return tool_get_prio_tc_exec(ctx, &ndev_info);
    } else {
        ret = tool_set_prio_tc_exec(ctx, &ndev_info);
        CHK_PRT_RETURN(ret != 0, TOOL_PRINT_ERR("[dev_name:%s] set the prio_tc map failed!", ctx->dev_name), ret);

        ret = tool_write_conf(ctx->slot_id, "prio_tc", prio_tc_args->map);
        CHK_PRT_RETURN(
            ret != 0,
            hccn_err("write cfg error, ret:%d slot_id:%d logic_id:%d)", ret, ctx->slot_id, ctx->npu_info.logic_id),
            TOOL_WRITE_CONF_ERR);
    }

    return 0;
}

STATIC void tool_prio_tc_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_prio_tc_print_help(ctx, MODE_GET);
            tool_prio_tc_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-prio_tc", "get the mapping between priority and TC");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-prio_tc", "set the mapping between each priority and TC, TC range [0, 3]");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-map <tc_arr>", "each number corresponds to one TC, such as 0,0,0,0,0,0,0,0");
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry udhcpc_opts_param[] = {
    {"-timeout", ARG_TYPE_INT, parse_int, MODE_GET, 0, true, .range.int_range = {1, MAX_UDHCPC_TIMEOUT}},
    {"-quit", ARG_TYPE_NONE, parse_none, MODE_GET, 0},
    {"-release", ARG_TYPE_NONE, parse_none, MODE_SET, MODE_SET},
};

STATIC void get_udhcpc_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = udhcpc_opts_param;
    *count = ARRAY_SIZE_COMPUTE(udhcpc_opts_param);
}

STATIC void *alloc_udhcpc_args_init(void)
{
    return calloc(1, sizeof(struct udhcpc_cmd_args));
}

STATIC int udhcpc_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct udhcpc_cmd_args *udhcpc = (struct udhcpc_cmd_args *)args;

    if (strcmp(opt->name, "-timeout") == 0) {
        udhcpc->timeout = type_val->int_val;
    } else if (strcmp(opt->name, "-quit") == 0) {
        udhcpc->proc_type = UDHCP_PROC_QUIT;
    }

    return 0;
}

STATIC int tool_print_udhcpc_info(struct command_context *ctx)
{
    int ret = 0;

    if (strstr(ctx->result, "lease time") != NULL && strstr(ctx->result, "setting") != NULL) {
        if (strstr(ctx->result, "adding router") != NULL) {
            LOG_PROMPT(ctx, "Acquired DHCP info, set ip/mask and router success.");
        } else {
            LOG_PROMPT(ctx, "Acquired DHCP info, set ip/mask success, no router set.");
        }
    } else {
        LOG_PROMPT(ctx, "Acquire DHCP cmd run success, no ip and router set.");
        ret = TOOL_DHCPC_CONFIG_ERR;
    }

    TOOL_PRINT_INFO("device:%d:%s", ctx->npu_info.logic_id, ctx->result);
    return ret;
}

int tool_get_udhcpc_exec(struct command_context *ctx)
{
    struct udhcpc_cmd_args *udhcpc_args = (struct udhcpc_cmd_args *)ctx->cmd_args;
    unsigned int size = OUTBUF_LEN_5120 - 1;
    struct dsmi_info info = {0};
    int ret = 0;

    LOG_PROMPT(ctx, "start to acquire dhcp ip, logic_id:%d", ctx->npu_info.logic_id);

    if (udhcpc_args == NULL) {
        udhcpc_args = calloc(1, sizeof(struct udhcpc_cmd_args));
        CHK_PRT_RETURN(udhcpc_args == NULL, hccn_err("udhcpc_cmd_args calloc failed"), -ENOMEM);
    }

    if (udhcpc_args->timeout == 0) {
        udhcpc_args->timeout = UDHCPC_TIMEOUT_DEFAULT;
    }
    if (udhcpc_args->proc_type == UDHCP_PROC_TYPE_INIT) {
        udhcpc_args->proc_type = UDHCP_PROC_RETAIN;
    }

    ctx->cmd_args = udhcpc_args;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)udhcpc_args,
                   sizeof(struct udhcpc_cmd_args), ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_ACQUIRE_DHCP_IP, &info);
    if (ret == TOOL_DHCPC_PROCESS_EXIST_ERR) {
        LOG_PROMPT(ctx, "UDHCPC is running.");
    }
    if (ret != 0) {
        hccn_err("acquire dhcp ip failed, ret:%d", ret);
        LOG_PROMPT(ctx, "acquire dhcp run failed, logic_id:%d", ctx->npu_info.logic_id);
        return ret;
    }

    return ret;
}

STATIC int tool_set_udhcpc_exec(struct command_context *ctx)
{
    unsigned int size = OUTBUF_LEN_5120 - 1;
    char dhcp_res[OUTBUF_LEN_5120] = {0};
    struct dsmi_info info = {0};
    int ret = 0;

    LOG_PROMPT(ctx, "start to release dhcp ip, logic_id:%d", ctx->npu_info.logic_id);

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    ctx->result = dhcp_res;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_RELEASE_DHCP_IP, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("release dhcp ip failed, ret:%d", ret), ret);

    LOG_PROMPT(ctx, "release dhcp ip success, logic_id:%d", ctx->npu_info.logic_id);
    TOOL_PRINT_INFO("device:%d:release success!", ctx->npu_info.logic_id);
    return 0;
}

STATIC int tool_add_udhcpc_conf(struct command_context *ctx)
{
#define UDHCPC_ENABLE_LEN 4
    char udhcpc_enable[UDHCPC_ENABLE_LEN] = {0};
    int ret = 0;

    ret = sprintf_s(udhcpc_enable, UDHCPC_ENABLE_LEN, "%s", "on");
    CHK_PRT_RETURN(ret <= 0, hccn_err("udhcpc enable info sprintf failed, ret:%d", ret), -EIO);

    ret = tool_write_conf(ctx->slot_id, "udhcpc_enable", udhcpc_enable);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("write cfg error, ret:%d slot_id:%d logic_id:%d)", ret, ctx->slot_id, ctx->npu_info.logic_id),
        TOOL_WRITE_CONF_ERR);

    return ret;
}

STATIC int tool_del_udhcpc_conf(struct command_context *ctx)
{
    int ret = 0;

    ret = tool_del_conf(ctx->slot_id, "udhcpc_enable");
    CHK_PRT_RETURN(ret != 0, hccn_err("del cfg error! ret:%d slot_id:%d", ret, ctx->slot_id), TOOL_WRITE_CONF_ERR);

    return ret;
}

STATIC int tool_udhcpc_exec(struct command_context *ctx)
{
    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);
    char dhcp_res[OUTBUF_LEN_5120] = {0};
    int ret = 0;

    if (ctx->mode == MODE_GET) {
        ctx->result = dhcp_res;
        ret = tool_get_udhcpc_exec(ctx);
        if (ret != 0) {
            goto out;
        }

        ret = tool_print_udhcpc_info(ctx);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool_print_udhcpc_info failed, ret:%d", ret), ret);

        ret = tool_add_udhcpc_conf(ctx);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool_add_udhcpc_conf failed, ret:%d", ret), ret);
    } else {
        ret = tool_set_udhcpc_exec(ctx);
        if (ret != 0) {
            goto out;
        }

        ret = tool_del_udhcpc_conf(ctx);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool_del_udhcpc_conf failed, ret:%d", ret), ret);
    }

out:
    return ret;
}

STATIC void tool_udhcpc_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_udhcpc_print_help(ctx, MODE_GET);
            tool_udhcpc_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-udhcpc", "acquire dhcp ip");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("[-timeout <timeout>]", "default is 3 seconds, range [1, 300] seconds");
            TOOL_PRINT_OPT("[-quit]", "exit after obtaining lease, default not exit");
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-udhcpc", "release dhcp ip");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-release", "release dhcp ip");
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry dscp_tc_opts_param[] = {
    {"-dscp", ARG_TYPE_INT, parse_int, MODE_GET, 0, true, .range.int_range = {0, 63}},
};

STATIC void get_dscp_to_tc_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = dscp_tc_opts_param;
    *count = ARRAY_SIZE_COMPUTE(dscp_tc_opts_param);
}

STATIC void *alloc_dscp_to_tc_args_init(void)
{
    return calloc(1, sizeof(struct dscp_to_tc_args));
}

STATIC int dscp_to_tc_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct dscp_to_tc_args *dscp_tc_args = (struct dscp_to_tc_args *)args;

    if (strcmp(opt->name, "-dscp") == 0) {
        dscp_tc_args->dscp_val = type_val->int_val;
    }

    return 0;
}

STATIC int tool_get_dscp_map_info(struct command_context *ctx)
{
    struct dscp_to_tc_args *dscp_tc_args = (struct dscp_to_tc_args *)ctx->cmd_args;
    unsigned int size_out = OUTBUF_LEN_1024 - 1;
    char dscp_map_info[OUTBUF_LEN_1024] = {0};
    struct dsmi_info info = {0};
    int ret;

    if (ctx->cmd_args == NULL) {
        dscp_tc_args = calloc(1, sizeof(struct dscp_to_tc_args));
        CHK_PRT_RETURN(dscp_tc_args == NULL, hccn_err("dscp_tc_args calloc failed"), -ENOMEM);

        ctx->cmd_args = dscp_tc_args;
        dscp_tc_args->get_all_flag = true;
    }

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)dscp_tc_args,
                   sizeof(struct dscp_to_tc_args), dscp_map_info, &size_out);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_DSCP_MAP_INFO, &info);
    CHK_PRT_RETURN(
        ret != 0, hccn_err("get dscp_map info failed, slot_id:%d dev_name:%s ret:%d", ctx->slot_id, ctx->dev_name, ret),
        ret);

    TOOL_PRINT_INFO("[dev_name:%s]", ctx->dev_name);
    TOOL_PRINT_INFO("%s\t%s", "dscp", "tc");
    TOOL_PRINT_INFO("%s", dscp_map_info);

    return 0;
}

STATIC int tool_get_dscp_to_tc_exec(struct command_context *ctx, struct netdev_info *ndev_info)
{
    unsigned int err_count = 0;
    int ret, i = 0;

    for (i = 0; i < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; i++) {
        (void)memset_s(ctx->dev_name, MAX_DEV_NAME_LEN, 0, MAX_DEV_NAME_LEN);
        ret = strncpy_s(ctx->dev_name, MAX_DEV_NAME_LEN, ndev_info->bond_info.slaves_ifname[i],
                        strlen(ndev_info->bond_info.slaves_ifname[i]));
        CHK_PRT_RETURN(ret != 0, hccn_err("copy slave_name to ctx->dev_name failed, ret:%d", ret), ret);

        ret = tool_get_dscp_map_info(ctx);
        if (ret != 0) {
            err_count++;
            hccn_err("tool_get_dscp_map_info failed, ret:%d dev_name:%s err_count:%u", ret, ctx->dev_name, err_count);
            TOOL_PRINT_ERR("[dev_name:%s] get the mapping between dscp and TC failed!", ctx->dev_name);
        }
    }

    return (err_count == DEV_INFO_DEFAULT_BOND_SLAVE_NUM) ? ret : 0;
}

STATIC int tool_dscp_to_tc_exec(struct command_context *ctx)
{
    struct netdev_info ndev_info = {0};
    int ret = 0;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    ret = tool_get_bond_slaves(ctx, &ndev_info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get bond slaves failed, ret:%d", ret), ret);

    return tool_get_dscp_to_tc_exec(ctx, &ndev_info);
}

STATIC void tool_dscp_to_tc_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_dscp_to_tc_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-dscp_to_tc", "get dscp to tc map info");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("[-dscp <dscp_val>]", "get the specified dscp of tc map info, range [0, 63]");
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry tc_stat_opts_param[] = {
    {"-tc_id", ARG_TYPE_INT, parse_int, MODE_SET, 0, true, .range.int_range = {0, 7}},
    {"-clear", ARG_TYPE_NONE, parse_none, MODE_SET, 0},
};

STATIC void get_tc_stat_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = tc_stat_opts_param;
    *count = ARRAY_SIZE_COMPUTE(tc_stat_opts_param);
}

STATIC void *alloc_tc_stat_args_init(void)
{
    return calloc(1, sizeof(struct tc_stat_cmd_args));
}

STATIC int tc_stat_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct tc_stat_cmd_args *tc_args = (struct tc_stat_cmd_args *)args;

    if (strcmp(opt->name, "-clear") == 0 && tc_args->mode == TC_SET_NONE) {
        tc_args->mode = TC_SET_CLEAR;
    } else if (strcmp(opt->name, "-tc_id") == 0 && tc_args->mode == TC_SET_NONE) {
        tc_args->mode = TC_SET_ENABLE;
        tc_args->tc_id = (unsigned int)type_val->int_val;
    } else {
        TOOL_PRINT_ERR("Option -tc_id and -clear conflict with each other.");
        return TOOL_PRIVATE_ARGS_CONFLICT_ERR;
    }

    return 0;
}

STATIC int tool_get_tc_stat_exec(struct command_context *ctx)
{
    unsigned int size_out = sizeof(struct tc_packet_stat);
    struct tc_packet_stat tc_stat = {0};
    struct dsmi_info info = {0};
    int ret, i;

    ctx->result = (char *)&tc_stat;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size_out);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_TC_PACKET_STATISTICS, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get tc stat info failed, ret:%d", ret), ret);

    TOOL_PRINT_INFO("tc packet statistics:");
    TOOL_PRINT_INFO("tc_id: %u", tc_stat.tc_id);
    for (i = 0; i < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; i++) {
        if (tc_stat.packet_stat[i].is_valid) {
            TOOL_PRINT_INFO("[dev_name: %s]", tc_stat.packet_stat[i].dev_name);
            TOOL_PRINT_INFO("tc_tx_pkt_num: %u", tc_stat.packet_stat[i].tx_num);
            TOOL_PRINT_INFO("tc_rx_pkt_num: %u", tc_stat.packet_stat[i].rx_num);
        } else {
            TOOL_PRINT_ERR("[dev_name: %s] get tc_id %u packet stat failed!", tc_stat.packet_stat[i].dev_name,
                           tc_stat.tc_id);
        }
    }

    return 0;
}

STATIC int tool_set_tc_stat_exec(struct command_context *ctx)
{
    struct tc_stat_cmd_args *tc_args = (struct tc_stat_cmd_args *)ctx->cmd_args;
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    CHK_PRT_RETURN(tc_args == NULL, TOOL_PRINT_ERR("Option -clear or -tc_id <tc_id> is required but missing."),
                   TOOL_PRIVATE_ARGS_VALUE_ERR);

    if (tc_args->mode == TC_SET_ENABLE) {
        ret = tool_double_check("Running this command will reset the tc packet statistics to zero.\n"
                                "Are you sure you want to continue?");
        CHK_PRT_RETURN(ret != 0, hccn_info("User aborts command"), ret);
    }

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)ctx->cmd_args,
                   sizeof(struct tc_stat_cmd_args), NULL, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_TC_PACKET_STATISTICS, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi set tc packet failed, ret:%d", ret), ret);

    return 0;
}

STATIC int tool_tc_stat_exec(struct command_context *ctx)
{
    int ret;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    if (ctx->mode == MODE_GET) {
        return tool_get_tc_stat_exec(ctx);
    } else {
        ret = hccn_check_usr_identify();
        CHK_PRT_RETURN(
            ret != 0,
            hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
            TOOL_PUBLIC_ROOT_UNSUPPORT);

        return tool_set_tc_stat_exec(ctx);
    }

    return 0;
}

STATIC void tool_tc_stat_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_tc_stat_print_help(ctx, MODE_GET);
            tool_tc_stat_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-tc_stat", "get tc packet statistics");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-tc_stat", "set tc packet statistics");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-tc_id <tc_id>", "specify the tc id to enable statistics, range [0, 7]");

            TOOL_PRINT_CMD("-s", "-tc_stat", "set tc packet statistics");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-clear", "clear tc packet statistics");

            break;
        default:
            break;
    }
}

STATIC struct cmd_entry g_shell_cmds[] = {
    {
        "-link",
        get_link_sub_opts,
        MODE_BOTH,
        alloc_link_args_init,
        set_link_assign,
        tool_link_exec,
        {DEV_PORT_TYPE, DEV_DEV_ATTR},
        tool_link_print_help,
        NULL,
        0,
    },
    {
        "-process",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_process_exec,
        {DEV_NPU_ATTR},
        tool_process_print_help,
        NULL,
        0,
    },
    {
        "-bit_err",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_bit_err_exec,
        {DEV_PORT_TYPE},
        tool_bit_err_print_help,
        NULL,
        0,
    },
    {
        "-show_res",
        get_show_res_sub_opts,
        MODE_GET,
        alloc_show_res_args_init,
        show_res_assign,
        tool_show_res_exec,
        {DEV_DEV_ATTR},
        tool_show_res_help,
        NULL,
        0,
    },
    {
        "-show_list",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_show_list_exec,
        {DEV_DEV_ATTR},
        tool_show_list_print_help,
        NULL,
        0,
    },
    {
        "-context",
        get_context_sub_opts,
        MODE_GET,
        alloc_context_args_init,
        context_assign,
        tool_context_exec,
        {DEV_DEV_ATTR},
        tool_context_print_help,
        NULL,
        0,
    },
    {
        "-version",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_version_exec,
        {DEV_NPU_ATTR},
        tool_version_print_help,
        NULL,
        0,
    },
    {
        "-status",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_status_exec,
        {DEV_DEV_ATTR},
        tool_status_print_help,
        NULL,
        0,
    },
    {
        "-port_info",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_port_info_exec,
        {DEV_PORT_TYPE},
        tool_port_info_print_help,
        NULL,
        0,
    },
    {
        "-eth_test",
        get_eth_test_sub_opts,
        MODE_TEST,
        alloc_eth_test_init,
        eth_test_assign,
        tool_eth_test_exec,
        {DEV_DEV_ATTR},
        tool_eth_test_print_help,
        NULL,
        0,
    },
    {
        "-stat",
        get_stat_sub_opts,
        MODE_BOTH,
        alloc_stat_args_init,
        stat_assign,
        tool_stat_exec,
        {DEV_PORT_TYPE},
        tool_stat_print_help,
        NULL,
        0,
    },
    {
        "-peak_bw",
        get_bandwidth_sub_opts,
        MODE_GET,
        alloc_bandwidth_args_init,
        bandwidth_assign,
        tool_peak_bw_exec,
        {DEV_PORT_TYPE},
        tool_peak_bw_print_help,
        NULL,
        0,
    },
    {
        "-mtu",
        get_mtu_sub_opts,
        MODE_BOTH,
        alloc_mtu_args_init,
        set_mtu_assign,
        tool_mtu_exec,
        {DEV_DEV_ATTR},
        tool_mtu_print_help,
        NULL,
        0,
    },
    {
        "-ip",
        get_ip_sub_opts,
        MODE_BOTH,
        alloc_ip_args_init,
        ip_assign,
        tool_ip_exec,
        {DEV_DEV_ATTR},
        tool_ip_print_help,
        NULL,
        0,
    },
    {
        "-gateway",
        get_gateway_sub_opts,
        MODE_BOTH,
        alloc_gateway_args_init,
        gateway_assign,
        tool_gateway_exec,
        {DEV_DEV_ATTR},
        tool_gateway_print_help,
        NULL,
        0,
    },
    {
        "-loopback",
        get_lb_sub_opts,
        MODE_BOTH,
        alloc_lb_args_init,
        lb_assign,
        tool_lb_exec,
        {DEV_PORT_TYPE},
        tool_lb_print_help,
        NULL,
        0,
    },
    {
        "-arp",
        get_arp_sub_opts,
        MODE_BOTH,
        alloc_arp_args_init,
        arp_assign,
        tool_arp_exec,
        {DEV_DEV_ATTR},
        tool_arp_print_help,
        NULL,
        0,
    },
    {
        "-mac",
        get_mac_sub_opts,
        MODE_BOTH,
        alloc_mac_args_init,
        set_mac_assign,
        tool_mac_exec,
        {DEV_DEV_ATTR},
        tool_mac_print_help,
        NULL,
        0,
    },
    {
        "-bond_attr",
        get_bond_attr_sub_opts,
        MODE_BOTH,
        alloc_bond_attr_args_init,
        bond_attr_assign,
        tool_bond_attr_exec,
        {DEV_DEV_ATTR},
        tool_bond_attr_print_help,
        NULL,
        0,
    },
    {
        "-lldp",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_lldp_exec,
        {DEV_DEV_ATTR},
        tool_lldp_print_help,
        NULL,
        0,
    },
    {
        "-pfc",
        get_pfc_sub_opts,
        MODE_BOTH,
        alloc_pfc_args_init,
        pfc_assign,
        tool_pfc_exec,
        {DEV_DEV_ATTR},
        tool_pfc_print_help,
        NULL,
        0,
    },
    {
        "-prio_tc",
        get_prio_tc_sub_opts,
        MODE_BOTH,
        alloc_prio_tc_args_init,
        prio_tc_assign,
        tool_prio_tc_exec,
        {DEV_DEV_ATTR},
        tool_prio_tc_print_help,
        NULL,
        0,
    },
    {
        "-udhcpc",
        get_udhcpc_sub_opts,
        MODE_BOTH,
        alloc_udhcpc_args_init,
        udhcpc_assign,
        tool_udhcpc_exec,
        {DEV_DEV_ATTR},
        tool_udhcpc_print_help,
        NULL,
        0,
    },
    {
        "-dscp_to_tc",
        get_dscp_to_tc_sub_opts,
        MODE_GET,
        alloc_dscp_to_tc_args_init,
        dscp_to_tc_assign,
        tool_dscp_to_tc_exec,
        {DEV_DEV_ATTR},
        tool_dscp_to_tc_print_help,
        NULL,
        0,
    },
    {
        "-tc_stat",
        get_tc_stat_sub_opts,
        MODE_BOTH,
        alloc_tc_stat_args_init,
        tc_stat_assign,
        tool_tc_stat_exec,
        {DEV_DEV_ATTR},
        tool_tc_stat_print_help,
        NULL,
        0,
    },
};

void tool_get_shell_cmds(struct cmd_entry **cmds, unsigned int *count)
{
    *cmds = g_shell_cmds;
    *count = ARRAY_SIZE_COMPUTE(g_shell_cmds);
}
