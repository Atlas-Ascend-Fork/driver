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

#define TOOL_DIA_PAGE0_MIN 0
#define TOOL_DIA_PAGE_MAX 255
#define TOOL_DIA_PAGE_NOT0_MIN 128
#define TOOL_DIAG_I2C_PARAM_INDEX_I2C_MODE 0x0
#define TOOL_DIAG_I2C_PARAM_INDEX_SLAVE_OBJ 0x2
#define TOOL_DIAG_I2C_PARAM_INDEX_PAGE_IDX 0x4
#define TOOL_DIAG_I2C_PARAM_INDEX_REG_OFFSET 0x6
#define TOOL_DIAG_I2C_PARAM_INDEX_REG_SIZE 0x8
#define TOOL_DIAG_I2C_PARAM_INDEX_REG_VALUE 0xA
#define TOOL_DIAG_I2C_PARAM_MAX_REG_SIZE 0x4
#define DIAG_READ_PAGE_MAX_LEN 128
#define DIAG_READ_SIZE_MAX_LEN 128

#define TOOL_DIAG_BYTE_REVERSE_32(x) \
    ((((x) >> 24) & 0x000000FF) | (((x) >> 8) & 0x0000FF00) | (((x) << 8) & 0x00FF0000) | (((x) << 24) & 0xFF000000))

#define TOOL_DIAG_MAKE_MASK(x) (1 << ((x) - 1))

struct diag_slave_obj_table g_diag_slave_obj_table[] = {
    {"xsfp", DIAG_SLAVE_XSFP},
    {"cpld", DIAG_SLAVE_CPLD},
};

struct diag_i2c_mode_table g_diag_i2c_mode_table[] = {{"write", TOOL_DIAGNOSTIC_I2C_WRITE},
                                                      {"read", TOOL_DIAGNOSTIC_I2C_READ}};

char *g_diag_i2c_access_params_desc[] = {
    "i2c_mode", "object", "page", "offset", "size", "value",
};

void tool_show_diagnostic_mode_info(struct ds_diag_mode_params diag_mode_params)
{
    char *enable;
    unsigned long long remaining_seconds = diag_mode_params.remaining_seconds / TOOL_IMP_SOCIP_CLOCK;
    enable = (diag_mode_params.is_enabled == TOOL_DIAGNOSTIC_ENABLE) ? "on" : "off";
    TOOL_PRINT_INFO("Diagnostic mode: %s", enable);
    TOOL_PRINT_INFO("Configured duration: %d minutes", diag_mode_params.configured_duration_min);
    TOOL_PRINT_INFO("Remaining time: %u minutes %u seconds", remaining_seconds / TOOL_ONE_MINUTES_SECONDS,
                    remaining_seconds % TOOL_ONE_MINUTES_SECONDS);
}

STATIC int tool_get_diagnostic_info(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct ds_diag_mode_params ds_diag_mode_params = {0};

    if (argc > 0) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ds_diag_mode_params.operation = TOOL_DIAGNOSTIC_MODE_GET;

    ret = dsmi_operate_diagnostic_mode(param->logic_id, &ds_diag_mode_params, DS_GET_DIAGNOSTIC_MODE);
    if (ret != 0) {
        roce_err("Failed to get diagnostic mode. (ret=%d, logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    tool_show_diagnostic_mode_info(ds_diag_mode_params);

    return UDA_EXE_SUCCESS;
}

STATIC int tool_diagnostic_mode_params_set_is_enabled(char **argv, int index,
                                                      struct ds_diag_mode_params *diag_mode_params)
{
    if (strncmp(argv[index], "on", strlen("on") + 1) == 0) {
        diag_mode_params->is_enabled = TOOL_DIAGNOSTIC_ENABLE;
    } else if (strncmp(argv[index], "off", strlen("off") + 1) == 0) {
        diag_mode_params->is_enabled = TOOL_DIAGNOSTIC_DISABLE;
    } else {
        roce_err("The value of diagnostic enable %s is wrong. [on or off]", argv[index]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return 0;
}

STATIC int tool_parse_diagnostic_mode_params(int argc, char **argv, struct ds_diag_mode_params *diag_mode_params)
{
    char flag[2] = {0}; // Indicates the subcommand settings, total num is 2.
    char *p_tmp = NULL;
    long tmp;
    int i, ret;

    // 设置无效值，imp侧判断如果为无效值，则判断之前是否设置过，如果设置过，则使用之前设置的值，未设置过则为60min
    diag_mode_params->is_enabled = TOOL_DIAGNOSTIC_INVALID_PARAM;
    diag_mode_params->duration_min = TOOL_DIAGNOSTIC_INVALID_PARAM;

    if (((argc % PARAM_COUNT_TWO) != 0) || (argc < PARAM_COUNT_TWO) || (argc > PARAM_COUNT_FOUR)) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    diag_mode_params->operation = TOOL_DIAGNOSTIC_MODE_SET;

    for (i = 0; i < argc; i += STEP_TWO) {
        if ((strncmp(argv[i], "enable", strlen("enable") + 1) == 0) && (flag[0] == 0)) {
            ret = tool_diagnostic_mode_params_set_is_enabled(argv, i + 1, diag_mode_params);
            if (ret != 0) {
                return ret;
            }
            flag[0] = 1; // Indicates that the field has been set.
        } else if ((strncmp(argv[i], "duration", strlen("duration") + 1) == 0) && (flag[INDEX_ONE] == 0)) {
            tmp = strtol(argv[i + 1], &p_tmp, NUMBER_BASE);
            if ((*p_tmp != '\0') || (tmp < TOOL_MIN_DIAGNOSTIC_DUTRTION_TIME) ||
                (tmp > TOOL_MAX_DIAGNOSTIC_DUTRTION_TIME)) {
                roce_err("The value of duration time is wrong. [%d ~ %d]", TOOL_MIN_DIAGNOSTIC_DUTRTION_TIME,
                         TOOL_MAX_DIAGNOSTIC_DUTRTION_TIME);
                return UDA_PARAM_INVALID_ERR;
            }
            diag_mode_params->duration_min = (int)tmp;
            flag[INDEX_ONE] = 1; // Indicates that the field has been set.
        } else {
            roce_err("Invalid operation.)");
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }

    return 0;
}

int tool_check_diagnostic_i2c_access_params(int argc, char **argv)
{
    int i;
    int j;

    if (((argc % PARAM_COUNT_TWO) != 0) || (argc < PARAM_COUNT_TEN) || (argc > PARAM_COUNT_TWELVE)) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (strncmp(argv[INDEX_ONE], "write", strlen("write") + 1) == 0) {
        if (argc == PARAM_COUNT_TWELVE && strncmp(argv[INDEX_TEN], "value", strlen("value") + 1) != 0) {
            roce_err("Err argv. (argv=%s, desc=value)", argv[INDEX_TEN]);
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }

    for (i = 0, j = 0; i < argc - PARAM_COUNT_TWO; i += STEP_TWO, j += STEP_ONE) {
        if (strncmp(argv[i], g_diag_i2c_access_params_desc[j], strlen(g_diag_i2c_access_params_desc[j]) + 1) != 0) {
            roce_err("Err argv. (argv=%s, desc=%s)", argv[i], g_diag_i2c_access_params_desc[j]);
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }
    return 0;
}

STATIC int tool_check_and_set_diagnostic_i2c_access_page_index(char **argv, struct i2c_access_params *i2c_access_params,
                                                               unsigned char *page_idx)
{
    char *p_tmp = NULL;
    long page_temp;

    page_temp = strtol(argv[TOOL_DIAG_I2C_PARAM_INDEX_PAGE_IDX + 1], &p_tmp, 0);
    if ((*p_tmp != '\0') || (page_temp < 0) || (page_temp > TOOL_MAX_DIAGNOSTIC_XSFP_PAGE_NUMS)) {
        roce_err("The value of page index is wrong. [0 ~ %d]", TOOL_MAX_DIAGNOSTIC_XSFP_PAGE_NUMS);
        return UDA_PARAM_INVALID_ERR;
    }
    *page_idx = (unsigned char)page_temp;
    i2c_access_params->xsfp.page_idx = *page_idx;
    return 0;
}

STATIC int tool_check_and_set_diagnostic_i2c_access_reg_offset(char **argv, struct i2c_access_params *i2c_access_params,
                                                               unsigned char *reg_offset)
{
    char *p_tmp = NULL;
    long offset_temp;

    offset_temp = strtol(argv[TOOL_DIAG_I2C_PARAM_INDEX_REG_OFFSET + 1], &p_tmp, 0);
    // reg_offset无法单独判断，在后续与reg_size一起判断, 此处仅检查offset是否符合自身合法范围
    if ((*p_tmp != '\0') || (offset_temp < 0) || (offset_temp > TOOL_DIA_PAGE_MAX)) {
        roce_err("The value of reg offset is wrong");
        return UDA_PARAM_INVALID_ERR;
    }
    *reg_offset = (unsigned char)offset_temp;
    i2c_access_params->xsfp.reg_offset = *reg_offset;
    return 0;
}

STATIC int tool_check_diagnostic_i2c_access_write_reg_size(unsigned char page_idx, unsigned char reg_size,
                                                           unsigned char reg_offset, unsigned char reg_offset_min,
                                                           unsigned char reg_offset_max)
{
    // 写操作时，reg_size范围为1~4,reg_offset范围为min~max(page0 0~255, other page 128~255)
    // 所以reg_offset+reg_size-1需要<=max，并且page0的118~125为光模块的电子标签，所以reg_offset和reg_offset+reg_size-1不可以在118~125范围内
    // 为了清晰描述，if分开来描述
    if (!(reg_size >= 0x1 && reg_size <= 0x4 && reg_offset >= reg_offset_min && reg_offset <= reg_offset_max &&
          (reg_size + reg_offset - 1) <= reg_offset_max)) {
        roce_err("The value of write reg size is wrong.");
        return UDA_PARAM_INVALID_ERR;
    }
    if (page_idx == 0 && (reg_size + reg_offset - 1) >= TOOL_DIAGNOSTIC_OPTICAL_REG_OFFSET_LABEL_START &&
        (reg_size + reg_offset - 1) <= TOOL_DIAGNOSTIC_OPTICAL_REG_OFFSET_LABEL_END) {
        roce_err("The value of write page 0 reg size is wrong.");
        return UDA_PARAM_INVALID_ERR;
    }
    return 0;
}

STATIC int tool_check_diagnostic_i2c_access_read_reg_size(unsigned char reg_size, unsigned char reg_offset,
                                                          unsigned char reg_offset_min, unsigned char reg_offset_max)
{
    // 读操作时，reg_size范围为1~127,reg_offset范围为min~max(page0 0~255, other page 128~255)
    // 所以reg_offset+reg_size-1需要<=max
    // 为了清晰描述，if分开来描述
    if (!(reg_size >= 0x1 && reg_size <= DIAG_READ_SIZE_MAX_LEN && reg_offset >= reg_offset_min &&
          reg_offset <= reg_offset_max && (reg_size + reg_offset - 1) <= reg_offset_max)) {
        roce_err("The value of read reg size is wrong.");
        return UDA_PARAM_INVALID_ERR;
    }
    return 0;
}

STATIC int tool_check_and_set_diagnostic_i2c_access_reg_size(char **argv, struct i2c_access_params *i2c_access_params,
                                                             int i2c_mode, unsigned char page_idx,
                                                             unsigned char reg_offset)
{
    char *p_tmp = NULL;
    unsigned char reg_size, reg_offset_min, reg_offset_max;
    int ret;

    reg_offset_min = page_idx == 0 ? TOOL_DIA_PAGE0_MIN : TOOL_DIA_PAGE_NOT0_MIN;
    reg_offset_max = TOOL_DIA_PAGE_MAX;

    reg_size = strtoul(argv[TOOL_DIAG_I2C_PARAM_INDEX_REG_SIZE + 1], &p_tmp, 0);
    if (*p_tmp != '\0') {
        roce_err("The value of reg size is wrong.");
        return UDA_PARAM_INVALID_ERR;
    }
    if (i2c_mode == TOOL_DIAGNOSTIC_I2C_WRITE) {
        ret = tool_check_diagnostic_i2c_access_write_reg_size(page_idx, reg_size, reg_offset, reg_offset_min,
                                                              reg_offset_max);
    }
    if (i2c_mode == TOOL_DIAGNOSTIC_I2C_READ) {
        ret = tool_check_diagnostic_i2c_access_read_reg_size(reg_size, reg_offset, reg_offset_min, reg_offset_max);
    }
    if (ret != 0) {
        return ret;
    }

    i2c_access_params->xsfp.reg_size = reg_size;
    return 0;
}

STATIC int tool_check_and_set_diagnostic_i2c_access_reg_value(char **argv, struct i2c_access_params *i2c_access_params)
{
    unsigned long reg_value;
    char *p_tmp = NULL;

    reg_value = strtoul(argv[TOOL_DIAG_I2C_PARAM_INDEX_REG_VALUE + 1], &p_tmp, 0);
    if ((*p_tmp != '\0') || (reg_value < 0) || (reg_value > TOOL_MAX_UINT_BYTES(i2c_access_params->xsfp.reg_size))) {
        roce_err("The value of reg value is wrong. [0 ~ %d]", TOOL_MAX_UINT_BYTES(i2c_access_params->xsfp.reg_size));
        return UDA_PARAM_INVALID_ERR;
    }
    // 按1字节读取
    i2c_access_params->xsfp.reg_value[0x0] = reg_value & 0xFF;
    i2c_access_params->xsfp.reg_value[0x1] = (reg_value >> 0x8) & 0xFF;
    i2c_access_params->xsfp.reg_value[0x2] = (reg_value >> 0x10) & 0xFF;
    i2c_access_params->xsfp.reg_value[0x3] = (reg_value >> 0x18) & 0xFF;
    return 0;
}

STATIC int tool_check_and_set_diagnostic_i2c_access_params(int argc, char **argv,
                                                           struct i2c_access_params *i2c_access_params,
                                                           struct tool_param *param)
{
    int index, ret = 0;
    static unsigned char page_idx, reg_offset;

    if (((argc % PARAM_COUNT_TWO) != 0) || ((argc != PARAM_COUNT_TEN) && (argc != PARAM_COUNT_TWELVE))) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (tool_is_optical_supported_board_type(param)) {
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }

    for (index = 0; index < argc; index += STEP_TWO) {
        if (strncmp(argv[index], g_diag_i2c_access_params_desc[index / STEP_TWO],
                    strlen(g_diag_i2c_access_params_desc[index / STEP_TWO]) + 1) != 0) {
            roce_err("Err argv. (argv=%s, desc=%s)", argv[index], g_diag_i2c_access_params_desc[index / STEP_TWO]);
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
        switch (index) {
            case TOOL_DIAG_I2C_PARAM_INDEX_I2C_MODE:
                break;
            case TOOL_DIAG_I2C_PARAM_INDEX_SLAVE_OBJ:
                break;
            case TOOL_DIAG_I2C_PARAM_INDEX_PAGE_IDX:
                ret = tool_check_and_set_diagnostic_i2c_access_page_index(argv, i2c_access_params, &page_idx);
                break;
            case TOOL_DIAG_I2C_PARAM_INDEX_REG_OFFSET:
                ret = tool_check_and_set_diagnostic_i2c_access_reg_offset(argv, i2c_access_params, &reg_offset);
                break;
            case TOOL_DIAG_I2C_PARAM_INDEX_REG_SIZE:
                ret = tool_check_and_set_diagnostic_i2c_access_reg_size(
                    argv, i2c_access_params, i2c_access_params->i2c_mode, page_idx, reg_offset);
                break;
            case TOOL_DIAG_I2C_PARAM_INDEX_REG_VALUE:
                ret = tool_check_and_set_diagnostic_i2c_access_reg_value(argv, i2c_access_params);
                break;
            default:
                roce_err("Invalid index. (index=%d).", index);
                ret = UDA_PARAM_INVALID_ERR;
                break;
        }
        if (ret) {
            return ret;
        }
    }

    return 0;
}

STATIC int tool_cpld_diagnostic_param_parse(int argc, struct i2c_access_params *i2c_access_params,
                                            struct tool_param *param)
{
    if (i2c_access_params->i2c_mode == TOOL_DIAGNOSTIC_I2C_WRITE) {
        roce_err("cpld diag support read only.");
        return UDA_PARAM_INVALID_ERR;
    }
    // 当前需求规定CPLD读命令仅支持A3调用，后续再放开A2吧
    if (!is_ATLAS_900_A3_SUPERPOD(param->mainboard_id)) {
        roce_err("cpld diag support A3 only.");
        return UDA_TOOL_CMD_UNSUPPORT_ON_PRODUCT_ERR;
    }
    if (argc > 0) {
        roce_err("cpld diagnostic cmd need no sub params.");
        return UDA_PARAM_CMD_COUNT_ERR;
    }
    return 0;
}

STATIC int tool_parse_diagnostic_i2c_access_params(int argc, char **argv, struct i2c_access_params *i2c_access_params,
                                                   struct tool_param *param)
{
    int i, ret;

    // 至少应有 i2c_mode read/write object xsfp/cpld
    if (argc < ARGC_ID_4) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (strncmp(argv[TOOL_DIAG_I2C_PARAM_INDEX_I2C_MODE + 1], "write", strlen("write") + 1) == 0) {
        i2c_access_params->i2c_mode = TOOL_DIAGNOSTIC_I2C_WRITE;
    } else if (strncmp(argv[TOOL_DIAG_I2C_PARAM_INDEX_I2C_MODE + 1], "read", strlen("read") + 1) == 0) {
        i2c_access_params->i2c_mode = TOOL_DIAGNOSTIC_I2C_READ;
    } else {
        roce_err("Invalid operation. (operation=%s)", argv[INDEX_ONE]);
        return UDA_PARAM_INVALID_ERR;
    }

    for (i = 0; i < DIAG_SLAVE_MAX; i++) {
        if (strncmp(argv[TOOL_DIAG_I2C_PARAM_INDEX_SLAVE_OBJ + 1], g_diag_slave_obj_table[i].slave_str,
                    strlen(g_diag_slave_obj_table[i].slave_str) + 1) == 0) {
            i2c_access_params->slave_obj = g_diag_slave_obj_table[i].slave_obj;
            break;
        }
    }

    if (i == DIAG_SLAVE_MAX) {
        roce_err("Invalid slave obj. (slave obj=%s)", argv[TOOL_DIAG_I2C_PARAM_INDEX_SLAVE_OBJ + 1]);
        return UDA_PARAM_INVALID_ERR;
    }

    switch (i2c_access_params->slave_obj) {
        case DIAG_SLAVE_XSFP:
            ret = tool_check_and_set_diagnostic_i2c_access_params(argc, argv, i2c_access_params, param);
            break;
        case DIAG_SLAVE_CPLD:
            ret = tool_cpld_diagnostic_param_parse(argc - ARGC_ID_4, i2c_access_params, param);
            break;
        default:
            roce_err("Invalid slave obj. (slave obj=%d)", i2c_access_params->slave_obj);
            ret = UDA_PARAM_INVALID_ERR;
    }

    return ret;
}

STATIC int tool_set_diagnostic_mode_cmd(int logic_id, struct ds_diag_mode_params *diag_mode_params)
{
    int ret;

    ret = dsmi_operate_diagnostic_mode(logic_id, diag_mode_params, DS_SET_DIAGNOSTIC_MODE);
    if (ret != 0) {
        roce_err("Failed to set diagnostic mode. (ret=%d, logic_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return UDA_EXE_SUCCESS;
}

STATIC int tool_sprintf_xsfp_diagnostic_i2c_access_col(int index, struct i2c_access_params i2c_access_params,
                                                       unsigned char *value, unsigned int *value_index)
{
    int col = 0;
    int ret;

    for (col = 0; col < SHOW_ONE_ROW && (index + col) < i2c_access_params.xsfp.reg_size; col++) {
        ret = sprintf_s(value + strlen(value), TOOL_DUMP_REG_MARGIN + strlen(value), " %02x",
                        i2c_access_params.xsfp.reg_value[*value_index]);
        if (ret < 0) {
            roce_err("sprintf_s cmd fail. ret[%d]", ret);
            return ret;
        }
        *value_index += 1;
    }
    return 0;
}

STATIC int tool_show_xsfp_diagnostic_i2c_access_result(struct i2c_access_params i2c_access_params)
{
    int i;
    int ret;
    int byte_no = i2c_access_params.xsfp.reg_offset;
    unsigned int value_index = 0;
    unsigned char value[DUMP_PAGE_LEN];
    if (i2c_access_params.i2c_mode == TOOL_DIAGNOSTIC_I2C_WRITE) {
        return 0;
    }
    if (i2c_access_params.i2c_mode == TOOL_DIAGNOSTIC_I2C_READ) {
        roce_info("Read i2c access result."
                  "(slave_obj=%u, page_idx=0x%x, reg_offset=0x%x, reg_size=%u)",
                  i2c_access_params.slave_obj, i2c_access_params.xsfp.page_idx, i2c_access_params.xsfp.reg_offset,
                  i2c_access_params.xsfp.reg_size);
        TOOL_PRINT_INFO("Page %xh Memory : %u - %u", i2c_access_params.xsfp.page_idx, i2c_access_params.xsfp.reg_offset,
                        i2c_access_params.xsfp.reg_offset + i2c_access_params.xsfp.reg_size - 1);
        for (i = 0; i < i2c_access_params.xsfp.reg_size; i += SHOW_ONE_ROW) {
            ret = sprintf_s(value, DUMP_PAGE_LEN, "[%03d]:", byte_no);
            if (ret < 0) {
                roce_err("sprintf_s cmd fail. ret[%d]", ret);
                return ret;
            }
            ret = tool_sprintf_xsfp_diagnostic_i2c_access_col(i, i2c_access_params, value, &value_index);
            if (ret != 0) {
                return ret;
            }
            TOOL_PRINT_INFO("%s", value);
            byte_no += SHOW_ONE_ROW;
        }
    }
    return 0;
}

static void tool_show_cpld_diag_header(void)
{
    TOOL_PRINT_INFO("+-------------------------------------------------------------------------------+");
    TOOL_PRINT_INFO("|                                   CPLD INFO                                   |");
    TOOL_PRINT_INFO("+---------+------+------+------+------+------+------+------+------+------+------+");
}

// 这部分所有安全函数预期都不会出错，只是为了cleancode才处理返回值
static int tool_show_cpld_diag_add_element(char *line_addr, char *line_value, unsigned char addr, unsigned char value,
                                           int length)
{
    int ret;
    ret = sprintf_s(line_addr + strlen(line_addr), length - strlen(line_addr), " 0x%02x |", addr) < 0 ? 1 : 0;
    ret += sprintf_s(line_value + strlen(line_value), length - strlen(line_value), " 0x%02x |", value) < 0 ? 1 : 0;
    return ret;
}

static int tool_show_cpld_diag_print_line(char *addr, char *value, int length, int num)
{
    int ret, temp = num;
    while (temp++ % DIAG_CPLD_ROW_LEN) {
        ret = sprintf_s(addr + strlen(addr), length - strlen(addr), "  --  |") < 0 ? 1 : 0;
        ret += sprintf_s(value + strlen(value), length - strlen(addr), "  --  |") < 0 ? 1 : 0;
        if (ret) {
            return ret;
        }
    }
    TOOL_PRINT_INFO("%s", addr);
    TOOL_PRINT_INFO("%s", value);
    TOOL_PRINT_INFO("+---------+------+------+------+------+------+------+------+------+------+------+");
    ret = strcpy_s(addr, length, "| address |");
    ret += strcpy_s(value, length, "|  value  |");
    return ret;
}

static int tool_show_cpld_diagnostic_result_A3(struct i2c_access_params *data)
{
    int ret;
    int index, col = 0;
    char addr[DIAG_CPLD_STR_LEN];
    char value[DIAG_CPLD_STR_LEN];
    tool_show_cpld_diag_header();

    ret = strcpy_s(addr, DIAG_CPLD_STR_LEN, "| address |");
    ret += strcpy_s(value, DIAG_CPLD_STR_LEN, "|  value  |");
    if (ret) {
        return ret;
    }

    for (index = 0; index < A3_CPLD_DIAG_SIZE; index++) {
        ret = tool_show_cpld_diag_add_element(addr, value, index + A3_CPLD_DIAG_OFFSET, data->cpld.val[index],
                                              DIAG_CPLD_STR_LEN);
        if (ret) {
            return ret;
        }
        col++;
        if ((col % DIAG_CPLD_ROW_LEN) == 0) {
            ret = tool_show_cpld_diag_print_line(addr, value, DIAG_CPLD_STR_LEN, 0);
            if (ret) {
                return ret;
            }
        }
    }
    ret += tool_show_cpld_diag_print_line(addr, value, DIAG_CPLD_STR_LEN, col);
    return ret;
}

static int tool_show_cpld_diagnostic_result_A2(struct i2c_access_params *data)
{
    int ret;
    int index = 0, col = 0;
    char addr[DIAG_CPLD_STR_LEN];
    char value[DIAG_CPLD_STR_LEN];

    tool_show_cpld_diag_header();
    ret = strcpy_s(addr, DIAG_CPLD_STR_LEN, "| address |");
    ret += strcpy_s(value, DIAG_CPLD_STR_LEN, "|  value  |");
    if (ret) {
        return ret;
    }

    ret += tool_show_cpld_diag_add_element(addr, value, A2_CPLD_SPECIAL_REG1, data->cpld.val[index++],
                                           DIAG_CPLD_STR_LEN);
    ret += tool_show_cpld_diag_add_element(addr, value, A2_CPLD_SPECIAL_REG2, data->cpld.val[index++],
                                           DIAG_CPLD_STR_LEN);
    ret += tool_show_cpld_diag_print_line(addr, value, DIAG_CPLD_STR_LEN, index);
    if (ret) {
        return ret;
    }

    for (col = 0; col < A2_CPLD_DIAG_SIZE1; col++) {
        ret += tool_show_cpld_diag_add_element(addr, value, A2_CPLD_DIAG_OFFSET1 + col, data->cpld.val[index++],
                                               DIAG_CPLD_STR_LEN);
    }
    ret += tool_show_cpld_diag_print_line(addr, value, DIAG_CPLD_STR_LEN, col);

    for (col = 0; col < A2_CPLD_DIAG_SIZE2; col++) {
        ret += tool_show_cpld_diag_add_element(addr, value, A2_CPLD_DIAG_OFFSET2 + col, data->cpld.val[index++],
                                               DIAG_CPLD_STR_LEN);
    }
    ret += tool_show_cpld_diag_print_line(addr, value, DIAG_CPLD_STR_LEN, col);

    for (col = 0; col < A2_CPLD_DIAG_SIZE3; col++) {
        ret += tool_show_cpld_diag_add_element(addr, value, A2_CPLD_DIAG_OFFSET3 + col, data->cpld.val[index++],
                                               DIAG_CPLD_STR_LEN);
    }
    ret += tool_show_cpld_diag_print_line(addr, value, DIAG_CPLD_STR_LEN, col);

    return ret;
}

static int tool_show_cpld_diagnostic_result(struct i2c_access_params *data, int is_910A3)
{
    int ret;

    if (data->i2c_mode == TOOL_DIAGNOSTIC_I2C_WRITE) {
        return 0;
    }

    if (is_910A3) {
        ret = tool_show_cpld_diagnostic_result_A3(data);
    } else {
        ret = tool_show_cpld_diagnostic_result_A2(data);
    }
    if (ret) {
        roce_err("tool show cpld diag result failed. (ret=%d, is_910A3=%d)", ret, is_910A3);
        ret = UDA_TOOL_NO_MEM_ERR;
    }
    return ret;
}

int tool_show_diagnostic_i2c_access_result(char **argv, struct i2c_access_params i2c_access_params, int is_910A3)
{
    int i, slave_obj, ret = 0;
    for (i = 0; i < DIAG_SLAVE_MAX; i++) {
        if (strncmp(argv[TOOL_DIAG_I2C_PARAM_INDEX_SLAVE_OBJ + 1], g_diag_slave_obj_table[i].slave_str,
                    strlen(g_diag_slave_obj_table[i].slave_str) + 1) == 0) {
            slave_obj = g_diag_slave_obj_table[i].slave_obj;
            break;
        }
    }

    if (i == DIAG_SLAVE_MAX) {
        roce_err("Invalid slave obj. (slave obj=%s)", argv[TOOL_DIAG_I2C_PARAM_INDEX_SLAVE_OBJ + 1]);
        return UDA_PARAM_INVALID_ERR;
    }

    if (slave_obj == DIAG_SLAVE_XSFP) {
        ret = tool_show_xsfp_diagnostic_i2c_access_result(i2c_access_params);
    } else if (slave_obj == DIAG_SLAVE_CPLD) {
        ret = tool_show_cpld_diagnostic_result(&i2c_access_params, is_910A3);
    }

    return ret;
}

STATIC int tool_set_diagnostic_mode_handle(int argc, char **argv, struct tool_param *param)
{
    int ret;
    struct ds_diag_mode_params diag_mode_params = {0};
    int logic_id;

    ret = tool_double_check(
        "[WARNING]Executing this command will activate diagnostic mode. "
        "High-risk operations in diagnostic mode may affect the fundamental functions of the network. "
        "After performing operations on the optical module, "
        "please reset the optical module to ensure proper operation.\n"
        "Are you sure you want to continue?");
    if (ret != 0) {
        roce_err("Hccn tool set diagnostic mode command aborted by user.");
        return ret;
    }
    ret = tool_parse_diagnostic_mode_params(argc, argv, &diag_mode_params);
    if (ret != 0) {
        roce_err("Parse diagnostic mode param return error. (ret=%d)\n", ret);
        return ret;
    }
    ret = tool_set_diagnostic_mode_cmd(param->logic_id, &diag_mode_params);
    if (ret != 0) {
        roce_err("Tool set diagnostic mode fail. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return ret;
    }
    return ret;
}

STATIC int tool_set_diagnostic_i2c_access_handle(int argc, char **argv, struct tool_param *param)
{
    int ret;
    int logic_id = param->logic_id;
    struct i2c_access_params i2c_access_params = {0};
    int is_910A3 = is_ATLAS_900_A3_SUPERPOD(param->mainboard_id);

    ret = tool_parse_diagnostic_i2c_access_params(argc, argv, &i2c_access_params, param);
    if (ret != 0) {
        roce_err("Parse diagnostic i2c access param return error. (ret=%d)\n", ret);
        return ret;
    }

    // A3上的光模块访问由主控DIE代理，CPLD访问不做转发
    if (is_910A3 && i2c_access_params.slave_obj == DIAG_SLAVE_XSFP) {
        logic_id = ATLAS_900_A3_SUPERPOD_logic_id_convert(param->phy_id);
        i2c_access_params.xsfp.guest_mode = (logic_id != param->logic_id);
    }

    ret = dsmi_set_diagnostic_i2c_access_cmd(logic_id, &i2c_access_params, DS_SET_DIAGNOSTIC_I2C_ACCESS);
    if (ret != 0) {
        roce_err("Dsmi set diagnostic mode fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    if (i2c_access_params.result != 0) {
        roce_err("Tool set diagnostic mode fail. (result=%d; logic_id=%d)", i2c_access_params.result, logic_id);
        if (i2c_access_params.slave_obj == DIAG_SLAVE_XSFP || i2c_access_params.slave_obj == DIAG_SLAVE_CPLD) {
            // CPLD先借用光模块的回显解析框架，后续整改
            return tool_dump_optical_info_ret_convert(i2c_access_params.result);
        } else {
            return UDA_PARAM_OP_NOT_SUPPORT_ERR;
        }
    }

    ret = tool_show_diagnostic_i2c_access_result(argv, i2c_access_params, is_910A3);
    if (ret != 0) {
        roce_err("Tool set diagnostic mode fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }
    return 0;
}

STATIC int tool_set_diagnostic_info(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc < 0x1) {
        roce_err("Err argc. (argc=%d)", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "enable", strlen("enable") + 1) || !strncmp(argv[0], "duration", strlen("duration") + 1)) {
        ret = tool_set_diagnostic_mode_handle(argc, argv, param);
        if (ret != 0) {
            return ret;
        }
    } else if (!strncmp(argv[0], "i2c_mode", strlen("i2c_mode") + 1)) {
        ret = tool_set_diagnostic_i2c_access_handle(argc, argv, param);
        if (ret != 0) {
            return ret;
        }
    } else {
        roce_err("Invalid operation for diagnostic command.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return UDA_EXE_SUCCESS;
}

int tool_diagnostic_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc < ARGC_NUM_1 || argv == NULL) {
        roce_err("Invalid argument count. (argc=[%d])", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = tool_is_optical_supported_board_type(param);
    if (ret) {
        return ret;
    }

    // 检查命令行子命令字参数并执行相应操作
    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_diagnostic_info(argc - 1, argv + 1, param);
        if (ret != UDA_EXE_SUCCESS) {
            return ret;
        }
    } else if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        param->get_cmd_flag = false;
        ret = tool_set_diagnostic_info(argc - 1, argv + 1, param);
        if (ret != UDA_EXE_SUCCESS) {
            return ret;
        }
    } else {
        roce_err("Invalid operation for diagnostic command.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }
    return UDA_EXE_SUCCESS;
}
