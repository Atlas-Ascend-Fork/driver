/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_FLASH_CTL_H
#define DEV_FLASH_CTL_H

#define ALGIN_UP(size, align) (((size) + (align) - 1) & (~((align) - 1)))

#define ALGIN_DOWN(size, align) (((size) / (align)) * (align))

#define AGING_TEST_CONFIG_FLAG 0x1
#define AGING_TEST_CONFIG_TIME 0x2
#ifndef FLASH_BLOCK_SIZE
#define FLASH_BLOCK_SIZE 65536
#endif
#define FLASH_BLOCK_CHECK_SIZE (256 * 1024)

#define AGING_TEST_BLOCK_OFFSET 0x1A0000

#define AGING_TEST_MODE_START_FLAG 0x0FF0C33C

#define AGING_TEST_PART "/dev/mtd6"

#define P0_FLASH_TEST_PART "Test_P0_N"
#define P1_FLASH_TEST_PART "Test_P1_N"
#define P2_FLASH_TEST_PART "Test_P2_N"
#define P3_FLASH_TEST_PART "Test_P3_N"

#define P0_PART_NAME_RESERVER "Reserve_P0_N"

#define AGING_TEST_CONFIG_DATA_OFFSET 0x0

#define AGING_TEST_CONFIG_DATA_SIZE 128
#define AGING_TEST_RES_DATA_SIZE 128
#define AGING_TEST_DATA_READ_MAX_SIZE 2048
#define AGING_TEST_TIME_SET_OFFSET 4
#define AGING_TEST_TIME_QUERY_OFFSET 1
#define AGING_TEST_TIME_SIZE 2
#define DDR_AGING_TEST_FLAG 8
#define FLASH_AGING_TEST_FLAG 16
#define AI_AGING_TEST_FLAG 24
#define DDR_AGING_TEST_OFFSET 12
#define FLASH_AGING_TEST_OFFSET 20
#define AI_AGING_TEST_OFFSET 28
#define HBM_AGING_TEST_OFFSET 32

#define AGING_TEST_RES_LEN 16

#define DFT_TEST_FLAG_DATA_OFFSET (AGING_TEST_CONFIG_DATA_OFFSET + 0x80)

#define DFT_TEST_MODE_START_FLAG 0x0AFC0120

#define DDR_TEST_RES_BUFF_1 32
#define DDR_TEST_RES_BUFF_2 36
#define DDR_TEST_RES_BUFF_LEN 4
#define AI_TEST_RES_BUFF_LEN 4

#define PCIE_SRAM_AGINR_FALG_BASE_ADDRESS 0x11203D000
#define PCIE_SRAM_AGINR_FALG_BASE_OFFSET 0xBFC

#define PCIE_SRAM_AGINR_FALG_DATA 0x55AA55AA
#define PCIE_SRAM_DEV "/dev/mem"

#define PCIE_SET_AGINR_FALG 0x1
#define PCIE_NOT_SET_AGINR_FALG 0x0

int write_data_to_dft_area(unsigned int offset, char *data, unsigned int wr_len);
int read_data_from_dft_area(unsigned int offset, char *data, unsigned int rd_len);

#endif
