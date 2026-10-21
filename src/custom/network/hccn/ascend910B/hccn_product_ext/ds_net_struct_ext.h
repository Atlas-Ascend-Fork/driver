/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SRC_DRIVER_NETWORK_HCCN_DSMI_DS_NET_STRUCT_EXT_H
#define SRC_DRIVER_NETWORK_HCCN_DSMI_DS_NET_STRUCT_EXT_H

#define DS_LANE_MAX_NUM 4
#define BOOST_VALS_NUM 9

#include <stdbool.h>

struct hilink_ctle_data;
struct hilink_dfe_data;
struct hilink_ffe_data;
struct hilink_port_info;
struct query_mac_lane_info;
struct query_cdr_reg_info;
struct trig_get_cdr_info;
struct hilink_adapt_status_info;
struct gratuitous_arp_info;
struct xsfp_reset_info;
struct ds_host_info;
struct pfc_duration_info;
struct pfc_storm_info;
struct scdr_reg_info;

// derdes 数据结构
struct hilink_info {
    // pll lock
    unsigned int pll0_out_of_lock;
    unsigned int pll1_out_of_lock;
    unsigned int pll0_pwrdb_trigger;
    unsigned int pll1_pwrdb_trigger;
    unsigned int pll0_ref_clk_sel;
    unsigned int pll0_core_clk_divrstn;

    // snr
    unsigned int heh;
    unsigned int snr_metric_msb;
    unsigned int snr_metric_lsb;
    unsigned int snr_metric_his_min_msb;
    unsigned int snr_metric_his_min_lsb;
    unsigned int snr_cycles;

    // tx_drv
    unsigned int txfir_post_sign;
    unsigned int txfir_post;
    unsigned int txfir_pre_sign;
    unsigned int txfir_pre;

    unsigned int tx_drv_hswing_en;
    unsigned int tx_drv_hswing_cs_p;
    unsigned int tx_drv_hswing_cs_m;
    unsigned int txcp_dcd_adj;
    unsigned int txcp_dcd_adj_sign;
    unsigned int txpwrdb;
    unsigned int txratemode;
    unsigned int txalignmode;
    unsigned int txphsval;

    // rx
    unsigned int rxpwrdb;
    unsigned int rxratemode;

    // boost_index;
    unsigned int boost_index;
    unsigned int boost_vals[BOOST_VALS_NUM];

    // CTLE_GAIN
    unsigned int rx_ctle_st1_agc;

    // cdr
    unsigned int cdrphscodeout; // 补码
    unsigned int cdrphasegain;
    unsigned int cdrvcogain;
    unsigned int cdr2ndlpcfg;
    unsigned int cdrfreqgain;
    unsigned int cursorconfig;

    unsigned int integrator_out; // 补码

    unsigned int cs_calib_en_api;
    unsigned int cs_calib_status_api;
    unsigned int ds_calib_en_api;
    unsigned int dc_calib_status_api;
    unsigned int init_apapt_en_api;
    unsigned int init_apapt_status_api;
    unsigned int cont_apapt_en_api;
    unsigned int cont_apapt_status_api;
    unsigned int ds_state_api;

    unsigned int los_det_status_api;
    unsigned int lms_los_det_status_api;
    unsigned int cdr_los_det_status_api;
    unsigned int rnd_los_det_status_api;
    unsigned int los_history_status_api;
    unsigned int lms_los_history_status_api;
    unsigned int cdr_los_history_status_api;
    unsigned int rnd_los_history_status_api;

    unsigned int ds_rx_insertion_loss_api;
    unsigned int ds_power_mode_api;

    unsigned int rx_ctle_st1_offset;
    unsigned int dp_coef_read_indx;
};

struct ds_serdes_info {
    int chip_id;
    struct hilink_info ds_info[DS_LANE_MAX_NUM];
};

#define INFO_PAYLOAD_LEN 1000
/* PFC风暴检测watchdog配置项未修改占位符 */

#define PFC_STORM_CFG_NOT_CHANGE (~0U)
#define PFC_STORM_CFG_CHANGE (0U)
#define PFC_STORM_WATCHDOG_CLOSE_TIME_MIN 0
#define PFC_STORM_WATCHDOG_CLOSE_TIME_MAX 100
#define PFC_STORM_WATCHDOG_DETECT_PERIOD_MIN 10
#define PFC_STORM_WATCHDOG_DETECT_PERIOD_MAX 100
#define PFC_STORM_WARN_THRD_MIN 40
#define PFC_STORM_WARN_THRD_MAX 80
#define PFC_STORM_ERR_THRD_MIN 60
#define PFC_STORM_ERR_THRD_MAX 100
#define PFC_100MS_TO_S 10
#define PFC_100MS_TO_MS 100
/* PFC风暴检测watchdog的三种检测模式 */

#define PFC_STORM_WATCHDOG_MODE_0 0X0
#define PFC_STORM_WATCHDOG_MODE_1 0X1
#define PFC_STORM_WATCHDOG_MODE_2 0X2
#define PFC_STORM_WATCHDOG_MODE_NUM 3
/* PFC风暴watchdog的参数取值范围是0 -100 */

#define MAX_PFC_STORM_STORM_PARAM_LEN 40
/* PFC风暴检测watchdog的参数个数 */

#define MAX_PFC_STORM_WATCHDOG_PARAM_NUM 5
struct pfc_storm_cfg_param {
    unsigned int mode;
    unsigned int close_time;    // ms
    unsigned int detect_period; // s
    unsigned int warn_thrd;     // ms
    unsigned int err_thrd;      // ms
};
struct pfc_storm_cfg_info {
    struct pfc_storm_cfg_param user_cfg_param;
    unsigned int cfg_change_flag;
};

struct ds_optical_mode_info {
    unsigned char mode;
    unsigned char master_flag;
};

enum optical_cmd {
    CMD_HARD_RESET = 0,
    CMD_SOFT_RESET,
    CMD_LOW_POWER,
    CMD_HIGH_POWER,
    CMD_DISABLE_TX,
    CMD_ENABLE_TX,
};

enum tool_optical_status {
    TOOL_OPTICAL_ABSENT = 0,
    TOOL_OPTICAL_PRESENT,
    TOOL_OPTICAL_INVALID,
};

#define PAGE_MAX_LEN 256
struct ds_optical_register_info {
    unsigned int mode;
    unsigned int master_flag;
    unsigned int index;
    unsigned int len;
    unsigned int result;
    unsigned char page_info[PAGE_MAX_LEN];
};

enum dsmi_ping_result {
    PING_NOT_START,
    PING_SUCCESS,
    PING_SEND_FAILED,
    PING_RECV_TIMEOUT,
    PING_RES_MAX
};

struct prbs_adapt_mode_info;

struct ds_diag_mode_params {
    int duration_min;                     // 设置时使用：持续时间（分钟）
    char is_enabled;                      // 设置时使用：ON/OFF
    unsigned long long remaining_seconds; // 查询时返回：剩余时间（秒）
    int configured_duration_min;          // 查询时返回：配置时长
    int operation;                        // 设置时使用：操作类型write/read
};

enum ip_rule_dir {
    IP_RULE_DIR_FROM,
    IP_RULE_DIR_TO
};

struct ip_rule_cmd_info {
    enum ip_rule_dir dir;
    unsigned int ip_address;
    unsigned int table_id; /* 0~255 */
    char cmd_type;         /* g-get s-add d-del */
};

struct ip_route_cmd_info {
    unsigned int eth_id;
    unsigned int ip_address;
    unsigned int ip_mask; /* 0~32 */
    unsigned int via_address;
    unsigned int table_id; /* 0~255 */
    char cmd_type;         /* g-get s-add d-del */
};

struct reserve_mem_info {
    unsigned int type;          /* 内存预留类型 */
    unsigned int mem_size;      /* 当前生效的内存大小，单位：MB */
    unsigned int page_size;     /* 页面大小，单位：KB */
    unsigned int result;        /* 返回结果，参考 enum reserve_mem_result */
    unsigned int rebooted_size; /* 重启后生效的内存大小，单位：MB */
};

enum reserve_mem_type {
    RESERVE_MEM_TYPE_PDCCL = 0,
    RESERVE_MEM_TYPE_MAX
};

/* Mapping from reserve_mem_type to string */
struct mem_reserve_type_map {
    enum reserve_mem_type type;
    const char *type_str;
};

static const struct mem_reserve_type_map g_mem_reserve_type_map[] = {{RESERVE_MEM_TYPE_PDCCL, "pdccl"}};

enum reserve_mem_result {
    RESERVE_MEM_RESULT_SUCCESS = 0, // 配置下发成功，无需重启。适用于首次下发配置，或者二次下发相同配置的场景。
    RESERVE_MEM_RESULT_REBOOT_REQUIRED = 1, // 配置下发成功，需要重启生效。适用于二次下发不同配置的场景。
    RESERVE_MEM_RESULT_CANCEL_REBOOT = 2, // 配置取消下发成功，需要重启生效。适用于下发配置后再下发size=0的场景。
    RESERVE_MEM_RESULT_CANCEL_SUCCESS = 3, // 配置取消下发成功，无需重启。适用于首次即下发size=0的场景。
};

int dsmi_clear_pfc_storm(int logic_id, int mode);
int dsmi_get_pfc_storm_info(int logic_id, struct pfc_storm_info *info);
int dsmi_set_pfc_storm_watchdog_param(int logic_id, struct pfc_storm_cfg_info *cfg_param);
int dsmi_get_pfc_storm_watchdog_param(int logic_id, struct pfc_storm_cfg_param *cfg_param);
int dsmi_dump_optical_info_cmd(int logic_id, int port, struct ds_optical_register_info *info);
int dsmi_handle_scdr_reg(int logic_id, int port_id, struct scdr_reg_info *reg_info);
int dsmi_set_optical_mode(int logic_id, struct ds_optical_mode_info *data);
int dsmi_set_mem_reserve(int logic_id, struct reserve_mem_info *info);
int dsmi_get_mem_reserve(int logic_id, struct reserve_mem_info *info);
#endif
