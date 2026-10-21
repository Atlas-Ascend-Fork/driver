/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef _HBM_CTRL_H_
#define _HBM_CTRL_H_
#include <stdint.h>
#include "hbm_user_type.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#ifdef CFG_SOC_PLATFORM_CLOUD
#define DEVICE_NUM_MAX 4
#elif defined CFG_SOC_PLATFORM_MINIV2
#define DEVICE_NUM_MAX 2
#else
#define DEVICE_NUM_MAX 1
#endif

#define MAX_RECORD_ECC_ADDR_COUNT 64
#define UINT32_BIT_SIZE 32
#define ECC_ERROR_TYPE_COUNT 2

#if defined CFG_FEATURE_HBM_ECC_ISOLATION || defined CFG_FEATURE_DDR_ECC_ISOLATION
struct ecc_statistics_s_all {
    /* Cumulative number of HBM single bit ECC error reports */
    uint32_t hbm_single_bit_count;
    /* Cumulative number of HBM multiple bit ECC error reports */
    uint32_t hbm_mul_bit_count;
    uint32_t hbm_single_bit_record_count;
    uint32_t hbm_mul_bit_record_count;
};
#else
struct ecc_statistics_s_all {
    uint16_t ddr_single_bit_count;
    /* Cumulative number of DDRC multiple bit ECC error reports */
    uint16_t ddr_mul_bit_count;
    /* Cumulative number of HBM single bit ECC error reports */
    uint16_t hbm_single_bit_count;
    /* Cumulative number of HBM multiple bit ECC error reports */
    uint16_t hbm_mul_bit_count;
    uint16_t ddr_single_bit_record_count;
    uint16_t ddr_mul_bit_record_count;
    uint16_t hbm_single_bit_record_count;
    uint16_t hbm_mul_bit_record_count;
};
#endif

#if defined CFG_FEATURE_HBM_FLASH || defined CFG_FEATURE_DDR_FLASH
#define HIGH_ADDR_SID_BITS_COUNT 4
#define LOW_ADDR_ROW_BITS_OFFSET 8
#else
#define HIGH_ADDR_COLUMN_BITS_COUNT 14
#define LOW_ADDR_RANK_BITS_OFFSET 4
#endif

#pragma pack(1)
/* Force single byte alignment to return to out of band fetch */
typedef struct multi_ecc_time_data {
    uint32_t multi_record_count;
    uint32_t multi_ecc_times[MAX_RECORD_ECC_ADDR_COUNT];
} MULTI_ECC_TIMES;

struct ecc_common_data_s {
    uint64_t physical_addr;
    uint32_t stack_pc_id;
    uint32_t reg_addr_h;
    uint32_t reg_addr_l;
    uint32_t ecc_count;
    int timestamp;
};
#pragma pack()

int dev_ecc_config_get_total_isolated_pages_info(unsigned int dev_id, struct ecc_statistics_s_all *pvalue);
int dev_ecc_config_clear_isolated_info(unsigned int dev_id);
int dev_ecc_config_get_multi_ecc_time_info(unsigned int dev_id, MULTI_ECC_TIMES *multi_eccs_timestamp);
int dev_ecc_config_get_multi_ecc_info(unsigned int dev_id, unsigned int data_index,
                                      struct ecc_common_data_s *ecc_detail_info);
int dev_ecc_config_get_single_ecc_info(unsigned int dev_id, unsigned int data_index,
                                       struct ecc_common_data_s *ecc_detail_info);
int dev_ecc_config_get_ecc_addr_count(unsigned int dev_id, unsigned char err_type,
                                      unsigned int count_value[ECC_ERROR_TYPE_COUNT]);
int dev_ecc_config_get_va_info(unsigned int devId, void *buf, unsigned int *size);
#endif
