/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#ifndef DMC_FEATURE_USER_H
#define DMC_FEATURE_USER_H

#include <stdint.h>
#include <stdbool.h>
#include "ascend_hal_define.h"
#include "dmc_feature_id_def.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

bool dmc_has_feature_id(uint32_t devid, enum dmc_feature_id feature_id);
bool dmc_has_feature_type(uint32_t devid, drvFeature_t type);
#endif /* DMC_FEATURE_USER_H */
