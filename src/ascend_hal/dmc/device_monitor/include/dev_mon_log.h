/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEVICE_MONITOR_LOG_H
#define DEVICE_MONITOR_LOG_H

#include <limits.h>
#include <stdlib.h>
#include "dmc/dmc_log_user.h"

#define MON_ERR "error"
#define MON_WARN "warning"
#define MON_INGO "info"
#define MON_DEBUG "debug"
#define MON_EVENT "event"

#define DEV_MON "device monitor"

#define DEV_UPGRADE "device upgrade"

#ifdef CFG_SOC_PLATFORM_CLOUD
#define MAX_DEVICE_NUM 4
#else
#ifdef CFG_SOC_PLATFORM_MINIV2
#define MAX_DEVICE_NUM 2
#else
#define MAX_DEVICE_NUM 1
#endif
#endif

unsigned int dev_mon_get_log_flag(void);
void dev_mon_set_log_flag(unsigned int value);

#define drv_mon_printk(level, module, fmt, ...) \
    printf(level " [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

#define drv_err(module, fmt...) drv_mon_printk(MON_ERR, module, fmt)
#define drv_warn(module, fmt...) drv_mon_printk(MON_WARN, module, fmt)
#define drv_info(module, fmt...) drv_mon_printk(MON_INGO, module, fmt)
#define drv_debug(module, fmt...) drv_mon_printk(MON_DEBUG, module, fmt)
#define drv_event(module, fmt...) drv_mon_printk(MON_EVENT, module, fmt)

#ifdef DSMI_USE_PRINTF
#define DEV_MON_ERR(msg...)             \
    do {                                \
        if (dev_mon_get_log_flag() > 3) \
            drv_err(DEV_MON, msg);      \
    } while (0)
#define DEV_MON_WARNING(msg...)         \
    do {                                \
        if (dev_mon_get_log_flag() > 4) \
            drv_warn(DEV_MON, msg);     \
    } while (0)
#define DEV_MON_INFO(msg...)            \
    do {                                \
        if (dev_mon_get_log_flag() > 6) \
            drv_info(DEV_MON, msg);     \
    } while (0)
#define DEV_MON_DEBUG(msg...)           \
    do {                                \
        if (dev_mon_get_log_flag() > 7) \
            drv_debug(DEV_MON, msg);    \
    } while (0)
#define DEV_MON_EVENT(msg...)           \
    do {                                \
        if (dev_mon_get_log_flag() > 3) \
            drv_event(DEV_MON, msg);    \
    } while (0)

#define DEV_MON_PRINT(msg...) \
    do {                      \
        printf(msg);          \
    } while (0)

#define DEV_MON_CRIT_EVENT(fmt, ...) DEV_MON_EVENT(fmt, ##__VA_ARGS__)
#define DEV_MON_CRIT_ERR(fmt, ...) DEV_MON_ERR(fmt, ##__VA_ARGS__)

#define dev_upgrade_err(msg...)         \
    do {                                \
        if (dev_mon_get_log_flag() > 3) \
            drv_err(DEV_UPGRADE, msg);  \
    } while (0)
#define dev_upgrade_warn(msg...)        \
    do {                                \
        if (dev_mon_get_log_flag() > 4) \
            drv_warn(DEV_UPGRADE, msg); \
    } while (0)
#define dev_upgrade_info(msg...)        \
    do {                                \
        if (dev_mon_get_log_flag() > 6) \
            drv_info(DEV_UPGRADE, msg); \
    } while (0)
#define dev_upgrade_debug(msg...)        \
    do {                                 \
        if (dev_mon_get_log_flag() > 7)  \
            drv_debug(DEV_UPGRADE, msg); \
    } while (0)

#define dev_upgrade_event(msg...)        \
    do {                                 \
        if (dev_mon_get_log_flag() > 3)  \
            drv_event(DEV_UPGRADE, msg); \
    } while (0)

#define dev_upgrade_print(msg...) \
    do {                          \
        printf(msg);              \
    } while (0)

#else
#define DEV_MON_ERR(fmt, ...) DRV_ERR(HAL_MODULE_TYPE_DMP, fmt, ##__VA_ARGS__)
#define DEV_MON_WARNING(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_DMP, fmt, ##__VA_ARGS__)
#define DEV_MON_INFO(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_DMP, fmt, ##__VA_ARGS__)
#define DEV_MON_DEBUG(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_DMP, fmt, ##__VA_ARGS__)
#define DEV_MON_PRINT(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_DMP, fmt, ##__VA_ARGS__)

#define dev_upgrade_err(fmt, ...) DRV_ERR(HAL_MODULE_TYPE_UPGRADE, fmt, ##__VA_ARGS__)
#define dev_upgrade_warn(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_UPGRADE, fmt, ##__VA_ARGS__)
#define dev_upgrade_info(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_UPGRADE, fmt, ##__VA_ARGS__)
#define dev_upgrade_debug(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_UPGRADE, fmt, ##__VA_ARGS__)
#define dev_upgrade_print(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_UPGRADE, fmt, ##__VA_ARGS__)

#define DEV_MON_EVENT(fmt, ...) DRV_RUN_INFO(HAL_MODULE_TYPE_DMP, fmt, ##__VA_ARGS__)
#define dev_upgrade_event(fmt, ...) DRV_RUN_INFO(HAL_MODULE_TYPE_UPGRADE, fmt, ##__VA_ARGS__)
#define DEV_MON_CRIT_EVENT(fmt, ...)                              \
    do {                                                          \
        DRV_RUN_INFO(HAL_MODULE_TYPE_DMP, fmt, ##__VA_ARGS__);    \
        DRV_SYSLOG_WARN(HAL_MODULE_TYPE_DMP, fmt, ##__VA_ARGS__); \
    } while (0)

#define APP_20 45U << 3 /* critical logs additionally flushed to middleware.log */
#define DRV_SYSLOG_ERR(module, fmt, ...) \
    syslog(APP_20 | LOG_ERR, "[%s][%s %u] " fmt, halDrvLogGetModuleStr(module), __func__, __LINE__, ##__VA_ARGS__)
#define DRV_SYSLOG_WARN(module, fmt, ...) \
    syslog(APP_20 | LOG_WARNING, "[%s][%s %d] " fmt, halDrvLogGetModuleStr(module), __func__, __LINE__, ##__VA_ARGS__)

#define DEV_MON_CRIT_ERR(fmt, ...)                               \
    do {                                                         \
        DRV_ERR(HAL_MODULE_TYPE_DMP, fmt, ##__VA_ARGS__);        \
        DRV_SYSLOG_ERR(HAL_MODULE_TYPE_DMP, fmt, ##__VA_ARGS__); \
    } while (0)
#endif

#define DEV_MON_ERR_EXTEND(ret, no_error_code, fmt, ...) \
    do {                                                 \
        if ((ret) != (no_error_code)) {                  \
            DEV_MON_ERR(fmt, ##__VA_ARGS__);             \
        } else {                                         \
            DEV_MON_WARNING(fmt, ##__VA_ARGS__);         \
        }                                                \
    } while (0)

#define DEV_MON_EX_NOTSUPPORT_ERR(ret, fmt, ...)   \
    do {                                           \
        if ((ret) != (int)DRV_ERROR_NOT_SUPPORT) { \
            DEV_MON_ERR(fmt, ##__VA_ARGS__);       \
        }                                          \
    } while (0)

#define DEV_MON_CRIT_EX_NOTSUPPORT_ERR(ret, fmt, ...) \
    do {                                              \
        if ((ret) != DRV_ERROR_NOT_SUPPORT) {         \
            DEV_MON_CRIT_ERR(fmt, ##__VA_ARGS__);     \
        }                                             \
    } while (0)

#define dev_upgrade_ex_notsupport_err(ret, fmt, ...) \
    do {                                             \
        if ((ret) != (int)DRV_ERROR_NOT_SUPPORT) {   \
            dev_upgrade_err(fmt, ##__VA_ARGS__);     \
        }                                            \
    } while (0)

#ifndef DRV_CHECK_STR
#define DRV_CHECK_STR(a)                                                   \
    {                                                                      \
        DEV_MON_WARNING("[%s %d] Drv_check:%s\n", __func__, __LINE__, #a); \
    }
#endif

#ifndef DRV_CHECK_CHK
#define DRV_CHECK_CHK(a)     \
    {                        \
        if (!(a)) {          \
            DRV_CHECK_STR(a) \
        }                    \
    }
#endif

#ifndef DRV_CHECK_RET
#define DRV_CHECK_RET(a)     \
    {                        \
        if (!(a)) {          \
            DRV_CHECK_STR(a) \
            return;          \
        }                    \
    }
#endif

#ifndef DRV_CHECK_RETV
#define DRV_CHECK_RETV(a, v) \
    {                        \
        if (!(a)) {          \
            DRV_CHECK_STR(a) \
            return (v);      \
        }                    \
    }
#endif

#ifndef DRV_CHECK_RETV_DO_SOMETHING
#define DRV_CHECK_RETV_DO_SOMETHING(a, v, something) \
    {                                                \
        if (!(a)) {                                  \
            DRV_CHECK_STR(a)                         \
            something;                               \
            return (v);                              \
        }                                            \
    }
#endif

#ifndef DRV_CHECK_RET_DO_SOMETHING
#define DRV_CHECK_RET_DO_SOMETHING(a, something) \
    {                                            \
        if (!(a)) {                              \
            DRV_CHECK_STR(a)                     \
            something;                           \
            return;                              \
        }                                        \
    }
#endif

#ifndef DRV_CHECK_CONTINUE
#define DRV_CHECK_CONTINUE(a) \
    {                         \
        if (!(a)) {           \
            DRV_CHECK_STR(a)  \
            continue;         \
        }                     \
    }
#endif

#ifndef DRV_CHECK_GOTO
#define DRV_CHECK_GOTO(a, v) \
    {                        \
        if (!(a)) {          \
            DRV_CHECK_STR(a) \
            goto v;          \
        }                    \
    }
#endif

#ifndef DRV_CHECK_GOTO_DO_SOMETHING
#define DRV_CHECK_GOTO_DO_SOMETHING(a, v, something) \
    {                                                \
        if (!(a)) {                                  \
            DRV_CHECK_STR(a)                         \
            something;                               \
            goto v;                                  \
        }                                            \
    }
#endif

#ifndef DRV_CHECK_DO_SOMETHING
#define DRV_CHECK_DO_SOMETHING(a, something) \
    {                                        \
        if (!(a)) {                          \
            DRV_CHECK_STR(a)                 \
            something;                       \
        }                                    \
    }
#endif
#define DEV_MON_DEBUG_PRINT_BUFFER_LEN 0X1000U

#ifndef CHECK_DEVICE_BUSY
#define CHECK_DEVICE_BUSY(devid, ret)                                                \
    {                                                                                \
        if ((ret) == (int)DRV_ERROR_RESOURCE_OCCUPIED) {                             \
            DEV_MON_ERR("Device is busy. (device_id=%d, ret=%d)\n", (devid), (ret)); \
            return (ret);                                                            \
        }                                                                            \
    }
#endif

#define dev_mon_fopen(out_fd, in_path, flags, ...)                                                              \
    do {                                                                                                        \
        char *out_path = NULL;                                                                                  \
        *(out_fd) = NULL;                                                                                       \
        if ((in_path != NULL) && (strnlen((const char *)(in_path), (PATH_MAX + 1)) < (size_t)(PATH_MAX + 1)) && \
            (out_path = malloc(PATH_MAX + 1)) && (!memset_s(out_path, (PATH_MAX + 1), 0, (PATH_MAX + 1))) &&    \
            (realpath((const char *)(in_path), out_path))) {                                                    \
            *(out_fd) = fopen(out_path, (flags), ##__VA_ARGS__);                                                \
            if (*(out_fd) == NULL) {                                                                            \
                DEV_MON_ERR("open file fail, (file=%s, errno=%d).\n", out_path, errno);                         \
            }                                                                                                   \
        }                                                                                                       \
        if (out_path != NULL) {                                                                                 \
            free(out_path);                                                                                     \
            out_path = NULL;                                                                                    \
        }                                                                                                       \
    } while (0)

#define dev_mon_fopen_creat(out_fd, in_path, flags, ...)                                                     \
    do {                                                                                                     \
        char *out_path = NULL;                                                                               \
        *(out_fd) = NULL;                                                                                    \
        if ((in_path != NULL) && (strnlen((in_path), (PATH_MAX + 1)) < (PATH_MAX + 1)) &&                    \
            (out_path = malloc(PATH_MAX + 1)) && (!memset_s(out_path, (PATH_MAX + 1), 0, (PATH_MAX + 1)))) { \
            (void)realpath((in_path), out_path);                                                             \
            *(out_fd) = fopen(out_path, (flags), ##__VA_ARGS__);                                             \
            if (*(out_fd) == NULL) {                                                                         \
                DEV_MON_ERR("open file fail, (file=%s, errno=%d).\n", out_path, errno);                      \
            }                                                                                                \
        }                                                                                                    \
        if (out_path != NULL) {                                                                              \
            free(out_path);                                                                                  \
            out_path = NULL;                                                                                 \
        }                                                                                                    \
    } while (0)

#define dev_mon_open(out_fd, in_path, flags, ...)                                                            \
    do {                                                                                                     \
        char *out_path = NULL;                                                                               \
        *(out_fd) = -1;                                                                                      \
        if ((in_path != NULL) && (strnlen((in_path), (PATH_MAX + 1)) < (PATH_MAX + 1)) &&                    \
            (out_path = malloc(PATH_MAX + 1)) && (!memset_s(out_path, (PATH_MAX + 1), 0, (PATH_MAX + 1))) && \
            (realpath((in_path), out_path))) {                                                               \
            *(out_fd) = open(out_path, (flags), ##__VA_ARGS__);                                              \
            if (*(out_fd) < 0) {                                                                             \
                DEV_MON_ERR("open file fail, (file=%s, errno=%d).\n", out_path, errno);                      \
            }                                                                                                \
        }                                                                                                    \
        if (out_path != NULL) {                                                                              \
            free(out_path);                                                                                  \
            out_path = NULL;                                                                                 \
        }                                                                                                    \
    } while (0)

#define DEV_MON_ERR_RLIMIT(INTERVAL_SEC, fmt, ...)                                                              \
    do {                                                                                                        \
        static long long last_print_ms_##INTERVAL_SEC##_##__LINE__ = 0;                                         \
        static pthread_mutex_t rate_mutex_##INTERVAL_SEC##_##__LINE__ = PTHREAD_MUTEX_INITIALIZER;              \
        static int print_count_##INTERVAL_SEC##_##__LINE__ = 0;                                                 \
        int can_print = 0;                                                                                      \
        long long current_ms = 0;                                                                               \
        struct timespec ts;                                                                                     \
        (void)clock_gettime(CLOCK_MONOTONIC, &ts);                                                              \
        current_ms = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;                                        \
        pthread_mutex_lock(&rate_mutex_##INTERVAL_SEC##_##__LINE__);                                            \
        can_print = (current_ms - last_print_ms_##INTERVAL_SEC##_##__LINE__ >= (long long)INTERVAL_SEC * 1000); \
        if (can_print) {                                                                                        \
            last_print_ms_##INTERVAL_SEC##_##__LINE__ = current_ms;                                             \
        }                                                                                                       \
        print_count_##INTERVAL_SEC##_##__LINE__++;                                                              \
        pthread_mutex_unlock(&rate_mutex_##INTERVAL_SEC##_##__LINE__);                                          \
        if (can_print) {                                                                                        \
            DEV_MON_ERR("[Trigger=%d] " fmt, print_count_##INTERVAL_SEC##_##__LINE__, ##__VA_ARGS__);           \
        }                                                                                                       \
    } while (0)
#endif
