/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>
#include <sys/mman.h>
#include <string.h>
#include "securec.h"
#include "dmc/dmc_log_user.h"
#include "hal_error_code/drv_error_code.h"
#include "dmc/drv_log_urma.h"
#include "drv_log_user_kernel_api.h"

#define BUFF_LENTH (64U)
#define DRV_LOG_START_TIME (1900)
#define ERROR_NUN_MAX (EUBOPFAIL + 1U)

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define DRV_LOG_LEVEL_MAX (LOG_DEBUG + 1U)
STATIC const char *drv_log_level_default_str[DRV_LOG_LEVEL_MAX] = {
    [LOG_EMERG] = "[EMERG]",     [LOG_ALERT] = "[ALERT]",   [LOG_CRIT] = "[EVENT]", [LOG_ERR] = "[ERROR]",
    [LOG_WARNING] = "[WARNING]", [LOG_NOTICE] = "[NOTICE]", [LOG_INFO] = "[INFO]",  [LOG_DEBUG] = "[DEBUG]",
};
#define EDEADLOCK_VALUE (58U)
#define EWOULDBLOCK_VALUE (41U)
#define DRV_KERNEL_ERROR_RESUME (150)
#define DRV_KERNEL_ERROR_DUP_CONFIG (151)
#define DRV_KERNEL_ERROR_POWER_OP_FAIL (152)
#define DRV_NSEC_PER_USECOND (1000)

struct drv_log_print_info {
    uint32_t *con_log_level;
    const char *(*log_get_level_string)(uint32_t level);
    const char *(*log_get_print_time)(void);
    uint32_t (*log_level_shift)(uint32_t level);
    void (*log_print)(int32_t module_id, int32_t level, const char *fmt, ...);
};

static int32_t user_err[ERROR_NUN_MAX] = {
    [0] = DRV_ERROR_NONE,
    [EPERM] = DRV_ERROR_OPER_NOT_PERMITTED,
    [ENOENT] = DRV_ERROR_FILE_OPS,
    [ESRCH] = DRV_ERROR_IOCRL_FAIL,
    [EINTR] = DRV_ERROR_IOCRL_FAIL,
    [EIO] = DRV_ERROR_IOCRL_FAIL,
    [ENXIO] = DRV_ERROR_NO_DEVICE,
    [E2BIG] = DRV_ERROR_OVER_LIMIT,
    [ENOEXEC] = DRV_ERROR_IOCRL_FAIL,
    [EBADF] = DRV_ERROR_IOCRL_FAIL,
    [ECHILD] = DRV_ERROR_IOCRL_FAIL,
    [EAGAIN] = DRV_ERROR_TRY_AGAIN,
    [ENOMEM] = DRV_ERROR_OUT_OF_MEMORY,
    [EACCES] = DRV_ERROR_IOCRL_FAIL,
    [EFAULT] = DRV_ERROR_INVALID_HANDLE,
    [ENOTBLK] = DRV_ERROR_IOCRL_FAIL,
    [EBUSY] = DRV_ERROR_BUSY,
    [EEXIST] = DRV_ERROR_FILE_OPS,
    [EXDEV] = DRV_ERROR_IOCRL_FAIL,
    [ENODEV] = DRV_ERROR_NO_DEVICE,
    [ENOTDIR] = DRV_ERROR_IOCRL_FAIL,
    [EISDIR] = DRV_ERROR_IOCRL_FAIL,
    [EINVAL] = DRV_ERROR_PARA_ERROR,
    [ENFILE] = DRV_ERROR_IOCRL_FAIL,
    [EMFILE] = DRV_ERROR_IOCRL_FAIL,
    [ENOTTY] = DRV_ERROR_IOCRL_FAIL,
    [ETXTBSY] = DRV_ERROR_IOCRL_FAIL,
    [EFBIG] = DRV_ERROR_IOCRL_FAIL,
    [ENOSPC] = DRV_ERROR_NO_RESOURCES,
    [ESPIPE] = DRV_ERROR_IOCRL_FAIL,
    [EROFS] = DRV_ERROR_IOCRL_FAIL,
    [EMLINK] = DRV_ERROR_IOCRL_FAIL,
    [EPIPE] = DRV_ERROR_IOCRL_FAIL,
    [EDOM] = DRV_ERROR_IOCRL_FAIL,
    [ERANGE] = DRV_ERROR_IOCRL_FAIL,

    [EDEADLK] = DRV_ERROR_IOCRL_FAIL,
    [ENAMETOOLONG] = DRV_ERROR_IOCRL_FAIL,
    [ENOLCK] = DRV_ERROR_IOCRL_FAIL,

    [ENOSYS] = DRV_ERROR_IOCRL_FAIL,

    [ENOTEMPTY] = DRV_ERROR_IOCRL_FAIL,
    [ELOOP] = DRV_ERROR_IOCRL_FAIL,
    [EWOULDBLOCK_VALUE] = DRV_ERROR_IOCRL_FAIL,

    [ENOMSG] = DRV_ERROR_IOCRL_FAIL,
    [EIDRM] = DRV_ERROR_IOCRL_FAIL,
    [ECHRNG] = DRV_ERROR_IOCRL_FAIL,
    [EL2NSYNC] = DRV_ERROR_IOCRL_FAIL,
    [EL3HLT] = DRV_ERROR_IOCRL_FAIL,
    [EL3RST] = DRV_ERROR_IOCRL_FAIL,
    [ELNRNG] = DRV_ERROR_IOCRL_FAIL,
    [EUNATCH] = DRV_ERROR_IOCRL_FAIL,
    [ENOCSI] = DRV_ERROR_IOCRL_FAIL,
    [EL2HLT] = DRV_ERROR_IOCRL_FAIL,
    [EBADE] = DRV_ERROR_IOCRL_FAIL,
    [EBADR] = DRV_ERROR_IOCRL_FAIL,
    [EXFULL] = DRV_ERROR_IOCRL_FAIL,
    [ENOANO] = DRV_ERROR_IOCRL_FAIL,
    [EBADRQC] = DRV_ERROR_IOCRL_FAIL,
    [EBADSLT] = DRV_ERROR_IOCRL_FAIL,

    [EDEADLOCK_VALUE] = DRV_ERROR_IOCRL_FAIL,
    [EBFONT] = DRV_ERROR_IOCRL_FAIL,
    [ENOSTR] = DRV_ERROR_IOCRL_FAIL,
    [ENODATA] = DRV_ERROR_IOCRL_FAIL,
    [ETIME] = DRV_ERROR_IOCRL_FAIL,
    [ENOSR] = DRV_ERROR_IOCRL_FAIL,
    [ENONET] = DRV_ERROR_IOCRL_FAIL,
    [ENOPKG] = DRV_ERROR_IOCRL_FAIL,
    [EREMOTE] = DRV_ERROR_IOCRL_FAIL,
    [ENOLINK] = DRV_ERROR_IOCRL_FAIL,
    [EADV] = DRV_ERROR_IOCRL_FAIL,
    [ESRMNT] = DRV_ERROR_IOCRL_FAIL,
    [ECOMM] = DRV_ERROR_IOCRL_FAIL,
    [EPROTO] = DRV_ERROR_IOCRL_FAIL,
    [EMULTIHOP] = DRV_ERROR_IOCRL_FAIL,
    [EDOTDOT] = DRV_ERROR_IOCRL_FAIL,
    [EBADMSG] = DRV_ERROR_IOCRL_FAIL,
    [EOVERFLOW] = DRV_ERROR_IOCRL_FAIL,
    [ENOTUNIQ] = DRV_ERROR_IOCRL_FAIL,
    [EBADFD] = DRV_ERROR_IOCRL_FAIL,
    [EREMCHG] = DRV_ERROR_IOCRL_FAIL,
    [ELIBACC] = DRV_ERROR_IOCRL_FAIL,
    [ELIBBAD] = DRV_ERROR_IOCRL_FAIL,
    [ELIBSCN] = DRV_ERROR_IOCRL_FAIL,
    [ELIBMAX] = DRV_ERROR_IOCRL_FAIL,
    [ELIBEXEC] = DRV_ERROR_IOCRL_FAIL,
    [EILSEQ] = DRV_ERROR_IOCRL_FAIL,
    [ERESTART] = DRV_ERROR_IOCRL_FAIL,
    [ESTRPIPE] = DRV_ERROR_IOCRL_FAIL,
    [EUSERS] = DRV_ERROR_IOCRL_FAIL,
    [ENOTSOCK] = DRV_ERROR_IOCRL_FAIL,
    [EDESTADDRREQ] = DRV_ERROR_IOCRL_FAIL,
    [EMSGSIZE] = DRV_ERROR_IOCRL_FAIL,
    [EPROTOTYPE] = DRV_ERROR_IOCRL_FAIL,
    [ENOPROTOOPT] = DRV_ERROR_IOCRL_FAIL,
    [EPROTONOSUPPORT] = DRV_ERROR_IOCRL_FAIL,
    [ESOCKTNOSUPPORT] = DRV_ERROR_IOCRL_FAIL,
    [EOPNOTSUPP] = DRV_ERROR_NOT_SUPPORT,
    [EPFNOSUPPORT] = DRV_ERROR_IOCRL_FAIL,
    [EAFNOSUPPORT] = DRV_ERROR_IOCRL_FAIL,
    [EADDRINUSE] = DRV_ERROR_IOCRL_FAIL,
    [EADDRNOTAVAIL] = DRV_ERROR_IOCRL_FAIL,
    [ENETDOWN] = DRV_ERROR_IOCRL_FAIL,
    [ENETUNREACH] = DRV_ERROR_IOCRL_FAIL,
    [ENETRESET] = DRV_ERROR_IOCRL_FAIL,
    [ECONNABORTED] = DRV_ERROR_IOCRL_FAIL,
    [ECONNRESET] = DRV_ERROR_IOCRL_FAIL,
    [ENOBUFS] = DRV_ERROR_IOCRL_FAIL,
    [EISCONN] = DRV_ERROR_IOCRL_FAIL,
    [ENOTCONN] = DRV_ERROR_IOCRL_FAIL,
    [ESHUTDOWN] = DRV_ERROR_IOCRL_FAIL,
    [ETOOMANYREFS] = DRV_ERROR_IOCRL_FAIL,
    [ETIMEDOUT] = DRV_ERROR_WAIT_TIMEOUT,
    [ECONNREFUSED] = DRV_ERROR_IOCRL_FAIL,
    [EHOSTDOWN] = DRV_ERROR_IOCRL_FAIL,
    [EHOSTUNREACH] = DRV_ERROR_IOCRL_FAIL,
    [EALREADY] = DRV_ERROR_IOCRL_FAIL,
    [EINPROGRESS] = DRV_ERROR_IOCRL_FAIL,
    [ESTALE] = DRV_ERROR_IOCRL_FAIL,
    [EUCLEAN] = DRV_ERROR_IOCRL_FAIL,
    [ENOTNAM] = DRV_ERROR_IOCRL_FAIL,
    [ENAVAIL] = DRV_ERROR_IOCRL_FAIL,
    [EISNAM] = DRV_ERROR_IOCRL_FAIL,
    [EREMOTEIO] = DRV_ERROR_IOCRL_FAIL,
    [EDQUOT] = DRV_ERROR_IOCRL_FAIL,

    [ENOMEDIUM] = DRV_ERROR_IOCRL_FAIL,
    [EMEDIUMTYPE] = DRV_ERROR_IOCRL_FAIL,
    [ECANCELED] = DRV_ERROR_IOCRL_FAIL,
    [ENOKEY] = DRV_ERROR_IOCRL_FAIL,
    [EKEYEXPIRED] = DRV_ERROR_IOCRL_FAIL,
    [EKEYREVOKED] = DRV_ERROR_IOCRL_FAIL,
    [EKEYREJECTED] = DRV_ERROR_IOCRL_FAIL,

    [EOWNERDEAD] = DRV_ERROR_IOCRL_FAIL,
    [ENOTRECOVERABLE] = DRV_ERROR_IOCRL_FAIL,
    [ERFKILL] = DRV_ERROR_IOCRL_FAIL,
    [EHWPOISON] = DRV_ERROR_IOCRL_FAIL,
    [EUBINITFAIL] = DRV_ERROR_URMA_INIT_FAIL,
    [EUBNORES] = DRV_ERROR_URMA_NO_RES,
    [EUBNODEV] = DRV_ERROR_URMA_NO_DEVICE,
    [EUBOPFAIL] = DRV_ERROR_URMA_OP_FAIL,

};

int32_t errno_to_user_errno_inner(int32_t kern_err_no)
{
    uint32_t drv_kern_err_no = (uint32_t)kern_err_no;

    if (kern_err_no < 0) {
        if (kern_err_no == -1) {
            return DRV_ERROR_IOCRL_FAIL;
        }
        drv_kern_err_no = (uint32_t)(-kern_err_no);
    }
    if (drv_kern_err_no >= ERROR_NUN_MAX) {
        if (drv_kern_err_no == DRV_KERNEL_ERROR_RESUME) {
            return DRV_ERROR_RESUME;
        } else if (drv_kern_err_no == DRV_KERNEL_ERROR_DUP_CONFIG) {
            return DEV_ERROR_DUP_CONFIG;
        } else if (drv_kern_err_no == DRV_KERNEL_ERROR_POWER_OP_FAIL) {
            return DRV_ERROR_POWER_OP_FAIL;
        } else {
            return DRV_ERROR_IOCRL_FAIL;
        }
    } else if (drv_kern_err_no == 0) {
        return DRV_ERROR_NONE;
    } else if (user_err[drv_kern_err_no] == 0) {
        return DRV_ERROR_IOCRL_FAIL;
    } else {
        ;
    }
    return user_err[drv_kern_err_no];
}

const char *drv_log_get_module_str_inner(enum devdrv_module_type module)
{
    STATIC const char *drv_log_module_str[HAL_MODULE_TYPE_MAX] = {
        [HAL_MODULE_TYPE_VNIC] = "vnic",
        [HAL_MODULE_TYPE_HDC] = "hdc",
        [HAL_MODULE_TYPE_DEVMM] = "devmm",
        [HAL_MODULE_TYPE_DEV_MANAGER] = "devmng",
        [HAL_MODULE_TYPE_DMP] = "dmp",
        [HAL_MODULE_TYPE_FAULT] = "faultmng",
        [HAL_MODULE_TYPE_UPGRADE] = "upgrade",
        [HAL_MODULE_TYPE_PROCESS_MON] = "process-mon",
        [HAL_MODULE_TYPE_LOG] = "log",
        [HAL_MODULE_TYPE_PROF] = "prof",
        [HAL_MODULE_TYPE_DVPP] = "dvpp",
        [HAL_MODULE_TYPE_PCIE] = "pcie",
        [HAL_MODULE_TYPE_IPC] = "ipc",
        [HAL_MODULE_TYPE_TS_DRIVER] = "tsdrv",
        [HAL_MODULE_TYPE_SAFETY_ISLAND] = "sis",
        [HAL_MODULE_TYPE_BSP] = "bsp",
        [HAL_MODULE_TYPE_USB] = "usb",
        [HAL_MODULE_TYPE_NET] = "net",
        [HAL_MODULE_TYPE_EVENT_SCHEDULE] = "event-sche",
        [HAL_MODULE_TYPE_BUF_MANAGER] = "bufmng",
        [HAL_MODULE_TYPE_QUEUE_MANAGER] = "queuemng",
        [HAL_MODULE_TYPE_DP_PROC_MNG] = "dp-procmng",
        [HAL_MODULE_TYPE_COMMON] = "common",
        [HAL_MODULE_TYPE_ASCEND_URMA_ADAPT] = "ascend_urma_adapt",
        [HAL_MODULE_TYPE_LIDAR_DP] = "lidar_dp",
        [HAL_MODULE_TYPE_ADSPC] = "adspc",
        [HAL_MODULE_TYPE_APM] = "apm",
    };
    uint32_t module_type = (uint32_t)module;

    if (module_type >= (uint32_t)(HAL_MODULE_TYPE_MAX)) {
        return "unknown";
    }

    if (drv_log_module_str[module_type] == NULL) {
        return "unknown";
    }

    return drv_log_module_str[module_type];
}

STATIC const char drv_log_level_str[] = "\0";
STATIC const char *drv_log_get_level_str(uint32_t level)
{
    (void)level;
    return drv_log_level_str;
}
const char *drv_log_get_level_str_default(uint32_t level)
{
    if (level >= (uint32_t)(DRV_LOG_LEVEL_MAX)) {
        return NULL;
    }

    return drv_log_level_default_str[level];
}

STATIC const char *drv_get_tm(void)
{
    return drv_log_level_str;
}

const char *drv_get_tm_default(void)
{
    static char tmbuf[BUFF_LENTH] = {0};
    struct timespec ts;
    struct tm *tm_now = NULL;
    int32_t ret;
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    (void)pthread_mutex_lock(&lock);
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        (void)pthread_mutex_unlock(&lock);
        return NULL;
    }

    tm_now = localtime(&ts.tv_sec);
    if (tm_now == NULL) {
        (void)pthread_mutex_unlock(&lock);
        return NULL;
    }

    ret = sprintf_s(tmbuf, BUFF_LENTH, "[%d-%02d-%02d-%02d:%02d:%02d:%06d]", tm_now->tm_year + DRV_LOG_START_TIME,
                    tm_now->tm_mon + 1, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
                    (int32_t)(ts.tv_nsec / DRV_NSEC_PER_USECOND));
    if (ret < 0) {
        (void)pthread_mutex_unlock(&lock);
        return NULL;
    }

    (void)pthread_mutex_unlock(&lock);
    return tmbuf;
}

#define LOG_GLIBC_LEVEL_TYPE_MAX (LOG_DEBUG + 1U)
static uint32_t drv_log_level_glibc_to_tool_table[LOG_GLIBC_LEVEL_TYPE_MAX] = {
    [LOG_EMERG] = DLOG_ERROR,  [LOG_ALERT] = DLOG_ERROR,  [LOG_CRIT] = DLOG_EVENT, [LOG_ERR] = DLOG_ERROR,
    [LOG_WARNING] = DLOG_WARN, [LOG_NOTICE] = DLOG_EVENT, [LOG_INFO] = DLOG_INFO,  [LOG_DEBUG] = DLOG_DEBUG,
};

#define LOG_TOOL_LEVEL_TYPE_MAX (DLOG_NULL + 1U)
static uint32_t drv_log_level_tool_to_glibc_table[LOG_TOOL_LEVEL_TYPE_MAX] = {
    [DLOG_DEBUG] = LOG_DEBUG, [DLOG_INFO] = LOG_INFO, [DLOG_WARN] = LOG_WARNING,
    [DLOG_ERROR] = LOG_ERR,   [DLOG_NULL] = LOG_CRIT,
};

STATIC uint32_t drv_log_level_shift_default(uint32_t level)
{
    return level;
}

STATIC uint32_t drv_log_level_glibc_to_tool(uint32_t level)
{
    return drv_log_level_glibc_to_tool_table[level];
}

STATIC uint32_t drv_log_level_tool_to_glibc(uint32_t level)
{
    return drv_log_level_tool_to_glibc_table[level];
}

STATIC void drv_syslog(int32_t module_id, int32_t priority, const char *format, ...)
{
    (void)module_id;
    va_list args;

    va_start(args, format);
    vsyslog(priority, format, args);
    va_end(args);
}

STATIC uint32_t drv_log_rsyslog_console_level = LOG_ERR;
STATIC uint32_t drv_log_tool_console_level = LOG_ERR;
struct drv_log_print_info g_log_print_info = {
    .con_log_level = &drv_log_rsyslog_console_level, /* default log level */
    .log_get_level_string = drv_log_get_level_str_default,
    .log_get_print_time = drv_get_tm_default,
    .log_level_shift = drv_log_level_shift_default,
    .log_print = drv_syslog,
};

STATIC uint32_t g_run_log_status;

int32_t drv_log_out_handle_register_inner(struct log_out_handle *handle, size_t input_size, uint32_t flag)
{
    int32_t ret;
    if (input_size != sizeof(struct log_out_handle)) {
        DRV_LOG_ERR("Log_out_handle_register failed. (input_size=%zu; size_log_out_handle=%zu)\n", input_size,
                    sizeof(struct log_out_handle)); // lint !e559
        return DRV_ERROR_INVALID_VALUE;
    }
    if (handle == NULL) {
        DRV_LOG_ERR("Log_out_handle_register failed, handle is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (handle->DlogInner == NULL) {
        DRV_LOG_ERR("Log_out_handle_register failed, the member DlogInner in handle is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (handle->logLevel >= (uint32_t)LOG_TOOL_LEVEL_TYPE_MAX) {
        DRV_LOG_ERR("Log_out_handle_register failed. (handle->logLevel=%u)\n", handle->logLevel);
        return DRV_ERROR_INVALID_VALUE;
    }
    DRV_LOG_RUN_INFO("Register log handle. (log_level=%u)\n", handle->logLevel);
    g_run_log_status = flag;
    drv_log_tool_console_level = drv_log_level_tool_to_glibc(handle->logLevel);
    g_log_print_info.con_log_level = &drv_log_tool_console_level;
    g_log_print_info.log_get_level_string = drv_log_get_level_str;
    g_log_print_info.log_get_print_time = drv_get_tm;
    g_log_print_info.log_level_shift = drv_log_level_glibc_to_tool;
    g_log_print_info.log_print = handle->DlogInner;
    ret = drv_log_urma_register(handle->DlogInner, handle->logLevel);
    if ((ret != DRV_ERROR_NONE) && (ret != (int)DRV_ERROR_NOT_SUPPORT)) {
        (void)drv_log_out_handle_unregister_inner();
        DRV_LOG_ERR("Failed to register urma log. (ret=%d)\n", ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

int32_t drv_log_set_module_log_level(int32_t level, int32_t *module_ids, int32_t size)
{
    int32_t i;

    if ((module_ids == NULL) || (size <= 0) || (level < 0) || (level > DLOG_NULL)) {
        DRV_LOG_ERR("Invalid param. (module_ids_is_null=%d, size=%d, level=%d)\n", (int)(module_ids == NULL), size,
                    level);
        return DRV_ERROR_INVALID_VALUE;
    }
    DRV_LOG_INFO("Set level for modules. (level=%d)\n", level);
    for (i = 0; i < size; i++) {
        switch (module_ids[i]) {
            case DRV:
                drv_log_tool_console_level = drv_log_level_tool_to_glibc((uint32_t)level);
                break;
            case UNIFIEDBUS:
                (void)drv_log_set_urma_log_level((uint32_t)level);
                break;
            default:
                DRV_LOG_DEBUG("Unsupported module id. (module_id=%d)\n", module_ids[i]);
                break;
        }
    }

    return DRV_ERROR_NONE;
}
/* compatibility */
int32_t is_run_log_inner(void)
{
    return (int32_t)g_run_log_status;
}

int32_t drv_log_out_handle_unregister_inner(void)
{
    g_log_print_info.con_log_level = &drv_log_rsyslog_console_level;
    g_log_print_info.log_get_level_string = drv_log_get_level_str_default;
    g_log_print_info.log_get_print_time = drv_get_tm_default;
    g_log_print_info.log_level_shift = drv_log_level_shift_default;
    g_log_print_info.log_print = drv_syslog;
    drv_log_urma_unregister();
    return DRV_ERROR_NONE;
}

#ifdef DRV_HOST
static void __attribute__((constructor)) drv_log_base_init(void)
{
    share_log_create(HAL_MODULE_TYPE_COMMON, SHARE_LOG_MAX_SIZE);
}

void drv_log_rsyslog_console_level_set(uint32_t level)
{
    drv_log_rsyslog_console_level = level;
}

#endif

uint32_t get_con_log_level_inner(void)
{
    return *(g_log_print_info.con_log_level);
}

const char *get_log_get_level_string_inner(uint32_t level)
{
    return g_log_print_info.log_get_level_string(level);
}

const char *get_log_get_print_time_inner(void)
{
    return g_log_print_info.log_get_print_time();
}

uint32_t get_log_level_shift_inner(uint32_t level)
{
    return g_log_print_info.log_level_shift(level);
}

void (*get_log_print_inner(void))(int32_t, int32_t, const char *, ...)
{
    return g_log_print_info.log_print;
}

static struct err_msg_report_handle g_report_err_msg_info;

int32_t drv_log_report_err_msg_handle_register_impl(struct err_msg_report_handle *handle, size_t input_size)
{
    if (input_size != sizeof(struct err_msg_report_handle) || handle == NULL) {
        DRV_LOG_ERR("Invalid param. (handle_is_null=%d, input_size=%zu, target_size=%zu)\n", (int)(handle == NULL),
                    input_size, sizeof(struct err_msg_report_handle));
        return DRV_ERROR_INVALID_VALUE;
    }
    if (handle->register_format_func == NULL || handle->predefined_report_func == NULL ||
        handle->inner_report_func == NULL) {
        DRV_LOG_ERR("Invalid param. (register_func_is_null=%d, predefined_report_func_is_null=%d, "
                    "inner_report_func_is_null=%d)\n",
                    (int)(handle->register_format_func == NULL), (int)(handle->predefined_report_func == NULL),
                    (int)(handle->inner_report_func == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }
    if (g_report_err_msg_info.is_registered == 1) {
        DRV_LOG_WARN("Msg handle has been registered.\n");
        return DRV_ERROR_REPEATED_INIT;
    }
    g_report_err_msg_info.register_format_func = handle->register_format_func;
    g_report_err_msg_info.predefined_report_func = handle->predefined_report_func;
    g_report_err_msg_info.inner_report_func = handle->inner_report_func;
    g_report_err_msg_info.is_registered = 1;
    REGISTER_FORMAT_ERR_MSG(g_drv_error_code, (unsigned long)strlen(g_drv_error_code));
    return DRV_ERROR_NONE;
}

int32_t drv_log_report_err_msg_handle_unregister_impl(void)
{
    g_report_err_msg_info.register_format_func = NULL;
    g_report_err_msg_info.predefined_report_func = NULL;
    g_report_err_msg_info.inner_report_func = NULL;
    g_report_err_msg_info.is_registered = 0;
    return DRV_ERROR_NONE;
}

register_format_err_msg_func get_format_err_msg_register_func_impl(void)
{
    return g_report_err_msg_info.register_format_func;
}

report_predefined_err_msg_func get_predefined_err_msg_report_func_impl(void)
{
    return g_report_err_msg_info.predefined_report_func;
}

report_inner_err_msg_func get_inner_err_msg_report_func_impl(void)
{
    return g_report_err_msg_info.inner_report_func;
}

__attribute__((weak)) int32_t drv_log_urma_register(alog_print_func log_print, uint32_t log_level)
{
    (void)log_print;
    (void)log_level;
    return DRV_ERROR_NOT_SUPPORT;
}

__attribute__((weak)) void drv_log_urma_unregister(void)
{}

__attribute__((weak)) int32_t drv_log_set_urma_log_level(uint32_t level)
{
    (void)level;
    return DRV_ERROR_NOT_SUPPORT;
}
