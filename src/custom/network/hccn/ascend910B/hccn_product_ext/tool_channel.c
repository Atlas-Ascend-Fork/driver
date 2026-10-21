/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccn_comm.h"
#include "user_log.h"
#include "ossl_user_linux.h"
#include "ds_net.h"
#include "ds_net_ext.h"
#include "tool_ext.h"
#include "network_tool.h"

int host_nt_channel_send(unsigned int device_id, unsigned int cmd, void *data, unsigned int data_len)
{
    struct ds_trans_data trans_data = {0};
    int ret, size_out = data_len;
    char *middle = calloc(1, sizeof(unsigned int) + data_len); // HD通道只接收一个输入，需要把cmd和data拼一起

    if (middle == NULL) {
        roce_err("[NT CHANNEL] alloc middle failed. (cmd=%x)\n", cmd);
        return -EINVAL;
    }

    ret = memcpy_s(middle, sizeof(unsigned int), &cmd, sizeof(unsigned int));
    ret += memcpy_s(middle + sizeof(unsigned int), data_len, data, data_len);
    if (unlikely(ret)) {
        roce_err("[NT CHANNEL] memcpy failed. (ret=%d, cmd=%x)\n", ret, cmd);
        free(middle);
        return ret;
    }

    // HD通道上本来有[root]和[物理/虚机/容器]的鉴别，这里不做。如果想鉴别请自行在调用host_nt_channel_send前NT_CHANNEL_ROOT_CHECK和NT_CHANNEL_PHY_CHECK
    // NTC通道完成后将注册占用HDKI通道上各一个命令字，当前预埋阶段先设置一个无效命令字
    DSMI_SET_TRANS_DATA(trans_data, DS_MAX_CMD_EXT, middle, data_len + sizeof(unsigned int), data, &size_out);
    ret = dsmi_network_transmission_channel(device_id, &trans_data);
    free(middle);
    if (ret) {
        roce_err("[NT CHANNEL] host-device channel error. (ret=%d, cmd=%x)\n", ret, cmd);
        return ret;
    } else if (trans_data.result) {
        roce_err("[NT CHANNEL] device error. (result=%d, cmd=%x)\n", trans_data.result, cmd);
        return trans_data.result;
    }
    return 0;
}

// 自由区间 ↓
// #ifdef DEBUG_TOOL
#define PRINT printf

int tool_huyang_reg_operate(int argc, char **argv, void *input)
{
    struct huyang_reg_op_t data = {0};
    struct tool_param *param = input; // 或许这个地方可以优化一下，主要是不想让network_tool.h依赖上ossl_user_linux.h
    char *endp = NULL;
    int ret;

    if ((argc != ARGC_ID_2) && (argc != ARGC_ID_3)) {
        roce_err("err argc. (argc=%d)\n", argc);
        return UDA_PARAM_CMD_COUNT_ERR;
    }

    if (strcmp(argv[0], "read") == 0) {
        data.rw = 0;
    } else if (strcmp(argv[0], "write") == 0) {
        data.rw = 1;
    } else {
        return UDA_PARAM_INVALID_ERR;
    }

    data.reg = strtoul(argv[ARGC_ID_1], &endp, 0);
    if ((endp == NULL) || (*endp != '\0')) {
        return UDA_PARAM_INVALID_ERR;
    }
    if (argc == ARGC_ID_3) {
        data.val = strtoul(argv[ARGC_ID_2], &endp, 0);
        if ((endp == NULL) || (*endp != '\0')) {
            return UDA_PARAM_INVALID_ERR;
        }
    }
    NT_CHANNEL_ROOT_CHECK_RETURN;
    ret = host_nt_channel_send(param->logic_id, NT_IMP_ASYNC_CMD(HUYANG_REG_OP), &data, sizeof(data));
    if (data.rw) {
        PRINT("[%s] huyang write [%x] >> [%x]\n", ret ? "FAIL" : "PASS", data.val, data.reg);
    } else {
        PRINT("[%s] huyang read  [%x] >> [%x]\n", ret ? "FAIL" : "PASS", data.val, data.reg);
    }
    return ret;
}

struct sub_command_t {
    char *cmd;
    int (*execute)(int argc, char **argv, void *data);
    void *data;
};
int goto_sub_command(int argc, char **argv, struct sub_command_t *list)
{
    int i = 0;
    for (i = 0; list[i].cmd; i++) {
        if (argc && strcmp(argv[0], list[i].cmd) == 0) {
            return list[i].execute(argc - 1, argv + 1, list[i].data);
        }
    }
    if (argc == 0) {
        if (list[i].execute) {
            return list[i].execute(argc, argv, list[i].data);
        } else {
            PRINT("please input sub command.\n");
            return 0;
        }
    }
    PRINT("sub command [%s] is not found.\n", argc ? argv[0] : NULL);
    return 0;
}

int tool_debug_tool(int argc, char **argv, struct tool_param *param)
{
    struct sub_command_t sub_command_list[] = {{"huyang", tool_huyang_reg_operate, param}, {0}};
    return goto_sub_command(argc, argv, sub_command_list);
}
// #endif