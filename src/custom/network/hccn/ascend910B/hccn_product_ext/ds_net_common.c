/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "user_log.h"
#include "ds_net.h"
#include "ds_net_ext.h"
#include "ds_net_common.h"

/**
 * 发送网络DSMI命令
 * @param logic_id 逻辑ID
 * @param port_id 端口ID
 * @param trans_data 传输数据结构体指针
 * @return 成功返回0，失败返回错误码
 */
int dsmi_network_common_send(int logic_id, int port_id, struct ds_trans_data *trans_data)
{
    int ret;

    // 检查trans_data是否为空
    if (!trans_data) {
        roce_err("Network dsmi trans_data parameter is NULL");
        return -EINVAL;
    }

    // 检查logic_id是否在有效范围内
    if (logic_id > DS_MAX_LOGIC_ID || logic_id < 0) {
        roce_err("Failed to validate logic_id. (opcode=0x%x, logic_id=%d, max=%d)", trans_data->cmd, logic_id,
                 DS_MAX_LOGIC_ID);
        return -EINVAL;
    }

    // 检查port_id是否在有效范围内
    if ((port_id < 0) || (port_id > MAX_PORT_ID)) {
        roce_err("Failed to validate port_id. (opcode=0x%x, port_id=%d, max=%d)", trans_data->cmd, port_id,
                 MAX_PORT_ID);
        return -EINVAL;
    }
    // 发送命令
    roce_info("Sending DSMI network command. (logic_id=%d, port_id=%d, opcode=0x%x)", logic_id, port_id,
              trans_data->cmd);
    ret = dsmi_network_transmission_channel(logic_id, trans_data);
    if (ret) {
        roce_err("Failed to send command (ret=%d; logic_id=%d; port=%d;  opcode=0x%x)", ret, logic_id, port_id,
                 trans_data->cmd);
        return ret;
    }

    // 失败场景
    if (trans_data->result != 0) {
        roce_err("Command execution failed. (result=%d; logic_id=%d; port=%d;  opcode=0x%x)", trans_data->result,
                 logic_id, port_id, trans_data->cmd);
    }

    return trans_data->result;
}
