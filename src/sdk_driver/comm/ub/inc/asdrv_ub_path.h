/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _ASDRV_UB_PATH_H_
#define _ASDRV_UB_PATH_H_

#include "asdrv_ub_load.h"

#define UBDRV_MAX_CFG_FILE_SIZE (1024 * 100)
#define UBDRV_CONFIG_OK 0
#define UBDRV_CONFIG_FAIL 1
#define UBDRV_OPEN_CFG_FILE_TIME_MS 100
#define UBDRV_OPEN_CFG_FILE_COUNT 50
#define UBDRV_CONFIG_NO_MATCH 1

int ubdrv_get_env_value_from_file(char *file, const char *env_name, char *env_val, u32 env_val_len);
#endif