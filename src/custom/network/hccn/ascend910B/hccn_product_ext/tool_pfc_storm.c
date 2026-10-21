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
#include <limits.h>
#include <float.h>
#include <errno.h>
#include "tool_recovery.h"
#include "tool_ext.h"
#include "cmd.h"
#include "cfg.h"
#include "tls.h"
#include "tool.h"
#include "log.h"
#include "ds_net_ext.h"
#include "ossl_user_linux.h"
#include "tool_lib.h"

#define MAX_U32_PARAM_LENGTH 11
struct pfc_storm_parse {
    char *param;
    int (*parse_proc)(int argc, char **argv, int *argc_index, struct pfc_storm_cfg_param *user_param,
                      const struct dfx_u32_parse *dfx);
    unsigned int minv;
    unsigned int maxv;
};
struct pfc_storm_save {
    char *param;
    int (*save_proc)(char *inbuf, int length, struct pfc_storm_cfg_param *user_cfg_param);
};

int dfx_parse_uinteger_param(int argc, char **argv, int *argc_index, unsigned int *value,
                             const struct dfx_u32_parse *dfx)
{
    int ret;
    if (*argc_index + 1 >= argc) {
        roce_err("%s param [%s] has no value.", dfx->dfx_name, argv[*argc_index]); // 参数后面没有跟值
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    char *end;
    *value = strtoul(argv[*argc_index + 1], &end, STR_NUM_BASE);
    if (end == argv[*argc_index + 1] || *end != '\0') {
        roce_err("%s input value is not integer value. (value=[%u]; tail=[%s])", dfx->dfx_name, *value, end);
        return UDA_PARAM_INVALID_ERR;
    }

    if (*value < dfx->minv || *value > dfx->maxv) {
        roce_err("%s invalid param [%s].(value=[%u], min=[%u], max=[%u]))", dfx->dfx_name, argv[*argc_index], *value,
                 dfx->minv, dfx->maxv);
        return UDA_PARAM_INVALID_ERR;
    }
    char value_str[MAX_U32_PARAM_LENGTH] = {0};

    ret = sprintf_s(value_str, MAX_U32_PARAM_LENGTH, "%u", *value);
    if (ret <= 0) {
        roce_err("%s parse_uinteger_param sprintf_s for  faild. (ret=%d)", dfx->dfx_name, ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    if (strlen(value_str) != strlen(argv[*argc_index + 1])) {
        roce_err("%s parse_uinteger_param param length error. (ret=%d)", dfx->dfx_name, ret);
        return UDA_PARAM_INVALID_ERR;
    }

    *argc_index += 1;
    return UDA_EXE_SUCCESS;
}

STATIC int pfc_storm_parse_mode(int argc, char **argv, int *argc_index, struct pfc_storm_cfg_param *user_param,
                                const struct dfx_u32_parse *dfx)
{
    return dfx_parse_uinteger_param(argc, argv, argc_index, &user_param->mode, dfx);
}

STATIC int pfc_storm_parse_close_time(int argc, char **argv, int *argc_index, struct pfc_storm_cfg_param *user_param,
                                      const struct dfx_u32_parse *dfx)
{
    return dfx_parse_uinteger_param(argc, argv, argc_index, &user_param->close_time, dfx);
}

STATIC int pfc_storm_parse_detect_period(int argc, char **argv, int *argc_index, struct pfc_storm_cfg_param *user_param,
                                         const struct dfx_u32_parse *dfx)
{
    return dfx_parse_uinteger_param(argc, argv, argc_index, &user_param->detect_period, dfx);
}

STATIC int pfc_storm_parse_warn_thrd(int argc, char **argv, int *argc_index, struct pfc_storm_cfg_param *user_param,
                                     const struct dfx_u32_parse *dfx)
{
    return dfx_parse_uinteger_param(argc, argv, argc_index, &user_param->warn_thrd, dfx);
}

STATIC int pfc_storm_parse_err_thrd(int argc, char **argv, int *argc_index, struct pfc_storm_cfg_param *user_param,
                                    const struct dfx_u32_parse *dfx)
{
    return dfx_parse_uinteger_param(argc, argv, argc_index, &user_param->err_thrd, dfx);
}

STATIC struct pfc_storm_parse g_pfc_storm_parse_list[] = {
    {"mode", pfc_storm_parse_mode, PFC_STORM_WATCHDOG_MODE_0, PFC_STORM_WATCHDOG_MODE_2},
    {"close_time", pfc_storm_parse_close_time, PFC_STORM_WATCHDOG_CLOSE_TIME_MIN, PFC_STORM_WATCHDOG_CLOSE_TIME_MAX},
    {"detect_period", pfc_storm_parse_detect_period, PFC_STORM_WATCHDOG_DETECT_PERIOD_MIN,
     PFC_STORM_WATCHDOG_DETECT_PERIOD_MAX},
    {"warn_thrd", pfc_storm_parse_warn_thrd, PFC_STORM_WARN_THRD_MIN, PFC_STORM_WARN_THRD_MAX},
    {"err_thrd", pfc_storm_parse_err_thrd, PFC_STORM_ERR_THRD_MIN, PFC_STORM_ERR_THRD_MAX}};

STATIC int tool_pfc_storm_watchdog_param_list_parse(int argc, char **argv, struct pfc_storm_cfg_param *user_cfg_param,
                                                    struct tool_param *param_of_tool)
{
    int ret;
    int argc_idx;
    int parse_idx;
    int parse_list_size = sizeof(g_pfc_storm_parse_list) / sizeof(struct pfc_storm_parse);
    int parseed_flag[MAX_PFC_STORM_WATCHDOG_PARAM_NUM] = {0};
    struct dfx_u32_parse dfx = {"pfc storm watchdog", 0, 0};

    for (argc_idx = 0; argc_idx < argc; ++argc_idx) {
        for (parse_idx = 0; parse_idx < parse_list_size; ++parse_idx) {
            if (!strcmp(argv[argc_idx], g_pfc_storm_parse_list[parse_idx].param)) {
                break;
            }
        }

        if (parse_idx == parse_list_size) {
            roce_err("%s match param %s erro.", __func__, argv[argc_idx]);
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }

        if (parseed_flag[parse_idx] != 0) {
            roce_err("%s duplicate param [%s].", __func__, argv[argc_idx]);
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }

        dfx.minv = g_pfc_storm_parse_list[parse_idx].minv;
        dfx.maxv = g_pfc_storm_parse_list[parse_idx].maxv;
        ret = g_pfc_storm_parse_list[parse_idx].parse_proc(argc, argv, &argc_idx, user_cfg_param, &dfx);
        if (ret != UDA_EXE_SUCCESS) {
            roce_err("%s get param failed.(ret=[%d], phy_id=[%d], logic_id=[%d])", __func__, ret, param_of_tool->phy_id,
                     param_of_tool->logic_id);
            return ret;
        }

        parseed_flag[parse_idx] = 1;
    }

    return 0;
}

STATIC int tool_check_pfc_storm_watchdog_param(struct pfc_storm_cfg_param *user_cfg_param,
                                               const struct pfc_storm_cfg_param *watchdog_cfg_param)
{
    int ret;
    int err_changed_flag = 0, warn_changed_flag = 0, period_changed_flag = 0;

    if (user_cfg_param->close_time > user_cfg_param->detect_period) {
        roce_err("[ERROR]close_time cannot be greater than detector_period.");
        return UDA_PARAM_INVALID_ERR;
    }
    if (user_cfg_param->warn_thrd >= user_cfg_param->err_thrd) {
        roce_err("[ERROR]err_threshold cannot be less than or equal to warn_threshold.");
        return UDA_PARAM_INVALID_ERR;
    }
    const char *mode_description[PFC_STORM_WATCHDOG_MODE_NUM] = {
        "[Disable PFC storm detection]", "[Only warn and err counts are counted, and no closing action is performed]",
        "[Enable PFC storm detection]"};
    if (user_cfg_param->mode != watchdog_cfg_param->mode) {
        TOOL_PRINT_INFO("[Warning]mode has been changed to %u %s.", user_cfg_param->mode,
                        mode_description[user_cfg_param->mode]);
    }
    /* Comparison with existing configurations for double check */
    if (user_cfg_param->detect_period != watchdog_cfg_param->detect_period) {
        period_changed_flag = 1;
        TOOL_PRINT_INFO("[Warning]detect_period has been changed to %u(100ms).", user_cfg_param->detect_period);
    }

    if (user_cfg_param->err_thrd != watchdog_cfg_param->err_thrd) {
        err_changed_flag = 1;
        TOOL_PRINT_INFO("[Warning]err_thrd has been changed to %u%%.", user_cfg_param->err_thrd);
    }

    if (user_cfg_param->warn_thrd != watchdog_cfg_param->warn_thrd) {
        warn_changed_flag = 1;
        TOOL_PRINT_INFO("[Warning]warn_thrd has been changed to %u%%.", user_cfg_param->warn_thrd);
    }

    if (err_changed_flag == 1 || warn_changed_flag == 1 || period_changed_flag == 1) {
        ret = tool_double_check("Historical statistics may no longer be applicable.\n"
                                "You should clear the statistics count with command "
                                "'hccn_tool [-i %d] -pfc_stat -c'\n"
                                "Are you sure you want to continue?");
        if (ret) {
            return ret;
        }
    }
    return UDA_EXE_SUCCESS;
}

STATIC int tool_pfc_storm_watchdog_param_parse(int argc, char **argv, int logic_id,
                                               struct pfc_storm_cfg_param *user_cfg_param,
                                               struct tool_param *param_of_tool, unsigned int *new_param_flag)
{
    int ret;

    struct pfc_storm_cfg_param watchdog_cfg_param = {0};
    ret = dsmi_get_pfc_storm_watchdog_param(logic_id, &watchdog_cfg_param);
    if (ret != 0) {
        roce_err("get pfc storm watchdog param failed when parse param. (ret=[%d]; logic_id=[%d])", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = memcpy_s(user_cfg_param, sizeof(struct pfc_storm_cfg_param), &watchdog_cfg_param,
                   sizeof(struct pfc_storm_cfg_param));
    if (ret != 0) {
        roce_err("%s memcpy_s failed. (ret=%d).", __func__, ret);
        return ret;
    }

    ret = tool_pfc_storm_watchdog_param_list_parse(argc, argv, user_cfg_param, param_of_tool);
    if (ret != 0) {
        roce_err("%s param list parse failed. (ret=%d)", __func__, ret);
        return ret;
    }

    ret = tool_check_pfc_storm_watchdog_param(user_cfg_param, &watchdog_cfg_param);
    if (ret != 0) {
        roce_err("param check failed. (ret = %d)", ret);
        return ret;
    }

    if (memcmp(user_cfg_param, &watchdog_cfg_param, sizeof(struct pfc_storm_cfg_param)) != 0) {
        *new_param_flag = PFC_STORM_CFG_CHANGE;
    }
    return UDA_EXE_SUCCESS;
}

STATIC int pfc_storm_watchdog_save_mode(char *inbuf, int length, struct pfc_storm_cfg_param *user_cfg_param)
{
    int ret;
    ret = sprintf_s(inbuf, length, "%u", user_cfg_param->mode);
    if (ret <= 0) {
        roce_err("Sprintf_s for save pfc storm watchdog param mode faild. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    return 0;
}

STATIC int pfc_storm_watchdog_save_close_time(char *inbuf, int length, struct pfc_storm_cfg_param *user_cfg_param)
{
    int ret;
    ret = sprintf_s(inbuf, length, "%u", user_cfg_param->close_time);
    if (ret <= 0) {
        roce_err("Sprintf_s for save pfc storm watchdog param close_time faild. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    return 0;
}

STATIC int pfc_storm_watchdog_save_detect_period(char *inbuf, int length, struct pfc_storm_cfg_param *user_cfg_param)
{
    int ret;
    ret = sprintf_s(inbuf, length, "%u", user_cfg_param->detect_period);
    if (ret <= 0) {
        roce_err("Sprintf_s for save pfc storm watchdog param detect_period faild. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    return 0;
}

STATIC int pfc_storm_watchdog_save_warn_thrd(char *inbuf, int length, struct pfc_storm_cfg_param *user_cfg_param)
{
    int ret;
    ret = sprintf_s(inbuf, length, "%u", user_cfg_param->warn_thrd);
    if (ret <= 0) {
        roce_err("Sprintf_s for save pfc storm watchdog param warn_thrd faild. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    return 0;
}

STATIC int pfc_storm_watchdog_save_err_thrd(char *inbuf, int length, struct pfc_storm_cfg_param *user_cfg_param)
{
    int ret;
    ret = sprintf_s(inbuf, length, "%u", user_cfg_param->err_thrd);
    if (ret <= 0) {
        roce_err("Sprintf_s for save pfc storm watchdog param err_thrd faild. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    return 0;
}

struct pfc_storm_save g_pfc_storm_save_list[] = {{"mode", pfc_storm_watchdog_save_mode},
                                                 {"close_time", pfc_storm_watchdog_save_close_time},
                                                 {"detect_period", pfc_storm_watchdog_save_detect_period},
                                                 {"warn_thrd", pfc_storm_watchdog_save_warn_thrd},
                                                 {"err_thrd", pfc_storm_watchdog_save_err_thrd}};

STATIC int tool_pfc_storm_watchdog_save_param(struct pfc_storm_cfg_param *user_cfg_param,
                                              struct tool_param *param_of_tool)
{
    int ret;
    char config_buf[MAX_PFC_STORM_STORM_PARAM_LEN] = {0};
    char param_name[MAX_PFC_STORM_STORM_PARAM_LEN] = {0};
    int save_list_len = sizeof(g_pfc_storm_save_list) / sizeof(g_pfc_storm_save_list[0]);

    for (int i = 0; i < save_list_len; ++i) {
        ret = sprintf_s(param_name, MAX_PFC_STORM_STORM_PARAM_LEN, "pfc_storm_watchdog_%s",
                        g_pfc_storm_save_list[i].param);
        if (ret <= 0) {
            roce_err("save pfc storm watchdog param %s sprintf_s failed. (ret=%d)", g_pfc_storm_save_list[i].param,
                     ret);
            return UDA_TOOL_NO_MEM_ERR;
        }

        ret = g_pfc_storm_save_list[i].save_proc(config_buf, MAX_PFC_STORM_STORM_PARAM_LEN, user_cfg_param);
        if (ret != 0) {
            roce_err("g_pfc_storm_save_list save %s func execution failed!", param_name);
            return ret;
        }

        ret = cfg_write_conf(param_of_tool->phy_id, param_name, config_buf);
        if (ret != 0) {
            roce_err("cfg_write_conf for %s failed. (ret=%d)", param_name, ret);
            return ret;
        }

        ret = memset_s(config_buf, MAX_PFC_STORM_STORM_PARAM_LEN, 0, MAX_PFC_STORM_STORM_PARAM_LEN);
        if (ret != 0) {
            roce_err("pfc storm watchdog save param memset_s for config_buf failed. (ret=%d)", ret);
            return ret;
        }

        ret = memset_s(param_name, MAX_PFC_STORM_STORM_PARAM_LEN, 0, MAX_PFC_STORM_STORM_PARAM_LEN);
        if (ret != 0) {
            roce_err("pfc storm watchdog save param memset_s for param_name failed. (ret=%d)", ret);
            return ret;
        }
    }
    return 0;
}

STATIC int tool_pfc_storm_watchdog_cfg(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id;
    struct pfc_storm_cfg_info cfg_info = {0};
    cfg_info.cfg_change_flag = PFC_STORM_CFG_NOT_CHANGE;
    logic_id = param->logic_id;

    if (argc <= 0) {
        roce_err("%s Err argc[%d]", __func__, argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_pfc_storm_watchdog_param_parse(argc, argv, logic_id, &cfg_info.user_cfg_param, param,
                                              &cfg_info.cfg_change_flag);
    if (ret != 0) {
        roce_err("%s Invalid param. (ret = %d)", __func__, ret);
        return ret;
    }

    ret = dsmi_set_pfc_storm_watchdog_param(logic_id, &cfg_info);
    if (ret != 0) {
        roce_err("dsmi set pfc storm watchdog param failed. (ret=[%d]; logic_id=[%d])", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = tool_pfc_storm_watchdog_save_param(&cfg_info.user_cfg_param, param);
    if (ret != 0) {
        roce_err("save pfc storm watchdog param failed. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return ret;
    }
    return UDA_EXE_SUCCESS;
}

STATIC void tool_pfc_storm_watchdog_print(struct pfc_storm_cfg_param *user_cfg_param)
{
    TOOL_PRINT_INFO("Config info:");
    switch (user_cfg_param->mode) {
        case PFC_STORM_WATCHDOG_MODE_0:
            TOOL_PRINT_INFO("mode: %u [Disable PFC storm detection]", user_cfg_param->mode);
            break;
        case PFC_STORM_WATCHDOG_MODE_1:
            TOOL_PRINT_INFO("mode: %u [Only warn and err counts are counted, and no closing action is performed]",
                            user_cfg_param->mode);
            break;
        case PFC_STORM_WATCHDOG_MODE_2:
            TOOL_PRINT_INFO("mode: %u [Enable PFC storm detection]", user_cfg_param->mode);
            break;
        default:
            break;
    }
    TOOL_PRINT_INFO("close_time: %u ms", user_cfg_param->close_time * PFC_100MS_TO_MS);
    TOOL_PRINT_INFO("detect_period: %.1f s", (float)user_cfg_param->detect_period / PFC_100MS_TO_S);
    TOOL_PRINT_INFO("warn_threshold: %u%%", user_cfg_param->warn_thrd);
    TOOL_PRINT_INFO("err_threshold: %u%%", user_cfg_param->err_thrd);
}

STATIC int tool_get_pfc_storm_whtchdog(int argc, char **argv, struct tool_param *param)
{
    int ret;
    if (argc != 1) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    struct pfc_storm_cfg_param user_cfg_param = {0};

    ret = dsmi_get_pfc_storm_watchdog_param(param->logic_id, &user_cfg_param);
    if (ret != 0) {
        roce_err("get pfc storm watchdog param failed from dsmi. (ret=[%d]; logic_id=[%d])", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    tool_pfc_storm_watchdog_print(&user_cfg_param);
    return UDA_EXE_SUCCESS;
}

int tool_pfc_storm_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id;
    int argc_tmp = argc - 1;

    if (param == NULL) {
        roce_err("Param is NULL.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    // 天工不支持
    if (param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID || param->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID_2) {
        roce_err("This device does not support pfc storm cmd.");
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }
    if (argc == 0) {
        roce_err("err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    logic_id = param->logic_id;

    if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        param->get_cmd_flag = false;
        LOG_SET_CONFIG("Start to set PFC storm watchdog param.(logic_id=%d)", param->logic_id);
        ret = tool_pfc_storm_watchdog_cfg(argc_tmp, argv + 1, param);
        LOG_SET_CONFIG("End to set PFC storm watchdog param.(logic_id=%d, result=%d)", param->logic_id, ret);
        if (ret != 0) {
            roce_err("set the cfg parameters of the pfc storm watchdog fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_pfc_storm_whtchdog(argc, argv, param);
        if (ret != 0) {
            roce_err("get the cfg parameters of the pfc storm watchdog fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else {
        roce_err("pfc_storm_cmd fail, invalid operation.\n");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

STATIC int pfc_storm_watchdog_recovery_get_saved_config(int *argc, char **argv, struct tool_param *param_of_tool)
{
    int ret;
    int i;
    char param_name_saved[MAX_PFC_STORM_STORM_PARAM_LEN];

    for (i = 0; i < MAX_PFC_STORM_WATCHDOG_PARAM_NUM; ++i) {
        ret = memset_s(param_name_saved, MAX_PFC_STORM_STORM_PARAM_LEN, 0, MAX_PFC_STORM_STORM_PARAM_LEN);
        if (ret != 0) {
            roce_err("%s memset_s for param_name_saved failed. (ret=%d)", __func__, ret);
            return ret;
        }

        ret = sprintf_s(param_name_saved, MAX_PFC_STORM_STORM_PARAM_LEN, "pfc_storm_watchdog_%s",
                        g_pfc_storm_parse_list[i].param);
        if (ret <= 0) {
            roce_err("%s sprintf_s for param %s failed. (ret=%d)", __func__, g_pfc_storm_parse_list[i].param, ret);
            return UDA_TOOL_NO_MEM_ERR;
        }

        ret = cfg_read_conf(param_of_tool->phy_id, param_name_saved, argv[TWO_VALUE * (*argc) + 1],
                            MAX_PFC_STORM_STORM_PARAM_LEN);
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
            roce_info("pfc storm watchdog do not need recover cfg %s. (ret=%d, phy_id=%d)",
                      g_pfc_storm_parse_list[i].param, ret, param_of_tool->phy_id);
            continue;
        } else if (ret != 0) {
            roce_err("%s cfg_read_conf for %s failed. (ret=%d)", __func__, param_name_saved, ret);
            return ret;
        }

        ret = memcpy_s(argv[TWO_VALUE * (*argc)], MAX_PFC_STORM_STORM_PARAM_LEN, g_pfc_storm_parse_list[i].param,
                       strlen(g_pfc_storm_parse_list[i].param) + 1);
        if (ret != 0) {
            roce_err("%s get param name failed. (ret=%d)", __func__, ret);
            return ret;
        }
        (*argc)++;
    }
    return 0;
}

int tool_downdelay_recovery(struct tool_param *param_of_tool)
{
    int ret;
    char str[CONLINE_LENGTH] = {0};
    char *argv = str;

    ret = cfg_read_conf(param_of_tool->phy_id, "down_fault_time", str, CONLINE_LENGTH);
    if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
        roce_info("down fault time do not need recover. (phy_id=%d)", param_of_tool->phy_id);
        return 0;
    } else if (ret) {
        roce_err("down fault time configuration check failed. (ret=%d)", ret);
        return ret;
    }

    return tool_link_set_downdelay(1, &argv, param_of_tool);
}

int tool_cfg_pfc_storm_watchdog_recovery(struct tool_param *param_of_tool)
{
    int ret, i, k;
    char *argv[MAX_PFC_STORM_WATCHDOG_PARAM_NUM * TWO_VALUE] = {NULL};
    int argc = 0;
    // 天工不支持
    if (param_of_tool->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID ||
        param_of_tool->mainboard_id == ATLAS_9000_A3_MAINBOARD_ID_2) {
        roce_info("This device does not support pfc storm cmd. No need to recovery");
        return 0;
    }

    for (i = 0; i < MAX_PFC_STORM_WATCHDOG_PARAM_NUM * TWO_VALUE; ++i) {
        argv[i] = (char *)malloc(MAX_PFC_STORM_STORM_PARAM_LEN * sizeof(char));
        if (argv[i] == NULL) {
            roce_err("%s malloc failed.", __func__);
            goto malloc_free;
        }
    }

    // 将config配置转为二维argv字符串数组
    ret = pfc_storm_watchdog_recovery_get_saved_config(&argc, argv, param_of_tool);
    if (ret != 0) {
        roce_err("%s get saved config failed. (ret=%d)", __func__, ret);
        goto malloc_free;
    }

    if (argc == 0) {
        roce_info("pfc storm watchdog do not need recover cfg. (ret=%d, phy_id=%d)", ret, param_of_tool->phy_id);
        goto malloc_free;
    }
    // argv有效的字符串数量是 argc*2：参数名 + 值
    ret = tool_pfc_storm_watchdog_cfg(TWO_VALUE * argc, argv, param_of_tool);
    if (ret != 0) {
        roce_err("recovery the cfg parameters of the pfc storm watchdog fail. (ret=%d; phy_id=%d)", ret,
                 param_of_tool->phy_id);
        goto malloc_free;
    }

malloc_free:
    for (k = 0; k < MAX_PFC_STORM_WATCHDOG_PARAM_NUM * TWO_VALUE; k++) {
        if (argv[k] != NULL) {
            free(argv[k]);
            argv[k] = NULL;
        }
    }
    return ret;
}