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
#ifndef FMS_MODULE_H
#define FMS_MODULE_H

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#include "dms_dtm_init.h"
#include "dms_smf_init.h"

#ifndef CFG_EDGE_HOST
#include "fpdc_init.h"
#endif

#endif
