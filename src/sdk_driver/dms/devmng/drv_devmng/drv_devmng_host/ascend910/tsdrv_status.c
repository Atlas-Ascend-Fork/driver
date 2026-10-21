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

#include "ascend_dev_num.h"
#include "devdrv_common.h"
#include "ka_base_pub.h"
#include "ka_kernel_def_pub.h"
#include "tsdrv_status.h"

static struct tsdrv_mng g_tsdrv_mng[ASCEND_DEV_MAX_NUM][DEVDRV_MAX_TS_NUM];

void tsdrv_set_ts_status(u32 devid, u32 tsid, enum devdrv_ts_status status)
{
    if (devid >= ASCEND_DEV_MAX_NUM || tsid >= DEVDRV_MAX_TS_NUM || status < 0 || status >= TS_MAX_STATUS) {
        devdrv_drv_err("devid:%u, tsid:%u, status:%u\n", devid, tsid, status);
        return;
    }

    ka_base_atomic_set(&g_tsdrv_mng[devid][tsid].status, status);
}
KA_EXPORT_SYMBOL(tsdrv_set_ts_status);

void tsdrv_status_init(void)
{
    int i, j;
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        for (j = 0; j < DEVDRV_MAX_TS_NUM; j++) {
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
            ka_base_atomic_set(&g_tsdrv_mng[i][j].status, TS_WORK);
#endif
        }
    }
}
KA_EXPORT_SYMBOL(tsdrv_status_init);

bool tsdrv_is_ts_work(u32 devid, u32 tsid)
{
    enum devdrv_ts_status status;

    if (devid >= ASCEND_DEV_MAX_NUM || tsid >= DEVDRV_MAX_TS_NUM) {
        devdrv_drv_err("devid:%u, tsid:%u\n", devid, tsid);
        return false;
    }

    status = ka_base_atomic_read(&g_tsdrv_mng[devid][tsid].status);

    return (status == TS_WORK);
}
KA_EXPORT_SYMBOL(tsdrv_is_ts_work);

bool tsdrv_is_ts_sleep(u32 devid, u32 tsid)
{
    enum devdrv_ts_status status;

    if (devid >= ASCEND_DEV_MAX_NUM || tsid >= DEVDRV_MAX_TS_NUM) {
        devdrv_drv_err("devid:%u, tsid:%u\n", devid, tsid);
        return false;
    }

    status = ka_base_atomic_read(&g_tsdrv_mng[devid][tsid].status);

    return (status == TS_SUSPEND);
}
KA_EXPORT_SYMBOL(tsdrv_is_ts_sleep);

enum devdrv_ts_status tsdrv_get_ts_status(u32 devid, u32 tsid)
{
    if (devid >= ASCEND_DEV_MAX_NUM || tsid >= DEVDRV_MAX_TS_NUM) {
        devdrv_drv_err("devid:%u, tsid:%u\n", devid, tsid);
        return TS_MAX_STATUS;
    }

    return ka_base_atomic_read(&g_tsdrv_mng[devid][tsid].status);
}
KA_EXPORT_SYMBOL(tsdrv_get_ts_status);
