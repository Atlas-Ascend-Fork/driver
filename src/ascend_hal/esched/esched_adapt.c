/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "esched_ioctl.h"
#include "event_sched.h"
#include "ascend_hal_define.h"
#include "ascend_hal_error.h"
#include "esched_user_interface.h"

static THREAD__ int32_t attach_refcnt[ESCHED_DEV_NUM] = {0};
pthread_mutex_t esched_proc_mutex = PTHREAD_MUTEX_INITIALIZER;
static THREAD__ unsigned long long g_sched_usr_time = 0LL;
static THREAD__ unsigned long long g_sched_kernel_time = 0LL;

void esched_share_log_create(void)
{
#ifndef CFG_FEATURE_SYSLOG
    share_log_create(HAL_MODULE_TYPE_EVENT_SCHEDULE, SHARE_LOG_MAX_SIZE);
#endif
}

void esched_share_log_read(void)
{
#ifndef CFG_FEATURE_SYSLOG
    share_log_read_err(HAL_MODULE_TYPE_EVENT_SCHEDULE);
#endif
}

void esched_share_log_destroy(void)
{
#ifndef CFG_FEATURE_SYSLOG
    esched_share_log_read();
    share_log_destroy(HAL_MODULE_TYPE_EVENT_SCHEDULE);
#endif
}

void sched_set_k2u_time_delta(unsigned long long kernel_tick)
{
    if (kernel_tick > 0) {
        g_sched_usr_time = esched_get_cur_cpu_tick();
        g_sched_kernel_time = kernel_tick;
    }
    sched_info("Attach kernel tick. (user_tick=%llu; kernel_tick=%llu)\n", g_sched_usr_time, kernel_tick);
}

unsigned long long sched_adapt_curr_time(unsigned long long curr_tick)
{
#if defined(__arm__) || defined(__aarch64__) || defined(USER_EVENT_SCHED_UT)
    return curr_tick;
#else
    return (curr_tick - g_sched_usr_time + g_sched_kernel_time);
#endif
}

int32_t esched_attach_device_inner(unsigned int dev_id, struct sched_ioctl_para_attach *para_attach)
{
    int ret;
    if (esched_device_check(dev_id) != 0) {
        sched_err("The dev_id is invalid. (dev_id=%u; max=%u)\n", dev_id, ESCHED_LOGIC_DEV_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    (void)pthread_mutex_lock(&esched_proc_mutex);
    if ((attach_refcnt[dev_id]++) > 0) {
        (void)pthread_mutex_unlock(&esched_proc_mutex);
        sched_info("Attach refcnt++. (refcnt=%d; dev_id=%u)\n", attach_refcnt[dev_id], dev_id);
        return DRV_ERROR_NONE;
    }
    para_attach->dev_id = dev_id;
    ret = esched_dev_ioctl(dev_id, SCHED_ATTACH_PROCESS_TO_CHIP_ID, para_attach);
    if (ret != DRV_ERROR_NONE) {
        attach_refcnt[dev_id]--;
    } else {
        sched_set_k2u_time_delta(para_attach->kernel_time);
    }
    (void)pthread_mutex_unlock(&esched_proc_mutex);
    sched_info("Attach device. (ret=%d; dev_id=%u)\n", ret, dev_id);
    return ret;
}

int32_t esched_dettach_device_inner(unsigned int dev_id, struct sched_ioctl_para_detach *para_detach)
{
    int ret;

    if (esched_device_check(dev_id) != 0) {
        sched_err("The dev_id is invalid. (dev_id=%u; max=%u)\n", dev_id, ESCHED_LOGIC_DEV_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    (void)pthread_mutex_lock(&esched_proc_mutex);
    if ((--attach_refcnt[dev_id]) > 0) {
        (void)pthread_mutex_unlock(&esched_proc_mutex);
        sched_info("Attach refcnt--. (refcnt=%d; dev_id=%u)\n", attach_refcnt[dev_id], dev_id);
        return DRV_ERROR_NONE;
    }

    para_detach->dev_id = dev_id;

    ret = esched_dev_ioctl(dev_id, SCHED_DETTACH_PROCESS_FROM_CHIP_ID, para_detach);
    if (ret == DRV_ERROR_NONE) {
        esched_clear_grp_info(dev_id);
    }

    if (ret != DRV_ERROR_NONE) {
        attach_refcnt[dev_id]++;
    }
    (void)pthread_mutex_unlock(&esched_proc_mutex);
    sched_info("Detach device. (ret=%d; dev_id=%u)\n", ret, dev_id);
    return ret;
}

bool esched_need_judge_thread_id(void)
{
    return true;
}

bool esched_support_thread_swap_out(void)
{
    return true;
}

bool esched_support_thread_giveup(void)
{
#ifdef EMU_ST
    return true;
#endif
    return false;
}

bool esched_support_extern_interface(void)
{
    return true;
}

void esched_detach_device(unsigned int dev_id, struct sched_ioctl_para_detach *para_detach)
{
    int ret;

    if (dev_id >= ESCHED_DEV_NUM) {
        return;
    }

    (void)pthread_mutex_lock(&esched_proc_mutex);
    if (attach_refcnt[dev_id]++ > 0) {
        para_detach->dev_id = dev_id;
        ret = esched_dev_ioctl(dev_id, SCHED_DETTACH_PROCESS_FROM_CHIP_ID, para_detach);
        if (ret != DRV_ERROR_NONE) {
            sched_run_info("The dev_id close del process. (dev_id=%u; ret=%d)\n", dev_id, ret);
        }
    }
    (void)pthread_mutex_unlock(&esched_proc_mutex);
}

void esched_clear_attach_refcnt(unsigned int dev_id)
{
    (void)pthread_mutex_lock(&esched_proc_mutex);
    attach_refcnt[dev_id] = 0;
    (void)pthread_mutex_unlock(&esched_proc_mutex);
}
