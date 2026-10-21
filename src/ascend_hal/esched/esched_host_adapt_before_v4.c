/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/*
 * The apm module (src/ascend_hal/dpa/apm) is only compiled and linked for CLOUD_V4.
 * esched_host_adapt.c references halQueryMasterPidByDeviceSlave unconditionally, so
 * a stub is provided here for the non-CLOUD_V4 build to keep libascend_hal.so linkable.
 * This file is only compiled when the apm module is absent, so there is no duplicate symbol.
 */

#include "dpa_user_interface.h"

int halQueryMasterPidByDeviceSlave(unsigned int devid, int slave_pid, unsigned int *master_pid, unsigned int *proc_type)
{
    (void)devid;
    (void)slave_pid;
    (void)master_pid;
    (void)proc_type;
    return 0;
}

/*
 * The upper layer of milan obp does not distinguish between host and dev.
 * The obj of the drvevent module is not linked on the host side. Therefore,
 * symbols halEventProc, halDrvEventThreadInit, halDrvEventThreadUninit are
 * provided on the host side for stubbing.
 */
#ifndef EMU_ST
drvError_t halEventProc(unsigned int devId, struct event_info *event)
{
    (void)devId;
    (void)event;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halDrvEventThreadInit(unsigned int devId)
{
    (void)devId;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halDrvEventThreadUninit(unsigned int devId)
{
    (void)devId;
    return DRV_ERROR_NOT_SUPPORT;
}
#endif
