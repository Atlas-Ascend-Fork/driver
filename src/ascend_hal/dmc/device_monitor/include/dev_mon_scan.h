/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_MON_SCAN_H
#define DEV_MON_SCAN_H

#include "dms_user_interface.h"

#ifdef CFG_SOC_PLATFORM_CLOUD
#define DM_DEVICE_NUM_MAX 4
#else
#ifdef CFG_SOC_PLATFORM_MINIV2
#define DM_DEVICE_NUM_MAX 2
#else
#define DM_DEVICE_NUM_MAX 1
#endif
#endif

#define DM_MAX_INTERNEL_TIME 60000

#define DEV_SB_SLAVE "/dev/i2c0_slave"

#define BOARD_ID_MASK 0xfff0
#define EVB_PLATFORM 0x3000
#define SCAN_VALUE_CHAR_INVALID 0xff
#define SCAN_VALUE_SHORT_INVALID 0XFFFF
#define SCAN_VALUE_INT_INVALID (0xFFFFFFFFU)
#define SCAN_VALUE_TEMP_INVALID 32767

#define ERROR_CODE_MAX_NUM 128

#define SCAN_PAGE_SIZE 4096
#define SCAN_MAP_OFFSET 0

#define UNUSED_ARG 0
#define MAX_DATA_LEN 20
#define DATA_NO_REFRESH 2
#define VALID 1
#define INVALID 0
#define DMP_HEAD_LEN 12
#define OEN_ARG 1
#define TWO_ARG 2
#define THREE_ARG 3
#define FOUR_ARG 4
#define MAX_ITEM_LEN 64

#define I2C_VALID 1
#define I2C_INVALID 0

#define INFO_TYPE_MEM 0x01
#define INFO_TYPE_CPU 0x02
#define INFP_TYPE_HBM 0x03

#define SEEK_XSFP_FAIL_COUNT 5
#define ECC_VALUE_LEN 2

#define INFO_TYPE_RATE (0x00 << 4 & 0xf0)
#define INFO_TYPE_FREQ (0x01 << 4 & 0xf0)
#define INFP_TYPE_SIZE (0x02 << 4 & 0xf0)

#define ALL_TEMP_LEN 20
#define DMANAGE_ALL_TEMP_LEN 18
#define MAX_TEMP_ADDR 0
#define MIN_TEMP_ADDR 1

struct drv_all_temp {
    char aicore0_temp;
    char aicore1_temp;
    char aicore2_temp;
    char aicore3_temp;
    char aicore4_temp;
    char aicore5_temp;
    char aicore6_temp;
    char aicore7_temp;
    char aicore8_temp;
    char aicore9_temp;
    char cpu0_temp;
    char cpu1_temp;
    char cpu2_temp;
    char cpu3_temp;
    char dvpp_temp;
    char io_temp;
    char ao_temp;
    char isp_temp;
};

typedef struct drv_ioctl_all_temp_arg {
    union {
        struct drv_all_temp tmp;
        char tmpp[DMANAGE_ALL_TEMP_LEN];
    } drv_data;
} DRV_IOCTL_ALL_TEMP_ARG;

/* define the item list in mcu2mini_query_block */
enum item_location {
    VOLTAGE_VALUE = 0,
    TEMPER_VALUE = 1,
    POWER_VALUE = 2,
    HEALTH_STATE = 3,

    PCIE_VENDER_ID = 4,
    PCIE_SUBVENDER_ID = 5,
    PCIE_DEVICE_ID = 6,
    PCIE_SUBDEVICE_ID = 7,
    PCIE_BUS = 8,
    PCIE_DEVICE = 9,
    PCIE_FN = 10,
    PCIE_DAVINCI_ID = 11,

    D_SN = 12,
    FW_VERSION = 13,
    ECC_STATISTIC = 14,

    D_MEM_RATE = 20,
    D_MEM_FREQ = 21,
    D_MEM_BANDW_RATE = 22,
    D_MEM_SIZE_INFO = 23,

    D_CTRLCPU_RATE = 24,
    D_CPU_RATE = 25,
    D_CPU_FREQ = 26,

    D_AICPU_RATE = 27,
    D_AICORE0_FREQ = 28,
    D_AICORE1_FREQ = 29,

    D_HBM_RATE = 30,
    D_HBM_FREQ = 31,
    D_HBM_CAPACITY = 32,

    D_DDR_RATE = 33,
    D_DDR_CAPACITY = 34,

    FLASH_INFO = 35,

    D_HBM_BW_RATE = 36,

    D_ALL_TEMP = 37,

    MAX_ITEMS_LEN = 64,
};

typedef int (*DEV_MON_SCAN_HANDLE_T)(unsigned int id, void *update_value);

typedef struct scan_period_ctrl_ {
    unsigned int interval_cnt;
    unsigned int remain_cnt;
    struct timespec first_time;
} SCAN_PERIOD_CTRL;

typedef struct scan_proc_st_ {
    DEV_MON_SCAN_HANDLE_T func;
    SCAN_PERIOD_CTRL period_ctrl[DM_DEVICE_NUM_MAX];
    unsigned int value_size;
    void *value;
} SCAN_PROC_ST;

typedef struct errorcode_st {
    unsigned int err_count;
    unsigned int errorcode[ERROR_CODE_MAX_NUM];
} ERRORCODE_ST;
struct xsfp_info_str {
    unsigned fail_count;
    int xsfp_temperature;
    int fail_ret;
};

struct davinci_freq_ret {
    int dev_type;
    DEV_MON_SCAN_HANDLE_T func;
};

typedef struct d_info_st {
    unsigned int mem_freq;
    unsigned int CPU_freq; // CTRL_CPU_freq
    unsigned int HBM_freq;
    unsigned int AICORE0_freq;
    unsigned int AICORE1_freq;
    unsigned int MEM_rate; // DDR_rate
    unsigned int CPU_rate; // AICORE_rate
    unsigned int AI_CPU_rate;
    unsigned int CTRL_CPU_rate;
    unsigned int MEM_BANDWIDTH_rate;
    unsigned int HBM_BANDWIDTH_rate;
    unsigned int HBM_rate;
    unsigned int DDR_rate;
    unsigned int mem_info;
    unsigned int HBM_capacity;
    unsigned int DDR_capacity;
    unsigned int vector_rate;
    unsigned int vector_freq;
    struct xsfp_info_str xsfp_info;
    struct dmanage_aicpu_info_stru aicpu_info;
    unsigned int voltage_value;
    unsigned int power_value;
    signed int temp_value;
    unsigned int ddr_ecc[ECC_VALUE_LEN];
    unsigned int hbm_ecc[ECC_VALUE_LEN];
    unsigned char cluster_temp;
    unsigned char peri_temp;
    unsigned char aicore0_temp;
    unsigned char aicore1_temp;
    unsigned char aicore_limit;
    unsigned char aicore_total_per;
    unsigned char aicore_elim_per;
    unsigned short aicore_base_freq;
    unsigned short npu_ddr_rate;
    char thermal_temp[THERMAL_THRESHOLD_NUM];
    unsigned char soc_temp;
    int n_die_temp;
    int hbm_temp;
} D_INFO_ST;

struct d_sensor_info {
    unsigned int sensor_id;
    void *value;
};

#define DEV_MON_QUERY_DATA_ARG_LEN 4
#define DEV_MON_QUERY_DATA_LEN 20
typedef struct mcu_query_data_format {
    unsigned char op_cmd;
    unsigned char op_fun;
    unsigned char type_len;
    unsigned char arg[DEV_MON_QUERY_DATA_ARG_LEN];
    unsigned char valid;
    unsigned int total_length;
    unsigned int length;
    unsigned int offset;
    unsigned char data[DEV_MON_QUERY_DATA_LEN];
} MCU_QUERY_DATA_FORMAT;

typedef struct mcu2mini_query_block {
    int id;
    long updata_time;
    MCU_QUERY_DATA_FORMAT item_info[MAX_ITEM_LEN];
} MCU2MINI_QUERY_BLOCK;

int dev_mon_get_temperature(unsigned int id, int *temp);
int dev_mon_get_voltage(unsigned int id, unsigned int *voltage);
int dev_mon_get_power(unsigned int id, unsigned int *power);
int dev_mon_get_freq(unsigned int id, unsigned char type, unsigned int *freq);
int dev_mon_get_sensor(unsigned int id, unsigned int type, TAG_SENSOR_INFO *sensor);
int dev_mon_get_ai_core_utilization(unsigned int id, unsigned int *util);
int dev_mon_get_xsfp_info(unsigned int id, struct xsfp_info_str *xsfp_info);

int dev_mon_get_d_info_static(unsigned int id, D_INFO_ST *d_info);
int dev_mon_get_pcie_id_info_static(unsigned int id, struct dmanage_pcie_id_info *p_info);
int dev_mon_get_health_static(unsigned int id, unsigned int *phealth);
unsigned int dev_mon_get_result_len(unsigned int core_id);

void dev_mon_scan_proc(void);

#endif
