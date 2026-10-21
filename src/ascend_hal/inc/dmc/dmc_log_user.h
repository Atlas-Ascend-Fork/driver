/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DMC_LOG_USER_H
#define DMC_LOG_USER_H

#include <syslog.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <stdbool.h>
#include <stdio.h>

#include "ascend_hal_define.h"
#include "dlog_pub.h"
#include "dmc_share_log.h"
#include "securec.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux
#define DLLEXPORT __attribute__((visibility("default")))
#else
#define DLLEXPORT _declspec(dllexport)
#endif

int32_t errno_to_user_errno(int32_t kern_err_no);
drvError_t drvMngGetConsoleLogLevel(unsigned int *logLevel); // This function is defined in devdrv_manage.
int32_t drv_log_out_handle_register(struct log_out_handle *handle, size_t input_size, uint32_t flag);
int32_t drv_log_out_handle_unregister(void);
int dsmi_printf(const char *fmt, ...);

DLLEXPORT uint32_t get_con_log_level(void);
DLLEXPORT const char *get_log_get_level_string(uint32_t level);
DLLEXPORT const char *get_log_get_print_time(void);
DLLEXPORT uint32_t get_log_level_shift(uint32_t level);
DLLEXPORT void (*get_log_Print(void))(int32_t, int32_t, const char *, ...);
DLLEXPORT const char *drv_log_get_module_str(enum devdrv_module_type module);

DLLEXPORT int halDsmiPrintf(const char *fmt, ...);
DLLEXPORT int32_t halIsRunLog(void);
DLLEXPORT const char *halDrvLogGetModuleStr(enum devdrv_module_type module);
DLLEXPORT uint32_t halGetConLogLevel(void);
DLLEXPORT const char *halGetLogGetLevelString(uint32_t level);
DLLEXPORT const char *halGetLogGetLevelStringDefault(uint32_t level);
DLLEXPORT const char *halGetLogGetPrintTime(void);
DLLEXPORT const char *halGetLogGetPrintTimeDefault(void);
DLLEXPORT uint32_t halGetLogLevelShift(uint32_t level);
DLLEXPORT void (*halGetLogPrint(void))(int32_t, int32_t, const char *, ...);
DLLEXPORT const char *halGetProcessName(void);

#ifdef DRV_HOST
#define DRV_DSMI_PRINTF(module, mask, LEVEL, fmt, ...)                                                                 \
    do {                                                                                                               \
        if (module == HAL_MODULE_TYPE_DMP || module == HAL_MODULE_TYPE_DEV_MANAGER) {                                  \
            halDsmiPrintf("%s DRV(%d, %s):%s[%s:%d][ascend][curpid:%d,%ld][drv][%s][%s]" fmt,                          \
                          halGetLogGetLevelStringDefault((uint32_t)((mask & RUN_LOG_MASK) == 0 ? LEVEL : LOG_CRIT)),   \
                          getpid(), halGetProcessName(), halGetLogGetPrintTimeDefault(), __FILE__, __LINE__, getpid(), \
                          syscall(__NR_gettid), halDrvLogGetModuleStr(module), __func__, ##__VA_ARGS__);               \
        }                                                                                                              \
    } while (0)
#else
#define DRV_DSMI_PRINTF(module, mask, LEVEL, fmt, ...) \
    do {                                               \
    } while (0)
#endif

#define DRV_SYSLOG_BASE(LEVEL, module, mask, actual_print_level, fmt, ...)                                             \
    do {                                                                                                               \
        if ((uint32_t)LEVEL <= (uint32_t)actual_print_level) {                                                         \
            (*halGetLogPrint())(mask, (int32_t)halGetLogLevelShift((uint32_t)LEVEL),                                   \
                                "%s%s[%s:%d][ascend][curpid:%d,%d][drv][%s][%s]" fmt,                                  \
                                halGetLogGetLevelString((uint32_t)LEVEL), halGetLogGetPrintTime(), __FILE__, __LINE__, \
                                getpid(), syscall(__NR_gettid), halDrvLogGetModuleStr(module), __func__,               \
                                ##__VA_ARGS__);                                                                        \
            DRV_DSMI_PRINTF(module, mask, LEVEL, fmt, ##__VA_ARGS__);                                                  \
        }                                                                                                              \
    } while (0)

#define DRV_SYSLOG(LEVEL, module, fmt, ...) DRV_SYSLOG_BASE(LEVEL, module, DRV, halGetConLogLevel(), fmt, ##__VA_ARGS__)
#define DRV_SYSLOG_RUN(LEVEL, module, fmt, ...) \
    DRV_SYSLOG_BASE(LEVEL, module, (DRV | RUN_LOG_MASK), LOG_INFO, fmt, ##__VA_ARGS__)
#ifdef DRV_HOST
/* debug log, the default log level is LOG_ERR. */
#define DRV_ERR(module, fmt, ...) DRV_SYSLOG(LOG_ERR, module, fmt, ##__VA_ARGS__)
#else
#ifdef CFG_BUILD_DEBUG
/* In debug build, DlogFlush in DRV_ERR can be disabled via env DRV_ERR_FLUSH_DISABLE to reduce perf overhead. */
static inline bool drv_err_flush_disabled(void)
{
    static bool checked = false;
    static bool disabled = false;
    if (!checked) {
        if (getenv("DRV_ERR_FLUSH_DISABLE") != NULL) {
            disabled = true;
        }
        checked = true;
    }
    return disabled;
}
/* debug log, the default log level is LOG_ERR. */
#define DRV_ERR(module, fmt, ...)                        \
    do {                                                 \
        DRV_SYSLOG(LOG_ERR, module, fmt, ##__VA_ARGS__); \
        if (!drv_err_flush_disabled()) {                 \
            DlogFlush();                                 \
        }                                                \
    } while (0)
#else
#define DRV_ERR(module, fmt, ...)                        \
    do {                                                 \
        DRV_SYSLOG(LOG_ERR, module, fmt, ##__VA_ARGS__); \
        DlogFlush();                                     \
    } while (0)
#endif
#endif

#define DRV_WARN(module, fmt, ...) DRV_SYSLOG(LOG_WARNING, module, fmt, ##__VA_ARGS__)
#define DRV_INFO(module, fmt, ...) DRV_SYSLOG(LOG_INFO, module, fmt, ##__VA_ARGS__)
#define DRV_DEBUG(module, fmt, ...) DRV_SYSLOG(LOG_DEBUG, module, fmt, ##__VA_ARGS__)

/* alarm event log, non-alarm events use debug or run log */
#define DRV_CRIT(module, fmt, ...) DRV_SYSLOG(LOG_CRIT, module, fmt, ##__VA_ARGS__)
#define DRV_EVENT(module, fmt, ...) DRV_CRIT(module, fmt, ##__VA_ARGS__)

/* infrequent log level */
#define DRV_EMERG(module, fmt, ...) DRV_SYSLOG(LOG_EMERG, module, fmt, ##__VA_ARGS__)
#define DRV_ALERT(module, fmt, ...) DRV_SYSLOG(LOG_ALERT, module, fmt, ##__VA_ARGS__)
#define DRV_NOTICE(module, fmt, ...) DRV_SYSLOG(LOG_NOTICE, module, fmt, ##__VA_ARGS__)

#define DRV_LOG_CMPT(LEVEL, module, fmt, ...)                  \
    do {                                                       \
        if (halIsRunLog()) {                                   \
            DRV_SYSLOG_RUN(LEVEL, module, fmt, ##__VA_ARGS__); \
        } else {                                               \
            DRV_SYSLOG(LOG_CRIT, module, fmt, ##__VA_ARGS__);  \
        }                                                      \
    } while (0)

/* run log, the default log level is LOG_INFO. */
#define DRV_RUN_ERR(module, fmt, ...) DRV_LOG_CMPT(LOG_ERR, module, fmt, ##__VA_ARGS__)
#define DRV_RUN_WARN(module, fmt, ...) DRV_LOG_CMPT(LOG_WARNING, module, fmt, ##__VA_ARGS__)
#define DRV_RUN_INFO(module, fmt, ...) DRV_LOG_CMPT(LOG_INFO, module, fmt, ##__VA_ARGS__)
#define DRV_RUN_DEBUG(module, fmt, ...) DRV_LOG_CMPT(LOG_DEBUG, module, fmt, ##__VA_ARGS__)

int32_t __attribute__((weak)) drv_log_report_err_msg_handle_register(struct err_msg_report_handle *handle,
                                                                     size_t input_size);
int32_t __attribute__((weak)) drv_log_report_err_msg_handle_unregister(void);
register_format_err_msg_func get_format_err_msg_register_func(void);
report_predefined_err_msg_func get_predefined_err_msg_report_func(void);
report_inner_err_msg_func get_inner_err_msg_report_func(void);
#define MAX_MEM_SIZE_STR_SIZE 32
#define MAX_PARA_VALUE_STR_SIZE 30
#define MAX_REASON_STR_SIZE 300
void report_enum_arg_out_of_range(const char *func_name, const char *para_name, unsigned int para_value,
                                  const char *enum_name);
void report_arg_out_of_range(const char *func_name, const char *para_name, unsigned int para_value, unsigned int left,
                             unsigned int right);
void report_arg_equal_to_zero(const char *func_name, const char *para_name);
void report_arg_null_pointer(const char *func_name, const char *para_name);
void report_insufficient_host_memory(unsigned long long size, const char *module_name);
void report_insufficient_device_memory(unsigned long long size, const char *module_name);
#define REGISTER_FORMAT_ERR_MSG(error_msg, error_msg_len)                                \
    do {                                                                                 \
        register_format_err_msg_func register_func = get_format_err_msg_register_func(); \
        if (register_func == NULL) {                                                     \
            break;                                                                       \
        }                                                                                \
        register_func((error_msg), (error_msg_len));                                     \
    } while (0)

#define REPORT_PREDEFINED_ERR_MSG(error_code, key, value, arg_num)                         \
    do {                                                                                   \
        report_predefined_err_msg_func report_func = get_predefined_err_msg_report_func(); \
        if (report_func == NULL) {                                                         \
            break;                                                                         \
        }                                                                                  \
        report_func((error_code), (key), (value), (arg_num));                              \
    } while (0)

#define REPORT_INNER_ERR_MSG(error_code, format, ...)                                         \
    do {                                                                                      \
        report_inner_err_msg_func report_func = get_inner_err_msg_report_func();              \
        if (report_func == NULL) {                                                            \
            break;                                                                            \
        }                                                                                     \
        report_func(__FILE__, __FUNCTION__, __LINE__, (error_code), (format), ##__VA_ARGS__); \
    } while (0)

static inline int drv_hex_format_line(char *buf, size_t bufsz, const unsigned char *data, size_t len)
{
    size_t pos = 0;
    size_t i;
    int sn_ret;

    for (i = 0; i < len; i++) {
        sn_ret = snprintf_s(buf + pos, bufsz - pos, (size_t)(bufsz - pos - 1), "%02x", data[i]);
        if (sn_ret < 0) {
            return -1;
        }
        pos += (size_t)sn_ret;
    }

    if (pos < bufsz) {
        buf[pos] = '\0';
    } else {
        buf[bufsz - 1] = '\0';
    }

    return (int)pos;
}

/* NOTE: Do not include a newline ('\n') in @fmt. The macro adds
 * a space and a newline automatically for each line.
 */
#define DRV_ERR_HEX_DUMP(module, buf, len, fmt, ...)                                                 \
    do {                                                                                             \
        const unsigned char *data_ptr = (const unsigned char *)(buf);                                \
        size_t remain = (len);                                                                       \
        size_t offset = 0;                                                                           \
        char line_buf[129];                                                                          \
                                                                                                     \
        if ((data_ptr == NULL) || (remain == 0)) {                                                   \
            break;                                                                                   \
        }                                                                                            \
                                                                                                     \
        while (remain > 0) {                                                                         \
            size_t chunk = (remain > 64) ? 64 : remain;                                              \
            int hex_ret = drv_hex_format_line(line_buf, sizeof(line_buf), data_ptr + offset, chunk); \
            if (hex_ret <= 0) {                                                                      \
                break;                                                                               \
            }                                                                                        \
            DRV_ERR(module, fmt " %s\n", ##__VA_ARGS__, line_buf);                                   \
            offset += chunk;                                                                         \
            remain -= chunk;                                                                         \
        }                                                                                            \
    } while (0)

int32_t drv_log_set_module_log_level(int32_t level, int32_t *module_ids, int32_t size);
#define DRV_LOG_ERR(fmt, ...) DRV_ERR(HAL_MODULE_TYPE_LOG, fmt, ##__VA_ARGS__)
#define DRV_LOG_WARN(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_LOG, fmt, ##__VA_ARGS__)
#define DRV_LOG_INFO(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_LOG, fmt, ##__VA_ARGS__)
#define DRV_LOG_DEBUG(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_LOG, fmt, ##__VA_ARGS__)
#define DRV_LOG_RUN_INFO(fmt, ...) DRV_RUN_INFO(HAL_MODULE_TYPE_LOG, fmt, ##__VA_ARGS__)
#ifdef __cplusplus
}
#endif
#endif /* _DRV_SYSLOG_USER_H_ */
