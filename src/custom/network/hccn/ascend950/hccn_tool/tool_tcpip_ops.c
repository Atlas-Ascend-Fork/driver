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
#include <arpa/inet.h>
#include <netinet/in.h>
#include "securec.h"
#include "ascend_hal.h"
#include "ds_net_interface.h"
#include "dsmi_common_interface.h"
#include "hccn_err.h"
#include "ds_data.h"
#include "dev_info_data.h"
#include "tcpip_cmd_data.h"
#include "tool_dev_info.h"
#include "tool_cmd_err.h"
#include "hccn_err.h"
#include "tool_cfg_ops.h"
#include "tool_log.h"
#include "tool_shell_ops.h"
#include "tool_udis.h"
#include "tool_tcpip_ops.h"

STATIC int tool_vnic_exec(struct command_context *ctx)
{
    char netmask_addr[MAX_IPV4_LEN] = {0};
    ip_addr_t mask_address, ip_address;
    char ip_addr[MAX_IPV4_LEN] = {0};
    unsigned int size = sizeof(int);
    struct dsmi_info info = {0};
    const char *pstr = NULL;
    int link, ret = 0;

    ctx->result = (char *)&link;
    ip_address.ip_type = IPADDR_TYPE_V4;
    mask_address.ip_type = IPADDR_TYPE_V4;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_VNIC_STATUS, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get vnic status failed, ret:%d", ret), ret);

    if (link == LINK_STATUS_UP) {
        TOOL_PRINT_INFO("vnic link status: UP");
    } else {
        TOOL_PRINT_INFO("vnic link status: DOWN");
    }

    ret = dsmi_get_device_ip_address(ctx->npu_info.logic_id, DSMI_VNIC_PORT, 0, &ip_address, &mask_address);
    CHK_PRT_RETURN(
        ret != 0, hccn_err("dsmi_get_device_ip_address failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    pstr = inet_ntop(AF_INET, (void *)&ip_address, ip_addr, MAX_IPV4_LEN);
    CHK_PRT_RETURN(pstr == NULL, hccn_err("ip[0x%x] transfer error", ((struct in_addr *)&ip_address)->s_addr), -EINVAL);

    pstr = inet_ntop(AF_INET, (void *)&mask_address, netmask_addr, MAX_IPV4_LEN);
    CHK_PRT_RETURN(pstr == NULL, hccn_err("netmask[0x%x] transfer error", ((struct in_addr *)&mask_address)->s_addr),
                   -EINVAL);

    TOOL_PRINT_INFO("vnic ipaddr: %s", ip_addr);
    TOOL_PRINT_INFO("vnic netmask: %s", netmask_addr);

    return 0;
}

STATIC void tool_vnic_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_vnic_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-vnic", "get vnic link status, ip address and netmask");
            tool_print_npu_info(DEV_NPU_ATTR);
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry ping_opts_param[] = {
    {"-pkt", ARG_TYPE_INT, parse_int, MODE_TEST, 0, true, .range.int_range = {0, 65507}},
    {"-src_address", ARG_TYPE_IP, parse_ip, MODE_TEST, MODE_TEST},
    {"-dst_address", ARG_TYPE_IP, parse_ip, MODE_TEST, MODE_TEST},
};

STATIC void get_ping_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = ping_opts_param;
    *count = ARRAY_SIZE_COMPUTE(ping_opts_param);
}

STATIC void *alloc_ping_args_init(void)
{
    // Use IPv4 address by default
    struct ping_cmd_args *ping_args = (struct ping_cmd_args *)calloc(1, sizeof(struct ping_cmd_args));

    CHK_PRT_RETURN(ping_args == NULL, hccn_err("ping_args calloc failed"), NULL);
    ping_args->pkt = DEFAULT_PING_PKT_LEN;

    return (void *)ping_args;
}

STATIC int ping_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct ping_cmd_args *ping_args = (struct ping_cmd_args *)args;
    int ret = 0;

    if (strcmp(opt->name, "-src_address") == 0) {
        ret = memcpy_s(&ping_args->src_addr, sizeof(ping_args->src_addr), &type_val->ip, sizeof(type_val->ip));
    } else if (strcmp(opt->name, "-dst_address") == 0) {
        ret = memcpy_s(&ping_args->dst_addr, sizeof(ping_args->dst_addr), &type_val->ip, sizeof(type_val->ip));
    } else if (strcmp(opt->name, "-pkt") == 0) {
        ping_args->pkt = type_val->int_val;
    } else {
        return -EINVAL;
    }

    return ret;
}

STATIC bool is_valid_ping_args(struct ping_cmd_args *ping_args)
{
    int expected_family;

    expected_family = ping_args->is_ipv6_cmd ? AF_INET6 : AF_INET;
    if (is_valid_ip(ping_args->src_addr.str_ip, expected_family) &&
        is_valid_ip(ping_args->dst_addr.str_ip, expected_family)) {
        return true;
    }

    return false;
}

STATIC int tool_ping_exec(struct command_context *ctx)
{
    struct ping_cmd_args *ping_args = (struct ping_cmd_args *)ctx->cmd_args;
    char ping_outbuff[OUTBUF_LEN_1024] = {0};
    unsigned int size = OUTBUF_LEN_1024 - 1;
    struct dsmi_info info = {0};
    int ret;

    ctx->result = (char *)ping_outbuff;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)ctx->cmd_args,
                   sizeof(struct ping_cmd_args), ctx->result, &size);

    if (!is_valid_ping_args(ping_args)) {
        TOOL_PRINT_ERR("IP type matching failed: expected IPv4, got IPv6, src_address: %s dst_address: %s.",
                       ping_args->src_addr.str_ip, ping_args->dst_addr.str_ip);
        return TOOL_PRIVATE_ARGS_VALUE_ERR;
    }

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_PING_INFO, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get ping info failed, ret:%d", ret), ret);

    if (ping_args->is_ipv6_cmd) {
        TOOL_PRINT_INFO("device %d dev %s PING6 %s to %s\n%s", ctx->npu_info.logic_id, ctx->dev_name,
                        ping_args->src_addr.str_ip, ping_args->dst_addr.str_ip, ctx->result);
    } else {
        TOOL_PRINT_INFO("device %d dev %s PING %s to %s\n%s", ctx->npu_info.logic_id, ctx->dev_name,
                        ping_args->src_addr.str_ip, ping_args->dst_addr.str_ip, ctx->result);
    }

    if (strstr(ctx->result, MSG_TOO_LONG) != NULL || strstr(ctx->result, MSG_RECV_TIMEOUT) != NULL ||
        strstr(ctx->result, MSG_DEST_UNREACHABLE) != NULL) {
        hccn_err("ping internal error.");
        return -EINVAL;
    }

    return 0;
}

STATIC void tool_ping_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_ping_print_help(ctx, MODE_TEST);
            break;
        case MODE_TEST:
            TOOL_PRINT_CMD("-t", "-ping", "ICMP ping test");
            tool_print_npu_info(DEV_NPU_ATTR);
            TOOL_PRINT_OPT("-src_address <src_address>", "net detect source IPv4 IP address, such as x.x.x.x");
            TOOL_PRINT_OPT("-dst_address <dst_address>", "net detect destination IP address");
            TOOL_PRINT_OPT("[-pkt <size>]", "the packet data length, default 4096, range [0, 65507]");
            break;
        default:
            break;
    }
}

struct net_health_status_map g_net_health_status[] = {
    {0x0, "Success"}, {0x1, "Socket fail"}, {0x2, "Receive timeout"}, {0x3, "Unreachable"},   {0x4, "Time exceeded"},
    {0x5, "Fault"},   {0x6, "Init"},        {0x7, "Thread error"},    {0x8, "Detect ip set"}, {0x9, "Unknown"}};

STATIC int tool_net_health_exec(struct command_context *ctx)
{
    int status_num = ARRAY_SIZE_COMPUTE(g_net_health_status);
    DSMI_NET_HEALTH_STATUS presult = RDFX_DETECT_INIT;
    int ret;

    ret = tool_udis_get_net_health_exec(ctx, &presult);
    ret = (ret != 0) ? dsmi_get_network_health(ctx->npu_info.logic_id, &presult) : 0;
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("dsmi get net health status failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id), ret);
    CHK_PRT_RETURN((int)presult >= status_num || (int)presult < 0,
                   hccn_err("dsmi get net health status invalid, ret:%d logic_id:%d presult:%d", ret,
                            ctx->npu_info.logic_id, presult),
                   -EINVAL);

    TOOL_PRINT_INFO("net health status: %s", g_net_health_status[(int)presult].status_str);

    return 0;
}

STATIC void tool_net_health_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_net_health_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-net_health", "get net health status");
            tool_print_npu_info(DEV_NPU_ATTR);
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry netdetect_opts_param[] = {
    {"-address", ARG_TYPE_IP, parse_ip, MODE_SET, MODE_SET},
};

STATIC void get_netdetect_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = netdetect_opts_param;
    *count = ARRAY_SIZE_COMPUTE(netdetect_opts_param);
}

STATIC void *alloc_netdetect_args_init(void)
{
    return calloc(1, sizeof(struct netdetect_cmd_args));
}

STATIC int netdetect_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct netdetect_cmd_args *netdetect_args = (struct netdetect_cmd_args *)args;
    int ret = 0;

    if (strcmp(opt->name, "-address") == 0) {
        ret = memcpy_s(&netdetect_args->address, sizeof(netdetect_args->address), &type_val->ip, sizeof(type_val->ip));
    }

    return ret;
}

STATIC int tool_get_netdetect(struct command_context *ctx)
{
    unsigned int size = sizeof(unsigned int);
    char address[MAX_IPV4_LEN] = {0};
    struct dsmi_info info = {0};
    unsigned int ip = 0;
    int ret;

    ctx->result = (char *)&ip;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_NET_DETECT, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get net detect ip address failed, ret:%d", ret), ret);

    ret = tool_ipv4_int_to_str(ip, address, MAX_IPV4_LEN);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool ipv4 int to str failed ret:%d", ret), ret);

    TOOL_PRINT_INFO("netdetect addr: %s", address);

    return 0;
}

STATIC int tool_netdetect_write_conf(struct command_context *ctx)
{
    struct netdetect_cmd_args *netdetect_args = (struct netdetect_cmd_args *)ctx->cmd_args;
    int ret;

    ret = tool_write_conf(ctx->slot_id, "netdetect", netdetect_args->address.str_ip);
    CHK_PRT_RETURN(
        ret != 0, hccn_err("write cfg error, ret:%d slot_id:%d logic_id:%d", ret, ctx->slot_id, ctx->npu_info.logic_id),
        TOOL_WRITE_CONF_ERR);

    return 0;
}

int tool_set_netdetect(struct command_context *ctx)
{
    struct netdetect_cmd_args *netdetect_args = (struct netdetect_cmd_args *)ctx->cmd_args;
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret;

    LOG_PROMPT(ctx, "start to set net detect ip:%s, logic_id:%d", netdetect_args->address.str_ip,
               ctx->npu_info.logic_id);

    CHK_PRT_RETURN(
        !is_valid_ip(netdetect_args->address.str_ip, AF_INET),
        TOOL_PRINT_ERR("IP type matching failed: expected IPv4, got IPv6, addr: %s", netdetect_args->address.str_ip),
        -EINVAL);

    CHK_PRT_RETURN(netdetect_args->address.ip_addr == IP_BROADCAST,
                   TOOL_PRINT_ERR("The IP address is invalid! addr: %s", netdetect_args->address.str_ip), -EINVAL);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)ctx->cmd_args,
                   sizeof(struct netdetect_cmd_args), NULL, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_NET_DETECT, &info);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("dsmi set netdetect ip address failed, ret:%d ip_address:%s", ret, netdetect_args->address.str_ip),
        ret);

    return 0;
}

STATIC int tool_netdetect_exec(struct command_context *ctx)
{
    int ret;

    if (ctx->mode == MODE_GET) {
        return tool_get_netdetect(ctx);
    } else {
        ret = hccn_check_usr_identify();
        CHK_PRT_RETURN(
            ret != 0,
            hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
            TOOL_PUBLIC_ROOT_UNSUPPORT);

        ret = tool_set_netdetect(ctx);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool set netdetect failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
                       ret);

        ret = tool_netdetect_write_conf(ctx);
        CHK_PRT_RETURN(ret != 0,
                       hccn_err("tool netdetect write_conf failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
                       ret);

        return 0;
    }
}

STATIC void tool_netdetect_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_netdetect_print_help(ctx, MODE_GET);
            tool_netdetect_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-netdetect", "get net detect cfg");
            tool_print_npu_info(DEV_NPU_ATTR);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-netdetect", "set net detect cfg");
            tool_print_npu_info(DEV_NPU_ATTR);
            TOOL_PRINT_OPT("-address <netdetect_address>", "set net detect IPv4 ip address, such as x.x.x.x");
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry traceroute_opts_param[] = {
    {"-reset", ARG_TYPE_NONE, parse_none, MODE_TEST, 0},
    {"-m", ARG_TYPE_INT, parse_int, MODE_TEST, 0, true, .range.int_range = {1, 255}},
    {"-tos", ARG_TYPE_INT, parse_int, MODE_TEST, 0, true, .range.int_range = {0, 63}},
    {"-sport", ARG_TYPE_INT, parse_int, MODE_TEST, 0, true, .range.int_range = {0, 65535}},
    {"-sip", ARG_TYPE_IP, parse_ip, MODE_TEST, 0},
    {"-w", ARG_TYPE_INT, parse_int, MODE_TEST, 0, true, .range.int_range = {1, 60}},
    {"-dport", ARG_TYPE_INT, parse_int, MODE_TEST, 0, true, .range.int_range = {0, 65535}},
    {"-dip", ARG_TYPE_IP, parse_ip, MODE_TEST, 0},
};

STATIC void get_traceroute_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = traceroute_opts_param;
    *count = ARRAY_SIZE_COMPUTE(traceroute_opts_param);
}

STATIC void *alloc_traceroute_args_init(void)
{
    return calloc(1, sizeof(struct traceroute_info));
}

STATIC int traceroute_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct traceroute_info *traceroute_args = (struct traceroute_info *)args;

    if (strcmp(opt->name, "-reset") == 0) {
        traceroute_args->reset_flag = true;
        return 0;
    }

    if (strcmp(opt->name, "-m") == 0) {
        traceroute_args->max_ttl = type_val->int_val;
        goto check_reset_flag;
    }

    if (strcmp(opt->name, "-tos") == 0) {
        traceroute_args->tos = type_val->int_val;
        goto check_reset_flag;
    }

    if (strcmp(opt->name, "-sport") == 0) {
        traceroute_args->sport = type_val->int_val;
        goto check_reset_flag;
    }

    if (strcmp(opt->name, "-sip") == 0) {
        (void)memcpy_s(&traceroute_args->src_ip, sizeof(traceroute_args->src_ip), &type_val->ip, sizeof(type_val->ip));
        goto check_reset_flag;
    }

    if (strcmp(opt->name, "-w") == 0) {
        traceroute_args->waittime = type_val->int_val;
        goto check_reset_flag;
    }

    if (strcmp(opt->name, "-dport") == 0) {
        traceroute_args->dport = type_val->int_val;
        goto check_reset_flag;
    }

    if (strcmp(opt->name, "-dip") == 0) {
        (void)memcpy_s(&traceroute_args->dst_ip, sizeof(traceroute_args->dst_ip), &type_val->ip, sizeof(type_val->ip));
        goto check_reset_flag;
    }

check_reset_flag:
    CHK_PRT_RETURN(traceroute_args->reset_flag, TOOL_PRINT_ERR("Option %s conflicts with option -reset.", opt->name),
                   TOOL_PRIVATE_ARGS_CONFLICT_ERR);
    return 0;
}

STATIC int tool_check_traceroute_args(struct command_context *ctx, struct traceroute_info *traceroute_args)
{
    if ((!traceroute_args->reset_flag) && (ctx->sub_opt_mask & SUB_OPT_TRACEROUTE_DIP_MASK) == 0) {
        TOOL_PRINT_ERR("Option -dip <dest_ip> is required but missing.");
        return TOOL_PRIVATE_ARGS_VALUE_ERR;
    }
    if (traceroute_args->reset_flag && (ctx->sub_opt_mask & (~SUB_OPT_TRACEROUTE_RESET_MASK)) != 0) {
        TOOL_PRINT_ERR("Option -reset conflicts with previous options.");
        return TOOL_PRIVATE_ARGS_CONFLICT_ERR;
    }
    return 0;
}

STATIC int tool_traceroute_get_status(struct command_context *ctx, enum traceroute_status *troute_status)
{
    unsigned int size = sizeof(enum traceroute_status);
    struct dsmi_info info = {0};
    int ret = 0;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, (char *)troute_status, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_TRACEROUTE_STATUS, &info);
    CHK_PRT_RETURN(
        ret != 0, hccn_err("dsmi get traceroute status failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    return 0;
}

STATIC int tool_traceroute_reset(struct command_context *ctx)
{
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, NULL, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_RESET_TRACEROUTE, &info);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("tool traceroute reset status failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id),
                   ret);

    return 0;
}

STATIC int tool_traceroute_start(struct command_context *ctx, struct traceroute_info *traceroute_args)
{
    enum traceroute_status troute_status = TRACEROUTE_STATUS_HALT;
    unsigned int size = sizeof(enum traceroute_status);
    struct dsmi_info info = {0};
    int ret = 0;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)traceroute_args,
                   sizeof(struct traceroute_info), (char *)&troute_status, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_START_TRACEROUTE, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi start traceroute failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id),
                   ret);

    if (troute_status == TRACEROUTE_STATUS_RUNNING) {
        return TOOL_TROUTE_PROCESS_EXIST_ERR;
    }

    if (troute_status == TRACEROUTE_STATUS_ERROR) {
        ret = tool_traceroute_reset(ctx);
        CHK_PRT_RETURN(ret != 0,
                       hccn_err("get status_error, try to reset traceroute failed, ret:%d, logic_id:%d", ret,
                                ctx->npu_info.logic_id),
                       ret);
    }

    return 0;
}

STATIC int tool_traceroute_waiting_finish(struct command_context *ctx)
{
    enum traceroute_status status = TRACEROUTE_STATUS_HALT;
    int wait_num = MAX_TRACEROUTE_WAITING_TIMES;
    int ret = 0;

    while (wait_num > 0) {
        ret = tool_traceroute_get_status(ctx, &status);
        CHK_PRT_RETURN(ret != 0,
                       hccn_err("tool traceroute get status failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id),
                       ret);
        if (status == TRACEROUTE_STATUS_HALT) {
            break;
        } else if (status == TRACEROUTE_STATUS_ERROR) {
            return DEV_EXE_TRACEROUTE_ERR;
        }

        wait_num--;
        CHK_PRT_RETURN(wait_num <= 0,
                       hccn_err("tool wait traceroute finish timeout, logic_id:%d", ctx->npu_info.logic_id),
                       DEV_EXE_TRACEROUTE_TIMEOUT);
        sleep(1);
    }

    return 0;
}

STATIC int tool_traceroute_get_output(struct command_context *ctx, char *traceroute_output, unsigned int output_size)
{
    unsigned int size = output_size - 1;
    struct dsmi_info info = {0};
    int ret = 0;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, traceroute_output, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_TRACEROUTE_INFO, &info);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("dsmi get traceroute info failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    size = (unsigned int)strlen(traceroute_output);
    while (size != 0 && (traceroute_output[size - 1] == '\n')) {
        traceroute_output[size - 1] = '\0';
        size = (unsigned int)strlen(traceroute_output);
    }

    return 0;
}

STATIC int tool_traceroute_exec(struct command_context *ctx)
{
    struct traceroute_info *traceroute_args = (struct traceroute_info *)ctx->cmd_args;
    char *traceroute_output = NULL;
    int ret = 0;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    CHK_PRT_RETURN(traceroute_args == NULL, hccn_err("traceroute_args is NULL"), TOOL_PRIVATE_ARGS_NUM_ERR);
    ret = tool_check_traceroute_args(ctx, traceroute_args);
    CHK_PRT_RETURN(ret != 0, hccn_err("traceroute_args is invalid, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id),
                   ret);

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    if (traceroute_args->reset_flag) {
        return tool_traceroute_reset(ctx);
    }

    traceroute_output = (char *)calloc(OUTBUF_LEN_32K, sizeof(char));
    CHK_PRT_RETURN(traceroute_output == NULL, hccn_err("calloc traceroute_output failed"), -ENOMEM);

    ret = tool_traceroute_start(ctx, traceroute_args);
    CHK_PRT_GOTO(ret != 0, hccn_err("tool traceroute start failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id),
                 out);

    ret = tool_traceroute_waiting_finish(ctx);
    CHK_PRT_GOTO(ret != 0,
                 hccn_err("waiting traceroute finish failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id), out);

    ret = tool_traceroute_get_output(ctx, traceroute_output, OUTBUF_LEN_32K);
    CHK_PRT_GOTO(ret != 0,
                 hccn_err("tool traceroute get output failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id), out);

    TOOL_PRINT_INFO("%s", traceroute_output);

out:
    FREE_BUF(traceroute_output);
    return ret;
}

STATIC void tool_traceroute_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_traceroute_print_help(ctx, MODE_TEST);
            break;
        case MODE_TEST:
            TOOL_PRINT_CMD("-t", "-traceroute", "trace the route to destination IP");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("[-m <max_ttl>]",
                           "max number of hops(max TTL to be reached), default is 30, range [1, 255]");
            TOOL_PRINT_OPT("[-tos <tos>]", "TOS value for outgoing packets, default is 0, range [0, 63]");
            TOOL_PRINT_OPT("[-sport <source_port>]", "source port range [0, 65535]");
            TOOL_PRINT_OPT("[-sip <source_ip>]", "source IP address, only support IPv4, such as x.x.x.x");
            TOOL_PRINT_OPT("[-w <wait_time>]",
                           "the number of seconds to wait for response to a probe, default is 3, range [1, 60]");
            TOOL_PRINT_OPT("[-dport <dest_port>]", "destination port range [0, 65535]");
            TOOL_PRINT_OPT("-dip <dest_ip>", "destination IP address, only support IPv4, such as x.x.x.x");

            TOOL_PRINT_CMD("-t", "-traceroute", "trace the route to destination IP");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-reset", "kill all traceroute processes on the specified device and reset status");
            break;
        default:
            break;
    }
}

STATIC struct cmd_entry g_tcpip_cmds[] = {
    {
        "-vnic",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_vnic_exec,
        {DEV_NPU_ATTR},
        tool_vnic_print_help,
        NULL,
        0,
    },
    {
        "-ping",
        get_ping_sub_opts,
        MODE_TEST,
        alloc_ping_args_init,
        ping_assign,
        tool_ping_exec,
        {DEV_NPU_ATTR},
        tool_ping_print_help,
        NULL,
        0,
    },
    {
        "-dev_info",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_info_exec,
        {DEV_NPU_ATTR},
        tool_info_print_help,
        NULL,
        0,
    },
    {
        "-net_health",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_net_health_exec,
        {DEV_NPU_ATTR},
        tool_net_health_print_help,
        NULL,
        0,
    },
    {
        "-netdetect",
        get_netdetect_sub_opts,
        MODE_BOTH,
        alloc_netdetect_args_init,
        netdetect_assign,
        tool_netdetect_exec,
        {DEV_NPU_ATTR},
        tool_netdetect_print_help,
        NULL,
        0,
    },
    {
        "-traceroute",
        get_traceroute_sub_opts,
        MODE_TEST,
        alloc_traceroute_args_init,
        traceroute_assign,
        tool_traceroute_exec,
        {DEV_DEV_ATTR},
        tool_traceroute_print_help,
        NULL,
        0,
    },
};

void tool_get_tcpip_cmds(struct cmd_entry **cmds, unsigned int *count)
{
    *cmds = g_tcpip_cmds;
    *count = ARRAY_SIZE_COMPUTE(g_tcpip_cmds);
}
