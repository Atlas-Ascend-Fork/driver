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
#include <sys/utsname.h>
#include "tool_recovery.h"
#include "tool_ops.h"
#include "tool_ext.h"
#include "cmd.h"
#include "dms_devdrv_info_comm.h"
#include "dsmi_common_interface.h"
#include "weak_passwd_dict.h"
#include "tls.h"
#include "tool.h"
#include "hccn_dfx_struct.h"
#include "log.h"
#include "ds_net_ext.h"
#include "ascend_hal.h"
#define WAIT_TIME 1

#define FEC_EEROR_CMD_SIZE 1

#define DOWNGRADE_LANE_SWITCH "downgrade_lane_switch"
#define DOWNGRADE_LANE_FILTER_TIME "downgrade_lane_filter_time"
#define DOWNGRADE_LANE_ENABLE 1
#define DOWNGRADE_LANE_DISABLE 0
#define DOWNGRADE_FILTER_MIN_TIME_S 3
#define DOWNGRADE_FILTER_MAX_TIME_S 8
#define DOWNGRADE_FILE_NUM 2

/* mem_reserve magic numbers */

#define MEM_RESERVE_STRTOL_BASE 10
#define MEM_RESERVE_CONFIG_LENGTH 384
#define MEM_RESERVE_SET_ARGC 4 /* "type %s size %u" format requires 4 arguments */

#define MEM_RESERVE_ARG_IDX_SIZE 2 /* argv index for "size" keyword */

#define MEM_RESERVE_ARG_IDX_VALUE 3 /* argv index for size value */

#define MEM_RESERVE_KB_TO_BYTES 1024
#define MEM_RESERVE_MAX_SIZE_MB 512 /* Max supported mem_size in MB */

static void tool_display_down_info(int phy_id, const struct ds_link_down_info *down_info, int cnt)
{
    (void)fprintf(stdout, "[time %d] =======================\n", cnt);
    (void)fprintf(stdout, "[time %d] ber_cnt          : 0x%x\n", cnt, down_info->pcs_info.err_cnt);
    (void)fprintf(stdout, "[time %d] err_block_cnt    : 0x%x\n", cnt, down_info->pcs_info.err_block_cnt);
    (void)fprintf(stdout, "[time %d] e_blk_cnt        : 0x%x\n", cnt, down_info->pcs_info.e_blk_cnt);
    (void)fprintf(stdout, "[time %d] dec_err_blk_cnt  : 0x%x\n", cnt, down_info->pcs_info.dec_err_blk_cnt);
    (void)fprintf(stdout, "[time %d] test_err_cnt     : 0x%x\n", cnt, down_info->pcs_info.test_err_cnt);
    (void)fprintf(stdout, "[time %d] lane_bip_err_cnt : 0x%x\n", cnt, down_info->pcs_info.lane_bip_err_cnt);

    (void)fprintf(stdout, "[time %d] pcs_link         : 0x%x\n", cnt, down_info->pcs_link);
    (void)fprintf(stdout, "[time %d] mac_link         : 0x%x\n", cnt, down_info->mac_link);
    (void)fprintf(stdout, "[time %d] rf_lf            : 0x%x\n", cnt, down_info->rf_lf);
}

static void tool_display_eth_down_data(int phy_id, const struct ds_link_down_data *down_data)
{
    int cnt = 0;
    int i;

    if (down_data->total_nums > 0) {
        (void)fprintf(stdout, "[devid %d] ======================\n", phy_id);
        (void)fprintf(stdout, "[devid %d] link down record :\n", phy_id);
    } else {
        (void)fprintf(stdout, "[devid %d] link down record : NA\n", phy_id);
        return;
    }

    for (i = (down_data->down_info_idx - 1); i >= 0; i--) {
        tool_display_down_info(phy_id, &down_data->down_info[i], cnt);
        cnt++;
    }

    if ((down_data->total_nums == MAX_LINK_DOWN_NUMS) && (down_data->down_info_idx < MAX_LINK_DOWN_NUMS)) {
        for (i = (MAX_LINK_DOWN_NUMS - 1); i > (down_data->down_info_idx - 1); i--) {
            tool_display_down_info(phy_id, &down_data->down_info[i], cnt);
            cnt++;
        }
    }
    (void)fprintf(stdout, "[time %d] =======================\n", --cnt);
}

int tool_down_data_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    struct ds_link_down_data down_data = {0};
    int argc_tmp = argc - 1;
    int logic_id;
    int ret;

    // 天工不支持
    if (param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID || param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID_2) {
        roce_err("This device does not support this function.");
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    if (argc != ARGV_ID_1) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    logic_id = param->logic_id;
    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = dsmi_get_eth_down_data(logic_id, &down_data);
        if (ret != 0) {
            roce_err("DSMI get eth down data fail. (ret=%d; dev_id=%d)", ret, logic_id);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
        tool_display_eth_down_data(param->phy_id, &down_data);
    } else {
        roce_err("Invalid operation.\n)");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}

static int tool_traceroute_ip_param(unsigned int *dip, unsigned int dip_size, bool ipv6_flag, char *ipaddr)
{
    struct in_addr addr;
    struct in6_addr addr6;
    int ret;

    if (ipv6_flag) {
        ret = inet_pton(AF_INET6, ipaddr, &addr6);
        if (ret <= 0) {
            roce_err("IPv6 convert failed! (ip=%s; ret=%d)", ipaddr, ret);
            return UDA_PARAM_IP_INVALID_ERR;
        }
        ret = memcpy_s(dip, DEVDRV_IPV6_ARRAY_LEN, addr6.s6_addr, DEVDRV_IPV6_ARRAY_LEN);
        if (ret != 0) {
            roce_err("Memcpy failed! (ret=%d)", ret);
            return UDA_TOOL_NO_MEM_ERR;
        }
    } else {
        ret = inet_pton(AF_INET, ipaddr, &addr);
        if (ret <= 0) {
            roce_err("IPv4 convert failed! (ip=%s; ret=%d)", ipaddr, ret);
            return UDA_PARAM_IP_INVALID_ERR;
        }
        dip[0] = addr.s_addr;
    }

    return UDA_EXE_SUCCESS;
}

static int tool_parse_traceroute_param(int index, char **argv, struct traceroute_parse_param *tmp_map, int map_len,
                                       bool is_ipv6)
{
    int tmp_index;
    int ret, troute_info;
    char *p_tmp = NULL, *map_param = NULL;

    if (strncmp(argv[index], "-d", strlen("-d") + 1) ==
        0) { // strlen()+1是为了全字符串比较，带上'\0'，否则形如"-dxxx"的也能通过
        if (tmp_map[map_len - 1].is_used) {
            roce_err("Operation '-d' was used.");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
        tmp_map[map_len - 1].is_used = true;
        ret = tool_traceroute_ip_param(tmp_map[map_len - 1].value.unint_value, DEVDRV_IPV6_ARRAY_INT_LEN, is_ipv6,
                                       argv[index + 1]);
        if (ret != UDA_EXE_SUCCESS) {
            roce_err("Tool loaded traceroute ip param failed! (ip=%s; ret=%d)", argv[index + 1], ret);
            return ret;
        }
        return UDA_EXE_SUCCESS;
    }

    for (tmp_index = 0; tmp_index < map_len; tmp_index++) {
        map_param = tmp_map[tmp_index].param;
        if (strncmp(argv[index], map_param, strlen(map_param) + 1) != 0) {
            continue;
        }
        if (tmp_map[tmp_index].is_used) {
            roce_err("Operation '%s' was used.", tmp_map[tmp_index].param);
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
        tmp_map[tmp_index].is_used = true;

        troute_info = strtol(argv[index + 1], &p_tmp, STR_TO_INT_LEN);
        if (*p_tmp != '\0') {
            roce_err("Invalid param in strtol. (string=%s)", argv[index + 1]);
            return UDA_PARAM_INVALID_ERR;
        }

        if (troute_info < tmp_map[tmp_index].min_num || troute_info > tmp_map[tmp_index].max_num) {
            roce_err("The input num is invalid. (operation=%s; input_num=%d)", tmp_map[tmp_index].param, troute_info);
            return UDA_PARAM_INVALID_ERR;
        }
        *tmp_map[tmp_index].value.int_value = troute_info;
        return UDA_EXE_SUCCESS;
    }

    return UDA_PARAM_OP_NOT_SUPPORT_ERR;
}

static int tool_traceroute_info_load(int argc, char **argv, struct dsmi_traceroute_info *traceroute_info_send,
                                     bool ipv6_flag)
{
    int ret;
    struct dsmi_traceroute_info *tmp_info = traceroute_info_send;
    int max_tos_or_tc = ipv6_flag ? TRACEROUTE_TC_MAX : TRACEROUTE_TOS_MAX;
    struct traceroute_parse_param map_traceroute_info[] = {
        {.value.int_value = &tmp_info->max_ttl, "-m", TRACEROUTE_TTL_MIN, TRACEROUTE_TTL_MAX, false},
        {.value.int_value = &tmp_info->tos, "-t", TOS_NUMBER_MIN, max_tos_or_tc, false},
        {.value.int_value = &tmp_info->waittime, "-w", TRACEROUTE_WAITTIME_MIN, TRACEROUTE_WAITTIME_MAX, false},
        {.value.int_value = &tmp_info->source_port, "-sport", UDP_PORT_NUMBER_MIN, UDP_PORT_NUMBER_MAX, false},
        {.value.int_value = &tmp_info->dest_port, "-dport", UDP_PORT_NUMBER_MIN, UDP_PORT_NUMBER_MAX, false},
        {.value.unint_value = tmp_info->dip, "-d", 0, 0, false},
    };
    int map_len = sizeof(map_traceroute_info) / sizeof(struct traceroute_parse_param);

    ret = memset_s(traceroute_info_send, sizeof(struct dsmi_traceroute_info), -1, sizeof(struct dsmi_traceroute_info));
    if (ret != 0) {
        roce_err("Memset_s traceroute_info_send failed. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    traceroute_info_send->ipv6_flag = ipv6_flag;
    for (int i = 0; i < argc; i += TRACEROUTE_DOUBLE_ADD) {
        ret = tool_parse_traceroute_param(i, argv, map_traceroute_info, map_len, ipv6_flag);
        if (ret != 0) {
            roce_err("Traceroute used invalid cmd. (ret=%d)", ret);
            return ret;
        }
    }

    return ret;
}

static int tool_traceroute_start(int logic_id, struct dsmi_traceroute_info *traceroute_info_send)
{
    int ret;
    int troute_status = 0, troute_result = 0, troute_reset = 0;

    ret = dsmi_get_traceroute_status(logic_id, &troute_status);
    if (ret != 0) {
        roce_err("Tool traceroute start get status failed. (ret=%d; logic_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    if (troute_status == TRACEROUTE_STATUS_RUNNING) {
        TOOL_PRINT_ERR("Traceroute is running, please kill the running process and start a new one.");
        return UDA_TOOL_ERR;
    } else if (troute_status == TRACEROUTE_STATUS_ERROR) {
        ret = dsmi_reset_traceroute(logic_id, &troute_reset);
        if (ret != 0) {
            roce_err("Dsmi reset traceroute failed. (ret=%d; logic_id=%d; troute_reset=%d)", ret, troute_reset,
                     logic_id);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
        if (troute_reset != 0) {
            roce_err("Dsmi reset traceroute failed. (ret=%d; logic_id=%d; troute_reset=%d)", ret, troute_reset,
                     logic_id);
            return UDA_DSMI_EXE_ERR;
        }
    }

    ret = dsmi_start_traceroute(logic_id, traceroute_info_send, &troute_result);
    if (ret != 0) {
        roce_err("Dsmi_start_traceroute failed. (ret=%d; logic_id=%d; troute_result=%d)", ret, logic_id, troute_result);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    if (troute_result == TRACEROUTE_STATUS_ERROR) {
        roce_err("Dsmi_start_traceroute failed. (ret=%d; logic_id=%d; troute_result=%d)", ret, logic_id, troute_result);
        return UDA_DSMI_EXE_ERR;
    }

    return ret;
}

STATIC int tool_waiting_traceroute_finish(int logic_id)
{
    int ret, troute_status = 0;
    int wait_num = MAX_TRACEROUTE_WAITING_TIMES;

    while (1) {
        ret = dsmi_get_traceroute_status(logic_id, &troute_status);
        if (ret != 0) {
            roce_err("Tool_waiting_traceroute_finish get status failed. (ret=%d; logic_id=%d)", ret, logic_id);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
        if (troute_status == TRACEROUTE_STATUS_NOT_RUNNING) {
            break;
        } else if (troute_status == TRACEROUTE_STATUS_ERROR) {
            roce_err("Dsmi got error traceroute status. (ret=%d; logic_id=%d; troute_status=%d)", ret, logic_id,
                     troute_status);
            return UDA_DSMI_EXE_ERR;
        }

        wait_num--;
        if (wait_num <= 0) {
            roce_err("Tool wait traceroute finish timeout. (logic_id=%d)", logic_id);
            return UDA_EXE_TIME_OUT_ERR;
        }
        sleep(1);
    }

    return 0;
}

static int tool_get_traceroute_info(int argc, char **argv, struct tool_param *param, bool ipv6_flag)
{
    int ret;
    struct dsmi_traceroute_info traceroute_info_send = {0};
    char *troute_info_show = NULL;

    if ((argc < TOOL_TRACEROUTE_CMD_LEN_MIN) ||
        (((unsigned int)argc & 1) == 1)) { // agrc按位与1，即取余数，若为奇数则错误
        roce_err("Argc error. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    troute_info_show = (char *)calloc(MAX_DATA_LEN, sizeof(char));
    if (troute_info_show == NULL) {
        roce_err("Tool get traceroute info malloc outbuf failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = tool_traceroute_info_load(argc, argv, &traceroute_info_send, ipv6_flag);
    if (ret != 0) {
        roce_err("Tool traceroute info load failed. (ret=%d)", ret);
        goto out_err;
    }
    ret = tool_traceroute_start(param->logic_id, &traceroute_info_send);
    if (ret != 0) {
        roce_err("Tool traceroute start failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        goto out_err;
    }
    ret = tool_waiting_traceroute_finish(param->logic_id);
    if (ret != 0) {
        roce_err("Waitting roce perftest finish failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        goto out_err;
    }
    ret = dsmi_get_traceroute_info(param->logic_id, troute_info_show, MAX_DATA_LEN, TRACEROUTE_HCCN_CMD);
    if (ret != 0) {
        roce_err("Dsmi get traceroute info failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        ret = dsmi_analysis_dsmi_ret_to_uda(ret);
        goto out_err;
    }

    troute_info_show[MAX_DATA_LEN - 1] = '\0';
    DSMI_PRINT_INFO("%s", troute_info_show);

out_err:
    free(troute_info_show);
    troute_info_show = NULL;

    return ret;
}

static int tool_reset_traceroute(struct tool_param *param)
{
    int ret, troute_reset = 0;

    ret = dsmi_reset_traceroute(param->logic_id, &troute_reset);
    if (ret != 0 || troute_reset != 0) {
        roce_err("Dsmi reset traceroute failed. (ret=%d; logic_id=%d; troute_reset=%d)", ret, troute_reset,
                 param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    TOOL_PRINT_ERR("Traceroute processes have been killed.");

    return ret;
}

int tool_traceroute_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret, argc_tmp = argc;
    char **argv_tmp = argv;
    bool is_ipv6, ipv6_flag = false;

    if (argc <= 0 || argc > TRACEROUTE_ARGC_MAX) {
        roce_err("The argc is error. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = hccn_check_usr_identify();
    if (ret != 0) {
        roce_err("Check usr identify failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    if (!strncmp(argv_tmp[0], "reset", strlen("reset") + 1)) {
        if (argc > 1) {
            roce_err("Reset argc error. (argc=%d)", argc);
            return UDA_PARAM_CMD_COUNT_ERR;
        }
        ret = tool_reset_traceroute(param);
        if (ret != UDA_EXE_SUCCESS) {
            roce_err("Tool_reset_traceroute failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        }
        return ret;
    }

    if (is_ipv6_cmd(&argc_tmp, &argv_tmp)) {
        ipv6_flag = true;
        if (argc_tmp <= 0) {
            roce_err("The argc is error. (argc=%d)", argc);
            return UDA_PARAM_CMD_COUNT_ERR;
        }
        if (is_atlas_9000_a3(param)) {
            roce_err("Ipv6 atlas_9000_a3 command is not supported.");
            return UDA_TOOL_OP_NOT_SUPPORT_IPV6_ERR;
        }
    }

    if (!strncmp(argv_tmp[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_traceroute_info(argc_tmp - 1, argv_tmp + 1, param, ipv6_flag);
        if (ret != UDA_EXE_SUCCESS) {
            roce_err("Tool got traceroute info failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
    } else {
        roce_err("Invalid option. (option=%s)", argv_tmp[0]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return ret;
}

int perftest_stop_status(int device_id, unsigned int is_client)
{
    int ret;
    char *reset_report = NULL;

    ret = dsmi_stop_roce_perftest(device_id, is_client);
    if (ret) {
        roce_err("Dsmi_stop_roce_perftest fail. (ret=%d; dev_id=%d)", ret, device_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    reset_report = (char *)calloc(MAX_REPORT_LEN, sizeof(char));
    if (reset_report == NULL) {
        roce_err("Tool get rule calloc reset_report failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = dsmi_reset_roce_perftest(device_id, reset_report);
    if (ret) {
        roce_err("Dsmi reset roce perftest fail. (ret=%d; dev_id=%d)", ret, device_id);
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

int verify_little_param(int argc, struct tool_param *param, struct perftest_cmd_info cmd_info[], int is_client,
                        int start_cnt)
{
    struct perftest_curr_stat perftest_curr = {0};
    unsigned int little_mem = get_little_mem_process();
    int ret;

    ret = dsmi_get_roce_perftest_status(param->logic_id, is_client, &perftest_curr);
    if (ret) {
        roce_err("Dsmi get roce perftest status failed. (ret=%d; dev_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    if (perftest_curr.free_mem <= MEM_MIN) {
        if (little_mem) {
            TOOL_PRINT_ERR("There isn't enough memory to perform the requested, only -n and -s parameters"
                           " are allowed here.");
            return UDA_PARAM_INVALID_ERR;
        }
    }
    return 0;
}

int waiting_roce_perftest_finish(int device_id, unsigned int is_client, struct perftest_cmd_info cmd_info[],
                                 struct perftest_curr_stat *perftest_curr, int wait_num)
{
    int ret;
    int local_wait_num = wait_num;
    unsigned int little_mem = get_little_mem_process();

    while (1) {
        sleep(WAIT_TIME);
        ret = dsmi_get_roce_perftest_status(device_id, is_client, perftest_curr);
        if (ret) {
            roce_err("Dsmi_get_roce_perftest_status fail. (ret=%d, dev_id=%d)", ret, device_id);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
        TOOL_PRINT_INFO("Dsmi get perftest status end. (status=%u)", perftest_curr->status);
        if (perftest_curr->status != PERFTEST_STATUS_DOING) {
            break;
        }
        // 小内存，并且出现了-n -s 以外的参数
        if ((perftest_curr->free_mem <= MEM_MIN) && (little_mem == MEM_STATUS_LIMIT)) {
            ret = perftest_stop_status(device_id, is_client);
            if (ret) {
                roce_err("Perftest stop status fail. (ret=%d; dev_id=%d)", ret, device_id);
                return ret;
            }
            TOOL_PRINT_ERR("There is not enough memory to perform the requested, please decrease your parameters.");
        }
        local_wait_num--;
        if (local_wait_num <= 0) {
            ret = perftest_stop_status(device_id, is_client);
            if (ret) {
                roce_err("Perftest stop status fail. (ret=%d; dev_id=%d)", ret, device_id);
                return ret;
            }
            roce_err("hccn_tool_roce_perftest timeout, dev_id:%d", device_id);
            return UDA_EXE_TIME_OUT_ERR;
        }
    }
    return 0;
}

static int tool_get_xsfp_reset_info(int argc, int logic_id)
{
    struct xsfp_reset_info reset_info = {0};
    int ret;

    if (argc != 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_xsfp_reset_info(logic_id, &reset_info);
    if (ret != 0) {
        roce_err("DSMI get the setting of xsfp reset fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    TOOL_PRINT_INFO("The setting of the xsfp reset:");
    TOOL_PRINT_INFO("cur reset times  : %u", reset_info.recovery_time);
    TOOL_PRINT_INFO("max reset times  : %u", reset_info.max_time);
    TOOL_PRINT_INFO("wait times(*1.5S): %u (%.1fS)", reset_info.wait_time, BASE_WATI_TIME * reset_info.wait_time);

    return ret;
}

static int tool_parse_xsfp_reset_info_parm(int argc, char **argv, struct xsfp_reset_info *reset_info)
{
    char flag[2] = {0}; // Indicates the subcommand settings, total num is 2.
    char *p_tmp = NULL;
    long tmp;
    int i;

    for (i = 0; i < argc; i += STEP_TWO) {
        if ((strncmp(argv[i], "max_times", strlen("max_times") + 1) == 0) && (flag[0] == 0)) {
            tmp = strtol(argv[i + 1], &p_tmp, NUMBER_BASE);
            if ((*p_tmp != '\0') || (tmp < 0) || (tmp > MAX_XSFP_RESET_TIME)) {
                roce_err("The value of max time is wrong. [0 or %d]", MAX_XSFP_RESET_TIME);
                return UDA_PARAM_OP_NOT_SUPPORT_ERR;
            }
            reset_info->max_time = (unsigned char)tmp;
            flag[0] = 1; // Indicates that the field has been set.
        } else if ((strncmp(argv[i], "wait_times", strlen("wait_times") + 1) == 0) && (flag[INDEX_ONE] == 0)) {
            tmp = strtol(argv[i + 1], &p_tmp, NUMBER_BASE);
            if ((*p_tmp != '\0') || (tmp < 0) || (tmp > MAX_XSFP_WATI_TIME)) {
                roce_err("The value of wait time is wrong. [0 or %d]", MAX_XSFP_WATI_TIME);
                return UDA_PARAM_OP_NOT_SUPPORT_ERR;
            }
            reset_info->wait_time = (unsigned char)tmp;
            flag[1] = 1; // Indicates that the field has been set.
        } else {
            roce_err("Invalid operation.)");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }

    return 0;
}

static int tool_save_xsfp_reset_info(struct xsfp_reset_info *reset_info, struct tool_param *param)
{
    char tmp[PARAM_COUNT_THREE] = {'\0'};
    int ret;

    if (reset_info->max_time != INVALID_VALUE) {
        ret = sprintf_s(tmp, PARAM_COUNT_THREE, "%u", reset_info->max_time);
        if (ret < 0) {
            roce_err("Sprintf_s for save xsfp reset max time failed. (ret=%d)", ret);
            return ret;
        }
        ret = cfg_write_conf(param->phy_id, "xsfp_reset_max_times", tmp);
        if (ret != 0) {
            roce_err("Write the max time of xsfp reset error. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    }

    if (reset_info->wait_time != INVALID_VALUE) {
        ret = sprintf_s(tmp, PARAM_COUNT_THREE, "%u", reset_info->wait_time);
        if (ret < 0) {
            roce_err("Sprintf_s for save xsfp reset wait time failed. (ret=%d)", ret);
            return ret;
        }
        ret = cfg_write_conf(param->phy_id, "xsfp_reset_wait_times", tmp);
        if (ret != 0) {
            roce_err("Write the wait time of xsfp reset error. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    }

    return 0;
}

static int tool_set_xsfp_reset_info(int argc, char **argv, struct tool_param *param)
{
    struct xsfp_reset_info reset_info = {0};
    int logic_id;
    int ret;

    if (((argc % PARAM_COUNT_TWO) != 0) || (argc < PARAM_COUNT_TWO) || (argc > PARAM_COUNT_FOUR)) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    logic_id = param->logic_id;

    ret = memset_s(&reset_info, sizeof(struct xsfp_reset_info), INVALID_VALUE, sizeof(struct xsfp_reset_info));
    if (ret != 0) {
        roce_err("Memset xsfp reset info return error. (ret=%d)\n", ret);
        return ret;
    }

    ret = tool_parse_xsfp_reset_info_parm(argc, argv, &reset_info);
    if (ret != 0) {
        roce_err("Parse xsfp reset info parm return error. (ret=%d)\n", ret);
        return ret;
    }

    ret = dsmi_set_xsfp_reset_info(logic_id, &reset_info);
    if (ret != 0) {
        roce_err("Dsmi set xsfp reset_info fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = tool_save_xsfp_reset_info(&reset_info, param);
    if (ret != 0) {
        roce_err("Tool save xsfp reset info fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return ret;
    }

    return 0;
}

static int recovery_xsfp_reset_max_times(struct tool_param *param, char *max_time_data, unsigned int length)
{
    int ret;

    if (max_time_data == NULL) {
        roce_err("Input param is NULL.");
        return -EINVAL;
    }

    ret = cfg_read_conf(param->phy_id, "xsfp_reset_max_times", max_time_data, length);
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            roce_info("Xsfp reset max time conf name is not found in config file, skipped. (ret=%d)", ret);
            return 0;
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    }

    return ret;
}

static int recovery_xsfp_reset_wait_times(struct tool_param *param, const char *wait_time_data, unsigned int length)
{
    int ret;

    if (wait_time_data == NULL) {
        roce_err("Input param is NULL.");
        return -EINVAL;
    }

    ret = cfg_read_conf(param->phy_id, "xsfp_reset_wait_times", wait_time_data, length);
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            roce_info("The Xsfp reset wait time conf name is not found in config file, skipped. (ret=%d)", ret);
            return 0;
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    }

    return ret;
}

int tool_cfg_xsfp_reset_recovery(struct tool_param *param)
{
#ifdef HCCN_TOOL_SHOW_EXT
    char *argv_tmp[PARAM_COUNT_FOUR] = {NULL};
    char wait_time_data[PARAM_COUNT_THREE] = {"7"};
    char max_time_data[PARAM_COUNT_THREE] = {"0"};
    int board_type;
    int ret;

    ret = tool_get_board_type(param, &board_type);
    if (ret != 0) {
        roce_err("Tool xsfp reset cmd get board type failed. (ret=%d)", ret);
        return UDA_DSMI_EXE_ERR;
    }

    if (board_type != A_K_BOARD_TYPE) {
        roce_info("Xsfp reset cfg not need recover.");
        return 0;
    }

    argv_tmp[0] = "max_times";
    ret = recovery_xsfp_reset_max_times(param, max_time_data, sizeof(max_time_data));
    if (ret != 0) {
        roce_err("Recovery xsfp reset max times failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }
    argv_tmp[1] = max_time_data;

    argv_tmp[INDEX_TWO] = "wait_times";
    ret = recovery_xsfp_reset_wait_times(param, wait_time_data, sizeof(wait_time_data));
    if (ret != 0) {
        roce_err("Recovery xsfp reset max times failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }
    argv_tmp[INDEX_TWO + 1] = wait_time_data;

    ret = tool_set_xsfp_reset_info(PARAM_COUNT_FOUR, argv_tmp, param);
    if (ret != 0) {
        roce_err("Cfg xsfp reset recovery failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }
#endif

    return 0;
}

int tool_xsfp_reset_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int argc_tmp = argc - 1;
    int board_type;
    int logic_id;
    int ret;

    if (argc < ARGV_ID_1) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_get_board_type(param, &board_type);
    if (ret != 0) {
        roce_err("Tool xsfp reset cmd get board type failed. (ret=%d)", ret);
        return UDA_DSMI_EXE_ERR;
    }

    if (board_type != A_K_BOARD_TYPE) {
        roce_info("Command unsupported on this product.");
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    logic_id = param->logic_id;
    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_xsfp_reset_info(argc_tmp, logic_id);
        if (ret != 0) {
            roce_err("Get the setting of xsfp reset fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        ret = tool_set_xsfp_reset_info(argc_tmp, argv + 1, param);
        if (ret != 0) {
            roce_err("Set the setting of xsfp reset fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else {
        roce_err("Invalid operation.\n)");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}
STATIC int tool_get_extra_statistics_info(int argc, int logic_id)
{
    int ret;
    struct ds_extra_statistics_info info = {0};
    double correcting_bit_rate;

    if (argc != 0) {
        roce_err("argc err. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_extra_statistics_info(logic_id, 0, &info);
    if (ret != 0) {
        roce_err("Dsmi get extra statistics info fail! (ret=%d; logic_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    TOOL_PRINT_INFO("extra statistics info:");
    TOOL_PRINT_INFO("cw_total_cnt : %llu", info.cw_total_cnt);
    TOOL_PRINT_INFO("cw_before_correct_cnt : %llu", info.cw_before_correct_cnt);
    TOOL_PRINT_INFO("cw_correct_cnt : %llu", info.cw_correct_cnt);
    TOOL_PRINT_INFO("cw_uncorrect_cnt : %llu", info.cw_uncorrect_cnt);
    TOOL_PRINT_INFO("cw_bad_cnt : %llu", info.cw_bad_cnt);
    TOOL_PRINT_INFO("trans_total_bit : %llu", info.trans_total_bit);
    TOOL_PRINT_INFO("cw_total_correct_bit : %llu", info.cw_total_correct_bit);
    TOOL_PRINT_INFO("rx_full_drop_cnt : %llu", info.drop_num);
    TOOL_PRINT_INFO("pcs_err_cnt : %llu", info.pcs_err_count);
    TOOL_PRINT_INFO("rx_send_app_good_pkts : %llu", info.rx_send_app_good_pkts);
    TOOL_PRINT_INFO("rx_send_app_bad_pkts : %llu", info.rx_send_app_bad_pkts);

    if (info.trans_total_bit != 0) {
        correcting_bit_rate = (double)info.cw_total_correct_bit / (double)info.trans_total_bit;
        TOOL_PRINT_INFO("correcting_bit_rate : %.2e", correcting_bit_rate);
    } else {
        TOOL_PRINT_INFO("correcting_bit_rate : 0");
    }

    return 0;
}

static int tool_get_fec_error_info(int logic_id, int mac_id)
{
    int ret;
    int j;
    struct ds_fec_error_info info = {0};

    ret = dsmi_get_fec_error_info(logic_id, &info, mac_id);
    if (ret != 0) {
        roce_err("Dsmi get fec erro info fail! (ret=%d; logic_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    TOOL_PRINT_INFO("Number of all CW");
    TOOL_PRINT_INFO("cw_dec_all_cnt: %llu", info.cw_dec_all_cnt);
    TOOL_PRINT_INFO("----------------------------");
    TOOL_PRINT_INFO("Number of uncorrectable CW");
    TOOL_PRINT_INFO("cw_fail_err_cnt: %llu", info.cw_fail_err_cnt);
    TOOL_PRINT_INFO("----------------------------");
    TOOL_PRINT_INFO("Number of correctable CW");
    for (j = 0; j < FEC_ERROR_COUNT_MAX; ++j) {
        TOOL_PRINT_INFO("cw%d_sym_err_cnt: %llu", (j + 1), info.cw_sym_err_cnt[j]);
    }
    return 0;
}

int tool_get_extra_statistics_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret = 0;
    int argc_tmp = argc - 1;
    int logic_id = param->logic_id;

    if (argc != 1) {
        roce_err("Tool_get_extra_statistics err. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen(argv[0]) + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_extra_statistics_info(argc_tmp, logic_id);
        if (ret) {
            roce_err("Tool_get_extra_statistics fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "-c", strlen("-c") + 1)) {
        LOG_CONFIG("start to clear packet extra statistics of dev[%d]", param->logic_id);
        param->get_cmd_flag = false;
        ret = dsmi_clear_extra_statistics_info(param->logic_id, 0);
        if (ret) {
            roce_err("Dsmi clear stat fail (logic_id=%d; port=%d; ret=%d)", param->logic_id, 0, ret);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
    } else {
        roce_err("Tool_get_extra_statistics invalid operation.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}

int tool_get_fec_error_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret = 0;
    int logic_id = param->logic_id;
    int mac_id = 0;
    param->get_cmd_flag = true;

    if (argc != FEC_EEROR_CMD_SIZE) {
        roce_err("Get fec error argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    if (!strncmp(argv[0], "-g", strlen(argv[0]) + 1)) {
        ret = tool_get_fec_error_info(logic_id, mac_id);
        if (ret != 0) {
            roce_err("Get fec error fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return UDA_DSMI_EXE_ERR;
        }
    } else {
        roce_err("Get fec error invalid operation.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

static int tool_parse_cw_bad_cnt_thd(int argc, char **argv, struct ds_cw_bad_cnt_thd *info)
{
    char *p_tmp = NULL;
    long tmp;
    int i;

    if (argc == 0) {
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    for (i = 0; i < argc; i += STEP_TWO) {
        if (strncmp(argv[i], "high_limit", strlen("high_limit") + 1) == 0) {
            tmp = strtol(argv[i + 1], &p_tmp, NUMBER_BASE);
            if ((*p_tmp != '\0') || (tmp < ROCE_MIN_CW_BAD_CNT_THD) || (tmp > ROCE_MAX_CW_BAD_CNT_THD)) {
                roce_err("The value of cw_bad_cnt_thd high_limit wrong. [%d ~ %d]", ROCE_MIN_CW_BAD_CNT_THD,
                         ROCE_MAX_CW_BAD_CNT_THD);
                return UDA_PARAM_INVALID_ERR;
            }
            info->cw_bad_cnt_thd = (unsigned char)tmp;
        } else {
            roce_err("Invalid operation.)");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }

    return 0;
}

int tool_set_roce_cw_bad_cnt_thd(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id;
    struct ds_cw_bad_cnt_thd info = {0};

    LOG_CONFIG("Start to set cw_bad_cnt_thd cfg of logic_id[%d]", param->logic_id);

    if (argc != PARAM_COUNT_TWO) {
        roce_err("Tool set roce cw_bad_cnt_thd, argc wrong. (argc=%d)\n", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    logic_id = param->logic_id;

    if (strncmp(argv[0], "high_limit", strlen("high_limit") + 1)) {
        roce_err("Tool set roce cw_bad_cnt_thd, invalid option. (argv=%s)\n", argv[0]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    ret = tool_parse_cw_bad_cnt_thd(argc, argv, &info);
    if (ret != 0) {
        roce_err("Parse cw_bad_cnt_thd failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = dsmi_set_roce_cw_bad_cnt_thd(logic_id, info.cw_bad_cnt_thd);
    if (ret != 0) {
        roce_err("Dsmi set cw_bad_cnt_thd failed. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return 0;
}

int tool_get_roce_cw_bad_cnt_thd(int argc, char **argv, int logic_id)
{
    struct ds_cw_bad_cnt_thd info = {0};
    int ret;

    if (argc != 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_roce_cw_bad_cnt_thd(logic_id, &info);
    if (ret != 0) {
        roce_err("DSMI get the value of cw_bad_cnt_thd failed. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    TOOL_PRINT_INFO("cw_bad_cnt_thd: %u", info.cw_bad_cnt_thd);

    return ret;
}

int tool_roce_cw_bad_cnt_thd_recovery(struct tool_param *param)
{
    int ret;
    char high_limit[PARAM_COUNT_THREE] = "";
    char *argv[PARAM_COUNT_TWO] = {NULL};

    roce_info("Process cw_bad_cnt_thd recovery.\n");

    argv[0] = "high_limit";
    ret = cfg_read_conf(param->phy_id, "cw_bad_cnt_thd_high_limit", high_limit, sizeof(high_limit));
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            roce_info("The cw_bad_cnt_thd_high_limit conf name is not found in config file, skipped. (ret=%d)", ret);
            return 0;
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        roce_info("Cw_bad_cnt_thd high_limit need recover cfg.");
        argv[1] = high_limit;
        ret = tool_set_roce_cw_bad_cnt_thd(PARAM_COUNT_TWO, argv, param);
        if (ret != 0) {
            roce_err("Set cw_bad_cnt_thd failed.(ret=%d, logic_id=%d)", ret, param->logic_id);
            return ret;
        }
    }

    return ret;
}

int host_info_read(struct ds_host_info *host_info)
{
    int ret;
    struct utsname buffer;

    if (host_info == NULL) {
        roce_err("Input param is NULL.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    /* 通过uname()函数获取系统信息，等同于uname -a */
    ret = uname(&buffer);
    if (ret != 0) {
        roce_err("Get uname -a info failed. (ret=%d)\n", ret);
        return ret;
    }
    /* 拷贝host_name */
    ret = memcpy_s(host_info->host_name, HOST_NAME_SIZE_MAX, buffer.nodename, HOST_NAME_SIZE_MAX);
    if (ret != 0) {
        roce_err("Call memcpy failed. (ret=%d)\n", ret);
        return ret;
    }
    /* 拷贝host_description */
    ret = sprintf_s(host_info->host_description, HOST_INFO_SIZE_MAX, "%s %s %s %s %s", buffer.sysname, buffer.nodename,
                    buffer.release, buffer.version, buffer.machine);
    if (ret <= 0) {
        roce_err("Sprintf_s host info err. (ret=%d)", ret);
        return -ENOMEM;
    }

    return 0;
}

int tool_cfg_host_info_recovery(struct tool_param *param)
{
    int ret;
    struct ds_host_info host_info = {0}; /* 用于保存host name和host description */

    /* 获取host name和host description，保存到host_info */
    ret = host_info_read(&host_info);
    if (ret) {
        roce_err("Get host info failed. (ret=%d)", ret);
        return ret;
    }

    ret = dsmi_set_host_info(param->logic_id, &host_info);
    if (ret) {
        roce_err(" Set host info failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        ret = dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return ret;
}

static void tool_show_ippair_list(struct ds_ippair_list *list, struct ds_roce_port_ippair *ippair)
{
    int i;
    char sip[INET6_ADDRSTRLEN] = {0};
    char dip[INET6_ADDRSTRLEN] = {0};

    TOOL_PRINT_INFO("UDP port list(based on IP pair):");
    if (list->count == 0) {
        TOOL_PRINT_INFO("no entry has been configured.");
        return;
    }

    if (ippair == NULL) {
        TOOL_PRINT_INFO("%-8s%-40s%-40s%-10s", "index", "src_ip", "dst_ip", "udp_port");
        for (i = 0; i < list->count; i++) {
            (void)inet_ntop(list->ippair[i].sip_family, &list->ippair[i].sip.ipv6, sip, INET6_ADDRSTRLEN);
            (void)inet_ntop(list->ippair[i].dip_family, &list->ippair[i].dip.ipv6, dip, INET6_ADDRSTRLEN);
            TOOL_PRINT_INFO("%-8d%-40s%-40s%-10d", i, sip, dip, list->ippair[i].roce_port);
            bzero(sip, INET6_ADDRSTRLEN);
            bzero(dip, INET6_ADDRSTRLEN);
        }
        return;
    }

    for (i = 0; i < list->count; i++) {
        if (memcmp(&list->ippair[i].sip.ipv6, &ippair->sip.ipv6, sizeof(struct in6_addr)) == 0 &&
            memcmp(&list->ippair[i].dip.ipv6, &ippair->dip.ipv6, sizeof(struct in6_addr)) == 0) {
            TOOL_PRINT_INFO("%-8s%-40s%-40s%-10s", "index", "src_ip", "dst_ip", "udp_port");
            (void)inet_ntop(list->ippair[i].sip_family, &list->ippair[i].sip.ipv6, sip, INET6_ADDRSTRLEN);
            (void)inet_ntop(list->ippair[i].dip_family, &list->ippair[i].dip.ipv6, dip, INET6_ADDRSTRLEN);
            TOOL_PRINT_INFO("%-8d%-40s%-40s%-10d", i, sip, dip, list->ippair[i].roce_port);
            return;
        }
    }

    TOOL_PRINT_INFO("no matched entry has been found.");
    return;
}

int tool_roce_port_get_ip(char *arg, struct in6_addr *ip, int *family)
{
    if (inet_pton(AF_INET, arg, ip) > 0) {
        *family = AF_INET;
    } else if (inet_pton(AF_INET6, arg, ip) > 0) {
        *family = AF_INET6;
    } else {
        roce_err("Invalid sip address type.(sip=%s)", arg);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    return UDA_SUCCESS;
}

static bool ippair_ip_addrees_is_null(struct ds_roce_port_ippair *ippair)
{
    struct in6_addr zero_addr = {0};

    if (memcmp(&zero_addr, &ippair->sip.ipv6, sizeof(struct in6_addr)) == 0 ||
        memcmp(&zero_addr, &ippair->dip.ipv6, sizeof(struct in6_addr)) == 0) {
        roce_err("Ip address is null.");
        return true;
    }

    return false;
}

static int parse_roce_port_ippair(int argc, char **argv, struct ds_roce_port_ippair *ippair)
{
    int i;
    int ret;

    for (i = 0; i < argc && i + 1 < argc; i += STEP_TWO) {
        if (strncmp(argv[i], "port", strlen("port") + 1) == 0) {
            ippair->roce_port = (int)strtol(argv[i + 1], NULL, NUMBER_BASE);
            continue;
        }

        if (!strncmp(argv[i], "sip", strlen("sip") + 1)) {
            ret = tool_roce_port_get_ip(argv[i + 1], &ippair->sip.ipv6, &ippair->sip_family);
            if (ret != UDA_SUCCESS) {
                return ret;
            }
            continue;
        }

        if (!strncmp(argv[i], "dip", strlen("dip") + 1)) {
            ret = tool_roce_port_get_ip(argv[i + 1], &ippair->dip.ipv6, &ippair->dip_family);
            if (ret != UDA_SUCCESS) {
                return ret;
            }
            continue;
        }

        roce_err("Invalid operation.(arg = %s)", argv[i]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    if (ippair_ip_addrees_is_null(ippair) == true) {
        return UDA_PARAM_IP_INVALID_ERR;
    }

    return UDA_SUCCESS;
}

static int tool_get_ippair_list(int logic_id, struct ds_ippair_list **out_list)
{
    int ret;
    const int max_ippair_num = 128; // 需求规格支持128个IP对
    size_t buf_len = sizeof(struct ds_ippair_list) + sizeof(struct ds_roce_port_ippair) * max_ippair_num;
    struct ds_ippair_list *list = calloc(1, buf_len);

    if (list == NULL) {
        roce_err("Tool alloc ippair list fail.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    list->count = max_ippair_num;
    ret = dsmi_get_roce_port_ippair(logic_id, 0, list);
    if (ret != 0) {
        roce_err("Dsmi get ippair fail.(ret=%d; logic_id=%d)", ret, logic_id);
        free(list);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    *out_list = list;
    return UDA_SUCCESS;
}

#ifdef HCCN_TOOL_SHOW_EXT
int tool_update_roce_port_ippair(int logic_id, int argc, char **argv, enum tool_ippair_op_type op)
{
    int ret;
    char *op_name[] = {"set", "del"};
    char sip[INET6_ADDRSTRLEN] = {0};
    char dip[INET6_ADDRSTRLEN] = {0};
    struct ds_roce_port_ippair ippair = {0};
    int (*func[])(int, int, struct ds_roce_port_ippair *) = {dsmi_set_roce_port_ippair, dsmi_del_roce_port_ippair};

    if (argc != ARGC_ID_6) {
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = parse_roce_port_ippair(argc, argv, &ippair);
    if (ret != UDA_SUCCESS) {
        return ret;
    }

    (void)inet_ntop(ippair.sip_family, &ippair.sip.ipv6, sip, INET6_ADDRSTRLEN);
    (void)inet_ntop(ippair.dip_family, &ippair.dip.ipv6, dip, INET6_ADDRSTRLEN);

    LOG_CONFIG("Start to %s roce port ippair.(logic_id=%d; roce_port=%d; sip=%s; dip=%s)", op_name[op], logic_id,
               ippair.roce_port, sip, dip);
    ret = func[op](logic_id, 0, &ippair);
    LOG_CONFIG("End to %s roce port ippair.(logic_id=%d; roce_port=%d; sip=%s; dip=%s; result=%d)", op_name[op],
               logic_id, ippair.roce_port, sip, dip, ret);
    if (ret != 0) {
        roce_err("Dsmi %s roce port fail.(ret=%d; logic_id=%d; roce_port=%d; sip=%s; dip=%s)", op_name[op], ret,
                 logic_id, ippair.roce_port, sip, dip);
        if (ret == -EINVAL) {
            return op == IPPAIR_SET ? UDA_PARAM_INVALID_ERR : UDA_TOOL_CONF_NOT_EXIST_ERR;
        }
        if (ret == -EACCES) {
            return UDA_TOOL_SYS_NOT_ACCESS;
        }

        if (op == IPPAIR_SET) {
            switch (ret) {
                case -ENOSPC:
                    return UDA_DSMI_CONFIG_NUM_EXCEED_LIMIT_ERR;
                case -EAGAIN:
                    return UDA_DSMI_UPDATE_QP_UDP_SPORT_ERR;
                default:
                    return UDA_DSMI_SET_UDP_PORT_IPPAI_ERR;
            }
        }

        return UDA_DSMI_EXE_ERR;
    }

    return UDA_SUCCESS;
}

int tool_clear_roce_port_ippair(int logic_id, int argc)
{
    int ret;

    if (argc != 0) {
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    LOG_CONFIG("Clear to del roce port ippair.(logic_id=%d)", logic_id);
    ret = dsmi_clear_roce_port_ippair(logic_id, 0);
    LOG_CONFIG("End to clear roce port ippair.(logic_id=%d; result=%d)", logic_id, ret);
    if (ret != 0) {
        roce_err("Dsmi clear roce port fail.(ret=%d; logic_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return UDA_SUCCESS;
}

int tool_get_single_ippair(int logic_id, int argc, char **argv)
{
    int ret;
    int roce_port = 0;
    struct ds_ippair_list *list = NULL;
    struct ds_roce_port_ippair ippair = {0};

    ret = parse_roce_port_ippair(argc, argv, &ippair);
    if (ret != UDA_SUCCESS) {
        return ret;
    }

    ret = tool_get_ippair_list(logic_id, &list);
    if (ret != UDA_SUCCESS) {
        return ret;
    }

    ret = dsmi_get_roce_port(logic_id, 0, &roce_port);
    if (ret) {
        roce_err("Dsmi get roce port fail.(ret=%d; logic_id=%d)", ret, logic_id);
        free(list);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    tool_show_ippair_list(list, &ippair);
    free(list);
    return UDA_SUCCESS;
}

int tool_get_all_ippair(int logic_id)
{
    int ret;
    struct ds_ippair_list *list = NULL;

    ret = tool_get_ippair_list(logic_id, &list);
    if (ret != UDA_SUCCESS) {
        return ret;
    }

    tool_show_ippair_list(list, NULL);
    free(list);
    return UDA_SUCCESS;
}
#endif

int tool_get_npu_serders_snr_macro(int phy_id, int logic_id, unsigned int macro_id)

{
    int ret;
    tool_serdes_quality_info serdes_quality_info;
    unsigned int out_size = (unsigned int)sizeof(tool_serdes_quality_info);

    serdes_quality_info.macro_id = macro_id;
    ret = dsmi_get_device_info(logic_id, DSMI_MAIN_CMD_SERDES, DSMI_SERDES_SUB_CMD_QUALITY_INFO,
                               (void *)&serdes_quality_info, &out_size);
    if ((ret != 0) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        roce_err("Dsmi_get_device_info failed.(ret = %u)", ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    if (ret == DSMI_ERR_NOT_SUPPORT) {
        roce_err("Npu_serdes_snr is not supported.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    if (macro_id == ATLAS_NPU_SERDERS_NUM_0) {
        TOOL_PRINT_INFO("----PHY %d RX SNR INFO:-----------------------------", phy_id);
    }
    for (int i = 0; i < DEVICE_NPU_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO("npu serdes lane%d  :  %08u", i, serdes_quality_info.serdes_quality_info[i].snr);
    }

    return ret;
}

static void tool_print_serdes_speed_lane_cnt(unsigned int macro_id)

{
    TOOL_PRINT_INFO("");
    TOOL_PRINT_INFO("npu serdes speed  :  56.125G");
    TOOL_PRINT_INFO("npu lane cnt      :  4");
    TOOL_PRINT_INFO("----THE SERDES INFO OF PORT %d----------------------", macro_id);
}

int tool_get_npu_serders_snr(struct tool_param *param)
{
    int ret;
    ret = tool_get_npu_serders_snr_macro(param->phy_id, param->logic_id, ATLAS_NPU_SERDERS_NUM_0);
    if ((ret != 0) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        roce_err("tool_get_npu_serders_snr macro_uid 0 failed.(ret = %u)", ret);
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    if (param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID || param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID_2) {
        tool_print_serdes_speed_lane_cnt(ATLAS_NPU_SERDERS_NUM_0);
        ret = tool_get_npu_serders_snr_macro(param->phy_id, param->logic_id, ATLAS_NPU_SERDERS_NUM_1);
        if ((ret != 0) && (ret != DSMI_ERR_NOT_SUPPORT)) {
            roce_err("tool_get_npu_serders_snr macro_uid 1 failed.(ret = %u)", ret);
            return UDA_TOOL_INNER_PARAM_ERR;
        }
        tool_print_serdes_speed_lane_cnt(ATLAS_NPU_SERDERS_NUM_1);
    } else {
        TOOL_PRINT_INFO("---------------------------------------------------");
    }

    return UDA_SUCCESS;
}

struct auto_downgrade_status_map {
    int status;
    const char *desc;
};

static void tool_show_downgrade_lane_info(struct downgrade_lane_info info)
{
    int i;
    struct auto_downgrade_status_map status_str_map[] = {
        {DOWNGRADE_LANE_IDLE, "not downgrade"},
        {DOWNGRADE_LANE_SUCC, "downgrade success"},
        {DOWNGRADE_LANE_FAIL, "downgrade failure"},
    };

    TOOL_PRINT_INFO("setting          : %s", info.switch_flag == DOWNGRADE_LANE_ENABLE ? "enable" : "disable");
    TOOL_PRINT_INFO("ability          : %s", info.work_mode == DOWNGRADE_LANE_WORK ? "support" : "not support");
    TOOL_PRINT_INFO("filter time      : %u s", info.filter_time);
    for (i = 0; i < sizeof(status_str_map) / sizeof(status_str_map[0]); ++i) {
        if (status_str_map[i].status == info.actual_status) {
            TOOL_PRINT_INFO("downgrade status : %s", status_str_map[i].desc);
            break;
        }
    }
}

static int tool_parse_downgrade_info_parm(int argc, char **argv, struct downgrade_lane_info *info)
{
    char flag[2] = {0}; // Indicates the subcommand settings, total num is 2.
    char *p_tmp = NULL;
    long tmp;
    int i;
    int ret;

    ret = memset_s(info, sizeof(struct downgrade_lane_info), INVALID_VALUE, sizeof(struct downgrade_lane_info));
    if (ret != 0) {
        roce_err("Memset downgrade info return error. (ret=%d)\n", ret);
        return ret;
    }

    for (i = 0; i < argc; i += STEP_TWO) {
        if ((strncmp(argv[i], "enable", strlen("enable") + 1) == 0) && (flag[0] == 0)) {
            tmp = strtol(argv[i + 1], &p_tmp, NUMBER_BASE);
            if ((*p_tmp != '\0') || ((tmp != DOWNGRADE_LANE_ENABLE) && (tmp != DOWNGRADE_LANE_DISABLE))) {
                roce_err("The value of elable flag is wrong. (tmp=%u)", tmp);
                return UDA_PARAM_INVALID_ERR;
            }
            info->switch_flag = (unsigned char)tmp;
            flag[0] = 1; // Indicates that the field has been set.
        } else if ((strncmp(argv[i], "filter_time", strlen("filter_time") + 1) == 0) && (flag[INDEX_ONE] == 0)) {
            tmp = strtol(argv[i + 1], &p_tmp, NUMBER_BASE);
            if ((*p_tmp != '\0') || (tmp > DOWNGRADE_FILTER_MAX_TIME_S) || (tmp < DOWNGRADE_FILTER_MIN_TIME_S)) {
                roce_err("The value of filter time is wrong. (tmp=%u)", tmp);
                return UDA_PARAM_INVALID_ERR;
            }
            info->filter_time = (unsigned char)tmp;
            flag[1] = 1; // Indicates that the field has been set.
        } else {
            roce_err("Invalid operation.)");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }

    return 0;
}

static int tool_save_downgrade_info(struct downgrade_lane_info *info, struct tool_param *param)
{
    char tmp[PARAM_COUNT_THREE] = {'\0'};
    int ret;

    if (info->switch_flag != INVALID_VALUE) {
        ret = sprintf_s(tmp, PARAM_COUNT_THREE, "%u", info->switch_flag);
        if (ret < 0) {
            roce_err("Sprintf_s for save downgrade switch flag failed. (ret=%d)", ret);
            return ret;
        }
        ret = cfg_write_conf(param->phy_id, DOWNGRADE_LANE_SWITCH, tmp);
        if (ret != 0) {
            roce_err("Write the switch flag of downgrade info error. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    }

    if (info->filter_time != INVALID_VALUE) {
        ret = sprintf_s(tmp, PARAM_COUNT_THREE, "%u", info->filter_time);
        if (ret < 0) {
            roce_err("Sprintf_s for save downgrade filter time failed. (ret=%d)", ret);
            return ret;
        }
        ret = cfg_write_conf(param->phy_id, DOWNGRADE_LANE_FILTER_TIME, tmp);
        if (ret != 0) {
            roce_err("Write the filter time of downgrade info error. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    }

    return 0;
}

static int tool_set_auto_downgrade_info(int argc, char **argv, struct tool_param *param)
{
    struct downgrade_lane_info info = {0};
    int logic_id, board_type = 0, is_master_die;
    int ret;

    if (((argc % PARAM_COUNT_TWO) != 0) || (argc < PARAM_COUNT_TWO) || (argc > PARAM_COUNT_FOUR)) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_parse_downgrade_info_parm(argc, argv, &info);
    if (ret != 0) {
        roce_err("Parse downgrade info parm return error. (ret=%d)\n", ret);
        return ret;
    }

    ret = tool_get_board_type(param, &board_type);
    if (ret != 0) {
        roce_err("Tool auto downgrade cmd get board type failed. (ret=%d)", ret);
        return UDA_DSMI_EXE_ERR;
    }

    logic_id = param->logic_id;
    if (board_type == SUPERPOD_900_BOARD_TYPE) {
        info.master_flag = atlas_900_a3_superpod_phy_id_convert(param->phy_id) == param->phy_id ? 1 : 0;
        logic_id = ATLAS_900_A3_SUPERPOD_logic_id_convert(param->phy_id);
        if (logic_id == INVALID_LOGIC_ID) {
            return UDA_DSMI_CONTROL_LINK_UNREACHABLE_ERR;
        }
    } else if (board_type == A_K_BOARD_TYPE) {
        info.master_flag = 1;
    } else {
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    ret = tool_double_check("[WARNING] Please ensure that the same configuration is set on the peer end, continue?");
    if (ret) {
        return ret;
    }

    ret = dsmi_set_downgrade_lane_info(logic_id, &info);
    if (ret != 0) {
        roce_err("Dsmi set downgrade fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    if (!g_recovering) {
        ret = tool_save_downgrade_info(&info, param);
        if (ret != 0) {
            roce_err("Tool save downgrade info fail. (ret=%d; dev_id=%d)", ret, logic_id);
            return ret;
        }
    }
    return 0;
}

static int tool_get_auto_downgrade_info(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id;
    int board_type = 0;
    struct downgrade_lane_info info = {0};

    if (argc != 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_get_board_type(param, &board_type);
    if (ret != 0) {
        roce_err("Tool auto downgrade cmd get board type failed. (ret=%d)", ret);
        return UDA_DSMI_EXE_ERR;
    }

    logic_id = param->logic_id;
    if (board_type == SUPERPOD_900_BOARD_TYPE) {
        info.master_flag = atlas_900_a3_superpod_phy_id_convert(param->phy_id) == param->phy_id ? 1 : 0;
        logic_id = ATLAS_900_A3_SUPERPOD_logic_id_convert(param->phy_id);
        if (logic_id == INVALID_LOGIC_ID) {
            return UDA_DSMI_CONTROL_LINK_UNREACHABLE_ERR;
        }
    } else if (board_type == A_K_BOARD_TYPE) {
        info.master_flag = 1;
    } else {
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    /* 从imp层获取降lane的工作模式和实际状态 */
    ret = dsmi_get_downgrade_lane_info(logic_id, &info);
    if (ret != 0) {
        roce_err("Failed to get downgrade lane info from dsmi. (ret=%u; logic_id=%d).", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    /* 打印相关信息 */
    tool_show_downgrade_lane_info(info);
    return UDA_EXE_SUCCESS;
}

int tool_downgrade_lane_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret = 0;
    int board_type = 0;
    int logic_id;
    int argc_tmp;

    if (argc < ARGV_ID_1) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_get_board_type(param, &board_type);
    if (ret != 0) {
        roce_err("Tool auto downgrade cmd get board type failed. (ret=%d)", ret);
        return UDA_DSMI_EXE_ERR;
    }

    if (board_type != SUPERPOD_900_BOARD_TYPE && board_type != A_K_BOARD_TYPE) {
        roce_err("Command not support the current product(boardType:%u).", board_type);
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    logic_id = param->logic_id;
    argc_tmp = argc - 1;
    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_auto_downgrade_info(argc_tmp, argv + 1, param);
        if (ret != 0) {
            roce_err("Get the auto downgrade info fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        ret = tool_set_auto_downgrade_info(argc_tmp, argv + 1, param);
        if (ret != 0) {
            roce_err("Set the setting of auto downgrade fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else {
        roce_err("Invalid operation.\n)");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

int tool_read_downgrade_info(struct tool_param *param, char **argv_tmp, char *switch_data, char *filter_time,
                             int *check_times)
{
    int ret;
    argv_tmp[0] = "enable";
    ret = cfg_read_conf(param->phy_id, DOWNGRADE_LANE_SWITCH, switch_data, PARAM_COUNT_THREE);
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            (*check_times)++;
            roce_info("The Downgrade filter time conf name is not found in config file, skipped. (ret=%d)", ret);
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    }
    argv_tmp[1] = switch_data;

    argv_tmp[INDEX_TWO] = "filter_time";
    ret = cfg_read_conf(param->phy_id, DOWNGRADE_LANE_FILTER_TIME, filter_time, PARAM_COUNT_THREE);
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            (*check_times)++;
            roce_info("The Downgrade filter time conf name is not found in config file, skipped. (ret=%d)", ret);
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    }
    argv_tmp[INDEX_TWO + 1] = filter_time;
    return 0;
}

int tool_cfg_downgrade_lane_recovery(struct tool_param *param)
{
#ifdef HCCN_TOOL_SHOW_EXT
    char downgrade_switch_data[PARAM_COUNT_THREE] = {"0"};
    char filter_time_data[PARAM_COUNT_THREE] = {"7"};
    char *argv_tmp[PARAM_COUNT_FOUR] = {NULL};
    int board_type;
    int ret;
    int check_times = 0;

    ret = tool_get_board_type(param, &board_type);
    if (ret != 0) {
        roce_err("Tool downgrade lane cmd get board type failed. (ret=%d)", ret);
        return UDA_DSMI_EXE_ERR;
    }

    if (board_type != SUPERPOD_900_BOARD_TYPE && board_type != A_K_BOARD_TYPE) {
        roce_info("Downgrade lane cfg not need recover.");
        return 0;
    }

    ret = tool_read_downgrade_info(param, argv_tmp, downgrade_switch_data, filter_time_data, &check_times);
    if (ret != 0) {
        roce_err("Read downgrade info err. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return ret;
    }

    if (check_times == DOWNGRADE_FILE_NUM) {
        roce_info("The Downgrade filter time conf name is not found in config file, skipped.");
    } else {
        ret = tool_set_auto_downgrade_info(PARAM_COUNT_FOUR, argv_tmp, param);
        if (ret != 0) {
            roce_err("Cfg downgrade lane recovery failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
    }
#endif

    return 0;
}

static int tool_a3_ipaddr_conflict_extend(int logic_id, unsigned int phy_id, unsigned int ipaddr, unsigned int mask)
{
    int ret;
    const char *vnic_a3_spod_addr[MAX_A3_LIMIT_IP_NUMS] = {
        "192.0.0.0", "192.168.0.0"}; // index 0代表超节点内的ip限制，1代表非超节点内
    const char *vnic_a3_spod_mask[MAX_A3_LIMIT_IP_NUMS] = {"255.0.0.0", "255.255.248.0"};
    unsigned int addr_index = 0;
    unsigned int vnic_h_ipaddr = 0;
    unsigned int vnic_h_mask = 0;
    struct dsmi_spod_info spod_info = {0};
    unsigned int spod_info_size = sizeof(struct dsmi_spod_info);

    ret = dsmi_get_device_info(logic_id, DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_SPOD_INFO, &spod_info,
                               &spod_info_size);
    if (ret) {
        roce_err("Dsmi_get_device_info failed.(ret=%d; main_cmd=%u; sub_cmd=%u)", ret, DSMI_MAIN_CMD_CHIP_INF,
                 DSMI_CHIP_INF_SUB_CMD_SPOD_INFO);
        return ret;
    }

    addr_index = (spod_info.server_id < A3_SUPERPOD_MAX_NUMS) ? A3_IN_SUPERPOD_INDEX : A3_NOT_IN_SUPERPOD_INDEX;
    vnic_h_ipaddr = inet_addr(vnic_a3_spod_addr[addr_index]);
    vnic_h_mask = inet_addr(vnic_a3_spod_mask[addr_index]);
    if ((ipaddr & vnic_h_mask) == (vnic_h_ipaddr & vnic_h_mask)) {
        roce_err("A3 product get same network address. "
                 "(vnic_h_ipaddr=0x%x; vnic_h_mask=0x%x; addr_index=%u; server_id=%u)",
                 vnic_h_ipaddr, vnic_h_mask, addr_index, spod_info.server_id);
        return UDA_PARAM_CONFLICT_IP_ADDR_ERR;
    }

    return 0;
}

static int tool_a1_a2_ipaddr_conflict_extend(int logic_id, unsigned int phy_id, unsigned int ipaddr, unsigned int mask)
{
    int ret;
    const char *vnic_addr[MAX_LIMIT_IP_NUMS] = {"192.168.2.0", "192.168.3.0", "192.168.4.0", "192.168.5.0"};
    const char *vnic_mask = "255.255.255.0";
    unsigned int vnic_h_ipaddr = 0;
    unsigned int vnic_h_mask = 0;
    unsigned int work_mode = 0; // 0 - AMP, 1 - SMP

    /* A1、A2产品AMP模式网段拦截 */
    vnic_h_ipaddr = inet_addr(vnic_addr[0]);
    vnic_h_mask = inet_addr(vnic_mask);
    if ((ipaddr & vnic_h_mask) == (vnic_h_ipaddr & vnic_h_mask)) {
        roce_err("AMP mode get same network address. (vnic_h_ipaddr=0x%x; vnic_h_mask=0x%x)", vnic_h_ipaddr,
                 vnic_h_mask);
        return UDA_PARAM_CONFLICT_IP_ADDR_ERR;
    }

#ifdef HCCN_TOOL_SHOW_LIST
    /* A1产品SMP模式网段拦截 */
    ret = dsmi_get_work_mode(&work_mode);
    if (ret != 0) {
        roce_err("Get work mode failed. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }
    if (work_mode == 0) {
        return 0;
    }
    for (int i = SMP_VNIC_IP_INDEX; i < MAX_LIMIT_IP_NUMS; i++) {
        vnic_h_ipaddr = inet_addr(vnic_addr[i]);
        vnic_h_mask = inet_addr(vnic_mask);
        if ((ipaddr & vnic_h_mask) == (vnic_h_ipaddr & vnic_h_mask)) {
            roce_err("A1 product SMP mode get same network address. vnic_h_ipaddr=0x%x; vnic_h_mask=0x%x; i=%d)",
                     vnic_h_ipaddr, vnic_h_mask, i);
            return UDA_PARAM_CONFLICT_IP_ADDR_ERR;
        }
    }
#endif

    return 0;
}

int tool_ipaddr_conflict_extend(int logic_id, unsigned int phy_id, unsigned int ipaddr, unsigned int mask)
{
    int ret;
    unsigned int mainboard_id = 0;

    ret = dsmi_get_mainboard_id(phy_id, &mainboard_id);
    if (ret != 0) {
        roce_err("Call dsmi get mainboard id failed. (ret=%d, phy_id=%u)", ret, phy_id);
        return ret;
    }

    if (is_ATLAS_900_A3_SUPERPOD(mainboard_id) || is_ATLAS_9000_A3_SUPERPOD(mainboard_id)) {
        return tool_a3_ipaddr_conflict_extend(logic_id, phy_id, ipaddr, mask); // A3产品网段拦截
    } else {
        return tool_a1_a2_ipaddr_conflict_extend(logic_id, phy_id, ipaddr, mask); // A1、A2产品网段拦截
    }
}

int tool_ip_read_gateway_conf_v2(int phy_id, bool is_pcie_card, unsigned int *gateway, int *gateway_phy_id)
{
    int ret;
    unsigned int gw_ip = 0;
    char gw[MAX_IP_LEN] = "";

    ret = cfg_read_conf(phy_id, "gateway", gw, sizeof(gw));
    if (ret) {
        if (ret != UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
            roce_err("Read config fail. (phy_id=%d; ret=%d; len=%d))", phy_id, ret, sizeof(gw));
        }
        return ret;
    }

    ret = tool_inet_pton_ipv4_address(gw, &gw_ip);
    if (ret) {
        roce_err("Tool inet pton ipv4 address fail. (ret=%d; phy_id=%d; gw_ip=0x%x)", ret, phy_id, gw_ip);
        return ret;
    }

    *gateway_phy_id = phy_id;
    *gateway = gw_ip;
    return 0;
}

int tool_dump_execute(int argc, char **argv, struct tool_param *param)
{
    int i, ret, index = -1;
    struct send_data_info data_info;
    char *target[] = {"adapt_info"};

    // -dump命令后面必须是 -t + 目标
    if (argc != ARGC_ID_2) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    if (strcmp(argv[0], "-t")) {
        roce_err("Unexpected input. (input=%s)", argv[0]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    // 确认目标代号
    for (i = 0; i < sizeof(target) / sizeof(target[0]); i++) {
        if (strcmp(argv[1], target[i]) == 0) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        roce_err("Unknown target. (target=%s)", argv[1]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    // dump命令不取回数据，直接在imp日志中打印
    TOOL_SET_SEND_DATA(data_info, DS_DUMP_INFO, &index, sizeof(int), NULL, 0);
    ret = dsmi_send_data(param->logic_id, 0, &data_info);
    if (ret) {
        roce_err("Dsmi send data failed. (ret=%d; logic_id=%d; index=%d)", ret, param->logic_id, index);
    }
    return ret == -EACCES ? UDA_TOOL_SYS_NOT_ACCESS : ret;
}

static bool tool_is_mem_reserve_size_valid(unsigned int size)
{
    /* Valid values: 0, 2, 4, 8, 16, 32, 64, 128, 256, 512 (power of 2 and <= 512) */
    if (size > MEM_RESERVE_MAX_SIZE_MB || size == 1 || (size != 0 && (size & (size - 1)) != 0)) {
        roce_err("Invalid mem_reserve size. (size=%u, valid values: 0,2,4,8,16,32,64,128,256,512)", size);
        return false;
    }
    return true;
}

static int tool_get_mem_reserve_page_size(unsigned int *page_size)
{
    long page_size_bytes;

    if (page_size == NULL) {
        roce_err("page_size is NULL");
        return -EINVAL;
    }

    page_size_bytes = sysconf(_SC_PAGESIZE);
    if (page_size_bytes <= 0) {
        roce_err("sysconf _SC_PAGESIZE fail. (page_size_bytes=%ld)", page_size_bytes);
        return -EIO;
    }

    if (page_size_bytes == (long)(MEM_RESERVE_PAGE_SIZE_4KB * MEM_RESERVE_KB_TO_BYTES)) {
        *page_size = MEM_RESERVE_PAGE_SIZE_4KB;
    } else if (page_size_bytes == (long)(MEM_RESERVE_PAGE_SIZE_64KB * MEM_RESERVE_KB_TO_BYTES)) {
        *page_size = MEM_RESERVE_PAGE_SIZE_64KB;
    } else {
        roce_err("Invalid page_size, only %uKB or %uKB supported. (page_size_bytes=%ld)", MEM_RESERVE_PAGE_SIZE_4KB,
                 MEM_RESERVE_PAGE_SIZE_64KB, page_size_bytes);
        return -EIO;
    }

    return 0;
}

/*
 * Parse mem_reserve config: format "{type}:{size}"
 * Return 0 on success, negative on error, 1 if config not found or invalid
 */
STATIC int tool_parse_mem_reserve_conf(const char *conf_value, unsigned int *type, unsigned int *size)
{
    char *colon = NULL;
    char *tmp_str = NULL;
    unsigned int i;
    unsigned int type_str_len;

    if (conf_value == NULL || type == NULL || size == NULL) {
        return -EINVAL;
    }

    /* 解析格式 "{type}:{size}" - 先找到冒号位置 */
    colon = strchr(conf_value, ':');
    if (colon == NULL) {
        /* 缺少冒号，配置无效 */
        return UDA_PARAM_INVALID_ERR;
    }

    /* 计算类型字符串长度（冒号之前的长度） */
    type_str_len = (unsigned int)(colon - conf_value);

    /* 确保类型部分完全匹配 */
    for (i = 0; i < sizeof(g_mem_reserve_type_map) / sizeof(g_mem_reserve_type_map[0]); i++) {
        unsigned int map_type_len = strlen(g_mem_reserve_type_map[i].type_str);
        if (type_str_len == map_type_len &&
            strncmp(conf_value, g_mem_reserve_type_map[i].type_str, map_type_len) == 0) {
            *type = g_mem_reserve_type_map[i].type;
            break;
        }
    }
    if (i >= sizeof(g_mem_reserve_type_map) / sizeof(g_mem_reserve_type_map[0])) {
        return UDA_PARAM_INVALID_ERR; /* Invalid type */
    }

    /* 解析 size */
    *size = (unsigned int)strtol(colon + 1, &tmp_str, MEM_RESERVE_STRTOL_BASE);
    /* 检查转换是否成功：tmp_str 应该指向字符串结尾 */
    if ((*tmp_str != '\0') || !tool_is_mem_reserve_size_valid(*size)) {
        return UDA_PARAM_INVALID_ERR; /* size 部分包含无效字符 */
    }

    return 0;
}

/*
 * Get type string from type enum
 */
STATIC const char *tool_mem_reserve_get_type_str(unsigned int type)
{
    unsigned int i;
    for (i = 0; i < sizeof(g_mem_reserve_type_map) / sizeof(g_mem_reserve_type_map[0]); i++) {
        if (g_mem_reserve_type_map[i].type == type) {
            return g_mem_reserve_type_map[i].type_str;
        }
    }
    return NULL;
}

/*
 * Save result to config file and print message based on result code
 */
STATIC int tool_mem_reserve_save_result(struct tool_param *param, unsigned int result, unsigned int type,
                                        unsigned int size)
{
    int ret;
    char conf_value[32] = {0};
    const char *type_str = NULL;

    /* Get type string from type enum */
    type_str = tool_mem_reserve_get_type_str(type);
    if (type_str == NULL) {
        roce_err("Invalid mem_reserve type. (type=%u)", type);
        return UDA_PARAM_INVALID_ERR;
    }

    ret = snprintf_s(conf_value, sizeof(conf_value), sizeof(conf_value) - 1, "%s:%u", type_str, size);
    if (ret <= 0 || ret >= (int)sizeof(conf_value)) {
        roce_err("Snprintf_s failed or truncated. (ret=%d, buf_size=%zu)", ret, sizeof(conf_value));
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    /* Skip config file write during recovery */
    if (g_recovering) {
        return UDA_EXE_SUCCESS;
    }

    switch (result) {
        case RESERVE_MEM_RESULT_SUCCESS:
        case RESERVE_MEM_RESULT_REBOOT_REQUIRED:
            ret = cfg_write_conf(param->phy_id, "mem_reserve", conf_value);
            if (ret != 0) {
                roce_err("Write mem_reserve config fail. (ret=%d)", ret);
                return ret;
            }
            break;
        case RESERVE_MEM_RESULT_CANCEL_REBOOT:
        case RESERVE_MEM_RESULT_CANCEL_SUCCESS:
            ret = cfg_del_conf(param->phy_id, "mem_reserve");
            if (ret != 0) {
                roce_err("Delete mem_reserve config fail. (ret=%d)", ret);
                return ret;
            }
            break;
        default:
            roce_err("Unknown result. (result=%u)", result);
            return UDA_PARAM_INVALID_ERR;
    }

    if (result == RESERVE_MEM_RESULT_CANCEL_REBOOT || result == RESERVE_MEM_RESULT_REBOOT_REQUIRED) {
        TOOL_PRINT_INFO("[Info] The configuration takes effect only after the device is rebooted.");
    }

    return UDA_EXE_SUCCESS;
}

/*
 * Execute mem_reserve set operation:
 * 1. Get page_size
 * 2. Call dsmi_set_mem_reserve
 * 3. Handle config file and print result
 */
STATIC int tool_mem_reserve_set_execute(struct tool_param *param, unsigned int type, unsigned int size)
{
    int ret;
    struct reserve_mem_info info = {0};

    info.type = type;
    info.mem_size = size;

    ret = tool_get_mem_reserve_page_size(&info.page_size);
    if (ret != 0) {
        roce_err("Get page_size fail. (logic_id=%d, ret=%d)", param->logic_id, ret);
        return ret;
    }

    ret = dsmi_set_mem_reserve(param->logic_id, &info);
    if (ret != 0) {
        roce_err("DSMI set mem_reserve fail. (ret=%d; dev_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    roce_info("DSMI set mem_reserve success. (logic_id=%d; type=%u; mem_size=%uMB; page_size=%uKB; result=%u)",
              param->logic_id, info.type, info.mem_size, info.page_size, info.result);

    return tool_mem_reserve_save_result(param, info.result, type, size);
}

/*
 * Execute mem_reserve get operation:
 * 1. Query device side for current and rebooted config
 * 2. Print result
 */
STATIC int tool_mem_reserve_get_execute(struct tool_param *param)
{
    int ret;
    struct reserve_mem_info info = {0};

    /* Query device side to get both current and rebooted config values */
    ret = dsmi_get_mem_reserve(param->logic_id, &info);
    if (ret != 0) {
        roce_err("DSMI get mem_reserve fail. (ret=%d; dev_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    TOOL_PRINT_INFO("Process Type              :  %s", (info.type == RESERVE_MEM_TYPE_PDCCL) ? "pdccl" : "unknown");
    TOOL_PRINT_INFO("Cur Reserve MEM Size      :  %u MB", info.mem_size);
    TOOL_PRINT_INFO("Rebooted Reserve MEM Size :  %u MB", info.rebooted_size);

    return UDA_EXE_SUCCESS;
}

/*
 * Entry function for -g (get) operation
 */
STATIC int tool_mem_reserve_get(struct tool_param *param)
{
    return tool_mem_reserve_get_execute(param);
}

/*
 * Handle -s (set) operation
 * Command format: -s type %s size %u
 */
STATIC int tool_mem_reserve_cmd_set(int argc, char **argv, struct tool_param *param)
{
    int ret;
    unsigned int type;
    unsigned int size;
    char *tmp_str = NULL;
    unsigned int i;

    if (argc != MEM_RESERVE_SET_ARGC) {
        roce_err("Invalid argc for -s. (argc=%d, expected=%d)", argc, MEM_RESERVE_SET_ARGC);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    /* Parse "type %s size %u" format */
    /* Check "type" keyword */
    if (strcmp(argv[0], "type") != 0) {
        roce_err("Invalid param: expected 'type', got '%s'", argv[0]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    /* Parse type value from type map */
    for (i = 0; i < sizeof(g_mem_reserve_type_map) / sizeof(g_mem_reserve_type_map[0]); i++) {
        if (strcmp(argv[1], g_mem_reserve_type_map[i].type_str) == 0) {
            type = g_mem_reserve_type_map[i].type;
            break;
        }
    }
    if (i >= sizeof(g_mem_reserve_type_map) / sizeof(g_mem_reserve_type_map[0])) {
        roce_err("Invalid type value: %s", argv[1]);
        return UDA_PARAM_INVALID_ERR;
    }

    /* Check "size" keyword */
    if (strcmp(argv[MEM_RESERVE_ARG_IDX_SIZE], "size") != 0) {
        roce_err("Invalid param: expected 'size', got '%s'", argv[MEM_RESERVE_ARG_IDX_SIZE]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    /* Parse size value */
    size = (unsigned int)strtol(argv[MEM_RESERVE_ARG_IDX_VALUE], &tmp_str, MEM_RESERVE_STRTOL_BASE);
    if ((*tmp_str != '\0') || !tool_is_mem_reserve_size_valid(size)) {
        roce_err("Invalid size value. (size=%s)", argv[MEM_RESERVE_ARG_IDX_VALUE]);
        return UDA_PARAM_INVALID_ERR;
    }

    LOG_SET_CONFIG("Start to set mem_reserve.(phy_id=%d; type=%s; size=%u)", param->phy_id,
                   tool_mem_reserve_get_type_str(type), size);
    ret = tool_mem_reserve_set_execute(param, type, size);
    LOG_SET_CONFIG("End to set mem_reserve.(phy_id=%d; type=%s; size=%u; ret=%d)", param->phy_id,
                   tool_mem_reserve_get_type_str(type), size, ret);
    if (ret != 0) {
        roce_err("set mem_reserve fail. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return ret;
    }
    return 0;
}

/*
 * Handle -g (get) operation
 */
STATIC int tool_mem_reserve_cmd_get(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc != 0) {
        roce_err("Invalid argc for -g. (argc=%d, expected=0)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_mem_reserve_get(param);
    if (ret != 0) {
        roce_err("get mem_reserve fail. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return ret;
    }
    return 0;
}

/*
 * Main entry for mem_reserve command
 * Command format:
 *   hccn_tool -i device_id -mem_reserve -s type pdccl size 256
 *   hccn_tool -i device_id -mem_reserve -g
 */
int tool_mem_reserve_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc < 1) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (strncmp(argv[0], "-g", strlen("-g") + 1) == 0) {
        param->get_cmd_flag = true;
        return tool_mem_reserve_cmd_get(argc - 1, argv + 1, param);
    } else if (strncmp(argv[0], "-s", strlen("-s") + 1) == 0) {
        return tool_mem_reserve_cmd_set(argc - 1, argv + 1, param);
    } else {
        roce_err("Invalid option: must specify -g or -s. (argv[0]=%s)", argv[0]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
}

int tool_cfg_mem_reserve_recovery(struct tool_param *param)
{
    int ret;
    char conf_value[MEM_RESERVE_CONFIG_LENGTH] = {0};
    char size_str[16] = {0};
    char *argv_tmp[MEM_RESERVE_SET_ARGC] = {NULL};
    unsigned int type;
    unsigned int size;
    unsigned int i;
    const char *type_str = NULL;

    ret = cfg_read_conf(param->phy_id, "mem_reserve", conf_value, MEM_RESERVE_CONFIG_LENGTH);
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
            return 0;
        }
        roce_err("Read mem_reserve config fail. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return ret;
    }

    ret = tool_parse_mem_reserve_conf(conf_value, &type, &size);
    if (ret != 0) {
        roce_err("Parse mem_reserve conf failed in recovery. (ret=%d; conf_value=%s)", ret, conf_value);
        return ret;
    }

    /* Construct command line arguments: "type" "pdccl" "size" "256" */
    argv_tmp[0] = "type";
    type_str = tool_mem_reserve_get_type_str(type);
    if (type_str == NULL) {
        roce_err("Invalid mem_reserve type in recovery. (type=%u)", type);
        return UDA_PARAM_INVALID_ERR;
    }
    argv_tmp[1] = (char *)type_str;
    argv_tmp[ARGC_ID_2] = "size";

    ret = snprintf_s(size_str, sizeof(size_str), sizeof(size_str) - 1, "%u", size);
    if (ret <= 0) {
        roce_err("Snprintf_s size_str failed in recovery. (ret=%d)", ret);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    argv_tmp[ARGC_ID_3] = size_str;

    /* Call command line set interface directly */
    ret = tool_mem_reserve_cmd_set(MEM_RESERVE_SET_ARGC, argv_tmp, param);
    if (ret != 0) {
        roce_err("Cfg mem_reserve recovery failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }

    return 0;
}
