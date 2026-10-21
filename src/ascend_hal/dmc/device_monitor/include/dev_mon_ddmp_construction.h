/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_MON_DDMP_CONSTRUCTION_H
#define DEV_MON_DDMP_CONSTRUCTION_H

#include "dm_common.h"

#define DDMP_RESPONSE_BUF_LEN 32
typedef struct sendmsg_user_data {
    unsigned long sem_id;
    unsigned int buff_len;
    unsigned char resp_buff[DDMP_RESPONSE_BUF_LEN];
} SENDMSG_USER_DATA;

/* direct response on the device side */
int ddmp_get_device_capability(DM_INTF_S *intf, DM_RECV_ST *data, unsigned short identify, unsigned char type,
                               unsigned short capa_count, const unsigned short *op_buff);
int ddmp_get_device_health(DM_INTF_S *intf, DM_RECV_ST *data, unsigned char health);
int ddmp_get_device_state(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buf, unsigned int len);
int ddmp_get_device_errorcode(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned int *errorcode, int count);
int ddmp_get_device_temp(DM_INTF_S *intf, DM_RECV_ST *data, signed short temp);
int ddmp_get_device_power(DM_INTF_S *intf, DM_RECV_ST *data, unsigned short power);
int ddmp_get_device_fw_ver(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *ver, unsigned int len);
int ddmp_get_peripheral_device_fw_ver(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *ver, unsigned int len);
int ddmp_get_pcie_vender_id(DM_INTF_S *intf, DM_RECV_ST *data, unsigned short vid);
int ddmp_get_pcie_device_id(DM_INTF_S *intf, DM_RECV_ST *data, unsigned short did);
int ddmp_get_device_serial_number(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *sn,
                                  unsigned int total_length);
int ddmp_get_pcie_sub_vender_id(DM_INTF_S *intf, DM_RECV_ST *data, unsigned short sub_vid);
int ddmp_get_pcie_sub_device_id(DM_INTF_S *intf, DM_RECV_ST *data, unsigned short sub_did);
int ddmp_get_device_voltage(DM_INTF_S *intf, DM_RECV_ST *data, unsigned short volt);
int ddmp_get_board_id(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int board_id);
int ddmp_get_board_info(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int id_val);
int ddmp_get_pcb_ver(DM_INTF_S *intf, DM_RECV_ST *data, unsigned char pcb_ver);
int ddmp_get_device_name(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *name, unsigned int total_length);
int ddmp_get_driver_ver(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *ver, unsigned int ver_len);
int ddmp_get_davinci_information(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int ret_value);
int ddmp_get_reboot_reason(DM_INTF_S *intf, DM_RECV_ST *data, const void *buff, unsigned int len);
int ddmp_get_last_bootstate(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int state);
int ddmp_ctrl_device_node(DM_INTF_S *intf, DM_RECV_ST *data, const void *buf, unsigned int len);
int ddmp_get_bist_info(DM_INTF_S *intf, DM_RECV_ST *data, const void *buff, unsigned int len);
int ddmp_get_flash_content(DM_INTF_S *intf, DM_RECV_ST *data, const void *buf, unsigned int size);
int ddmp_get_ub_link_status(DM_INTF_S *intf, DM_RECV_ST *data, const void *buf, unsigned int size);
int ddmp_get_ub_port_status(DM_INTF_S *intf, DM_RECV_ST *data, const void *buf, unsigned int size);

/* peripheral device information, which may vary and needs to be expanded as needed */
int ddmp_get_peripheral_device_info(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff,
                                    unsigned int total_length);
int ddmp_get_ecc_statistics(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned int *value, unsigned int count);
int ddmp_get_system_time(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int system_clock);

/* a request needs to be sent to obtain data from the MCU, and then a response will be received on the COM interface */
int ddmp_get_fan_info_req(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_fan_info_rsp(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_dft_ctrl_rep(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int estimate_time);
int ddmp_get_dft_state(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_load_dft_testlib_res(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int rep);
int ddmp_get_soc_die_id(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_config_flash_mem_res(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int rep);
int ddmp_get_config_aging_test_res(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int rep);
int ddmp_get_aging_test_config(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_aging_test_res(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_send_failed_response(DM_INTF_S *intf, DM_RECV_ST *data, int errorcode);
int ddmp_get_freq_info(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int freq);
int ddmp_get_chip_info(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int chip_info);
int ddmp_get_chip_version(DM_INTF_S *intf, DM_RECV_ST *data, unsigned char chip_info);
int ddmp_get_device_cgroup_info(DM_INTF_S *intf, DM_RECV_ST *data, const void *buff, unsigned int len);
int ddmp_get_all_device_node(DM_INTF_S *intf, DM_RECV_ST *data, const void *buf, unsigned int len);

#define ddmp_send_normal_response(a, b) ddmp_send_failed_response(a, b, DRV_ERROR_NONE)
void fromat_failed_response(const DM_MSG_ST *req, DM_MSG_ST *resp, unsigned short errorcode);

int ddmp_get_fan_info(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_mac_info(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_ip_info(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_net_dev_info(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_gateway_info(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_efuse_info(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int check_result);
int ddmp_get_sensor_info(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_enable_info(DM_INTF_S *intf, DM_RECV_ST *data, char enable_flag);
int ddmp_get_elabel_data(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_sendmsg_to_mcu(unsigned char rw_flag, DM_MSG_ST *msg, SENDMSG_USER_DATA *user_data);
int ddmp_get_heartbeat_status(DM_INTF_S *intf, DM_RECV_ST *data, unsigned char status, unsigned int disconn_cnt);
int ddmp_get_user_config(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int buf_size, const char *buf);
int ddmp_get_debug_info(DM_INTF_S *intf, DM_RECV_ST *data, unsigned char *buff, unsigned char total_length);
int ddmp_get_pmu_voltage(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int pmu_voltage);
int ddmp_get_cntpct(DM_INTF_S *intf, DM_RECV_ST *data, const void *buff, unsigned int len);
int ddmp_get_network_health(DM_INTF_S *intf, DM_RECV_ST *data, unsigned int health_status);
int ddmp_get_bb_errstr(DM_INTF_S *intf, DM_RECV_ST *data, unsigned char *buff, unsigned int total_length);
int ddmp_get_llc_perf_para(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned int *buff, unsigned int total_length);
int ddmp_get_aicpu_info(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned int *buff, unsigned int total_length);
int dev_sendmsg_to_mcu(unsigned char rw_flag, DM_MSG_ST *msg, SENDMSG_USER_DATA *user_data);
int ddmp_passthru_rsp(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_send_no_data_response(DM_INTF_S *intf, DM_RECV_ST *data, unsigned char errorcode);
int ddmp_send_data_bin_response(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff,
                                unsigned int total_length);
int ddmp_get_common_struct_info(DM_INTF_S *intf, DM_RECV_ST *data, const void *buff, unsigned int total_length);
int ddmp_get_device_info(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned char *buff, unsigned int total_length);
int ddmp_get_persistence_statistics(DM_INTF_S *intf, DM_RECV_ST *data, const unsigned int *value,
                                    unsigned int total_length);
#endif
