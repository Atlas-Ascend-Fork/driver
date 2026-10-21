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

#include "pbl/pbl_uda.h"
#include "securec.h"
#include "dms_define.h"
#include "devdrv_user_common.h"
#include "devdrv_common.h"
#include "ka_kernel_def_pub.h"
#include "ascend_kernel_hal.h"

#ifndef CFG_HOST_ENV
int hal_kernel_get_device_chip_die_id(u32 dev_id, u32 *chip_id, u32 *die_id)
{
    if ((chip_id == NULL) || (die_id == NULL) || (!uda_is_udevid_exist(dev_id))) {
        dms_err("Invalid parameter. (dev_id=%u; chip_id=%s; die_id=%s)\n", dev_id, (chip_id == NULL) ? "NULL" : "OK",
                (die_id == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    return devdrv_get_chip_die_id(dev_id, chip_id, die_id);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_device_chip_die_id);
#endif

int __attribute__((weak)) hal_kernel_get_d2d_topology_type(u32 dev_id1, u32 dev_id2,
                                                           HAL_KERNEL_TOPOLOGY_TYPE *topology_type)
{
    return -EOPNOTSUPP;
}
