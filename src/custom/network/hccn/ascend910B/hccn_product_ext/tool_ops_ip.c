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
#include <termios.h>
#include <arpa/inet.h>
#include <limits.h>
#include <errno.h>
#include "tool_recovery.h"
#include "tool_ops.h"
#include "tool_ext.h"
#include "dsmi_common_interface.h"
#include "cmd.h"
#include "cfg.h"
#include "ds_net.h"
#include "ds_net_ext.h"
#include "tls.h"
#include "tool.h"
#include "tool_ops_ip.h"

STATIC int tool_ip_set_delete_ip_route_item(FILE *fp, const char *line_buf, int index, struct tool_param *param,
                                            struct ds_route_table_value *new_ip_info)
{
    int ret;
    char via_ip_s[MAX_PARAM_LEN] = {0};
    unsigned int via_ip, eth_id, ip_mask, table_id;
    unsigned char ip_fst, ip_sed, ip_thd, ip_fth, via_fst, via_sed, via_thd, via_fth;

    ret = sscanf_s(&line_buf[index], "add %hhu.%hhu.%hhu.%hhu/%u via %hhu.%hhu.%hhu.%hhu dev eth%u table %u", &ip_fst,
                   &ip_sed, &ip_thd, &ip_fth, &ip_mask, &via_fst, &via_sed, &via_thd, &via_fth, &eth_id, &table_id);
    if (ret <= 0) {
        roce_err("Sscanf ip route fail. (ret=%d)", ret);
        return UDA_TOOL_PARSE_CONF_FILE_ERR;
    }
    ret = sprintf_s(via_ip_s, sizeof(via_ip_s), "%hhu.%hhu.%hhu.%hhu", via_fst, via_sed, via_thd, via_fth);
    if (ret <= 0) {
        roce_err("Sprintf via_ip_s failed. (ret=%d)", ret);
        return UDA_TOOL_PARSE_CONF_FILE_ERR;
    }
    via_ip = inet_addr(via_ip_s);
    if (via_ip < 0) {
        roce_err("inet_addr via_ip_s INADDR_NONE.");
        return UDA_TOOL_PARSE_CONF_FILE_ERR;
    }

    if ((new_ip_info->address & new_ip_info->netmask) != (via_ip & new_ip_info->netmask)) {
        ret = fseek(fp, 0, SEEK_SET);
        if (ret) {
            roce_err("hccn.conf fseek fd failed! (fd=%d, ret=%d, errno=%d)", fileno(fp), ret, errno);
            return UDA_TOOL_SYS_DELETE_FILE_ERR;
        }

        ret = cfg_inner_del_conf_byfd(fp, line_buf);
        if (ret) {
            roce_err("cfg_inner_del_conf_byfd: delete cfg error! (ret=%d logic_id=%d)", ret, param->logic_id);
            return ret;
        }

        ret = fseek(fp, 0, SEEK_SET);
        if (ret) {
            roce_err("hccn.conf fseek fd failed! (fd=%d, ret=%d, errno=%d)", fileno(fp), ret, errno);
            return UDA_TOOL_SYS_DELETE_FILE_ERR;
        }
    }

    return 0;
}

int tool_trans_config_to_ip_route(FILE *fp, struct tool_param *param, struct ds_route_table_value ip_param)
{
    int ret, index;
    char line_buf[CONLINE_LENGTH] = {0};
    char ip_route_prefix[CONLINE_LENGTH] = {0};

    ret = sprintf_s(ip_route_prefix, CONLINE_LENGTH, "ip_route_%d=", param->phy_id);
    if (ret <= 0) {
        roce_err("sprintf_s ip route net segment prefix failed. (ret=%d)", ret);
        return ret;
    }
    index = strlen(ip_route_prefix);

    while (!feof(fp) && fgets(line_buf, CONLINE_LENGTH, fp) != NULL) {
        if (strstr(line_buf, ip_route_prefix) != NULL) {
            ret = tool_ip_set_delete_ip_route_item(fp, line_buf, index, param, &ip_param);
            if (ret) {
                roce_err("tool_ip_set_delete_ip_route_item failed. (ret=%d)", ret);
                return ret;
            }
        }
    }

    return 0;
}

int tool_check_ip_route_net_segment(struct tool_param *param, const char *address, const char *netmask)
{
    int ret;
    struct ds_route_table_value ip_param = {0};

    if (address == NULL || netmask == NULL) {
        roce_err("address or netmask is NULL");
        return 0;
    }
    ip_param.address = inet_addr(address);
    ip_param.netmask = inet_addr(netmask);

    ret = tool_read_hccn_conf_handle_action(param, READ_TO_DEL_IP_ROUTE_NOT_SAME_SEGMENT, ip_param);
    if (ret) {
        roce_err("tool_check_route_net_segment: tool_read_hccn_conf_handle_action failed! (ret=%d)", ret);
        return ret;
    }
    return 0;
}

int tool_transfer_check_ipaddr(char *address, unsigned int *address_ip)
{
    int ret;

    ret = tool_inet_pton_ipv4_address(address, address_ip);
    if (ret) {
        roce_err("Tool inet pton ipv4 address fail. (ret=%d)", ret);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    if (*address_ip == 0 || *address_ip == UINT_MAX) {
        roce_err("Tool check ipaddr is invalid. (address=%u)", *address_ip);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    return 0;
}

int tool_transfer_check_netmask(char *netmask, unsigned int *netmask_ip)
{
    unsigned int tmp;
    int ret;

    ret = tool_inet_pton_ipv4_address(netmask, netmask_ip);
    if (ret) {
        roce_err("Tool inet pton ipv4 netmask fail. (ret=%d)", ret);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    tmp = ntohl(*netmask_ip);
    if ((tmp | (tmp - 1)) != UINT_MAX) {
        roce_err("Tool check netmask is invalid. (netmask_ip=%u)", *netmask_ip);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    return 0;
}

bool tool_is_910_a3(int logic_id)
{
    int ret;
    unsigned int unsigned_logic_id = (unsigned int)logic_id;
    unsigned int mainboard_id;

    ret = dsmi_get_mainboard_id(unsigned_logic_id, &mainboard_id);
    if (ret) {
        roce_err("get mainboard id fail! (ret=%d; logic_id=%u)", ret, unsigned_logic_id);
        return false;
    }

    return is_ATLAS_900_A3_SUPERPOD(mainboard_id);
}

int is_910_a3_ip_conflict(int logic_id, const char *address, const char *netmask, int is_ipv4)
{
    int ret;
    unsigned int unsigned_logic_id = (unsigned int)logic_id;
    unsigned int mainboard_id;
    unsigned int phy_id;
    ip_addr_t ip_address = {0};
    ip_addr_t mask_address = {0};
    struct ip6_addr ipv6_address = {0};

    // 910_a3，找到当前device对应的另一个DIE
    ret = dsmi_get_phyid_from_logicid(unsigned_logic_id, &phy_id);
    if (ret) {
        roce_err("get phy id from logic id fail! (ret=%d; logic_id=%u)", ret, unsigned_logic_id);
        return ret; // 这个函数不应该失败，失败说明当前device有问题
    }

    phy_id ^= 1; // 对最后一位异或，即0变1，1变0，从而找到该device对应的另一个device
    ret = dsmi_get_logicid_from_phyid(phy_id, &unsigned_logic_id);
    if (ret) {
        roce_info("get logic id from phy id fail! (ret=%d; phy_id=%u)", ret, phy_id);
        return ret; // 允许失败，对方可能不在位或有异常，此时认为其IP也不参与工作，不认为冲突
    }

    if (is_ipv4) {
        return is_dev_ipaddr_conflict(unsigned_logic_id, DSMI_ROCE_PORT, 0, inet_addr(address), inet_addr(netmask));
    } else {
        ret = tool_inet_pton_ipv6_address(address, ipv6_address.address_ip, MAX_IPV6_NUM);
        ret += tool_transfer_check_netmask_ipv6(netmask, ipv6_address.netmask_ip, MAX_IPV6_NUM);
        if (ret) {
            roce_err("translate ipv6 address failed. (ret=%d)", ret);
            return ret; // 正常这里不应该报错，因IPV6地址在之前已被解析过。如果这里报错，不上报IP冲突，而是交给后人拦截非法IP
        }
        return is_dev_ipaddr_conflict_ipv6(unsigned_logic_id, DSMI_ROCE_PORT, 0, ipv6_address.address_ip,
                                           ipv6_address.netmask_ip);
    }
}

// 规避 -route -c 时配置为空的场景;
int tool_route_clear_zero_conf(struct tool_param *param, enum read_route_operation route_op, int route_type)
{
    int ret;
    struct ds_route_table_value route_table_value = {0};
    struct ds_ipv6_route_table_value route_param_v6 = {0};
    char info[MAX_CMD_PAYLOAD_LEN] = {0};

    if (route_op == READ_TO_CLEAR) {
        if (route_type == CLEAR_ROUTE_TYPE_IPV4) {
            route_table_value.address = 0x0;
            route_table_value.netmask = 0x0;
            ret = dsmi_del_route_table(param->logic_id, 0, &route_table_value, info, MAX_CMD_PAYLOAD_LEN);
            if (ret) {
                roce_err("dsmi del route table fail! ret[%d] logic_id[%d]", ret, param->logic_id);
                return dsmi_analysis_dsmi_ret_to_uda(ret);
            }
        }
        if (route_type == CLEAR_ROUTE_TYPE_IPV6) {
            ret = memset_s(&route_param_v6.address, sizeof(route_param_v6.address), 0x0,
                           sizeof(route_param_v6.address));
            if (ret != 0) {
                roce_err("Memset route_param_v6.address failed. (ret=%d)", ret);
                return ret;
            }
            route_param_v6.prefix_length = INET6_PREFIX_MAX;
            ret = dsmi_del_route_table_ipv6(param->logic_id, 0, &route_param_v6, info, MAX_CMD_PAYLOAD_LEN);
            if (ret) {
                roce_err("dsmi del route table fail! ret[%d] logic_id[%d]", ret, param->logic_id);
                return dsmi_analysis_dsmi_ret_to_uda(ret);
            }
        }
    }
    return 0;
}