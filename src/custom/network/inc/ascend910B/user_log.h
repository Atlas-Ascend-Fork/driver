/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef USER_LOG_H
#define USER_LOG_H

#ifndef CONFIG_LLT
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/time.h>
#include "securec.h"
#ifndef HCCN_TOOL_PRODUCT
#include "slog.h"
#endif

#define CHK_PRT_RETURN(result, exeLog, ret) \
    do {                                    \
        if (result) {                       \
            exeLog;                         \
            return (ret);                   \
        }                                   \
    } while (0)

#define DEBUG_LEVEL 0
#define INFO_LEVEL 1
#define WARN_LEVEL 2
#define ERROR_LEVEL 3
#define EVENT_LEVEL 16

#ifdef DRV_HOST
#include "dmc_user_interface.h"
#define roce_err(fmt, ...) DRV_ERR(HAL_MODULE_TYPE_NET, fmt, ##__VA_ARGS__)
#define roce_warn(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_NET, fmt, ##__VA_ARGS__)
#define roce_info(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_NET, fmt, ##__VA_ARGS__)
#define roce_dbg(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_NET, fmt, ##__VA_ARGS__)
#define roce_run_warn(fmt, ...) DRV_RUN_WARN(HAL_MODULE_TYPE_NET, fmt, ##__VA_ARGS__)
#define roce_run_info(fmt, ...) DRV_NOTICE(HAL_MODULE_TYPE_NET, fmt, ##__VA_ARGS__)

#define net_err roce_err
#define net_warn roce_warn
#define net_info roce_info
#define net_dbg roce_dbg
#define net_run_info roce_run_info
#else
#ifdef LOG_HOST

/* HCCP module */
#ifndef HCCN_TOOL_PRODUCT
#define hccp_err(fmt, args...) \
    DlogForC(HCCP, ERROR_LEVEL, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_warn(fmt, args...) \
    DlogForC(HCCP, WARN_LEVEL, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_info(fmt, args...) \
    DlogForC(HCCP, INFO_LEVEL, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_dbg(fmt, args...) \
    DlogForC(HCCP, DEBUG_LEVEL, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)

#define hccp_run_err(fmt, args...) \
    DlogForC(HCCP | RUN_LOG_MASK, ERROR_LEVEL, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_run_warn(fmt, args...) \
    DlogForC(HCCP | RUN_LOG_MASK, WARN_LEVEL, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_run_info(fmt, args...) \
    DlogForC(HCCP | RUN_LOG_MASK, INFO_LEVEL, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_run_dbg(fmt, args...) \
    DlogForC(HCCP | RUN_LOG_MASK, DEBUG_LEVEL, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)

#define hccp_event(fmt, args...) \
    DlogForC(HCCP, EVENT_LEVEL, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)

/* ROCE module */
#define roce_err(fmt, args...) DlogForC(ROCE, ERROR_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_warn(fmt, args...) DlogForC(ROCE, WARN_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_info(fmt, args...) DlogForC(ROCE, INFO_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_dbg(fmt, args...) DlogForC(ROCE, DEBUG_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_event(fmt, args...) DlogForC(ROCE, EVENT_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)

#define roce_run_err(fmt, args...) \
    DlogForC(ROCE | RUN_LOG_MASK, ERROR_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_run_warn(fmt, args...) \
    DlogForC(ROCE | RUN_LOG_MASK, WARN_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_run_info(fmt, args...) \
    DlogForC(ROCE | RUN_LOG_MASK, INFO_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_run_dbg(fmt, args...) \
    DlogForC(ROCE | RUN_LOG_MASK, DEBUG_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)

#define roce_event(fmt, args...) DlogForC(ROCE, EVENT_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)

/* NET module */
#define net_err(fmt, args...) DlogForC(NET, ERROR_LEVEL, "%s(%d) : " fmt, __func__, ##args)
#define net_warn(fmt, args...) DlogForC(NET, WARN_LEVEL, "%s(%d) : " fmt, __func__, ##args)
#define net_info(fmt, args...) DlogForC(NET, INFO_LEVEL, "%s(%d) : " fmt, __func__, ##args)
#define net_dbg(fmt, args...) DlogForC(NET, DEBUG_LEVEL, "%s(%d) : " fmt, __func__, ##args)
#define net_run_err(fmt, args...) DlogForC(NET | RUN_LOG_MASK, ERROR_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define net_run_warn(fmt, args...) DlogForC(NET | RUN_LOG_MASK, WARN_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define net_run_info(fmt, args...) DlogForC(NET | RUN_LOG_MASK, INFO_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define net_run_dbg(fmt, args...) DlogForC(NET | RUN_LOG_MASK, DEBUG_LEVEL, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#else
#define BUFF_LENTH (64U)
#define DRV_LOG_START_TIME (1900)

static inline const char *get_tm_default(void)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static char tmbuf[BUFF_LENTH] = {0};
    struct timeval usec = {0};
    struct tm *tm_now = NULL;
    time_t rawtime = {0};
    int ret;

    (void)pthread_mutex_lock(&lock);
    (void)time(&rawtime);
    tm_now = localtime(&rawtime);
    CHK_PRT_RETURN(tm_now == NULL, (void)pthread_mutex_unlock(&lock), NULL);

    (void)gettimeofday(&usec, NULL);

    ret = sprintf_s(tmbuf, BUFF_LENTH, "[%d-%02d-%02d-%02d:%02d:%02d:%06d]", tm_now->tm_year + DRV_LOG_START_TIME,
                    tm_now->tm_mon + 1, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
                    (int32_t)usec.tv_usec);
    if (ret < 0) {
        (void)pthread_mutex_unlock(&lock);
        return NULL;
    }

    (void)pthread_mutex_unlock(&lock);
    return tmbuf;
}

#define NET_SYSLOG_BASE(LEVEL, level_str, module_str, fmt, ...)                                                \
    do {                                                                                                       \
        syslog(LEVEL, "%s%s[%s:%d][ascend][curpid:%d,%ld][drv][%s][%s]" fmt "\n", level_str, get_tm_default(), \
               __FILE__, __LINE__, getpid(), syscall(__NR_gettid), module_str, __func__, ##__VA_ARGS__);       \
    } while (0)

#define NET_ERR(module, fmt, ...) NET_SYSLOG_BASE(LOG_ERR, "[ERROR]", module, fmt, ##__VA_ARGS__)
#define NET_WARN(module, fmt, ...) NET_SYSLOG_BASE(LOG_WARNING, "[WARNING]", module, fmt, ##__VA_ARGS__)
#define NET_INFO(module, fmt, ...) NET_SYSLOG_BASE(LOG_INFO, "[INFO]", module, fmt, ##__VA_ARGS__)
#define NET_DEBUG(module, fmt, ...) NET_SYSLOG_BASE(LOG_DEBUG, "[DEBUG]", module, fmt, ##__VA_ARGS__)
#define NET_EVENT(module, fmt, ...) NET_SYSLOG_BASE(LOG_DEBUG, "[EVENT]", module, fmt, ##__VA_ARGS__)

/* HCCN_TOOL HCCP module */
#define hccp_err(fmt, ...) NET_ERR("hccp", fmt, ##__VA_ARGS__)
#define hccp_warn(fmt, ...) NET_WARN("hccp", fmt, ##__VA_ARGS__)
#define hccp_info(fmt, ...) NET_INFO("hccp", fmt, ##__VA_ARGS__)
#define hccp_dbg(fmt, ...) NET_DEBUG("hccp", fmt, ##__VA_ARGS__)

#define hccp_run_err(fmt, ...) NET_ERR("hccp", fmt, ##__VA_ARGS__)
#define hccp_run_warn(fmt, ...) NET_WARN("hccp", fmt, ##__VA_ARGS__)
#define hccp_run_info(fmt, ...) NET_INFO("hccp", fmt, ##__VA_ARGS__)
#define hccp_run_dbg(fmt, ...) NET_DEBUG("hccp", fmt, ##__VA_ARGS__)

#define hccp_event(fmt, ...) NET_EVENT("hccp", fmt, ##__VA_ARGS__)

/* ROCE module */
#define roce_err(fmt, ...) NET_ERR("roce", fmt, ##__VA_ARGS__)
#define roce_warn(fmt, ...) NET_WARN("roce", fmt, ##__VA_ARGS__)
#define roce_info(fmt, ...) NET_INFO("roce", fmt, ##__VA_ARGS__)
#define roce_dbg(fmt, ...) NET_DEBUG("roce", fmt, ##__VA_ARGS__)
#define roce_event(fmt, ...) NET_EVENT("roce", fmt, ##__VA_ARGS__)

#define roce_run_err(fmt, ...) NET_ERR("roce", fmt, ##__VA_ARGS__)
#define roce_run_warn(fmt, ...) NET_WARN("roce", fmt, ##__VA_ARGS__)
#define roce_run_info(fmt, ...) NET_INFO("roce", fmt, ##__VA_ARGS__)
#define roce_run_dbg(fmt, ...) NET_DEBUG("roce", fmt, ##__VA_ARGS__)

/* NET module */
#define net_err(fmt, ...) NET_ERR("net", fmt, ##__VA_ARGS__)
#define net_warn(fmt, ...) NET_WARN("net", fmt, ##__VA_ARGS__)
#define net_info(fmt, ...) NET_INFO("net", fmt, ##__VA_ARGS__)
#define net_dbg(fmt, ...) NET_DEBUG("net", fmt, ##__VA_ARGS__)

#define net_run_err(fmt, ...) NET_ERR("net", fmt, ##__VA_ARGS__)
#define net_run_warn(fmt, ...) NET_WARN("net", fmt, ##__VA_ARGS__)
#define net_run_info(fmt, ...) NET_INFO("net", fmt, ##__VA_ARGS__)
#define net_run_dbg(fmt, ...) NET_DEBUG("net", fmt, ##__VA_ARGS__)
#endif

#else
/* HCCP module */
#define hccp_err(fmt, args...) \
    dlog_error(HCCP, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_warn(fmt, args...) \
    dlog_warn(HCCP, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_info(fmt, args...) \
    dlog_info(HCCP, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_dbg(fmt, args...) \
    dlog_debug(HCCP, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)

#define hccp_run_err(fmt, args...) \
    dlog_error(HCCP | RUN_LOG_MASK, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_run_warn(fmt, args...) \
    dlog_warn(HCCP | RUN_LOG_MASK, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_run_info(fmt, args...) \
    dlog_info(HCCP | RUN_LOG_MASK, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)
#define hccp_run_dbg(fmt, args...) \
    dlog_debug(HCCP | RUN_LOG_MASK, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)

#define hccp_event(fmt, args...) \
    dlog_event(HCCP, "tid:%d,%s(%d) : " fmt, syscall(__NR_gettid), __func__, __LINE__, ##args)

/* ROCE module */
#define roce_err(fmt, args...) dlog_error(ROCE, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_warn(fmt, args...) dlog_warn(ROCE, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_info(fmt, args...) dlog_info(ROCE, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_dbg(fmt, args...) dlog_debug(ROCE, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_event(fmt, args...) dlog_event(ROCE, "%s(%d) : " fmt, __func__, __LINE__, ##args)

#define roce_run_err(fmt, args...) dlog_error(ROCE | RUN_LOG_MASK, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_run_warn(fmt, args...) dlog_warn(ROCE | RUN_LOG_MASK, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_run_info(fmt, args...) dlog_info(ROCE | RUN_LOG_MASK, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define roce_run_dbg(fmt, args...) dlog_debug(ROCE | RUN_LOG_MASK, "%s(%d) : " fmt, __func__, __LINE__, ##args)

/* fix slog compatibility issue: NET module log with ROCE module */
#define net_err(fmt, args...) dlog_error(ROCE, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define net_warn(fmt, args...) dlog_warn(ROCE, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define net_info(fmt, args...) dlog_info(ROCE, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define net_dbg(fmt, args...) dlog_debug(ROCE, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define net_run_err(fmt, args...) dlog_error(ROCE | RUN_LOG_MASK, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define net_run_warn(fmt, args...) dlog_warn(ROCE | RUN_LOG_MASK, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define net_run_info(fmt, args...) dlog_info(ROCE | RUN_LOG_MASK, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#define net_run_dbg(fmt, args...) dlog_debug(ROCE | RUN_LOG_MASK, "%s(%d) : " fmt, __func__, __LINE__, ##args)
#endif
#endif

#else
static inline void printf_stub(char *format, ...)
{}
#define hccp_err printf
#define hccp_warn printf
#define hccp_info printf
#define hccp_dbg printf
#define hccp_event printf
#define roce_err printf
#define roce_warn printf
#define roce_info printf
#define roce_dbg printf
#define roce_event printf
#define DEBUG_LEVEL 0
#define INFO_LEVEL 1
#define WARN_LEVEL 2
#define ERROR_LEVEL 3
#define EVENT_LEVEL 16
#endif

#endif
