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
#include <math.h>
#include "cmd.h"
#include "cfg.h"
#include "dsmi_common_interface.h"
#include "log.h"
#include "weak_passwd_dict.h"
#include "tls.h"
#include "tool.h"
#include "ds_net_ext.h"
#include "tool_cdr.h"

static major_cmd_t g_cdr_info_cmd = {
    .name = "cdr",
    .option_count = 0,
};

STATIC struct tool_cdr_data g_cdr_cfg = {0};

STATIC int tool_cdr_type_parse(major_cmd_t *self, const char *argv)
{
    off_t opcode;

    if (str_to_off_t(argv, &opcode)) {
        return UDA_PARAM_INVALID_ERR;
    }
    g_cdr_cfg.op_code = (u32)opcode;

    return 0;
}

STATIC int tool_trig_query_ext_cdr_log_exe(void)
{
    struct trig_get_cdr_info trig_info = {0};
    int logic_id = g_cdr_cfg.logic_id;
    int ret;

    trig_info.type = TRIG_CDR_EXT_LOG;
    ret = dsmi_trig_query_cdr_info(logic_id, &trig_info);
    if (ret) {
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return UDA_EXE_SUCCESS;
}

STATIC int tool_trig_query_cdr_reg_info_exe(void)
{
    struct trig_get_cdr_info trig_info = {0};
    int logic_id = g_cdr_cfg.logic_id;
    int ret;

    trig_info.type = TRIG_CDR_REG;
    ret = dsmi_trig_query_cdr_info(logic_id, &trig_info);
    if (ret) {
        roce_err("Dsmi_trig_query_cdr_info fail(ret=%d)", ret);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    if (trig_info.flag == IS_UPDATING) {
        roce_err("CDR reg data is updating, please wait a moment(30s) to trig");
        return UDA_TOOL_CDR_IS_UPDATING_ERR;
    }

    return UDA_EXE_SUCCESS;
}

STATIC void store_cdr_reg_info(u32 size, struct query_cdr_reg_info *cdr_info)
{
    u32 *reg_addr = NULL;
    u32 *reg_value = NULL;
    u32 i;

    for (i = 0; i < size; i += 8U) {
        reg_addr = (u32 *)(&cdr_info->data[i]);
        reg_value = (u32 *)(&cdr_info->data[i + sizeof(u32)]);
        CdrRegLog("\t Reg_addr:0x%x, Reg_value:0x%x;", *reg_addr, *reg_value);
    }
}

STATIC int get_cdr_reg_info(int logic_id, u32 size, u32 base_offset, u8 cdr_lane_logic_id)
{
    struct query_cdr_reg_info cdr_reg_info = {{0}};
    u32 cur_get_cdr_reg_size;
    int ret;
    u32 size_tmp = size;
    u32 base_offset_tmp = base_offset;

    while (size_tmp > 0) {
        cur_get_cdr_reg_size = (size_tmp > ONE_CMD_MAX_CDR_REG_SIZE) ? ONE_CMD_MAX_CDR_REG_SIZE : size_tmp;
        size_tmp -= cur_get_cdr_reg_size;
        cur_get_cdr_reg_size = cur_get_cdr_reg_size & (~0x7U); // data get from imp must be align with 8Byte
        cdr_reg_info.head.start_offset = base_offset_tmp;
        cdr_reg_info.head.size = cur_get_cdr_reg_size;
        cdr_reg_info.head.cdr_logic_lane_id = cdr_lane_logic_id;
        ret = dsmi_get_cdr_reg_info(logic_id, &cdr_reg_info);
        if (ret) {
            roce_err("Dsmi_get_cdr_reg_info (ret=%d)", ret);
            return dsmi_analysis_dsmi_ret_to_uda(ret);
        }

        base_offset_tmp += cur_get_cdr_reg_size;
        store_cdr_reg_info(cur_get_cdr_reg_size, &cdr_reg_info);
    }

    return UDA_EXE_SUCCESS;
}

STATIC int check_query_cdr_reg_flag(struct query_mac_lane_info *mac_lane_info)
{
    if (mac_lane_info->head.info.flag == TRIG_UPDATE_START) {
        roce_err("CDR reg data is updating, please wait a moment(30s) to get");
        return UDA_TOOL_CDR_IS_UPDATING_ERR;
    } else if (mac_lane_info->head.info.flag == INIT_STATE) {
        roce_err("CDR reg data is not trigged, must trig at least once");
        return UDA_TOOL_CDR_NOT_TRIGGERED_ERR;
    } else if (mac_lane_info->head.info.flag == TRIG_UPDATE_FAIL) {
        roce_err("CDR reg data update data fail, please retrig or exception cdr");
        return UDA_TOOL_CDR_UPDATE_FAIL_ERR;
    }

    return UDA_SUCCESS;
}

STATIC int tool_cdr_reg_dump_exe(void)
{
    struct query_mac_lane_info mac_lane_info = {{{0}}};
    int logic_id = g_cdr_cfg.logic_id;
    u32 cdr_reg_base_offset;
    u32 cdr_chanel_size;
    u8 cdr_lane_logic_id;
    u8 logic_num;
    int ret;
    int i;

    ret = dsmi_query_mac_lane_cdr_info(logic_id, &mac_lane_info);
    if (ret) {
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    ret = check_query_cdr_reg_flag(&mac_lane_info);
    if (ret) {
        return ret;
    }

    ret = generate_file_name(g_cdr_cfg.phy_id);
    if (ret) {
        roce_err("Generate CDR reg log file fail");
        return ret;
    }

    logic_num = mac_lane_info.head.info.logic_num;
    for (i = 0; i < logic_num; i++) {
        cdr_chanel_size = mac_lane_info.data[i].tx_cdr_chanel_size;
        cdr_reg_base_offset = mac_lane_info.data[i].tx_cdr_reg_base_offset;
        cdr_lane_logic_id = mac_lane_info.data[i].tx_cdr_logic_lane_id;
        CdrRegLog("Tx_cdr_lane_id:0x%x", mac_lane_info.data[i].tx_cdr_chanel);
        ret = get_cdr_reg_info(logic_id, cdr_chanel_size, cdr_reg_base_offset, cdr_lane_logic_id);
        if (ret) {
            return ret;
        }

        cdr_chanel_size = mac_lane_info.data[i].rx_cdr_chanel_size;
        cdr_reg_base_offset = mac_lane_info.data[i].rx_cdr_reg_base_offset;
        cdr_lane_logic_id = mac_lane_info.data[i].rx_cdr_logic_lane_id;
        CdrRegLog("Rx_cdr_lane_id:0x%x", mac_lane_info.data[i].rx_cdr_chanel);

        ret = get_cdr_reg_info(logic_id, cdr_chanel_size, cdr_reg_base_offset, cdr_lane_logic_id);
        if (ret) {
            return ret;
        }
    }
    return UDA_EXE_SUCCESS;
}

STATIC int tool_write_cdr_log_to_file(char *buf, int len, int dev_id, int cdr_id)
{
    struct timeval timeVal = {0};
    struct tm nowTime = {0};
    char log_name[MAX_CDR_LOG_NAME_LEN] = {0};
    FILE *fp = NULL;
    size_t write_cnt;
    int ret;

    if ((gettimeofday(&timeVal, NULL)) != 0) {
        roce_err("Gettimeofday failed. (errno=%d)", errno);
        return UDA_TOOL_SYS_TIME_OP_ERR;
    }

    if (localtime_r(&(timeVal.tv_sec), &nowTime) == NULL) {
        roce_err("Localtime_r failed. (errno=%d)", errno);
        return UDA_TOOL_SYS_TIME_OP_ERR;
    }

    ret = sprintf_s(log_name, MAX_CDR_LOG_NAME_LEN, "device-%d-cdr-%d_%04d%02d%02d%02d%02d%02d%03ld.log", dev_id,
                    cdr_id, nowTime.tm_year + TIME_BEGIN_YEAR, nowTime.tm_mon + 0x1, nowTime.tm_mday, nowTime.tm_hour,
                    nowTime.tm_min, nowTime.tm_sec, (timeVal.tv_usec / MS_TO_US_CNT));
    if (ret <= 0) {
        roce_err("Sprintf_s log name failed. (ret=%d)", ret);
        return UDA_TOOL_SYS_TIME_OP_ERR;
    }

    fp = fopen(log_name, "w+");
    if (fp == NULL) {
        roce_err("Open log file failed. (file=\"%s\")", log_name);
        return UDA_TOOL_SYS_FOPEN_ERR;
    }

    write_cnt = fwrite(buf, 0x1, len, fp);
    if (write_cnt < (size_t)len) {
        roce_err("Write log file failed. (write_cnt=%u; file=\"%s\")", write_cnt, log_name);
        fclose(fp);
        return UDA_TOOL_SYS_WRITE_FILE_ERR;
    }

    (void)fclose(fp);
    return 0;
}

STATIC int tool_cdr_log_cmd_exe(void)
{
    struct send_data_info data_info;
    int logic_id;
    int ret;

    logic_id = g_cdr_cfg.logic_id;

    TOOL_SET_SEND_DATA(data_info, DS_GET_CDR_LOG, NULL, 0, NULL, 0);
    data_info.out_buf = (char *)calloc(MAX_CDR_LOG_SIZE, sizeof(char));
    if (data_info.out_buf == NULL) {
        roce_err("Tool get cdr log calloc out buf failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }
    data_info.size_out = MAX_CDR_LOG_SIZE;

    ret = dsmi_send_data(logic_id, 0, &data_info);
    if (ret) {
        free(data_info.out_buf);
        roce_err("Dsmi send data failed. (ret=%d; logic_id=%d)", ret, logic_id);
        ret = (ret == UDA_TOOL_SYS_BUSY_ERR) ? UDA_TOOL_SYS_BUSY_ERR : dsmi_analysis_dsmi_ret_to_uda(ret);
        return ret;
    }

    if (data_info.size_out > MAX_CDR_LOG_SIZE) {
        data_info.size_out = MAX_CDR_LOG_SIZE;
    }

    ret = tool_write_cdr_log_to_file(data_info.out_buf, data_info.size_out, logic_id, 0);
    if (ret) {
        roce_err("Write cdr log to file failed. (ret=%d)", ret);
    }
    free(data_info.out_buf);

    return ret;
}

static struct cdr_execute g_cmd_handle[] = {
    {CDR_OP_TRIG_REG_DUMP, tool_trig_query_cdr_reg_info_exe},
    {CDR_OP_CDR_REG_DUMP, tool_cdr_reg_dump_exe},
    {CDR_OP_TRIG_EXT_LOG_DUMP, tool_trig_query_ext_cdr_log_exe},
    {CDR_OP_CDR_LOG_DUMP, tool_cdr_log_cmd_exe},
};

int tool_cdr_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    u32 handle_size = ARRAY_SIZE(g_cmd_handle);
    u32 op_code;
    int ret;
    u32 i;

    if (argc == 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    g_cdr_cfg.logic_id = param->logic_id;
    g_cdr_cfg.phy_id = param->phy_id;
    ret = major_command_option(&g_cdr_info_cmd, "-t", "--type", 1, tool_cdr_type_parse);
    if (ret) {
        return ret;
    }

    ret = command_parse(&g_cdr_info_cmd, argc, argv);
    if (ret) {
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    op_code = g_cdr_cfg.op_code;
    for (i = 0; i < handle_size; i++) {
        if (g_cmd_handle[i].op_code == op_code) {
            return g_cmd_handle[i].func();
        }
    }

    return UDA_PARAM_OP_NOT_SUPPORT_ERR;
}
