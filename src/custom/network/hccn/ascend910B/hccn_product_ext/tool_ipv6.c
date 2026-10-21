/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <unistd.h>
#include <sys/utsname.h>
#include "tool_recovery.h"
#include "cmd.h"
#include "tool_ext.h"
#include "dsmi_common_interface.h"
#include "tool.h"
#include "log.h"
#include "tool_ops.h"
#include "tool_ops_ip.h"

int tool_get_route_table_ipv6(int argc, char **argv, struct tool_param *param)
{
    int ret;
    char *info = NULL;

    if (argc != ONE_VALUE) {
        roce_err("tool_route_table_cmd_execute err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    info = (char *)calloc(MAX_ROUTE_TABLE_LEN, sizeof(char));
    if (info == NULL) {
        roce_err("tool_route_table_cmd_execute calloc fail.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = dsmi_get_route_table_ipv6(param->logic_id, 0, info, MAX_ROUTE_TABLE_LEN);
    if (ret) {
        free(info);
        info = NULL;
        roce_err("dsmi get route fail logic_id[%d] port[%d] ret[%d]", param->logic_id, 0, ret);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    info[MAX_ROUTE_TABLE_LEN - 1] = '\0';
    DSMI_PRINT_INFO("Routing table:\n%s", info);

    free(info);
    info = NULL;
    return ret;
}

int tool_check_ipv6_prefix_valid(unsigned int prefix_length)
{
    if (prefix_length > INET6_PREFIX_MAX) {
        return UDA_PARAM_INVALID_ERR;
    }
    return UDA_EXE_SUCCESS;
}

STATIC int tool_check_route_cmd_argv_ipv6(char **argv, struct ds_ipv6_route_table_character *route_char, bool is_add)
{
    int ret;

    ret = tool_check_single_route_cmd_argv(argv + ARGC_ID_1, argv + ARGC_ID_2, "ipv6_address", route_char->address,
                                           sizeof(route_char->address));
    if (ret) {
        roce_err("tool_check_single_route_cmd_argv ipv6_address fail! ret [%d]", ret);
        return ret;
    }

    if (!strncmp(argv[ARGC_ID_3], "prefix_length", strlen("prefix_length") + 1)) {
        route_char->prefix_length = (unsigned int)strtol(argv[ARGC_ID_4], NULL, NUMBER_BASE);
    } else {
        roce_err("err cmd");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    if (is_add) {
        ret = tool_check_single_route_cmd_argv(argv + ARGC_ID_5, argv + ARGC_ID_6, "next_address",
                                               route_char->next_address, sizeof(route_char->next_address));
        if (ret) {
            roce_err("tool_check_single_route_cmd_argv next_address fail! ret [%d]", ret);
            return ret;
        }
    }

    return 0;
}

STATIC bool is_default_ipv6_address(struct in6_addr addr)
{
    char all_zereo_ip[INET6_ADDRSTRLEN] = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
    if (memcmp(addr.s6_addr, all_zereo_ip, IP_SPLIT_BIT_16) == 0) {
        return true;
    } else {
        return false;
    }
}

STATIC int tool_inet_pton_check(struct ds_ipv6_route_table_value *route_param,
                                struct ds_ipv6_route_table_character *route_char, bool use_gateway)
{
    int ret;

    ret = inet_pton(AF_INET6, route_char->address, &route_param->address);
    if (ret <= 0) {
        roce_err("inet_pton ipv6_address fail! ret[%d], address[%s]", ret, route_char->address);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    if (is_default_ipv6_address(route_param->address) || (route_char->prefix_length == 0)) {
        roce_err("invalid params! can not use default route.");
        return UDA_TOOL_COFNIG_DEFAULT_ROUTE_ERR;
    }

    ret = tool_check_ipv6_prefix_valid(route_char->prefix_length);
    if (ret) {
        return ret;
    } else {
        route_param->prefix_length = route_char->prefix_length;
    }

    if (use_gateway) {
        ret = inet_pton(AF_INET6, route_char->next_address, &route_param->next_address);
        if (ret <= 0) {
            roce_err("inet_pton next_address fail! ret[%d], next_address[%s]", ret, route_char->next_address);
            return UDA_PARAM_IP_INVALID_ERR;
        }
    }
    return 0;
}

STATIC int tool_standardize_ipv6(struct ds_ipv6_route_table_value *route_param,
                                 struct ds_ipv6_route_table_character *route_char, bool is_add)
{
    if (inet_ntop(AF_INET6, (void *)&route_param->address, route_char->address, INET6_ADDRSTRLEN) == NULL) {
        roce_err("inet_ntop address fail!");
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    if (is_add) {
        if (inet_ntop(AF_INET6, (void *)&route_param->next_address, route_char->next_address, INET6_ADDRSTRLEN) ==
            NULL) {
            roce_err("inet_ntop next_address fail");
            return UDA_TOOL_INNER_PARAM_ERR;
        }
    }
    return 0;
}

STATIC int get_ipv6_prefix(struct in6_addr *addr, int prefix_len, struct in6_addr *ipv6_prefix)
{
    int i, j;

    if (addr == NULL || ipv6_prefix == NULL) {
        roce_err("get_ipv6_prefix fail! addr or ipv6_prefix is null");
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    for (i = 0; i < prefix_len / IP_SPLIT_BIT_8; i++) {
        ipv6_prefix->s6_addr[i] = addr->s6_addr[i];
    }
    if (prefix_len % IP_SPLIT_BIT_8 != 0) {
        ipv6_prefix->s6_addr[i] = addr->s6_addr[i] & (0xff << (IP_SPLIT_BIT_8 - (prefix_len % IP_SPLIT_BIT_8)));
    }
    for (j = i + 1; j < IP_SPLIT_BIT_16; j++) {
        ipv6_prefix->s6_addr[j] = 0;
    }
    return 0;
}

STATIC int tool_route_param_trans_ipv6(char **argv, int logic_id, struct ds_ipv6_route_table_value *route_param,
                                       struct ds_ipv6_route_table_character *route_char, bool is_add)
{
    int ret;
    struct in6_addr address_prefix = {0};

    ret = tool_check_route_cmd_argv_ipv6(argv, route_char, is_add);
    if (ret) {
        roce_err("tool_check_route_cmd_argv error! ret is:%d", ret);
        return ret;
    }

    ret = tool_inet_pton_check(route_param, route_char, is_add);
    if (ret) {
        roce_err("tool_route_table_inet_pton fail logic_id[%d] address[%s] prefix_length[%u] next_address[%s] ret[%d]",
                 logic_id, route_char->address, route_char->prefix_length, route_char->next_address, ret);
        return ret;
    }

    // ipv6 route工具没有限制目的地址有效位数是否超过前缀长度
    ret = get_ipv6_prefix(&route_param->address, route_param->prefix_length, &address_prefix);
    if (ret) {
        roce_err("get_ipv6_prefix fail. (address=[%s]; prefix_length=[%d]; ret=[%d])", route_char->address,
                 route_char->prefix_length, ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    if (memcmp(route_param->address.s6_addr, address_prefix.s6_addr, IP_SPLIT_BIT_16)) {
        roce_err("address prefix_length check fail. (address=[%s]; prefix_length=[%d])", route_char->address,
                 route_char->prefix_length);
        TOOL_PRINT_ERR("ipv6_address and prefix_length conflict!");
        return UDA_PARAM_IP_INVALID_ERR;
    }

    ret = tool_standardize_ipv6(route_param, route_char, is_add);
    if (ret) {
        roce_err("tool_standardize_ipv6 error! ret is:%d", ret);
        return ret;
    }

    return 0;
}

STATIC int tool_route_trans_conf_ipv6(struct ds_ipv6_route_table_character route_char, char *route_cfg,
                                      unsigned int route_cfg_len, char *next_address, unsigned int next_address_len)
{
    int ret;
    // inet6_address_0=2001:db8::/32
    ret = snprintf_s(route_cfg, route_cfg_len, route_cfg_len - 1, "IPv6route_address_%s/%u", route_char.address,
                     route_char.prefix_length);
    if (ret <= 0) {
        roce_err("snprintf_s route config fail! ret : %d.", ret);
        return -EINVAL;
    }

    // next_address_0=2001:db8::
    ret = snprintf_s(next_address, next_address_len, next_address_len - 1, "next_address_%s", route_char.next_address);
    if (ret <= 0) {
        roce_err("snprintf_s route next_address fail! ret : %d.", ret);
        return -EINVAL;
    }

    return 0;
}

STATIC int tool_get_next_address_from_conf(const char *conf_nxtaddr, char *nxtaddr, unsigned int nxtaddr_len)
{
    int ret = 0;
    char *prefix = "next_address_";
    char *pos;

    pos = strstr(conf_nxtaddr, prefix);
    if (pos != NULL) {
        ret = strcpy_s(nxtaddr, nxtaddr_len, pos + strlen(prefix));
        if (ret) {
            roce_err("strcpy_s nxtaddr failed. (ret=%d)", ret);
            return ret;
        }
        return 0;
    } else {
        roce_err("strstr find next_address failed. ret(%d)", ret);
        return -ENOMEM;
    }
}

STATIC void build_mask_ipv6(unsigned int prefix_length, struct in6_addr *mask)
{
    unsigned int i;
    unsigned int byte;
    unsigned int bit;

    (void)memset_s(mask, sizeof(struct in6_addr), 0, sizeof(struct in6_addr));

    // 逐位设置掩码
    for (i = 0; i < prefix_length; ++i) {
        byte = i / 0x8;
        bit = i % 0x8;
        mask->s6_addr[byte] |= (0x80 >> bit);
    }
}

STATIC int is_network_segment_ipv6(const struct in6_addr address, unsigned int prefix_length)
{
    unsigned int i;
    struct in6_addr mask;
    struct in6_addr host_part;
    struct in6_addr zero;

    // 初始化掩码
    build_mask_ipv6(prefix_length, &mask);

    // 计算主机部分
    for (i = 0; i < MAX_IPV6_NUM; ++i) {
        host_part.s6_addr[i] = address.s6_addr[i] & (~mask.s6_addr[i]);
    }

    // 判断主机部分是否全为 0
    // 初始化一个全 0 的地址
    memset_s(&zero, sizeof(zero), 0, sizeof(zero));

    if (memcmp(&host_part, &zero, sizeof(zero)) == 0) {
        return true;
    } else {
        return false;
    }
}

STATIC bool is_address_gateway_same_segment_ipv6(const struct in6_addr address, unsigned int prefix_length,
                                                 const struct in6_addr gateway)
{
    struct in6_addr mask;
    struct in6_addr addr_network;
    struct in6_addr next_network;

    // 构建掩码
    build_mask_ipv6(prefix_length, &mask);

    // 计算 network address
    for (unsigned int i = 0; i < MAX_IPV6_NUM; ++i) {
        addr_network.s6_addr[i] = address.s6_addr[i] & mask.s6_addr[i];
        next_network.s6_addr[i] = gateway.s6_addr[i] & mask.s6_addr[i];
    }

    // 比较两个网络地址是否相同
    if (memcmp(&addr_network, &next_network, sizeof(struct in6_addr)) == 0) {
        return true;
    } else {
        return false;
    }
}

STATIC int tool_check_route_ipv6(struct tool_param *param, struct ds_ipv6_route_table_value route_param,
                                 struct ds_ipv6_route_table_character route_char, const char *route_cfg,
                                 unsigned int route_cfg_len, ip_addr_t ip_address, ip_addr_t mask_address)
{
    int ret;
    char nxtaddr[INET6_ADDRSTRLEN] = {0};
    char conf_nxtaddr[CONLINE_LENGTH] = {0};
    struct in6_addr address;
    unsigned int prefix_length;

    if (is_default_ipv6_address(route_param.address)) {
        roce_err("Address cannot be default segment");
        return UDA_TOOL_COFNIG_DEFAULT_ROUTE_ERR;
    }

    if (!is_network_segment_ipv6(route_param.address, route_param.prefix_length)) {
        roce_err("Address is not network segment");
        return UDA_TOOL_IP_MISMATCH_MASK_ERR;
    }

    ret = memcpy_s(&address, sizeof(struct in6_addr), ip_address.u_addr.ip6, sizeof(struct in6_addr));
    if (ret) {
        roce_err("memcpy_s ipv6 address fail[%d], dlen[%d], slen[%d]", ret, sizeof(struct in6_addr),
                 sizeof(ip_address.u_addr.ip6));
        return UDA_TOOL_NO_MEM_ERR;
    }
    prefix_length = mask_address.u_addr.ip6[0];

    if (!is_address_gateway_same_segment_ipv6(address, prefix_length, route_param.next_address)) {
        roce_err("Address and gateway are not in the same network segment");
        return UDA_PARAM_DIFF_SEGMT_GTWY_ERR;
    }

    // 虚拟网口地址为ipv4, 不判断冲突
    ret = cfg_read_conf(param->phy_id, route_cfg, conf_nxtaddr, sizeof(conf_nxtaddr));
    if (ret == 0) {
        ret = tool_get_next_address_from_conf(conf_nxtaddr, nxtaddr, INET6_ADDRSTRLEN);
        if (ret) {
            roce_err("tool_check_route_exist err! ret is [%d]", ret);
            return ret;
        } else {
            roce_err("Route already exist in routing table!");
            return UDA_TOOL_ROUTE_ALREADY_EXIST_ERR;
        }
    }

    if (ret != UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
        roce_err("tool_add_route_table cfg_read_conf err! ret is [%d] route_cfg_len is [0x%x]", ret, route_cfg_len);
        return ret;
    }

    return 0;
}

STATIC int tool_add_route_table_ipv6(char **argv, struct tool_param *param, bool is_add)
{
    int ret, temp_ret, port_type = DSMI_ROCE_PORT, port_id = 0;
    char info[MAX_CMD_PAYLOAD_LEN] = {0}, route_cfg[CONLINE_LENGTH] = {0}, next_address[CONLINE_LENGTH] = {0};
    struct ds_ipv6_route_table_value route_param = {{0}};
    struct ds_ipv6_route_table_character route_char = {{0}};
    ip_addr_t ip_address, mask_address;

    LOG_CONFIG("start to add route of logic_id[%d]", param->logic_id);

    ret = tool_route_param_trans_ipv6(argv, param->logic_id, &route_param, &route_char, is_add);
    if (ret) {
        roce_err("tool_route_param_trans failed! ret is: %d, is_add is: %d", ret, is_add);
        return ret;
    }

    ret = tool_route_trans_conf_ipv6(route_char, route_cfg, sizeof(route_cfg), next_address, sizeof(next_address));
    if (ret) {
        roce_err("tool_route_trans_conf failed! ret is %d", ret);
        return ret;
    }

    ip_address.ip_type = IPADDR_TYPE_V6;
    mask_address.ip_type = IPADDR_TYPE_V6;
    ret = dsmi_get_device_ip_address(param->logic_id, port_type, port_id, &ip_address, &mask_address);
    if (ret) {
        roce_err("Dsmi_get_device_ip_address fail ret=%d, logic_id[%d]", ret, param->logic_id);
        ret = (ret == DRV_ERROR_NO_DEVICE) ? UDA_DSMI_IPCONF_NOT_PRESET_ERR : UDA_DSMI_EXE_ERR;
        return ret;
    }

    ret = dsmi_add_route_table_ipv6(param->logic_id, 0, &route_param, info, MAX_CMD_PAYLOAD_LEN);
    if (ret == TOOL_MAX_ROUTE_ROWS) {
        roce_err("Route rows reaches max! Can not add anymore!");
        return UDA_DSMI_ROUTE_ROW_REACH_MAX_ERR;
    }
    if (ret) {
        temp_ret = tool_check_route_ipv6(param, route_param, route_char, route_cfg, sizeof(route_cfg), ip_address,
                                         mask_address);
        if (temp_ret) {
            return temp_ret;
        }
        roce_err("dsmi add route table fail! ret[%d] logic_id[%d] address[%s] prefix_length[%d] next_address[%s]", ret,
                 param->logic_id, route_char.address, route_char.prefix_length, route_char.next_address);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    if (!check_udhcpc_write_hccn_conf(param, 1)) {
        ret = cfg_write_conf(param->phy_id, route_cfg, next_address);
        if (ret) {
            roce_err("add route cfg error! ret[%d] logic_id[%d]", ret, param->logic_id);
            return ret;
        }
    }

    return 0;
}

STATIC int tool_del_route_ipv6_get_next_address_from_conf(struct ds_ipv6_route_table_value *route_param,
                                                          char *conf_nxtaddr, unsigned int conf_length)
{
    char next_address[MAX_IPV6_LEN] = {0};
    int ret;

    ret = tool_get_next_address_from_conf(conf_nxtaddr, next_address, MAX_IPV6_LEN);
    if (ret) {
        roce_err("Func tool_get_next_address_from_conf err! (ret=%d)", ret);
        return ret;
    }

    ret = inet_pton(AF_INET6, next_address, &route_param->next_address);
    if (ret <= 0) {
        roce_err("Tool_del_route_ipv6 call inet_pton next_address fail! (ret=%d; next_address=%s)", ret, next_address);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    return 0;
}

STATIC int tool_del_route_ipv6(struct ds_ipv6_route_table_character route_char,
                               struct ds_ipv6_route_table_value route_param, struct tool_param *param, char *info,
                               unsigned int len)
{
    int ret;
    char route_cfg[CONLINE_LENGTH] = {0};
    char conf_nxtaddr[CONLINE_LENGTH] = {0};

    ret = snprintf_s(route_cfg, CONLINE_LENGTH, CONLINE_LENGTH - 1, "IPv6route_address_%s/%u", route_char.address,
                     route_char.prefix_length);
    if (ret <= 0) {
        roce_err("snprintf_s route config fail! ret : %d.", ret);
        return -EINVAL;
    }

    ret = cfg_read_conf(param->phy_id, route_cfg, conf_nxtaddr, sizeof(conf_nxtaddr));
    if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
        roce_info("Route not exist in routing table, can not delete!");
        return UDA_TOOL_ROUTE_NOT_EXIST_ERR;
    }
    if (ret != 0) {
        roce_err("Func tool_del_route_ipv6 cfg_read_conf err! (ret=%d)", ret);
        return ret;
    }

    ret = tool_del_route_ipv6_get_next_address_from_conf(&route_param, conf_nxtaddr, CONLINE_LENGTH);
    if (ret != 0) {
        roce_err("tool_del_route_ipv6_get_next_address fail. (conf_nxtaddr=%s; ret=%d)", conf_nxtaddr, ret);
        return ret;
    }

    ret = tool_inet_pton_check(&route_param, &route_char, false);
    if (ret) {
        roce_err("tool_route_table_inet_pton fail phy_id[%d] address[%s] prefix_length[%u] next_address[%s] ret[%d]",
                 param->phy_id, route_char.address, route_char.prefix_length, route_char.next_address, ret);
        return ret;
    }

    ret = dsmi_del_route_table_ipv6(param->logic_id, 0, &route_param, info, len);
    if (ret) {
        roce_err("dsmi del route table ipv6 fail! ret[%d] logic_id[%d]", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = cfg_del_conf(param->phy_id, route_cfg);
    if (ret) {
        roce_err("delete route cfg error! ret[%d] phy_id[%d]", ret, param->phy_id);
        return ret;
    }

    return 0;
}

STATIC int tool_del_route_table_ipv6(char **argv, struct tool_param *param, bool is_add)
{
    int ret;
    char info[MAX_CMD_PAYLOAD_LEN] = {0};
    struct ds_ipv6_route_table_value route_param = {0};
    struct ds_ipv6_route_table_character route_char = {0};
    LOG_CONFIG("start to delete route of logic_id[%d]", param->logic_id);

    ret = tool_route_param_trans_ipv6(argv, param->logic_id, &route_param, &route_char, is_add);
    if (ret) {
        roce_err("tool_route_param_trans failed! ret is: %d", ret);
        return ret;
    }

    ret = tool_del_route_ipv6(route_char, route_param, param, info, MAX_CMD_PAYLOAD_LEN);
    if (ret) {
        roce_err("tool_del_route failed! ret is: %d", ret);
        return ret;
    }
    return 0;
}

STATIC int tool_get_route_from_conf_ipv6(char *line_buf, unsigned int *conf_id,
                                         struct ds_ipv6_route_table_character *route_char)
{
    int ret;

    roce_info("tool_get_route_from_conf_ipv6. line is[%s]", line_buf);

    // IPv6route_address_2001:db8::/32_0=next_address_fe80::121b:54ff:fe96:200
    char *address_ptr = strstr(line_buf, "IPv6route_address_");
    if (address_ptr == NULL) {
        roce_err("strstr find route IPv6route_address_ failed. line is[%s]", line_buf);
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    char *prefix_ptr = strstr(address_ptr, "/");
    if (prefix_ptr == NULL) {
        roce_err("strstr find route prefix failed. line is[%s]", line_buf);
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    char *nxtaddr_ptr = strstr(prefix_ptr, "next_address_");
    if (nxtaddr_ptr == NULL) {
        roce_err("strstr find route next_address_ failed. line is[%s]", line_buf);
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    ret = strncpy_s(route_char->address, sizeof(route_char->address), address_ptr + strlen("IPv6route_address_"),
                    prefix_ptr - (address_ptr + strlen("IPv6route_address_")));
    if (ret) {
        roce_err("get address from conf failed. ret[%d]", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    ret = sscanf_s(prefix_ptr, "/%u_%u", &(route_char->prefix_length), conf_id);
    if (ret <= 0) {
        roce_err("get prefix_length from conf failed. ret[%d]", ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    ret = strncpy_s(route_char->next_address, sizeof(route_char->next_address), nxtaddr_ptr + strlen("next_address_"),
                    strlen(nxtaddr_ptr + strlen("next_address_")));
    if (ret) {
        roce_err("get next_address from conf failed. ret[%d]", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    return 0;
}

STATIC int tool_del_route_config_ipv6(char *route_cfg, unsigned int route_cfg_len,
                                      struct ds_ipv6_route_table_character *route_char, struct tool_param *param,
                                      FILE *fp)
{
    int ret;
    ret = snprintf_s(route_cfg, CONLINE_LENGTH, CONLINE_LENGTH - 1, "IPv6route_address_%s/%u_%d=", route_char->address,
                     route_char->prefix_length, param->phy_id);
    if (ret <= 0) {
        roce_err("snprintf_s route config fail! ret : %d. route config length is %u", ret, route_cfg_len);
        return -EINVAL;
    }

    ret = fseek(fp, 0, SEEK_SET);
    if (ret) {
        roce_err("hccn.conf fseek fd[%d] failed! ret[%d] errno[%d]", fileno(fp), ret, errno);
        return UDA_TOOL_SYS_DELETE_FILE_ERR;
    }

    ret = cfg_inner_del_conf_byfd(fp, route_cfg);
    if (ret) {
        roce_err("cfg_inner_del_conf_byfd: delete cfg error! ret[%d] logic_id[%d]", ret, param->logic_id);
        return ret;
    }

    ret = fseek(fp, 0, SEEK_SET);
    if (ret) {
        roce_err("hccn.conf fseek fd[%d] failed! ret[%d] errno[%d]", fileno(fp), ret, errno);
        return UDA_TOOL_SYS_DELETE_FILE_ERR;
    }

    return 0;
}

STATIC int tool_recover_single_route_ipv6(struct ds_ipv6_route_table_character *route_char, struct tool_param *param)
{
    int ret;
    char info[MAX_CMD_PAYLOAD_LEN] = {0};
    struct ds_ipv6_route_table_value route_param = {0};

    ret = tool_inet_pton_check(&route_param, route_char, true);
    if (ret) {
        roce_err("tool_route_table_inet_pton fail logic_id[%d] address[%s] prefix_length[%u] next_address[%s] ret[%d]",
                 param->logic_id, route_char->address, route_char->prefix_length, route_char->next_address, ret);
        return ret;
    }

    ret = dsmi_add_route_table_ipv6(param->logic_id, 0, &route_param, info, MAX_CMD_PAYLOAD_LEN);
    if (ret == TOOL_MAX_ROUTE_ROWS) {
        roce_warn("Route rows reaches max! Can not add anymore!");
        return 0;
    }

    if (ret) {
        roce_err("dsmi add route table fail! ret[%d] logic_id[%d] address[%s] prefix_length[%u] next_address[%s]", ret,
                 param->logic_id, route_char->address, route_char->prefix_length, route_char->next_address);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return 0;
}
STATIC int tool_del_route_by_fd_ipv6(FILE *fp, struct ds_ipv6_route_table_character *route_char,
                                     struct tool_param *param)
{
    int ret;
    char info[MAX_CMD_PAYLOAD_LEN] = {0};
    char route_cfg[CONLINE_LENGTH] = {0};
    struct ds_ipv6_route_table_value route_param = {0};

    ret = tool_inet_pton_check(&route_param, route_char, true);
    if (ret) {
        roce_err("tool_route_table_inet_pton fail logic_id[%d] address[%s] prefix_length[%u] next_address[%s] ret[%d]",
                 param->logic_id, route_char->address, route_char->prefix_length, route_char->next_address, ret);
        return ret;
    }

    ret = dsmi_del_route_table_ipv6(param->logic_id, 0, &route_param, info, MAX_CMD_PAYLOAD_LEN);
    if (ret) {
        roce_err("dsmi del route table fail! ret[%d] logic_id[%d] address[0x%x] netmask[0x%x]", ret, param->logic_id,
                 route_char->address, route_char->prefix_length);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = tool_del_route_config_ipv6(route_cfg, sizeof(route_cfg), route_char, param, fp);
    if (ret) {
        roce_err("tool_del_route_config failed! route_char is %s, ret is %d", route_cfg, ret);
        return ret;
    }

    return 0;
}

STATIC int tool_del_not_segment_route_by_fd_ipv6(FILE *fp, struct ds_ipv6_route_table_character *route_char,
                                                 struct tool_param *param, struct ds_ipv6_route_table_value ip_param)
{
    int ret;
    char route_cfg[CONLINE_LENGTH] = {0};
    struct in6_addr next_address = {0};
    struct in6_addr route_param_prefix = {0};
    struct in6_addr ip_param_prefix = {0};
    struct ds_ipv6_route_table_value route_param = {0};
    char info[MAX_CMD_PAYLOAD_LEN] = {0};

    ret = inet_pton(AF_INET6, route_char->next_address, &next_address);
    if (ret <= 0) {
        roce_err("inet_pton next_address fail. (next_address=[%s]; ret=[%d])", route_char->next_address, ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    ret = get_ipv6_prefix(&next_address, ip_param.prefix_length, &route_param_prefix);
    if (ret) {
        roce_err("get_ipv6_prefix fail. (next_address=[%s]; prefix_length=[%d]; ret=[%d])", route_char->next_address,
                 ip_param.prefix_length, ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    ret = get_ipv6_prefix(&ip_param.address, ip_param.prefix_length, &ip_param_prefix);
    if (ret) {
        roce_err("get_ipv6_prefix fail. (ret=[%d])", ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    if (memcmp(route_param_prefix.s6_addr, ip_param_prefix.s6_addr, IP_SPLIT_BIT_16)) {
        ret = tool_del_route_config_ipv6(route_cfg, sizeof(route_cfg), route_char, param, fp);
        if (ret) {
            roce_err("tool_del_route_config_ipv6 failed. (route_cfg=[%s]; ret=[%d])", route_cfg, ret);
            return ret;
        }

        // ifconfig ipv6行为与ipv4不一致，不会清除路由表，os适配后删除
        ret = tool_inet_pton_check(&route_param, route_char, false);
        if (ret) {
            roce_err("tool_route_table_inet_pton fail logic_id[%d] address[%s] prefix_length[%u] ret[%d]",
                     param->logic_id, route_char->address, route_char->prefix_length, ret);
            return ret;
        }

        ret = dsmi_del_route_table_ipv6(param->logic_id, 0, &route_param, info, MAX_CMD_PAYLOAD_LEN);
        if (ret) {
            roce_err("dsmi del route table fail! ret[%d] logic_id[%d] address[0x%x] netmask[0x%x]", ret,
                     param->logic_id, route_char->address, route_char->prefix_length);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
    }
    return 0;
}

STATIC int tool_select_action_for_route_ipv6(enum read_route_operation route_op, struct tool_param *param, FILE *fp,
                                             struct ds_ipv6_route_table_character *route_char,
                                             struct ds_ipv6_route_table_value ip_param)
{
    int ret;

    switch (route_op) {
        case READ_TO_RECOVERY:
            ret = tool_recover_single_route_ipv6(route_char, param);
            if (ret) {
                roce_err("tool_select_action_for_route_ipv6: tool_recover_single_route_ipv6 failed! ret is [%d]", ret);
                return ret;
            }
            break;
        case READ_TO_CLEAR:
            ret = tool_del_route_by_fd_ipv6(fp, route_char, param);
            if (ret) {
                roce_err("tool_select_action_for_route_ipv6: tool_del_route_by_fd_ipv6 failed! ret is [%d]", ret);
                return ret;
            }
            break;
        case READ_TO_DEL_NOT_SAME_SEGMENT:
            ret = tool_del_not_segment_route_by_fd_ipv6(fp, route_char, param, ip_param);
            if (ret) {
                roce_err("tool_select_action_for_route_ipv6: tool_del_not_segment_route_by_fd_ipv6 failed! ret is [%d]",
                         ret);
                return ret;
            }
            break;
        default:
            break;
    }
    return 0;
}

STATIC int tool_trans_config_to_route_ipv6(FILE *fp, struct tool_param *param, enum read_route_operation route_op,
                                           struct ds_ipv6_route_table_value ip_param)
{
    int ret, route_count = 0;
    unsigned int conf_id = 0;
    char line_buf[CONLINE_LENGTH] = {0};
    unsigned int len_buf;
    struct ds_ipv6_route_table_character route_char = {{0}};

    while (!feof(fp) && fgets(line_buf, CONLINE_LENGTH, fp) != NULL) {
        if ((line_buf[0] == '#') || (strlen(line_buf) < strlen("*=*")) || ((char *)strchr(line_buf, '=') == NULL)) {
            continue;
        }

        if (strncmp(line_buf, "IPv6route", INET6_CONF_PREFIX) != 0) {
            continue;
        }

        len_buf = strlen(line_buf) - 1;
        if ((len_buf < CONLINE_LENGTH) && (line_buf[len_buf] == '\n')) {
            line_buf[len_buf] = '\0';
        }
        ret = tool_get_route_from_conf_ipv6(line_buf, &conf_id, &route_char);
        if (ret) {
            roce_err("get_route_from_conf failed! ret is [%d].", ret);
            return ret;
        }

        if (conf_id != (unsigned int)param->phy_id) {
            continue;
        }

        route_count++;
        ret = tool_select_action_for_route_ipv6(route_op, param, fp, &route_char, ip_param);
        if (ret) {
            roce_err("tool_select_action_for_route failed! ret is [%d]", ret);
            return ret;
        }
    }

    if (route_count == 0) {
        ret = tool_route_clear_zero_conf(param, route_op, CLEAR_ROUTE_TYPE_IPV6);
        if (ret) {
            roce_err("tool_route_clear_zero_conf failed! ret is [%d]", ret);
            return ret;
        }
    }
    return 0;
}

int tool_read_hccn_conf_handle_action_ipv6(struct tool_param *param, enum read_route_operation route_op,
                                           struct ds_ipv6_route_table_value ip_param)
{
    FILE *fp = NULL;
    int ret, ret_val;
    char real_conf_path[PATH_MAX + 1] = {0};

    if ((strlen(CFGFILE_NAME) > PATH_MAX) || (realpath(CFGFILE_NAME, real_conf_path) == NULL)) {
        roce_err("read path_len > PATH_MAX or conf_path is invalid, err[%d]", errno);
        return UDA_TOOL_ACCESS_CONF_FILE_ERR;
    }

    fp = fopen(real_conf_path, "r+");
    if (fp == NULL) {
        roce_err("Open configure file fail errno[%d] real_conf_path[%s]", errno, real_conf_path);
        return UDA_TOOL_SYS_READ_FILE_ERR;
    }

    ret = flock(fileno(fp), LOCK_EX);
    if (ret) {
        roce_err("hccn.conf lock fd[%d] failed! ret[%d] errno[%d]", fileno(fp), ret, errno);
        ret = UDA_TOOL_SYS_READ_FILE_ERR;
        goto out;
    }

    ret = fseek(fp, 0, SEEK_SET);
    if (ret) {
        roce_err("hccn.conf fseek fd[%d] failed! ret[%d] errno[%d]", fileno(fp), ret, errno);
        ret = UDA_TOOL_SYS_READ_FILE_ERR;
        goto out;
    }

    ret = tool_trans_config_to_route_ipv6(fp, param, route_op, ip_param);
out:
    ret_val = fclose(fp);
    if (ret_val) {
        roce_warn("Fclose fail. (ret_val=%d; errno=%d)", ret_val, errno);
    }
    fp = NULL;
    return ret;
}

STATIC int tool_clear_route_table_ipv6(struct tool_param *param, char info[], unsigned int len)
{
    int ret;
    struct ds_ipv6_route_table_value ip_param = {0};

    ret = tool_read_hccn_conf_handle_action_ipv6(param, READ_TO_CLEAR, ip_param);
    if (ret) {
        roce_err("tool_read_hccn_conf: read config and clear failed! ret is [%d]", ret);
        return ret;
    }
    return 0;
}

int tool_check_argc_clear_route_ipv6(int argc, char **argv, struct tool_param *param)
{
    int ret;
    char info[MAX_CMD_PAYLOAD_LEN] = {0};

    if (argc != ONE_VALUE) {
        roce_err("tool_route_table_cmd_execute err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    LOG_CONFIG("start to clear route of logic_id[%d]", param->logic_id);

    ret = tool_clear_route_table_ipv6(param, info, MAX_CMD_PAYLOAD_LEN);
    if (ret) {
        roce_err("tool_clear_route_table fail logic_id[%d] ret[%d]", param->logic_id, ret);
        return ret;
    }
    return ret;
}

int tool_route_table_cmd_execute_ipv6(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc == 0 || argv == NULL) {
        roce_err("tool_route_table_cmd_execute err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_route_table_ipv6(argc, argv, param);
        if (ret) {
            roce_err("tool_get_route_table_ipv6 err argc[%d] ret is [%d]", argc, ret);
            return ret;
        }
    } else if (!strncmp(argv[0], "-a", strlen("-a") + 1)) {
        if (argc != SEVEN_VALUE) {
            roce_err("tool_route_table_cmd_execute add route err argc[%d]", argc);
            return UDA_PARAM_CMD_COUNT_ERR;
        }
        ret = tool_add_route_table_ipv6(argv, param, true);
        if (ret) {
            roce_err("tool_add_route_table_ipv6 fail logic_id[%d] result [%d]", param->logic_id, ret);
            return ret;
        }
    } else if (!strncmp(argv[0], "-d", strlen("-d") + 1)) {
        if (argc != FIVE_VALUE) {
            roce_err("tool_route_table_cmd_execute delete route err argc[%d]", argc);
            return UDA_PARAM_CMD_COUNT_ERR;
        }
        ret = tool_del_route_table_ipv6(argv, param, false);
        if (ret) {
            roce_err("tool_del_route_table_ipv6 fail logic_id[%d] result [%d]", param->logic_id, ret);
            return ret;
        }
    } else if (!strncmp(argv[0], "-c", strlen("-c") + 1)) {
        ret = tool_check_argc_clear_route_ipv6(argc, argv, param);
        if (ret) {
            roce_err("tool_check_argc_clear_route err argc[%d], ret is [%d]", argc, ret);
            return ret;
        }
    } else {
        roce_err("invalid option");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return ret;
}

int tool_get_neigh_table(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct send_data_info data_info = {0};
    struct ds_neigh_table_value cmd_info = {0};

    if (argc != 0) {
        roce_err("Tool get neigh param failed. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    cmd_info.cmd_type = 'g';

    TOOL_SET_SEND_DATA(data_info, DS_NEIGH_TABLE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);
    data_info.out_buf = (char *)malloc(MAX_TRANS_DATA_LEN * sizeof(char));
    if (data_info.out_buf == NULL) {
        roce_err("Tool get neigh malloc out buf failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }
    data_info.size_out = MAX_TRANS_DATA_LEN;

    ret = dsmi_send_data(param->logic_id, 0, &data_info);
    if (ret) {
        free(data_info.out_buf);
        roce_err("Dsmi send data failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        ret = (ret == UDA_TOOL_SYS_BUSY_ERR) ? UDA_TOOL_SYS_BUSY_ERR : dsmi_analysis_dsmi_ret_to_uda(ret);
        return ret;
    }
    data_info.out_buf[MAX_ROUTE_TABLE_LEN - 1] = '\0';
    DSMI_PRINT_INFO("neigh table:");
    DSMI_PRINT_INFO("%s", data_info.out_buf);

    free(data_info.out_buf);
    return 0;
}

int tool_prepare_add_neigh_param(const char *dev, const char *ip, const char *mac,
                                 struct ds_neigh_table_value *cmd_info)
{
    const char *dev_name = NULL;
    unsigned int eth_id;
    int ret;

    cmd_info->cmd_type = 's';
    if (strncmp(dev, "eth", strlen("eth")) != 0) {
        return UDA_PARAM_INVALID_ERR;
    }
    dev_name = dev + strlen("eth");
    eth_id = (unsigned int)strtoul(dev_name, NULL, 0);
    if (eth_id > MAX_DEV_ID) {
        roce_err("Invalid eth id in neigh cmd.");
        return UDA_PARAM_INVALID_ERR;
    }
    cmd_info->eth_id = eth_id;

    ret = inet_pton(AF_INET6, ip, &cmd_info->ip6_address);
    if (ret <= 0) {
        roce_err("Invalid IPv6 address. (ret=%d; errno=%d)", ret, errno);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    ret = tool_mac_str2char(mac, cmd_info->mac_address, TOOL_MAC_ADDR_LEN);
    if (ret != 0) {
        roce_err("Invalid mac_address.");
        return ret;
    }
    return ret;
}

int tool_prepare_del_neigh_param(const char *dev, const char *ip, struct ds_neigh_table_value *cmd_info)
{
    const char *dev_name = NULL;
    unsigned int eth_id;
    int ret;

    cmd_info->cmd_type = 'd';
    if (strncmp(dev, "eth", strlen("eth")) != 0) {
        return UDA_PARAM_INVALID_ERR;
    }
    dev_name = dev + strlen("eth");
    eth_id = (unsigned int)strtoul(dev_name, NULL, 0);
    if (eth_id > MAX_DEV_ID) {
        roce_err("Invalid eth id in arp cmd.");
        return UDA_PARAM_INVALID_ERR;
    }
    cmd_info->eth_id = eth_id;

    ret = inet_pton(AF_INET6, ip, &cmd_info->ip6_address);
    if (ret <= 0) {
        roce_err("Invalid IPv6 address. (ret=%d; errno=%d)", ret, errno);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    return UDA_EXE_SUCCESS;
}

int tool_add_neigh_conf(int logic_id, const char *cmd, const char *dev, const char *ip)
{
#define ARP_ADD_MATCH_CNT 3
    int ret;
    char cmd_buf[CONLINE_LENGTH] = {0};
    char match_list[ARP_ADD_MATCH_CNT][CONLINE_LENGTH];
    unsigned int tmp;

    ret = memset_s(match_list, sizeof(match_list), 0, sizeof(match_list));
    if (ret) {
        roce_err("Add neigh conf memset failed. (ret=%d)", ret);
        return ret;
    }
    ret = sprintf_s(cmd_buf, CONLINE_LENGTH, "IPv6neigh_%d=%s\n", logic_id, cmd);
    if (ret <= 0) {
        roce_err("Add neigh conf sprintf match list failed. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    ret = sprintf_s(match_list[0], CONLINE_LENGTH, "IPv6neigh_%d=", logic_id);
    if (ret <= 0) {
        roce_err("Add neigh conf sprintf match list failed. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    tmp = (unsigned int)strcpy_s(match_list[ARGC_ID_1], CONLINE_LENGTH, dev);
    tmp |= (unsigned int)strcpy_s(match_list[ARGC_ID_2], CONLINE_LENGTH, ip);
    ret = (int)tmp;
    if (ret) {
        roce_err("Add neigh conf strcpy match list failed. (ret=%d)", ret);
        return ret;
    }
    ret = handle_conf_to_file(cmd_buf, FILE_OPT_TYPE_MATCH_REPLACES, match_list, ARP_ADD_MATCH_CNT);
    if (ret) {
        roce_err("Tool add neigh cfg failed. (ret=%d; cmd=\"%s\")", ret, cmd_buf);
    }
    return ret;
}

int tool_add_neigh_table_conf(int logic_id, const char *dev, const char *ip, const char *mac)
{
    int ret;
    char *cmd_buf = NULL;
    cmd_buf = (char *)calloc(INFO_PAYLOAD_LEN, sizeof(char));
    if (cmd_buf == NULL) {
        roce_err("Tool add neigh calloc in buf failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = sprintf_s(cmd_buf, INFO_PAYLOAD_LEN, "-i %s -s %s %s", dev, ip, mac);
    if (ret <= 0) {
        free(cmd_buf);
        roce_err("Tool construct cmdline failed. (ret=%d)", ret);
        return -UDA_TOOL_INNER_PARAM_ERR;
    }

    ret = tool_add_neigh_conf(logic_id, cmd_buf, dev, ip);
    free(cmd_buf);
    return ret;
}

int tool_add_neigh_table(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct send_data_info data_info = {0};
    struct ds_neigh_table_value cmd_info = {0};
    char dev[MAX_IP_LEN] = {0};
    char ip[MAX_IP6_LEN] = {0};
    char mac[MAX_IP_LEN] = {0};

    if (argc != ARGC_ID_6) {
        roce_err("Tool add neigh param failed. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_get_param(argc, argv, "dev", dev, MAX_IP_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "ipv6_address", ip, MAX_IPV6_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "mac", mac, MAX_IP_LEN);
    if (ret != 0) {
        return ret;
    }

    ret = change_ip_form(ip, MAX_IP6_LEN);
    if (ret) {
        roce_err("Add neigh change ip form failed.");
        return ret;
    }
    ret = tool_prepare_add_neigh_param(dev, ip, mac, &cmd_info);
    if (ret) {
        roce_err("Tool add neigh prepare param failed.");
        return ret;
    }

    TOOL_SET_SEND_DATA(data_info, DS_SET_NEIGH_TABLE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);

    ret = dsmi_send_data(param->logic_id, 0, &data_info);
    if (ret) {
        roce_err("Dsmi send data failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        if (ret == EEXIST) {
            return UDA_TOOL_CONF_ALREADY_EXIST_ERR;
        }
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = tool_add_neigh_table_conf(param->phy_id, dev, ip, mac);
    if (ret) {
        roce_err("Add neigh to config file failed. (ret=%d)", ret);
        return ret;
    }

    return 0;
}

int tool_del_neigh_conf(int logic_id, const char *dev, const char *ip)
{
#define NEIGH_DEL_MATCH_CNT 3
    int ret;
    char match_list[NEIGH_DEL_MATCH_CNT][CONLINE_LENGTH];
    unsigned int tmp;

    ret = memset_s(match_list, sizeof(match_list), 0, sizeof(match_list));
    if (ret) {
        roce_err("Del neigh conf memset failed. (ret=%d)", ret);
        return ret;
    }
    ret = sprintf_s(match_list[0], CONLINE_LENGTH, "IPv6neigh_%d=", logic_id);
    if (ret <= 0) {
        roce_err("Del neigh conf sprintf match list failed. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    tmp = (unsigned int)strcpy_s(match_list[ARGC_ID_1], CONLINE_LENGTH, dev);
    tmp |= (unsigned int)strcpy_s(match_list[ARGC_ID_2], CONLINE_LENGTH, ip);
    ret = (int)tmp;
    if (ret) {
        roce_err("Del neigh conf strcpy match list failed. (ret=%d)", ret);
        return ret;
    }
    ret = handle_conf_to_file(NULL, FILE_OPT_TYPE_MATCH_DEL, match_list, NEIGH_DEL_MATCH_CNT);
    if (ret) {
        roce_err("Tool del neigh cfg failed. (ret=%d)", ret);
    }
    return ret;
}

int tool_del_neigh_table(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct send_data_info data_info = {0};
    struct ds_neigh_table_value cmd_info = {0};
    char dev[MAX_IP_LEN] = {0};
    char ip[MAX_IP6_LEN] = {0};

    if (argc != ARGC_ID_4) {
        roce_err("Tool add neigh param failed. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_get_param(argc, argv, "dev", dev, MAX_IP_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "ipv6_address", ip, MAX_IPV6_LEN);
    if (ret != 0) {
        return ret;
    }

    ret = change_ip_form(ip, MAX_IP6_LEN);
    if (ret) {
        roce_err("Del neigh change ip form failed.");
        return ret;
    }
    ret = tool_prepare_del_neigh_param(dev, ip, &cmd_info);
    if (ret) {
        roce_err("Tool del neigh prepare param failed.");
        return ret;
    }

    TOOL_SET_SEND_DATA(data_info, DS_SET_NEIGH_TABLE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);

    ret = dsmi_send_data(param->logic_id, 0, &data_info);
    if (ret) {
        roce_err("Dsmi send data failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        if (ret == ENOENT) {
            return UDA_TOOL_CONF_NOT_EXIST_ERR;
        }
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = tool_del_neigh_conf(param->phy_id, dev, ip);
    if (ret) {
        roce_err("Del neigh from config file failed. (ret=%d)", ret);
        return ret;
    }
    return 0;
}

int tool_neigh_table_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc < PARAM_COUNT_TWO) {
        roce_err("tool_neigh_table_cmd_execute err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (is_ipv6_cmd(&argc, &argv)) {
        if (is_atlas_9000_a3(param)) {
            roce_err("Ipv6 atlas_9000_a3 command is not supported.");
            return UDA_TOOL_OP_NOT_SUPPORT_IPV6_ERR;
        }
        if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
            param->get_cmd_flag = true;
            ret = tool_get_neigh_table(argc - 1, argv + 1, param);
            if (ret) {
                roce_err("Tool_get_neigh_table failed. (ret=%d)", ret);
                return ret;
            }
        } else if (!strncmp(argv[0], "-a", strlen("-a") + 1)) {
            ret = tool_add_neigh_table(argc - 1, argv + 1, param);
            if (ret) {
                roce_err("Tool_add_neigh_table failed. (ret=%d)", ret);
                return ret;
            }
        } else if (!strncmp(argv[0], "-d", strlen("-d") + 1)) {
            ret = tool_del_neigh_table(argc - 1, argv + 1, param);
            if (ret) {
                roce_err("Tool_del_neigh_table failed. (ret=%d)", ret);
                return ret;
            }
        } else {
            roce_err("Arp cmd get option failed.");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    } else {
        roce_err("invalid option");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}

int tool_transfer_check_netmask_ipv6(const char *netmask, unsigned char *netmask_ip, int length)
{
    int ret;
    int mask_int = 0;
    int mask_index = 0;
    mask_int = strtol(netmask, NULL, NUMBER_BASE);
    ret = tool_check_ipv6_prefix_valid(mask_int);
    if (ret != 0) {
        roce_err("ipv6 prefix unvalid. (ret=%d)", ret);
        return ret;
    }
    netmask_ip[mask_index] = (unsigned char)mask_int;
    return UDA_EXE_SUCCESS;
}

int tool_ip_set_address_ipv6(struct tool_param *param, const char *address, const char *netmask)
{
    int ret;
    ip_addr_t ip_address = {0};
    ip_addr_t mask_address = {0};
    struct ip6_addr addr6 = {0};
    ip_address.ip_type = IPADDR_TYPE_V6;
    mask_address.ip_type = IPADDR_TYPE_V6;

    ret = is_ipaddr_conflict_ipv6(param->logic_id, address, netmask);
    if (ret == UDA_PARAM_CONFLICT_IP_ADDR_ERR) {
        roce_err("Ipaddr conflict.");
        return ret;
    } else if ((ret != 0) && (ret != UDA_DSMI_IPCONF_NOT_PRESET_ERR)) {
        roce_err("Ipaddr conflict failed.");
        return ret;
    }

    ret = tool_inet_pton_ipv6_address(address, addr6.address_ip, MAX_IPV6_NUM);
    if (ret) {
        roce_err("Tool set device ipaddr is invalid.");
        return ret;
    }

    ret = tool_transfer_check_netmask_ipv6(netmask, addr6.netmask_ip, MAX_IPV6_NUM);
    if (ret) {
        roce_err("Tool set device netmask ipv6 is invalid.");
        return ret;
    }

    ret = tool_check_route_net_segment_ipv6(param, address, netmask);
    if (ret) {
        roce_err("tool_check_route_net_segment: route not in the same net segment del failed! ret is %d", ret);
        return ret;
    }

    ret = memcpy_s(ip_address.u_addr.ip6, sizeof(ip_address.u_addr.ip6), addr6.address_ip, sizeof(addr6.address_ip));
    if (ret) {
        roce_err("memcpy_s ipv6 address fail[%d], dlen[%d], slen[%d]", ret, sizeof(ip_address.u_addr.ip6),
                 sizeof(addr6.address_ip));
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = memcpy_s(mask_address.u_addr.ip6, sizeof(mask_address.u_addr.ip6), addr6.netmask_ip,
                   sizeof(addr6.netmask_ip));
    if (ret) {
        roce_err("memcpy_s ipv6 mask fail[%d], dlen[%d], slen[%d]", ret, sizeof(mask_address.u_addr.ip6),
                 sizeof(addr6.netmask_ip));
        return UDA_TOOL_NO_MEM_ERR;
    }
    ret = dsmi_set_device_ip_address(param->logic_id, 0x1, 0, ip_address, mask_address);
    if (ret) {
        roce_err("dsmi set ipv6 fail! ret[%d] logic_id[%d]", ret, param->logic_id);
        return UDA_DSMI_EXE_ERR;
    }

    return 0;
}

int tool_ip_get_ipaddr_ipv6(int argc, char *argv[], char *address, char *prefix, int length)
{
    int i, ret;
    int j = 0;
    char *msg = NULL;

    if (address == NULL) {
        roce_err("The address is NULL");
    }

    for (i = 1; i < argc && j < argc; i = i + ARGC_ID_2) {
        j = i + 1;
        msg = (char *)(argv[j]);
        if (!strncmp(argv[i], "ipv6_address", strlen("ipv6_address") + 1)) {
            ret = memcpy_s(address, MAX_IPV6_LEN, msg, strlen(msg) + 1);
            if (ret) {
                roce_err("memcpy_s address ipv6 fail[%d], dlen[%d], slen[%u]", ret, MAX_IPV6_LEN, strlen(msg) + 1);
                return UDA_TOOL_NO_MEM_ERR;
            }
        } else if (!strncmp(argv[i], "prefix_length", strlen("prefix_length") + 1)) {
            ret = memcpy_s(prefix, MAX_IPV6_LEN, msg, strlen(msg) + 1);
            if (ret) {
                roce_err("memcpy_s prefix fail[%d], dlen[%d], slen[%u]", ret, strlen(msg) + 1, strlen(msg) + 1);
                return UDA_TOOL_NO_MEM_ERR;
            }
        } else {
            roce_err("Err input cmd.");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }

    return 0;
}

int is_dev_ipaddr_conflict_ipv6(int logic_id, int port_type, int port_id, unsigned char *address_ip,
                                unsigned char *netmask_ip)
{
    int ret;
    ip_addr_t ip_address = {0};
    ip_addr_t mask_address = {0};
    unsigned char addr_ip6[MAX_IPV6_NUM] = "";

    ip_address.ip_type = IPADDR_TYPE_V6;
    mask_address.ip_type = IPADDR_TYPE_V6;
    ret = dsmi_get_device_ip_address(logic_id, port_type, port_id, &ip_address, &mask_address);
    if (ret) {
        roce_err("dsmi_get_device_ipv6_address fail. (ret=%d; logic_id=%d)", ret, logic_id);
        ret = (ret == DRV_ERROR_NO_DEVICE) ? UDA_DSMI_IPCONF_NOT_PRESET_ERR : UDA_DSMI_EXE_ERR;
        return ret;
    }

    ret = memcpy_s(addr_ip6, MAX_IPV6_NUM, ip_address.u_addr.ip6, sizeof(ip_address.u_addr.ip6));
    if (ret) {
        roce_err("memcpy_s dsmi ipv6 address fail. (ret=%d; dlen=%d; slen=%d)", ret, MAX_IPV6_NUM,
                 sizeof(ip_address.u_addr.ip6));
        return UDA_TOOL_NO_MEM_ERR;
    }

    if (!memcmp(addr_ip6, address_ip, MAX_IPV6_NUM)) {
        return UDA_PARAM_CONFLICT_IP_ADDR_ERR;
    }
    return 0;
}

static bool is_total_dev_ipaddr_conflict_ipv6(int logic_id, int device_start, int device_end, unsigned char *address_ip,
                                              unsigned char *netmask_ip)
{
    int node_id;
    int ret;
    int port_id = 0;
    for (node_id = device_start; node_id < device_end; node_id++) {
        if (node_id == logic_id) {
            continue;
        }
        ret = is_dev_ipaddr_conflict_ipv6(node_id, DSMI_VNIC_PORT, port_id, address_ip, netmask_ip);
        if (ret == UDA_PARAM_CONFLICT_IP_ADDR_ERR) {
            roce_err("IPv6 ipaddr ip conflict.");
            return true;
        }

        ret = is_dev_ipaddr_conflict_ipv6(node_id, DSMI_ROCE_PORT, port_id, address_ip, netmask_ip);
        if (ret == UDA_PARAM_CONFLICT_IP_ADDR_ERR) {
            roce_err("IPv6 ipaddr ip conflict.");
            return true;
        }
    }
    return false;
}

int is_ipaddr_conflict_ipv6(int logic_id, const char *address, const char *netmask)
{
    int ret, end, start = 0;
    unsigned int phy_id;
    struct ip6_addr addr6 = {0};
    bool is_pcie_card = false;

    ret = tool_inet_pton_ipv6_address(address, addr6.address_ip, MAX_IPV6_NUM);
    if (ret) {
        roce_err("Tool set device ipaddr is invalid.");
        return ret;
    }

    ret = tool_transfer_check_netmask_ipv6(netmask, addr6.netmask_ip, MAX_IPV6_NUM);
    if (ret) {
        roce_err("Tool_transfer_check_netmask_ipv6 failed.");
        return ret;
    }

    ret = dsmi_get_device_count(&end);
    if (ret) {
        roce_err("dsmi_get_device_count is invalid.");
        return ret;
    }

    ret = dsmi_get_phyid_from_logicid(logic_id, &phy_id);
    if (ret) {
        roce_err("dsmi_get_phyid_from_logicid is invalid.");
        return ret;
    }

    // 910_a3里，同一个NPU下属的两个DIE不能配置相同的IP
    if (tool_is_910_a3(logic_id)) {
        ret = is_910_a3_ip_conflict(logic_id, address, netmask, 0);
        return ret;
    }

    if (tool_is_pcie_card(logic_id, &is_pcie_card)) {
        roce_err("tool_is_pcie_card ipv6 is invalid.");
        return ret;
    }

    if (is_pcie_card) {
        ret = is_dev_ipaddr_conflict_ipv6(logic_id, DSMI_VNIC_PORT, 0, addr6.address_ip, addr6.netmask_ip);
        // 如果host侧没有配置ipv6, 此时获取不到ipv6无法对比是正常的，只有获取到且对比异常时应当报冲突
        if (ret == UDA_PARAM_CONFLICT_IP_ADDR_ERR) {
            roce_err("pcie is_dev_ipaddr_conflict_ipv6 failed.");
        }
        return ret;
    }

    if (phy_id >= TOOL_DEVICE_CHIP_ID_MAX) {
        start = TOOL_DEVICE_CHIP_ID_MAX;
    } else if (end > TOOL_DEVICE_CHIP_ID_MAX) {
        end = TOOL_DEVICE_CHIP_ID_MAX;
    }
    if (is_total_dev_ipaddr_conflict_ipv6(logic_id, start, end, addr6.address_ip, addr6.netmask_ip)) {
        return UDA_PARAM_CONFLICT_IP_ADDR_ERR;
    }

    return 0;
}

int tool_check_route_net_segment_ipv6(struct tool_param *param, const char *address, const char *netmask)
{
    int ret;
    struct ip6_addr addr6 = {0};
    struct ds_ipv6_route_table_value ip_param = {0};

    ret = inet_pton(AF_INET6, address, &ip_param.address);
    if (ret <= 0) {
        roce_err("inet_pton address fail! ret[%d]", ret);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    ip_param.prefix_length = (unsigned int)strtol(netmask, NULL, NUMBER_BASE);
    ret = tool_read_hccn_conf_handle_action_ipv6(param, READ_TO_DEL_NOT_SAME_SEGMENT, ip_param);
    if (ret != 0) {
        roce_err("tool_read_hccn_conf: read config and clear failed! ret is [%d]", ret);
        return ret;
    }
    return 0;
}

int tool_cfg_gateway_recovery_ipv6(struct tool_param *param)
{
    int ret;
    char gateway[MAX_IPV6_LEN] = "";
    char address[MAX_IPV6_LEN] = "";
    char netmask[MAX_IPV6_LEN] = "";
    unsigned char gateway_ip[MAX_IPV6_NUM] = "";
    struct ds_gateway_v6_addr gw_v6_ip = {0};

    ret = cfg_read_conf(param->phy_id, "udhcpc_ipv6_enable", address, sizeof(address));
    if (ret == 0) {
        roce_info("[IPv6]Udhcpc ipv6 enable, do not need recover gateway cfg, phy_id:[%d]", param->phy_id);
        return 0;
    }

    ret = cfg_read_conf(param->phy_id, "IPv6gateway", gateway, sizeof(gateway));
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            roce_info("The IPv6gateway conf name is not found in config file, skipped. (ret=%d)", ret);
            return 0;
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        ret = tool_inet_pton_ipv6_address(gateway, gateway_ip, MAX_IPV6_NUM);
        if (ret) {
            roce_err("Tool inet pton ipv6 address fail. (ret=%d; gateway_ip=0x%x)", ret, gateway_ip);
            return ret;
        }

        ret = memcpy_s(gw_v6_ip.address, MAX_IPV6_NUM, gateway_ip, MAX_IPV6_NUM);
        if (ret != 0) {
            roce_err("memcpy_s gw ipv6 failed.\n");
            return UDA_TOOL_NO_MEM_ERR;
        }

        ret = dsmi_set_ipv6_gateway_address(param->logic_id, gw_v6_ip);
        if (ret) {
            roce_err("[IPv6]Dsmi set ipv6 default gateway fail! ret[%d] logic_id[%d] gateway_ip[0x%x]", ret,
                     param->logic_id, gateway_ip);
            return UDA_DSMI_EXE_ERR;
        }
    }

    return 0;
}

static int tool_get_ip_conf_ipv6(int phy_id, char *ip, int ip_len, char *mask, int mask_len)
{
    int ret;

    ret = cfg_read_conf(phy_id, "IPv6address", ip, ip_len);
    if (ret) {
        roce_err("read IPv6address fail! (ret=%d; phy_id=%d)", ret, phy_id);
        return ret;
    }

    ret = cfg_read_conf(phy_id, "IPv6netmask", mask, mask_len);
    if (ret) {
        roce_err("read IPv6netmask fail! (ret=%d; phy_id=%d)", ret, phy_id);
        return ret;
    }

    return UDA_EXE_SUCCESS;
}

static int tool_write_ip_conf_ipv6(struct tool_param *param, const char *address, const char *netmask)
{
    int ret;
    ret = cfg_write_conf(param->phy_id, "IPv6address", address);
    if (ret) {
        roce_err("write address cfg error. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return ret;
    }
    ret = cfg_write_conf(param->phy_id, "IPv6netmask", netmask);
    if (ret) {
        roce_err("Write netmask cfg error. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return ret;
    }
    return 0;
}

int tool_check_ip_net_segment_ipv6(const char *sip, const char *dip, const char *netmask)
{
    int ret, netmask_int;
    struct in6_addr src_addr = {0};
    struct in6_addr dst_addr = {0};
    struct in6_addr src_prefix = {0};
    struct in6_addr dst_prefix = {0};

    ret = tool_inet_pton_ipv6_address(sip, src_addr.s6_addr, MAX_IPV6_NUM);
    if (ret == UDA_PARAM_IP_INVALID_ERR) {
        return ret;
    } else if (ret) {
        roce_err("check and inet6 pton source ip fail, ret[%d]", ret);
        return ret;
    }
    ret = tool_inet_pton_ipv6_address(dip, dst_addr.s6_addr, MAX_IPV6_NUM);
    if (ret == UDA_PARAM_IP_INVALID_ERR) {
        return ret;
    } else if (ret) {
        roce_err("check and inet6 pton dest ip fail, ret[%d]", ret);
        return ret;
    }
    netmask_int = (int)strtol(netmask, NULL, NUMBER_BASE);
    ret = get_ipv6_prefix(&src_addr, netmask_int, &src_prefix);
    if (ret) {
        roce_err("get_ipv6_prefix fail. (ret=[%d])", ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    ret = get_ipv6_prefix(&dst_addr, netmask_int, &dst_prefix);
    if (ret) {
        roce_err("get_ipv6_prefix fail. (ret=[%d])", ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    if (memcmp(src_prefix.s6_addr, dst_prefix.s6_addr, MAX_IPV6_NUM)) {
        roce_err("src_ip and dst_ip are not the same net segment!");
        return UDA_PARAM_DIFF_SEGMT_GTWY_ERR;
    }
    return UDA_EXE_SUCCESS;
}

int tool_check_mtu_ipv6(int logic_id)
{
    int ret;
    unsigned int mtu = 0;

    ret = dsmi_get_mtu(logic_id, 0, &mtu);
    if (ret) {
        roce_err("dsmi get mtu fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    if (mtu < TOOL_MIN_MTU_SIZE_IPV6) {
        LOG_CONFIG("Config ipv6 address failed because mtu[%u] of logic_id[%d] is less than %u", mtu, logic_id,
                   TOOL_MIN_MTU_SIZE_IPV6);
        roce_err("config ipv6 address failed because mtu is less than %u. (mtu=%u; logic_id=%d)",
                 TOOL_MIN_MTU_SIZE_IPV6, mtu, logic_id);
        return UDA_TOOL_MTU_TOO_SMALL_FOR_IPV6_ERR;
    }
    return UDA_EXE_SUCCESS;
}

int tool_del_ip_conf_ipv6(int phy_id)
{
    int ret = 0;
    ret = cfg_del_conf(phy_id, "IPv6address");
    if (ret) {
        roce_err("del ipv6 address cfg error! ret[%d] phy_id[%d]", ret, phy_id);
        return ret;
    }
    ret = cfg_del_conf(phy_id, "IPv6netmask");
    if (ret) {
        roce_err("del ipv6 netmask cfg error! ret[%d] phy_id[%d]", ret, phy_id);
        return ret;
    }
    return ret;
}

static int tool_set_ipaddr_recovery_ipv6(struct tool_param *param, const char *address, const char *netmask,
                                         char *gateway, int length)
{
    int ret;
    ret = cfg_read_conf(param->phy_id, "IPv6gateway", gateway, length);
    if (ret == 0) {
        ret = tool_check_ip_net_segment_ipv6(address, gateway, netmask);
        if (ret == UDA_PARAM_DIFF_SEGMT_GTWY_ERR) {
            roce_err("Not same net segment. (ret=%d)", ret);
            ret = cfg_del_conf(param->phy_id, "IPv6gateway");
            if (ret) {
                roce_err("del cfg error! ret[%d] logic_id[%d]", ret, param->logic_id);
                return ret;
            }
        } else if (ret) {
            return ret;
        }
    }
    ret = tool_cfg_gateway_recovery_ipv6(param);
    if (ret) {
        roce_err("recover gateway cfg fail! ret[%d] phy_id[%d]", ret, param->phy_id);
        return ret;
    }
    ret = tool_cfg_route_ipv6_recovery(param);
    if (ret) {
        roce_err("recover route cfg fail[%d], phy_id[%d]", ret, param->phy_id);
        return ret;
    }
    ret = tool_write_ip_conf_ipv6(param, address, netmask);
    if (ret) {
        roce_err("Write cfg error. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }
    return 0;
}

static int tool_set_device_ipaddr_ipv6(int argc, char **argv, struct tool_param *param)
{
    int ret;
    char address[MAX_IPV6_LEN] = {0};
    char netmask[MAX_IPV6_LEN] = {0};
    char gateway[MAX_IPV6_LEN] = {0};
    ret = tool_ip_get_ipaddr_ipv6(argc, argv, address, netmask, MAX_IPV6_LEN);
    if (ret) {
        roce_err("Tool ip get ipaddr fail. (ret=%d)", ret);
        return ret;
    }

    ret = tool_check_mtu_ipv6(param->logic_id);
    if (ret) {
        roce_err("check mtu failed when config ipv6. (ret=%d)", ret);
        return ret;
    }
    ret = tool_ip_set_address_ipv6(param, address, netmask);
    if (ret) {
        roce_err("Ip set address fail! (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }
    ret = tool_set_ipaddr_recovery_ipv6(param, address, netmask, gateway, MAX_IPV6_LEN);
    if (ret) {
        roce_err("tool recover after set ipaddr err! ret is [%d]", ret);
        return ret;
    }
    roce_info("Set logic_id[%d] ip success!", param->logic_id);
    return UDA_EXE_SUCCESS;
}

int tool_get_device_ipaddr_ipv6(struct tool_param *param)
{
    int ret;
    char address[MAX_IPV6_LEN] = {0};
    unsigned char prefix_length = 0;

    ret = tool_get_ipaddr_ipv6_from_device(param, address, &prefix_length, sizeof(address));
    if (ret) {
        roce_err("Tool get ipaddr ipv6 from device fail ret=%d, logic_id[%d]", ret, param->logic_id);
        return ret;
    }

    DSMI_PRINT_INFO("ipv6_address:%s", address);
    DSMI_PRINT_INFO("prefix_length:%u", prefix_length);
    return UDA_EXE_SUCCESS;
}

int tool_get_ipaddr_ipv6_from_device(struct tool_param *param, char *address, unsigned char *prefix_length,
                                     int ipv6_len)
{
    int ret;
    int port_type = DSMI_ROCE_PORT;
    int port_id = 0;
    ip_addr_t ip_address = {0};
    ip_addr_t mask_address = {0};
    const char *pstr = NULL;

    ip_address.ip_type = IPADDR_TYPE_V6;
    mask_address.ip_type = IPADDR_TYPE_V6;
    ret = dsmi_get_device_ip_address(param->logic_id, port_type, port_id, &ip_address, &mask_address);
    if (ret) {
        roce_err("dsmi_get_device_ip6_address fail ret=%d, logic_id[%d]", ret, param->logic_id);
        ret = (ret == DRV_ERROR_NO_DEVICE) ? UDA_DSMI_IPCONF_NOT_PRESET_ERR : UDA_DSMI_EXE_ERR;
        return ret;
    }

    pstr = inet_ntop(AF_INET6, (void *)ip_address.u_addr.ip6, address, ipv6_len);
    if (pstr == NULL) {
        roce_err("ip transfer error");
        return UDA_TOOL_NO_MEM_ERR;
    }
    *prefix_length = mask_address.u_addr.ip6[0];
    return UDA_EXE_SUCCESS;
}

int tool_ip_cmd_execute_ipv6(int argc, char **argv, struct tool_param *param)
{
    int ret;
    if (argc == 0) {
        roce_err("err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        if (argc != ARGC_ID_5) {
            roce_err("err ipv6 set cmd argc[%d]", argc);
            return UDA_PARAM_CMD_COUNT_ERR;
        }
        ret = tool_set_device_ipaddr_ipv6(argc, argv, param);
        if (ret) {
            roce_err("set device [%d] ip fail", param->logic_id);
            return ret;
        }
        return UDA_EXE_SUCCESS;
    } else if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        if (argc != ARGC_ID_1) {
            roce_err("err ipv6 get cmd argc[%d]", argc);
            return UDA_PARAM_CMD_COUNT_ERR;
        }
        ret = tool_get_device_ipaddr_ipv6(param);
        if (ret) {
            roce_err("get logic_id [%d] ip fail", param->logic_id);
            return ret;
        }
        return UDA_EXE_SUCCESS;
    } else {
        roce_err("Invalid option.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

STATIC int tool_ip_get_gateway_ipv6(int argc, char *argv[], char *gateway)
{
    int i, ret;
    int j = 0;
    char *msg = NULL;

    for (i = 1; i < argc && j < argc; i = i + ARGC_ID_2) {
        j = i + 1;
        msg = (char *)(argv[j]);
        if (!strncmp(argv[i], "ipv6_gateway", strlen("ipv6_gateway") + 1)) {
            ret = memcpy_s(gateway, MAX_IPV6_LEN, msg, strlen(msg) + 1);
            if (ret) {
                roce_err("memcpy_s gateway ipv6 fail[%d], dlen[%d], slen[%u]", ret, MAX_IPV6_LEN, strlen(msg) + 1);
                return UDA_TOOL_NO_MEM_ERR;
            }
        } else {
            roce_err("Err input cmd.");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }

    return UDA_EXE_SUCCESS;
}

STATIC bool is_dev_conf_gateway_ipv6(int phy_id, int logic_id, struct default_gateway_info *default_gtw)
{
    int ret;
    char gateway_inner[MAX_IPV6_LEN] = "";
    ret = cfg_read_conf(phy_id, "IPv6gateway", gateway_inner, sizeof(gateway_inner));
    if (ret) {
        roce_info("gateway do not need recover cfg [%d]", ret);
        return false;
    } else {
        ret = memcpy_s(default_gtw->gateway, MAX_IPV6_LEN, gateway_inner, sizeof(gateway_inner));
        if (ret) {
            roce_err("memcpy_s dsmi ipv6 address fail[%d], dlen[%d], slen[%d]", ret, sizeof(default_gtw->gateway),
                     sizeof(gateway_inner));
            return false;
        }
        default_gtw->logic_id = logic_id;
        return true;
    }
    return false;
}

STATIC bool is_total_dev_exist_gateway_ipv6(int phy_id, int logic_id, int device_start, int device_end,
                                            struct default_gateway_info *default_gtw)
{
    int node_id;
    for (node_id = device_start; node_id < device_end; node_id++) {
        if (is_dev_conf_gateway_ipv6(phy_id, node_id, default_gtw)) {
            return true;
        }
    }
    return false;
}

STATIC bool tool_exist_default_gateway(int logic_id, struct default_gateway_info *default_gtw)
{
    int ret, end;
    unsigned int phy_id;
    bool is_pcie_card = false;
    int start = 0;
    ret = dsmi_get_device_count(&end);
    if (ret) {
        roce_err("dsmi_get_device_count fail");
        return false;
    }

    ret = dsmi_get_phyid_from_logicid(logic_id, &phy_id);
    if (ret) {
        roce_err("dsmi_get_phyid_from_logicid fail");
        return false;
    }

    if (tool_is_pcie_card(logic_id, &is_pcie_card)) {
        roce_err("tool_is_pcie_card fail");
        return false;
    }
    if (is_pcie_card) {
        if (is_dev_conf_gateway_ipv6(phy_id, logic_id, default_gtw)) {
            return true;
        }
    }
    if (phy_id >= TOOL_DEVICE_CHIP_ID_MAX) {
        start = TOOL_DEVICE_CHIP_ID_MAX;
    } else if (end > TOOL_DEVICE_CHIP_ID_MAX) {
        end = TOOL_DEVICE_CHIP_ID_MAX;
    }
    if (is_total_dev_exist_gateway_ipv6(phy_id, logic_id, start, end, default_gtw)) {
        return true;
    }
    return false;
}

STATIC int tool_del_gateway_ipv6(int logic_id, int port_type, int port_id, ip_addr_t *gateway_address)
{
    int ret;
    ret = cfg_del_conf(logic_id, "IPv6gateway");
    if (ret) {
        roce_err("del cfg error! ret[%d] phy_id[%d]", ret, logic_id);
        return ret;
    }
    return UDA_EXE_SUCCESS;
}

int tool_transfer_check_ipaddr_ipv6(const char *address, unsigned char *address_ip, int length)
{
    int ret;
    ret = tool_inet_pton_gateway_address(address, address_ip, length);
    if (ret) {
        roce_err("Tool inet pton ipv6 address fail. (ret=%d)", ret);
        return UDA_PARAM_IP_INVALID_ERR;
    }
    return UDA_EXE_SUCCESS;
}

int tool_clean_default(int logic_id, int port_type, int port_id, const char *gateway)
{
    int ret;
    struct default_gateway_info default_gtw = {0};
    default_gtw.logic_id = logic_id;
    ip_addr_t gtw_address = {0};
    unsigned char gateway_ip[MAX_IPV6_NUM] = {0};
    gtw_address.ip_type = IPADDR_TYPE_V6;
    if (tool_exist_default_gateway(logic_id, &default_gtw)) {
        if (!memcmp(gateway, default_gtw.gateway, MAX_IPV6_NUM) && (logic_id == default_gtw.logic_id)) {
            ret = tool_transfer_check_ipaddr_ipv6(default_gtw.gateway, gateway_ip, MAX_IPV6_NUM);
            if (ret) {
                roce_err("Tool set device gateway is invalid.");
                return ret;
            }
            ret = dsmi_get_gateway_addr(default_gtw.logic_id, port_type, port_id, &gtw_address);
            if (ret) {
                roce_err("dsmi_get_device_ip6_address fail ret=%d, logic_id[%d]", ret, default_gtw.logic_id);
                return UDA_DSMI_EXE_ERR;
            }
            if (!memcmp(gtw_address.u_addr.ip6, gateway_ip, MAX_IPV6_NUM)) {
                return 0;
            }
        }
        ret = tool_del_gateway_ipv6(default_gtw.logic_id, port_type, port_id, &gtw_address);
        if (ret) {
            roce_err("tool_del_gateway_ipv6 fail ret=%d, default_gtw.logic_id[%d]", ret, default_gtw.logic_id);
            return ret;
        }
    }
    return UDA_EXE_SUCCESS;
}

STATIC int tool_write_gateway_conf_ipv6(struct tool_param *param, const char *gateway)
{
    int ret;
    ret = cfg_write_conf(param->phy_id, "IPv6gateway", gateway);
    if (ret) {
        roce_err("write address cfg error. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return ret;
    }
    return UDA_EXE_SUCCESS;
}

int tool_ip_set_gateway_ipv6(struct tool_param *param, int logic_id, const char *gateway)
{
    int ret;
    unsigned char gateway_ip[MAX_IPV6_NUM] = "";
    ip_addr_t gateway_address = {0};
    struct ds_gateway_v6_addr gw_v6_ip = {0};
    gateway_address.ip_type = IPADDR_TYPE_V6;
    ret = tool_transfer_check_ipaddr_ipv6(gateway, gateway_ip, MAX_IPV6_NUM);
    if (ret) {
        roce_err("Tool set device gateway is invalid.");
        return ret;
    }
    ret = memcpy_s(gateway_address.u_addr.ip6, sizeof(gateway_address.u_addr.ip6), gateway_ip, MAX_IPV6_NUM);
    if (ret) {
        roce_err("memcpy_s dsmi ipv6 address fail[%d], dlen[%d], slen[%d]", ret, sizeof(gateway_address.u_addr.ip6),
                 sizeof(gateway_ip));
        return UDA_TOOL_NO_MEM_ERR;
    }
    ret = tool_clean_default(logic_id, 0x1, 0, gateway);
    if (ret) {
        roce_err("tool_clean_default fail! ret[%d] logic_id[%d] gateway[0x%x]", ret, logic_id, gateway_ip);
        return UDA_DSMI_EXE_ERR;
    }

    ret = memcpy_s(gw_v6_ip.address, MAX_IPV6_NUM, gateway_address.u_addr.ip6, MAX_IPV6_NUM);
    if (ret != 0) {
        roce_err("memcpy_s gw ipv6 failed.\n");
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = dsmi_set_ipv6_gateway_address(logic_id, gw_v6_ip);
    if (ret) {
        roce_err("dsmi set default gw fail! ret[%d] logic_id[%d] gateway_ip[%s]", ret, logic_id,
                 gateway_address.u_addr.ip6);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    if (!check_udhcpc_write_hccn_conf(param, 1)) {
        ret = tool_write_gateway_conf_ipv6(param, gateway);
        if (ret) {
            roce_err("Write cfg error. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    }
    return UDA_EXE_SUCCESS;
}

STATIC int tool_set_device_gateway_ipv6(int argc, char **argv, struct tool_param *param)
{
    int ret;
    char address[MAX_IPV6_LEN] = {0};
    char netmask[MAX_IPV6_LEN] = {0};
    char gateway[MAX_IPV6_LEN] = {0};
    unsigned char prefix_length = 0;
    int ipv6_len = MAX_IPV6_LEN;

    ret = tool_ip_get_gateway_ipv6(argc, argv, gateway);
    if (ret) {
        roce_err("Tool ip get ipaddr fail. (ret=%d)", ret);
        return ret;
    }

    ret = tool_get_ipaddr_ipv6_from_device(param, address, &prefix_length, ipv6_len);
    if (ret) {
        roce_err("tool_get_ipaddr_ipv6_from_device fail ret=%d, logic_id[%d]", ret, param->logic_id);
        ret = (ret == UDA_DSMI_IPCONF_NOT_PRESET_ERR) ? UDA_TOOL_GATEWAY_NO_IP_CONF_ERR : ret;
        return ret;
    }
    ret = sprintf_s(netmask, MAX_IPV6_LEN, "%u", prefix_length);
    if (ret <= 0) {
        roce_err("Sprintf netmask cfg size failed. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = tool_check_ip_net_segment_ipv6(address, gateway, netmask);
    if (ret) {
        roce_err("Not the same net segment. (ret=%d)", ret);
        return ret;
    }
    ret = tool_ip_set_gateway_ipv6(param, param->logic_id, gateway);
    if (ret) {
        roce_err("Tool ip set gateway fail. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }
    roce_info("Set logic_id[%d] ip success!", param->logic_id);
    return UDA_EXE_SUCCESS;
}

int tool_inet_pton_gateway_address(const char *str, unsigned char *ipv6, int length)
{
    struct ip6_addr addr = {0};
    int ret;

    if (str == NULL || ipv6 == NULL) {
        roce_err("Input param is NULL.");
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    ret = inet_pton(AF_INET6, str, addr.address_ip);
    if (ret <= 0) {
        roce_err("Invalid IPv6 address. (ret=%d; errno=%s)", ret, addr.address_ip);
        return UDA_PARAM_IP_INVALID_ERR;
    }
    ret = memcpy_s(ipv6, length, addr.address_ip, MAX_IPV6_NUM);
    if (ret) {
        roce_err("memcpy_s dsmi ipv6 address fail[%d], dlen[%d], slen[%d]", ret, length, sizeof(addr.address_ip));
        return UDA_TOOL_NO_MEM_ERR;
    }
    return UDA_EXE_SUCCESS;
}

STATIC int tool_get_device_gateway_ipv6(struct tool_param *param)
{
    int ret;
    int port_type = DSMI_ROCE_PORT;
    int port_id = 0;
    struct ds_gateway_v6_addr gtw_address = {0};
    const char *pstr = NULL;
    char gateway[MAX_IPV6_LEN] = {0};

    ret = dsmi_get_ipv6_gateway_address(param->logic_id, &gtw_address);
    if (ret == -ENOENT) {
        roce_err("dsmi_get_gateway_addr fail ret=%d, logic_id[%d]", ret, param->logic_id);
        return UDA_DSMI_GATEWAY_NOT_PRESET_ERR;
    } else if (ret) {
        roce_err("dsmi_get_gateway_addr fail ret=%d, logic_id[%d]", ret, param->logic_id);
        return UDA_DSMI_EXE_ERR;
    }
    pstr = inet_ntop(AF_INET6, gtw_address.address, gateway, sizeof(gateway));
    if (pstr == NULL) {
        roce_err("gateway transfer error");
        return UDA_TOOL_NO_MEM_ERR;
    }
    DSMI_PRINT_INFO("gateway:%s", gateway);
    return UDA_EXE_SUCCESS;
}

int tool_gateway_cmd_execute_ipv6(int argc, char **argv, struct tool_param *param)
{
    int ret;
    if (argc == 0) {
        roce_err("err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        if (argc != ARGC_ID_3) {
            roce_err("err ipv6 set cmd argc[%d]", argc);
            return UDA_PARAM_CMD_COUNT_ERR;
        }
        ret = tool_set_device_gateway_ipv6(argc, argv, param);
        if (ret) {
            roce_err("set device [%d] ip fail", param->logic_id);
            return ret;
        }
        return UDA_EXE_SUCCESS;
    } else if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        if (argc != ARGC_ID_1) {
            roce_err("err ipv6 get cmd argc[%d]", argc);
            return UDA_PARAM_CMD_COUNT_ERR;
        }
        ret = tool_get_device_gateway_ipv6(param);
        if (ret) {
            roce_err("get logic_id [%d] ip fail", param->logic_id);
            return ret;
        }
        return UDA_EXE_SUCCESS;
    } else {
        roce_err("Invalid option.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return UDA_EXE_SUCCESS;
}

int tool_del_ipv6_conf_after_set_mtu(int phy_id, unsigned int mac_type, unsigned int mtu_size)
{
    int ret = 0;
    if (mac_type == MAC_TYPE_ETH && mtu_size < TOOL_MIN_MTU_SIZE_IPV6) {
        if (!g_recovering) { // 配置恢复的时候不删除配置，仅在正常命令下发且用户二次确认后删除ipv6配置
            ret = tool_del_ip_conf_ipv6(phy_id);
            if (ret) {
                roce_err("del ipv6 address cfg error when config mtu! ret[%d] phy_id[%d]", ret, phy_id);
                return ret;
            }
        }
    }
    return ret;
}