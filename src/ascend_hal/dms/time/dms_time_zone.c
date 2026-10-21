/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "ascend_hal.h"
#include "securec.h"
#include "ascend_hal_error.h"
#include "dms_user_common.h"
#include "dms_time_zone.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

STATIC pthread_mutex_t g_dms_time_sync_mutex = PTHREAD_MUTEX_INITIALIZER;
STATIC pthread_t g_dms_time_sync_thread;
STATIC u8 g_dms_time_sync_work = 0;
#define DMS_TIME_THREAD_STACK_SIZE (128 * 1024)

STATIC drvError_t dms_set_time_sync(void *data, struct dms_time_sync *interval_time)
{
    (void)data;
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SET_TIME_ZONE_SYNC;
    ioarg.filter_len = 0;
    ioarg.input = (void *)interval_time;
    ioarg.input_len = sizeof(struct dms_time_sync);
    ioarg.output = NULL;
    ioarg.output_len = 0;
    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        ret = (ret == DRV_ERROR_PARA_ERROR) ? DRV_ERROR_INNER_ERR : ret;
        DMS_EX_NOTSUPPORT_ERR(ret, "Set time sync failed. (ret=%d)\n", ret);
        return ret;
    }
    DMS_DEBUG("Set time sync success.\n");
    return DRV_ERROR_NONE;
}

STATIC void dms_get_iterval_seconds(struct dms_time_sync *interval_time)
{
    long interval;
    time_t local;
    time_t utc;
    struct tm tmp_ptr = {0};

    (void)time(&local);
    (void)gmtime_r(&local, &tmp_ptr);
    utc = mktime(&tmp_ptr);

    interval = local - utc;
    interval_time->interval_seconds = interval;

    DMS_DEBUG("Interval seconds between local seconds and utc. (interval=%lds)\n", interval);
}

STATIC void *dms_time_sync_serve_thread_device(void *data)
{
    struct dms_time_sync interval_time = {0};
    int ret;

    DMS_INFO("device manager time zone sync serve start.\n");

    (void)prctl(PR_SET_NAME, "dms_time_sync");

    while (g_dms_time_sync_work != 0) {
        (void)sleep(5); // 5s
        dms_get_iterval_seconds(&interval_time);
        ret = dms_set_time_sync(data, &interval_time);
        if (ret != 0) {
            DMS_EX_NOTSUPPORT_ERR(ret, "ioctl failed, ret(%d), stop time sync thread.\n", ret);
            break;
        }
    }

    DMS_INFO("device manager time zone sync serve stop.\n");
    return NULL;
}

/* only call once by device daemon, other process should never call this */
drvError_t dmsStartTimeSyncServeDevice(void)
{
    pthread_attr_t attr = {{0}};
    int ret;

    (void)pthread_mutex_lock(&g_dms_time_sync_mutex);

    if (g_dms_time_sync_work != 0) {
        (void)pthread_mutex_unlock(&g_dms_time_sync_mutex);
        DMS_INFO("time sync thread has already started.\n");
        return DRV_ERROR_NONE;
    }

    g_dms_time_sync_work = 1;

    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    (void)pthread_attr_setstacksize(&attr, DMS_TIME_THREAD_STACK_SIZE);
    ret = pthread_create(&g_dms_time_sync_thread, &attr, dms_time_sync_serve_thread_device, NULL);
    if (ret != 0) {
        DMS_ERR("pthread_create fail, unable to create timezone sync thread.\n");
        g_dms_time_sync_work = 0;
        (void)pthread_attr_destroy(&attr);
        (void)pthread_mutex_unlock(&g_dms_time_sync_mutex);
        return DRV_ERROR_SOCKET_CREATE;
    }
    (void)pthread_attr_destroy(&attr);

    (void)pthread_mutex_unlock(&g_dms_time_sync_mutex);

    return DRV_ERROR_NONE;
}