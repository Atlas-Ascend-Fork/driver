/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SRC_DRIVER_NETWORK_HCCN_DSMI_DS_NET_EXT_H
#define SRC_DRIVER_NETWORK_HCCN_DSMI_DS_NET_EXT_H
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include "ds_net_struct_ext.h"

#define PERFTEST_FLAG_LEN 200

#define MAX_CDR_LOG_SIZE (128 * 1024)
#define RDFX_EXT_CMD_MAX_LEN 1024
#define RDFX_EXT_REPLY_MAX_LEN 386000
#define MIN_IP_ROUTE_TAB_IDX 0
#define MAX_IP_ROUTE_TAB_IDX 255
#define MAX_IP_MASK_BIT 32
#define MIN_IP_MASK_BIT 0
#define MIN_IP_VAL 0
#define MAX_IP_VAL 255
#define MAX_PARAM_LEN 50
#define MIN_DEV_ID 0
#define MAX_DEV_ID 7
#define MAX_IP_CNT 4
#define IP_INDEX_0 0
#define IP_INDEX_1 1
#define IP_INDEX_2 2
#define IP_INDEX_3 3
#define ARGC_NUM_1 1
#define ARGC_NUM_2 2
#define ARGC_NUM_3 3
#define ARGC_NUM_4 4
#define ARGC_NUM_5 5
#define ARGC_NUM_6 6
#define ARGC_NUM_7 7
#define ARGC_NUM_8 8
#define ARGC_NUM_10 10

#define MAX_TYPE_LEN 20
#define MAX_CDR_LEN 4
#define MAX_CFG_LEN 20

#define ROCE_CMD_MAX_LEN 128
#define MAX_REPORT_LEN 90000
#define MAX_CMD_COUNT 40

#define PERFTEST_START_SUCCESS 1
#define PERFTEST_START_FAIL 2

/**
 * perftest性能测试状态枚举
 */
enum ds_perftest_status {
    PERFTEST_STATUS_DOING = 2,
    PERFTEST_STATUS_DONE = 1,
    PERFTEST_ERROR = 3,
};

#define MAX_PERFTEST_WAITING_TIMES 1600
#define MEM_MIN 3500
#define MEM_STATUS_INIT 0
#define MEM_STATUS_LIMIT 1

#define ETH_SPEED_UNKNOWN 0xFFFF
#define PRIORITY_NUM 8
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define PRIORITY_NUM_TOTAL 4 // The number of 910B/C pfc priority queues is 4
#else
#define PRIORITY_NUM_TOTAL 8 // The number of 910A pfc priority queues is 8
#endif

#define TC_NUM 8
#define PFC_CFG_STR_LEN 15

#define PRIO_TC_CFG_STR_LEN 15
#define RDFX_PRIO_TC_STR_LEN 31

#define FEC_MODE_STR_MAX 4
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#define FEC_MODE_TYPE_MAX 2
#else
#define FEC_MODE_TYPE_MAX 3
#endif

#define IP_ADDR_LEN 24
#define IP6_ADDR_LEN 48
#define STR_NUM_BASE 10
#define MAX_DATA_LEN (65535 - 20 - 8)
#define INBUF_LEN 24
#define INET6_PREFIX_MAX 128
#define PING_INNER_ERROR 100
#define DECIMAL_NUM 1000
#define PERCENT 100
#define THOUSAND 1000
#define MOD_NUM_TWO 2

#define DEVDRV_IPV6_ARRAY_LEN 16

#define TRACEROUTE_STATUS_ERROR 2
#define TRACEROUTE_STATUS_RUNNING 1
#define TRACEROUTE_STATUS_NOT_RUNNING 0
#define MAX_TRACEROUTE_WAITING_TIMES 600
#define TRACEROUTE_HCCN_CMD 0
#define TRACEROUTE_DCMI_CMD 1
#define STR_TO_INT_TEN 10
#define VNIC_IP_SERVER_ID 16
#define SDID_SERVER_ID 22
struct perftest_cmd_info {
    char type[MAX_TYPE_LEN];
    unsigned long cfg_data;
    unsigned int ipv6_index;
    struct in6_addr in6_ip;
    unsigned int little_mem;
};
struct perftest_curr_stat {
    enum ds_perftest_status status;
    unsigned int free_mem;
};

struct ds_cdr_version_info {
    char hilink_version[MAX_CDR_LEN];
    char riscv_version[MAX_CDR_LEN];
    char model[MAX_CDR_LEN];
    char reset_switch;
    char reserve[MAX_CDR_LEN - 1];
};

struct send_data_info {
    enum ds_net_opcode_type_ext cmd;
    char *in_buf;
    char *out_buf;
    unsigned int size_out;
    unsigned int size_in;
};

#define TOOL_SET_SEND_DATA(_send_data, _cmd, _in_buf, _size_in, _out_buf, _size_out) \
    do {                                                                             \
        (_send_data).cmd = (_cmd);                                                   \
        (_send_data).in_buf = (_in_buf);                                             \
        (_send_data).size_in = (_size_in);                                           \
        (_send_data).out_buf = (_out_buf);                                           \
        (_send_data).size_out = (_size_out);                                         \
    } while (0)

struct param_check_info {
    char *param_name;
    int (*check_func)(char *);
};

struct tc_cfg_set {
    unsigned int tc_buff[TC_NUM];
    unsigned int waterline[TC_NUM];
};

struct tc_cfg_get {
    unsigned int tc_buff[TC_NUM];
    unsigned int up_waterline[TC_NUM];
    unsigned int low_waterline[TC_NUM];
};

struct ping_operate_info {
    unsigned int data_len;
    int ipv6_index;
    unsigned int dip;
    char str_ip[IP_ADDR_LEN];
    char str_ip6[IP6_ADDR_LEN];
};

#define VNIC_IP_HEADER 192
#define FEC_ERROR_COUNT_MAX 15
#define TC_MAX_NUM 8
#define NS_CHANGE_TO_US 1000
#define US_CHANGE_TO_MS 1000

struct ping_parse_param {
    char *param;
    int (*parse_proc)(struct ping_operate_info *operate_info, int *cur, int argc, char **argv);
};

struct ping_parse_ext {
    char *param;
    int (*parse_proc)(hccs_ping_operate_info *operate, int *argc_index, char **argv, int min, int max);
    int min;
    int max;
};

struct ds_ipv6_route_table_value {
    struct in6_addr address;
    unsigned int prefix_length;
    struct in6_addr next_address;
};

struct ds_ipv6_route_table_character {
    char address[INET6_ADDRSTRLEN];
    unsigned int prefix_length;
    char next_address[INET6_ADDRSTRLEN];
};

struct rx_pcs_info {
    unsigned int err_cnt;
    unsigned int err_block_cnt;
    unsigned int e_blk_cnt;
    unsigned int dec_err_blk_cnt;
    unsigned int test_err_cnt;
    unsigned int lane_bip_err_cnt;
};

struct ds_link_down_info {
    struct rx_pcs_info pcs_info;
    unsigned int pcs_link;
    unsigned int mac_link;
    unsigned int rf_lf;
    unsigned int reserve[3]; // reserve filed 3
};

#define MAX_LINK_DOWN_NUMS 5
struct ds_link_down_data {
    struct ds_link_down_info down_info[MAX_LINK_DOWN_NUMS];
    unsigned int down_info_idx;
    unsigned int total_nums;
};

union troute_value {
    int *int_value;
    unsigned int *unint_value;
};

struct traceroute_parse_param {
    union troute_value value;
    char *param;
    int min_num;
    int max_num;
    bool is_used;
};

struct ds_fec_error_info {
    unsigned long long cw_sym_err_cnt[FEC_ERROR_COUNT_MAX];
    unsigned long long cw_dec_all_cnt;
    unsigned long long cw_fail_err_cnt;
    unsigned long long reserved;
};

struct ds_cw_bad_cnt_thd {
    int chip_id;
    unsigned int cw_bad_cnt_thd;
};

enum ds_ippair_op_type {
    DS_IPPAIR_SET = 0,
    DS_IPPAIR_DEL,
    DS_IPPAIR_BUTT
};

struct ds_roce_port_ippair {
    union {
        struct in_addr ipv4;
        struct in6_addr ipv6;
    } sip;
    union {
        struct in_addr ipv4;
        struct in6_addr ipv6;
    } dip;
    int sip_family; // use address families macros, like AF_INET/AF_INET6
    int dip_family; // use address families macros, like AF_INET/AF_INET6
    int roce_port;  // value 4097- 65535
};

struct ds_update_ippair {
    enum ds_ippair_op_type op;
    struct ds_roce_port_ippair ippair;
};

struct ds_ippair_list {
    int count;
    struct ds_roce_port_ippair ippair[];
};

/* 降lane的工作模式 */

#define DOWNGRADE_LANE_NOT_WOKR 0x0
#define DOWNGRADE_LANE_WORK 0x1
/* 实际降Lane的几种状态 */

#define DOWNGRADE_LANE_IDLE 0x0
#define DOWNGRADE_LANE_SUCC 0x1
#define DOWNGRADE_LANE_FAIL 0x2

struct downgrade_lane_info {
    unsigned char master_flag;   /* 主从die标记 */
    unsigned char switch_flag;   /* 用户设置的降lane开关 0:disable; 1:enable */
    unsigned char work_mode;     /* 工作模式 0:not support; 1:support */
    unsigned char filter_time;   /* 滤抖时间 */
    unsigned char actual_status; /* 实际降lane情况:  */
};

int dsmi_start_roce_perftest(int device_id, struct perftest_cmd_info cmd_info[], unsigned int in_len,
                             unsigned int *out);
int dsmi_get_roce_perftest_status(int device_id, unsigned int is_client, struct perftest_curr_stat *perftest_curr);
int dsmi_get_roce_perftest_report(int device_id, unsigned int is_client, char *report, unsigned int len);
int dsmi_stop_roce_perftest(int device_id, unsigned int is_client);
int dsmi_reset_roce_perftest(int device_id, char *report);
int dsmi_get_eth_speed(int logic_id, int *speed);
int dsmi_get_eth_duplex(int logic_id, int *duplex);
int dsmi_get_ssu_reg(int logic_id, unsigned int *reg, unsigned int reg_size);
int dsmi_get_serdes_ctle_info(int logic_id, struct hilink_ctle_data *info);
int dsmi_get_serdes_dfe_info(int logic_id, struct hilink_dfe_data *info);
int dsmi_get_serdes_ffe_info(int logic_id, struct hilink_ffe_data *info);
int dsmi_get_fec_info(int logic_id, int *info);
int dsmi_set_fec_info(int logic_id, char mode);
int dsmi_get_pcs_info(int logic_id, struct hilink_port_info *info);
int dsmi_get_adapt_status_info(int logic_id, int port_id, struct hilink_adapt_status_info *info);
int dsmi_trig_query_cdr_info(int logic_id, struct trig_get_cdr_info *info);
int dsmi_query_mac_lane_cdr_info(int logic_id, struct query_mac_lane_info *info);
int dsmi_get_cdr_reg_info(int logic_id, struct query_cdr_reg_info *info);
int dsmi_send_data(int logic_id, int port_id, struct send_data_info *data_info);
int dsmi_get_pfc_enabled(int logic_id, int port_id, char *pfc_enabled);
int dsmi_set_pfc_enabled(int logic_id, int port_id, char *pfc_enabled, unsigned int priority_num);
int dsmi_get_tc_cfg(int logic_id, int port_id, struct tc_cfg_get *tc_cfg);
int dsmi_set_tc_cfg(int logic_id, int port_id, struct tc_cfg_set *tc_cfg);
int dsmi_get_current_tc_used(int logic_id, int port_id, int tc_id, unsigned int *tc_used);
int dsmi_get_prio_tc_info(int logic_id, int port_id, char *prio_tc);
int dsmi_set_prio_tc_info(int logic_id, int port_id, char *prio_tc, unsigned int priority_num);
int dsmi_ping_fun(int logic_id, int phy_id, char *inbuf, struct ping_operate_info *operate_info, unsigned int pkt_len);
int dsmi_get_gratuitous_arp_info(int logic_id, struct gratuitous_arp_info *info);
int dsmi_set_gratuitous_arp_info(int logic_id, struct gratuitous_arp_info *info);
int dsmi_set_optical_auto_adapt(int logic_id, char *inbuf, unsigned int inbuf_len);
int dsmi_get_route_table_ipv6(int logic_id, int port_id, char *route, unsigned int len);
int dsmi_add_route_table_ipv6(int logic_id, int port_id, struct ds_ipv6_route_table_value *route_param, char *outbuf,
                              unsigned int len);
int dsmi_del_route_table_ipv6(int logic_id, int port_id, struct ds_ipv6_route_table_value *route_param, char *outbuf,
                              unsigned int len);
int dsmi_get_eth_down_data(int logic_id, struct ds_link_down_data *down_data);
int dsmi_clear_extra_statistics_info(int logic_id, int port);
int dsmi_get_fec_error_info(int logic_id, struct ds_fec_error_info *info, int mac_id);
int dsmi_get_xsfp_reset_info(int logic_id, struct xsfp_reset_info *reset_info);
int dsmi_set_xsfp_reset_info(int logic_id, struct xsfp_reset_info *reset_info);
int dsmi_set_host_info(int logic_id, struct ds_host_info *host_info);
int dsmi_set_roce_port_ippair(int logic_id, int port_id, struct ds_roce_port_ippair *ippair);
int dsmi_del_roce_port_ippair(int logic_id, int port_id, struct ds_roce_port_ippair *ippair);
int dsmi_clear_roce_port_ippair(int logic_id, int port_id);
int dsmi_get_roce_port_ippair(int logic_id, int port_id, struct ds_ippair_list *list);
int dsmi_get_downgrade_lane_info(int logic_id, struct downgrade_lane_info *info);
int dsmi_set_downgrade_lane_info(int logic_id, struct downgrade_lane_info *info);
int dsmi_get_serdes_info_steady(int logic_id, struct ds_serdes_info *serdes_info);
int dsmi_get_serdes_info_flash(int logic_id, struct ds_serdes_info *serdes_info);
int dsmi_get_qp_context(int logic_id, int port_id, unsigned int qpn, char *context);
int dsmi_set_roce_cw_bad_cnt_thd(int logic_id, unsigned int cw_bad_cnt_thd);
int dsmi_get_roce_cw_bad_cnt_thd(int logic_id, struct ds_cw_bad_cnt_thd *info);
int dsmi_get_cdr_fmea(int logic_id);

enum ds_retimer_type {
    DS_CDR_RETIMER_TYPE_DEFAULT = 0,
    DS_CDR_RETIMER_TYPE_5901,
    DS_CDR_RETIMER_TYPE_HUYANG,
    DS_CDR_RETIMER_TYPE_INVALID,
};

#define CDR_RESET_DELAY_TIME_MAX 60

struct ds_scdr_reset_info {
    int dev_id;                 /**< die ID */
    unsigned char retimer_type; /**< cdr类型, enum ds_retimer_type 类型；当前仅HY支持scdr_reset参数设置查询*/
    unsigned char delay_time;  /**< 延时时间， 范围为[0-60](s)，如果未设置，默认为30*/
    unsigned char reserved[4]; /**< 保留字段 */
};

int dsmi_set_scdr_reset_info(int logic_id, struct ds_scdr_reset_info *info);

struct sds_esd_info {
    unsigned char sds_init_flag; /* 1:sds已经初始化， 0:sds未初始化 */
    unsigned char sds_esd_flag;  /* 1:sds esd pass,  0:sds esd fail. 需结合 sds_init_flag 一起看 */
    unsigned int offset;         /* adc 和 mid code的差值 */
};
#define CDR_ESD_MAX_LANE_NUM 16

struct sds_esd_info_array {
    int dev_id;
    int snr_pass;
    struct sds_esd_info info[CDR_ESD_MAX_LANE_NUM];
};

struct ds_optical_snr_query_info {
    unsigned char switch_status;
    unsigned char snr_support;
    unsigned char master_flag;
};

int dsmi_get_scdr_esd_status_cmd(int logic_id, struct sds_esd_info_array *info, int buf_size);
int dsmi_get_scdr_reset_info(int logic_id, struct ds_scdr_reset_info *info);
int dsmi_operate_diagnostic_mode(int logic_id, struct ds_diag_mode_params *ds_diag_mode_params,
                                 enum ds_net_opcode_type_ext cmd);
int dsmi_operate_tc_qos_info(int logic_id, char *params, size_t len, enum ds_net_opcode_type_ext cmd);
int dsmi_get_optical_snr_query_info(int logic_id, struct ds_optical_snr_query_info *info);
int dsmi_set_optical_snr_query_info(int logic_id, struct ds_optical_snr_query_info *info);
int dsmi_set_device_offline_nic_down_flag(int logic_id, int enable_flag);
#endif
