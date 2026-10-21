/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BBOX_PRINT_H
#define BBOX_PRINT_H

#if defined NO_DBGMSG
#define BBOX_INF(msg, ...)
#define BBOX_WAR(msg, ...)
#define BBOX_ERR(msg, ...)
#define BBOX_DBG(msg, ...)
#elif defined STD_DBGMSG
#include <stdio.h>
#define STD_FPRINTF(std, format, ...)        \
    do {                                     \
        fprintf(std, format, ##__VA_ARGS__); \
        fflush(std);                         \
    } while (0)

#define BBOX_INF(msg, ...) STD_FPRINTF(stdout, "[INF][BBOX]%s:%d:" msg "\n", __func__, __LINE__, ##__VA_ARGS__)
#define BBOX_WAR(msg, ...) STD_FPRINTF(stdout, "[WAR][BBOX]%s:%d:" msg "\n", __func__, __LINE__, ##__VA_ARGS__)
#define BBOX_ERR(msg, ...) STD_FPRINTF(stderr, "[ERR][BBOX]%s:%d:" msg "\n", __func__, __LINE__, ##__VA_ARGS__)
#define BBOX_DBG(msg, ...)
#elif defined DRV_HOST
#include "bbox_custom_log.h"
#define BBOX_INF(msg, ...) BBOX_CUSTOM_LOG_INF(msg, ##__VA_ARGS__)
#define BBOX_WAR(msg, ...) BBOX_CUSTOM_LOG_WAR(msg, ##__VA_ARGS__)
#define BBOX_ERR(msg, ...) BBOX_CUSTOM_LOG_ERR(msg, ##__VA_ARGS__)
#define BBOX_DBG(msg, ...)
#else
#include "dlog_pub.h"
#define BBOX_INF(msg, ...) Dlog(BBOX, DLOG_INFO, msg, ##__VA_ARGS__)
#define BBOX_WAR(msg, ...) Dlog(BBOX, DLOG_WARN, msg, ##__VA_ARGS__)
#define BBOX_ERR(msg, ...) Dlog(BBOX, DLOG_ERROR, msg, ##__VA_ARGS__)
#define BBOX_DBG(msg, ...) Dlog(BBOX, DLOG_DEBUG, msg, ##__VA_ARGS__)
#endif

#include "bbox_perror.h"

#define MAX_ERRSTR_LEN 128
#define PERROR_INNER(LOG_LEVEL, func, msg, last_err)                                   \
    do {                                                                               \
        char err_str_[MAX_ERRSTR_LEN] = {0};                                           \
        s32 err_idx_ = last_err;                                                       \
        const char *err_str_ptr_ = format_err_str(err_idx_, err_str_, MAX_ERRSTR_LEN); \
        LOG_LEVEL("[%s]%s:(%d)%s", func, msg, err_idx_, err_str_ptr_);                 \
    } while (0)

#define BBOX_PERROR(func, msg) PERROR_INNER(BBOX_ERR, func, msg, get_last_error())
#define BBOX_PWARN(func, msg) PERROR_INNER(BBOX_WAR, func, msg, get_last_error())

#define BBOX_ERR_CTRL(LOG_LEVEL, ACTION, msg, ...) \
    do {                                           \
        LOG_LEVEL(msg, ##__VA_ARGS__);             \
        ACTION;                                    \
    } while (0)

#define BBOX_CHK_EXPR_CTRL(LOG_LEVEL, expr, ACTION, msg, ...) \
    do {                                                      \
        if (expr) {                                           \
            LOG_LEVEL(msg, ##__VA_ARGS__);                    \
            ACTION;                                           \
        }                                                     \
    } while (0)

#define BBOX_CHK_EXPR_ACTION(expr, ACTION, msg, ...) \
    do {                                             \
        if (expr) {                                  \
            BBOX_ERR(msg, ##__VA_ARGS__);            \
            ACTION;                                  \
        }                                            \
    } while (0)

#define BBOX_CHK_EXPR(expr, msg, ...)     \
    do {                                  \
        if (expr) {                       \
            BBOX_ERR(msg, ##__VA_ARGS__); \
        }                                 \
    } while (0)

#define BBOX_CHK_NULL_PTR(PTR, ACTION)                           \
    do {                                                         \
        if ((PTR) == NULL) {                                     \
            BBOX_ERR("Invalid ptr parameter [" #PTR "](NULL)."); \
            ACTION;                                              \
        }                                                        \
    } while (0)

#define BBOX_CHK_INVALID_HANDLE(HANDLE, ACTION)                           \
    do {                                                                  \
        if ((HANDLE) == INVAL_HANDLE) {                                   \
            BBOX_ERR("Invalid handle parameter [" #HANDLE "](invalid)."); \
            ACTION;                                                       \
        }                                                                 \
    } while (0)

#define BBOX_CHK_INVALID_PARAM(INVALID_CONDITION, ACTION, FORMAT, param)    \
    do {                                                                    \
        if (INVALID_CONDITION) {                                            \
            BBOX_ERR("Invalid parameter [" #param "](" FORMAT ").", param); \
            ACTION;                                                         \
        }                                                                   \
    } while (0)

// Output log with LEVEL first and every N times, COUNT is print counter
#define BBOX_LOG_N(LEVEL, COUNT, N, FORMAT, ...) \
    do {                                         \
        if ((COUNT) == 0) {                      \
            (COUNT)++;                           \
            LEVEL(FORMAT, ##__VA_ARGS__);        \
        } else {                                 \
            (COUNT) = ((COUNT) + 1U) % (N);      \
        }                                        \
    } while (0)

#endif // ~BBOX_PRINT_H
