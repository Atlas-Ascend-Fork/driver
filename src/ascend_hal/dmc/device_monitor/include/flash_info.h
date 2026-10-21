/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef FLASH_INFO_H
#define FLASH_INFO_H

#include "dsmi_common_interface.h"

#define FLASH_INFO_NAME "/proc/flash/flash_chip0_info"
#define STATE_INFO_NAME "/proc/flash/flash_state_info"

#ifdef CFG_SOC_PLATFORM_CLOUD
#define BUF_COUNT 4096
#else
#define BUF_COUNT 512
#endif

#ifndef CFG_SOC_PLATFORM_MINI
#define OFFSET_flash 140
#else
#define OFFSET_flash 100
#endif

#define OFFSET_state 12
#define CHAR_MAX_LEN 1024
#define FLASH_SIZE_UNIT_KB 1024

int flash_open(const char *pathname, int flags, int mode);
char *flash_malloc(size_t size); // lint !e101 !e132
int state_sscanf(const char *buffer, const char *format, unsigned int *buffer1);

#endif
