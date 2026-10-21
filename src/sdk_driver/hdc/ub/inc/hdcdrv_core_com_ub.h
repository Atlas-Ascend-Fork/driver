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
#ifndef _HDCDRV_CORE_COM_H_
#define _HDCDRV_CORE_COM_H_

#include "dmc_kernel_interface.h"
#include "hdcdrv_log.h"
#include "hdcdrv_ka_memory.h"

#define HDCDRV_VALID 1
#define HDCDRV_INVALID 0

#define HDCDRV_KERNEL_DEFAULT_PID 0x7FFFFFFFULL
#define HDCDRV_INVALID_PID 0x7FFFFFFEULL
#define HDCDRV_RAW_PID_MASK 0xFFFFFFFFULL
#define HDCDRV_INVALID_VALUE (-1)

#ifdef DRV_UT
#define STATIC
#else
#define STATIC static
#endif

#endif /* _HDCDRV_CORE_COM_H_ */