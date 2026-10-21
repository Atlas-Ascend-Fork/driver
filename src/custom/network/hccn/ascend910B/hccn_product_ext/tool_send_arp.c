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
#include <getopt.h>
#include <arpa/inet.h>
#include <limits.h>
#include <errno.h>
#include "tool_recovery.h"
#include "tool_ext.h"
#include "cmd.h"
#include "dsmi_common_interface.h"
#include "weak_passwd_dict.h"
#include "tls.h"
#include "tool.h"
#include "log.h"
#include "tool_ops.h"
#include "hccn_dfx_struct.h"

static int tool_get_gratuitous_arp_info(int argc, int logic_id)
{
    struct gratuitous_arp_info arp_info = {0};
    int ret;

    if (argc != 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_gratuitous_arp_info(logic_id, &arp_info);
    if (ret != 0) {
        roce_err("DSMI get the setting of send arp fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    TOOL_PRINT_INFO("The setting of the gratuitous arp:");
    TOOL_PRINT_INFO("current enable status: %s", (arp_info.enable == 1) ? "enable" : "disable");
    TOOL_PRINT_INFO("gratuitous arp period: %u", arp_info.period);

    return ret;
}

static int tool_set_gratuitous_arp_enable(char **argv, int logic_id, struct gratuitous_arp_info *arp_info)
{
    struct gratuitous_arp_info arp_info_tmp = {0};
    char *p_tmp = NULL;
    int ret, len;
    long enable;

    len = strlen(argv[0]);
    enable = strtol(argv[0], &p_tmp, NUMBER_BASE);
    if (len != ONE_VALUE || p_tmp == argv[0] || enable < 0 || enable > 1) {
        roce_err("The value of enable is wrong. [0 or 1]");
        return UDA_PARAM_INVALID_ERR;
    }
    arp_info->enable = (unsigned char)enable;

    ret = dsmi_get_gratuitous_arp_info(logic_id, &arp_info_tmp);
    if (ret != 0) {
        roce_err("DSMI get the setting of send arp fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    arp_info->period = arp_info_tmp.period;

    ret = dsmi_set_gratuitous_arp_info(logic_id, arp_info);
    if (ret != 0) {
        roce_err("DSMI set gratuitous arp status fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return 0;
}

static int tool_set_gratuitous_arp_period(char **argv, int logic_id, struct gratuitous_arp_info *arp_info)
{
    struct gratuitous_arp_info arp_info_tmp = {0};
    long period;
    int ret;
    char *endptr = NULL;

    period = strtol(argv[0], &endptr, NUMBER_BASE);
    if (endptr == NULL || *endptr != '\0') {
        roce_err("The param is invalid!");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    if (period < 1 || period > MAX_ARP_PERIOD) {
        roce_err("The period is out of range. [1 - 3600]");
        return UDA_PARAM_INVALID_ERR;
    }
    arp_info->period = (unsigned short)period;

    ret = dsmi_get_gratuitous_arp_info(logic_id, &arp_info_tmp);
    if (ret != 0) {
        roce_err("DSMI get the setting of send arp fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    arp_info->enable = arp_info_tmp.enable;

    ret = dsmi_set_gratuitous_arp_info(logic_id, arp_info);
    if (ret != 0) {
        roce_err("DSMI set gratuitous arp period fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return 0;
}

static int tool_get_ip_mac_info_for_arp(int logic_id, struct gratuitous_arp_info *arp_info)
{
    ip_addr_t ip_address = {0};
    ip_addr_t mask_address = {0};
    const char *pstr = NULL;
    char *ip_addr = arp_info->ip_addr;
    unsigned char mac_addr[TOOL_MAC_ADDR_LEN] = {0};
    int ret;

    ip_address.ip_type = IPADDR_TYPE_V4;
    mask_address.ip_type = IPADDR_TYPE_V4;
    ret = dsmi_get_device_ip_address(logic_id, 1, 0, &ip_address, &mask_address);
    if (ret != 0) {
        roce_err("Gratuitous arp get device ip fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return (ret == DRV_ERROR_NO_DEVICE) ? UDA_DSMI_IPCONF_NOT_PRESET_ERR : UDA_DSMI_EXE_ERR;
    }

    pstr = inet_ntop(AF_INET, (void *)&ip_address, ip_addr, ARP_ADDR_LEN);
    if (pstr == NULL) {
        roce_err("Ip for arp info transfer error.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = dsmi_get_mac_address(logic_id, 0, mac_addr);
    if (ret != 0) {
        roce_err("Gratuitous arp get device mac fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = sprintf_s(arp_info->mac_addr, ARP_ADDR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0x0], mac_addr[0x1],
                    mac_addr[0x2], mac_addr[0x3], mac_addr[0x4], mac_addr[0x5]);
    if (ret <= 0) {
        roce_err("Sprintf_s mac address fail. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    return 0;
}

static int tool_set_gratuitous_arp_info(int argc, char **argv, struct tool_param *param)
{
    struct gratuitous_arp_info arp_info = {0};
    int logic_id;
    int ret;

    if (argc != ARGC_ID_2) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    logic_id = param->logic_id;

    ret = tool_get_ip_mac_info_for_arp(logic_id, &arp_info);
    if (ret != 0) {
        roce_err("Get ip and mac info for arp fail. (ret=%d)", ret);
        return ret;
    }

    if (!strncmp(argv[0], "period", strlen("period") + 1)) {
        ret = tool_set_gratuitous_arp_period(argv + 1, logic_id, &arp_info);
        if (ret != 0) {
            roce_err("Set the setting of send arp period fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
        if (!check_udhcpc_write_hccn_conf(param, 0)) {
            ret = cfg_write_conf(param->phy_id, "send_arp_period", argv[ARGV_ID_1]);
            if (ret != 0) {
                roce_err("Write arp period cfg error. (ret=%d; phy_id=%d)", ret, param->phy_id);
                return ret;
            }
        }
    } else if (!strncmp(argv[0], "enable", strlen("enable") + 1)) {
        ret = tool_set_gratuitous_arp_enable(argv + 1, logic_id, &arp_info);
        if (ret != 0) {
            roce_err("Set the setting of gratuitous arp enable fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
        if (!check_udhcpc_write_hccn_conf(param, 0)) {
            ret = cfg_write_conf(param->phy_id, "send_arp_status", argv[ARGV_ID_1]);
            if (ret != 0) {
                roce_err("Write arp status cfg error. (ret=%d; phy_id=%d)", ret, param->phy_id);
                return ret;
            }
        }
    } else {
        roce_err("Invalid operation.)");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}

static int recovery_send_arp_period_cfg(struct tool_param *param)
{
    int ret;
    char arp_info[ARP_CFG_STR_LEN + 1] = "";
    char *argv[ARGC_ID_2];
    argv[0] = "period";

    ret = cfg_read_conf(param->phy_id, "send_arp_period", arp_info, sizeof(arp_info));
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            roce_info("The send_arp_period conf name is not found in config file, skipped. (ret=%d)", ret);
            return 0;
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        roce_info("Send arp period need recover cfg. (logic_id=%d)", param->logic_id);
        argv[1] = arp_info;
        ret = tool_set_gratuitous_arp_info(ARGC_ID_2, argv, param);
        if (ret != 0) {
            roce_err("Send arp status set period failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
    }
    return ret;
}

static int recovery_send_arp_status_cfg(struct tool_param *param)
{
    int ret;
    char arp_info[ARP_CFG_STR_LEN + 1] = "";
    char *argv[ARGC_ID_2];

    argv[0] = "enable";
    ret = cfg_read_conf(param->phy_id, "send_arp_status", arp_info, sizeof(arp_info));
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            roce_info("The send_arp_status conf name is not found in config file, skipped. (ret=%d)", ret);
            return 0;
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        roce_info("Send arp status need recover cfg. (logic_id=%d)", param->logic_id);
        argv[1] = arp_info;
        ret = tool_set_gratuitous_arp_info(ARGC_ID_2, argv, param);
        if (ret != 0) {
            roce_err("Send arp status set enable failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        }
    }
    return ret;
}

int tool_cfg_gratuitous_arp_recovery(struct tool_param *param)
{
    int ret;
    char address[MAX_IP_LEN] = "";

    ret = cfg_read_conf(param->phy_id, "udhcpc_enable", address, sizeof(address));
    if (ret == 0) {
        roce_info("Udhcpc enable, do not need recover arp cfg.");
        return 0;
    }
    ret = cfg_read_conf(param->phy_id, "udhcpc_ipv6_enable", address, sizeof(address));
    if (ret == 0) {
        roce_info("[Ipv6]Udhcpc enable, do not need recover arp cfg.");
        return 0;
    }

    ret = recovery_send_arp_period_cfg(param);
    if (ret != 0) {
        roce_err("Recovery send arp period failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }

    ret = recovery_send_arp_status_cfg(param);
    if (ret != 0) {
        roce_err("Recovery send arp status failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }

    return ret;
}

int tool_send_gratuitous_arp_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id;
    int argc_tmp = argc - 1;

    if (argc < ARGV_ID_1) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    logic_id = param->logic_id;
    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_gratuitous_arp_info(argc_tmp, logic_id);
        if (ret != 0) {
            roce_err("Get the setting of send arp fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        ret = tool_set_gratuitous_arp_info(argc_tmp, argv + 1, param);
        if (ret != 0) {
            roce_err("Set the setting of send arp fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else {
        roce_err("Invalid operation.\n)");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}