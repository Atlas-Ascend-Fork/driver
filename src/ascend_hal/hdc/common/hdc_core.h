/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef _HDC_CORE_H_
#define _HDC_CORE_H_

#include "drv_type.h"

#define PKT_SIZE_1G (1024 * 1024 * 1024) // 1G Byte
#define MEM_SIZE_1M 0x100000             // 1M Byte

#define HUGE_PAGE_SIZE (2U * 1024U * 1024U)
#define HUGE_PAGE_MASK (~0x1FFFFFU)
#define PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64
#define PAGE_MASK (~0xFFFU)
#define PAGE_BIT 12
#define LAST_MASK 0x3FFFF // 256k-1

#define HDC_BIT_MAP_IS_HUGE(flag) (((flag) & HDC_FLAG_MAP_HUGE) >> 2)

#define HDC_MEM_ALLOC 1
#define HDC_MEM_FREE 0
#define HDC_MEM_LOG_LEVEL_WARN 0
#define HDC_MEM_LOG_LEVEL_ERR 1

#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA)
#define HDC_INIT_GET_HANDLE_COUNT 10000
#else
#define HDC_INIT_GET_HANDLE_COUNT 100
#endif
#define HDC_INIT_GET_HANDLE_COUNT_V3 1

#define HDC_SOCKET_DEFAULT_TIMEOUT 10000 /* 10ms */

/* hdc dfx */
#define HDC_DFX_INT_SIZEOF 4
#define HDC_DFX_16_BIT_MASK 0xffff
#define HDC_DFX_15_BIT_MASK 0x7fff
#define HDC_DFX_SHIFT_32 32

extern signed int hdc_handle_count;
extern signed int hdc_access_count;
extern signed int hdc_config_count;
extern signed int hdc_max_device_num;
extern unsigned int hdc_max_vf_devid_start;
signed int hdc_get_handle_count(void);
signed int hdc_get_access_count(void);
signed int hdc_get_config_count(void);
signed int hdc_get_max_device_num(void);
unsigned int hdc_get_max_vf_dev_id_start(void);
unsigned int hdc_get_alloc_len(unsigned int len, unsigned int flag);
bool hdc_is_in_ub(void);
void hdc_report_out_of_mem_err_msg(const char *func_name, size_t size);
#endif
