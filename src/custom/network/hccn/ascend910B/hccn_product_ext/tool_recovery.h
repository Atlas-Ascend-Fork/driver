/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SRC_DRIVER_NETWORK_HCCN_HCCN_TOOL_SRC_TOOL_RECOVERY_H
#define SRC_DRIVER_NETWORK_HCCN_HCCN_TOOL_SRC_TOOL_RECOVERY_H

#include "ossl_user_linux.h"
#include "cfg.h"
#include "ds_net_ext.h"

#define FILE_OPT_TYPE_APPEND_WRITE 0
#define FILE_OPT_TYPE_MATCH_DEL 1
#define FILE_OPT_TYPE_MATCH_REPLACES 2
#define INDEX_ZERO 0
#define INDEX_ONE 1
#define INDEX_TWO 2
#define INDEX_THREE 3
#define INDEX_FOUR 4
#define INDEX_FIVE 5

#define IP_RULE_EXSIT_FLAG 100

struct ip_rule_exsit_struct {
    char *ip_rule_out_buf;
    int ip_rule_out_buf_size;
    int *rule_list;
    int *same_list;
    int list_count;
};

struct err_code {
    char *err_name;
    int err_val;
};

struct ip_route_cmd_param {
    char via[MAX_PARAM_LEN];
    char ip[MAX_PARAM_LEN];
    char table[MAX_PARAM_LEN];
    char dev[MAX_PARAM_LEN];
    char ip_mask[MAX_PARAM_LEN];
};

char *get_cmd_err_str(int err_code);
int tool_cfg_cmdline_recovery(const char *prefix, int max_len, struct tool_param *param,
                              int (*recovery)(const char *, struct tool_param *param));
int handle_conf_to_file(const char *cfg, int op_type, char match_list[][CONLINE_LENGTH], int list_cnt);
int tool_cfg_ip_rule_recovery(struct tool_param *param);
int tool_cfg_ip_route_recovery(struct tool_param *param);
int tool_add_ip_route_conf(int logic_id, const char *cmd);
int tool_del_ip_route_conf(int logic_id, const char *ip, const char *ip_mask, const char *table);
int tool_add_ip_rule_conf(int logic_id, const char *cmd);
int tool_del_ip_rule_conf(int logic_id, const char *dir, const char *ip);
int tool_cfg_neigh_recovery(struct tool_param *param);
int change_ip_form(char *ip, int ip_len);
int tool_cfg_downgrade_lane_recovery(struct tool_param *param);
int tool_cfg_pfc_storm_watchdog_recovery(struct tool_param *param);
int tool_downdelay_recovery(struct tool_param *param_of_tool);
int tool_scdr_reset_delay_time_recovery(struct tool_param *param);
int tool_cdr_reset_recovery(struct tool_param *param);
#endif