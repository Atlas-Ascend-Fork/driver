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
#include <ctype.h>
#include "securec.h"
#include "ascend_hal.h"
#include "ds_net_interface.h"
#include "file_opt.h"
#include "hccn_err.h"
#include "ds_data.h"
#include "tool_cmd_err.h"
#include "dev_info_data.h"
#include "ubase_cmd_data.h"
#include "tool_dev_info.h"
#include "tool_bw_ops.h"
#include "tool_cfg_ops.h"
#include "tool_ubase_ops.h"

STATIC void tool_display_credit_info(struct credit_info *data)
{
    unsigned int vl = 0;

    TOOL_PRINT_INFO("link_alloc_port_share_credit:%u", data->link_alloc_port_share_credit);
    TOOL_PRINT_INFO("link_cur_used_port_share_credit:%u", data->link_cur_used_port_share_credit);

    for (vl = 0; vl < VL_NUM_PER_PORT; vl++) {
        TOOL_PRINT_INFO("link_alloc_vl_pri_credit(%u):%u", vl, data->link_alloc_vl_pri_credit[vl]);
        TOOL_PRINT_INFO("link_cur_used_pri_credit(%u):%u", vl, data->link_cur_used_pri_credit[vl]);
    }
}

STATIC int tool_credit_exec(struct command_context *ctx)
{
    unsigned int size = sizeof(struct credit_info);
    struct credit_info data = {0};
    struct dsmi_info info = {0};
    int ret;

    ctx->result = (char *)&data;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_CREDIT_INFO, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get credit info failed, ret:%d", ret), ret);

    tool_display_credit_info(&data);

    return 0;
}

STATIC void tool_credit_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_credit_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-credit", "query UB credit info");
            tool_print_npu_info(DEV_PORT_TYPE);
            break;
        default:
            break;
    }
}

STATIC int tool_upgrade_lane_exec(struct command_context *ctx)
{
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret;

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    ret = tool_double_check("Running this command will attempt to upgrade the lane, but success is not guaranteed.\n"
                            "Are you sure you want to continue?");
    CHK_PRT_RETURN(ret != 0, hccn_info("User aborts command"), ret);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, NULL, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_UPGRADE_LANE, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi upgrade lane failed, ret:%d", ret), ret);

    return 0;
}

STATIC void tool_upgrade_lane_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_upgrade_lane_print_help(ctx, MODE_SET);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-upgrade_lane", "try to upgrade lane, but does not guarantee success");
            tool_print_npu_info(DEV_PORT_TYPE);
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry get_cur_tc_buf_param[] = {
    {"-tc", ARG_TYPE_INT, parse_int, MODE_GET, MODE_GET, true, .range.int_range = {0, 7}}, // used TC num:8
};

STATIC void get_cur_tc_buf_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = get_cur_tc_buf_param;
    *count = ARRAY_SIZE_COMPUTE(get_cur_tc_buf_param);
}

STATIC void *alloc_cur_tc_buf_init(void)
{
    return calloc(1, sizeof(struct cur_tc_buf_info));
}

STATIC int cur_tc_buf_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct cur_tc_buf_info *cur_tc_buf_args = (struct cur_tc_buf_info *)args;

    if (strcmp(opt->name, "-tc") == 0) {
        cur_tc_buf_args->tc_id = (unsigned int)type_val->int_val;
    }

    return 0;
}

STATIC int tool_cur_tc_buf_exec(struct command_context *ctx)
{
    struct cur_tc_buf_info *cur_tc_buf_args = (struct cur_tc_buf_info *)ctx->cmd_args;
    struct cur_tc_buf_result result[DEV_INFO_DEFAULT_BOND_SLAVE_NUM] = {0};
    unsigned int size_out = sizeof(result);
    struct dsmi_info info = {0};
    unsigned int i = 0;
    int ret;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)cur_tc_buf_args,
                   sizeof(struct cur_tc_buf_info), (char *)result, &size_out);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_CUR_TC_BUF, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get cur_tc_buf failed, ret:%d", ret), ret);

    TOOL_PRINT_INFO("Current TC used buffer: ");
    for (i = 0; i < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; i++) {
        TOOL_PRINT_INFO("%s: %u", result[i].slave_name, result[i].info.max_tc_rx_buf);
    }

    return 0;
}

STATIC void tool_cur_tc_buf_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_cur_tc_buf_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-cur_tc_buf", "get the maximum tc buffer currently");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-tc <tc_id>", "specify the tc id, range [0, 7]");
            break;
        default:
            break;
    }
}

STATIC int tc_append_formatted_string(char *target_string, unsigned int max_target_len, unsigned int value)
{
#define TMP_LEN 10
#define FORMAT_WIDTH 7
#define FORMAT_ITEM "%-7u"
    char tmp_string[TMP_LEN] = {0};
    int ret = 0;

    ret = snprintf_s(tmp_string, TMP_LEN, FORMAT_WIDTH, FORMAT_ITEM, value);
    CHK_PRT_RETURN(ret <= 0, hccn_err("snprintf_s %d to tmp_string failed, ret:%d", value, ret), -EIO);

    ret = strncat_s(target_string, max_target_len, tmp_string, strlen(tmp_string) + 1);
    CHK_PRT_RETURN(ret != 0, hccn_err("strncat_s %s failed, ret:%d", tmp_string, ret), ret);

    return 0;
}

STATIC int tool_display_tc_cfg_info(struct tc_cfg_result *result)
{
#define ROW_LEN 100
    char high_waterline_string[ROW_LEN] = {0};
    char low_waterline_string[ROW_LEN] = {0};
    char buff_string[ROW_LEN] = {0};
    char id_string[ROW_LEN] = {0};
    unsigned int i, j = 0;
    int ret = 0;

    TOOL_PRINT_INFO("TC configuration:");
    for (j = 0; j < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; j++) {
        TOOL_PRINT_INFO("%s:", result[j].slave_name);

        for (i = 0; i < REAL_USED_TC_NUM; i++) {
            ret = tc_append_formatted_string(id_string, ROW_LEN, i);
            CHK_PRT_RETURN(ret != 0, hccn_err("tc_append_formatted_string for tc_id failed, ret:%d", ret), ret);

            ret = tc_append_formatted_string(buff_string, ROW_LEN, result[j].info.tc_buffer_size[i]);
            CHK_PRT_RETURN(ret != 0, hccn_err("tc_append_formatted_string for tc_buff failed, ret:%d", ret), ret);

            ret = tc_append_formatted_string(high_waterline_string, ROW_LEN, result[j].info.high_waterline[i]);
            CHK_PRT_RETURN(ret != 0, hccn_err("tc_append_formatted_string for h_waterline failed, ret:%d", ret), ret);

            ret = tc_append_formatted_string(low_waterline_string, ROW_LEN, result[j].info.low_waterline[i]);
            CHK_PRT_RETURN(ret != 0, hccn_err("tc_append_formatted_string for l_waterline failed, ret:%d", ret), ret);
        }

        TOOL_PRINT_INFO("\tTC id          %s", id_string);
        TOOL_PRINT_INFO("\tBuffer         %s", buff_string);
        TOOL_PRINT_INFO("\tUp waterline   %s", high_waterline_string);
        TOOL_PRINT_INFO("\tLow waterline  %s", low_waterline_string);

        (void)memset_s(id_string, ROW_LEN, 0, ROW_LEN);
        (void)memset_s(buff_string, ROW_LEN, 0, ROW_LEN);
        (void)memset_s(high_waterline_string, ROW_LEN, 0, ROW_LEN);
        (void)memset_s(low_waterline_string, ROW_LEN, 0, ROW_LEN);
    }

    return 0;
}

STATIC int tool_get_tc_cfg_exec(struct command_context *ctx)
{
    struct tc_cfg_result result[DEV_INFO_DEFAULT_BOND_SLAVE_NUM] = {0};
    unsigned int size_out = sizeof(result);
    struct dsmi_info info = {0};
    int ret;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, (char *)result, &size_out);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_TC_CFG_INFO, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get tc_cfg info failed, ret:%d", ret), ret);

    ret = tool_display_tc_cfg_info(result);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool_display_tc_cfg_info failed, ret:%d", ret), ret);

    return 0;
}

STATIC struct sub_option_entry tc_cfg_opts_param[] = {
    {"-size", ARG_TYPE_STR, parse_str, MODE_SET, MODE_SET},
    {"-waterline", ARG_TYPE_STR, parse_str, MODE_SET, MODE_SET},
};

void get_tc_cfg_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = tc_cfg_opts_param;
    *count = ARRAY_SIZE_COMPUTE(tc_cfg_opts_param);
}

STATIC void *alloc_tc_cfg_args_init(void)
{
    return calloc(1, sizeof(struct tc_cfg_cmd_args));
}

STATIC int set_tc_cfg_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct tc_cfg_cmd_args *tc_cfg_args = (struct tc_cfg_cmd_args *)args;
    int ret = 0;

    if (strcmp(opt->name, "-size") == 0) {
        ret = strcpy_s(tc_cfg_args->tc_buff, INPUT_TC_CFG_LEN, type_val->str_val);
    } else if (strcmp(opt->name, "-waterline") == 0) {
        ret = strcpy_s(tc_cfg_args->waterline, INPUT_TC_CFG_LEN, type_val->str_val);
    }
    CHK_PRT_RETURN(ret != 0, hccn_err("strcpy_s %s failed", opt->name), ret);
    return 0;
}

STATIC int tc_str_to_value(char *str, unsigned int *tc_value, unsigned int tc_num)
{
    unsigned int i, len = (unsigned int)strlen(str);
    char *end_tc_str_p = NULL;
    char *tc_size_str = NULL;
    unsigned int tmp_value;
    unsigned int tc_i = 0;

    CHK_PRT_RETURN(*str == ',' || str[strlen(str) - 1] == ',',
                   TOOL_PRINT_ERR("invalid TC config: string starts or ends with comma"), TOOL_PRIVATE_ARGS_VALUE_ERR);
    for (i = 0; i < len; i++) {
        if (str[i] == ',') {
            CHK_PRT_RETURN(i == 0 || !isdigit((unsigned char)str[i - 1]),
                           hccn_err("before ',' is [%c], not a valid digit", str[i - 1]), TOOL_PRIVATE_ARGS_PARSE_ERR);
            CHK_PRT_RETURN(i == len - 1 || str[i + 1] == '\0', hccn_err("last letter is ','"),
                           TOOL_PRIVATE_ARGS_PARSE_ERR);
        }
    }

    tc_size_str = strtok(str, ",");
    for (tc_i = 0; tc_size_str != NULL; tc_i++) {
        CHK_PRT_RETURN(tc_i >= tc_num, hccn_err("invalid TC config information"), TOOL_PRIVATE_ARGS_NUM_ERR);

        tmp_value = (unsigned int)strtoul(tc_size_str, &end_tc_str_p, 0);
        CHK_PRT_RETURN((end_tc_str_p == tc_size_str) || (*end_tc_str_p != '\0') || (errno == ERANGE),
                       TOOL_PRINT_ERR("transfer TC cfg value %s failed", tc_size_str), TOOL_PRIVATE_ARGS_VALUE_ERR);

        tc_value[tc_i] = tmp_value;
        tc_size_str = strtok(NULL, ",");
    }

    return 0;
}

int tool_tc_cfg_get_size(char *str_tc_buf, struct tc_cfg_info *tc_cfg)
{
    unsigned int tmp_value[REAL_USED_TC_NUM] = {0};
    unsigned int total_rx_tc_size = 0;
    int ret, tc_i;

    ret = tc_str_to_value(str_tc_buf, tmp_value, REAL_USED_TC_NUM);
    CHK_PRT_RETURN(ret != 0, hccn_err("No valid TC size information"), ret);

    for (tc_i = 0; tc_i < REAL_USED_TC_NUM; tc_i++) {
        CHK_PRT_RETURN(
            ((tmp_value[tc_i] != 0) && (tmp_value[tc_i] < MIN_RX_TC_SIZE)) || (tmp_value[tc_i] > MAX_TOTAL_RX_TC_SIZE),
            TOOL_PRINT_ERR("TC size is invalid, tc_i:%d tc_size:%u out of range [%d, %d]", tc_i, tmp_value[tc_i],
                           MIN_RX_TC_SIZE, MAX_TOTAL_RX_TC_SIZE),
            TOOL_PRIVATE_ARGS_VALUE_ERR);

        tc_cfg->tc_buffer_size[tc_i] = tmp_value[tc_i];
        total_rx_tc_size += tmp_value[tc_i];
        CHK_PRT_RETURN(total_rx_tc_size > MAX_TOTAL_RX_TC_SIZE,
                       TOOL_PRINT_ERR("total tc size %u out of range [0, %d]", total_rx_tc_size, MAX_TOTAL_RX_TC_SIZE),
                       TOOL_PRIVATE_ARGS_VALUE_ERR);
    }

    return 0;
}

int tool_tc_cfg_get_waterline(char *str_waterline, struct tc_cfg_info *tc_cfg)
{
    unsigned int tmp_value[REAL_USED_TC_NUM] = {0};
    int ret, tc_i;

    ret = tc_str_to_value(str_waterline, tmp_value, REAL_USED_TC_NUM);
    CHK_PRT_RETURN(ret != 0, hccn_err("No valid water line size information"), ret);

    for (tc_i = 0; tc_i < REAL_USED_TC_NUM; tc_i++) {
        CHK_PRT_RETURN(tmp_value[tc_i] > tc_cfg->tc_buffer_size[tc_i],
                       TOOL_PRINT_ERR("Invalid water line size, tc_i:%d waterline:%u exceeds tc_buff size %u", tc_i,
                                      tmp_value[tc_i], tc_cfg->tc_buffer_size[tc_i]),
                       TOOL_PRIVATE_ARGS_VALUE_ERR);

        tc_cfg->high_waterline[tc_i] = tmp_value[tc_i];
    }

    return 0;
}

STATIC int tool_set_tc_cfg_exec(struct command_context *ctx, struct tc_cfg_info *tc_cfg)
{
    struct tc_cfg_cmd_args *tc_cfg_args = (struct tc_cfg_cmd_args *)ctx->cmd_args;
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret;

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    ret = tool_tc_cfg_get_size(tc_cfg_args->tc_buff, tc_cfg);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool_tc_cfg_get_size failed"), ret);

    ret = tool_tc_cfg_get_waterline(tc_cfg_args->waterline, tc_cfg);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool_tc_cfg_get_waterline failed"), ret);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)tc_cfg, sizeof(struct tc_cfg_info), NULL,
                   &size);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_TC_CFG_INFO, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi set tc_cfg info failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
                   ret);

    return 0;
}

STATIC int tool_save_tc_cfg(struct command_context *ctx, struct tc_cfg_info *tc_cfg)
{
#define FORMAT_8_UINTS "%u,%u,%u,%u,%u,%u,%u,%u"
    char match_list[1][CONFLINE_LENGTH];
    char cmd_buf[CONFLINE_LENGTH] = {0};
    int ret = 0;

    ret = sprintf_s(match_list[0], CONFLINE_LENGTH, "tc_cfg_%d=", ctx->slot_id);
    CHK_PRT_RETURN(ret <= 0, hccn_err("set tc_cfg sprintf match list failed, ret:%d", ret), -EIO);

    ret = sprintf_s(cmd_buf, CONFLINE_LENGTH, "tc_cfg_%d=%s " FORMAT_8_UINTS " %s " FORMAT_8_UINTS, ctx->slot_id,
                    "size", PRIORITY_ARGS(tc_cfg->tc_buffer_size), "waterline", PRIORITY_ARGS(tc_cfg->high_waterline));
    CHK_PRT_RETURN(ret <= 0, hccn_err("sprintf TC cfg size failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
                   -EIO);

    cmd_buf[strlen(cmd_buf)] = '\n';
    ret = tool_update_conf(cmd_buf, FILE_OPT_TYPE_MATCH_REPLACES, match_list, 1);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool_update_conf for tc_cfg failed, ret:%d", ret), TOOL_WRITE_CONF_ERR);

    return 0;
}

STATIC int tool_tc_cfg_exec(struct command_context *ctx)
{
    struct tc_cfg_info tc_cfg = {0};
    int ret = 0;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    if (ctx->mode == MODE_GET) {
        return tool_get_tc_cfg_exec(ctx);
    } else {
        ret = tool_set_tc_cfg_exec(ctx, &tc_cfg);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool set tc cfg failed, ret:%d", ret), ret);

        ret = tool_save_tc_cfg(ctx, &tc_cfg);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool_save_tc_cfg failed, ret:%d", ret), ret);

        return 0;
    }
}

STATIC void tool_tc_cfg_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_tc_cfg_print_help(ctx, MODE_GET);
            tool_tc_cfg_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-tc_cfg", "get traffic class cfg");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-tc_cfg", "set traffic class cfg");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            TOOL_PRINT_OPT("-size <tc_buff_size_arr>", "set tc buffer size of tc [0, 7], such as x,x,x,x,x,x,x,x");
            TOOL_PRINT_OPT(" ", "size range {0, [4480, 849600]} and total size must be smaller than 849600");
            TOOL_PRINT_OPT("-waterline <waterline_arr>", "set waterline of tc [0, 7], such as x,x,x,x,x,x,x,x");
            TOOL_PRINT_OPT(" ", "waterline must not be larger than tc_buff size");
            break;
        default:
            break;
    }
}

STATIC int tool_shaping_exec(struct command_context *ctx)
{
    unsigned int size_out = sizeof(struct shaping_result);
    struct shaping_result result = {0};
    struct dsmi_info info = {0};
    int ret, i;

    CHECK_BOND_DEV_RETURN(ctx->dev_name, ctx->slot_id);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, (char *)&result, &size_out);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_TM_SHAPING_PORT, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi get shaping info failed, ret:%d", ret), ret);

    for (i = 0; i < DEV_INFO_DEFAULT_BOND_SLAVE_NUM; i++) {
        TOOL_PRINT_INFO(
            "[dev_name: %s] bw_cur:%u Mbps, bw_max:%u Mbps", result.info[i].slave_name,
            (result.info[i].bw_limit > result.info[i].bw_max) ? result.info[i].bw_max : result.info[i].bw_limit,
            result.info[i].bw_max);
    }

    return 0;
}

STATIC void tool_shaping_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_shaping_print_help(ctx, MODE_GET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-shaping", "get shaper port bw cur & max bw capability");
            tool_print_npu_info(DEV_DEV_ATTR);
            tool_print_dev_info(DEV_BOND_DEV);
            break;
        default:
            break;
    }
}

STATIC struct sub_option_entry qos_dscp_opts_param[] = {
    {"-map", ARG_TYPE_NONE, parse_none, MODE_SET, 0},
    {"-unmap", ARG_TYPE_NONE, parse_none, MODE_SET, 0},
    {"-qos", ARG_TYPE_INT, parse_int, MODE_BOTH, MODE_SET, true, .range.int_range = {0, HCCL_QOS_MAX_IDX}},
    {"-dscp", ARG_TYPE_INT, parse_int, MODE_SET, 0, true, .range.int_range = {0, DSCP_MAX_IDX}},
};

void get_qos_dscp_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = qos_dscp_opts_param;
    *count = ARRAY_SIZE_COMPUTE(qos_dscp_opts_param);
}

STATIC void *alloc_qos_dscp_args_init(void)
{
    return calloc(1, sizeof(struct qos_dscp_args));
}

STATIC int qos_dscp_assign(void *args, struct sub_option_entry *opt, value_union *type_val)
{
    struct qos_dscp_args *qos_args = (struct qos_dscp_args *)args;

    if (strcmp(opt->name, "-map") == 0) {
        if (qos_args->op != OP_NONE) {
            goto qos_op_conflict;
        }
        qos_args->op = OP_MAP;
        return 0;
    }

    if (strcmp(opt->name, "-unmap") == 0) {
        if (qos_args->op != OP_NONE) {
            goto qos_op_conflict;
        }
        qos_args->op = OP_UNMAP;
        return 0;
    }

    if (strcmp(opt->name, "-qos") == 0) {
        qos_args->qos_idx = type_val->int_val;
        return 0;
    }

    if (strcmp(opt->name, "-dscp") == 0) {
        qos_args->dscp_idx = type_val->int_val;
        return 0;
    }
qos_op_conflict:
    TOOL_PRINT_ERR("Option -map and -unmap conflict with each other.");
    return TOOL_PRIVATE_ARGS_CONFLICT_ERR;
}

STATIC void tool_print_qos_dscp_map(int qos_idx, struct qos_map_info *qos_cfg)
{
    if (qos_cfg[qos_idx].save_cfg_flag) {
        TOOL_PRINT_INFO("%d\t%d", qos_idx, qos_cfg[qos_idx].dscp_idx);
    } else {
        TOOL_PRINT_INFO("%d\t-", qos_idx);
    }
}

STATIC int tool_get_qos_dscp_exec(struct command_context *ctx)
{
    struct qos_dscp_args *qos_args = (struct qos_dscp_args *)ctx->cmd_args;
    struct qos_map_info qos_cfg[HCCL_QOS_LEVEL_CNT] = {0};
    unsigned int size_out = sizeof(qos_cfg);
    struct dsmi_info info = {0};
    int i, ret;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, (char *)qos_cfg, &size_out);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_QOS_DSCP_INFO, &info);
    CHK_PRT_RETURN(ret != 0 && ret != FILE_OPT_SYS_RD_FILE_NOT_FOUND,
                   hccn_err("dsmi get qos_to_dscp failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    TOOL_PRINT_INFO("qos\tdscp");
    if (qos_args == NULL) { // get all qos_dscp_map
        for (i = 0; i < HCCL_QOS_LEVEL_CNT; i++) {
            tool_print_qos_dscp_map(i, qos_cfg);
        }
    } else {
        tool_print_qos_dscp_map(qos_args->qos_idx, qos_cfg);
    }

    return 0;
}

STATIC int check_print_qos_ret(struct command_context *ctx, struct qos_dscp_args *qos_args, int ret)
{
    switch (ret) {
        case DEV_QOS_UNMAP_REPEAT_ERR:
            TOOL_PRINT_ERR("Cannot unmap not configured qos[%d].", qos_args->qos_idx);
            break;
        case DEV_QOS_MAP_CONFLICT_ERR:
            TOOL_PRINT_ERR("Cannot map different qos to the same dscp[%d].", qos_args->dscp_idx);
            break;
        default:
            break;
    }

    return ret;
}

int tool_set_qos_dscp_exec(struct command_context *ctx)
{
    struct qos_dscp_args *qos_args = (struct qos_dscp_args *)ctx->cmd_args;
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    CHK_PRT_RETURN(qos_args->op == OP_UNMAP && (ctx->sub_opt_mask & DSCP_OPT_BIT) != 0,
                   TOOL_PRINT_ERR("Option -unmap and -dscp conflict with each other."), TOOL_PRIVATE_ARGS_CONFLICT_ERR);
    CHK_PRT_RETURN(qos_args->op == OP_MAP && (ctx->sub_opt_mask & DSCP_OPT_BIT) == 0,
                   hccn_err("option -dscp must be specified in the map mode"), TOOL_PRIVATE_ARGS_NUM_ERR);
    CHK_PRT_RETURN(qos_args->op == OP_NONE, hccn_err("option -unmap or -map must be specified"),
                   TOOL_PRIVATE_ARGS_NUM_ERR);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)qos_args, sizeof(struct qos_dscp_args),
                   NULL, &size);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_QOS_DSCP_INFO, &info);
    CHK_PRT_RETURN(check_print_qos_ret(ctx, qos_args, ret) != 0,
                   hccn_err("dsmi set qos_to_dscp failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    return 0;
}

int parse_qos_cfg(struct qos_map_info *qos_cfg, int qos_cnt, char *string)
{
#define MAP_CNT_TWO 2
    int qos_idx, dscp_idx;
    char *save_ptr = NULL;
    char *token = NULL;
    int count = 0;

    token = strtok_r(string, ",", &save_ptr);
    while (token != NULL && count < qos_cnt) {
        CHK_PRT_RETURN(sscanf_s(token, "%d:%d", &qos_idx, &dscp_idx) != MAP_CNT_TWO,
                       hccn_err("Invalid format in token:%s", token), -EINVAL);
        CHK_PRT_RETURN(dscp_idx < 0 || dscp_idx > DSCP_MAX_IDX || qos_idx < 0 || qos_idx > HCCL_QOS_MAX_IDX,
                       hccn_err("dscp_idx or qos_idx is invalid, dscp_idx:%d, range[0, %d], qos_idx:%d, range[0, %d]",
                                dscp_idx, DSCP_MAX_IDX, qos_idx, HCCL_QOS_MAX_IDX),
                       -EINVAL);

        count++;
        qos_cfg[qos_idx].dscp_idx = dscp_idx;
        qos_cfg[qos_idx].save_cfg_flag = true;
        token = strtok_r(NULL, ",", &save_ptr);
    }

    return 0;
}

STATIC int tool_save_qos_dscp(struct command_context *ctx)
{
    struct qos_dscp_args *qos_args = (struct qos_dscp_args *)ctx->cmd_args;
    struct qos_map_info qos_cfg[HCCL_QOS_LEVEL_CNT] = {0};
    char qos_dscp_cfg_string[QOS_DSCP_CFG_LEN] = {0};
    char tmp_map_string[QOS_MAP_STRING_LEN] = {0};
    int ret, i;

    ret = tool_read_conf(ctx->slot_id, "qos_dscp", qos_dscp_cfg_string, QOS_DSCP_CFG_LEN);
    CHK_PRT_GOTO(ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND, hccn_warn("qos_map not saved before"), update);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("read %s for qos_dscp_cfg failed! ret:%d logic_id:%d slot_id:%d", CFGFILE_NAME, ret,
                            ctx->npu_info.logic_id, ctx->slot_id),
                   ret);

    ret = parse_qos_cfg(qos_cfg, HCCL_QOS_LEVEL_CNT, qos_dscp_cfg_string);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("parse_qos_cfg failed! ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id, ctx->slot_id),
        ret);

update:
    qos_cfg[qos_args->qos_idx].dscp_idx = qos_args->dscp_idx;
    qos_cfg[qos_args->qos_idx].save_cfg_flag = (qos_args->op == OP_MAP);

    (void)memset_s(qos_dscp_cfg_string, QOS_DSCP_CFG_LEN, 0, QOS_DSCP_CFG_LEN);
    for (i = 0; i < HCCL_QOS_LEVEL_CNT; i++) {
        if (!qos_cfg[i].save_cfg_flag) {
            continue;
        }
        (void)memset_s(tmp_map_string, QOS_MAP_STRING_LEN, 0, QOS_MAP_STRING_LEN);
        ret = snprintf_s(tmp_map_string, QOS_MAP_STRING_LEN, QOS_MAP_STRING_LEN - 1, "%d:%d,", i, qos_cfg[i].dscp_idx);
        CHK_PRT_RETURN(ret <= 0, hccn_err("snprintf_s tmp_string for qos%d failed, ret:%d", i, ret), -EIO);

        ret = strcat_s(qos_dscp_cfg_string, QOS_DSCP_CFG_LEN, tmp_map_string);
        CHK_PRT_RETURN(ret != 0, hccn_err("strcat %s failed, ret:%d", tmp_map_string, ret), ret);
    }

    if (strlen(qos_dscp_cfg_string) > 0) {
        qos_dscp_cfg_string[strlen(qos_dscp_cfg_string) - 1] = '\0'; // delete last ','
    }

    ret = tool_write_conf(ctx->slot_id, "qos_dscp", qos_dscp_cfg_string);
    CHK_PRT_RETURN(
        ret != 0, hccn_err("write cfg error, ret:%d slot_id:%d logic_id:%d", ret, ctx->slot_id, ctx->npu_info.logic_id),
        TOOL_WRITE_CONF_ERR);

    return 0;
}

STATIC int tool_qos_dscp_exec(struct command_context *ctx)
{
    int ret = 0;

    if (ctx->mode == MODE_GET) {
        return tool_get_qos_dscp_exec(ctx);
    } else {
        ret = hccn_check_usr_identify();
        CHK_PRT_RETURN(
            ret != 0,
            hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
            TOOL_PUBLIC_ROOT_UNSUPPORT);

        ret = tool_set_qos_dscp_exec(ctx);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool_set_qos_dscp_exec failed, ret:%d", ret), ret);

        ret = tool_save_qos_dscp(ctx);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool_save_qos_dscp failed, ret:%d", ret), ret);

        return 0;
    }
}

STATIC void tool_qos_dscp_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_qos_dscp_print_help(ctx, MODE_GET);
            tool_qos_dscp_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-qos_to_dscp", "get all qos to dscp map info");
            tool_print_npu_info(DEV_NPU_ATTR);
            TOOL_PRINT_OPT("[-qos <qos_level>]", "get the specified qos of dscp map info, range [0, 7]");
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-qos_to_dscp", "set the specified qos of dscp map info");
            tool_print_npu_info(DEV_NPU_ATTR);
            TOOL_PRINT_OPT("-map", "map mode");
            TOOL_PRINT_OPT("-qos <qos_level>", "set qos level, range [0, 7]");
            TOOL_PRINT_OPT("-dscp <dscp_id>", "set dscp value, range [0, 63]");

            TOOL_PRINT_CMD("-s", "-qos_to_dscp", "unmap the specified qos of dscp");
            tool_print_npu_info(DEV_NPU_ATTR);
            TOOL_PRINT_OPT("-unmap", "unmap mode");
            TOOL_PRINT_OPT("-qos <qos_level>", "set qos level, range [0, 7]");
            break;
        default:
            break;
    }
}

STATIC struct cmd_entry g_ubase_cmds[] = {
    {
        "-credit",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_credit_exec,
        {DEV_PORT_TYPE},
        tool_credit_print_help,
        NULL,
        0,
    },
    {
        "-upgrade_lane",
        NULL,
        MODE_SET,
        NULL,
        NULL,
        tool_upgrade_lane_exec,
        {DEV_PORT_TYPE},
        tool_upgrade_lane_print_help,
        NULL,
        0,
    },
    {
        "-cur_tc_buf",
        get_cur_tc_buf_sub_opts,
        MODE_GET,
        alloc_cur_tc_buf_init,
        cur_tc_buf_assign,
        tool_cur_tc_buf_exec,
        {DEV_DEV_ATTR},
        tool_cur_tc_buf_print_help,
        NULL,
        0,
    },
    {
        "-tc_cfg",
        get_tc_cfg_sub_opts,
        MODE_BOTH,
        alloc_tc_cfg_args_init,
        set_tc_cfg_assign,
        tool_tc_cfg_exec,
        {DEV_DEV_ATTR},
        tool_tc_cfg_print_help,
        NULL,
        0,
    },
    {
        "-bandwidth",
        get_bandwidth_sub_opts,
        MODE_GET,
        alloc_bandwidth_args_init,
        bandwidth_assign,
        tool_bandwidth_exec,
        {DEV_PORT_TYPE, DEV_DEV_ATTR},
        tool_bandwidth_print_help,
        NULL,
        0,
    },
    {
        "-shaping",
        NULL,
        MODE_GET,
        NULL,
        NULL,
        tool_shaping_exec,
        {DEV_DEV_ATTR},
        tool_shaping_print_help,
        NULL,
        0,
    },
    {
        "-qos_to_dscp",
        get_qos_dscp_sub_opts,
        MODE_BOTH,
        alloc_qos_dscp_args_init,
        qos_dscp_assign,
        tool_qos_dscp_exec,
        {DEV_NPU_ATTR},
        tool_qos_dscp_print_help,
        NULL,
        0,
    },
};

void tool_get_ubase_cmds(struct cmd_entry **cmds, unsigned int *count)
{
    *cmds = g_ubase_cmds;
    *count = ARRAY_SIZE_COMPUTE(g_ubase_cmds);
}
