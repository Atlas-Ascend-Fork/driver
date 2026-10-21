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

#ifndef TSDRV_STATUS_H
#define TSDRV_STATUS_H

#include "ka_base_pub.h"

#include "devdrv_user_common.h"
#include "tsdrv_kernel_common.h"

struct tsdrv_mng {
    ka_atomic_t status;
};

void tsdrv_status_init(void);
bool tsdrv_is_ts_work(u32 devid, u32 tsid);
bool tsdrv_is_ts_sleep(u32 devid, u32 tsid);

void tsdrv_set_ts_status(u32 devid, u32 tsid, enum devdrv_ts_status status);
enum devdrv_ts_status tsdrv_get_ts_status(u32 devid, u32 tsid);

#endif
