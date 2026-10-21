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
#include "esched_drv_adapt.h"
#include "esched_drv.h"

int esched_drv_reset_phy_dev(u32 devid)
{
    (void)devid;
    return 0;
}

void esched_drv_restore_phy_dev(u32 devid)
{
    (void)devid;
}

int esched_publish_event_to_topic(u32 chip_id, u32 event_src, struct sched_published_event_info *event_info,
                                  struct sched_published_event_func *event_func)
{
    (void)chip_id;
    (void)event_src;
    (void)event_info;
    (void)event_func;
    return 0;
}
