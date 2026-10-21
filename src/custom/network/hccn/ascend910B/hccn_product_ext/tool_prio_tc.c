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
#include "tool_recovery.h"
#include "tool_ext.h"
#include "cmd.h"
#include "dsmi_common_interface.h"
#include "tool.h"
#include "log.h"

STATIC int tool_get_prio_tc_info(int argc, char **argv, struct tool_param *param)
{
    int ret;
    char prio_tc[PRIORITY_NUM] = {0};
    if (argc != 1) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    ret = dsmi_get_prio_tc_info(param->logic_id, 0, prio_tc);
    if (ret) {
        roce_err("Dsmi get the mapping between priority and TC failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    TOOL_PRINT_INFO("Priority mapping:");
    TOOL_PRINT_INFO("priority   0 1 2 3 4 5 6 7");
    TOOL_PRINT_INFO("TC         %d %d %d %d %d %d %d %d", prio_tc[0x0], prio_tc[0x1], prio_tc[0x2], prio_tc[0x3],
                    prio_tc[0x4], prio_tc[0x5], prio_tc[0x6], prio_tc[0x7]);
    return 0;
}

STATIC int tool_prio_tc_str2char(const char prio_tc_str[], int str_len, char prio_tc[PRIORITY_NUM])
{
    int str_i;
    int priority_i = 0;

#define PRIO_TC_CONF_INDEX 2
    for (str_i = 0; str_i < str_len && priority_i < PRIORITY_NUM; str_i++) {
        if (str_i % PRIO_TC_CONF_INDEX == 1) {
            if (prio_tc_str[str_i] != ',') {
                roce_err("Tool_prio_tc_str2char contain invalid char[%c]", prio_tc_str[str_i]);
                return UDA_PARAM_INVALID_ERR;
            }
        } else if ((str_i % PRIO_TC_CONF_INDEX == 0) && ((prio_tc_str[str_i] < '0') || (prio_tc_str[str_i] > '3'))) {
            roce_err("Tool_prio_tc_str2char contain invalid char[%c]", prio_tc_str[str_i]);
            return UDA_PARAM_INVALID_ERR;
        } else {
            prio_tc[priority_i] = prio_tc_str[str_i];
            priority_i++;
        }
    }

    return 0;
}

STATIC int tool_set_prio_tc_info(int argc, char **argv, struct tool_param *param)
{
    int ret;
    char prio_tc[PRIORITY_NUM] = {0};

    LOG_CONFIG("Start to set prio_tc cfg of logic_id[%d]", param->logic_id);

    if (argc != ARGC_ID_2) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (strncmp(argv[0], "map", strlen("map") + 1)) {
        roce_err("Invalid option: %s.", argv[0]);
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    if (strlen(argv[1]) != PRIO_TC_CFG_STR_LEN) {
        roce_err("Check para length err.");
        return UDA_PARAM_INVALID_ERR;
    }

    ret = tool_prio_tc_str2char(argv[1], PRIO_TC_CFG_STR_LEN, prio_tc);
    if (ret) {
        roce_err("Invalid prio_tc cfg.");
        return ret;
    }
    ret = dsmi_set_prio_tc_info(param->logic_id, 0, prio_tc, PRIORITY_NUM);
    if (ret) {
        roce_err("Dsmi get the mapping between priority and TC failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return ret;
}

int tool_prio_tc_cmd_execute(int argc, char **argv, struct tool_param *param)
{
    int ret;

    if (argc == 0) {
        roce_err("Err argc[%d]", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (!strncmp(argv[0], "-g", strlen("-g") + 1)) {
        param->get_cmd_flag = true;
        ret = tool_get_prio_tc_info(argc, argv, param);
        if (ret) {
            roce_err("Get the mapping between priority and TC failed. (ret=%d)", ret);
            return ret;
        }
    } else if (!strncmp(argv[0], "-s", strlen("-s") + 1)) {
        ret = tool_set_prio_tc_info(argc - 1, argv + 1, param);
        if (ret) {
            roce_err("Set the mapping between each priority and TC failed. (ret=%d; logic_id=%d)", ret,
                     param->logic_id);
            return ret;
        }
        ret = cfg_write_conf(param->phy_id, "prio_tc", argv[ARGC_ID_2]);
        if (ret) {
            roce_err("Write prio_tc cfg error. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        roce_err("Invalid option.");
        return UDA_PARAM_OP_NOT_SUPPORT_ERR;
    }

    return ret;
}

int tool_cfg_prio_tc_recovery(struct tool_param *param)
{
    int ret;
    char prio_tc[PRIO_TC_CFG_STR_LEN + 1] = "";
    char *argv[ARGC_ID_2] = {NULL};
    argv[0] = "prio_tc";

    ret = cfg_read_conf(param->phy_id, "prio_tc", prio_tc, sizeof(prio_tc));
    if (ret != 0) {
        if (ret == UDA_TOOL_SYS_RD_FILE_NOT_FOUND) { // 当配置文件中没有找到这个配置时，返回成功，跳过配置
            roce_info("The prio_tc conf name is not found in config file, skipped. (ret=%d)", ret);
            return 0;
        } else { // 其他错误返回失败
            roce_err("Other inner err. (ret=%d; phy_id=%d)", ret, param->phy_id);
            return ret;
        }
    } else {
        roce_info("Prio_tc enabled need recover cfg. (logic_id=%d)", param->logic_id);
        argv[0] = "map";
        argv[1] = prio_tc;
        ret = tool_set_prio_tc_info(ARGC_ID_2, argv, param);
        if (ret) {
            roce_err("Prio_tc set failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        }
    }

    return ret;
}
