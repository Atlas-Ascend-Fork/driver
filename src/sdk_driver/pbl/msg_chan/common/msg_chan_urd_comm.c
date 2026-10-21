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

#include "ka_kernel_def_pub.h"
#include "ascend_dev_num.h"
#include "msg_chan_main.h"

u64 g_prob_device_bitmap[ASCEND_DEV_MAX_NUM / KA_BITS_PER_LONG_LONG + 1];
void devdrv_set_probe_dev_bitmap(u32 devid)
{
    if (devid >= ASCEND_DEV_MAX_NUM) {
        devdrv_err("Invalid devid. (devid=%u)\n", devid);
        return;
    }
    ka_base_set_bit((int)devid, (unsigned long *)g_prob_device_bitmap);
}
KA_EXPORT_SYMBOL(devdrv_set_probe_dev_bitmap);

void devdrv_clr_probe_dev_bitmap(u32 devid)
{
    if (devid >= ASCEND_DEV_MAX_NUM) {
        devdrv_err("Invalid devid. (devid=%u)\n", devid);
        return;
    }
    ka_base_clear_bit((int)devid, (unsigned long *)g_prob_device_bitmap);
}
KA_EXPORT_SYMBOL(devdrv_clr_probe_dev_bitmap);

u64 devdrv_check_probe_dev_bitmap(u32 devid)
{
    if (devid >= ASCEND_DEV_MAX_NUM) {
        devdrv_err("Invalid devid. (devid=%u)\n", devid);
        return 0;
    }
    return (u64)ka_base_test_bit((int)devid, (const unsigned long *)g_prob_device_bitmap);
}
KA_EXPORT_SYMBOL(devdrv_check_probe_dev_bitmap);
