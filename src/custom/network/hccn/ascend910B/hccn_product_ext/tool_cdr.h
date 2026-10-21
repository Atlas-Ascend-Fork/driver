/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SRC_DRIVER_NETWORK_HCCN_HCCN_TOOL_SRC_TOOL_CDR_H
#define SRC_DRIVER_NETWORK_HCCN_HCCN_TOOL_SRC_TOOL_CDR_H

#include <unistd.h>
#include <string.h>
#include "ossl_user_linux.h"
#include "tool_lib.h"

#define MAX_CDR_LOG_NAME_LEN 128
#define TIME_BEGIN_YEAR 1900
#define MS_TO_US_CNT 1000

#define MAX_MAC_LANE_INFO_BD_SIZE 5
#define ONE_CMD_MAX_CDR_REG_SIZE 464
#define MAX_MAC_LANE_INFO_SIZE 48
#define CDR_LANE_HEAD_SIZE 24

enum {
    TRIG_SUCCESS,
    IS_UPDATING,
    IS_READING,
    UPDATING_FAIL,
};

enum cdr_op_code {
    CDR_OP_TRIG_REG_DUMP = 0,
    CDR_OP_CDR_REG_DUMP = 1,
    CDR_OP_TRIG_EXT_LOG_DUMP = 2,
    CDR_OP_CDR_LOG_DUMP = 3,
};

typedef int (*cdr_func)(void);
struct cdr_execute {
    enum cdr_op_code op_code;
    cdr_func func;
};

struct tool_cdr_data {
    int logic_id;
    int phy_id;
    u32 op_code;
};

union lane_info_head {
    struct {
        u8 flag;
        u8 logic_num;
    } info;

    u8 data[CDR_LANE_HEAD_SIZE];
};

struct lane_info_data {
    u8 mac_logic_lane_id;
    u8 tx_cdr_chanel;
    u8 rx_cdr_chanel;
    u8 tx_cdr_logic_lane_id;
    u8 rx_cdr_logic_lane_id;
    u8 reserved[3];
    u32 tx_cdr_chanel_size;
    u32 tx_cdr_reg_base_offset;
    u32 rx_cdr_chanel_size;
    u32 rx_cdr_reg_base_offset;
};

struct query_mac_lane_info {
    union lane_info_head head;
    struct lane_info_data data[MAX_MAC_LANE_INFO_BD_SIZE - 1];
};

struct query_cdr_head_info {
    u32 start_offset;
    u32 size;
    u8 cdr_logic_lane_id;
    u8 reserved[7U];
};

struct query_cdr_reg_info {
    struct query_cdr_head_info head;
    u8 data[ONE_CMD_MAX_CDR_REG_SIZE];
};

struct trig_get_cdr_info {
    u32 type;
    u32 flag;
};

enum get_cdr_reg_state {
    INIT_STATE,
    TRIG_UPDATE_START,
    TRIG_UPDATE_FINISH,
    TRIG_UPDATE_FAIL,
};

enum {
    TRIG_CDR_EXT_LOG,
    TRIG_CDR_REG,
};

int tool_cdr_cmd_execute(int argc, char **argv, struct tool_param *param);
#endif