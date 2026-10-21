/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_MONO_API_H
#define DEV_MONO_API_H
#include "dsmi_common_interface.h"
#include "dsmi_common.h"

#define CLOCK_ID_STRLEN 6 // the length of the "dpclk=" string
#define CMDLINE_BUFFER_SIZE 4096
#define CLOCK_VIRTUAL 100
#define CLOCK_REAL 0
#define CMDLINE_FILE_PATH "/proc/cmdline"

#define DEVICE_SERIAL_NUMBER_MAX 32
#define DEVICE_MAC_INFO_MAX 6

#define MIN_FAN_ID 0X0
#define MAX_FAN_ID 0X09
#define MIN_FAN_SPEED 25
#define MAX_FAN_SPEED 100

#define MAX_OPCODE_COUNT 256

#define DEVICE_NAME_MAX_LEN 32

#define VER_MAX_LEN 64
#define ECC_RETURN_VALUE_LEN 2

#define I2C_HEARTBEAT_DISCONNECT_STATUS 0
#define I2C_HEARTBEAT_CONNECT_STATUS 1
#define NOT_SUPPORT_I2C_KEEPER 0xffff
#define MIN_DISCONN_COUNT 5

#define USR_CONFIG_MALLOC_MAX 1024
#define USR_CONFIG_NAME_MAX 128U

#define ECC_CONFIG_NAME "ddr_ecc_enable"
#define ECC_BUF_SIZE 1

#define FREQUENCY_MEMERY 0
#define FREQUENCY_CPU 1

#define MAC_ID_MIN 0X0
#define MAC_ID_MAX 0X3
#define GET_MAC_COUNT 0XFF

#define MAC_INFO_TYPE 0X0
#define MAC_COUNT_TYPE 0X1

#define NET_DATA 2048
#define DSMI_ERRSTR_LENGTH 48
#define DSMI_EVENTSTR_LENGTH 256

#define REVOCATION_FILE_LEN_MAX 0x200000

#define IMU_MAX_ACK_LEN 32

#define INVALID_VALUE 0xff

#define UDIS_IP_NAME_LEN 8

#pragma pack(1)
typedef struct dsmi_fan_info {
    unsigned char fan_num;
    unsigned short fan_speed[MAX_FAN_ID];
} DSMI_FAN_INFO;

typedef struct dsmi_elabel_code {
    unsigned char elabel_type;
    unsigned char elabel_item;
    int elabel_len;
} DSMI_ELABEL_CODE;

#pragma pack()

typedef enum {
    DMP_ECC_CONFIG_ITEM = 0X0,
    DMP_P2P_CONFIG_ITEM = 0X1,
    DMP_DFT_CONFIG_ITEM = 0X2,
} DMP_CONFIG_ITEM;

typedef struct dsmi_config_para {
    unsigned char device_type;
    unsigned char config_item;
    unsigned char value;
} DSMI_CONFIG_PARA;

typedef struct dsmi_get_enable_para {
    unsigned char device_type;
    unsigned char config_item;
} DSMI_GET_ENABLE_PARA;

typedef struct dsmi_get_enable_value {
    unsigned char value;
} DSMI_GET_ENABLE_VALUE;

#define MAX_ETH_NAME_LEN 0x10
#define MAX_IP_LEN 0x10
#define IPV6_ARRAY_LEN 0X04
#define MAC_ADDR_LEN 0X06
#define MAC_COUNT_LEN 0X01
#define ENABLE_VALUE_LEN 0X01

typedef struct dsmi_mac_para {
    unsigned char mac_type;
    int mac_id;
} DSMI_MAC_PARA;

typedef struct dsmi_mac_address {
    unsigned char mac_address[MAC_ADDR_LEN];
} DSMI_MAC_ADDRESS;

typedef struct dsmi_mac_count {
    unsigned char mac_count;
} DSMI_MAC_COUNT;

typedef enum {
    VIRTUAL_NETWORK = 0X1,
    ROCE_NETWORK = 0X2
} DMP_NETWORK_ITEM;

typedef struct dsmi_port_para {
    unsigned char card_type;
    union {
        struct {
            unsigned char card_id : 6;
            unsigned char ip_type : 2;
        } fields;
        unsigned char value;
    } card_info;
} DSMI_PORT_PARA;

typedef union _ipaddr {
    unsigned int addr_v4;
    unsigned int addr_v6[IPV6_ARRAY_LEN];
} IPADDR_ST;

typedef struct dsmi_addr_para {
    IPADDR_ST ip_addr;
    IPADDR_ST mask_addr;
} DSMI_ADDR_PARA;

typedef struct dsmi_ip_info {
    unsigned char ip_type;
    IPADDR_ST ip_addr;
    IPADDR_ST mask_addr;
} DSMI_IP_INFO;

typedef struct dsmi_system_data {
    unsigned int system_data;
} DSMI_SYSTEM_DATA;

typedef struct dsmi_efuse_type {
    unsigned char efuse_type;
} DSMI_EFUSE_TYPE;

typedef struct dsmi_efuse_request {
    unsigned int efuse_start_addr;
    unsigned int efuse_write_len;
} DSMI_EFUSE_REQUEST;

typedef struct dsmi_efuse_check {
    unsigned int efuse_check_result;
} DSMI_EFUSE_CHECK;

typedef enum {
    WRITE_EFUSE = 0X1,
    BURN_EFUSE = 0X2,
    EFUSE_FLASH_POWER_OFF = 0X3,
    EFUSE_FLASH_POWER_ON = 0X4,
    GET_BRUN_STATE = 0X5
} EFUSE_CONFIG_ITEM;

#define EFUSE_BURN_NO_START 0X0
#define EFUSE_BURN_IDEL 0X1
#define EFUSE_BURN_PROCESS 0X2
#define EFUSE_BURN_S_VALUE 0X7d
#define EFUSE_BURN_WAIT_TIME 0X3
#define EFUSE_BURN_WAIT_CONT 20

typedef struct dsmi_sensor_info {
    unsigned char sensor_id;
} DSMI_SENSOR_INFO;

#define PMU_MAIN_VBUCK 0
#define PMU_MAIN_VOUT 1
#define PMU_SUB0_VBUCK 2
#define PMU_SUB1_VBUCK 3
#define PMU_SUB0_DEVICE_ID 0xB
#define PMU_SUB1_DEVICE_ID 0x2
#define PMU_TYPE_MAX_ID 4
#define PMU_CHANNEL_MAX_ID 20

struct pmu_voltage_stru {
    unsigned int pmu_type;
    unsigned int device_id;
    unsigned int channel;
    unsigned int get_value;
    int return_value;
};

#define PMU_DIEID_MAX_LEN 32

void dev_mon_api_get_device_capability(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_device_health(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_device_errorcode(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_device_temp(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_device_power(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_device_fw_ver(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_device_serial_number(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_device_voltage(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_device_name(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_driver_ver(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_davinci_information(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_peripheral_device_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_ecc_statistics(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_ecc_info_v2(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_system_time(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_board_id(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_pcb_ver(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_inject_fault(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);

void dev_mon_api_get_board_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_peripheral_fw_version(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_fan_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_mac_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_set_mac_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_set_ip_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_ip_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_gateway_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_set_gateway_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_net_dev_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_enable(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);

void dev_mon_api_passthru_mcu(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_elabel_data(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_chip_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_sensor_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_mini2mcu_status(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);

void dev_mon_api_get_user_config(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_set_user_config(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_clear_user_config(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_debug_send_data(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_cntpct(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_network_health(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_errstr(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_llc_perf_para(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_soc_die_id(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_soc_die_id_v2(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_freq_v2(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_aicpu_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_set_sec_revocation(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_set_power_state(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_hiss_status(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_lp_status(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_sensorhub_status(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);

void dev_mon_api_get_sensorhub_config(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_set_sensorhub_config(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_gpio_status(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_device_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_create_capability_group(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_delete_capability_group(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_capability_group_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
#ifdef CFG_FEATURE_CAPABILITY_GROUP
void dev_mon_api_create_capability_multi_group(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_delete_capability_multi_group(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_capability_multi_group(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
#endif
void dev_mon_api_set_device_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_set_device_info_ex(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_set_detect_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_detect_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_reboot_reason(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_fault_inject(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);

void dev_mon_get_persistence_statistics(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_clear_ecc_isolated_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_get_multi_ecc_time_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
int get_davinci_info(unsigned int dev_id, unsigned int vfid, REQ_CMD_D_INFO_ARG arg, unsigned int *ret_value);
void dev_mon_get_ecc_record_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_chip_expand_version(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
int dev_mon_get_mgnt_clockid(int *clockid);
void dev_mon_api_get_ub_link_status(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_mon_api_get_ub_port_status(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
#endif
