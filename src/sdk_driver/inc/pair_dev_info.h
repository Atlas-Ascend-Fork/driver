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

#ifndef PAIR_DEV_INFO_H
#define PAIR_DEV_INFO_H

#include <linux/types.h>

#include "pair_dev_info_comm.h"

struct devdrv_dev_id_info {
    u16 device_id;
    u16 vendor_id;
    u16 module_vendor_id;
    u16 module_id;
    u8 reserved[32];
};

/* used for devdrv_get_d2d_eid d2d_eid_type */
enum devdrv_d2d_eid_type {
    DEVDRV_D2D_EID_IN_BOARD = 0,
    DEVDRV_D2D_EID_CROSS_BOARD,
    DEVDRV_D2D_EID_TYPE_MAX
};

int devdrv_get_d2d_eid(u32 local_udevid, u32 d2d_eid_type, struct devdrv_pair_info_eid *eid, u32 *eid_index);
int devdrv_get_bus_instance_eid(u32 udevid, struct devdrv_pair_info_eid *eid);
int devdrv_get_dev_id_info(u32 udevid, struct devdrv_dev_id_info *info);
#endif