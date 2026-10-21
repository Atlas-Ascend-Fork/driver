/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SRC_DRIVER_NETWORK_HCCN_HCCN_TOOL_SRC_TOOL_EXT_H
#define SRC_DRIVER_NETWORK_HCCN_HCCN_TOOL_SRC_TOOL_EXT_H

#include <unistd.h>
#include <string.h>
#include "ossl_user_linux.h"
#include "cfg.h"
#include "ds_net_ext.h"
#include "ds_net.h"
#include "ds_net_tool_ext.h"

#define DUPLEX_HALF 0X00
#define DUPLEX_FULL 0X01
#define DIVIDE_TWO 2
#define INDEX_ONE 1
#define INDEX_TWO 2
#define INDEX_TEN 10
#define PARAM_COUNT_TWO 2
#define PARAM_COUNT_THREE 3
#define PARAM_COUNT_FOUR 4
#define PARAM_COUNT_FIVE 5
#define PARAM_COUNT_NINE 9
#define PARAM_COUNT_TEN 10
#define PARAM_COUNT_ELEVEN 11
#define PARAM_COUNT_TWELVE 12
#define MAX_ROCE_PARAM_LEN 3

#define SSU_BP_STATUS_NUM 6
#define RESET_CMP_LEN 6

#define MAX_ARP_PERIOD 3600
#define ARP_CFG_STR_LEN 5

#define MAX_XSFP_RESET_TIME 30
#define MAX_XSFP_WATI_TIME 40
#define INVALID_VALUE 255
#define BASE_WATI_TIME 1.5
#define ROCE_MIN_CW_BAD_CNT_THD 3
#define ROCE_MAX_CW_BAD_CNT_THD 14

#define ARGV_ID_1 1
#define ARGC_ID_2 2
#define ARGC_ID_3 3

#define MAX_CDR_MODE_SIZE 8

#define STEP_ONE 1
#define STEP_TWO 2
#define STEP_THREE 3
#ifndef HCCN_TOOL_ADAPT_DEV_16P
#define MAX_DEV_ID 7
#else
#define MAX_DEV_ID 15
#endif
#define MIN_DEV_ID 0
#define NUM_ARRAY "0123456789"
#define INET6_CONF_PREFIX 9

#define MAX_IP_LEN 20

#define MAX_IPV6_LEN 48
#define MAX_IPV6_NUM 16
#define MAX_IPV4_NUM 4
#define PREFIX_LENGTH 128
#define PREFIX_LENMIN 0
#define SPEED_200G 200000
#define CUR_MAX_TC_NUMBER 4
struct ip6_addr {
    int ip_flag;
    unsigned char netmask_ip[MAX_IPV6_NUM];
    unsigned char address_ip[MAX_IPV6_NUM];
    unsigned char gateway_ip[MAX_IPV6_NUM];
};

#ifdef CFG_SOC_PLATFORM_CLOUD
#define MAX_PERFTEST_MSG_SIZE 2147483647
#else
#define MAX_PERFTEST_MSG_SIZE 874736000
#endif

#define IB_PORT_NUMBER_MIN 1
#define IB_PORT_NUMBER_MAX 1
#define UDP_PORT_NUMBER_MIN 0
#define UDP_PORT_NUMBER_MAX 65535
#define QP_NUMBER_MIN 1
#define QP_NUMBER_MAX 100
#define CQ_MOD_NUMBER_MIN 1
#define CQ_MOD_NUMBER_MAX 1024
#define TX_DEPTH_NUMBER_MIN 1
#define TX_DEPTH_NUMBER_MAX 15000
#define QP_TIMEOUT_NUMBER_MIN 0
#define QP_TIMEOUT_NUMBER_MAX 255
#define GID_INDEX_NUMBER_MIN 0
#define GID_INDEX_NUMBER_MAX 5
#define PRIORITY_NUMBER_MIN 0
#define PRIORITY_NUMBER_MAX 7

#define MASG_SIZE_MIN 1
#define MASG_SIZE_MAX MAX_PERFTEST_MSG_SIZE
#define IB_DEVICE_MIN 0
#define IB_DEVICE_MAX MAX_DEV_ID
#define SERVICE_ADDRESS_MIN 0
#define SERVICE_ADDRESS_MAX 0
#define EXCHANGES_NUMBER_MIN 5
#define EXCHANGES_NUMBER_MAX 20000
#define TOS_NUMBER_MIN 0
#define TOS_NUMBER_MAX 255
#define PERIOD_OF_SECONDS_MIN 1
#define PERIOD_OF_SECONDS_MAX 500
#define MEASURE_MARGINS_MIN 0
#define MEASURE_MARGINS_MAX 249
#define WQES_NUMBER_MIN 1
#define WQES_NUMBER_MAX 1024
#define MTU_NUMBER_MIN 1
#define MTU_NUMBER_MAX 2147483647
#define TOOL_TRACEROUTE_CMD_LEN_MIN 2
#define TRACEROUTE_WAITTIME_MAX 60
#define TRACEROUTE_WAITTIME_MIN 1
#define TRACEROUTE_TTL_MIN 1
#define TRACEROUTE_TTL_MAX 255
#define TRACEROUTE_TOS_MAX 63
#define TRACEROUTE_TC_MAX 255
#define TRACEROUTE_DOUBLE_ADD 2
#define TRACEROUTE_ARGC_MAX 16
#define STR_TO_INT_LEN 10
#define MEM_FREE_ZEOR 0
#define PERCENT_NINETY_FIVE 0.95

#define LINKDOWN_FILTER_TIME_MAX 3
#define SET_MODE 1
#define GET_MODE (-1)
#define DECIMAL 10

#define MAX_LIMIT_IP_NUMS 4
#define SMP_VNIC_IP_INDEX 1
#define MAX_A3_LIMIT_IP_NUMS 2
#define A3_IN_SUPERPOD_INDEX 0
#define A3_NOT_IN_SUPERPOD_INDEX 1
#define A3_SUPERPOD_MAX_NUMS 48

#define TOOL_MIN_DIAGNOSTIC_DUTRTION_TIME 5
#define TOOL_MAX_DIAGNOSTIC_DUTRTION_TIME 720 // 12h
#define TOOL_MAX_DIAGNOSTIC_XSFP_PAGE_NUMS 0xFF
#define TOOL_MAX_DIAGNOSTIC_REG_OFFSET 0x7F
#define TOOL_DIAGNOSTIC_OPTICAL_REG_OFFSET_LABEL_START 118
#define TOOL_DIAGNOSTIC_OPTICAL_REG_OFFSET_LABEL_END 125
#define TOOL_MAX_UINT_BYTES(x) ((1ULL << (8 * (x))) - 1)
#define TOOL_ONE_MINUTES_SECONDS 60
#define TOOL_IMP_SOCIP_CLOCK 250000000
#define TOOL_DIAGNOSTIC_DISABLE 0
#define TOOL_DIAGNOSTIC_ENABLE 1
#define TOOL_DIAGNOSTIC_INVALID_PARAM (-1)
#define TOOL_DIAGNOSTIC_I2C_WRITE 0
#define TOOL_DIAGNOSTIC_I2C_READ 1
#define TOOL_DIAGNOSTIC_ONE_BYTE 4
#define TOOL_DIAGNOSTIC_MODE_GET 0
#define TOOL_DIAGNOSTIC_MODE_SET 1
#define TOOL_TC_QOS_OUTBUFF_LEN 500
#define TOOL_TC_QOS_INFO_TC_BW_LENGTH 32
#define TOOL_TC_QOS_INFO_TSA_MAX_LENGTH 72
#define TOOL_TC_QOS_INFG_TC_BW_MIN 0
#define TOOL_TC_QOS_INFG_TC_BW_MAX 100
#define TOOL_TC_QOS_INFO_TC_COUNT 8
#define TOOL_TC_QOS_INFO_TEMP_LENGTH 128
#define TOOL_TC_QOS_INFO_PARAM_COUNT 4
#define TOOL_TC_QOS_INFO_PARAM_INVALID (-1)
#define TOOL_OPTICAL_SNR_QUERY_DISABLE 0
#define TOOL_OPTICAL_SNR_QUERY_ENABLE 1

#define A3_CPLD_DIAG_OFFSET 0x9
#define A3_CPLD_DIAG_SIZE 12
#define A2_CPLD_SPECIAL_REG1 0x22
#define A2_CPLD_SPECIAL_REG2 0x37
#define A2_CPLD_DIAG_OFFSET1 0xD0
#define A2_CPLD_DIAG_OFFSET2 0xE0
#define A2_CPLD_DIAG_OFFSET3 0xF0
#define A2_CPLD_DIAG_SIZE1 10
#define A2_CPLD_DIAG_SIZE2 10
#define A2_CPLD_DIAG_SIZE3 3
#define DIAG_CPLD_MAX_LEN 25
#define DIAG_CPLD_ROW_LEN 10
#define DIAG_CPLD_STR_LEN 256
#define TOOL_BIT_OFFSET_8 8
#define TOOL_BIT_OFFSET_16 16
#define TOOL_BIT_OFFSET_24 24
#define TOOL_BIT_OFFSET_32 32
#define ATLAS_NPU_SERDERS_NUM_0 0
#define ATLAS_NPU_SERDERS_NUM_1 1

enum tool_tc_qos_tsa_type {
    TSA_ETS = 0,
    TSA_STRICT,
    TSA_INVALID
};

enum tool_tc_qos_operate_mode {
    TOOL_TC_QOS_OPERATE_SET = 0,
    TOOL_TC_QOS_OPERATE_RECOVERY,
    TOOL_TC_QOS_OPERATE_INVALID
};

struct tool_tc_qos_info {
    int recovery_flag;
    char prio_map[TOOL_TC_QOS_INFO_TC_COUNT];
    char tc_bw[TOOL_TC_QOS_INFO_TC_COUNT];
    char tsa_map[TOOL_TC_QOS_INFO_TC_COUNT];
};

struct default_gateway_info {
    int logic_id;
    char gateway[MAX_IPV6_LEN];
};
struct perftest_param_info {
    char *param;
    int (*param_trans_func)(struct perftest_cmd_info *cmd_info, char *val, char *comment, int min_data, int max_data);
    char *comment;
    int min_data;
    int max_data;
};

enum tool_ippair_op_type {
    IPPAIR_SET = 0,
    IPPAIR_DEL
};

// 诊断模式控制参数及响应结构体
struct diag_mode_params {
    int duration_min;                     // 设置时使用：持续时间（分钟）
    char is_enabled;                      // 设置时使用：ON/OFF
    char master_flag;                     // 主从die标识
    unsigned long long remaining_seconds; // 查询时返回：剩余时间（秒）
    int configured_duration_min;          // 查询时返回：配置时长
    int operation;                        // 设置时使用：操作类型write/read
};

enum diag_slave_obj {
    DIAG_SLAVE_XSFP, // 光模块
    DIAG_SLAVE_CPLD, // CPLD
    DIAG_SLAVE_MAX,  // 当前仅支持到此处
    DIAG_SLAVE_CDR   // CDR, 当前不支持
};

struct diag_slave_obj_table {
    const char *slave_str;
    enum diag_slave_obj slave_obj;
};

struct diag_i2c_mode_table {
    const char *i2c_mode;
    int i2c_mode_value;
};

#define DIAG_PAGE_MAX_LEN 256

struct xsfp_access_params {
    unsigned char page_idx;   // Page Index
    unsigned char reg_offset; // 寄存器偏移
    unsigned char reg_size;   // 读写大小（字节）
    unsigned char guest_mode; // 代理标记，主DIE收到代理的光模块访问请求时，会确认对方的诊断模式
    unsigned char reg_value[DIAG_PAGE_MAX_LEN]; // 寄存器值
};

#define DIAG_CPLD_MAX_LEN 25
struct cpld_access_params {
    unsigned char val[DIAG_CPLD_MAX_LEN];
};

struct i2c_access_params {
    unsigned char i2c_mode;             // I2C 读/写
    unsigned char slave_obj;            // 枚举类型，如 SLAVE_XSFP, SLAVE_CPLD
    unsigned char result;               // 执行结果
    union {                             // 后续向下扩展cpld、cdr等
        struct xsfp_access_params xsfp; // 光模块访问参数
        struct cpld_access_params cpld;
    };
};

int tool_cdr_log_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_roce_test_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_speed_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_duplex_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_bp_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_serdes_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_fec_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_cfg_fec_recovery(struct tool_param *param);
int tool_pcs_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_adapt_status_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_ip_rule_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_ip_route_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_get_param(int argc, char **argv, char *prefix, char *val, int max_len);
int tool_cfg_pfc_recovery(struct tool_param *param);
int tool_pfc_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_tc_cfg_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_tc_buf_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_tc_cfg_recovery(struct tool_param *param);
int tool_cfg_prio_tc_recovery(struct tool_param *param);
int tool_prio_tc_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_ping_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_ping_cmd_execute_ext(int argc, char **argv, struct tool_param *param);
int tool_cfg_gratuitous_arp_recovery(struct tool_param *param);
int tool_send_gratuitous_arp_cmd_execute(int argc, char **argv, struct tool_param *param);
bool is_ipv6_cmd(int *argc, char ***argv);
bool is_atlas_9000_a3(struct tool_param *param);
int tool_get_route_table_ipv6(int argc, char **argv, struct tool_param *param);
int tool_route_table_cmd_execute_ipv6(int argc, char **argv, struct tool_param *param);
int tool_neigh_table_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_ip_cmd_execute_ipv6(int argc, char **argv, struct tool_param *param);
int tool_get_device_ipaddr_ipv6(struct tool_param *param);
int tool_ip_get_ipaddr_ipv6(int argc, char *argv[], char *address, char *prefix, int length);
int tool_check_ipv6_prefix_valid(unsigned int prefix_length);
int tool_ip_set_address_ipv6(struct tool_param *param, const char *address, const char *netmask);
int tool_sd_cdr_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_down_data_cmd_execute(int argc, char **argv, struct tool_param *param);
int perftest_stop_status(int device_id, unsigned int is_client);
int tool_xsfp_reset_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_get_extra_statistics_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_get_fec_error_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_cfg_xsfp_reset_recovery(struct tool_param *param);
int tool_get_ipaddr_ipv6_from_device(struct tool_param *param, char *address, unsigned char *prefix_length,
                                     int ipv6_len);
int verify_little_param(int argc, struct tool_param *param, struct perftest_cmd_info cmd_info[], int is_client,
                        int start_cnt);
int waiting_roce_perftest_finish(int device_id, unsigned int is_client, struct perftest_cmd_info cmd_info[],
                                 struct perftest_curr_stat *perftest_curr, int wait_num);
unsigned int get_little_mem_process();
void set_little_mem_process(unsigned int n);
int tool_traceroute_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_cfg_host_info_recovery(struct tool_param *param);
int tool_update_roce_port_ippair(int logic_id, int argc, char **argv, enum tool_ippair_op_type op);
int tool_clear_roce_port_ippair(int logic_id, int argc);
int tool_get_all_ippair(int logic_id);
int tool_get_single_ippair(int logic_id, int argc, char **argv);
int tool_downgrade_lane_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_pfc_stat_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_tc_stat_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_hilink_info_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_get_qp_info_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_roce_cw_bad_cnt_thd_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_roce_cw_bad_cnt_thd_recovery(struct tool_param *param);
int tool_set_roce_cw_bad_cnt_thd(int argc, char **argv, struct tool_param *param);
int tool_get_roce_cw_bad_cnt_thd(int argc, char **argv, int logic_id);
int tool_pfc_storm_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_check_mtu_ipv6(int logic_id);
int tool_del_ipv6_conf_after_set_mtu(int phy_id, unsigned int mac_type, unsigned int mtu_size);
int tool_set_sflag(struct tool_param *param, int *sflag, int *logic_id_after_convert);
int tool_link_set_downdelay(int argc, char **argv, struct tool_param *param);
int str_to_int(char *str, int *value);
int tool_config_downdelay(int logic_id, int value, int mode);
int is_dev_ipaddr_conflict_ipv6(int logic_id, int port_type, int port_id, unsigned char *address_ip,
                                unsigned char *netmask_ip);
int is_dev_ipaddr_conflict(int logic_id, int port_type, int port_id, unsigned int ipaddr, unsigned int mask);
int tool_transfer_check_ipaddr(char *address, unsigned int *address_ip);
int tool_transfer_check_netmask(char *netmask, unsigned int *netmask_ip);
bool tool_is_910_a3(int logic_id);
int is_910_a3_ip_conflict(int logic_id, const char *address, const char *netmask, int is_ipv4);
int tool_ipaddr_conflict_extend(int logic_id, unsigned int phy_id, unsigned int ipaddr, unsigned int mask);
int tool_ip_read_gateway_conf_v2(int phy_id, bool is_pcie_card, unsigned int *gateway, int *gateway_phy_id);
int tool_scdr_reset_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_scdr_esd_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_diagnostic_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_tc_qos_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_cfg_tc_qos_recovery(struct tool_param *param);
int dsmi_set_diagnostic_i2c_access_cmd(int logic_id, struct i2c_access_params *params, enum ds_net_opcode_type_ext cmd);
int tool_optical_snr_query_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_cfg_optical_snr_query_recovery(struct tool_param *param);
int tool_is_optical_supported_board_type(struct tool_param *param);
int tool_get_convert_logic_id_and_master_flag(struct tool_param *param, int *logic_id, unsigned char *master_flag);
int tool_mem_reserve_cmd_execute(int argc, char **argv, struct tool_param *param);
int tool_cfg_mem_reserve_recovery(struct tool_param *param);
int tool_dump_execute(int argc, char **argv, struct tool_param *param);
#endif
