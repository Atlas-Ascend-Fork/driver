/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef EVENT_SCHED_API_COMMON_H
#define EVENT_SCHED_API_COMMON_H

#include <pthread.h>
#include "ascend_hal_define.h"
#include "drv_user_common.h"
#include "esched_user_interface.h"
#include "esched_ioctl.h"

#ifdef EMU_ST
#include <sys/syscall.h>
#ifndef THREAD__
#define THREAD__ __thread
#endif
#define GETPID() syscall(__NR_gettid)
#else
#ifndef THREAD__
#define THREAD__
#endif
#define GETPID() getpid()
#endif

#define ESCHED_DEV_NUM (65 + 1) /* dc has 64 logic dev(0-63)+ 1 back dev + 1 host virtual dev(64) */

#ifdef CFG_FEATURE_EXTERNAL_CDEV
#define ESCHED_LOGIC_DEV_NUM 64 /* dc has 64 logic dev */
#else
#define ESCHED_LOGIC_DEV_NUM 1 /* helper host has 1 logic dev */
#endif

#ifdef CFG_FEATURE_SYSLOG
#include <syslog.h>
#define sched_err(fmt, ...) syslog(LOG_ERR, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define sched_warn(fmt, ...) syslog(LOG_WARNING, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define sched_info(fmt, ...) syslog(LOG_INFO, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define sched_debug(fmt, ...) syslog(LOG_DEBUG, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define sched_run_info(fmt, ...) syslog(LOG_INFO, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#ifndef EMU_ST
#include "dmc_user_interface.h"
#else
#include "ascend_inpackage_hal.h"
#include "ut_log.h"
#endif
#define sched_err(fmt, ...) DRV_ERR(HAL_MODULE_TYPE_EVENT_SCHEDULE, fmt, ##__VA_ARGS__)
#define sched_warn(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_EVENT_SCHEDULE, fmt, ##__VA_ARGS__)
#define sched_info(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_EVENT_SCHEDULE, fmt, ##__VA_ARGS__)
#define sched_debug(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_EVENT_SCHEDULE, fmt, ##__VA_ARGS__)
#define sched_run_info(format, ...)                                          \
    do {                                                                     \
        DRV_RUN_INFO(HAL_MODULE_TYPE_EVENT_SCHEDULE, format, ##__VA_ARGS__); \
    } while (0)
#endif

#define HOST_NAME_LEN 64
#define AOSSD_HOST_NAME "AOS_SD"
#define SCHED_DEV_FD_CLOSED (-2)

/* configured provisionally for tuning. default ctrl cpu num 2 */
#define HOST_SCHED_CPU_START 2
#define HOST_CPU_NUM 32
#define HOST_CTRL_CPU_NUM 2

#ifndef u64
typedef unsigned long long u64;
#endif

typedef struct {
    unsigned int gid;
    GROUP_TYPE type;
    char grp_name[EVENT_MAX_GRP_NAME_LEN];
} esched_grp_info;

typedef struct {
    pthread_mutex_t mutex;
    esched_grp_info info[SCHED_MAX_GRP_NUM];
} esched_proc_grp_info;

typedef struct {
    unsigned int dev_id;
    unsigned int gid;
    unsigned int tid;
} esched_thread_info;

typedef struct {
    esched_thread_info thread_info;
    int event_valid;
    esched_event_info event_info;
    struct list_head list;
} esched_thread_wait_info;

typedef struct {
    esched_thread_info cur_thread_info;
    struct list_head list_head;
} esched_thread_wait_info_head;

struct sync_event_wait_info {
    uint32_t grp_id;
    uint32_t thread_id;
    int32_t pid;
    EVENT_ID event_id;
    uint32_t subevent_id;
    int32_t timeout;
};

int esched_dev_ioctl(unsigned int dev_id, unsigned int cmd, void *para);
int esched_device_check(unsigned int dev_id);
int32_t esched_dev_init(unsigned int dev_id);
void esched_query_sync_msg_trace(uint32_t dev_id, struct event_summary *event, uint32_t grp_id, uint32_t thread_id);

#ifndef EMU_ST
int esched_open(const char *char_dev_name);
int esched_close(int fd);
int esched_ioctl(int fd, uint32_t cmd, void *para);
#endif

void esched_share_log_create(void);
void esched_share_log_read(void);
void esched_share_log_destroy(void);
int32_t esched_init_device_sched_cpu_num(unsigned int dev_id, int fd);
int32_t esched_init_sched_cpu_num(unsigned int dev_id, int fd);
int32_t esched_attach_device_inner(unsigned int dev_id, struct sched_ioctl_para_attach *para_attach);
int32_t esched_dettach_device_inner(unsigned int dev_id, struct sched_ioctl_para_detach *para_detach);
void esched_clear_grp_info(unsigned int dev_id);
bool esched_need_judge_thread_id(void);
bool esched_support_thread_swap_out(void);
bool esched_support_extern_interface(void);
bool esched_support_thread_giveup(void);
void esched_detach_device(unsigned int dev_id, struct sched_ioctl_para_detach *para_detach);
void esched_clear_attach_refcnt(unsigned int dev_id);
void esched_sync_res_reset(uint32_t dev_id);
drvError_t esched_query_curr_sched_mode(unsigned int dev_id, unsigned int *sched_mode);
#endif
