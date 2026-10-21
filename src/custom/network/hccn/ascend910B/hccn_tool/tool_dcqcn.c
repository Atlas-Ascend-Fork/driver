/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <time.h>
#include "tool.h"
#include "cmd.h"
#include "cfg.h"
#include "dsmi_common_interface.h"
#include "log.h"
#include "tool_ops.h"
#include "tool_recovery.h"
#include "tool_ext.h"
#include "ds_net_ext.h"
#include "ds_net.h"

static int tool_get_dcqcn_alg_info(int argc, char **argv, int logic_id)
{
    int ret;
    struct ds_dcqcn_info info = {0};
    unsigned char cfgdata;
    char *tmp_ptr = NULL;

    if (argc != TWO_VALUE) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "alg_mode", strlen("alg_mode") + 1)) {
        cfgdata = (unsigned char)strtoul(argv[1], &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == argv[1] || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if ((cfgdata != 0) && (cfgdata != 1)) { // alg_mode only support 0 or 1.
            roce_err("dcqcn_param alg_mode %u cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info.dcqcn_param.alg_mode = cfgdata;
    } else {
        roce_err("invalid param %s", argv[0]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    info.flag = 1; // get alg flag is 1.
    ret = dsmi_get_dcqcn_info(logic_id, &info);
    if (ret != 0) {
        roce_err("DSMI get the setting of dcqcn fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    TOOL_PRINT_INFO("f             : %u", info.dcqcn_param.f);
    TOOL_PRINT_INFO("g_shift       : %u", info.dcqcn_param.g);
    TOOL_PRINT_INFO("alpha_min     : %u", info.dcqcn_param.al);
    TOOL_PRINT_INFO("tkp_shift     : %u", info.dcqcn_param.tkp);
    // speed only support 100000 and 200000
    if (((info.dcqcn_param.max_speed) != 100000) && (info.dcqcn_param.max_speed != 200000)) {
        TOOL_PRINT_INFO("max_speed     : Unknow!");
    } else {
        TOOL_PRINT_INFO("max_speed     : %u", info.dcqcn_param.max_speed);
    }
    TOOL_PRINT_INFO("ai            : %u", info.dcqcn_param.ai);
    TOOL_PRINT_INFO("tmp_shift     : %u", info.dcqcn_param.tmp);
    TOOL_PRINT_INFO("alp           : %u", info.dcqcn_param.alp);
    TOOL_PRINT_INFO("cnp_time      : %u", info.dcqcn_param.cnp_time);
    TOOL_PRINT_INFO("alpha_shift   : %u", info.dcqcn_param.alp_shift);
    TOOL_PRINT_INFO("alg_mode      : %u", info.dcqcn_param.alg_mode);
    TOOL_PRINT_INFO("max_des_shift : %u", info.dcqcn_param.max_des_shift);

    return 0;
}

static int tool_get_dcqcn_enable_info(int logic_id)
{
    int ret;
    struct ds_dcqcn_info info = {0};

    info.flag = 2; // get dcqcn enable info flag is 2.
    ret = dsmi_get_dcqcn_info(logic_id, &info);
    if (ret != 0) {
        roce_err("DSMI get the setting of dcqcn fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    TOOL_PRINT_INFO("dcqcn enable status:   %s", (info.enable_param.enable == 1) ? "enable" : "disable");
    TOOL_PRINT_INFO("dcqcn enable alg_mode: %u", info.enable_param.alg_mode1);

    return 0;
}

static int tool_get_dcqcn_cnp_dscp(int logic_id)
{
    int ret;
    struct ds_dcqcn_info info = {0};

    info.flag = 3; // get dcqcn cnp_dscp info flag is 3.
    ret = dsmi_get_dcqcn_info(logic_id, &info);
    if (ret != 0) {
        roce_err("DSMI get the setting of dcqcn fail. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    if (info.cnp_dscp == 0xFF) {
        TOOL_PRINT_INFO("The cnp_dscp parameter is abnormal.");
        return 0;
    }
    TOOL_PRINT_INFO("cnp_dscp     : %u", info.cnp_dscp);

    return 0;
}

static int dcqcn_param_set_enable(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned char cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned char)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if ((cfgdata != 0) && (cfgdata != 1)) {
            roce_err("dcqcn_param enable %lu cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->enable_param.enable = cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->enable_param.enable);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_alg_mode1(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned char cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned char)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if ((cfgdata != 0) && (cfgdata != 1)) { // alg_mode only support 0 or 1.
            roce_err("dcqcn_param cnp_dscp %u cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }

        info->enable_param.alg_mode1 = cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->enable_param.alg_mode1);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_alg_mode(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned char cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned char)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if ((cfgdata != 0) && (cfgdata != 1)) { // alg_mode only support 0 or 1.
            roce_err("dcqcn_param cnp_dscp %u cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }

        info->dcqcn_param.alg_mode = cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.alg_mode);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_iteration_count(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned int cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned int)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if (cfgdata > 255) { // max dcqcn_param f is 255.
            roce_err("dcqcn_param f %lu cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->dcqcn_param.f = (unsigned char)cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.f);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_update_g(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned char cfgdata;
    char *tmp_ptr = NULL;
    int ret;
    if (para == NULL) {
        cfgdata = (unsigned char)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if (cfgdata > 15) { // max dcqcn_param g is 15.
            roce_err("dcqcn_param g %lu cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->dcqcn_param.g = cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.g);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_alpha_min(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned int cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned int)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if (cfgdata > 255) { // max dcqcn_param al is 255.
            roce_err("dcqcn_param al %lu cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->dcqcn_param.al = (unsigned char)cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.al);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_update_token(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned char cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned char)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if (cfgdata > 15) { // max dcqcn_param tkp is 15.
            roce_err("dcqcn_param tkp %lu cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->dcqcn_param.tkp = cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.tkp);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_max_speed(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned int cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned int)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if ((cfgdata != 100000) && (cfgdata != 200000)) { // dcqcn_param max_speed only support 100000 and 200000
            roce_err("dcqcn_param max_speed %lu cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->dcqcn_param.max_speed = cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.max_speed);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_fixed_step(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned int cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned int)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if (cfgdata > 65535) { // max dcqcn_param ai is 65535.
            roce_err("dcqcn_param ai %lu cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->dcqcn_param.ai = (unsigned short)cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.ai);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_update_timer(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned short cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned short)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if (cfgdata > 15) { // max dcqcn_param tmp is 15.
            roce_err("dcqcn_param tmp %lu cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->dcqcn_param.tmp = cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.tmp);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_update_alpha(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned int cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned int)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if (cfgdata > 65535) { // max dcqcn_param alp is 65535.
            roce_err("dcqcn_param alp %lu cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->dcqcn_param.alp = (unsigned short)cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.alp);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_cnp_time(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned int cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned int)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if (cfgdata > 255) { // max dcqcn_param cnp_time is 255.
            roce_err("dcqcn_param cnp_time %u cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->dcqcn_param.cnp_time = (unsigned char)cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.cnp_time);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_alp_shift(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned char cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned char)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if (cfgdata > 15) { // max dcqcn_param alp_shift is 15.
            roce_err("dcqcn_param alp_shift %u cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->dcqcn_param.alp_shift = cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.alp_shift);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

static int dcqcn_param_max_des_shift(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned char cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned char)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("DCQCN para max_des_shift val must be unsigned int. (cur=%u; tail=%s)", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if (cfgdata > 9) { // max dcqcn_param max_des_shift is 9.
            roce_err("DCQCN param max_des_shift cfg trans failed. (cfgdata=%u)", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }
        info->dcqcn_param.max_des_shift = cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->dcqcn_param.max_des_shift);
        if (ret <= 0) {
            roce_err("DCQCN para max_des_shift val cmd sprintf inbuf failed. (ret=%d)", ret);
            return UDA_TOOL_NO_MEM_ERR;
        }
    }
    return 0;
}

static int dcqcn_param_cnp_dsc(struct ds_dcqcn_info *info, char *val, char *para, unsigned int length)
{
    unsigned char cfgdata;
    char *tmp_ptr = NULL;
    int ret;

    if (para == NULL) {
        cfgdata = (unsigned char)strtoul(val, &tmp_ptr, STR_NUM_BASE);
        if (tmp_ptr == val || *tmp_ptr != '\0') {
            roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
            return UDA_PARAM_INVALID_ERR;
        }

        if (cfgdata > 63) { // max dcqcn_param cnp_dscp is 63.
            roce_err("dcqcn_param cnp_dscp %u cfg trans failed.", cfgdata);
            return UDA_PARAM_INVALID_ERR;
        }

        info->cnp_dscp = cfgdata;
    } else {
        ret = sprintf_s(para, length, "%u", info->cnp_dscp);
        if (ret <= 0) {
            roce_err("alg para val cmd sprintf inbuf failed!cnp_dscp is %u.", info->cnp_dscp);
            return UDA_TOOL_NO_MEM_ERR;
        }
    }

    return 0;
}

struct dcqcn_param_info dcqcn_cnp_dscp[] = {
    {"cnp_dscp", dcqcn_param_cnp_dsc},
};

struct dcqcn_param_info dcqcn_param_enable[] = {
    {"enable", dcqcn_param_set_enable},
    {"alg_mode", dcqcn_param_alg_mode1},
};

struct dcqcn_param_info dcqcn_param_list[] = {
    {"alg_mode", dcqcn_param_alg_mode},   {"f", dcqcn_param_iteration_count},
    {"g_shift", dcqcn_param_update_g},    {"al", dcqcn_param_alpha_min},
    {"tkp", dcqcn_param_update_token},    {"max_speed", dcqcn_param_max_speed},
    {"ai", dcqcn_param_fixed_step},       {"tmp", dcqcn_param_update_timer},
    {"alp", dcqcn_param_update_alpha},    {"cnp_time", dcqcn_param_cnp_time},
    {"alp_shift", dcqcn_param_alp_shift}, {"max_des_shift", dcqcn_param_max_des_shift},
};

static int tool_write_alg_pare_conf(struct ds_dcqcn_info *info, struct tool_param *param)
{
    int ret;
    char inbuf[MAX_DCQCN_PARAM_LEN] = {0};
    char para_name[MAX_DCQCN_PARAM_LEN] = {0};
    char *val = NULL;

    for (int i = 0; i < (sizeof(dcqcn_param_list) / sizeof(dcqcn_param_list[0])); i++) {
        ret = sprintf_s(para_name, MAX_DCQCN_PARAM_LEN, "dcqcn_%s", dcqcn_param_list[i].param);
        if (ret <= 0) {
            roce_err("alg para_name cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
        ret = dcqcn_param_list[i].param_parse_func(info, val, inbuf, MAX_DCQCN_PARAM_LEN);
        if (ret != 0) {
            roce_err("dcqcn_param_list parse_func execution failed!");
            return ret;
        }
        ret = cfg_write_conf(param->phy_id, para_name, inbuf);
        if (ret) {
            roce_err("add dcqcn cfg error! ret[%d] logic_id[%d]", ret, param->logic_id);
            return ret;
        }
        (void)memset_s(inbuf, MAX_DCQCN_PARAM_LEN, 0, MAX_DCQCN_PARAM_LEN);
        (void)memset_s(para_name, MAX_DCQCN_PARAM_LEN, 0, MAX_DCQCN_PARAM_LEN);
    }

    return 0;
}

static int tool_write_enable_pare_conf(struct ds_dcqcn_info *info, struct tool_param *param)
{
    int ret;
    char inbuf[MAX_DCQCN_PARAM_LEN] = {0};
    char para_name[MAX_DCQCN_PARAM_LEN] = {0};
    char *val = NULL;

    for (int i = 0; i < (sizeof(dcqcn_param_enable) / sizeof(dcqcn_param_enable[0])); i++) {
        ret = sprintf_s(para_name, MAX_DCQCN_PARAM_LEN, "dcqcn_en_%s", dcqcn_param_enable[i].param);
        if (ret <= 0) {
            roce_err("alg para_name cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
        ret = dcqcn_param_enable[i].param_parse_func(info, val, inbuf, MAX_DCQCN_PARAM_LEN);
        if (ret != 0) {
            roce_err("dcqcn_param_list parse_func execution failed!");
            return ret;
        }
        ret = cfg_write_conf(param->phy_id, para_name, inbuf);
        if (ret) {
            roce_err("add dcqcn cfg error! ret[%d] logic_id[%d]", ret, param->logic_id);
            return ret;
        }
        (void)memset_s(inbuf, MAX_DCQCN_PARAM_LEN, 0, MAX_DCQCN_PARAM_LEN);
        (void)memset_s(para_name, MAX_DCQCN_PARAM_LEN, 0, MAX_DCQCN_PARAM_LEN);
    }

    return 0;
}

static int tool_write_cnp_dscp_pare_conf(struct ds_dcqcn_info *info, struct tool_param *param)
{
    int ret;
    char inbuf[MAX_DCQCN_PARAM_LEN] = {0};
    char para_name[MAX_DCQCN_PARAM_LEN] = {0};
    char *val = NULL;

    for (int i = 0; i < (sizeof(dcqcn_cnp_dscp) / sizeof(dcqcn_cnp_dscp[0])); i++) {
        ret = sprintf_s(para_name, MAX_DCQCN_PARAM_LEN, "dcqcn_%s", dcqcn_cnp_dscp[i].param);
        if (ret <= 0) {
            roce_err("alg para_name cmd sprintf inbuf failed!");
            return UDA_TOOL_NO_MEM_ERR;
        }
        ret = dcqcn_cnp_dscp[i].param_parse_func(info, val, inbuf, MAX_DCQCN_PARAM_LEN);
        if (ret != 0) {
            roce_err("dcqcn_param_list parse_func execution failed!");
            return ret;
        }
        ret = cfg_write_conf(param->phy_id, para_name, inbuf);
        if (ret) {
            roce_err("add dcqcn cfg error! ret[%d] logic_id[%d]", ret, param->logic_id);
            return ret;
        }
        (void)memset_s(inbuf, MAX_DCQCN_PARAM_LEN, 0, MAX_DCQCN_PARAM_LEN);
        (void)memset_s(para_name, MAX_DCQCN_PARAM_LEN, 0, MAX_DCQCN_PARAM_LEN);
    }

    return 0;
}

static int check_dcqcn_param_data(int argc, char **argv, struct tool_param *param, struct ds_dcqcn_info *info)
{
    char *inbuf = NULL;
    int ret, logic_id;

    if (argc < TWO_VALUE || argc > MAX_PARAM_CNT || argc % TWO_VALUE != 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dcqcn_param_list[0].param_parse_func(info, argv[1], inbuf, 0);
    if (ret != 0) {
        roce_err("dcqcn_param_alg mode parse_func execution failed!");
        return ret;
    }
    logic_id = param->logic_id;
    info->flag = 1; // 1代表alg_param
    ret = dsmi_get_dcqcn_info(logic_id, info);
    if (ret != 0) {
        roce_err("DSMI get the setting of dcqcn fail in set dcqcn info. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return 0;
}

int tool_set_dcqcn_alg_info(int argc, char **argv, struct tool_param *param, struct ds_dcqcn_info *info)
{
    char val[MAX_DCQCN_PARAM_LEN] = {0};
    char *inbuf = NULL;
    int ret, i;
    int cnt = 0;
    char flag[12] = {0}; // Indicates the subcommand settings, total num is 12

    ret = check_dcqcn_param_data(argc, argv, param, info);
    if (ret != 0) {
        roce_err("check_dcqcn_param_data execution failed! ret[%d]", ret);
        return ret;
    }

    while (cnt < argc) {
        for (i = 0; i < (sizeof(dcqcn_param_list) / sizeof(dcqcn_param_list[0])); i++) {
            if ((strncmp(argv[cnt], dcqcn_param_list[i].param, strlen(dcqcn_param_list[i].param) + 1) != 0) ||
                (strlen(argv[cnt]) != strlen(dcqcn_param_list[i].param))) {
                continue;
            }
            if (flag[i] != 0) {
                roce_err("Invalid operation.");
                return UDA_PARAM_OP_NOT_SUPPORT_ERR;
            }
            ret = memcpy_s(val, MAX_DCQCN_PARAM_LEN, argv[cnt + 1], strlen(argv[cnt + 1]));
            if (ret != 0) {
                roce_err("Get param copy failed. ret[%d] max_len[%d] real_len[%d]", ret, MAX_DCQCN_PARAM_LEN,
                         strlen(argv[cnt + 1]));
                return UDA_PARAM_INVALID_ERR;
            }

            ret = dcqcn_param_list[i].param_parse_func(info, val, inbuf, 0);
            if (ret != 0) {
                roce_err("dcqcn_param_list parse_func execution failed! ret[%d]", ret);
                return ret;
            }
            (void)memset_s(val, MAX_DCQCN_PARAM_LEN, 0, MAX_DCQCN_PARAM_LEN);
            flag[i] = 1;
            break;
        }
        if (i == (sizeof(dcqcn_param_list) / sizeof(dcqcn_param_list[0]))) {
            roce_err("Invalid operation.");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
        cnt = cnt + TWO_VALUE;
    }

    ret = dsmi_set_dcqcn_info(param->logic_id, info);
    if (ret != 0) {
        roce_err("DSMI set the setting of dcqcn fail in set dcqcn info. (ret=%d; dev_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return 0;
}

int tool_set_dcqcn_enable(int argc, char **argv, struct tool_param *param, struct ds_dcqcn_info *info)
{
    char *inbuf = NULL;
    int logic_id;
    int ret;

    if (argc != FOUR_VALUE) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    logic_id = param->logic_id;

    ret = dcqcn_param_enable[0].param_parse_func(info, argv[1], inbuf, 0);
    if (ret != 0) {
        roce_err("dcqcn_param_enable mode parse_func execution failed!");
        return ret;
    }

    if (!strncmp(argv[TWO_VALUE], "alg_mode", strlen("alg_mode") + 1)) {
        ret = dcqcn_param_enable[1].param_parse_func(info, argv[THREE_VALUE], inbuf, 0);
        if (ret != 0) {
            roce_err("dcqcn_param_alg mode parse_func execution failed!");
            return UDA_PARAM_INVALID_ERR;
        }
    } else {
        roce_err("invalid param %s", argv[TWO_VALUE]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    info->flag = 2; // set enable flag is 2.

    ret = dsmi_set_dcqcn_info(logic_id, info);
    if (ret != 0) {
        roce_err("DSMI set the setting of dcqcn fail in set dcqcn info. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return 0;
}

int tool_set_dcqcn_cnp_dscp(int argc, char **argv, struct tool_param *param, struct ds_dcqcn_info *info)
{
    char *tmp_ptr = NULL;
    unsigned char cfgdata;
    int logic_id;
    int ret;

    if (argc != TWO_VALUE) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    logic_id = param->logic_id;

    cfgdata = (unsigned char)strtoul(argv[1], &tmp_ptr, STR_NUM_BASE);
    if (tmp_ptr == argv[1] || *tmp_ptr != '\0') {
        roce_err("alg para val must be unsigned int, now is %u, tail is %s", cfgdata, tmp_ptr);
        return UDA_PARAM_INVALID_ERR;
    }

    if (cfgdata > 63) { // max dcqcn_param cnp_dscp is 63.
        roce_err("dcqcn_param cnp_dscp %u cfg trans failed.", cfgdata);
        return UDA_PARAM_INVALID_ERR;
    }
    info->flag = 3; // cnp_dscp flag is 3.
    info->cnp_dscp = cfgdata;

    ret = dsmi_set_dcqcn_info(logic_id, info);
    if (ret != 0) {
        roce_err("DSMI set the setting of dcqcn fail in set dcqcn info. (ret=%d; dev_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return 0;
}

static int tool_get_dcqcn_info(int argc, char **argv, int logic_id)
{
    int ret;
    int argc_tmp = argc - 1;

    if (argc == 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "status", strlen("status") + 1)) {
        if (argc_tmp > 0) {
            roce_err("Err argc. (argc=%d)", argc_tmp);
            return UDA_PARAM_CMD_COUNT_ERR;
        }
        ret = tool_get_dcqcn_enable_info(logic_id);
        if (ret != 0) {
            roce_err("Get the setting of dcqcn fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "cnp_dscp", strlen("cnp_dscp") + 1)) {
        if (argc_tmp > 0) {
            roce_err("Err argc. (argc=%d)", argc_tmp);
            return UDA_PARAM_CMD_COUNT_ERR;
        }
        ret = tool_get_dcqcn_cnp_dscp(logic_id);
        if (ret != 0) {
            roce_err("Get the setting of dcqcn fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "param", strlen("param") + 1)) {
        ret = tool_get_dcqcn_alg_info(argc_tmp, argv + 1, logic_id);
        if (ret != 0) {
            roce_err("Get the setting of dcqcn fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else {
        roce_err("invalid param %s", argv[0]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return ret;
}

static int tool_set_dcqcn_info(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct ds_dcqcn_info info = {0};

    if (argc == 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "alg_mode", strlen("alg_mode") + 1)) {
        ret = tool_set_dcqcn_alg_info(argc, argv, param, &info);
        if (ret != 0) {
            roce_err("Set the setting of dcqcn alg info fail. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
        ret = tool_write_alg_pare_conf(&info, param);
        if (ret != 0) {
            roce_err("tool write alg pare conf fail!");
            return ret;
        }
    } else if (!strncmp(argv[0], "enable", strlen("enable") + 1)) {
        ret = tool_set_dcqcn_enable(argc, argv, param, &info);
        if (ret != 0) {
            roce_err("Set the setting of dcqcn enable fail. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
        ret = tool_write_enable_pare_conf(&info, param);
        if (ret != 0) {
            roce_err("tool write alg pare conf fail!");
            return ret;
        }
    } else if (!strncmp(argv[0], "cnp_dscp", strlen("cnp_dscp") + 1)) {
        ret = tool_set_dcqcn_cnp_dscp(argc, argv, param, &info);
        if (ret != 0) {
            roce_err("Set the setting of dcqcn cnp_dscp fail. (ret=%d; logic_id=%d)", ret, param->logic_id);
            return ret;
        }
        ret = tool_write_cnp_dscp_pare_conf(&info, param);
        if (ret != 0) {
            roce_err("tool write alg pare conf fail!");
            return ret;
        }
    } else {
        roce_err("invalid param %s", argv[0]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}

int tool_dcqcn_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int argc_tmp = argc - 1;
    int logic_id;
    int ret;

    if (argc == 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_HALF_CMD_ERR;
    }

    logic_id = param->logic_id;
    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_dcqcn_info(argc_tmp, argv + 1, logic_id);
        if (ret != 0) {
            roce_err("Get the setting of dcqcn fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        ret = tool_set_dcqcn_info(argc_tmp, argv + 1, param);
        if (ret != 0) {
            roce_err("Set the setting of dcqcn fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else {
        roce_err("Invalid operation.\n)");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return ret;
}

static int get_and_set_dcqcn_param(int *argc, char **argv, struct tool_param *param, char *para_name, char **para)
{
    int i, ret;

    for (i = 0; i < MAX_DCQCN_PARA_NUM; i++) {
        ret = sprintf_s(para_name, MAX_DCQCN_PARAM_LEN, "dcqcn_%s", para[i]);
        if (ret <= 0) {
            roce_err("alg para_name cmd sprintf inbuf failed! ret[%d]", ret);
            return UDA_PARAM_INVALID_ERR;
        }

        ret = cfg_read_conf(param->phy_id, para_name, argv[(TWO_VALUE * (*argc) + 1)], MAX_DCQCN_PARAM_LEN);
        if (ret) {
            if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
                roce_info("dcqcn alg do not need recover cfg [%d], phy_id:[%d]", ret, param->phy_id);
                continue;
            } else {
                roce_err("cfg_read_conf %s failed! ret[%d]", para_name, ret);
                return UDA_PARAM_INVALID_ERR;
            }
        } else {
            (void)memset_s(para_name, MAX_DCQCN_PARAM_LEN, 0, MAX_DCQCN_PARAM_LEN);
            ret = memcpy_s(argv[TWO_VALUE * (*argc)], MAX_DCQCN_PARAM_LEN, para[i], (strlen(para[i]) + 1));
            if (ret) {
                roce_err("memcpy error [%d]", ret);
                return UDA_PARAM_INVALID_ERR;
            }
            (*argc)++;
        }
    }

    return 0;
}

static int check_alg_mode_conf(struct tool_param *param, char **argv, int *argc)
{
    int ret;

    ret = cfg_read_conf(param->phy_id, "dcqcn_alg_mode", argv[1], MAX_DCQCN_PARAM_LEN);
    if (ret) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
            roce_info("dcqcn alg do not need recover cfg [%d], phy_id:[%d]", ret, param->phy_id);
            return -1;
        } else {
            roce_err("cfg_read_conf dcqcn_alg_mode failed! ret[%d]", ret);
            return UDA_PARAM_INVALID_ERR;
        }
    } else {
        ret = memcpy_s(argv[0], MAX_DCQCN_PARAM_LEN, "alg_mode", (strlen("alg_mode") + 1));
        if (ret) {
            roce_err("memcpy error [%d]", ret);
            return UDA_PARAM_INVALID_ERR;
        }
        (*argc)++;
    }

    return 0;
}

int tool_cfg_dcqcn_alg_recovery(struct tool_param *param)
{
    int ret = -1;
    int argc = 0;
    int j, k;
    char para_name[MAX_DCQCN_PARAM_LEN] = "";
    char *argv[DCQCN_ARGV_PARAM_NUM];
    struct ds_dcqcn_info info = {0};
    char *para[MAX_DCQCN_PARA_NUM] = {"f",   "g_shift", "al",       "tkp",       "max_speed",    "ai",
                                      "tmp", "alp",     "cnp_time", "alp_shift", "max_des_shift"};

    for (j = 0; j < DCQCN_ARGV_PARAM_NUM; j++) {
        argv[j] = malloc(MAX_DCQCN_PARAM_LEN * sizeof(char));
        if (argv[j] == NULL) {
            roce_err("malloc failed!");
            goto malloc_free;
        }
    }
    ret = check_alg_mode_conf(param, argv, &argc);
    if (ret == -1) {
        goto malloc_free0;
    } else if (ret == UDA_PARAM_INVALID_ERR) {
        goto malloc_free;
    }

    ret = get_and_set_dcqcn_param(&argc, argv, param, para_name, para);
    if (ret != 0) {
        goto malloc_free;
    }

    if (argc == 1) {
        roce_info("dcqcn alg do not need recover cfg [%d], phy_id:[%d]", ret, param->phy_id);
        goto malloc_free0;
    }
    ret = tool_set_dcqcn_alg_info(TWO_VALUE * argc, argv, param, &info);
    if (ret != 0) {
        roce_err("tool_set_dcqcn_info execution failed!");
        goto malloc_free;
    }

malloc_free0:
    for (k = 0; k < j; k++) {
        free(argv[k]);
    }
    return 0;

malloc_free:
    for (k = 0; k < j; k++) {
        free(argv[k]);
    }
    return ret;
}

static int check_alg_enable_mode_conf(struct tool_param *param, char **argv)
{
    int ret;

    ret = cfg_read_conf(param->phy_id, "dcqcn_en_enable", argv[1], MAX_DCQCN_PARAM_LEN);
    if (ret) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
            roce_info("dcqcn alg do not need recover cfg [%d], phy_id:[%d]", ret, param->phy_id);
            return -1;
        } else {
            roce_err("cfg_read_conf dcqcn_alg_mode failed! ret[%d]", ret);
            return UDA_PARAM_INVALID_ERR;
        }
    } else {
        ret = memcpy_s(argv[0], MAX_DCQCN_PARAM_LEN, "enable", (strlen("enable") + 1));
        if (ret) {
            roce_err("memcpy error [%d]", ret);
            return UDA_PARAM_INVALID_ERR;
        }
    }

    ret = cfg_read_conf(param->phy_id, "dcqcn_en_alg_mode", argv[THREE_VALUE], MAX_DCQCN_PARAM_LEN);
    if (ret) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
            roce_info("dcqcn alg do not need recover cfg [%d], phy_id:[%d]", ret, param->phy_id);
            return -1;
        } else {
            roce_err("cfg_read_conf dcqcn_alg_mode failed! ret[%d]", ret);
            return UDA_PARAM_INVALID_ERR;
        }
    } else {
        ret = memcpy_s(argv[TWO_VALUE], MAX_DCQCN_PARAM_LEN, "alg_mode", (strlen("alg_mode") + 1));
        if (ret) {
            roce_err("memcpy error [%d]", ret);
            return UDA_PARAM_INVALID_ERR;
        }
    }

    return 0;
}

int tool_cfg_dcqcn_enable_recovery(struct tool_param *param)
{
    int ret;
    int j, k;
    char *argv[ARGV_NUM_4];
    struct ds_dcqcn_info info = {0};

    for (j = 0; j < ARGV_NUM_4; j++) {
        argv[j] = malloc(MAX_DCQCN_PARAM_LEN * sizeof(char));
        if (argv[j] == NULL) {
            roce_err("malloc failed!");
            ret = -1;
            goto malloc_free;
        }
    }
    ret = check_alg_enable_mode_conf(param, argv);
    if (ret == -1) {
        goto malloc_free0;
    } else if (ret == UDA_PARAM_INVALID_ERR) {
        goto malloc_free;
    }

    ret = tool_set_dcqcn_enable(ARGV_NUM_4, argv, param, &info);
    if (ret != 0) {
        roce_err("tool_set_dcqcn_info execution failed!");
        goto malloc_free;
    }

malloc_free0:
    for (k = 0; k < j; k++) {
        free(argv[k]);
    }
    return 0;

malloc_free:
    for (k = 0; k < j; k++) {
        free(argv[k]);
    }
    return ret;
}

int tool_cfg_dcqcn_cnp_dscp_recovery(struct tool_param *param)
{
    int ret;
    char outbuf[MAX_DCQCN_PARAM_LEN];
    char *argv[ARGV_NUM_2];
    struct ds_dcqcn_info info = {0};

    ret = cfg_read_conf(param->phy_id, "dcqcn_cnp_dscp", outbuf, MAX_DCQCN_PARAM_LEN);
    if (ret) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) {
            roce_info("ip address do not need recover cfg [%d], phy_id:[%d]", ret, param->phy_id);
            return 0;
        } else {
            roce_err("cfg_read_conf dcqcn_alg_mode failed! ret[%d]", ret);
            return ret;
        }
    } else {
        argv[0] = "dcqcn_cnp_dscp";
        argv[1] = outbuf;

        if (ret != 0) {
            roce_err("dcqcn_param_list parse_func execution failed! ret[%d]", ret);
            return ret;
        }
        ret = tool_set_dcqcn_cnp_dscp(ARGV_NUM_2, argv, param, &info);
        if (ret != 0) {
            roce_err("tool_set_dcqcn_info execution failed! ret[%d]", ret);
            return ret;
        }
    }

    return ret;
}
