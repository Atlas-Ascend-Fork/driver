/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DSMI_DFT_COMMON_H__
#define __DSMI_DFT_COMMON_H__

#define DEV_MON_CMD_GET_PMU_DIE_ID 0x80 /* board dsmi cmd start form 0x80 */

typedef struct dsmi_dft_code {
    unsigned char start_end : 2;
    unsigned char test_item_id : 6;
    unsigned char test_cmd;
} DSMI_DFT_CODE;

typedef enum dsmi_aging_type {
    DSMI_AGING_FlAG = 0x1,
    DSMI_AGING_TIME = 0x2,
    DSMI_AGING_RESULT
} DSMI_AGIND_TYPE;

typedef struct dsmi_aging_code {
    unsigned char aging_flag_time;
    unsigned char rev;
} DSMI_AGING_CODE;

typedef struct dsmi_aging_data {
    unsigned char aging_flag;
    unsigned short aging_time;
} DSMI_AGINE_DATA;

typedef struct dsmi_aging_result_data {
    unsigned short aging_result;
    unsigned int ddr_result1;
    unsigned int hbm_result;
    unsigned int ai_result2;
} DSMI_AGING_RESULT_DATA;

typedef enum dsmi_elabel_type {
    DSMI_WRITE_ELABEL = 0x0,
    DSMI_CLEAR_ELABEL = 0x1,
    DSMI_UPDATE_ELABEL = 0x2
} DSMI_ELABEL_TYPE;

typedef struct dsmi_get_elabel {
    unsigned char elabel_item;
} DSMI_GET_ELABEL;

typedef struct dsmi_test_lib {
    unsigned char test_type;
    char test_lib_name[32]; // 32 test_lib_name size
} DSMI_TEST_LIB;

typedef struct dsmi_dft_fw_data {
    unsigned char test_type;
} DSMI_DFT_FW_DATA;

/* DFT control response msg */
typedef struct dsmi_dft_control_res {
    unsigned int estimate_time;
} DSMI_DFT_CONTROL_RES;

/* get DFT status res */
#define MAX_RESULT_CODE_NUM 32
typedef struct dsmi_get_dft_state_res {
    unsigned char result_status;
    unsigned char process;
    unsigned short result_num;
    unsigned short result_code[MAX_RESULT_CODE_NUM];
} DSMI_GET_DFT_STATE_RES;

typedef enum dsmi_dft_cmd {
    DFT_SOC_INTERNEL_TEST = 1,
    DFT_PERIPHERAL_TEST,
    DFT_BUTTEN_TEST,
    DFT_LIGHT_ON_TEST,
    DFT_LOG_STATUS
} DSMI_DFT_CONTROL_CMD;

typedef enum dsmi_dft_status {
    DSMI_DFT_SUCCESS = 0,
    DSMI_DFT_FAIL = 1,
    DSMI_DFT_UNFINISHED
} DSMI_DFT_STATUS;

typedef enum dsmi_opcmd {
    GET_HEALTH_STATE = 0x01,
    GET_ERROR_CODE = 0x02,
    GET_CHIP_TEMP = 0x03,
    GET_POWER = 0x04,
    GET_VENDER_ID = 0x06,
    GET_DEVICE_ID = 0x07,
    GET_SUB_VENDER_ID = 0x09,
    GET_SUB_DEVICE_ID = 0x0a,
    GET_CHIP_VOLTAGE = 0x0b,
    GET_BOARD_ID = 0x0F,
    GET_PCB_ID = 0x10,
    GET_BOARD_INFO = 0x1B,
    GET_ECC_INFO = 0x1f,
    LOAD_TEST_LIB = 0x01,
    GET_DIE_ID = 0x02,
    GET_DAVINCHI_INFO = 0x05,
    GET_FLASH_INFO = 0x06,
    UPGRADE_CONTROL = 0x03,
    UPGRADE_STATE = 0x04,
    GET_DAVINCHI_VERSION = 0x07,
    DFT_CONTROL = 0x16,
    DFT_GET_STATE = 0x17,
    WRITE_ELABLE_DATA = 0x14,
    READ_ELABLE_DATA = 0x15,
    SET_AGING_FLAG_TIME = 0x22,
    GET_AGING_FLAG_TIME = 0x23,
    GET_AGING_RESULT = 0x24
} DSMI_OPCMD;

typedef enum dsmi_opfunc {
    DSMI_FUNC_COMMON = 0x00,
    DSMI_FUNC_DAVINCI = 0x06
} DSMI_OPFUNC;
int dsmi_cmd_get_pmu_voltage(int device_id, unsigned char pmu_type, unsigned char channel, unsigned int *volt_mv);
#endif // !__DSMI_DFT_COMMON_H__
