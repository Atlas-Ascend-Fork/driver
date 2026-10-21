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
#include "hccn_dfx_struct.h"
#include "log.h"

#define MAX_ELEMENT_NUM 16
#define MAX_STR_LEN 1024
#define HEAD_LEN (8 + 2) /* "0x" len is 2 */

#define HEAD_FORMAT "0x%-8.04x"
#define ELE_LEN 3
#define ELE_FORMAT "%-3.02x"
#define MAX_PRINT_NUM 4

#define FEC_MODE_NO 0
#define FEC_MODE_RS 4
#define FEC_MODE_AUTO 1
#define AUTO_FLAG 0x1
#define FEC_GET_MODE_ERROR (-1)

#define PCS_POWER_INDEX_0 0
#define PCS_POWER_INDEX_1 1
#define PCS_POWER_INDEX_2 2
#define PCS_POWER_INDEX_3 3
#define PCS_EQ_DATA_DISPLAY_NUM 36
#define PCS_MACHINE_STATE_DATA_DISPLAY_NUM 19

const char *adapt_status_str[] = {
    "START",    "ADP_START",    "CDR_ADP",     "MAC_RF_1", "RESET_DONE_1", "CTLE_DONE_1",
    "MAC_RF_2", "RESET_DONE_2", "CTLE_DONE_2", "MAC_UP",   "OUT",
};
const char *err_code_str[] = {
    "NO_ERR",
    "ADP_START2START",
    "CDR_ADP2START",
    "MAC_RF2ADP_START",
    "RESET_DONE2ADP_START",
    "CTLE_DONE2ADP_START_1",
    "CTLE_DONE2ADP_START_2",
    "MAC_UP2MAC_UP",
    "MAC_UP2ADP_START",
};

static const char *fec_mode_str[] = {
    "rs",
    "off",
#ifndef CFG_SOC_PLATFORM_CLOUD_V2
    "auto",
#endif
};
unsigned int g_little_mem = MEM_STATUS_INIT;
unsigned int get_little_mem_process()
{
    return g_little_mem;
}
void set_little_mem_process(unsigned int n)
{
    g_little_mem = n;
    return;
}

static int tool_prepare_add_ip_rule_param(const char *dir, const char *ip, const char *table,
                                          struct ip_rule_cmd_info *cmd_info)
{
    unsigned int table_id;
    struct in_addr addr;
    int ret;

    cmd_info->cmd_type = 's';
    if (strncmp(dir, "from", strlen("from") + 1) == 0) {
        cmd_info->dir = IP_RULE_DIR_FROM;
    } else if (strncmp(dir, "to", strlen("to") + 1) == 0) {
        cmd_info->dir = IP_RULE_DIR_TO;
    } else {
        return UDA_PARAM_INVALID_ERR;
    }

    ret = inet_pton(AF_INET, ip, &addr);
    if (ret <= 0) {
        roce_err("Invalid IPv4 address. (ret=%d; errno=%d)", ret, errno);
        return UDA_PARAM_IP_INVALID_ERR;
    }
    cmd_info->ip_address = addr.s_addr;

    table_id = (unsigned int)strtoul(table, NULL, 0);
    if (table_id > MAX_IP_ROUTE_TAB_IDX) {
        roce_err("Invalid table id in ip rule cmd.");
        return UDA_PARAM_INVALID_ERR;
    }
    cmd_info->table_id = table_id;

    return 0;
}

static int tool_prepare_del_ip_route_param(const char *ip, const char *table, const char *ip_mask,
                                           struct ip_route_cmd_info *cmd_info)
{
    unsigned int table_id;
    unsigned int ipmask;
    struct in_addr addr;
    int ret;

    cmd_info->cmd_type = 'd';
    ret = inet_pton(AF_INET, ip, &addr);
    if (ret <= 0) {
        roce_err("Invalid IPv4 address. (ret=%d; errno=%d)", ret, errno);
        return UDA_PARAM_IP_INVALID_ERR;
    }
    cmd_info->ip_address = addr.s_addr;

    ipmask = (unsigned int)strtoul(ip_mask, NULL, 0);
    if (ipmask > MAX_IP_MASK_BIT) {
        roce_err("Invalid ip mask in ip route cmd.");
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    cmd_info->ip_mask = ipmask;

    table_id = (unsigned int)strtoul(table, NULL, 0);
    if (table_id > MAX_IP_ROUTE_TAB_IDX) {
        roce_err("Invalid table id in ip route cmd.");
        return UDA_PARAM_INVALID_ERR;
    }
    cmd_info->table_id = table_id;
    return 0;
}

static int tool_prepare_add_ip_route_param(struct ip_route_cmd_param *cmd_param, struct ip_route_cmd_info *cmd_info)
{
    const char *dev_name = NULL;
    unsigned int eth_id;
    unsigned int table_id;
    unsigned int ipmask;
    struct in_addr addr_ip;
    struct in_addr addr_via;
    int ret;

    cmd_info->cmd_type = 's';
    ret = inet_pton(AF_INET, cmd_param->ip, &addr_ip);
    if (ret <= 0) {
        roce_err("Invalid IPv4 address. (ret=%d; errno=%d)", ret, errno);
        return UDA_PARAM_IP_INVALID_ERR;
    }
    cmd_info->ip_address = addr_ip.s_addr;

    ipmask = (unsigned int)strtoul(cmd_param->ip_mask, NULL, 0);
    if (ipmask > MAX_IP_MASK_BIT) {
        roce_err("Invalid ip mask in ip route cmd.");
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    cmd_info->ip_mask = ipmask;

    table_id = (unsigned int)strtoul(cmd_param->table, NULL, 0);
    if (table_id > MAX_IP_ROUTE_TAB_IDX) {
        roce_err("Invalid table id in ip route cmd.");
        return UDA_PARAM_INVALID_ERR;
    }
    cmd_info->table_id = table_id;

    ret = inet_pton(AF_INET, cmd_param->via, &addr_via);
    if (ret <= 0) {
        roce_err("Invalid IPv4 via address. (ret=%d; errno=%d)", ret, errno);
        return UDA_PARAM_IP_INVALID_ERR;
    }
    cmd_info->via_address = addr_via.s_addr;

    if (strncmp(cmd_param->dev, "eth", strlen("eth")) != 0) {
        return UDA_PARAM_INVALID_ERR;
    }
    dev_name = cmd_param->dev + strlen("eth");
    eth_id = (unsigned int)strtoul(dev_name, NULL, 0);
    if (eth_id > MAX_DEV_ID) {
        roce_err("Invalid eth id in add ip route cmd.");
        return UDA_PARAM_INVALID_ERR;
    }
    cmd_info->eth_id = eth_id;
    return 0;
}

STATIC int tool_check_ip_mask_bit(char *str)
{
    char cmp[MAX_PARAM_LEN] = {0};
    int mask_bit = 0;
    int ret;

    ret = sscanf_s(str, "%d", &mask_bit);
    if (ret <= 0) {
        roce_err("Get ip mask bit val failed. (ret=%d)", ret);
        return UDA_PARAM_INVALID_ERR;
    }

    ret = sprintf_s(cmp, MAX_PARAM_LEN, "%d", mask_bit);
    if (ret <= 0) {
        roce_err("Set ip mask bit string failed. (ret=%d)", ret);
        return UDA_PARAM_INVALID_ERR;
    }

    if (strncmp(str, cmp, strlen(cmp) + 1)) {
        roce_err("Ip mask bit string cmp failed. (str=\"%s\"; cmp=\"%s\")", str, cmp);
        return UDA_PARAM_INVALID_ERR;
    }

    if (mask_bit < MIN_IP_MASK_BIT || mask_bit > MAX_IP_MASK_BIT) {
        roce_err("Mask bit val check failed. (mask_bit=%d)", mask_bit);
        return UDA_PARAM_INVALID_ERR;
    }

    return 0;
}

static int tool_check_ip6(char *str)
{
    struct sockaddr_in6 address_ip6;
    if (inet_pton(AF_INET6, str, &(address_ip6.sin6_addr)) != 0) {
        return 0;
    } else {
        return UDA_PARAM_IP_INVALID_ERR;
    }

    return 0;
}

STATIC int tool_check_ip4(char *str)
{
    char cmp[MAX_PARAM_LEN] = {0};
    int ip[MAX_IP_CNT] = {0};
    int ret;
    int i;

    ret = sscanf_s(str, "%d.%d.%d.%d", &ip[IP_INDEX_0], &ip[IP_INDEX_1], &ip[IP_INDEX_2], &ip[IP_INDEX_3]);
    if (ret <= 0) {
        roce_warn("Get ip4 val failed. (ret=%d)", ret);
        return UDA_PARAM_INVALID_ERR;
    }

    ret = sprintf_s(cmp, MAX_PARAM_LEN, "%d.%d.%d.%d", ip[IP_INDEX_0], ip[IP_INDEX_1], ip[IP_INDEX_2], ip[IP_INDEX_3]);
    if (ret <= 0) {
        roce_warn("Set ip4 string failed. (ret=%d)", ret);
        return UDA_PARAM_INVALID_ERR;
    }

    if (strncmp(str, cmp, strlen(cmp) + 1)) {
        roce_warn("Ip4 string cmp failed. (str=\"%s\"; cmp=\"%s\")", str, cmp);
        return UDA_PARAM_INVALID_ERR;
    }

    for (i = 0; i < MAX_IP_CNT; i++) {
        if (ip[i] > MAX_IP_VAL || ip[i] < MIN_IP_VAL) {
            roce_warn("Ip4 val check failed. (ip[%d]=%d)", i, ip[i]);
            return UDA_PARAM_INVALID_ERR;
        }
    }

    return 0;
}

STATIC int tool_check_ip(char *str)
{
    if (tool_check_ip4(str) == 0) {
        return 0;
    } else if (tool_check_ip6(str) == 0) {
        return 0;
    } else {
        roce_err("Ip is not adapt IPv4 or IPv6. (str=%s)", str);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    return 0;
}

STATIC int tool_check_mac(char *str)
{
#define MAC_PARAM_LEN 17
#define MAC_SPLITS_IDX 2
#define MAC_SPLITS_UNIT 3
    int i;
    unsigned char c;

    if (strlen(str) != MAC_PARAM_LEN) {
        roce_err("The length of the mac parameter is invalid. (str=\"%s\")", str);
        return UDA_PARAM_MAC_ADDR_INVALID_ERR;
    }

    for (i = 0; i < MAC_PARAM_LEN; i++) {
        c = str[i];
        if (i % MAC_SPLITS_UNIT == MAC_SPLITS_IDX) {
            if (c != ':') {
                roce_err("Mac val check failed. (mac[%d]=%d)", i, c);
                return UDA_PARAM_MAC_ADDR_INVALID_ERR;
            }
            continue;
        }
        if (c >= '0' && c <= '9') {
            continue;
        } else if (c >= 'a' && c <= 'f') {
            continue;
        } else if (c >= 'A' && c <= 'F') {
            continue;
        } else {
            roce_err("Mac val check failed. (mac[%d]=%d)", i, c);
            return UDA_PARAM_MAC_ADDR_INVALID_ERR;
        }
    }

    return 0;
}

STATIC int tool_check_dev(char *str)
{
    char cmp[MAX_PARAM_LEN] = {0};
    int val;
    int ret;

    ret = sscanf_s(str, "eth%d", &val);
    if (ret <= 0) {
        roce_err("Get dev val failed. (ret=%d)", ret);
        return UDA_PARAM_INVALID_ERR;
    }

    ret = sprintf_s(cmp, MAX_PARAM_LEN, "eth%d", val);
    if (ret <= 0) {
        roce_err("Set dev string failed. (ret=%d)", ret);
        return UDA_PARAM_INVALID_ERR;
    }

    if (strncmp(str, cmp, strlen(cmp) + 1)) {
        roce_err("Dev string cmp failed. (str=\"%s\"; cmp=\"%s\")", str, cmp);
        return UDA_PARAM_INVALID_ERR;
    }

    if (val < MIN_DEV_ID || val > MAX_DEV_ID) {
        roce_err("Dev val check failed. (val=%d)", val);
        return UDA_PARAM_INVALID_ERR;
    }

    return 0;
}

STATIC int tool_check_table(char *str)
{
    char cmp[MAX_PARAM_LEN] = {0};
    int val;
    int ret;

    ret = sscanf_s(str, "%d", &val);
    if (ret <= 0) {
        roce_err("Get table val failed. (ret=%d)", ret);
        return UDA_PARAM_INVALID_ERR;
    }

    ret = sprintf_s(cmp, MAX_PARAM_LEN, "%d", val);
    if (ret <= 0) {
        roce_err("Set table string failed. (ret=%d)", ret);
        return UDA_PARAM_INVALID_ERR;
    }

    if (strncmp(str, cmp, strlen(cmp) + 1)) {
        roce_err("Table string cmp failed. (str=\"%s\"; cmp=\"%s\")", str, cmp);
        return UDA_PARAM_INVALID_ERR;
    }

    if (val < MIN_IP_ROUTE_TAB_IDX || val > MAX_IP_ROUTE_TAB_IDX) {
        roce_err("Table val check failed. (val=%d)", val);
        return UDA_PARAM_INVALID_ERR;
    }

    return 0;
}

STATIC int tool_check_dir(char *str)
{
    if (!strncmp(str, "from", strlen("from") + 1)) {
        return 0;
    } else if (!strncmp(str, "to", strlen("to") + 1)) {
        return 0;
    } else {
        roce_err("Dir check failed. (str=\"%s\")", str);
        return UDA_PARAM_INVALID_ERR;
    }
}

static struct param_check_info g_param_check_tab[] = {
    {"ip", tool_check_ip},   {"via", tool_check_ip},      {"mac", tool_check_mac},
    {"dev", tool_check_dev}, {"table", tool_check_table}, {"ip_mask", tool_check_ip_mask_bit},
    {"dir", tool_check_dir}, {"address", tool_check_ip},  {"ipv6_address", tool_check_ip6}};

STATIC int tool_check_param(char *prefix, char *str)
{
    int cnt;
    int i;

    cnt = sizeof(g_param_check_tab) / sizeof(g_param_check_tab[0]);
    for (i = 0; i < cnt; i++) {
        if (!strncmp(prefix, g_param_check_tab[i].param_name, strlen(g_param_check_tab[i].param_name) + 1)) {
            return g_param_check_tab[i].check_func(str);
        }
    }
    roce_err("Find param failed. (prefix=\"%s\")", prefix);
    return UDA_PARAM_INVALID_ERR;
}

int tool_get_param(int argc, char **argv, char *prefix, char *val, int max_len)
{
#define PARAM_STEP_NUM 2
    int i;
    int ret;

    for (i = 0; i < argc;) {
        if ((!strncmp(argv[i], prefix, strlen(prefix) + 1)) && (i < argc - 1)) {
            ret = memcpy_s(val, max_len, argv[i + 1], strlen(argv[i + 1]));
            if (ret) {
                roce_err("Get param copy failed.(ret=%d; max_len=%d; real_len=%d)", ret, max_len, strlen(argv[i + 1]));
                return UDA_PARAM_INVALID_ERR;
            }
            ret = tool_check_param(prefix, val);
            return ret;
        }
        i += PARAM_STEP_NUM;
    }
    roce_err("Find param name failed.(prefix=%s)", prefix);
    return UDA_PARAM_OP_NOT_SUPPORT_ERR;
}

STATIC int tool_start_roce_perftest(int device_id, unsigned int is_client, struct perftest_curr_stat *perftest_curr,
                                    struct perftest_cmd_info cmd_info[], unsigned int num)
{
    int ret;
    unsigned int perftest_start_result = 0;

    ret = dsmi_get_roce_perftest_status(device_id, is_client, perftest_curr);
    if (ret) {
        roce_err("Dsmi get roce perftest status failed. (ret=%d; dev_id=%d)", ret, device_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    TOOL_PRINT_INFO("Dsmi get perftest status end. (status=%u)", perftest_curr->status);
    if (perftest_curr->status == PERFTEST_STATUS_DOING) {
        roce_err("Perftest status is doing. (dev_id=%d)", device_id);
        TOOL_PRINT_ERR("Roce perftest is doing, please try later.");
        return perftest_curr->status;
    }
    ret = dsmi_start_roce_perftest(device_id, cmd_info, num, &perftest_start_result);
    if (ret) {
        roce_err("Dsmi start roce perftest failed. (ret=%d; dev_id=%d)", ret, device_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    if (perftest_start_result != PERFTEST_START_SUCCESS) {
        roce_err("Start roce perftest create pthread failed. (dev_id=%d; perftest_start_result=%d)", device_id,
                 perftest_start_result);
        return perftest_start_result;
    }
    return UDA_EXE_SUCCESS;
}

STATIC int tool_roce_perftest_handle(int argc, char **argv, struct tool_param *param,
                                     struct perftest_cmd_info cmd_info[], unsigned int num)
{
    struct perftest_curr_stat perftest_curr = {0};
    char *roce_report = NULL;
    unsigned int is_client = 0;
    int wait_num = MAX_PERFTEST_WAITING_TIMES;
    int i, ret;

    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "address", strlen("address") + 1)) {
            is_client = 1;
        }
    }

    ret = tool_start_roce_perftest(param->logic_id, is_client, &perftest_curr, cmd_info, num);
    if (ret) {
        roce_err("Start roce perftest failed. (ret=%d; dev_id=%d)", ret, param->logic_id);
        return ret;
    }
    ret = waiting_roce_perftest_finish(param->logic_id, is_client, cmd_info, &perftest_curr, wait_num);
    if (ret) {
        roce_err("Waitting roce perftest finish failed. (ret=%d; dev_id=%d)", ret, param->logic_id);
        return ret;
    }

    roce_report = (char *)calloc(MAX_REPORT_LEN, sizeof(char));
    if (roce_report == NULL) {
        roce_err("Tool get rule calloc roce_report failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }
    ret = dsmi_get_roce_perftest_report(param->logic_id, is_client, roce_report, MAX_REPORT_LEN);
    if (ret) {
        roce_err("Dsmi get roce perftest report failed. (ret=%d; dev_id=%d)", ret, param->logic_id);
        ret = dsmi_analysis_dsmi_ret_to_uda(ret);
        goto roce_perftest_out;
    }
    if (roce_report[0] != '\0') {
        DSMI_PRINT_INFO("roce_report:\n%s", roce_report);
    }
    if (perftest_curr.status == PERFTEST_ERROR) {
        roce_err("Perftest failed. (perftest_status=%d)", perftest_curr.status);
        ret = -1;
        goto roce_perftest_out;
    }
    ret = UDA_EXE_SUCCESS;

roce_perftest_out:
    free(roce_report);
    roce_report = NULL;
    return ret;
}

static int perftest_param_trans_ip_address(struct perftest_cmd_info *cmd_info, char *val, char *comment, int min_data,
                                           int max_data)
{
    struct in_addr addr;
    struct in6_addr addr6;
    if (inet_pton(AF_INET, val, &addr) > 0) {
        cmd_info->cfg_data = addr.s_addr;
    } else if (inet_pton(AF_INET6, val, &addr6) > 0) {
        memcpy_s(&cmd_info->in6_ip, sizeof(struct in6_addr), &addr6, sizeof(struct in6_addr));
        cmd_info->ipv6_index = 1;
    } else {
        roce_err("Roce test param %s cfg trans failed. (errno=%d)", comment, errno);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    return 0;
}

static int perftest_param_trans_ib_device(struct perftest_cmd_info *cmd_info, char *val, char *comment, int min_data,
                                          int max_data)
{
    char *hns_name = NULL;
    unsigned long cfgdata;
    hns_name = val + strlen("hns_");
    cfgdata = strtoul(hns_name, NULL, 0);
    if (cfgdata > (unsigned long)max_data) {
        roce_err("Roce test param %s %lu cfg trans failed.", comment, cfgdata);
        return UDA_PARAM_INVALID_ERR;
    }
    cmd_info->cfg_data = cfgdata;
    return 0;
}

static int perftest_param_trans(struct perftest_cmd_info *cmd_info, char *val, char *comment, int min_data,
                                int max_data)
{
    unsigned long cfgdata;
    char *val_tmp = "";
    cfgdata = strtoul(val, &val_tmp, NUMBER_BASE);
    if (*val_tmp != '\0' || cfgdata > (unsigned long)max_data || cfgdata < (unsigned long)min_data) {
        roce_err("Roce test param %s cfg trans failed. (cfgdata=%lu)", comment, cfgdata);
        return UDA_PARAM_INVALID_ERR;
    }
    cmd_info->cfg_data = cfgdata;
    return 0;
}

static struct perftest_param_info perftest_cmd_list[] = {
    {"-s", perftest_param_trans, "msg size", MASG_SIZE_MIN, MASG_SIZE_MAX},
    {"-d", perftest_param_trans_ib_device, "ib device", IB_DEVICE_MIN, IB_DEVICE_MAX},
    {"address", perftest_param_trans_ip_address, "service address", SERVICE_ADDRESS_MIN, SERVICE_ADDRESS_MAX},
    {"-n", perftest_param_trans, "exchanges number", EXCHANGES_NUMBER_MIN, EXCHANGES_NUMBER_MAX},
    {"-tclass", perftest_param_trans, "tos number", TOS_NUMBER_MIN, TOS_NUMBER_MAX},
    {"-D", perftest_param_trans, "period of seconds", PERIOD_OF_SECONDS_MIN, PERIOD_OF_SECONDS_MAX},
    {"-f", perftest_param_trans, "measure margins", MEASURE_MARGINS_MIN, MEASURE_MARGINS_MAX},
    {"-l", perftest_param_trans, "WQEs number", WQES_NUMBER_MIN, WQES_NUMBER_MAX},
    {"-m", perftest_param_trans, "mtu number", MTU_NUMBER_MIN, MTU_NUMBER_MAX},
    {"-ib", perftest_param_trans, "ib-port", IB_PORT_NUMBER_MIN, IB_PORT_NUMBER_MAX},
    {"-p", perftest_param_trans, "udp port", UDP_PORT_NUMBER_MIN, UDP_PORT_NUMBER_MAX},
    {"-q", perftest_param_trans, "qp number", QP_NUMBER_MIN, QP_NUMBER_MAX},
    {"-Q", perftest_param_trans, "cq-mod", CQ_MOD_NUMBER_MIN, CQ_MOD_NUMBER_MAX},
    {"-t", perftest_param_trans, "tx-depth", TX_DEPTH_NUMBER_MIN, TX_DEPTH_NUMBER_MAX},
    {"-u", perftest_param_trans, "qp-timeout", QP_TIMEOUT_NUMBER_MIN, QP_TIMEOUT_NUMBER_MAX},
    {"-x", perftest_param_trans, "gid-index", GID_INDEX_NUMBER_MIN, GID_INDEX_NUMBER_MAX},
    {"-S", perftest_param_trans, "priority", PRIORITY_NUMBER_MIN, PRIORITY_NUMBER_MAX},
    {"-bind_source_ip", perftest_param_trans_ip_address, "Source IP", SERVICE_ADDRESS_MIN, SERVICE_ADDRESS_MAX},
};

STATIC int get_param_from_cmd_list_ext(int argc, char **argv, int *cnt, struct perftest_cmd_info *cmd_info, int *size)
{
    int perftest_ext_len;
    int i;
    int ret = 0;
    char *perftest_cmd_list_ext[] = {"-a", "-b", "-V", "-tcp", "-report_gbits"};

    perftest_ext_len = sizeof(perftest_cmd_list_ext) / sizeof(perftest_cmd_list_ext[0]);
    for (i = 0; i < perftest_ext_len; i++) {
        if (*cnt >= argc) {
            return 0;
        }
        if (!strncmp(argv[*cnt], perftest_cmd_list_ext[i], strlen(perftest_cmd_list_ext[i]) + 1)) {
            *size += STEP_ONE;
            ret = sprintf_s(cmd_info->type, MAX_TYPE_LEN, argv[*cnt]);
            if (ret <= 0) {
                roce_err("Sprintf roce test failed. (ret=%d; param=\"%s\")", ret, argv[*cnt]);
                return UDA_PARAM_INVALID_ERR;
            }
            *cnt += STEP_ONE;
            return 0;
        }
    }
    return 0;
}

static void little_param(struct perftest_cmd_info *cmd_info, char *cmd_param, int n)
{
    char *per_short_param[] = {"-a",  "-b", "-V", "-tclass", "-u", "-t", "-Q", "-q",
                               "-ib", "-m", "-l", "-f",      "-D", "-x", "-p"};
    int size_param = sizeof(per_short_param) / sizeof(per_short_param[0]);
    int j;

    for (j = 0; j < size_param; j++) {
        if (!strncmp(cmd_param, per_short_param[j], n)) {
            g_little_mem = MEM_STATUS_LIMIT;
            break;
        }
    }
}

STATIC int get_param_from_cmd_list(int argc, char **argv, int *cnt, struct perftest_cmd_info *cmd_info, int *size)
{
    char val[MAX_PARAM_LEN] = {0};
    int perftest_len;
    int i;
    int ret = 0;

    perftest_len = sizeof(perftest_cmd_list) / sizeof(perftest_cmd_list[0]);
    for (i = 0; i < perftest_len; i++) {
        if (*cnt + 1 >= argc) {
            return UDA_PARAM_INVALID_ERR;
        }
        if (!strncmp(argv[*cnt], perftest_cmd_list[i].param, strlen(perftest_cmd_list[i].param) + 1)) {
            *size += STEP_ONE;
            little_param(cmd_info, perftest_cmd_list[i].param, strlen(perftest_cmd_list[i].param) + 1);
            ret = memcpy_s(val, MAX_PARAM_LEN, argv[*cnt + 1], strlen(argv[*cnt + 1]) + 1);
            if (ret != 0) {
                roce_err("Get param copy failed. (ret=%d; max_len=%d; real_len=%d)", ret, MAX_PARAM_LEN,
                         strlen(argv[*cnt + 1]));
                return UDA_PARAM_INVALID_ERR;
            }
            ret = perftest_cmd_list[i].param_trans_func(cmd_info, val, perftest_cmd_list[i].comment,
                                                        perftest_cmd_list[i].min_data, perftest_cmd_list[i].max_data);
            if (ret != 0) {
                roce_err("Param trans failed. (param=\"%s\"; val=\"%s\")", perftest_cmd_list[i].param, val);
                return ret;
            }
            ret = sprintf_s(cmd_info->type, MAX_TYPE_LEN, "%s", perftest_cmd_list[i].param);
            if (ret <= 0) {
                roce_err("Sprintf roce test param failed. (ret=%d)", ret);
                return UDA_PARAM_INVALID_ERR;
            }
            *cnt += STEP_TWO;
            return 0;
        }
    }
    return 0;
}

STATIC int tool_roce_test_get_param(int argc, char **argv, int *cnt, struct perftest_cmd_info *cmd_info, int *size)
{
    char val[MAX_PARAM_LEN] = {0};
    int temp_cnt;
    int ret;

    temp_cnt = *cnt;
    ret = get_param_from_cmd_list_ext(argc, argv, cnt, cmd_info, size);
    if (ret) {
        roce_err("Get param from cmd ext list failed. (ret=%d)", ret);
        return ret;
    }
    if (temp_cnt == *cnt) {
        ret = get_param_from_cmd_list(argc, argv, cnt, cmd_info, size);
        if (ret) {
            roce_err("Get param from cmd list failed. (ret=%d)", ret);
            return ret;
        }
    }
    if (temp_cnt == *cnt) {
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

STATIC int tool_roce_test_reset(int argc, struct tool_param *param)
{
    int ret;
    char *reset_report = NULL;

    if (argc > 1) {
        roce_err("Too many params. (argc=%d)", argc);
        return UDA_MAJOR_CMD_NUM_ERR;
    }

    reset_report = (char *)calloc(MAX_REPORT_LEN, sizeof(char));
    if (reset_report == NULL) {
        roce_err("Tool get rule calloc reset_report failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = dsmi_reset_roce_perftest(param->logic_id, reset_report);
    if (ret) {
        roce_err("Dsmi reset roce perftest fail. (ret=%d; dev_id=%d)", ret, param->logic_id);
        ret = dsmi_analysis_dsmi_ret_to_uda(ret);
        goto roce_test_reset_out;
    }
    if (reset_report[0] != '\0') {
        DSMI_PRINT_INFO("reset_report:\n%s", reset_report);
    }
    ret = UDA_EXE_SUCCESS;

roce_test_reset_out:
    free(reset_report);
    reset_report = NULL;
    return ret;
}

static int tool_get_perftest_cmd_info(int argc, char **argv, struct tool_param *param,
                                      struct perftest_cmd_info cmd_info[], int *size)
{
    char *perftest_list[] = {"ib_send_bw", "ib_read_bw", "ib_write_bw", "ib_send_lat", "ib_read_lat", "ib_write_lat"};
    int ret, cnt = 1;
    int i, is_client = 0;
    int start_cnt, tcp_flag = 0;

    for (i = 0; i < sizeof(perftest_list) / sizeof(char *); i++) {
        if (!strncmp(argv[0], perftest_list[i], strlen(perftest_list[i]) + 1)) {
            ret = sprintf_s((char *)cmd_info[0].type, MAX_TYPE_LEN, "%s", perftest_list[i]);
            if (ret <= 0) {
                roce_err("Sprintf fail, (ret=%d)", ret);
                return -EINVAL;
            }
            break;
        }
    }
    // little_mem :有符合小参数的输入用例出现，只使用-n -s
    g_little_mem = MEM_STATUS_INIT;
    if (i >= sizeof(perftest_list) / sizeof(char *)) {
        roce_err("Invalid option.\n");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    start_cnt = cnt;
    while (cnt < argc) {
        // 910开始roce_test只支持-tcp模式打流，因此做了tcp_flag检查。但-V查询当前roce_test版本号又不是打流，无需-tcp检查
        if ((tcp_flag == 0) &&
            ((!strncmp(argv[cnt], "-tcp", strlen("-tcp") + 1)) || (!strncmp(argv[cnt], "-V", strlen("-V") + 1)))) {
            tcp_flag = 1;
        }
        if (!strncmp(argv[cnt], "address", strlen("address") + 1)) {
            is_client = 1;
        }
        ret = tool_roce_test_get_param(argc, argv, &cnt, &cmd_info[*size], size);
        if (ret) {
            roce_err("Tool get roce test param failed. (ret=%d; param=\"%s\")", ret, perftest_list[i]);
            return ret;
        }
        ret = verify_little_param(argc, param, cmd_info, is_client, start_cnt);
        if (ret) {
            roce_err("Verify little param failed. (ret=%d)", ret);
            return ret;
        }
    }

#ifndef CFG_SOC_PLATFORM_CLOUD
    if (tcp_flag == 0) {
        DSMI_PRINT_INFO("Please add \"-tcp\"");
        return UDA_PARAM_ERR;
    }
#endif
    return 0;
}

int tool_roce_test_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int size = 1;
    struct perftest_cmd_info cmd_info[MAX_CMD_COUNT];

    if (param == NULL || argv[0] == NULL) {
        roce_err("Input param is NULL.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    cmd_info[0].ipv6_index = 0;
    if (is_ipv6_cmd(&argc, &argv)) {
        cmd_info[0].ipv6_index = 1;
        if (is_atlas_9000_a3(param)) {
            roce_err("Ipv6 atlas_9000_a3 command is not supported.");
            return UDA_TOOL_OP_NOT_SUPPORT_IPV6_ERR;
        }
    }

    if (argc == 0 || argc > MAX_CMD_COUNT) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_HALF_CMD_ERR;
    }

    if (!strncmp(argv[0], "reset", RESET_CMP_LEN)) {
        ret = tool_roce_test_reset(argc, param);
        if (ret) {
            roce_err("Tool roce perftest reset failed. (ret=%d; dev_id=%d)", ret, param->logic_id);
        }
        return ret;
    }
    param->get_cmd_flag = true; // hccn_tool -i 0 -roce_test 除reset外，资料/测试那边归类为查询类命令行
    ret = tool_get_perftest_cmd_info(argc, argv, param, cmd_info, &size);
    if (ret != 0) {
        roce_err("Tool roce test get perftest cmd info failed. (ret=%d; dev_id=%d)", ret, param->logic_id);
        return ret;
    }

    ret = tool_roce_perftest_handle(argc, argv, param, cmd_info, size);
    if (ret) {
        roce_err("Tool roce perftest handle failed. (ret=%d; dev_id=%d)", ret, param->logic_id);
        return ret;
    }
    return 0;
}

STATIC int tool_get_eth_speed(int argc, int logic_id)
{
    int ret;
    int speed = 0;

    if (argc != 0) {
        roce_err("err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    ret = dsmi_get_eth_speed(logic_id, &speed);
    if (ret) {
        roce_err("dsmi_get_eth_speed fail, ret:%d, dev_id:%d", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    if (speed != ETH_SPEED_UNKNOWN) {
        TOOL_PRINT_INFO("Speed: %d Mb/s", speed);
    } else {
        TOOL_PRINT_INFO("Speed: Unknown!");
    }
    return 0;
}

int tool_speed_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (param == NULL) {
        roce_err("Param is NULL.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    if (argc == 0) {
        roce_err("err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_eth_speed(argc - 1, param->logic_id);
        if (ret) {
            roce_err("tool_get_eth_speed fail, ret:%d, logic_id:%d", ret, param->logic_id);
            return ret;
        }
    } else {
        roce_err("Tool_speed_cmd_execute invalid operation.\n");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

STATIC int tool_get_eth_duplex(int argc, int logic_id)
{
    int ret;
    int duplex = 0;

    if (argc != 0) {
        roce_err("err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    ret = dsmi_get_eth_duplex(logic_id, &duplex);
    if (ret) {
        roce_err("dsmi_get_eth_duplex fail, ret:%d, dev_id:%d", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    if (duplex == DUPLEX_FULL) {
        TOOL_PRINT_INFO("Duplex: Full");
    } else if (duplex == DUPLEX_HALF) {
        TOOL_PRINT_INFO("Duplex: Half");
    } else {
        TOOL_PRINT_INFO("Duplex: Unknown!");
    }
    return 0;
}

int tool_duplex_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (param == NULL) {
        roce_err("Param is NULL.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    if (argc == 0) {
        roce_err("err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_eth_duplex(argc - 1, param->logic_id);
        if (ret) {
            roce_err("tool_get_eth_duplex fail, ret:%d, logic_id:%d", ret, param->logic_id);
            return ret;
        }
    } else {
        roce_err("Tool_duplex_cmd_execute invalid operation.\n");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

STATIC int tool_get_bp_status(int argc, int logic_id)
{
    int ret;
    unsigned int reg[SSU_BP_STATUS_NUM];
    int i;

    if (argc != 0) {
        roce_err("err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_ssu_reg(logic_id, reg, SSU_BP_STATUS_NUM);
    if (ret) {
        roce_err("dsmi get eth reg fail! ret[%d] logic_id[%d]", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    TOOL_PRINT_INFO("\n********** Backpressure reg status **********\n");
    for (i = 0; i < SSU_BP_STATUS_NUM; i++) {
        TOOL_PRINT_INFO("SSU_BP_STATUS_%d, reg value is 0x%08x.", i, reg[i]);
    }
    return 0;
}

int tool_bp_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (param == NULL) {
        roce_err("Param is NULL");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    if (argc == 0) {
        roce_err("err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_bp_status(argc - 1, param->logic_id);
        if (ret) {
            roce_err("tool get bp status fail! ret=%d, logic_id=%d", ret, param->logic_id);
            return ret;
        }
    } else {
        roce_err("Tool bp cmd execute invalid operation.\n");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}

STATIC int tool_display_array(char *name, unsigned char *arr, int sz)
{
    char str[MAX_STR_LEN] = {0};
    int arr_size = sz;
    int i;
    int ele_num;
    int alread_print_size = 0;
    int str_idx;
    int ret;

    TOOL_PRINT_INFO("%s:", name);
    while (arr_size) {
        ret = memset_s(str, MAX_STR_LEN, 0, MAX_STR_LEN);
        if (ret) {
            roce_err("memset error when display array. (array[%s]; ret=%d)\n", name, ret);
            return ret;
        }

        str_idx = 0;
        if (arr_size < MAX_ELEMENT_NUM) {
            ele_num = arr_size;
            arr_size = 0;
        } else {
            ele_num = MAX_ELEMENT_NUM;
            arr_size -= MAX_ELEMENT_NUM;
        }

        ret = sprintf_s(str + str_idx, MAX_STR_LEN - str_idx, HEAD_FORMAT, alread_print_size);
        if (ret < 0) {
            roce_err("sprintf error when display array. (array[%s]; ret=%d)\n", name, ret);
            return ret;
        }
        str_idx += HEAD_LEN;

        for (i = 0; i < ele_num; ++i) {
            ret = sprintf_s(str + str_idx, MAX_STR_LEN - str_idx, ELE_FORMAT, arr[alread_print_size + i]);
            if (ret < 0) {
                roce_err("sprintf error when display array. (array[%s]; ret=%d)\n", name, ret);
                return ret;
            }
            str_idx += ELE_LEN;
        }
        alread_print_size += ele_num;
        TOOL_PRINT_INFO("%s", str);
    }
    return 0;
}

STATIC int tool_get_ctle_info(int logic_id)
{
    int ret;
    int i;
    int j;
    struct hilink_ctle_data s_ctle_data[MAX_SERDES_NUM] = {{{0}}};

    ret = dsmi_get_serdes_ctle_info(logic_id, s_ctle_data);
    if (ret) {
        roce_err("DSMI get serdes ctle info fail. (ret=%d; dev_id=%d)\n", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    for (i = 0; i < MAX_PRINT_NUM; ++i) {
        TOOL_PRINT_INFO("----SERDES %d CTLE INFO:----------------------------", i);
        for (j = 0; j < MAC_DFX_INDEX_THREE; ++j) {
            TOOL_PRINT_INFO("CTLE %d info:", j);
            TOOL_PRINT_INFO("%-40s:0x%08X", "ctlebst", s_ctle_data[i].ctlebst[j]);
            TOOL_PRINT_INFO("%-40s:0x%08X", "ctlecmband", s_ctle_data[i].ctlecmband[j]);
            TOOL_PRINT_INFO("%-40s:0x%08X", "ctlermband", s_ctle_data[i].ctlermband[j]);
            TOOL_PRINT_INFO("%-40s:0x%08X", "ctleza", s_ctle_data[i].ctleza[j]);
            TOOL_PRINT_INFO("%-40s:0x%08X", "ctlesqh", s_ctle_data[i].ctlesqh[j]);
            TOOL_PRINT_INFO("%-40s:0x%08X", "ctleactgn", s_ctle_data[i].ctleactgn[j]);
        }
        TOOL_PRINT_INFO("CTLE common info:");
        TOOL_PRINT_INFO("%-40s:0x%08X", "ctlepassgn", s_ctle_data[i].ctlepassgn);
        TOOL_PRINT_INFO("%-40s:0x%08X", "ctlerefsel", s_ctle_data[i].ctlerefsel);
        TOOL_PRINT_INFO("%-40s:0x%08X", "ctleibiastune", s_ctle_data[i].ctleibiastune);
        TOOL_PRINT_INFO("%-40s:0x%08X", "alos", s_ctle_data[i].alos);
        TOOL_PRINT_INFO("%-40s:0x%08X", "lpbk", s_ctle_data[i].lpbk);
    }
    TOOL_PRINT_INFO("---------------------------------------------------");
    return ret;
}

STATIC int tool_get_dfe_info(int logic_id)
{
    int ret;
    int i;
    struct hilink_dfe_data s_dfe_data[MAX_SERDES_NUM] = {{{0}}};

    ret = dsmi_get_serdes_dfe_info(logic_id, s_dfe_data);
    if (ret) {
        roce_err("DSMI get serdes dfe info fail. (ret=%d; dev_id=%d)\n", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    for (i = 0; i < MAX_PRINT_NUM; ++i) {
        TOOL_PRINT_INFO("----SERDES %d DFE INFO:-----------------------------", i);
        ret = tool_display_array("DFE Fix Tap info:", s_dfe_data[i].dfefxtap, MAX_DFE_FIX_TAP_NUM);
        if (ret) {
            roce_err("DFE info print error. (ret=%d)\n", ret);
            return ret;
        }
        ret = tool_display_array("DFE Floating Tap info:", s_dfe_data[i].floatingtap, MAX_DFE_FLOAT_TAP_NUM);
        if (ret) {
            roce_err("DFE info print error. (ret=%d)\n", ret);
            return ret;
        }
    }
    TOOL_PRINT_INFO("---------------------------------------------------");

    return ret;
}

STATIC int tool_get_ffe_info(int logic_id)
{
    int ret;
    int i;
    struct hilink_ffe_data s_ffe_data[MAX_SERDES_NUM] = {{0}};

    ret = dsmi_get_serdes_ffe_info(logic_id, s_ffe_data);
    if (ret) {
        roce_err("DSMI get serdes ffe info fail. (ret=%d; dev_id=%d)\n", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    for (i = 0; i < MAX_PRINT_NUM; ++i) {
        TOOL_PRINT_INFO("----SERDES %d FFE INFO:-----------------------------", i);
        TOOL_PRINT_INFO("%-40s:0x%08X", "pre2", s_ffe_data[i].pre2);
        TOOL_PRINT_INFO("%-40s:0x%08X", "pre1", s_ffe_data[i].pre1);
        TOOL_PRINT_INFO("%-40s:0x%08X", "main", s_ffe_data[i].main);
        TOOL_PRINT_INFO("%-40s:0x%08X", "post1", s_ffe_data[i].post1);
        TOOL_PRINT_INFO("%-40s:0x%08X", "post2", s_ffe_data[i].post2);
    }
    TOOL_PRINT_INFO("---------------------------------------------------");
    return ret;
}

STATIC int tool_get_serdes_info(int argc, struct tool_param *param)
{
    int ret;

    if (argc != 0) {
        roce_err("err argc. (argc=%d)\n", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

#ifdef HCCN_TOOL_SHOW_LIST
    // 910A获取信息方式
    ret = tool_get_ctle_info(param->logic_id);
    if (ret) {
        roce_err("tool get ctle info fail. (ret=%d; dev_id=%d)\n", ret, param->logic_id);
        return ret;
    }

    ret = tool_get_ffe_info(param->logic_id);
    if (ret) {
        roce_err("tool get ffe info fail. (ret=%d; dev_id=%d)\n", ret, param->logic_id);
        return ret;
    }

    ret = tool_get_dfe_info(param->logic_id);
    if (ret) {
        roce_err("tool get dfe info fail. (ret=%d; dev_id=%d)\n", ret, param->logic_id);
        return ret;
    }
#else
    ret = tool_get_npu_serders_snr(param);
    if (ret != 0) {
        roce_err("Tool get npu serdes snr fail. (ret=%d, dev_id=%d)\n", ret, param->logic_id);
    }
    return ret;
#endif
    return 0;
}

STATIC int tool_get_fec_info(int argc, int logic_id)
{
    int ret;
    int mode = 0;

    if (argc != 0) {
        roce_err("err argc. (argc=%d)\n", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_fec_info(logic_id, &mode);
    if (ret) {
        roce_err("DSMI get fec mode info fail. (ret=%d; dev_id=%d)\n", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    if (((unsigned int)mode & AUTO_FLAG) == FEC_MODE_AUTO) {
        TOOL_PRINT_INFO("fec mode: auto FEC mode");
        return ret;
    }

    switch (mode) {
        case FEC_MODE_NO:
            TOOL_PRINT_INFO("fec mode: no FEC mode");
            break;
        case FEC_MODE_RS:
            TOOL_PRINT_INFO("fec mode: rs FEC mode");
            break;
        default:
            roce_err("DSMI get fec mode info error. (mode=%d; dev_id=%d)\n", mode, logic_id);
            return FEC_GET_MODE_ERROR;
    }
    return ret;
}

STATIC int tool_fec_str2char(const char *fec_str, char *mode)
{
    int i;

    for (i = 0; i < FEC_MODE_TYPE_MAX; i++) {
        if (strncmp(fec_str, fec_mode_str[i], strlen(fec_mode_str[i]) + 1) == 0) {
            *mode = i + '0';
            return 0;
        }
    }

    roce_err("Check mode para err.");

    return UDA_PARAM_INVALID_ERR;
}

STATIC int tool_set_fec_info(int argc, char **argv, struct tool_param *param)
{
    int ret;
    char mode;
    struct dsmi_shaping_info shaping_info = {0};

    if (argc != ARGC_ID_2) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (strncmp(argv[0], "encoding", strlen("encoding") + 1)) {
        roce_err("Invalid option.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    ret = tool_fec_str2char(argv[1], &mode);
    if (ret != 0) {
        roce_err("Invalid fec.");
        return ret;
    }

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    if (mode == '1') {
        ret = dsmi_get_port_shaping(param->logic_id, 0, &shaping_info);
        if (ret) {
            roce_err("dsmi get shaping info fail ret[%d] logic_id[%d]", ret, param->logic_id);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
        if (shaping_info.bw_max_cap == SPEED_200G) { // 200G的时候不支持fec模式为off
            roce_err("fec mode off is not suppoerted in 200G");
            TOOL_PRINT_INFO("fec mode off is not supported in 200G");
            return UDA_DSMI_EXE_ERR;
        }
    }
#endif

    ret = dsmi_set_fec_info(param->logic_id, mode);
    if (ret != 0) {
        roce_err("DSMI set fec failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return ret;
}

STATIC void tool_display_pcs_bd0_reg(struct hilink_port_info *pinfo)
{
    TOOL_PRINT_INFO("PCS BD0 REG INFO:");
    TOOL_PRINT_INFO("%-18s:%s", "vendor_name", pinfo->vendor_name);
    TOOL_PRINT_INFO("%-18s:0x%08X", "port_type", pinfo->port_type);
    TOOL_PRINT_INFO("%-18s:0x%08X", "port_sub_type", pinfo->port_sub_type);
}

STATIC void tool_display_pcs_bd1_reg(struct hilink_port_info *pinfo)
{
    TOOL_PRINT_INFO("PCS BD1 REG INFO:");
    TOOL_PRINT_INFO("%-18s:0x%08X    %-13s:0x%08X    %-18s:0x%08X", "cable_length", pinfo->cable_length, "cable_temp",
                    pinfo->cable_temp, "max_speed", pinfo->max_speed);
    TOOL_PRINT_INFO("%-18s:0x%08X    %-13s:0x%08X    %-18s:0x%08X", "sfp_type", pinfo->sfp_type, "rsvd2", pinfo->rsvd2,
                    "power0", pinfo->power[PCS_POWER_INDEX_0]);
    TOOL_PRINT_INFO("%-18s:0x%08X    %-13s:0x%08X    %-18s:0x%08X", "power1", pinfo->power[PCS_POWER_INDEX_1], "power2",
                    pinfo->power[PCS_POWER_INDEX_2], "power3", pinfo->power[PCS_POWER_INDEX_3]);
}

STATIC void tool_display_pcs_bd2_reg(struct hilink_port_info *pinfo)
{
    TOOL_PRINT_INFO("PCS BD2 REG INFO:");
    TOOL_PRINT_INFO("%-18s:0x%08X    %-13s:0x%08X    %-18s:0x%08X", "an_state", pinfo->an_state, "fec", pinfo->fec,
                    "speed", pinfo->speed);
    TOOL_PRINT_INFO("%-18s:0x%08X    %-13s:0x%08X    %-18s:0x%08X", "gpio_insert", pinfo->gpio_insert, "alos",
                    pinfo->alos, "rx_los", pinfo->rx_los);
    TOOL_PRINT_INFO("%-18s:0x%08X    %-13s:0x%08X    %-18s:0x%08X", "pma_ctrl", pinfo->pma_ctrl, "pma_fifo_reg",
                    pinfo->pma_fifo_reg, "pma_signal_ok_reg", pinfo->pma_signal_ok_reg);
    TOOL_PRINT_INFO("%-18s:0x%08X    %-13s:0x%08X", "pcs_64_66b_reg", pinfo->pcs_64_66b_reg, "rf_lf", pinfo->rf_lf);
}

STATIC int tool_display_pcs_bd3_reg(struct hilink_port_info *pinfo)
{
    int ret;

    TOOL_PRINT_INFO("PCS BD3/4 REG INFO:");
    TOOL_PRINT_INFO("%-18s:0x%08X    %-13s:0x%08X    %-18s:0x%08X", "pcs_link", pinfo->pcs_link, "pcs_mac_link",
                    pinfo->pcs_mac_link, "tx_enable", pinfo->tx_enable);
    TOOL_PRINT_INFO("%-18s:0x%08X    %-13s:0x%08X", "rx_enable", pinfo->rx_enable, "pcs_err_cnt", pinfo->pcs_err_cnt);
    ret = tool_display_array("eq_data", pinfo->eq_data, PCS_EQ_DATA_DISPLAY_NUM);
    if (ret) {
        roce_err("PCS bd3 info print error. (ret=%d)\n", ret);
        return ret;
    }
    return ret;
}

STATIC int tool_display_pcs_bd5_reg(struct hilink_port_info *pinfo)
{
    int ret;

    TOOL_PRINT_INFO("PCS BD5/6 REG INFO:");
    TOOL_PRINT_INFO("%-25s:0x%08X    %-25s:0x%08X", "his_link_machine_state", pinfo->his_link_machine_state,
                    "cur_link_machine_state", pinfo->cur_link_machine_state);
    TOOL_PRINT_INFO("%-25s:0x%08X    %-25s:0x%08X", "his_machine_state_length", pinfo->his_machine_state_length,
                    "cur_machine_state_length", pinfo->cur_machine_state_length);
    ret = tool_display_array("his_machine_state_data", pinfo->his_machine_state_data,
                             PCS_MACHINE_STATE_DATA_DISPLAY_NUM);
    if (ret) {
        roce_err("PCS bd5 info print error. (ret=%d)\n", ret);
        return ret;
    }
    ret = tool_display_array("cur_machine_state_data", pinfo->cur_machine_state_data,
                             PCS_MACHINE_STATE_DATA_DISPLAY_NUM);
    if (ret) {
        roce_err("PCS bd5 info print error. (ret=%d)\n", ret);
        return ret;
    }

    return ret;
}

STATIC int tool_get_pcs_info(int argc, int logic_id)
{
    int ret;
    struct hilink_port_info port_info;

    if (argc != 0) {
        roce_err("err argc. (argc=%d)\n", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_pcs_info(logic_id, &port_info);
    if (ret) {
        roce_err("DSMI get pcs info fail. (ret=%d; dev_id=%d)\n", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    tool_display_pcs_bd0_reg(&port_info);
    tool_display_pcs_bd1_reg(&port_info);
    tool_display_pcs_bd2_reg(&port_info);
    ret = tool_display_pcs_bd3_reg(&port_info);
    if (ret) {
        roce_err("tool get pcs info print fail. (ret=%d)\n", ret);
        return ret;
    }
    ret = tool_display_pcs_bd5_reg(&port_info);
    if (ret) {
        roce_err("tool get pcs info print fail. (ret=%d)\n", ret);
        return ret;
    }
    return ret;
}

int tool_serdes_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int argc_tmp = argc - 1;

    if (param == NULL) {
        roce_err("Param is NULL.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    if (argc == 0) {
        roce_err("err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_serdes_info(argc_tmp, param);
        if (ret) {
            roce_err("get serdes info fail. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
    } else {
        roce_err("Get serdes info fail, invalid operation.\n");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

int tool_fec_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id;
    int argc_tmp = argc - 1;
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    TOOL_PRINT_INFO("This device does not support switching fec mode.");
    param->get_cmd_flag = true; // 不打印 cmd execute success
#else
    if (param == NULL) {
        roce_err("Param is NULL.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    if (argc == 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    logic_id = param->logic_id;

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_fec_info(argc_tmp, logic_id);
        if (ret != 0) {
            roce_err("Get fec info fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        ret = tool_set_fec_info(argc_tmp, argv + 1, param);
        if (ret != 0) {
            roce_err("Set fec info fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
        ret = cfg_write_conf(param->phy_id, "encoding", argv[ARGC_ID_2]);
        if (ret != 0) {
            roce_err("Write fec cfg error. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        roce_err("Invalid operation.\n)");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
#endif

    return 0;
}

#define FEC_ARGV_LEN 15
int tool_cfg_fec_recovery(struct tool_param *param)
{
#ifndef CFG_SOC_PLATFORM_CLOUD_V2
    int ret;
    char fec_mode[FEC_MODE_STR_MAX + 1] = {0};
    char *argv[ARGC_ID_2] = {NULL};
    argv[0] = "encoding";

    ret = cfg_read_conf(param->phy_id, "encoding", fec_mode, sizeof(fec_mode));
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            roce_info("The EFC conf name is not found in config file, skipped. (ret=%d)", ret);
            return 0;
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        roce_info("FEC need recover cfg. (logic_id=%d)", param->logic_id);
        argv[ARGV_ID_1] = fec_mode;
        ret = tool_set_fec_info(ARGC_ID_2, argv, param);
        if (ret != 0) {
            roce_err("FEC set failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        }
    }

    return ret;
#else
    return 0;
#endif
}

int tool_pcs_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id;
    int argc_tmp = argc - 1;

    if (param == NULL) {
        roce_err("Param is NULL.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    if (argc == 0) {
        roce_err("err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    logic_id = param->logic_id;

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_pcs_info(argc_tmp, logic_id);
        if (ret) {
            roce_err("get pcs info fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else {
        roce_err("Get pcs info fail, invalid operation.\n");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

static void tool_display_status_change(int index, unsigned int status_changes, struct err_record record)
{
    char tmp[PART_STR_LEN];
    char err[INDEX_TWO][PART_STR_LEN];
    int ret;
    int len;
    int sum_len;
    int err_len[INDEX_TWO];

    /* when status is start, the nums of change don't need to be output */
    if (index != 0) {
        ret = sprintf_s(tmp, PART_STR_LEN, "%u", status_changes);
        if (ret < 0) {
            roce_err("Sprintf error when display the nums of status change. (ret=%d)", ret);
            return;
        }
        len = strlen(tmp);
        TOOL_PRINT_INFO("|           |             %*s%s%*s|", (PART_TIMES_LEN - len) / DIVIDE_TWO, "", tmp,
                        TOTAL_LEN - (PART_TIMES_LEN + len) / DIVIDE_TWO, "");
    }

    ret = sprintf_s(tmp, PART_STR_LEN, "%s", adapt_status_str[index]);
    if (ret < 0) {
        roce_err("Sprintf error when display status. (ret=%d)", ret);
        return;
    }
    len = strlen(tmp);
    ret = sprintf_s(err[0], PART_STR_LEN, "%s", err_code_str[(int)record.cur_err_code]);
    if (ret < 0) {
        roce_err("Sprintf error when display cur err code. (ret=%d)", ret);
        return;
    }
    err_len[0] = strlen(err[0]);
    ret = sprintf_s(err[1], PART_STR_LEN, "%s", err_code_str[(int)record.last_err_code]);
    if (ret < 0) {
        roce_err("Sprintf error when display last err code. (ret=%d)", ret);
        return;
    }
    err_len[1] = strlen(err[1]);
    sum_len = err_len[0] + err_len[1] + INDEX_TWO;
    err_len[0] = (TOTAL_LEN - PART_TIMES_LEN - sum_len) / DIVIDE_TWO;
    err_len[1] = TOTAL_LEN - PART_TIMES_LEN - err_len[0] - sum_len;
    TOOL_PRINT_INFO("|%*s%s%*s                 %*s%s, %s%*s|", (PART_STR_LEN - len) / STEP_TWO, "", tmp,
                    PART_STR_LEN - (PART_STR_LEN - len) / STEP_TWO - len, "", err_len[0], "", err[0], err[1],
                    err_len[1], "");
}

static int tool_display_adapt_status(struct hilink_adapt_status_info *adapt_status_info)
{
    int ret;

    if (adapt_status_info->status[LINK_STATUS_OUT].cnt != 0) {
        TOOL_PRINT_INFO("adapt status         : %s", "Normal");
        ret = UDA_SUCCESS;
    } else {
        TOOL_PRINT_INFO("adapt status         : %s", "Abnormal");
        ret = UDA_FAIL;
    }

    TOOL_PRINT_INFO("cur adapt status     : %s", adapt_status_str[(int)adapt_status_info->cur_adapt_status]);
    TOOL_PRINT_INFO("last adapt status    : %s", adapt_status_str[(int)adapt_status_info->last_adapt_status]);

    return ret;
}

STATIC int tool_get_adapt_status_info(int logic_id, int port_id)
{
    struct hilink_adapt_status_info status_info = {0};
    int ret;
    int i;

    ret = dsmi_get_adapt_status_info(logic_id, port_id, &status_info);
    if (ret) {
        roce_err("Dsmi get adapt status info fail. (ret=%d; dev_id=%d; port_id=%d)", ret, logic_id, port_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    TOOL_PRINT_INFO("MAC ADAPT STATUS INFORMATION:");
    ret = tool_display_adapt_status(&status_info);
    TOOL_PRINT_INFO(
        "link setup start time: %04u-%02u-%02u-%02u:%02u:%02u",
        (unsigned short)(status_info.start_date >> MAC_DFX_INDEX_SIXTEEN),
        (unsigned char)(status_info.start_date >> MAC_DFX_INDEX_EIGHT), (unsigned char)status_info.start_date,
        (unsigned char)(status_info.start_time >> MAC_DFX_INDEX_SIXTEEN),
        (unsigned char)(status_info.start_time >> MAC_DFX_INDEX_EIGHT), (unsigned char)status_info.start_time);
    TOOL_PRINT_INFO("link setup  end  time: %04u-%02u-%02u-%02u:%02u:%02u",
                    (unsigned short)(status_info.end_date >> MAC_DFX_INDEX_SIXTEEN),
                    (unsigned char)(status_info.end_date >> MAC_DFX_INDEX_EIGHT), (unsigned char)status_info.end_date,
                    (unsigned char)(status_info.end_time >> MAC_DFX_INDEX_SIXTEEN),
                    (unsigned char)(status_info.end_time >> MAC_DFX_INDEX_EIGHT), (unsigned char)status_info.end_time);
    if (ret == UDA_SUCCESS) {
        TOOL_PRINT_INFO("Adapt Status Transition Process: NA");
    } else {
        TOOL_PRINT_INFO("Adapt Status Transition Process:");
        TOOL_PRINT_INFO("+------------------------------------------------------------------------------------------+");
        TOOL_PRINT_INFO("|    from begin to end   |  change times  |                   err_record                   |");
        TOOL_PRINT_INFO("+------------------------------------------------------------------------------------------+");
        for (i = 0; i < MAX_STATUS_NUM; ++i) {
            if (i == 0) {
                tool_display_status_change(i, 0, status_info.status[i].err);
            } else {
                tool_display_status_change(i, status_info.status[i].cnt, status_info.status[i].err);
            }
        }
        TOOL_PRINT_INFO("+------------------------------------------------------------------------------------------+");
    }

    return 0;
}

static int tool_get_port_id(int argc, char **argv, int *port_id)
{
    int i;
    int len;
    const char *para_info = NULL;
    char **argv_tmp = argv;

    argv_tmp += INDEX_ONE;
    para_info = *argv_tmp;

    if (!strncmp(para_info, "port", strlen("port") + 1)) {
        argv_tmp += INDEX_ONE;
        para_info = *argv_tmp;
        len = strlen(para_info);
        if (len > 1) {
            roce_err("Port id check fail! Valid length is 1. (length=%d)", len);
            return UDA_PARAM_INVALID_ERR;
        }
        for (i = 0; i < len; i++) {
            if ((para_info[i] < '0') || (para_info[i] > '7')) {
                roce_err("Get invalid port id. (port_id=%c)", para_info[i]);
                return UDA_PARAM_INVALID_ERR;
            }
        }
        *port_id = (int)strtol(para_info, NULL, NUMBER_BASE);
    } else {
        roce_err("Error cmd.(cmd=\"%s\")", para_info);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}

int tool_adapt_status_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id;
    int port_id = 0;

    if (argc != PARAM_COUNT_THREE) {
        roce_err("Error argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    logic_id = param->logic_id;

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_port_id(argc, argv, &port_id);
        if (ret) {
            roce_err("Get mac id info fail. (ret=%d; port_id=%d)", ret, port_id);
            return ret;
        }

        ret = tool_get_adapt_status_info(logic_id, port_id);
        if (ret) {
            roce_err("Get adapt status info fail. (ret=%d; logic_id=%d; port_id=%d)", ret, logic_id, port_id);
            return ret;
        }
    } else {
        roce_err("Get adapt status info fail, invalid operation.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

int tool_get_ip_rule_table(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct send_data_info data_info;
    struct ip_rule_cmd_info cmd_info = {0};

    if (argc != 0) {
        roce_err("Tool get rule param failed. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    cmd_info.cmd_type = 'g';

    TOOL_SET_SEND_DATA(data_info, DS_IP_RULE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);
    data_info.out_buf = (char *)calloc(RDFX_EXT_REPLY_MAX_LEN, sizeof(char));
    if (data_info.out_buf == NULL) {
        roce_err("Tool get rule calloc out buf failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }
    data_info.size_out = RDFX_EXT_REPLY_MAX_LEN;

    ret = dsmi_send_data(param->logic_id, 0, &data_info);
    if (ret) {
        free(data_info.out_buf);
        roce_err("Dsmi send data failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        ret = (ret == UDA_TOOL_SYS_BUSY_ERR) ? UDA_TOOL_SYS_BUSY_ERR : dsmi_analysis_dsmi_ret_to_uda(ret);
        return ret;
    }

    data_info.out_buf[RDFX_EXT_REPLY_MAX_LEN - 1] = '\0';
    DSMI_PRINT_INFO("ip rule table:");
    DSMI_PRINT_INFO("%s", data_info.out_buf);
    free(data_info.out_buf);

    return 0;
}

static int tool_add_ip_rule_table_conf(int phy_id, const char *dir, const char *ip, const char *table)
{
    int ret;
    char *cmd_buffer = NULL;

    cmd_buffer = (char *)calloc(RDFX_EXT_CMD_MAX_LEN, sizeof(char));
    if (cmd_buffer == NULL) {
        roce_err("Tool add ip rule calloc in buf failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = sprintf_s(cmd_buffer, RDFX_EXT_CMD_MAX_LEN, "add %s %s table %s", dir, ip, table);
    if (ret <= 0) {
        free(cmd_buffer);
        roce_err("Tool construct cmdline failed. (ret=%d)", ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    ret = tool_add_ip_rule_conf(phy_id, cmd_buffer);
    free(cmd_buffer);
    return ret;
}

int tool_add_ip_rule_table(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct send_data_info data_info;
    struct ip_rule_cmd_info cmd_info = {0};
    char dir[MAX_PARAM_LEN] = {0};
    char ip[MAX_PARAM_LEN] = {0};
    char table[MAX_PARAM_LEN] = {0};

    if (argc != ARGC_NUM_6) {
        roce_err("Tool add ip rule param failed. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_get_param(argc, argv, "dir", dir, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "ip", ip, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "table", table, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }

    ret = tool_prepare_add_ip_rule_param(dir, ip, table, &cmd_info);
    if (ret) {
        roce_err("Tool add ip rule prepare param failed.");
        return ret;
    }

    TOOL_SET_SEND_DATA(data_info, DS_IP_RULE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);

    ret = dsmi_send_data(param->logic_id, 0, &data_info);
    if (ret) {
        roce_err("Dsmi send data failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    if (!check_udhcpc_write_hccn_conf(param, 0)) {
        ret = tool_add_ip_rule_table_conf(param->phy_id, dir, ip, table);
        if (ret) {
            roce_err("Add ip rule to config file failed. (ret=%d)", ret);
            return ret;
        }
    }
    return 0;
}

static int tool_prepare_del_ip_rule_param(const char *dir, const char *ip, struct ip_rule_cmd_info *cmd_info)
{
    struct in_addr addr;
    int ret;

    cmd_info->cmd_type = 'd';
    if (strncmp(dir, "from", strlen("from") + 1) == 0) {
        cmd_info->dir = IP_RULE_DIR_FROM;
    } else if (strncmp(dir, "to", strlen("to") + 1) == 0) {
        cmd_info->dir = IP_RULE_DIR_TO;
    } else {
        return UDA_PARAM_INVALID_ERR;
    }

    ret = inet_pton(AF_INET, ip, &addr);
    if (ret <= 0) {
        roce_err("Invalid IPv4 address. (ret=%d; errno=%d)", ret, errno);
        return UDA_PARAM_IP_INVALID_ERR;
    }
    cmd_info->ip_address = addr.s_addr;

    return 0;
}

int tool_del_ip_rule_table(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct send_data_info data_info;
    struct ip_rule_cmd_info cmd_info = {0};
    char dir[MAX_PARAM_LEN] = {0};
    char ip[MAX_PARAM_LEN] = {0};

    if (argc != ARGC_NUM_4) {
        roce_err("Tool del rule param failed. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_get_param(argc, argv, "dir", dir, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "ip", ip, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }

    ret = tool_prepare_del_ip_rule_param(dir, ip, &cmd_info);
    if (ret) {
        roce_err("Tool del rule prepare param failed.");
        return ret;
    }

    TOOL_SET_SEND_DATA(data_info, DS_IP_RULE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);

    ret = dsmi_send_data(param->logic_id, 0, &data_info);
    if (ret) {
        roce_err("Dsmi send data failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        if (ret == ENOENT) {
            return UDA_TOOL_CONF_NOT_EXIST_ERR;
        }
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    ret = tool_del_ip_rule_conf(param->phy_id, dir, ip);
    if (ret) {
        roce_err("Del ip rule from config file failed. (ret=%d)", ret);
        return ret;
    }

    return 0;
}

int tool_ip_rule_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc == 0) {
        roce_err("Tool_ip_rule_cmd_execute failed. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_ip_rule_table(argc - 1, argv + 1, param);
        if (ret) {
            roce_err("Tool_get_ip_rule_table failed. (ret=%d)", ret);
            return ret;
        }
    } else if (!strncmp(argv[0], "-a", strlen("-a") + 1)) {
        ret = tool_add_ip_rule_table(argc - 1, argv + 1, param);
        if (ret) {
            roce_err("Tool_add_rule_table failed. (ret=%d)", ret);
            return ret;
        }
    } else if (!strncmp(argv[0], "-d", strlen("-d") + 1)) {
        ret = tool_del_ip_rule_table(argc - 1, argv + 1, param);
        if (ret) {
            roce_err("Tool_del_rule_table failed. (ret=%d)", ret);
            return ret;
        }
    } else {
        roce_err("Ip rule cmd get option failed.\n");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}

static int tool_add_ip_route_table_conf(int phy_id, struct ip_route_cmd_param *cmd_param)
{
    char *cmd_buf = NULL;
    int ret;

    cmd_buf = (char *)calloc(RDFX_EXT_CMD_MAX_LEN, sizeof(char));
    if (cmd_buf == NULL) {
        roce_err("Tool add ip rule calloc in buf failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = sprintf_s(cmd_buf, RDFX_EXT_CMD_MAX_LEN, "add %s/%s via %s dev %s table %s", cmd_param->ip,
                    cmd_param->ip_mask, cmd_param->via, cmd_param->dev, cmd_param->table);
    if (ret <= 0) {
        free(cmd_buf);
        roce_err("Tool construct cmdline failed. (ret=%d)", ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    ret = tool_add_ip_route_conf(phy_id, cmd_buf);
    free(cmd_buf);

    return ret;
}

static int tool_parse_add_ip_route_param(int argc, char **argv, struct ip_route_cmd_param *cmd_param)
{
    int ret;

    ret = tool_get_param(argc, argv, "ip", cmd_param->ip, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "via", cmd_param->via, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "dev", cmd_param->dev, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "table", cmd_param->table, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "ip_mask", cmd_param->ip_mask, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }
    return 0;
}

STATIC unsigned int tool_make_big_endian_mask(unsigned int ip_mask)
{
    unsigned int big_endian_mask = 0;
    unsigned int little_endian_mask = 0;
    unsigned int temp_mask = 0xFFFFFFFF;

    // 如果ip_mask为0，下面算法会由0xFFFFFFFF左移32位，导致发生左移位长操作，会发生未定义行为
    if (ip_mask == 0) {
        return 0;
    }

    // 生成掩码
    little_endian_mask = (temp_mask << (TOOL_BIT_OFFSET_32 - ip_mask)) & 0xFFFFFFFF;
    // 大小端转换
    big_endian_mask |= (little_endian_mask & 0xFF) << TOOL_BIT_OFFSET_24; // 第1字节移到第4字节
    big_endian_mask |= ((little_endian_mask >> TOOL_BIT_OFFSET_8) & 0xFF) << TOOL_BIT_OFFSET_16; // 第2字节移到第3字节
    big_endian_mask |= ((little_endian_mask >> TOOL_BIT_OFFSET_16) & 0xFF) << TOOL_BIT_OFFSET_8; // 第3字节移到第2字节
    big_endian_mask |= (little_endian_mask >> TOOL_BIT_OFFSET_24) & 0xFF; // 第4字节移到第1字节

    return big_endian_mask;
}

STATIC int tool_ip_route_check(struct tool_param *param, struct ip_route_cmd_info cmd_info)
{
    int ret;
    int port_type = DSMI_ROCE_PORT;
    int port_id = 0;
    unsigned int address;
    unsigned int ip_mask_address;
    ip_addr_t ip_address;
    ip_addr_t mask_address;

    ip_address.ip_type = IPADDR_TYPE_V4;
    mask_address.ip_type = IPADDR_TYPE_V4;
    ret = dsmi_get_device_ip_address(param->logic_id, port_type, port_id, &ip_address, &mask_address);
    if (ret) {
        roce_err("Dsmi_get_device_ip_address fail ret=%d, logic_id[%d]", ret, param->logic_id);
        ret = (ret == DRV_ERROR_NO_DEVICE) ? UDA_DSMI_IPCONF_NOT_PRESET_ERR : UDA_DSMI_EXE_ERR;
        return ret;
    }

    if (!is_network_segment(cmd_info.ip_address, tool_make_big_endian_mask(cmd_info.ip_mask))) {
        roce_err("Address is not network segment");
        return UDA_TOOL_IP_MISMATCH_MASK_ERR;
    }

    address = *(unsigned int *)(void *)&ip_address.u_addr;
    ip_mask_address = *(unsigned int *)(void *)&mask_address.u_addr;
    if (!is_address_gateway_same_segment(address, cmd_info.via_address, ip_mask_address)) {
        roce_err("Address and via are not in the same network segment");
        return UDA_PARAM_DIFF_SEGMT_VIA_ERR;
    }
}

int tool_add_ip_route_table(int argc, char **argv, struct tool_param *param)
{
    int ret, temp_ret;
    struct send_data_info data_info;
    struct ip_route_cmd_info cmd_info = {0};
    struct ip_route_cmd_param cmd_param;

    if (argc != ARGC_NUM_10) {
        roce_err("Tool add ip route param failed. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = memset_s(&cmd_param, sizeof(cmd_param), 0, sizeof(cmd_param));
    if (ret != 0) {
        roce_err("Tool add ip route memset param failed.");
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    ret = tool_parse_add_ip_route_param(argc, argv, &cmd_param);
    if (ret != 0) {
        roce_err("Tool add ip route parse param failed. (ret=%d)", ret);
        return ret;
    }

    ret = tool_prepare_add_ip_route_param(&cmd_param, &cmd_info);
    if (ret) {
        roce_err("Tool add ip route prepare param failed.");
        return ret;
    }

    TOOL_SET_SEND_DATA(data_info, DS_IP_ROUTE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);

    ret = dsmi_send_data(param->logic_id, 0, &data_info);
    if (ret == EEXIST) {
        return UDA_TOOL_IP_ROUTE_TABLE_ALREADY_EXIST_ERR;
    }
    if (ret) {
        temp_ret = tool_ip_route_check(param, cmd_info);
        if (temp_ret) {
            return temp_ret;
        }
        roce_err("Dsmi send failed. (ret=%d; logic_id=%d; err=\"%s\")", ret, param->logic_id, get_cmd_err_str(ret));
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    if (!check_udhcpc_write_hccn_conf(param, 0)) {
        ret = tool_add_ip_route_table_conf(param->phy_id, &cmd_param);
        if (ret) {
            roce_err("Add ip route to config file failed. (ret=%d)", ret);
            return ret;
        }
    }
    return 0;
}

int tool_del_ip_route_table(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct send_data_info data_info;
    struct ip_route_cmd_info cmd_info = {0};
    char table[MAX_PARAM_LEN] = {0};
    char ip_mask[MAX_PARAM_LEN] = {0};
    char ip[MAX_PARAM_LEN] = {0};

    if (argc != ARGC_NUM_6) {
        roce_err("Tool del ip route param failed. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_get_param(argc, argv, "ip", ip, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "table", table, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }
    ret = tool_get_param(argc, argv, "ip_mask", ip_mask, MAX_PARAM_LEN);
    if (ret != 0) {
        return ret;
    }

    ret = tool_prepare_del_ip_route_param(ip, table, ip_mask, &cmd_info);
    if (ret != 0) {
        roce_err("Tool del ip route prepare param failed.");
        return ret;
    }

    TOOL_SET_SEND_DATA(data_info, DS_IP_ROUTE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);

    ret = dsmi_send_data(param->logic_id, 0, &data_info);
    if (ret != 0) {
        roce_err("Dsmi send data failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        if (ret == ESRCH) {
            return UDA_TOOL_CONF_NOT_EXIST_ERR;
        }
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    ret = tool_del_ip_route_conf(param->phy_id, ip, ip_mask, table);
    if (ret != 0) {
        roce_err("Del ip route from config file failed. (ret=%d)", ret);
        return ret;
    }

    return 0;
}

int tool_get_ip_route_table(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct send_data_info data_info;
    struct ip_route_cmd_info cmd_info = {0};
    char table[MAX_PARAM_LEN] = {0};
    unsigned int table_id;

    if (argc == ARGC_NUM_2) {
        ret = tool_get_param(argc, argv, "table", table, MAX_PARAM_LEN);
        if (ret) {
            roce_err("Tool get ip route table param failed.");
            return UDA_PARAM_INVALID_ERR;
        }
        table_id = (unsigned int)strtoul(table, NULL, 0);
        if (table_id > MAX_IP_ROUTE_TAB_IDX) {
            roce_err("Invalid table id in ip rule cmd.");
            return UDA_PARAM_INVALID_ERR;
        }
        cmd_info.table_id = table_id;
    } else if (argc == 0) {
        cmd_info.table_id = MAX_IP_ROUTE_TAB_IDX + 1;
    } else {
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    cmd_info.cmd_type = 'g';

    TOOL_SET_SEND_DATA(data_info, DS_IP_ROUTE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);
    data_info.out_buf = (char *)calloc(RDFX_EXT_REPLY_MAX_LEN, sizeof(char));
    if (data_info.out_buf == NULL) {
        roce_err("Tool get ip route calloc out buf failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }
    data_info.size_out = RDFX_EXT_REPLY_MAX_LEN;

    ret = dsmi_send_data(param->logic_id, 0, &data_info);
    if (ret) {
        roce_err("Dsmi send data failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        if (ret == ENOENT) {
            return UDA_TOOL_ROUTE_TABLE_NOT_EXIST_ERR;
        }
        ret = dsmi_analysis_dsmi_ret_to_uda(ret);
    } else {
        data_info.out_buf[RDFX_EXT_REPLY_MAX_LEN - 1] = '\0';
        DSMI_PRINT_INFO("ip route table:");
        DSMI_PRINT_INFO("%s", data_info.out_buf);
    }
    free(data_info.out_buf);

    return ret;
}

int tool_ip_route_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc == 0) {
        roce_err("Tool_ip_route_cmd_execute failed. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_ip_route_table(argc - 1, argv + 1, param);
        if (ret) {
            roce_err("Tool_get_ip_route_table failed. (ret=%d)", ret);
            return ret;
        }
    } else if (!strncmp(argv[0], "-a", strlen("-a") + 1)) {
        ret = tool_add_ip_route_table(argc - 1, argv + 1, param);
        if (ret && ret != UDA_TOOL_IP_ROUTE_TABLE_ALREADY_EXIST_ERR) {
            roce_err("Tool_add_ip_route_table failed. (ret=%d)", ret);
        }
        ret = (ret) ? ret : 0;
        return ret;
    } else if (!strncmp(argv[0], "-d", strlen("-d") + 1)) {
        ret = tool_del_ip_route_table(argc - 1, argv + 1, param);
        if (ret) {
            roce_err("Tool_del_ip_route_table failed. (ret=%d)", ret);
            return ret;
        }
    } else {
        roce_err("Ip route cmd get option failed.\n");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

bool is_ipv6_cmd(int *argc, char ***argv)
{
#ifndef HCCN_TOOL_ADAPT_IPV6
    return false;
#endif
    if (argv == NULL || *argc == 0) {
        return false;
    }
    if (!strncmp((*argv)[0], "-inet6", strlen("-inet6") + 1)) {
        *argc = *argc - 1;
        *argv = *argv + 1;
        return true;
    }
    return false;
}
