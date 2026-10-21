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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <limits.h>
#include <float.h>
#include <errno.h>
#include <netinet/in.h>
#include "tool_recovery.h"
#include "tool_ext.h"
#include "cmd.h"
#include "cfg.h"
#include "dsmi_common_interface.h"
#include "weak_passwd_dict.h"
#include "tls.h"
#include "tool.h"
#include "log.h"
#include "ds_net_ext.h"
#include "hccn_dfx_struct.h"

#define PRI_NUM 8

STATIC int tool_get_pfc(int argc, char **argv, struct tool_param *param)
{
    int ret;
    char pfc_enabled[PRIORITY_NUM] = {0};
    if (argc != 1) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_pfc_enabled(param->logic_id, 0, pfc_enabled);
    if (ret) {
        roce_err("Dsmi get pfc enabled failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    TOOL_PRINT_INFO("PFC configuration:");
    TOOL_PRINT_INFO("priority   0 1 2 3 4 5 6 7");
    TOOL_PRINT_INFO("enabled    %d %d %d %d %d %d %d %d", pfc_enabled[0x0], pfc_enabled[0x1], pfc_enabled[0x2],
                    pfc_enabled[0x3], pfc_enabled[0x4], pfc_enabled[0x5], pfc_enabled[0x6], pfc_enabled[0x7]);
    return 0;
}

STATIC int tool_pfc_str2char(const char pfc_str[], int str_len, char pfc_enabled[PRIORITY_NUM])
{
    int priority_i = 0;
    int count = 0;
    int str_i;

#define PFC_CONF_INDEX 2
    for (str_i = 0; str_i < str_len && priority_i < PRIORITY_NUM; str_i++) {
        if (str_i % PFC_CONF_INDEX == 1) {
            if (pfc_str[str_i] != ',') {
                roce_err("Tool_pfc_str2char contain invalid char[%c]", pfc_str[str_i]);
                return UDA_PARAM_INVALID_ERR;
            }
        } else if ((str_i % PFC_CONF_INDEX == 0) && (pfc_str[str_i] != '0') && (pfc_str[str_i] != '1')) {
            roce_err("Tool_pfc_str2char contain invalid char[%c]", pfc_str[str_i]);
            return UDA_PARAM_INVALID_ERR;
        } else {
            pfc_enabled[priority_i] = pfc_str[str_i] - '0';
            if (pfc_enabled[priority_i] == 1) {
                count++;
            }
            priority_i++;
        }
        if (count > PRIORITY_NUM_TOTAL) {
            roce_err("The number of pfc priority queues exceeds the maximum value.");
            return UDA_TOOL_OP_PFC_NUMS_OUT_OF_VALUE;
        }
    }

    return 0;
}

STATIC int tool_set_pfc(int argc, char **argv, struct tool_param *param)
{
    int ret;
    char pfc_enabled[PRIORITY_NUM] = {0};

    LOG_CONFIG("Start to set PFC cfg of logic_id[%d]", param->logic_id);

    if (argc != ARGC_ID_2) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (strncmp(argv[0], "bitmap", strlen("bitmap") + 1)) {
        roce_err("Invalid option.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    if (strlen(argv[1]) != PFC_CFG_STR_LEN) {
        roce_err("Check para length err.");
        return UDA_PARAM_INVALID_ERR;
    }

    ret = tool_pfc_str2char(argv[1], PFC_CFG_STR_LEN, pfc_enabled);
    if (ret) {
        roce_err("Invalid pfc cfg.");
        return ret;
    }

    ret = dsmi_set_pfc_enabled(param->logic_id, 0, pfc_enabled, PRIORITY_NUM);
    if (ret) {
        roce_err("DSMI set pfc enabled failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return ret;
}

int tool_pfc_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc == 0) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_pfc(argc, argv, param);
        if (ret) {
            roce_err("Get pfc cfg failed. (ret=%d)", ret);
            return ret;
        }
    } else if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        ret = tool_set_pfc(argc - 1, argv + 1, param);
        if (ret) {
            roce_err("Set pfc failed. (logic_id=%d; ret=%d)", param->logic_id, ret);
            return ret;
        }
        ret = cfg_write_conf(param->phy_id, "bitmap", argv[ARGC_ID_2]);
        if (ret) {
            roce_err("Write pfc cfg error. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        roce_err("Invalid option.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return ret;
}

int tool_cfg_pfc_recovery(struct tool_param *param)
{
    int ret;
    char pfc_enabled[PFC_CFG_STR_LEN + 1] = "";
    char *argv[ARGC_ID_2];
    argv[0] = "bitmap";

    ret = cfg_read_conf(param->phy_id, "bitmap", pfc_enabled, sizeof(pfc_enabled));
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            roce_info("The PFC conf name is not found in config file, skipped. (ret=%d)", ret);
            return 0;
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        roce_info("PFC enabled need recover cfg. (logic_id=%d)", param->logic_id);
        argv[1] = pfc_enabled;
        ret = tool_set_pfc(ARGC_ID_2, argv, param);
        if (ret) {
            roce_err("PFC set failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        }
    }

    return ret;
}

STATIC int tool_get_tc_cfg(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct tc_cfg_get tc_cfg = {{0}, {0}, {0}};

    if (argc != 1) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_tc_cfg(param->logic_id, 0, &tc_cfg);
    if (ret) {
        roce_err("Dsmi get pfc enabled failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    TOOL_PRINT_INFO("TC configuration:");
    TOOL_PRINT_INFO("TC id          0 1 2 3 4 5 6 7");
    TOOL_PRINT_INFO("Buffer         %u %u %u %u %u %u %u %u", tc_cfg.tc_buff[0x0], tc_cfg.tc_buff[0x1],
                    tc_cfg.tc_buff[0x2], tc_cfg.tc_buff[0x3], tc_cfg.tc_buff[0x4], tc_cfg.tc_buff[0x5],
                    tc_cfg.tc_buff[0x6], tc_cfg.tc_buff[0x7]);
    TOOL_PRINT_INFO("Up waterline   %u %u %u %u %u %u %u %u", tc_cfg.up_waterline[0x0], tc_cfg.up_waterline[0x1],
                    tc_cfg.up_waterline[0x2], tc_cfg.up_waterline[0x3], tc_cfg.up_waterline[0x4],
                    tc_cfg.up_waterline[0x5], tc_cfg.up_waterline[0x6], tc_cfg.up_waterline[0x7]);
    TOOL_PRINT_INFO("Low waterline  %u %u %u %u %u %u %u %u", tc_cfg.low_waterline[0x0], tc_cfg.low_waterline[0x1],
                    tc_cfg.low_waterline[0x2], tc_cfg.low_waterline[0x3], tc_cfg.low_waterline[0x4],
                    tc_cfg.low_waterline[0x5], tc_cfg.low_waterline[0x6], tc_cfg.low_waterline[0x7]);
    return 0;
}

STATIC int tool_tc_str_to_value(char *str, unsigned long *tc_value, unsigned int tc_num)
{
    unsigned long tmp_value;
    unsigned int tc_i = 0;
    char *end_tc_str_p = NULL;
    char *tc_size_str = NULL;

    tc_size_str = strtok(str, ",");
    while (tc_size_str != NULL) {
        if (tc_i >= tc_num) {
            roce_err("No valid TC config infomation.\n");
            return UDA_PARAM_INVALID_ERR;
        }

        tmp_value = strtoul(tc_size_str, &end_tc_str_p, NUMBER_BASE);
        if ((end_tc_str_p == tc_size_str) || (*end_tc_str_p != '\0') || (errno == ERANGE)) {
            roce_err("Transfer TC value failed.\n");
            return UDA_PARAM_INVALID_ERR;
        }
        tc_value[tc_i] = tmp_value;
        tc_i++;
        tc_size_str = strtok(NULL, ",");
    }
    return 0;
}

STATIC int tool_tc_cfg_get_size(char *argv, struct tc_cfg_set *tc_cfg)
{
#define MAX_TOTAL_RX_TC_SIZE (14040 * 128)
#define MIN_RX_TC_SIZE 0x1880
    int ret, tc_i;
    unsigned long tmp_value[TC_NUM] = {0};
    unsigned long total_rx_tc_size = 0;

    ret = tool_tc_str_to_value(argv, tmp_value, TC_NUM);
    if (ret != 0) {
        roce_err("No valid TC size infomation.\n");
        return ret;
    }

    for (tc_i = 0; tc_i < TC_NUM; tc_i++) {
        if (((tmp_value[tc_i] != 0) && (tmp_value[tc_i] < MIN_RX_TC_SIZE)) ||
            (tmp_value[tc_i] > MAX_TOTAL_RX_TC_SIZE) || (total_rx_tc_size > MAX_TOTAL_RX_TC_SIZE)) {
            roce_err("TC size is invalid. (tc_i=%d; tc_size=%lu; total_size=%lu)\n", tc_i, tmp_value[tc_i],
                     total_rx_tc_size);
            return UDA_PARAM_INVALID_ERR;
        }
        tc_cfg->tc_buff[tc_i] = tmp_value[tc_i];
        total_rx_tc_size += tmp_value[tc_i];
    }

    return 0;
}

STATIC int tool_tc_cfg_get_watlin(char *argv, struct tc_cfg_set *tc_cfg)
{
#define MAX_WATER_LINE (0xFFFF * 128)
    int ret, tc_i;
    unsigned long tmp_value[TC_NUM] = {0};

    ret = tool_tc_str_to_value(argv, tmp_value, TC_NUM);
    if (ret != 0) {
        roce_err("No valid water line size infomation.\n");
        return ret;
    }

    for (tc_i = 0; tc_i < TC_NUM; tc_i++) {
        if (tmp_value[tc_i] > MAX_WATER_LINE || tmp_value[tc_i] > tc_cfg->tc_buff[tc_i]) {
            roce_err("Invalid water line size. (tc_i=%d; waterline=%lu)\n", tc_i, tmp_value[tc_i]);
            return UDA_PARAM_INVALID_ERR;
        }
        tc_cfg->waterline[tc_i] = tmp_value[tc_i];
    }
    return 0;
}

STATIC int tool_set_tc_cfg(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct tc_cfg_set tc_cfg = {{0}, {0}};

    LOG_CONFIG("Start to set TC cfg of logic_id[%d]", param->logic_id);
    if (strncmp(argv[0], "size", strlen("size") + 1)) {
        roce_err("Invalid option.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    ret = tool_tc_cfg_get_size(argv[1], &tc_cfg);
    if (ret) {
        roce_err("Invalid TC size cfg.");
        return ret;
    }

    if (strncmp(argv[ARGC_ID_2], "waterline", strlen("waterline") + 1)) {
        roce_err("Invalid option.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    ret = tool_tc_cfg_get_watlin(argv[ARGC_ID_3], &tc_cfg);
    if (ret) {
        roce_err("Invalid TC water line cfg.");
        return ret;
    }

    ret = dsmi_set_tc_cfg(param->logic_id, 0, &tc_cfg);
    if (ret) {
        roce_err("DSMI set TC cfg failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return ret;
}

STATIC int tool_set_write_tc_cfg(int argc, char **argv, struct tool_param *param)
{
    int ret, argc_i;
    char cmd_buf[CONLINE_LENGTH] = {0};
    char match_list[1][CONLINE_LENGTH];

    if (argc != ARGC_ID_4) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = sprintf_s(match_list[0], CONLINE_LENGTH, "tc_cfg_%d=", param->phy_id);
    if (ret <= 0) {
        roce_err("Add arp conf sprintf match list failed. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = sprintf_s(cmd_buf, CONLINE_LENGTH, "tc_cfg_%d=%s", param->phy_id, argv[0]);
    if (ret <= 0) {
        roce_err("Sprintf TC cfg size failed. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return UDA_TOOL_NO_MEM_ERR;
    }

    for (argc_i = 1; argc_i < argc; argc_i++) {
        ret = sprintf_s(cmd_buf, CONLINE_LENGTH, "%s %s", cmd_buf, argv[argc_i]);
        if (ret <= 0) {
            roce_err("Sprintf TC cfg phy_id failed. (ret=%d; argc_i=%d)", ret, argc_i);
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    ret = tool_set_tc_cfg(argc, argv, param);
    if (ret != 0) {
        roce_err("Set TC cfg failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }

    cmd_buf[strlen(cmd_buf)] = '\n';
    ret = handle_conf_to_file(cmd_buf, FILE_OPT_TYPE_MATCH_REPLACES, match_list, 1);
    if (ret != 0) {
        roce_err("Write TC cfg error. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return ret;
    }
    return ret;
}

int tool_tc_cfg_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc == 0) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_tc_cfg(argc, argv, param);
        if (ret) {
            roce_err("Get TC cfg failed. (ret=%d)", ret);
            return ret;
        }
    } else if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        ret = tool_set_write_tc_cfg(argc - 1, argv + 1, param);
        if (ret) {
            roce_err("Set TC cfg failed. (logic_id=%d; ret=%d)", param->logic_id, ret);
            return ret;
        }
    } else {
        roce_err("Invalid option.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return ret;
}

int tool_tc_cfg_recovery(struct tool_param *param)
{
    int ret, tc_param_i;
    char *tc_param_str = NULL;
    char tc_cfg_str[CONLINE_LENGTH] = {0};
    char *argv[ARGC_ID_4];

    ret = cfg_read_conf(param->phy_id, "tc_cfg", tc_cfg_str, sizeof(tc_cfg_str));
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            roce_info("The tc conf name is not found in config file, skipped. (ret=%d)", ret);
            return 0;
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        roce_info("TC need recover cfg. (logic_id=%d)", param->logic_id);

        tc_param_str = strtok(tc_cfg_str, " ");
        for (tc_param_i = 0; tc_param_i < ARGC_ID_4; tc_param_i++) {
            if (tc_param_str == NULL) {
                roce_err("TC cfg file is error. (ret=%d; logic_id=%d; tc_param_i=%d)", ret, param->logic_id,
                         tc_param_i);
                return UDA_PARAM_OP_NOT_SUPPORT_ERR;
            }
            argv[tc_param_i] = tc_param_str;
            tc_param_str = strtok(NULL, " ");
        }

        ret = tool_set_tc_cfg(ARGC_ID_4, argv, param);
        if (ret) {
            roce_err("TC cfg set failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        }
    }

    return ret;
}

STATIC int tool_get_tc_id(int argc, char **argv, int *tc_id)
{
    char *end_tc_str_p = NULL;
    const char *para_info = NULL;
    char **argv_tmp = argv;

    argv_tmp += INDEX_ONE;
    para_info = *argv_tmp;
    if (!strncmp(para_info, "tc", strlen("tc") + 1)) {
        argv_tmp += INDEX_ONE;
        para_info = *argv_tmp;

        *tc_id = (int)strtol(para_info, &end_tc_str_p, NUMBER_BASE);
        if ((end_tc_str_p == para_info) || (*end_tc_str_p != '\0') || (*tc_id >= TC_NUM) || (*tc_id < 0)) {
            roce_err("Get invalid TC id. (tc_id=%d)", *tc_id);
            return UDA_PARAM_TC_OUT_OF_RANGE_ERR;
        }
    } else {
        roce_err("Error tc used cmd.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}

STATIC int tool_get_current_tc_used(int logic_id, int tc_id)
{
    int ret;
    unsigned int tc_used;

    ret = dsmi_get_current_tc_used(logic_id, 0, tc_id, &tc_used);
    if (ret) {
        roce_err("Dsmi get current TC used buffer failed. (ret=%d; logic_id=%d; tc_id=%d)", ret, logic_id, tc_id);
        ret = dsmi_analysis_dsmi_ret_to_uda(ret);
        return ret;
    }

    TOOL_PRINT_INFO("Current TC used buffer:    %u", tc_used);
    return 0;
}

int tool_tc_buf_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id;
    int tc_id = 0;

    if (argc != PARAM_COUNT_THREE) {
        roce_err("Error argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    logic_id = param->logic_id;
    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_tc_id(argc, argv, &tc_id);
        if (ret) {
            roce_err("Get TC id failed. (ret=%d; tc_id=%d)", ret, tc_id);
            return ret;
        }
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
        if (tc_id >= CUR_MAX_TC_NUMBER) { // 当前A2, A3产品tc只放开了前4个， A1产品放开了8个
            TOOL_PRINT_INFO("Current TC %d is not enabled.", tc_id);
            return 0;
        }
#endif
        ret = tool_get_current_tc_used(param->logic_id, tc_id);
        if (ret) {
            roce_err("Get TC used buff failed. (ret=%d; logic_id=%d; tc_id=%d)", ret, logic_id, tc_id);
            return ret;
        }
    } else {
        roce_err("Get TC used buff failed, invalid operation.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

int parse_data_len_param(struct ping_operate_info *operate_info, int *cur, int argc, char **argv)
{
    unsigned int data_len;
    char *tmp_ptr = NULL;

    if ((*cur + 1) >= argc) {
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    data_len = strtoul(argv[*cur + 1], &tmp_ptr, STR_NUM_BASE);
    if (tmp_ptr == argv[*cur + 1] || *tmp_ptr != '\0') {
        roce_err("PING packet size must be unsigned int, now is %u, tail is %s", data_len, tmp_ptr);
        return UDA_PARAM_INVALID_ERR;
    } else if (data_len > MAX_DATA_LEN) {
        roce_err("PING packet size can not more than %u, now is %u", MAX_DATA_LEN, data_len);
        return UDA_PARAM_INVALID_ERR;
    }
    operate_info->data_len = data_len;
    *cur += 1;

    return 0;
}

int parse_dst_ip_param(struct ping_operate_info *operate_info, int *cur, int argc, char **argv)
{
    int ret;
    struct in_addr addr;

    if ((*cur + 1) >= argc) {
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (operate_info->ipv6_index == 1) {
        roce_err("Please use [ipv6_address] instead of [address]!");
        return UDA_PARAM_INVALID_ERR;
    }

    ret = inet_pton(AF_INET, argv[*cur + 1], &addr);
    if (ret <= 0) {
        roce_err("PING IP convert failed! (ret=%d; ip=%s; errno=%d)", ret, argv[*cur + 1], errno);
        return UDA_PARAM_IP_INVALID_ERR;
    }
    operate_info->dip = addr.s_addr;

    ret = sprintf_s(operate_info->str_ip, IP_ADDR_LEN, "%s", argv[*cur + 1]);
    if (ret <= 0) {
        roce_err("PING ICMP Sprintf_s fail! (ret=%d; ip=%s; errno=%d)", ret, argv[*cur + 1], errno);
        return UDA_TOOL_NO_MEM_ERR;
    }

    *cur += 1;
    return UDA_EXE_SUCCESS;
}

int parse_dst_ip6_param(struct ping_operate_info *operate_info, int *cur, int argc, char **argv)
{
    int ret;
    struct in6_addr addr6;

    if ((*cur + 1) >= argc) {
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (operate_info->ipv6_index == 0) {
        roce_err("Please use [address] instead of [ipv6_address]");
        return UDA_PARAM_INVALID_ERR;
    }

    ret = inet_pton(AF_INET6, argv[*cur + 1], &addr6);
    if (ret <= 0) {
        roce_err("PING6 IP convert failed! (ret=%d; ip=%s; errno=%d)", ret, argv[*cur + 1], errno);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    ret = sprintf_s(operate_info->str_ip6, IP6_ADDR_LEN, "%s", argv[*cur + 1]);
    if (ret <= 0) {
        roce_err("PING6 ICMP Sprintf_s fail! (ret=%d; ip=%s; errno=%d)", ret, argv[*cur + 1], errno);
        return UDA_TOOL_NO_MEM_ERR;
    }

    *cur += 1;
    return UDA_EXE_SUCCESS;
}

static struct ping_parse_param g_ping_param_arr[] = {
    {"pkt", parse_data_len_param},
    {"address", parse_dst_ip_param},
    {"ipv6_address", parse_dst_ip6_param},
    {NULL, NULL},
};

STATIC int tool_get_ping_status(int argc, char **argv, struct tool_param *param, int ipv6_index)
{
    int i, ret;
    char inbuf[INBUF_LEN + 1] = {0};
    struct ping_operate_info operate_info = {4096, 0, 0};
    struct ping_parse_param *param_info = NULL;

    if (argc > PARAM_COUNT_FIVE) {
        roce_err("Error argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (ipv6_index == ARGC_ID_1) {
        operate_info.ipv6_index = ARGC_ID_1;
    }

    for (i = ARGC_ID_1; i < argc; i++) {
        ret = UDA_PARAM_OP_NOT_SUPPORT_ERR;
        for (param_info = g_ping_param_arr; param_info->parse_proc != NULL; param_info++) {
            if (strcmp(argv[i], param_info->param) == 0) {
                ret = param_info->parse_proc(&operate_info, &i, argc, argv);
                break;
            }
        }
        if (ret != UDA_EXE_SUCCESS) {
            roce_err("PING Error cmd option. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
    }

    ret = dsmi_ping_fun(param->logic_id, param->phy_id, inbuf, &operate_info, sizeof(struct ping_operate_info));
    if (ret != 0) {
        roce_err("PING failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return ret;
}

int tool_ping_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int ipv6_index = 0;

    if (argc < PARAM_COUNT_THREE) {
        roce_err("Error argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (is_ipv6_cmd(&argc, &argv)) {
        ipv6_index = 1;
        if (is_atlas_9000_a3(param)) {
            roce_err("Ipv6 atlas_9000_a3 command is not supported.");
            return UDA_TOOL_OP_NOT_SUPPORT_IPV6_ERR;
        }
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_ping_status(argc, argv, param, ipv6_index);
        if (ret != UDA_EXE_SUCCESS) {
            return ret;
        }
    } else {
        roce_err("PING invalid operation.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return UDA_EXE_SUCCESS;
}

bool is_atlas_9000_a3(struct tool_param *param)
{
    if (param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID || param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID_2) {
        return true;
    } else {
        return false;
    }
}

// ip地址转换为sdid
// ip地址格式：[192].[server_id].[2+die_id].[199-device_id]
// sdid格式：[10位server_id][4位chip_id][2位die_id][16位device_id]
#define IP_GET_HEADER(A) ((A) & 0xFF)
#define IP_GET_SERVERID(A) (((A) >> 8) & 0xFF)
#define IP_GET_DIEID(A) ((((A) >> 16) & 0xFF) - 2)
#define IP_GET_DEVICEID(A) (199 - (((A) >> 24) & 0xFF))
#define IP_CONVERT_SDID(A) \
    ((IP_GET_SERVERID(A) << 22) | ((IP_GET_DEVICEID(A) / 2) << 18) | (IP_GET_DIEID(A) << 16) | (IP_GET_DEVICEID(A)))
#define SDID_GET_SERVERID(A) (((A) >> 22) & 0xFF)
#define SDID_GET_CHIPID(A) (((A) >> 18) & 0xF)
#define SDID_GET_DIEID(A) (((A) >> 16) & 0x3)
#define SDID_GET_DEVICEID(A) ((A) & 0xFFFF)

bool check_sdid_is_valid(unsigned int sdid)
{
    return (SDID_GET_SERVERID(sdid) < A3_SUPERPOD_MAX_NUMS &&
            (SDID_GET_CHIPID(sdid) * CHIP_DIE_CNT + SDID_GET_DIEID(sdid) == SDID_GET_DEVICEID(sdid)));
}

STATIC int parse_address_param(hccs_ping_operate_info *operate, int *argc_index, char **argv, int min, int max)
{
    int ret;
    struct in_addr addr = {0};

    if (operate->sdid != PING_SDID_INVALID_VALUE) { // 已输入过sdid，不能再使用ip地址
        roce_err("PING IP conflict with sdid. (sdid=[%u])", operate->sdid);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    ret = inet_pton(AF_INET, argv[*argc_index + 1], &addr);
    if (ret <= 0) {
        roce_err("PING IP convert failed. (ret=[%d]; address=[%s]; errno=[%d])", ret, argv[*argc_index + 1], errno);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    if (IP_GET_HEADER(addr.s_addr) != VNIC_IP_HEADER) {
        roce_err("PING IP header is invalid. (IP header=[%u])", IP_GET_HEADER(addr.s_addr));
        return UDA_PARAM_IP_INVALID_ERR;
    }

    operate->sdid = IP_CONVERT_SDID(addr.s_addr);
    if (check_sdid_is_valid(operate->sdid) == false) {
        roce_err("sdid is invalid. [sdid=%u]", operate->sdid);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    *argc_index += 1;
    return UDA_EXE_SUCCESS;
}

STATIC int parse_sdid_param(hccs_ping_operate_info *operate, int *argc_index, char **argv, int min, int max)
{
    uint32_t value;
    char *tmp_ptr = NULL;

    if (operate->sdid != PING_SDID_INVALID_VALUE) { // 已输入过ip地址，不能再使用sdid参数
        roce_err("PING sdid conflict with IP. (sdid=[%u])", operate->sdid);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    value = strtoul(argv[*argc_index + 1], &tmp_ptr, STR_NUM_BASE); // sdid为32位无符号数值
    if (tmp_ptr == argv[*argc_index + 1] || *tmp_ptr != '\0') {
        roce_err("PING input value is not integer value. (value=[%u]; tail=[%s])", value, tmp_ptr);
        return UDA_PARAM_INVALID_ERR;
    }

    operate->sdid = value;
    if (check_sdid_is_valid(operate->sdid) == false) {
        roce_err("sdid is invalid. [sdid=%u]", operate->sdid);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    *argc_index += 1;
    return UDA_EXE_SUCCESS;
}

STATIC int parse_integer_param(int *argc_index, char **argv, int min, int max, uint32_t *value)
{
    char *tmp_ptr = NULL;

    *value = strtoul(argv[*argc_index + 1], &tmp_ptr, STR_NUM_BASE);
    if (tmp_ptr == argv[*argc_index + 1] || *tmp_ptr != '\0') {
        roce_err("PING input value is not integer value. (value=[%u]; tail=[%s])", *value, tmp_ptr);
        return UDA_PARAM_INVALID_ERR;
    }

    if (*value < min || *value > max) {
        roce_err("PING input value is illegal. (value=[%u])", *value);
        return UDA_PARAM_INVALID_ERR;
    }

    *argc_index += 1;
    return UDA_EXE_SUCCESS;
}

STATIC int parse_pkt_param(hccs_ping_operate_info *operate, int *argc_index, char **argv, int min, int max)
{
    return parse_integer_param(argc_index, argv, min, max, &operate->packet_size);
}

STATIC int parse_cnt_param(hccs_ping_operate_info *operate, int *argc_index, char **argv, int min, int max)
{
    return parse_integer_param(argc_index, argv, min, max, &operate->packet_send_num);
}

STATIC int parse_interval_param(hccs_ping_operate_info *operate, int *argc_index, char **argv, int min, int max)
{
    return parse_integer_param(argc_index, argv, min, max, &operate->packet_interval);
}

STATIC int parse_ping_timeout_param(hccs_ping_operate_info *operate, int *argc_index, char **argv, int min, int max)
{
    return parse_integer_param(argc_index, argv, min, max, &operate->timeout);
}

STATIC struct ping_parse_ext g_ping_parse[] = {
    {"address", parse_address_param, 0, 0},
    {"sdid", parse_sdid_param, 0, 0},
    {"pkt", parse_pkt_param, PING_PACKET_SIZE_MIN, PING_PACKET_SIZE_MAX},
    {"cnt", parse_cnt_param, PING_PACKET_NUM_MIN, PING_PACKET_NUM_MAX},
    {"interval", parse_interval_param, PING_PACKET_INTERVAL_MIN, PING_PACKET_INTERVAL_MAX},
    {"timeout", parse_ping_timeout_param, PING_PACKET_TIMEOUT_MIN, PING_PACKET_TIMEOUT_MAX}};

// 校验参数条件需要修改，内部还有新增重试机制
STATIC int tool_ping_check_param(hccs_ping_operate_info operate_info)
{
    if ((operate_info.sdid == PING_SDID_INVALID_VALUE) ||
        (operate_info.packet_send_num * operate_info.packet_interval > PING_CONSTRAINT_VALUE)) {
        roce_err("PING input value is not meet constraint. [sdid = %u, send_num = %u, interval = %u].",
                 operate_info.sdid, operate_info.packet_send_num, operate_info.packet_interval);
        return UDA_PARAM_INVALID_ERR;
    }

    return UDA_EXE_SUCCESS;
}

int cmp(const void *a, const void *b)
{
    return *(long *)a - *(long *)b;
}

// 统计回复报文时延的95分位数
STATIC long tool_calc_95_percentile(long cost_time[], int num)
{
    if (num < 1) {
        return 0;
    }

    qsort(cost_time, num, sizeof(long), cmp);
    return cost_time[(int)(PERCENT_NINETY_FIVE * num)];
}

STATIC void tool_pkt_result_print(unsigned char result)
{
    if (result == RESULT_PING_NOT_START) {
        TOOL_PRINT_INFO("This pkt ping not start to send!");
    } else if (result == RESULT_PING_SEND_FAILED) {
        TOOL_PRINT_INFO("This pkt send fail!");
    } else if (result == RESULT_PING_CHECK_FAILED) {
        TOOL_PRINT_INFO("This pkt check recv msg fail!");
    } else {
        TOOL_PRINT_INFO("This pkt ping success!");
    }
}

STATIC void tool_ping_printf(int phy_id, hccs_ping_operate_info operate_info, hccs_ping_reply_info *reply_info)
{
    unsigned int loss_pkt_num = reply_info->total_packet_send_num - reply_info->total_packet_recv_num;
    long max_time = 0, min_time = LONG_MAX, tp95_time = 0, sum_time = 0;
    long cost_time[PING_PACKET_NUM_MAX] = {0};
    uint32_t seq, recv_num = 0;
    char ip_str[INET_ADDRSTRLEN];
    long double loss_rate;

    inet_ntop(AF_INET, &reply_info->dstip, ip_str, INET_ADDRSTRLEN);
    TOOL_PRINT_INFO("device %d PING %s\n", phy_id, ip_str);

    for (seq = 0; seq < operate_info.packet_send_num; ++seq) {
        tool_pkt_result_print(reply_info->ret[seq]);
        TOOL_PRINT_INFO("recv seq %u, time = %ld.%03ldus, L1 plane check result = 0x%x\n", seq,
                        reply_info->time[seq] / NS_CHANGE_TO_US, reply_info->time[seq] % NS_CHANGE_TO_US,
                        reply_info->ret_16k[seq]);
        if (reply_info->ret[seq] == RESULT_PING_SUCCESS) {
            max_time = max_time < reply_info->time[seq] ? reply_info->time[seq] : max_time;
            min_time = min_time > reply_info->time[seq] ? reply_info->time[seq] : min_time;
            sum_time += reply_info->time[seq];
            cost_time[recv_num++] = reply_info->time[seq];
        }
        usleep(US_CHANGE_TO_MS);
    }

    loss_rate = (loss_pkt_num == 0) ? 0 : (((long double)loss_pkt_num / reply_info->total_packet_send_num) * PERCENT);
    min_time = (min_time == LONG_MAX) ? 0 : min_time;
    tp95_time = tool_calc_95_percentile(cost_time, recv_num);

    TOOL_PRINT_INFO("%u packets transmitted, %u received, %.2Lf%% packets loss", reply_info->total_packet_send_num,
                    reply_info->total_packet_recv_num, loss_rate);
    TOOL_PRINT_INFO("max time %ld.%03ldus", max_time / NS_CHANGE_TO_US, max_time % NS_CHANGE_TO_US);
    TOOL_PRINT_INFO("min time %ld.%03ldus", min_time / NS_CHANGE_TO_US, min_time % NS_CHANGE_TO_US);
    if (recv_num) {
        TOOL_PRINT_INFO("average time %ld.%03ldus", (sum_time / recv_num) / NS_CHANGE_TO_US,
                        (sum_time / recv_num) % NS_CHANGE_TO_US);
    } else {
        TOOL_PRINT_INFO("average time %ld.%03ldus", 0, 0);
    }
    TOOL_PRINT_INFO("TP95 time %ld.%03ldus", tp95_time / NS_CHANGE_TO_US, tp95_time % NS_CHANGE_TO_US);
}

STATIC int tool_get_ping_status_ext(int argc, char **argv, struct tool_param *param)
{
    int ret, argc_index, parse_index;
    hccs_ping_operate_info operate_info = {.is_ipv6 = 0,
                                           .sdid = PING_SDID_INVALID_VALUE,
                                           .packet_size = PING_PACKET_SIZE_MIN,
                                           .packet_send_num = PING_PACKET_NUM_DEFAULT,
                                           .packet_interval = PING_PACKET_INTERVAL_MIN};
    hccs_ping_reply_info *reply_info = (hccs_ping_reply_info *)calloc(1, sizeof(hccs_ping_reply_info));
    if (reply_info == NULL) {
        roce_err("PING calloc reply_info failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    for (argc_index = ARGC_ID_1; argc_index + 1 < argc; argc_index++) {
        ret = UDA_PARAM_INVALID_ERR;
        for (parse_index = 0; parse_index < (sizeof(g_ping_parse) / sizeof(struct ping_parse_ext)); parse_index++) {
            if (strcmp(argv[argc_index], g_ping_parse[parse_index].param) == 0) {
                ret = g_ping_parse[parse_index].parse_proc(
                    &operate_info, &argc_index, argv, g_ping_parse[parse_index].min, g_ping_parse[parse_index].max);
                break;
            }
        }

        if (ret != UDA_EXE_SUCCESS) {
            roce_err("PING Error cmd option. (ret=[%d], logic_id=[%d])", ret, param->logic_id);
            goto exit;
        }
    }

    if (argc_index != argc) {
        ret = UDA_PARAM_INVALID_ERR;
        goto exit;
    }
    ret = tool_ping_check_param(operate_info);
    if (ret != 0) {
        goto exit;
    }

    operate_info.phy_id = param->phy_id;
    ret = dsmi_hccs_ping(param->logic_id, 0, &operate_info, reply_info);
    if (ret != 0) {
        roce_err("PING_ext failed. (ret=[%d]; logic_id=[%d])", ret, param->logic_id);
        ret = (ret == EBUSY) ? UDA_TOOL_HCCS_PING_RESOURCE_BUSY : dsmi_analysis_dsmi_ret_to_uda(ret);
        goto exit;
    }

    tool_ping_printf(param->phy_id, operate_info, reply_info);
exit:
    free(reply_info);
    return ret;
}

int tool_ping_cmd_execute_ext(int argc, char **argv, struct tool_param *param)
{
    int ret;

    // 只有天工、天成、910A3_A+X 910A3_A+K款型支持
    if (param->mainboard_id != ATLAS_9000_A3_MAINBOARD_ID && param->mainboard_id != ATLAS_9000_A3_MAINBOARD_ID_2 &&
        param->mainboard_id != ATLAS_900_A3_MAINBOARD_ID_1 && param->mainboard_id != ATLAS_900_A3_MAINBOARD_ID_2 &&
        param->mainboard_id != ATLAS_900_A3_MAINBOARD_A_X && param->mainboard_id != ATLAS_900_A3_MAINBOARD_A_K) {
        roce_err("This device does not support this function.");
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    if (argc < PARAM_COUNT_TWO || argc > PARAM_COUNT_ELEVEN) {
        roce_err("Error argc. (argc=[%d])", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = hccn_check_usr_identify();
    if (ret != 0) {
        roce_err("Check usr identify failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    if (strncmp(argv[0], "-g", strlen("-g") + 1) == 0) {
        ret = tool_get_ping_status_ext(argc, argv, param);
        if (ret != UDA_EXE_SUCCESS) {
            return ret;
        }
    } else {
        roce_err("PING invalid operation.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return UDA_EXE_SUCCESS;
}

STATIC void tool_display_pfc_duration(struct pfc_duration_info *duration_info, struct pfc_storm_info *storm_info)
{
    int i;

    for (i = 0; i < PRI_NUM; i++) {
        TOOL_PRINT_INFO("tx_pfc%d_duration_time=%.3fms", i, (double)duration_info->tx[i] / TIME_CHANGE);
    }

    for (i = 0; i < PRI_NUM; i++) {
        TOOL_PRINT_INFO("rx_pfc%d_duration_time=%.3fms", i, (double)duration_info->rx[i] / TIME_CHANGE);
    }

    for (i = 0; i < PRI_NUM; i++) {
        TOOL_PRINT_INFO("tx_pfc%d_duration_warn_cnt: %llu", i, storm_info->warn_cnt[i]);
    }

    for (i = 0; i < PRI_NUM; i++) {
        TOOL_PRINT_INFO("tx_pfc%d_duration_err_cnt: %llu", i, storm_info->err_cnt[i]);
    }
}

static int tool_get_pfc_duration_info(int argc, int logic_id)
{
    int ret;
    struct pfc_duration_info duration_info;
    struct pfc_storm_info storm_info;
    if (argc != 0) {
        roce_err("err argc. (argc=%d)\n", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_pfc_duration_info(logic_id, &duration_info);
    if (ret) {
        roce_err("DSMI get pfc duration info fail. (ret=%d; dev_id=%d)\n", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = dsmi_get_pfc_storm_info(logic_id, &storm_info);
    if (ret) {
        roce_err("DSMI get pfc storm info fail. (ret=%d; dev_id=%d)\n", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    tool_display_pfc_duration(&duration_info, &storm_info);

    return ret;
}

static int tool_clear_pfc_mode_duration(int argc, int logic_id, int duration_mod)
{
    int ret;
    if (argc != 0 && duration_mod == 0) {
        roce_err("err argc. (argc=%d)\n", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_clear_pfc_duration(logic_id, duration_mod);
    if (ret) {
        roce_err("DSMI clear pfc duration fail. (ret=%d; dev_id=%d)\n", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = dsmi_clear_pfc_storm(logic_id, duration_mod);
    if (ret) {
        roce_err("DSMI clear pfc storm fail. (ret=%d; dev_id=%d)\n", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return ret;
}

int tool_pfc_stat_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc == 0) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    // 天工不支持
    if (param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID || param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID_2) {
        roce_err("This device does not support this function.");
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_pfc_duration_info(argc - 1, param->logic_id);
        if (ret) {
            roce_err("Get pfc duration failed. (ret=%d)", ret);
            return ret;
        }
    } else if (!strncmp(argv[0], "-c", strlen("-c") + 1)) {
        ret = hccn_check_usr_identify();
        if (ret) {
            roce_err("Check usr identify failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }

        int duration_mod = 0; // duration_mod 0 = all;
        LOG_SET_CONFIG("Start to clear PFC pause duration. (logic_id=%d)", param->logic_id);
        ret = tool_clear_pfc_mode_duration(argc - 1, param->logic_id, duration_mod);
        LOG_SET_CONFIG("End to clear pfc pause duration.(logic_id=%d; ret=%d)", param->logic_id, ret);
        if (ret) {
            roce_err("Clear pfc pause duration  failed. (logic_id=%d; ret=%d)", param->logic_id, ret);
            return ret;
        }
    } else {
        roce_err("Invalid option.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return ret;
}

STATIC void tool_display_tc_stat_info(const struct ds_tc_stat_data *stat_info)
{
    int i;
    DSMI_PRINT_INFO("tc packet statistics:");
    for (i = 0; i < TC_MAX_NUM; i++) {
        DSMI_PRINT_INFO("tc_tx_pkt_num[%d]:%llu", i, stat_info->tc_tx[i]);
        DSMI_PRINT_INFO("tc_rx_pkt_num[%d]:%llu", i, stat_info->tc_rx[i]);
    }
}

int tool_tc_stat_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct ds_tc_stat_data tc_stat = {0};

    if (argc != ONE_VALUE) {
        roce_err("err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = dsmi_get_tc_stat(param->logic_id, &tc_stat);
        if (ret) {
            roce_err("dsmi get stat fail logic_id[%d] ret[%d]", param->logic_id, ret);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
        tool_display_tc_stat_info(&tc_stat);
    } else if (!strncmp(argv[0], "-c", strlen("-c") + 1)) {
        LOG_CONFIG("start to clear packet statistics of dev[%d]", param->logic_id);
        ret = dsmi_clear_tc_stat(param->logic_id);
        if (ret) {
            roce_err("dsmi clear stat fail logic_id[%d] ret[%d]", param->logic_id, ret);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
    } else {
        roce_err("invalid option");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return ret;
}

void tool_display_serdes_info_pll_lock(const int index, const struct ds_serdes_info *ds_info)
{
    if (index < 0 || index >= DS_LANE_MAX_NUM) {
        roce_err("Index of serdes info struct error. (index=[%d])", index);
        TOOL_PRINT_INFO("Index of struct array error.");
        return;
    }
    // pll lock
    TOOL_PRINT_INFO("pll 0 out of lock: %u", ds_info->ds_info[index].pll0_out_of_lock);
    TOOL_PRINT_INFO("pll 1 out of lock: %u", ds_info->ds_info[index].pll1_out_of_lock);
    TOOL_PRINT_INFO("pll 0 pwrdb trigger: %u", ds_info->ds_info[index].pll0_pwrdb_trigger);
    TOOL_PRINT_INFO("pll 1 pwrdb trigger: %u", ds_info->ds_info[index].pll1_pwrdb_trigger);
    TOOL_PRINT_INFO("pll 0 ref clk sel: %u", ds_info->ds_info[index].pll0_ref_clk_sel);
    TOOL_PRINT_INFO("pll 0 core clk divrstn: %u", ds_info->ds_info[index].pll0_core_clk_divrstn);
}

void tool_display_serdes_info_snr(const int index, const struct ds_serdes_info *ds_info)
{
    if (index < 0 || index >= DS_LANE_MAX_NUM) {
        roce_err("Index of serdes info struct error. (index=[%d])", index);
        TOOL_PRINT_INFO("Index of struct array error.");
        return;
    }
    // snr
    TOOL_PRINT_INFO("heh: %u", ds_info->ds_info[index].heh);
    TOOL_PRINT_INFO("snr metric msb: %u", ds_info->ds_info[index].snr_metric_msb);
    TOOL_PRINT_INFO("snr metric lsb: %u", ds_info->ds_info[index].snr_metric_lsb);
    TOOL_PRINT_INFO("snr metric his min msb: %u", ds_info->ds_info[index].snr_metric_his_min_msb);
    TOOL_PRINT_INFO("snr metric his min lsb: %u", ds_info->ds_info[index].snr_metric_his_min_lsb);
    TOOL_PRINT_INFO("snr cycles: %u", ds_info->ds_info[index].snr_cycles);
}

void tool_display_serdes_info_tx_drv(const int index, const struct ds_serdes_info *ds_info)
{
    if (index < 0 || index >= DS_LANE_MAX_NUM) {
        roce_err("Index of serdes info struct error. (index=[%d])", index);
        TOOL_PRINT_INFO("Index of struct array error.");
        return;
    }
    // tx_drv
    TOOL_PRINT_INFO("txfir post sign: %u", ds_info->ds_info[index].txfir_post_sign);
    TOOL_PRINT_INFO("txfir post: %u", ds_info->ds_info[index].txfir_post);
    TOOL_PRINT_INFO("txfir pre sign: %u", ds_info->ds_info[index].txfir_pre_sign);
    TOOL_PRINT_INFO("txfir pre: %u", ds_info->ds_info[index].txfir_pre);

    TOOL_PRINT_INFO("tx drv hswing en: %u", ds_info->ds_info[index].tx_drv_hswing_en);
    TOOL_PRINT_INFO("tx drv hswing cs p: %u", ds_info->ds_info[index].tx_drv_hswing_cs_p);
    TOOL_PRINT_INFO("tx drv hswing cs m: %u", ds_info->ds_info[index].tx_drv_hswing_cs_m);
    TOOL_PRINT_INFO("txcp dcd adj: %u", ds_info->ds_info[index].txcp_dcd_adj);
    TOOL_PRINT_INFO("txcp dcd adj sign: %u", ds_info->ds_info[index].txcp_dcd_adj_sign);
    TOOL_PRINT_INFO("txpwrdb: %u", ds_info->ds_info[index].txpwrdb);
    TOOL_PRINT_INFO("txratemode: %u", ds_info->ds_info[index].txratemode);
    TOOL_PRINT_INFO("txalignmode: %u", ds_info->ds_info[index].txalignmode);
    TOOL_PRINT_INFO("txphsval: %u", ds_info->ds_info[index].txphsval);
}

void tool_display_serdes_info_rx(const int index, const struct ds_serdes_info *ds_info)
{
    if (index < 0 || index >= DS_LANE_MAX_NUM) {
        roce_err("Index of serdes info struct error. (index=[%d])", index);
        TOOL_PRINT_INFO("Index of struct array error.");
        return;
    }

    TOOL_PRINT_INFO("rxpwrdb: %u", ds_info->ds_info[index].rxpwrdb);
    TOOL_PRINT_INFO("rxratemode: %u", ds_info->ds_info[index].rxratemode);

    TOOL_PRINT_INFO("boost index: %u", ds_info->ds_info[index].boost_index);
    for (int j = 0; j < BOOST_VALS_NUM; ++j) {
        TOOL_PRINT_INFO("boost vals[%d]: %u", j, ds_info->ds_info[index].boost_vals[j]);
    }

    TOOL_PRINT_INFO("rx ctle st1 agc: %u", ds_info->ds_info[index].rx_ctle_st1_agc);
}

void tool_display_serdes_info_cdr(const int index, const struct ds_serdes_info *ds_info)
{
    if (index < 0 || index >= DS_LANE_MAX_NUM) {
        roce_err("Index of serdes info struct error. (index=[%d])", index);
        TOOL_PRINT_INFO("Index of struct array error.");
        return;
    }
    TOOL_PRINT_INFO("cdrphscodeout: %u", ds_info->ds_info[index].cdrphscodeout);
    TOOL_PRINT_INFO("cdrphasegain: %u", ds_info->ds_info[index].cdrphasegain);
    TOOL_PRINT_INFO("cdrvcogain: %u", ds_info->ds_info[index].cdrvcogain);
    TOOL_PRINT_INFO("cdr2ndlpcfg: %u", ds_info->ds_info[index].cdr2ndlpcfg);
    TOOL_PRINT_INFO("cdrfreqgain: %u", ds_info->ds_info[index].cdrfreqgain);
    TOOL_PRINT_INFO("cursorconfig: %u", ds_info->ds_info[index].cursorconfig);
}

void tool_display_serdes_info_api(const int index, const struct ds_serdes_info *ds_info)
{
    if (index < 0 || index >= DS_LANE_MAX_NUM) {
        roce_err("Index of serdes info struct error. (index=[%d])", index);
        TOOL_PRINT_INFO("Index of struct array error.");
        return;
    }
    TOOL_PRINT_INFO("cs calib en api: %u", ds_info->ds_info[index].cs_calib_en_api);
    TOOL_PRINT_INFO("cs calib status api: %u", ds_info->ds_info[index].cs_calib_status_api);
    TOOL_PRINT_INFO("ds calib en api: %u", ds_info->ds_info[index].ds_calib_en_api);
    TOOL_PRINT_INFO("dc calib status api: %u", ds_info->ds_info[index].dc_calib_status_api);
    TOOL_PRINT_INFO("init apapt en api: %u", ds_info->ds_info[index].init_apapt_en_api);
    TOOL_PRINT_INFO("init apapt status api: %u", ds_info->ds_info[index].init_apapt_status_api);
    TOOL_PRINT_INFO("cont apapt en api: %u", ds_info->ds_info[index].cont_apapt_en_api);
    TOOL_PRINT_INFO("cont apapt status api: %u", ds_info->ds_info[index].cont_apapt_status_api);
    TOOL_PRINT_INFO("ds state api: %u", ds_info->ds_info[index].ds_state_api);

    TOOL_PRINT_INFO("los det status api: %u", ds_info->ds_info[index].los_det_status_api);
    TOOL_PRINT_INFO("lms los det status api: %u", ds_info->ds_info[index].lms_los_det_status_api);
    TOOL_PRINT_INFO("cdr los det status api: %u", ds_info->ds_info[index].cdr_los_det_status_api);
    TOOL_PRINT_INFO("rnd los det status api: %u", ds_info->ds_info[index].rnd_los_det_status_api);
    TOOL_PRINT_INFO("los history status api: %u", ds_info->ds_info[index].los_history_status_api);
    TOOL_PRINT_INFO("lms los history status api: %u", ds_info->ds_info[index].lms_los_history_status_api);
    TOOL_PRINT_INFO("cdr los history status api: %u", ds_info->ds_info[index].cdr_los_history_status_api);
    TOOL_PRINT_INFO("rnd los history status api: %u", ds_info->ds_info[index].rnd_los_history_status_api);

    TOOL_PRINT_INFO("ds rx insertion loss api: %u", ds_info->ds_info[index].ds_rx_insertion_loss_api);
    TOOL_PRINT_INFO("ds power mode api: %u", ds_info->ds_info[index].ds_power_mode_api);
}

void tool_display_serdes_info(int phy_id, const struct ds_serdes_info *ds_info)
{
    for (int i = 0; i < DS_LANE_MAX_NUM; ++i) {
        TOOL_PRINT_INFO("---------------------lane %d info------------------------", i);
        // pll lock
        tool_display_serdes_info_pll_lock(i, ds_info);
        // snr
        tool_display_serdes_info_snr(i, ds_info);
        // tx_drv
        tool_display_serdes_info_tx_drv(i, ds_info);

        tool_display_serdes_info_rx(i, ds_info);

        tool_display_serdes_info_cdr(i, ds_info);

        TOOL_PRINT_INFO("integrator out: %u", ds_info->ds_info[i].integrator_out);

        tool_display_serdes_info_api(i, ds_info);

        TOOL_PRINT_INFO("rx ctle st1 offset: %u", ds_info->ds_info[i].rx_ctle_st1_offset);
        TOOL_PRINT_INFO("dp coef read indx: %u", ds_info->ds_info[i].dp_coef_read_indx);
    }
    TOOL_PRINT_INFO("--------------------------------------------------------");
}

int tool_get_hilink_info_mode_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret = 0;
    struct ds_serdes_info serdes_info = {0};

    if (argc != ONE_VALUE) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "0", strlen("0") + 1)) {
        param->get_cmd_flag = true;
        // 稳态查询
        ret = dsmi_get_serdes_info_steady(param->logic_id, &serdes_info);
        if (ret) {
            roce_err("Dsmi get serdes info steady fail. (logic_id=%d; ret=%d)", param->logic_id, ret);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
        tool_display_serdes_info(param->phy_id, &serdes_info);
    } else if (!strncmp(argv[0], "1", strlen("1") + 1)) {
        param->get_cmd_flag = true;
        // 闪断记录查询
        ret = dsmi_get_serdes_info_flash(param->logic_id, &serdes_info);
        if (ret) {
            roce_err("Dsmi get serdes info flash fail. (logic_id=%d; ret=%d)", param->logic_id, ret);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
        tool_display_serdes_info(param->phy_id, &serdes_info);
    } else {
        roce_err("Invalid param.");
        return UDA_PARAM_INVALID_ERR;
    }

    return ret;
}

int tool_hilink_info_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret = 0;
    struct ds_serdes_info serdes_info = {0};

    if (argc != TWO_VALUE) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-t", strlen("-t") + 1) || !strncmp(argv[0], "--type", strlen("--type") + 1)) {
        ret = tool_get_hilink_info_mode_cmd_execute(argc - 1, argv + 1, param);
        if (ret) {
            roce_err("Tool get hilink info mode cmd execute fail. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
    } else {
        roce_err("Invalid opration.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return ret;
}

static int tool_get_qp_info(int argc, int logic_id)
{
    int ret;
    int port_id = 0;
    struct ds_qpn_list *list;
    struct ds_qp_info qp_info;
    int active_qp_num = 0;
    unsigned int i;

    if (argc != 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    list = calloc(1, sizeof(struct ds_qpn_list));
    if (list == NULL) {
        roce_err("TOOL get qpn list failed, cannot malloc.\n");
        return -ENOMEM;
    }

    ret = dsmi_get_qpn_list(logic_id, port_id, list);
    if (ret != 0) {
        roce_err("DSMI get qpn list failed. (ret=%d; dev_id=%d)\n", ret, logic_id);
        goto out;
    }

    TOOL_PRINT_INFO("%-6s %-9s %-6s %-16s %-35s %-12s %-15s %-8s", "qpn", "status", "type", "dst_qpn", "ip", "src_port",
                    "send_psn", "recv_psn");

    for (i = 0; i < list->qp_num; i++) {
        ret = dsmi_get_qp_info(logic_id, port_id, list->qpn_list[i], &qp_info);
        if (ret != 0) {
            roce_err("DSMI get qp info failed. (ret=%d; dev_id=%d)\n", ret, logic_id);
            goto out;
        }

        if (qp_info.status != 0) {
            active_qp_num++;
            TOOL_PRINT_INFO("%-8u %-8u %-8u %-10u %-40s %-11u %-15u %-8u", qp_info.src_qpn, qp_info.status,
                            qp_info.type, qp_info.dst_qpn, qp_info.ip, qp_info.src_port, qp_info.send_psn,
                            qp_info.recv_psn);
        }

        ret = memset_s(&qp_info, sizeof(qp_info), 0, sizeof(qp_info));
        if (ret != 0) {
            roce_err("Memset qp info failed. (ret=%d; dev_id=%d)\n", ret, logic_id);
            goto out;
        }
    }

    TOOL_PRINT_INFO("total_qp_count: %d", active_qp_num);

out:
    free(list);
    return ret;
}

int tool_get_qp_info_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc == 0) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_qp_info(argc - 1, param->logic_id);
        if (ret != 0) {
            roce_err("Tool_get_qp_info failed. (ret:%d; logic_id:%d)\n", ret, param->logic_id);
            return ret;
        }
    } else {
        roce_err("Tool_qp_info_cmd_execute invalid operation.\n");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return ret;
}

int tool_roce_cw_bad_cnt_thd_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id;
    int argc_tmp = argc - 1;

    if (argc == 0) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    // 天工不支持，Atlas 300I A2(标卡)不支持
    if ((param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID) || (param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID_2)) {
        roce_err("Command unsupported on this product.");
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    logic_id = param->logic_id;
    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_roce_cw_bad_cnt_thd(argc_tmp, argv, logic_id);
        if (ret != 0) {
            roce_err("Get roce cw_bad_cnt_thd failed. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        ret = tool_double_check("[Warning]Changing this value violates IEEE Std 802.3, \n"
                                "which will trigger a temporary link down and may increase bad codewords, continue?");
        if (ret != 0) {
            LOG_CONFIG("Hccn tool command aborts by user.");
            return ret;
        }
        ret = tool_set_roce_cw_bad_cnt_thd(argc_tmp, argv + 1, param);
        if (ret != 0) {
            roce_err("Tool set roce cw_bad_cnt_thd failed. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
        ret = cfg_write_conf(param->phy_id, "cw_bad_cnt_thd_high_limit", argv[ARGC_ID_2]);
        if (ret != 0) {
            roce_err("Tool write cw_bad_cnt_thd cfg failed. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        roce_err("Invalid operation.\n)");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return UDA_EXE_SUCCESS;
}

int str_to_int(char *str, int *value)
{
    int ret;
    char *endp = NULL;

    ret = strtol(str, &endp, DECIMAL);
    if ((endp != NULL) && (*endp == '\0')) {
        *value = ret;
        return 0;
    }

    roce_err("[%s] is not integer", str);
    return -EINVAL;
}

int tool_link_set_downdelay(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int time;

    if (argc != 1) {
        roce_err("err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (str_to_int(argv[0], &time)) {
        roce_err("down_fault_time is invalid. (input=%s)", argv[0]);
        return UDA_PARAM_INVALID_ERR;
    }

    if (time < 0 || time > LINKDOWN_FILTER_TIME_MAX) {
        roce_err("down_fault_time is outbound. (input=%s)", argv[0]);
        return UDA_PARAM_INVALID_ERR;
    }

    LOG_CONFIG("Start to set linkdown filter time of logic_id[%d]", param->logic_id);

    ret = tool_config_downdelay(param->logic_id, time, SET_MODE);
    if (ret) {
        roce_err("config link down delay failed. (ret=%d)", ret);
        return ret;
    }

    if (!g_recovering) {
        ret = cfg_write_conf(param->phy_id, "down_fault_time", argv[0]);
        if (ret) {
            roce_err("update config file failed. (ret=%d)", ret);
            return ret;
        }
    }
    return 0;
}

int tool_config_downdelay(int logic_id, int value, int mode)
{
    int ret;
    struct send_data_info data;
    int time;

    if (mode == GET_MODE) {
        time = -1; // 设置模式下给time置异常值，通知底层这不是设置命令
    } else {
        time = value;
    }

    TOOL_SET_SEND_DATA(data, (mode == SET_MODE) ? DS_SET_DOWNDELAY : DS_GET_DOWNDELAY, (char *)&time, sizeof(int),
                       (char *)&time, sizeof(int));
    ret = dsmi_send_data(logic_id, 0, &data);
    if (ret) {
        roce_err("%s link down delay time failed. (ret=%d)", value < 0 ? "get" : "set", ret);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    if (mode == GET_MODE) {
        TOOL_PRINT_INFO("down_fault_time: %ds", time);
    }
    return 0;
}