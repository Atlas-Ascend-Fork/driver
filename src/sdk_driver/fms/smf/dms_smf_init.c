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

#include "dms/dms_interface.h"
#include "dms_sensor_init.h"
#include "dms_sensor.h"
#include "dms_event.h"
#include "dms_smf_init.h"

int dms_smf_init(void)
{
    (void)dms_event_init();
    dms_sensor_init();
    return 0;
}

void dms_smf_exit(void)
{
    (void)dms_sen_exit_sensor_event();
    (void)dms_sensor_exit();
    dms_event_exit();
}
