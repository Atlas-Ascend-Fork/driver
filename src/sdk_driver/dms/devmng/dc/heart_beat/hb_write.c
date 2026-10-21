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
#include "ka_common_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "devdrv_common.h"
#include "dms_define.h"
#include "dms_timer.h"
#include "soft_fault_define.h"
#include "heart_beat.h"

#ifdef CFG_ENV_FPGA
#define HEART_BEAT_TIMER_EXPIRE_SEC (1 * 50)
#define HB_FORGET_WHITE_JUDGE_TIME (12 * 50)
#else
#define HEART_BEAT_TIMER_EXPIRE_SEC 1
#define HB_FORGET_WHITE_JUDGE_TIME 12
#endif

#define SHM_NOT_INIT 1
struct hb_write_block {
    u64 count;
    u64 last_normal_time;
    u64 last_write_time;
    u64 miss_write_count;
    u32 device_status;
};

struct hb_write_timer {
    ka_hrtimer_t timer;
    ka_work_struct_t work;
    ka_timespec64_t last_write_time;
    ka_timespec64_t last_normal_time;
    unsigned long long forget_count; /* number of times heartbeat was not written for more than 12 seconds */
    int write_ret;
};

static struct hb_write_block g_hb_write_block[DEVICE_NUM_MAX] = {{0}};
#if (defined CFG_FEATURE_HEARTBEAT_CNT_PCIE) || (!defined CFG_HOST_ENV)
static struct hb_write_timer g_hb_write_timer = {{{{0}}}, {{0}}};
#endif

struct hb_write_block *hb_get_write_item(unsigned int dev_id)
{
    return &g_hb_write_block[dev_id];
}

#ifndef DMS_UT
#if (defined CFG_FEATURE_HEARTBEAT_CNT_PCIE) || (!defined CFG_HOST_ENV)
int hb_update_heartbeat_count(void)
{
    int ret;
    unsigned int dev_id;

    for (dev_id = 0; dev_id < DEVICE_NUM_MAX; dev_id++) {
        if (g_hb_write_block[dev_id].device_status != HEART_BEAT_READY) {
            continue;
        }
        g_hb_write_block[dev_id].count++;

        ret = hb_set_heart_beat_count(dev_id, g_hb_write_block[dev_id].count);
        if (ret != 0) {
            soft_drv_err("Device heartbeat set failed. (count=%llu; ret=%d; device=%u)\n",
                         g_hb_write_block[dev_id].count, ret, dev_id);
            return ret;
        }
    }

    return 0;
}

static bool hb_write_work_abnormal_check(struct hb_write_timer *timer_info, ka_timespec64_t current_time)
{
    unsigned long interval;

    interval = current_time.tv_sec - timer_info->last_write_time.tv_sec;
    if (timer_info->last_write_time.tv_sec != 0 && interval > HB_FORGET_WHITE_JUDGE_TIME) {
        timer_info->forget_count++;
        return true;
    }

    return false;
}

static ka_hrtimer_restart_t heart_beat_write_count(ka_hrtimer_t *htr)
{
    int ret;
    bool abnormal = false;
    struct hb_write_timer *info = NULL;
    ka_timespec64_t current_time;

    if (htr == NULL) {
        soft_drv_err("htr is NULL.\n");
        return KA_HRTIMER_NORESTART;
    }
    info = ka_container_of(htr, struct hb_write_timer, timer);
    ret = hb_update_heartbeat_count();
    if (ret != 0) {
        if (ret == SHM_NOT_INIT) {
            ka_system_hrtimer_forward_now(htr, ka_system_ktime_set(HEART_BEAT_TIMER_EXPIRE_SEC, 0));
            return KA_HRTIMER_RESTART;
        }
        info->write_ret = ret;
        abnormal = true;
    } else {
        info->write_ret = 0;
        abnormal = false;
    }

#ifndef DRV_SOFT_UT
    ka_system_jiffies_to_timespec64(ka_jiffies, &current_time);
#endif

    info->last_write_time = current_time;
    if (abnormal || hb_write_work_abnormal_check(info, current_time)) {
        ka_task_schedule_work(&info->work);
    } else {
        info->last_normal_time = current_time;
    }
    ka_system_hrtimer_forward_now(htr, ka_system_ktime_set(HEART_BEAT_TIMER_EXPIRE_SEC, 0));
    return KA_HRTIMER_RESTART;
}

STATIC void heart_beart_write_dfx_handler(ka_work_struct_t *work)
{
    struct hb_write_timer *timer_info = NULL;

    timer_info = ka_container_of(work, struct hb_write_timer, work);
    soft_drv_warn(
        "Don't write heartbeat for a long time."
        "(write heartbeat ret=%d; total forget count=%llu; last normal write time =%llds; last write time=%llds)\n",
        timer_info->write_ret, timer_info->forget_count, timer_info->last_normal_time.tv_sec,
        timer_info->last_write_time.tv_sec);
}
#endif

void heart_beat_write_status_init(u32 dev_id)
{
    struct hb_write_block *write_item = NULL;

    write_item = hb_get_write_item(dev_id);
    if (write_item == NULL) {
        return;
    }
    write_item->device_status = HEART_BEAT_READY;
    return;
}

void heart_beat_write_status_uninit(u32 dev_id)
{
    struct hb_write_block *write_item = NULL;

    write_item = hb_get_write_item(dev_id);
    if (write_item == NULL) {
        return;
    }
    if (write_item->device_status != HEART_BEAT_LOST) {
        write_item->device_status = HEART_BEAT_EXIT;
    }
    return;
}

int heart_beat_write_timer_init(void)
{
#if (defined CFG_FEATURE_HEARTBEAT_CNT_PCIE) || (!defined CFG_HOST_ENV)
    int i;

    for (i = 0; i < DEVICE_NUM_MAX; i++) {
        g_hb_write_block[i].count = 0;
        g_hb_write_block[i].device_status = HEART_BEAT_NOT_INIT;
    }
#if defined(CFG_HOST_ENV)
    ka_system_hrtimer_init(&g_hb_write_timer.timer, KA_CLOCK_MONOTONIC, KA_HRTIMER_MODE_REL_TYPE);
#else
    ka_system_hrtimer_init(&g_hb_write_timer.timer, KA_CLOCK_MONOTONIC, KA_HRTIMER_MODE_REL_PINNED);
#endif
    g_hb_write_timer.timer.function = heart_beat_write_count;
    KA_TASK_INIT_WORK(&g_hb_write_timer.work, heart_beart_write_dfx_handler);
#if defined(CFG_HOST_ENV)
    ka_system_hrtimer_start(&g_hb_write_timer.timer, ka_system_ktime_set(HEART_BEAT_TIMER_EXPIRE_SEC, 0),
                            KA_HRTIMER_MODE_REL_TYPE);
#else
    ka_system_hrtimer_start(&g_hb_write_timer.timer, ka_system_ktime_set(HEART_BEAT_TIMER_EXPIRE_SEC, 0),
                            KA_HRTIMER_MODE_REL_PINNED);
#endif
    return DRV_ERROR_NONE;
#else
    return 0;
#endif
}

void heart_beat_write_timer_exit(void)
{
#if (defined CFG_FEATURE_HEARTBEAT_CNT_PCIE) || (!defined CFG_HOST_ENV)
    int i;
    int ret;

    for (i = 0; i < DEVICE_NUM_MAX; i++) {
        heart_beat_write_status_uninit(i);
    }

    ret = ka_system_hrtimer_cancel(&g_hb_write_timer.timer);
    if (ret < 0) {
        soft_drv_warn("hrtimer cannot cancel. (ret=%d)\n", ret);
    }
    (void)ka_task_cancel_work_sync(&g_hb_write_timer.work);
    soft_drv_info("Device heartbeat exit success.\n");

    return;
#endif
}
#endif