/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
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

#ifndef ESCHED_HOST_DC_ADAPT_ESCHED_H
#define ESCHED_HOST_DC_ADAPT_ESCHED_H

#ifdef CFG_FEATURE_EXTERNAL_CDEV
#define SCHED_MAX_CHIP_NUM 2048 /* dc host */
#else
#ifdef CFG_SOC_PLATFORM_CLOUD_V4
#define SCHED_MAX_CHIP_NUM 65 /* helper host (Independent working mode) + host local virtual dev(64) */
#else
#define SCHED_MAX_CHIP_NUM 64
#endif
#endif

#endif /* ESCHED_HOST_DC_ADAPT_ESCHED_H */
