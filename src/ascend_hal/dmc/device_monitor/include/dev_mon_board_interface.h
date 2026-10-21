/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_MON_BOARD_INTERFACE_H
#define DEV_MON_BOARD_INTERFACE_H

#include <stdint.h>
#include "dsmi_common_interface.h"

#define DEV_BOARDID_PCIE_VA 1
#define DEV_BOARDID_PCIE_VC 3
#define DEV_BOARDID_PCIE_VD 5
#define DEV_BOARDID_PCIE_MAX_ID 299
#define DEV_BOARDID_PCIE_MIN_ID 200

#define DEV_BOARDID_PCIE_LOW_LIMIT 200
#define DEV_BOARDID_PCIE_UPPER_LIMIT 299
#define DEV_BOARDID_EVB_LOW_LIMIT 900
#define DEV_BOARDID_EVB_UPPER_LIMIT 949

#define MINI_BOARD_POWER_REG_BASE 0x1100c3000
#define MINI_BOARD_POWER_REG_OFFSET 0x434

#define IP_NAME_LENGTH 0xA
#define DEVICE_IP_NAME "end"

#ifdef DEV_MON_UT
#define STATIC
#else
#define STATIC static
#endif

#define MAX_MATRIX_LINE_SIZE 512
#define DEVELOPMENT_BOARD_TYPE_FIELD_NUM 11
#define PCIE_BOARD_TYPE_FIELD_NUM 8
#define RC35XX_BOARD_TYPE_FIELD_NUM 7

#ifndef MAX_MATRIX_PROC_NUM
#define MAX_MATRIX_PROC_NUM 256
#endif

#define MAX_TOP_USER_LEN 32
#define MAX_TOP_PR_NI_LEN 16
#define MAX_TOP_STATE_LEN 8
#define MAX_TOP_TIME_LEN 16
#define MAX_TOP_COMMAND_LEN 64
#define MAX_TOP_VSZ_LEN 16
#define MAX_COMMAND_LEN 256

#ifdef CFG_SOC_PLATFORM_CLOUD
#define CHIP_INFO_REG_BASE 0x8000F000
#define CHIP_INFO_REG_OFFSET 0xFF8
#else
#define CHIP_INFO_REG_BASE 0x10015e000
#define CHIP_INFO_REG_OFFSET 0X1C
#endif
#define MON_MAP_SIZE 4096
#define CHIP_OFFSET 0x200000000000

typedef struct top_development_board_type {
    int pid;
    char user[MAX_TOP_USER_LEN];
    char pr[MAX_TOP_PR_NI_LEN];
    char ni[MAX_TOP_PR_NI_LEN];
    unsigned long virt;
    unsigned long res;
    unsigned long shr;
    char s[MAX_TOP_STATE_LEN];
    float cpu;
    float mem;
    char time[MAX_TOP_TIME_LEN];
    char command[MAX_TOP_COMMAND_LEN];
} TOP_DEVELOPMENT_BOARD_TYPE;

typedef struct top_pcie_board_type {
    int pid;
    int ppid;
    char user[MAX_TOP_USER_LEN];
    char stat[MAX_TOP_STATE_LEN];
    char vsz[MAX_TOP_VSZ_LEN];
    float vsz_percent;
    int cpu;
    float cpu_percent;
    char command[MAX_TOP_COMMAND_LEN];
} TOP_PCIE_BOARD_TYPE;

typedef union top_board_type {
    TOP_DEVELOPMENT_BOARD_TYPE development_type;
    TOP_PCIE_BOARD_TYPE pcie_type;
} TOP_BOARD_TYPE;

typedef struct dmp_matrix_proc_info_stru {
    unsigned int pid;
    unsigned int mem_rate;
    unsigned int cpu_rate;
    char command[MAX_COMMAND_LEN];
} DMP_MATRIX_PORC_INFO_S;

int dm_get_device_flash_info(unsigned int dev_id, unsigned int flash_index, struct dm_flash_info_stru *pflash_info);
int dm_get_ecc_statistics(unsigned char dev_type, unsigned char error_type, unsigned int *ecc_count);
int dm_ecc_config_enable(unsigned char device_type, unsigned char value);
int dm_ecc_get_enable(unsigned char device_type, unsigned char *value);
int dm_set_mini_device_ip_info(unsigned int ip_addr, unsigned int netmask);
int dm_get_mini_device_ip_info(unsigned int *ip_addr, unsigned int *netmask);

#endif
