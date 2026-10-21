/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <math.h>
#include "tool.h"
#include "tool_lib.h"
#include "dsmi_common_interface.h"
#include "ds_net_ext.h"
#include "tool_ops.h"
#include "hccn_dfx_struct.h"

#define TOOL_CDR_SUPPORT 0
#define TOOL_CDR_NOT_SUPPORT 1

#define TOOL_CDR_LOOPBACK_CLOSE 2
#define TOOL_CDR_LOOPBACK_OPEN 3

#define TOOL_CDR_GET_DFX_REG 4
#define TOOL_CDR_GET_SNR_MODE 5

#define TOOL_CDR_ENABLE_RESET 6
#define TOOL_CDR_DISABLE_RESET 7

#define TOOL_CDR_PRINT_FMEA 8

#define CDR_RESET_SWITCH "cdr_reset_switch"
#define TOOL_CDR_ENABLE_RESET_STR "6"
#define TOOL_CDR_DISABLE_RESET_STR "7"

static unsigned int tool_show_get_cdr_offset(int phy_id, unsigned int board_type)
{
    unsigned int offset = 0;
    if (board_type == SUPERPOD_900_BOARD_TYPE) {
        if (phy_id / MAX_FRONT_NPU_NUM == 1) {
            offset = DEVICE_CDR_LANE_MAX_NUM;
        }
    } else {
        if (phy_id % EVEN_NUM == 1) {
            offset = DEVICE_CDR_LANE_MAX_NUM;
        }
    }

    return offset;
}

static void tool_show_unsupported_cdr_snr_info(void)
{
    int i;
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO("Cdr Host SNR Lane%d         : not supported", i + 1);
    }
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO("Cdr Media SNR Lane%d        : not supported", i + 1);
    }
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO("Cdr His Host SNR Lane%d     : not supported", i + 1);
    }
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO("Cdr His Media SNR Lane%d    : not supported", i + 1);
    }
    return;
}

#define CDR_SNR_TO_DB(snr) (10 * (log10((double)(snr))))
static void tool_show_ql_cdr_snr_info(struct cdr_snr_info *cdr_snr_info, unsigned int offset)
{
    int i;
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO(
            "Cdr Host SNR Lane%d         : %.4f dB", i + 1,
            cdr_snr_info->host_snr[i + offset] == 0 ? 0.0 : CDR_SNR_TO_DB(cdr_snr_info->host_snr[i + offset]));
    }
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO(
            "Cdr Media SNR Lane%d        : %.4f dB", i + 1,
            cdr_snr_info->media_snr[i + offset] == 0 ? 0 : CDR_SNR_TO_DB(cdr_snr_info->media_snr[i + offset]));
    }
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO("Cdr His Host SNR Lane%d     : not supported", i + 1);
    }
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO("Cdr His Media SNR Lane%d    : not supported", i + 1);
    }
}

static void tool_show_5901_cdr_snr_info(struct cdr_snr_info *cdr_snr_info, unsigned int offset)
{
    int i;
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO("Cdr Host SNR Lane%d         : %u", i + 1, cdr_snr_info->host_snr[i + offset]);
    }
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO("Cdr Media SNR Lane%d        : %u", i + 1, cdr_snr_info->media_snr[i + offset]);
    }
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO("Cdr His Host SNR Lane%d     : %u", i + 1, cdr_snr_info->his_host_snr[i + offset]);
    }
    for (i = 0; i < DEVICE_CDR_LANE_MAX_NUM; i++) {
        TOOL_PRINT_INFO("Cdr His Media SNR Lane%d    : %u", i + 1, cdr_snr_info->his_media_snr[i + offset]);
    }
}

static void tool_show_cdr_snr_info(int is_cdr_support, int phy_id, unsigned int board_type,
                                   struct cdr_snr_info *cdr_snr_info)
{
    if (is_cdr_support == TOOL_CDR_NOT_SUPPORT) {
        tool_show_unsupported_cdr_snr_info();
        return;
    }
    unsigned int offset = 0;

    offset = tool_show_get_cdr_offset(phy_id, board_type);

    if (cdr_snr_info->retimer_type == CDR_RETIMER_TYPE_5901) {
        tool_show_5901_cdr_snr_info(cdr_snr_info, offset);
    } else if (cdr_snr_info->retimer_type == CDR_RETIMER_TYPE_QILIAN) {
        tool_show_ql_cdr_snr_info(cdr_snr_info, offset);
    } else {
        roce_err("Invalid retimer type. (type=%u)", cdr_snr_info->retimer_type);
    }
}

int tool_get_cdr_snr(struct tool_param *param, int port)
{
    int ret;
    int board_type;
    int is_cdr_supported = TOOL_CDR_SUPPORT;
    struct cdr_snr_info snr_infos = {0};
    int logic_id_after_convert = param->logic_id;

    ret = tool_get_board_type(param, &board_type);
    if (ret != 0) {
        roce_err("Tool get cdr snr cmd get board type failed. (ret=%d)", ret);
        return UDA_DSMI_EXE_ERR;
    }

    if ((board_type != A_K_BOARD_TYPE) && (board_type != A_X_BOARD_TYPE) && (board_type != SUPERPOD_900_BOARD_TYPE)) {
        is_cdr_supported = TOOL_CDR_NOT_SUPPORT;
        roce_info("Board_type:%d not support cdr.", board_type);
        tool_show_cdr_snr_info(is_cdr_supported, param->phy_id, board_type, &snr_infos);
        return 0;
    }

    if (board_type == SUPERPOD_900_BOARD_TYPE) {
        logic_id_after_convert = ATLAS_900_A3_SUPERPOD_logic_id_convert(param->phy_id);
        if (logic_id_after_convert == INVALID_LOGIC_ID) {
            roce_err("Tool get cdr snr convert failed. (logic_id_after_convert=%d, phy_id=%d)", logic_id_after_convert,
                     param->phy_id);
            return UDA_DSMI_CONTROL_LINK_UNREACHABLE_ERR;
        }
    }

    ret = dsmi_get_cdr_snr_cmd(logic_id_after_convert, port, &snr_infos);
    if (ret != UDA_EXE_SUCCESS) {
        roce_err("Dsmi_get_cdr_snr_cmd failed. (ret=%d)", ret);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    if (snr_infos.retimer_type != CDR_RETIMER_TYPE_5901 && snr_infos.retimer_type != CDR_RETIMER_TYPE_QILIAN) {
        roce_info("Invalid retimer type. (type=%u)", snr_infos.retimer_type);
        return UDA_DSMI_EXE_ERR;
    }

    tool_show_cdr_snr_info(is_cdr_supported, param->phy_id, board_type, &snr_infos);

    return 0;
}

static int tool_set_cdr_ret_convert(int result)
{
    if (result) {
        if (result == EBUSY) { // i2c控制权获取失败
            return UDA_DSMI_GET_I2C_CTRL_ERR;
        } else if (result == EOPNOTSUPP) {
            return UDA_DSMI_CMD_UPSUPPORT_ON_OPTICAL_ERR;
        } else if (result == ENOEXEC) {
            return UDA_DSMI_LOOPBACK_FAIL_FOR_DOWNGRADE_ERR;
        } else if (result == -EACCES) {
            return UDA_TOOL_SYS_NOT_ACCESS;
        }
        return UDA_DSMI_EXE_ERR;
    } else {
        return UDA_EXE_SUCCESS;
    }
}

int tool_set_cdr_mode(struct tool_param *param, struct ds_cdr_mode_info *info)
{
    int logic_id, convert_logic_id;
    int ret;

    logic_id = param->logic_id;

    ret = tool_get_convert_logic_id_and_master_flag(param, &convert_logic_id, &info->master_flag);
    if (ret != 0) {
        return ret;
    }

    // -t 0~4,6~7: 主die下发到主die;从die均先下发到主die（主die实际操作），再下发到从die(仅缓存)
    if (info->master_flag == 1) {
        ret = dsmi_set_cdr_mode_cmd(logic_id, 0, info);
        if (ret) {
            roce_err("Tool set cdr mode fail. (ret=%d; logic_id=%d; mode=%u; master_flag=%u)", ret, logic_id,
                     info->mode, info->master_flag);
            return tool_set_cdr_ret_convert(ret);
        }
    } else {
        ret = dsmi_set_cdr_mode_cmd(convert_logic_id, 0, info);
        if (ret) {
            roce_err("Tool set cdr mode fail. (ret=%d; convert_logic_id=%d; mode=%u; master_flag=%u)", ret,
                     convert_logic_id, info->mode, info->master_flag);
            return tool_set_cdr_ret_convert(ret);
        }
        ret = dsmi_set_cdr_mode_cmd(logic_id, 0, info);
        if (ret) {
            roce_err("Tool set cdr mode fail. (ret=%d; logic_id=%d; mode=%u; master_flag=%u)", ret, logic_id,
                     info->mode, info->master_flag);
            return tool_set_cdr_ret_convert(ret);
        }
    }

    return ret;
}

static int tool_set_cdr_reset_cmd(struct tool_param *param, struct ds_cdr_mode_info *info)
{
    int ret;
    struct tool_param temp_param;

    ret = tool_double_check("[WARNING] Running this command will influence another device that shares the same CDR.\n"
                            "Are you sure you want to continue?");
    if (ret) {
        return ret;
    }

    // A3双die场景仅主die有cdr控制权， 将操作转发到主die处理
    ret = memcpy_s(&temp_param, sizeof(struct tool_param), param, sizeof(struct tool_param));
    if (ret != 0) {
        roce_err("Memcpy failed! (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    if (is_ATLAS_900_A3_SUPERPOD(param->mainboard_id)) {
        temp_param.phy_id = atlas_900_a3_superpod_phy_id_convert(param->phy_id);
        if (temp_param.phy_id != param->phy_id) {
            // 从die，需要做logic_id 转换
            ret = dsmi_get_logicid_from_phyid(temp_param.phy_id, &temp_param.logic_id);
            if (ret) {
                roce_err("Dsmi get logicid from phyid fail. (ret=%d; phy_id=%d; origin_phy_id=%d)", ret,
                         temp_param.phy_id, param->phy_id);
                return UDA_DSMI_CONTROL_LINK_UNREACHABLE_ERR;
            }
        }
    }

    ret = tool_set_cdr_mode(&temp_param, info);
    if (ret) {
        roce_err("Tool set cdr reset cmd fail. (ret=%d, mode=%d)", ret, info->mode);
        return ret;
    }

    // cdr reset自愈恢复功能开关需要持久化
    if (!g_recovering) {
        ret = cfg_write_conf(temp_param.phy_id, CDR_RESET_SWITCH,
                             ((info->mode == TOOL_CDR_ENABLE_RESET) ? "enable" : "disable"));
        if (ret) {
            roce_err("Write cdr reset enable status error. (ret=%d; phy_id=%d)", ret, temp_param.phy_id);
            return ret;
        }
    }

    return ret;
}

static int tool_get_cdr_fmea(struct tool_param *param, int port)
{
    int ret;
    unsigned int i;
    int logic_id_after_convert = param->logic_id;

    if (is_ATLAS_900_A3_SUPERPOD(param->mainboard_id)) {
        logic_id_after_convert = ATLAS_900_A3_SUPERPOD_logic_id_convert(param->phy_id);
        if (logic_id_after_convert == INVALID_LOGIC_ID) {
            roce_err("Tool get fmea convert logic id failed. (phy_id=%d)", param->phy_id);
            return UDA_DSMI_CONTROL_LINK_UNREACHABLE_ERR;
        }
    } else {
        roce_err("this command not supported in this product");
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    ret = dsmi_get_cdr_fmea(logic_id_after_convert);
    if (ret) {
        roce_err("Tool get fmea dsmi failed. (logic_id=%d, ret=%d)", logic_id_after_convert, ret);
        return UDA_DSMI_EXE_ERR;
    }

    return 0;
}

static int tool_set_cdr_mode_cmd(int argc, char **argv, struct tool_param *param)
{
    char *p_tmp = NULL;
    int logic_id;
    int ret, len;
    long mode;
    struct ds_cdr_mode_info info = {0};

    if (argc != ARGC_ID_1) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    logic_id = param->logic_id;

    len = strlen(argv[0]);
    mode = strtol(argv[0], &p_tmp, NUMBER_BASE);
    if (len != ONE_VALUE || p_tmp == argv[0] || mode < 0 || mode > MAX_CDR_MODE_SIZE) {
        roce_err("The value of mode is wrong. [0 ~ %u].", MAX_CDR_MODE_SIZE);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    info.mode = (unsigned char)mode;

    if ((info.mode == TOOL_CDR_ENABLE_RESET) || (info.mode == TOOL_CDR_DISABLE_RESET)) {
        return tool_set_cdr_reset_cmd(param, &info);
    }

    if (info.mode == TOOL_CDR_GET_SNR_MODE) {
        param->get_cmd_flag = 1;
        ret = tool_get_cdr_snr(param, 0);
    } else if (info.mode == TOOL_CDR_PRINT_FMEA) {
        ret = tool_get_cdr_fmea(param, 0);
    } else {
        ret = tool_set_cdr_mode(param, &info);
    }

    return ret;
}

int tool_cdr_reset_recovery(struct tool_param *param)
{
#ifdef HCCN_TOOL_SHOW_EXT
    char reset_switch_data[ARGC_ID_8] = {"\0"};
    char *argv_tmp[ARGC_ID_1] = {NULL};
    int ret, board_type, temp_phy_id;

    ret = tool_get_board_type(param, &board_type);
    if (ret != 0) {
        roce_err("Tool sd cdr reset recovery get board type failed. (ret=%d)", ret);
        return UDA_DSMI_EXE_ERR;
    }
    if ((board_type != A_K_BOARD_TYPE) && (board_type != A_X_BOARD_TYPE) && (board_type != SUPERPOD_900_BOARD_TYPE)) {
        // 拦截非不带CDR产品类型
        return 0;
    }

    if (is_ATLAS_900_A3_SUPERPOD(param->mainboard_id)) {
        temp_phy_id = atlas_900_a3_superpod_phy_id_convert(param->phy_id);
        if (temp_phy_id != param->phy_id) {
            // 从die，不做recovery
            return 0;
        }
    }
    ret = cfg_read_conf(param->phy_id, CDR_RESET_SWITCH, reset_switch_data, ARGC_ID_8);
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            roce_info("The cdr reset switch conf name is not found in config file, skipped. (ret=%d)", ret);
            return 0;
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    }
    if (strcmp("enable", reset_switch_data) == 0) {
        argv_tmp[0] = TOOL_CDR_ENABLE_RESET_STR;
    } else if (strcmp("disable", reset_switch_data) == 0) {
        argv_tmp[0] = TOOL_CDR_DISABLE_RESET_STR;
    } else {
        roce_err("Invalid cdr reset switch. (phy_id=%d)", param->phy_id);
        return UDA_TOOL_PARSE_CONF_FILE_ERR;
    }

    ret = tool_set_cdr_mode_cmd(ARGC_ID_1, argv_tmp, param);
    if (ret != 0) {
        roce_err("Cfg cdr reset recovery failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }
#endif
    return 0;
}

STATIC void tool_print_5901_cdr_version(struct tool_param *param, struct ds_cdr_version_info *info)
{
    TOOL_PRINT_INFO("Manufacturer: Huawei");
    TOOL_PRINT_INFO("Chip Model: SD5901L");
    TOOL_PRINT_INFO("Chip Version: %d.%d.%d.%d - %d.%d.%d.%d", info->hilink_version[HILINK0],
                    info->hilink_version[HILINK1], info->hilink_version[HILINK2], info->hilink_version[HILINK3],
                    info->riscv_version[RISCV0], info->riscv_version[RISCV1], info->riscv_version[RISCV2],
                    info->riscv_version[RISCV3]);
    TOOL_PRINT_INFO("Reset Switch: %s", (info->reset_switch - '0' == 0) ? "disable" : "enable");
}

STATIC void tool_print_huyang_cdr_version(struct tool_param *param, struct ds_cdr_version_info *info)
{
    TOOL_PRINT_INFO("Manufacturer: HUYANG024");
    TOOL_PRINT_INFO("Chip Model: HY024-4301-0002");
    TOOL_PRINT_INFO("Chip Version: %d.%d.%d.%d", info->hilink_version[HILINK0], info->hilink_version[HILINK1],
                    info->hilink_version[HILINK2], info->hilink_version[HILINK3]);
    TOOL_PRINT_INFO("Reset Switch: %s", (info->reset_switch - '0' == 0) ? "disable" : "enable");
}

STATIC int tool_get_cdr_version(int argc, struct tool_param *param)
{
    int ret;
    struct ds_cdr_version_info info = {0};
    int logic_id;

    if (argc != 0) {
        roce_err("argc err. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    logic_id = param->logic_id;
    if (is_ATLAS_900_A3_SUPERPOD(param->mainboard_id)) {
        logic_id = ATLAS_900_A3_SUPERPOD_logic_id_convert(param->phy_id);
        if (logic_id == INVALID_LOGIC_ID) {
            roce_err("Get logic_id fail. (logic_id=%d)", logic_id);
            return UDA_DSMI_CONTROL_LINK_UNREACHABLE_ERR;
        }
    }

    ret = dsmi_get_cdr_version(logic_id, 0, &info);
    if (ret != 0) {
        roce_err("Dsmi get cdr version info fail! (ret=%d; logic_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    TOOL_PRINT_INFO("CDR info:");
    if (info.model[0] - '0' == CDR_5901) {
        tool_print_5901_cdr_version(param, &info);
    } else if (info.model[0] - '0' == CDR_QL) {
        tool_print_huyang_cdr_version(param, &info);
    } else {
        TOOL_PRINT_INFO("Manufacturer: NA");
        TOOL_PRINT_INFO("Chip Model: NA");
        TOOL_PRINT_INFO("Chip Version: NA");
        TOOL_PRINT_INFO("Reset Switch: NA");
    }
    return 0;
}

STATIC int tool_check_read_reg_param_addr(char *addr, struct scdr_reg_info *reg_info, char *is_visited_flag)
{
    char *p_end_str = NULL;
    long reg_param;
    if (addr && (strncmp(addr, "0x", strlen("0x")) != 0) && (strncmp(addr, "0X", strlen("0X")) != 0)) {
        roce_err("need 0x or 0X at address head.");
        return UDA_PARAM_INVALID_ERR;
    }
    // 字符串转换为16进制
    reg_param = strtol(addr, &p_end_str, NUMBER_HEX);
    if ((*p_end_str != '\0') || (reg_param <= 0)) {
        return UDA_PARAM_INVALID_ERR;
    }
    reg_info->address = (unsigned int)reg_param;
    *is_visited_flag = 1;
    return 0;
}

STATIC int tool_check_read_reg_param_valid(unsigned int address, unsigned int nums)
{
    unsigned int max_address;

    if ((nums < 1) || (nums > MAX_REG_NUMS) || (address % TOOL_CDR_REG_ADDR_OFFSET != 0)) {
        return -EINVAL;
    }

    max_address = address + (nums - 1) * TOOL_CDR_REG_ADDR_OFFSET;

    if ((address >= QL_LOW_REG_ADDR_MIN) && (max_address <= QL_LOW_REG_ADDR_MAX)) {
        return 0;
    }

    if ((address >= QL_MIDDLE_REG_ADDR_MIN) && (max_address <= QL_MIDDLE_REG_ADDR_MAX)) {
        return 0;
    }

    if ((address >= QL_HIGH_REG_ADDR_MIN) && (max_address <= QL_HIGH_REG_ADDR_MAX)) {
        return 0;
    }

    return -EINVAL;
}

STATIC int tool_parse_handle_read_reg_param(int argc, char **argv, struct scdr_reg_info *reg_info)
{
    char is_visited_flag[2] = {0}; // Indicates the subcommand settings, total num is 2.
    char *p_end_str = NULL;
    long reg_param;
    int i;
    int ret;

    if (argc != ARGC_NUM_2 && argc != ARGC_NUM_4) {
        roce_err("err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    for (i = 0; i < argc; i += STEP_TWO) {
        if ((strncmp(argv[i], "address", strlen("address") + 1) == 0) && (is_visited_flag[0] == 0)) {
            // 若argv[i + 1]为空则strncmp会报错
            ret = tool_check_read_reg_param_addr(argv[i + 1], reg_info, &is_visited_flag[0]);
            if (ret) {
                return ret;
            }
        } else if ((strncmp(argv[i], "nums", strlen("nums") + 1) == 0) && (is_visited_flag[INDEX_ONE] == 0)) {
            // 字符串转换为10进制
            reg_param = strtol(argv[i + 1], &p_end_str, NUMBER_BASE);
            if ((*p_end_str != '\0') || (reg_param <= 0)) {
                roce_err("The value of reg nums is wrong. [0 or %d]", MAX_REG_NUMS);
                return UDA_PARAM_INVALID_ERR;
            }
            reg_info->nums = (unsigned int)reg_param;
            is_visited_flag[INDEX_ONE] = 1;
        } else {
            roce_err("Invalid operation.");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }

    ret = tool_check_read_reg_param_valid(reg_info->address, reg_info->nums);
    if (ret) {
        return UDA_PARAM_INVALID_ERR;
    }

    return 0;
}

STATIC int tool_print_cdr_reg_info_last_row(struct scdr_reg_info *reg_info, unsigned int *address,
                                            unsigned int *value_index, char *addr, char *value)
{
    int ret;
    char *null_str = "NA";
    unsigned int num_rows = (reg_info->nums / MAX_SHOW_REG_INFO_COL_NUM) + 1;

    ret = sprintf_s(addr, TOOL_CDR_REG_BUF_LEN, "| address | ");
    if (ret < 0) {
        return ret;
    }
    ret = sprintf_s(value, TOOL_CDR_REG_BUF_LEN, "|  value  | ");
    if (ret < 0) {
        return ret;
    }

    while (*value_index < reg_info->nums) {
        ret = sprintf_s(addr + strlen(addr), TOOL_CDR_REG_INFO_MARGIN + strlen(addr), "0x%-10x | ", *address);
        if (ret < 0) {
            return ret;
        }
        ret = sprintf_s(value + strlen(value), TOOL_CDR_REG_INFO_MARGIN + strlen(value), "0x%-10x | ",
                        reg_info->value[*value_index]);
        if (ret < 0) {
            return ret;
        }
        *address += TOOL_CDR_REG_ADDR_OFFSET;
        (*value_index)++;
    }
    while (*value_index < (num_rows * MAX_SHOW_REG_INFO_COL_NUM)) {
        // 画剩余的边框
        ret = sprintf_s(addr + strlen(addr), TOOL_CDR_REG_INFO_MARGIN + strlen(addr), "%-12s | ", null_str);
        if (ret < 0) {
            return ret;
        }
        ret = sprintf_s(value + strlen(value), TOOL_CDR_REG_INFO_MARGIN + strlen(value), "%-12s | ", null_str);
        if (ret < 0) {
            return ret;
        }
        (*value_index)++;
    }

    TOOL_PRINT_INFO("%s", addr);
    TOOL_PRINT_INFO("%s", value);
    TOOL_PRINT_INFO("+=========+==============+==============+==============+==============+==============+");
    return 0;
}

STATIC int tool_show_cdr_reg_info(struct scdr_reg_info *reg_info)
{
    char addr[TOOL_CDR_REG_BUF_LEN];
    char value[TOOL_CDR_REG_BUF_LEN];
    unsigned int address = reg_info->address;
    unsigned int num_rows = reg_info->nums / MAX_SHOW_REG_INFO_COL_NUM;
    unsigned int row;
    unsigned int col;
    unsigned int value_index = 0;
    int ret = 0;

    TOOL_PRINT_INFO("The valid result of the reg read address from 0x%-8x to 0x%-8x is as follows:", reg_info->address,
                    reg_info->address + (reg_info->nums - 1) * TOOL_CDR_REG_ADDR_OFFSET);
    TOOL_PRINT_INFO("+====================================================================================+");
    TOOL_PRINT_INFO("| start address:0x%-10x\t\tnums:%-40u|", reg_info->address, reg_info->nums);
    TOOL_PRINT_INFO("+=========+==============+==============+==============+==============+==============+");

    for (row = 0; row < num_rows; ++row) {
        ret = sprintf_s(addr, TOOL_CDR_REG_BUF_LEN, "| address | ");
        if (ret < 0) {
            return ret;
        }
        ret = sprintf_s(value, TOOL_CDR_REG_BUF_LEN, "|  value  | ");
        if (ret < 0) {
            return ret;
        }
        for (col = 0; col < MAX_SHOW_REG_INFO_COL_NUM; ++col) {
            ret = sprintf_s(addr + strlen(addr), TOOL_CDR_REG_INFO_MARGIN + strlen(addr), "0x%-10x | ", address);
            if (ret < 0) {
                return ret;
            }
            ret = sprintf_s(value + strlen(value), TOOL_CDR_REG_INFO_MARGIN + strlen(value), "0x%-10x | ",
                            reg_info->value[value_index]);
            if (ret < 0) {
                return ret;
            }
            address += TOOL_CDR_REG_ADDR_OFFSET;
            value_index += 1;
        }

        TOOL_PRINT_INFO("%s", addr);
        TOOL_PRINT_INFO("%s", value);
        TOOL_PRINT_INFO("+=========+==============+==============+==============+==============+==============+");
    }

    if (reg_info->nums % MAX_SHOW_REG_INFO_COL_NUM) {
        ret = tool_print_cdr_reg_info_last_row(reg_info, &address, &value_index, addr, value);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

STATIC int tool_sd_cdr_handle_reg_cmd(int argc, char **argv, int logic_id)
{
    struct scdr_reg_info reg_info = {0};
    int ret;

    reg_info.nums = 1; // 默认值是1
    ret = tool_parse_handle_read_reg_param(argc, argv, &reg_info);
    if (ret != 0) {
        roce_err("Parse handle read reg param failed. (ret=%d)", ret);
        return ret;
    }

    ret = dsmi_handle_scdr_reg(logic_id, 0, &reg_info);
    if (ret != 0) {
        roce_err("Dsmi handle scdr reg failed. (ret=%d)", ret);
        if (ret == EBUSY) {
            return UDA_DSMI_GET_I2C_CTRL_ERR;
        } else if (ret == EOPNOTSUPP) {
            return UDA_DSMI_CDR_NOT_SUPPORT_ERROR;
        }
        return ret;
    }

    ret = tool_show_cdr_reg_info(&reg_info);
    if (ret != 0) {
        roce_err("Show cdr reg info failed. (ret=%d)", ret);
    }
    return 0;
}

STATIC int tool_sd_cdr_read_cdr_reg(int argc, char **argv, struct tool_param *param)
{
    int logic_id, ret;
    int argc_tmp = argc - 1;
    logic_id = param->logic_id;
    if (is_ATLAS_900_A3_SUPERPOD(param->mainboard_id)) {
        logic_id = ATLAS_900_A3_SUPERPOD_logic_id_convert(param->phy_id);
        if (logic_id == INVALID_LOGIC_ID) {
            roce_err("Get logic_id fail. (logic_id=%d)", logic_id);
            return UDA_DSMI_CONTROL_LINK_UNREACHABLE_ERR;
        }
    }
    ret = tool_sd_cdr_handle_reg_cmd(argc_tmp, argv + 1, logic_id);
    if (ret != 0) {
        roce_err("Read cdr reg fail. (ret = %d; logic_id = %d)", ret, logic_id);
        return ret;
    }
    return 0;
}

int tool_sd_cdr_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret, logic_id, board_type;
    int argc_tmp = argc - 1;

    ret = tool_get_board_type(param, &board_type);
    if (ret != 0) {
        roce_err("Tool sd cdr cmd get board type failed. (ret=%d)", ret);
        return UDA_DSMI_EXE_ERR;
    }

    if ((board_type != A_K_BOARD_TYPE) && (board_type != A_X_BOARD_TYPE) && (board_type != SUPERPOD_900_BOARD_TYPE)) {
        roce_err("Board_type:%d not support cdr.", board_type);
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    if (argc < ARGC_ID_1) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    logic_id = param->logic_id;
    if (!strncmp(argv[0], "-t", strlen("-t") + 1)) {
        ret = tool_set_cdr_mode_cmd(argc_tmp, argv + 1, param);
        if (ret != 0) {
            roce_err("Set cdr mode cmd fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_cdr_version(argc_tmp, param);
        if (ret != 0) {
            roce_err("get cdr version cmd fail. (ret=%d; logic_id=%d)", ret, logic_id);
            return ret;
        }
    } else if (!strncmp(argv[0], "-reg_read", strlen("-reg_read") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_sd_cdr_read_cdr_reg(argc, argv, param);
        if (ret) {
            return ret;
        }
    } else {
        roce_err("Invalid operation.\n)");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}

static int tool_get_scdr_reset_info(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct ds_scdr_reset_info scdr_reset_info = {0};

    if (argc != 0) {
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_scdr_reset_info(param->logic_id, &scdr_reset_info);
    if (ret == UDA_DSMI_CDR_NOT_SUPPORT_ERROR) {
        roce_err("Failed to get scdr reset info, retimer type not supported. (ret=%d, logic_id=%d)", ret,
                 param->logic_id);
        return UDA_DSMI_CDR_NOT_SUPPORT_ERROR;
    } else if (ret != 0) {
        roce_err("Failed to get scdr reset info. (ret=%d, logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    TOOL_PRINT_INFO("delay_time(s): %us", scdr_reset_info.delay_time);
    return 0;
}

static int tool_scdr_reset_delay_time_set(struct ds_scdr_reset_info *scdr_reset_info, struct tool_param *param)
{
    char tmp[PARAM_COUNT_THREE] = {'\0'};
    int ret;

    // 调用dsmi_set_scdr_reset_info函数，设置SCDR复位信息
    ret = dsmi_set_scdr_reset_info(param->logic_id, scdr_reset_info);
    if (ret == UDA_DSMI_CDR_NOT_SUPPORT_ERROR) {
        // 如果返回值为UDA_DSMI_CDR_NOT_SUPPORT_ERROR，表示不支持的cdr类型，需要打屏不支持信息
        roce_err("Failed to set scdr reset info, retimer type not supported.  (ret=%d, logic_id=%d)", ret,
                 param->logic_id);
        return UDA_DSMI_CDR_NOT_SUPPORT_ERROR;
    } else if (ret != 0) {
        // 其他非0场景， 回显dsmi函数执行失败
        roce_err("Failed to set scdr reset info. (ret=%d, logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    // 该项配置需要持久化
    // 将延迟时间转换为字符串格式，用于写入配置文件
    ret = sprintf_s(tmp, PARAM_COUNT_THREE, "%u", scdr_reset_info->delay_time);
    if (ret < 0) {
        roce_err("Failed to format delay time string. (ret=%d)", ret);
        return ret;
    }
    // 对于A3产品param中的id为转换后的主dieid，配置文件中记录主die的phy_id， 避免从die和主die分别记录不同的两个参数。
    ret = cfg_write_conf(param->phy_id, "scdr_reset_delay_time", tmp);
    if (ret != 0) {
        roce_err("Failed to write scdr reset delay time to config. (ret=%d; phy_id=%d)", ret, param->phy_id);
        return ret;
    }
    return UDA_EXE_SUCCESS;
}

static int tool_set_scdr_reset_info(int argc, char **argv, struct tool_param *param)
{
    struct ds_scdr_reset_info scdr_reset_info = {0};
    long int delay_time;
    char *endptr = NULL;
    int ret;

    if (argc != ARGC_NUM_2) {
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    // 入参校验处理
    if ((strncmp(argv[0], "delay_time", strlen("delay_time") + 1) == 0)) {
        delay_time = strtol(argv[ARGC_ID_1], &endptr, STR_TO_INT_LEN);
        if (*endptr != '\0' || delay_time > CDR_RESET_DELAY_TIME_MAX || delay_time < 0) {
            roce_err("Invalid delay_time parameter. (string=%s)", argv[ARGC_ID_1]);
            return UDA_PARAM_INVALID_ERR;
        }
        scdr_reset_info.delay_time = (unsigned char)delay_time;

        ret = tool_double_check(
            "[WARNING] Running this command will influence another device that shares the same CDR.\n"
            "Are you sure you want to continue?");
        if (ret != 0) {
            roce_err("Hccn tool scdr reset command aborted by user.");
            return ret;
        }
        return tool_scdr_reset_delay_time_set(&scdr_reset_info, param);
    } else {
        roce_err("Invalid operation. (str=%s)", argv[0]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
}

/**
 * @brief 判断是否支持A2和A3类型的cdr
 * @param param 工具参数
 * @return 如果支持返回true，否则返回false
 */
static bool is_cdr_supported_board_type_A2_A3(const struct tool_param *param)
{
    int board_type;
    int ret;

    ret = tool_get_board_type(param, &board_type);
    if (ret != 0) {
        // 如果获取失败，记录错误信息并返回不支持
        roce_err("Failed to get board type. (ret=%d)", ret);
        return false;
    }

    // 判断板类型是否为A_K_BOARD_TYPE、A_X_BOARD_TYPE或SUPERPOD_900_BOARD_TYPE
    if ((board_type != A_K_BOARD_TYPE) && (board_type != A_X_BOARD_TYPE) && (board_type != SUPERPOD_900_BOARD_TYPE)) {
        return false;
    }

    return true;
}

int tool_scdr_reset_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct tool_param temp_param;

    // 该命令在A1上屏蔽，需要检查是否支持cdr，拦截天工和A2pod/podc场景
    if (!is_cdr_supported_board_type_A2_A3(param)) {
        roce_err("Function not supported on this device.");
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    if (argc < ARGC_NUM_1 || argv == NULL) {
        roce_err("Invalid argument count. (argc=[%d])", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    // 复制param参数到临时变量， 用于id转换
    ret = memcpy_s(&temp_param, sizeof(struct tool_param), param, sizeof(struct tool_param));
    if (ret != 0) {
        roce_err("Failed to copy parameters. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    // A3需要做主从die id转换， A2的temp_param和原始param完全一致
    if (is_ATLAS_900_A3_SUPERPOD(param->mainboard_id)) {
        // A3双die场景仅主die有cdr控制权， 将操作转发到主die处理
        temp_param.phy_id = atlas_900_a3_superpod_phy_id_convert(param->phy_id);
        if (temp_param.phy_id != param->phy_id) {
            // 从die，需要做logic_id 转换，将转换后的id存储在temp_param
            ret = dsmi_get_logicid_from_phyid(temp_param.phy_id, &temp_param.logic_id);
            if (ret) {
                // 如果id获取失败，说明主die丢卡或其他异常
                roce_err("Failed to get logic_id from phy_id. (ret=%d; phy_id=%d; origin_phy_id=%d)", ret,
                         temp_param.phy_id, param->phy_id);
                return UDA_DSMI_CONTROL_LINK_UNREACHABLE_ERR;
            }
        }
    }

    // 检查命令行子命令字参数并执行相应操作
    if (strncmp(argv[0], "-g", strlen("-g") + 1) == 0) {
        param->get_cmd_flag = true;
        ret = tool_get_scdr_reset_info(argc - 1, argv + 1, &temp_param);
        if (ret != UDA_EXE_SUCCESS) {
            return ret;
        }
    } else if (strncmp(argv[0], "-s", strlen("-s") + 1) == 0) {
        param->get_cmd_flag = false;
        ret = tool_set_scdr_reset_info(argc - 1, argv + 1, &temp_param);
        if (ret != UDA_EXE_SUCCESS) {
            return ret;
        }
    } else {
        roce_err("Invalid operation for scdr_reset command.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return UDA_EXE_SUCCESS;
}

static int tool_get_scdr_reset_info_from_conf(struct tool_param *param, struct ds_scdr_reset_info *scdr_reset_info)
{
    char delay_time_str[CONLINE_LENGTH] = {0};
    long int delay_time_int;
    char *endptr = NULL;
    int ret;

    ret = cfg_read_conf(param->phy_id, "scdr_reset_delay_time", delay_time_str, CONLINE_LENGTH);
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，跳过配置
            roce_info("SCDR reset delay time configuration not found in config file., skipped. (ret=%d)", ret);
            return UDA_TOOL_SYS_RD_FILE_NOT_FOUND;
        } else { // 其他错误返回失败
            roce_err("cfg read conf failed. (name=scdr_reset_delay_time, ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    }
    // 将字符串转换为整数， 并校验参数是否在[0, CDR_RESET_DELAY_TIME_MAX]范围内
    delay_time_int = strtol(delay_time_str, &endptr, STR_TO_INT_LEN);
    if (*endptr != '\0' || delay_time_int > CDR_RESET_DELAY_TIME_MAX || delay_time_int < 0) {
        roce_err("Invalid delay_time value. (string=%s)", delay_time_str);
        return UDA_PARAM_INVALID_ERR;
    }
    scdr_reset_info->delay_time = (u8)delay_time_int;
    return 0;
}

static int tool_scdr_reset_delay_time_recovery_impl(struct tool_param *param)
{
    struct ds_scdr_reset_info scdr_reset_info = {0};
    struct tool_param temp_param;
    int ret;

    if (param == NULL) {
        roce_err("Tool scdr reset cmd get NULL parameter.");
        return UDA_PARAM_INVALID_ERR;
    }

    ret = memcpy_s(&temp_param, sizeof(struct tool_param), param, sizeof(struct tool_param));
    if (ret != 0) {
        roce_err("Failed to copy parameters. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    // A3需要做主从die id转换， A2的temp_param和原始param完全一致
    if (is_ATLAS_900_A3_SUPERPOD(param->mainboard_id)) {
        temp_param.phy_id = atlas_900_a3_superpod_phy_id_convert(param->phy_id);
        if (temp_param.phy_id != param->phy_id) {
            // 从die，需要做logic_id 转换，将转换后的id存储在temp_param
            ret = dsmi_get_logicid_from_phyid(temp_param.phy_id, &temp_param.logic_id);
            if (ret) {
                // 如果id获取失败，说明主die丢卡或其他异常，跳过这项配置
                roce_info("Cooperative device %d is abnormal, skipping configuration.", param->phy_id);
                return 0;
            }
        }
    }

    ret = tool_get_scdr_reset_info_from_conf(&temp_param, &scdr_reset_info);
    if (ret != 0) {
        return (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) ? 0 : ret; // 如果conf文件中没有对应配置项，跳过
    }
    return tool_scdr_reset_delay_time_set(&scdr_reset_info, &temp_param); // 执行配置恢复动作
}

int tool_scdr_reset_delay_time_recovery(struct tool_param *param)
{
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    // 该命令在A1上屏蔽，A2 A3场景需要检查是否支持cdr，跳过天工和A2 pod/podc场景
    if (!is_cdr_supported_board_type_A2_A3(param)) {
        roce_info("Function not supported on this device.");
        return 0;
    }
    return tool_scdr_reset_delay_time_recovery_impl(param);
#else
    return UDA_EXE_SUCCESS;
#endif
}

static int tool_print_scdr_esd(struct sds_esd_info_array *result, const int master_die_flag)
{
    int lane_id;
    int lane_num = ESD_MAX_DISPLAY_LANE / 2;
    int host_start_index = 0 + (1 - master_die_flag) * lane_num;
    int media_start_index = ESD_MAX_DISPLAY_LANE + (1 - master_die_flag) * lane_num;
    int i;

    // 分别检查host和media
    for (i = 0; i < lane_num; i++) {
        lane_id = host_start_index + i;
        if (result->info[lane_id].sds_init_flag == 0) {
            roce_err("result->info[%d].sds_init_flag=%d)", lane_id, result->info[lane_id].sds_init_flag);
            return UDA_DSMI_EXE_ERR; // about 0.1%
        }

        lane_id = media_start_index + i;
        if (result->info[lane_id].sds_init_flag == 0) {
            roce_err("result->info[%d].sds_init_flag=%d)", lane_id, result->info[lane_id].sds_init_flag);
            return UDA_DSMI_EXE_ERR; // about 0.1%
        }
    }

    TOOL_PRINT_INFO("The status of cdr serdes esd:");
    if (result->snr_pass == false) {
        TOOL_PRINT_INFO("[Warning] CDR RX signal detected during query, result may not be reliable.");
    }
    TOOL_PRINT_INFO("Type\t Index\t ESD Flag\t Voltage Offset");

    // 打印host
    for (i = 0; i < lane_num; i++) {
        lane_id = host_start_index + i;
        TOOL_PRINT_INFO("host\t %2d\t %s\t\t %2d", // 格式化数字对齐
                        i, (result->info[lane_id].sds_esd_flag == 1) ? "PASS" : "FAIL", result->info[lane_id].offset);
    }

    // 打印media
    for (i = 0; i < lane_num; i++) {
        lane_id = media_start_index + i;
        TOOL_PRINT_INFO("media\t %2d\t %s\t\t %2d", // 格式化数字对齐
                        i, (result->info[lane_id].sds_esd_flag == 1) ? "PASS" : "FAIL", result->info[lane_id].offset);
    }

    return 0;
}

static int tool_get_scdr_esd_cmd(int argc, char **argv, struct tool_param *param, const int master_die_flag)
{
    struct sds_esd_info_array result = {0};
    int ret;

    if (argc != 0) {
        roce_err("argc err. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_scdr_esd_status_cmd(param->logic_id, &result, sizeof(struct sds_esd_info_array));
    if (ret != 0) {
        roce_err("Failed to get scdr esd status. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret == EOPNOTSUPP ? UDA_DSMI_CDR_NOT_SUPPORT_ERROR : dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    ret = tool_print_scdr_esd(&result, master_die_flag);
    if (ret != 0) {
        roce_err("Failed to print scdr esd information. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    return 0;
}

int tool_scdr_esd_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id = param->logic_id;
    int master_die_flag = 1; // 默认为主die
    struct tool_param temp_param;

    if (argc < ARGC_NUM_1 || argv == NULL) {
        roce_err("Error argc. (argc=[%d])", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    // 只有天成、910A3_A+X 910A3_A+K款型支持
    if (!is_ATLAS_900_A3_SUPERPOD(param->mainboard_id)) {
        roce_err("This device does not support this function.");
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    // 天成中，DFX信息只有主DIE能设置cdr tx，从DIE的CDR命令会导向主DIE
    // A3双die场景仅主die有cdr控制权， 将操作转发到主die处理
    ret = memcpy_s(&temp_param, sizeof(struct tool_param), param, sizeof(struct tool_param));
    if (ret != 0) {
        roce_err("Memcpy failed! (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    temp_param.phy_id = atlas_900_a3_superpod_phy_id_convert(param->phy_id);
    if (temp_param.phy_id != param->phy_id) {
        // 从die，需要做logic_id 转换，将转换后的id存储在temp_param
        master_die_flag = 0;
        // master die display the front 4 lane for 0~7 npu or guest die display the rear 4 lane for 8~15 npu
        ret = dsmi_get_logicid_from_phyid(temp_param.phy_id, &temp_param.logic_id);
        if (ret) {
            // 如果id获取失败，说明主die丢卡或其他异常
            roce_err("Failed to get logic_id from phy_id. (ret=%d; phy_id=%d; origin_phy_id=%d)", ret,
                     temp_param.phy_id, param->phy_id);
            return UDA_DSMI_CONTROL_LINK_UNREACHABLE_ERR;
        }
    }

    // 检查命令行子命令字参数并执行相应操作
    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_double_check(
            "[WARNING] Running this command will influence another device that shares the same CDR, "
            "causing link down and related alerts.\n"
            "Are you sure you want to continue?");
        if (ret != 0) {
            roce_err("Hccn tool esd query command aborted by user.");
            return ret;
        }
        ret = tool_get_scdr_esd_cmd(argc - 1, argv + 1, &temp_param, master_die_flag);
        if (ret != 0) {
            roce_err("Get scdr esd cmd fail. (ret=%d; logic_id=%d, temp_logic_id=%d)", ret, logic_id,
                     temp_param.logic_id);
            return ret;
        }
    } else {
        roce_err("Invalid operation for scdr esd command.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return 0;
}
