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

#ifndef DEVDRV_SPEC_ADAPT_H
#define DEVDRV_SPEC_ADAPT_H

#include "ascend_dev_num.h"

#ifdef CFG_FEATURE_VF_USE_DEVID
#define DEVDRV_MAX_NODE_NUM ASCEND_DEV_MAX_NUM
#else
#define DEVDRV_MAX_NODE_NUM 4
#endif

#if defined(CFG_FEATURE_VF_USE_DEVID)
#define VFID_NUM_MAX 1
#else
#define VFID_NUM_MAX 32 /* virtual function id max num */
#endif

#endif