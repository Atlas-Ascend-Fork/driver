/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ds_net_ext.h"
#include "tool_ext.h"
#include "tool.h"
#include "tool_ops.h"

#define TOOL_TC_QOS_PARAM_TYPE_TCBW 0
#define TOOL_TC_QOS_PARAM_TYPE_TSA 1

static int tool_get_tc_qos_info(int argc, char **argv, struct tool_param *param)
{
    int ret;
    char tc_qos_info[TOOL_TC_QOS_OUTBUFF_LEN] = {0};
    char tc_bw[TOOL_TC_QOS_INFO_TEMP_LENGTH] = {0};
    char tsa_map[TOOL_TC_QOS_INFO_TEMP_LENGTH] = {0};
    char *ptr = NULL;

    if (argc != 0) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_operate_tc_qos_info(param->logic_id, tc_qos_info, sizeof(tc_qos_info), DS_GET_TC_QOS_INFO);
    if (ret) {
        roce_err("Failed to get tc qos info. (ret=%d, logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    // 找到 TC Bandwidth
    ptr = strstr(tc_qos_info, "TC Bandwidth:");
    if (!ptr) {
        roce_err("TC Bandwidth field not found. (tc_qos_info=%s).", tc_qos_info);
        return -ENOEXEC;
    }
    ret = sscanf_s(ptr, "TC Bandwidth:%127[^\n]", tc_bw, sizeof(tc_bw));
    if (ret <= 0) {
        roce_err("TC Bandwidth field not found. (ret=%d, tc_qos_info=%s).", ret, tc_qos_info);
        return -ENOEXEC;
    }
    // 找到 TSA_MAP
    ptr = strstr(tc_qos_info, "TSA_MAP:");
    if (!ptr) {
        roce_err("TSA_MAP field not found. (tc_qos_info=%s).", tc_qos_info);
        return -ENOEXEC;
    }
    ret = sscanf_s(ptr, "TSA_MAP:%127[^\n]", tsa_map, sizeof(tsa_map));
    if (ret <= 0) {
        roce_err("TSA_MAP field not found. (ret=%d, tc_qos_info=%s).", ret, tc_qos_info);
        return -ENOEXEC;
    }
    // 拼接
    ret = snprintf_s(tc_qos_info, TOOL_TC_QOS_OUTBUFF_LEN, TOOL_TC_QOS_OUTBUFF_LEN - 1,
                     "TC Bandwidth: %s\nTSA_MAP: %s\0", tc_bw, tsa_map);
    if (ret <= 0) {
        roce_err("snprintf_s fail, ret:%d, dest_len:%d", ret, TOOL_TC_QOS_OUTBUFF_LEN);
        return -ENOMEM;
    }

    TOOL_PRINT_INFO("%s", tc_qos_info);
    return 0;
}

static int tool_tc_qos_init_prio_map_info(const char *tc_qos_info, struct tool_tc_qos_info *info)
{
    int ret;
    int index = 0;
    char *str;
    char *token;
    char *colon;
    char prio_map_info[TOOL_TC_QOS_INFO_TEMP_LENGTH] = {0};

    str = strstr(tc_qos_info, "PRIO_MAP:");
    if (!str) {
        roce_err("PRIO_MAP field not found. (tc_qos_info=%s).", tc_qos_info);
        return UDA_DSMI_EXE_ERR;
    }
    ret = sscanf_s(str, "PRIO_MAP:%127[^\n]", prio_map_info, TOOL_TC_QOS_INFO_TEMP_LENGTH);
    if (ret <= 0) {
        roce_err("PRIO_MAP field not found. (ret=%d, tc_qos_info=%s).", ret, tc_qos_info);
        return UDA_DSMI_EXE_ERR;
    }

    token = strtok(prio_map_info, " ");
    while (token != NULL && index < TOOL_TC_QOS_INFO_TC_COUNT) {
        colon = strchr(token, ':');
        if (colon == NULL) {
            roce_err("Invalid tc qos prio_map info. (tc_qos_info=%s)", tc_qos_info);
            return UDA_DSMI_EXE_ERR;
        }
        *colon = '\0';
        info->prio_map[index] = strtoul(colon + 1, NULL, 0);
        token = strtok(NULL, " ");
        index++;
    }

    if (index != TOOL_TC_QOS_INFO_TC_COUNT) {
        roce_err("Invalid tc qos prio_map info. (tc_qos_info=%s)", tc_qos_info);
        return UDA_DSMI_EXE_ERR;
    }

    return 0;
}

static int tool_tc_qos_init_tc_bw_info(const char *tc_qos_info, struct tool_tc_qos_info *info)
{
    int ret;
    int index = 0;
    char *str;
    char *token;
    char *colon;
    char tc_bw_info[TOOL_TC_QOS_INFO_TEMP_LENGTH] = {0};

    str = strstr(tc_qos_info, "TC Bandwidth:");
    if (!str) {
        roce_err("TC Bandwidth field not found. (tc_qos_info=%s).", tc_qos_info);
        return UDA_DSMI_EXE_ERR;
    }
    ret = sscanf_s(str, "TC Bandwidth:%127[^\n]", tc_bw_info, TOOL_TC_QOS_INFO_TEMP_LENGTH);
    if (ret <= 0) {
        roce_err("TC Bandwidth field not found. (ret=%d, tc_qos_info=%s).", ret, tc_qos_info);
        return UDA_DSMI_EXE_ERR;
    }

    token = strtok(tc_bw_info, " ");
    while (token != NULL && index < TOOL_TC_QOS_INFO_TC_COUNT) {
        colon = strchr(token, '%');
        if (colon == NULL) {
            roce_err("Invalid tc qos tc_bw info. (tc_qos_info=%s)", tc_qos_info);
            return UDA_DSMI_EXE_ERR;
        }
        *colon = '\0';
        info->tc_bw[index] = strtoul(token, NULL, 0);
        token = strtok(NULL, " ");
        index++;
    }

    if (index != TOOL_TC_QOS_INFO_TC_COUNT) {
        roce_err("Invalid tc qos tc_bw info. (tc_qos_info=%s)", tc_qos_info);
        return UDA_DSMI_EXE_ERR;
    }

    return 0;
}

static int tool_tc_qos_init_tsa_info(const char *tc_qos_info, struct tool_tc_qos_info *info)
{
    int ret;
    int index = 0;
    char *str;
    char *token;
    char *colon;
    char tsa_info[TOOL_TC_QOS_INFO_TEMP_LENGTH] = {0};

    str = strstr(tc_qos_info, "TSA_MAP:");
    if (!str) {
        roce_err("TSA_MAP field not found. (tc_qos_info=%s).", tc_qos_info);
        return UDA_DSMI_EXE_ERR;
    }
    ret = sscanf_s(str, "TSA_MAP:%127[^\n]", tsa_info, TOOL_TC_QOS_INFO_TEMP_LENGTH);
    if (ret <= 0) {
        roce_err("TSA_MAP field not found. (ret=%d, tc_qos_info=%s).", ret, tc_qos_info);
        return UDA_DSMI_EXE_ERR;
    }

    token = strtok(tsa_info, " ");
    while (token != NULL && index < TOOL_TC_QOS_INFO_TC_COUNT) {
        colon = strchr(token, ':');
        if (colon == NULL) {
            roce_err("Invalid tc qos tsa info. (tc_qos_info=%s)", tc_qos_info);
            return UDA_DSMI_EXE_ERR;
        }
        *colon = '\0';
        if (strcmp(colon + 1, "ets") == 0) {
            info->tsa_map[index] = TSA_ETS;
        } else if (strcmp(colon + 1, "strict") == 0) {
            info->tsa_map[index] = TSA_STRICT;
        } else {
            roce_err("Invalid tc qos tsa param. (tc_qos_info=%s)", tc_qos_info);
            return UDA_PARAM_INVALID_ERR; // 不是ets或strict
        }
        token = strtok(NULL, " ");
        index++;
    }

    if (index != TOOL_TC_QOS_INFO_TC_COUNT) {
        roce_err("Invalid tc qos tsa info. (tc_qos_info=%s)", tc_qos_info);
        return UDA_DSMI_EXE_ERR;
    }

    return 0;
}

static int tool_init_tc_qos_info(int logic_id, struct tool_tc_qos_info *info)
{
    int ret;
    char tc_qos_info[TOOL_TC_QOS_OUTBUFF_LEN] = {0};

    ret = dsmi_operate_tc_qos_info(logic_id, tc_qos_info, TOOL_TC_QOS_OUTBUFF_LEN, DS_GET_TC_QOS_INFO);
    if (ret) {
        roce_err("Failed to get tc qos info. (ret=%d, logic_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = tool_tc_qos_init_prio_map_info(tc_qos_info, info);
    if (ret != 0) {
        return ret;
    }

    ret = tool_tc_qos_init_tc_bw_info(tc_qos_info, info);
    if (ret != 0) {
        return ret;
    }

    ret = tool_tc_qos_init_tsa_info(tc_qos_info, info);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

static int tool_tc_qos_check_param_info(struct tool_tc_qos_info *info)
{
    int i;
    int count = 0;
    int tsa_sum = 0;
    int max_tc_id = 0;

    for (i = 0; i < TOOL_TC_QOS_INFO_TC_COUNT; i++) {
        if (info->prio_map[i] > max_tc_id) {
            max_tc_id = info->prio_map[i];
        }
    }

    for (i = 0; i < TOOL_TC_QOS_INFO_PARAM_COUNT; i++) {
        if (info->tsa_map[i] == TSA_STRICT) {
            if (info->tc_bw[i] != 0) {
                roce_err("Strict value is invalid. (value=%d)", info->tc_bw[i]);
                return UDA_PARAM_INVALID_ERR;
            }
        } else if (info->tsa_map[i] == TSA_ETS) {
            if ((info->tc_bw[i] <= TOOL_TC_QOS_INFG_TC_BW_MIN) || (info->tc_bw[i] > TOOL_TC_QOS_INFG_TC_BW_MAX)) {
                roce_err("Check tc qos param info failed. Ets mode tc value is invalid. (tc_id=%d, value=%d)", i,
                         info->tc_bw[i]);
                return UDA_PARAM_INVALID_ERR;
            }
            // lldp_tool约束，ets模式的tc的值不能大于优先级中配置的tc最大值
            if (i > max_tc_id) {
                roce_err("Invalid tc qos tc_bw info. (tc_id=%d, max_tc_id=%d)", i, max_tc_id);
                return UDA_TOOL_CHECK_PRIO_TC_CONFIG;
            }
            count += info->tc_bw[i];
            tsa_sum++;
        }
    }

    if (tsa_sum > 0 && count != TOOL_TC_QOS_INFG_TC_BW_MAX) {
        roce_err("Tcbw param is invalid. (count=%d)", count);
        return UDA_PARAM_INVALID_ERR;
    }

    // 全为strict情况，lldp_tool约束tcbw总和为100，所以此处默认设置tc_bw[7]为100
    if (tsa_sum == 0) {
        info->tc_bw[7] = TOOL_TC_QOS_INFG_TC_BW_MAX; // 当前使用tc0~3,此处填写7规避该问题
    } else {
        info->tc_bw[7] = 0; // 非全strict场景，需将tcbw[7]中还原为0
    }

    return 0;
}

static int tool_check_and_set_qos_value(char *value, int param_type, struct tool_tc_qos_info *info, int index)
{
    char *endptr = NULL;
    long result;

    if (param_type == TOOL_TC_QOS_PARAM_TYPE_TSA) {
        if (strcmp(value, "ets") == 0) {
            info->tsa_map[index] = TSA_ETS;
        } else if (strcmp(value, "strict") == 0) {
            info->tsa_map[index] = TSA_STRICT;
        } else {
            return UDA_PARAM_INVALID_ERR; // 不是ets或strict
        }
    } else if (param_type == TOOL_TC_QOS_PARAM_TYPE_TCBW) {
        result = strtol(value, &endptr, STR_TO_INT_TEN);
        if (*endptr != '\0' || endptr == value || result < TOOL_TC_QOS_INFG_TC_BW_MIN ||
            result > TOOL_TC_QOS_INFG_TC_BW_MAX) {
            return UDA_PARAM_INVALID_ERR;
        }
        info->tc_bw[index] = (char)result;
    }
    return 0;
}

static int tool_set_tc_qos_parse_and_check_input_param(char *param, struct tool_tc_qos_info *info, int param_type)
{
    char *token = NULL;
    char *colon = NULL;
    char *value = NULL;
    int index = -1;
    int ret;
    int used[TOOL_TC_QOS_INFO_PARAM_COUNT] = {0}; // 用于检查数字0~3是否重复
    char param_temp[TOOL_TC_QOS_INFO_TEMP_LENGTH] = {0};
    char *temp = NULL;

    ret = strncpy_s(param_temp, TOOL_TC_QOS_INFO_TEMP_LENGTH, param, TOOL_TC_QOS_INFO_TEMP_LENGTH - 1);
    if (ret) {
        roce_err("strncpy_s failed! (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    // 分割字符串
    temp = param_temp;
    while ((token = strsep(&temp, ",")) != NULL) {
        // 防止两个连续逗号导致空字段被忽略
        if (strlen(token) == 0) {
            roce_err("Invalid tc qos param, empty field found. (param=%s)", param_temp);
            return UDA_PARAM_INVALID_ERR;
        }
        // 检查是否格式为 "X:Y"
        colon = strchr(token, ':');
        if (!colon || colon == token || *(colon + 1) == '\0') {
            roce_err("Invalid tc qos param. (param=%s)", param_temp);
            return UDA_PARAM_INVALID_ERR;
        }

        // 提取数字部分
        if (sscanf_s(token, "%d", &index) != 1 || index < 0 || index > (TOOL_TC_QOS_INFO_PARAM_COUNT - 1)) {
            roce_err("Invalid tc qos tsa param. (param=%s, index=%d)", param_temp, index);
            return UDA_PARAM_INVALID_ERR; // 索引不在0~3范围内
        }

        // 检查是否重复
        if (used[index]) {
            roce_err("Invalid tc qos tsa param. (param=%s)", param_temp);
            return UDA_PARAM_INVALID_ERR; // 重复
        }
        used[index] = 1;

        // 检查后面的值
        value = colon + 1;
        ret = tool_check_and_set_qos_value(value, param_type, info, index);
        if (ret) {
            roce_err("Invalid tc qos tsa param. (param=%s)", param_temp);
            return ret;
        }
    }

    return 0;
}

static int tool_set_tc_qos_write_conf(int phy_id, int argc, char **argv)
{
    int ret;
    int i;

    for (i = 0; i < argc; i += STEP_TWO) {
        if (strncmp(argv[i], "tsa", strlen("tsa") + 1) == 0) {
            ret = cfg_write_conf(phy_id, "tsa", argv[i + 1]);
            if (ret) {
                roce_err("Write tsa cfg error. (ret=%d; phy_id=%d)", ret, phy_id);
                return ret;
            }
        }
        if (strncmp(argv[i], "tcbw", strlen("tcbw") + 1) == 0) {
            ret = cfg_write_conf(phy_id, "tcbw", argv[i + 1]);
            if (ret) {
                roce_err("Write tsa cfg error. (ret=%d; phy_id=%d)", ret, phy_id);
                return ret;
            }
        }
    }

    return 0;
}

static int tool_set_tc_qos_info(int argc, char **argv, struct tool_param *param)
{
    int ret, i;
    char flag[2] = {0}; // Indicates the subcommand settings, total num is 2.
    struct tool_tc_qos_info info = {0};

    if (argc != ARGC_NUM_2 && argc != ARGC_NUM_4) {
        roce_err("tool_set_tc_qos_info err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    // 根据读取的tc qos信息，初始化tc qos info结构体
    ret = tool_init_tc_qos_info(param->logic_id, &info);
    if (ret) {
        roce_err("Failed to init tc qos info. (ret=%d, logic_id=%d)", ret, param->logic_id);
        return ret;
    }

    for (i = 0; i < argc; i += STEP_TWO) {
        if ((strncmp(argv[i], "tsa", strlen("tsa") + 1) == 0) && (flag[0] == 0)) {
            ret = tool_set_tc_qos_parse_and_check_input_param(argv[i + 1], &info, TOOL_TC_QOS_PARAM_TYPE_TSA);
            if (ret != 0) {
                return ret;
            }
            flag[0] = 1; // Indicates that the field has been set.
        } else if ((strncmp(argv[i], "tcbw", strlen("tcbw") + 1) == 0) && (flag[1] == 0)) {
            ret = tool_set_tc_qos_parse_and_check_input_param(argv[i + 1], &info, TOOL_TC_QOS_PARAM_TYPE_TCBW);
            if (ret != 0) {
                return ret;
            }
            flag[1] = 1; // Indicates that the field has been set.
        } else {
            roce_err("Invalid operation.)");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }
    ret = tool_tc_qos_check_param_info(&info);
    if (ret) {
        return ret;
    }

    info.recovery_flag = TOOL_TC_QOS_OPERATE_SET;
    ret = dsmi_operate_tc_qos_info(param->logic_id, &info, sizeof(struct tool_tc_qos_info), DS_SET_TC_QOS_INFO);
    if (ret) {
        roce_err("Failed to set tc qos info. (ret=%d, logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return 0;
}

int tool_tc_qos_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc == 0) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_tc_qos_info(argc - 1, argv + 1, param);
        if (ret) {
            roce_err("Get the tc qos configuration failed. (ret=%d)", ret);
            return ret;
        }
    } else if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        ret = tool_set_tc_qos_info(argc - 1, argv + 1, param);
        if (ret) {
            roce_err("Set the tc qos configuration failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
        ret = tool_set_tc_qos_write_conf(param->phy_id, argc - 1, argv + 1);
        if (ret) {
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }
    } else {
        roce_err("Invalid option.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return ret;
}

static int tool_cfg_tc_qos_read_cfg(int phy_id, char *flag, int *count, char *tsa_info, char *tcbw_info)
{
    int ret;

    ret = cfg_read_conf(phy_id, "tsa", tsa_info, TOOL_TC_QOS_INFO_TEMP_LENGTH);
    if (ret != 0) {
        if (ret != UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, phy_id);
            return ret;
        }
    } else {
        flag[0x0] = 1;
        (*count)++;
    }

    ret = cfg_read_conf(phy_id, "tcbw", tcbw_info, TOOL_TC_QOS_INFO_TEMP_LENGTH);
    if (ret != 0) {
        if (ret != UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, phy_id);
            return ret;
        }
    } else {
        flag[0x1] = 1;
        (*count)++;
    }

    return 0;
}

STATIC void tool_tc_qos_info_init(struct tool_tc_qos_info *info)
{
    int i;

    info->recovery_flag = TOOL_TC_QOS_OPERATE_INVALID;
    for (i = 0; i < TOOL_TC_QOS_INFO_TC_COUNT; i++) {
        info->prio_map[i] = TOOL_TC_QOS_INFO_PARAM_INVALID;
        info->tc_bw[i] = TOOL_TC_QOS_INFO_PARAM_INVALID;
        info->tsa_map[i] = TOOL_TC_QOS_INFO_PARAM_INVALID;
    }
}

int tool_cfg_tc_qos_recovery(struct tool_param *param)
{
    int ret, count = 0;
    struct tool_tc_qos_info info = {0};
    char tsa_info[TOOL_TC_QOS_INFO_TEMP_LENGTH] = {0};
    char tcbw_info[TOOL_TC_QOS_INFO_TEMP_LENGTH] = {0};
    char flag[2] = {0}; // Indicates the subcommand settings, total num is 2.

    ret = tool_cfg_tc_qos_read_cfg(param->phy_id, flag, &count, tsa_info, tcbw_info);
    if (ret) {
        return ret;
    }

    if (count == 0) {
        return 0;
    }

    tool_tc_qos_info_init(&info);

    if (flag[0x0] == 1) {
        ret = tool_set_tc_qos_parse_and_check_input_param(tsa_info, &info, TOOL_TC_QOS_PARAM_TYPE_TSA);
        if (ret != 0) {
            return ret;
        }
    }
    if (flag[0x1] == 1) {
        ret = tool_set_tc_qos_parse_and_check_input_param(tcbw_info, &info, TOOL_TC_QOS_PARAM_TYPE_TCBW);
        if (ret != 0) {
            return ret;
        }
    }

    info.recovery_flag = TOOL_TC_QOS_OPERATE_RECOVERY;
    ret = dsmi_operate_tc_qos_info(param->logic_id, &info, sizeof(struct tool_tc_qos_info), DS_SET_TC_QOS_INFO);
    if (ret) {
        roce_err("Failed to set tc qos info. (ret=%d, logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return 0;
}
