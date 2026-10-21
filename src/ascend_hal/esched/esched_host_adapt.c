/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "esched_platform.h"
#include "esched_feature.h"

int32_t esched_init_device_sched_cpu_num(unsigned int dev_id, int fd)
{
    (void)dev_id;
    (void)fd;
    return 0;
}

bool esched_is_host_virtual_dev(unsigned int dev_id)
{
#ifdef CFG_SOC_PLATFORM_CLOUD_V4
    uint32_t host_id;
    drvError_t ret = halGetHostID(&host_id);
    if (ret != DRV_ERROR_NONE) {
        sched_err("drv update host_id failed. (ret=%d)\n", ret);
        return false;
    }
    if (dev_id == host_id) {
        return true;
    }
#endif
    (void)dev_id;
    return false;
}

unsigned int esched_get_sync_pid(unsigned int dst_engine)
{
#if !defined(CFG_SOC_PLATFORM_HELPER)
    if ((dst_engine == ACPU_DEVICE) || (dst_engine == CCPU_DEVICE) || (dst_engine == DCPU_DEVICE)) {
        return (uint32_t)drvDeviceGetBareTgid();
    }
#endif
    return (uint32_t)GETPID();
}

unsigned int esched_get_sync_dst_engine(void)
{
    return CCPU_HOST;
}

void esched_fill_sync_msg_peer_platform(uint32_t dev_id, struct event_summary *event, const struct event_res *res)
{
    (void)res;
    uint32_t host_id = ESCHED_DEV_NUM;
    drvError_t ret = halGetHostID(&host_id);
    if (ret != DRV_ERROR_NONE) {
        sched_err("drv update host_id failed. (ret=%d)\n", ret);
    }
    if (dev_id == host_id) {
        ((struct event_sync_msg *)event->msg)->dst_engine = VIRTUAL_CCPU_HOST;
    }
}

/*
 * Wait for a sync event in slices, so that the death of the peer process can be
 * detected between two waits instead of blocking for the whole timeout.
 */
static int esched_wait_sync_event_by_slice(uint32_t dev_id, const struct sync_event_wait_info *wait_info,
                                           struct event_info *back_event_info, int32_t slice_time)
{
    int32_t timeout = wait_info->timeout;
    int32_t wait_time;
    int32_t query_ret;
    int ret;

    while (timeout > 0) {
        wait_time = (timeout > slice_time) ? slice_time : timeout;
        timeout = timeout - wait_time;
        ret = (int)esched_wait_event_ex(dev_id, wait_info->grp_id, wait_info->thread_id, wait_time, back_event_info);
        if (ret != DRV_ERROR_WAIT_TIMEOUT) {
            return ret;
        }
        query_ret = halQueryMasterPidByDeviceSlave(dev_id, wait_info->pid, NULL, NULL);
        if (query_ret != 0) {
            sched_run_info("Query device pid not success. (devid=%d, pid=%d; ret=%d)\n", dev_id, wait_info->pid,
                           query_ret);
            return query_ret;
        }
    }

    return DRV_ERROR_WAIT_TIMEOUT;
}

/* SVM sync event single wait time used in Cloud V4 platforms */
#define SVM_SYNC_EVENT_SINGLE_WAIT_TIME (500)       /* 500 ms */
#define TRS_SYNC_EVENT_SINGLE_WAIT_TIME (10 * 1000) /* 10s */
static bool esched_is_support_slice_and_get_slice_time(const struct sync_event_wait_info *wait_info,
                                                       int32_t *slice_time)
{
    if ((wait_info->subevent_id >= DRV_SUBEVENT_SVM_DEV_OPEN_MSG) &&
        (wait_info->subevent_id < DRV_SUBEVENT_HDC_CREATE_LINK_MSG) &&
        (wait_info->timeout > SVM_SYNC_EVENT_SINGLE_WAIT_TIME)) {
        *slice_time = SVM_SYNC_EVENT_SINGLE_WAIT_TIME;
        return true;
    }
    if ((wait_info->subevent_id >= DRV_SUBEVENT_TRS_ALLOC_RES_ID_MSG) &&
        (wait_info->subevent_id < DRV_SUBEVENT_ESCHED_SCHED_MODE_CHANGE_MSG) &&
        (wait_info->timeout > TRS_SYNC_EVENT_SINGLE_WAIT_TIME)) {
        *slice_time = TRS_SYNC_EVENT_SINGLE_WAIT_TIME;
        return true;
    }
    return false;
}

int esched_wait_sync_event(uint32_t dev_id, struct sync_event_wait_info *wait_info, struct event_info *back_event_info)
{
    int32_t slice_time = wait_info->timeout;

    if (esched_has_dev_feature(dev_id, ESCHED_SOC_PLATFORM_CLOUD_V4) &&
        esched_is_support_slice_and_get_slice_time(wait_info, &slice_time)) {
        return esched_wait_sync_event_by_slice(dev_id, wait_info, back_event_info, slice_time);
    }
    return (int)esched_wait_event_ex(dev_id, wait_info->grp_id, wait_info->thread_id, wait_info->timeout,
                                     back_event_info);
}

drvError_t esched_dev_feature_init_all(int fd)
{
    static struct esched_feature_get_arg all_features;
    drvError_t ret;

    all_features.user_feature_num = ESCHED_FEATURE_MAX;
    ret = esched_feature_ioctl_get_all(fd, &all_features);
    if (ret != DRV_ERROR_NONE) {
        sched_err("esched_dev_feature_init_all: query all features failed. (ret=%d)\n", ret);
        return ret;
    }
    (void)memcpy_s(g_esched_dev_feature, sizeof(g_esched_dev_feature), all_features.feature_bitmaps,
                   sizeof(all_features.feature_bitmaps));
    return DRV_ERROR_NONE;
}

int32_t esched_init_sched_cpu_num(unsigned int dev_id, int fd)
{
    (void)dev_id;
    (void)fd;
    return 0;
}

bool esched_is_svm_alloc_event(uint32_t dev_id, uint32_t event_id, uint32_t sub_event_id)
{
    if (!esched_has_dev_feature(dev_id, ESCHED_SOC_PLATFORM_CLOUD_V4)) {
        (void)event_id;
        (void)sub_event_id;
        return false;
    }
    return ((event_id == EVENT_DRV_MSG_EX) &&
            ((sub_event_id == SVM_SMM_MMAP_EVENT) || (sub_event_id == SVM_SMM_MUNMAP_EVENT) ||
             (sub_event_id == SVM_VA_RESERVE_EVENT)));
}
