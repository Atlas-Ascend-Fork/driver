/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __HDC_LOG_H__
#define __HDC_LOG_H__

#include "drv_user_common.h"

#if !defined(CFG_FEATURE_SHARE_LOG) || defined(CFG_FEATURE_SHARE_LOG_NOT)
#define HDC_SHARE_LOG_CREATE()
#define HDC_SHARE_LOG_DESTROY()
#define HDC_SHARE_LOG_READ_ERR()
#define HDC_SHARE_LOG_READ_INFO()
#else
#define HDC_SHARE_LOG_CREATE()                                     \
    do {                                                           \
        share_log_create(HAL_MODULE_TYPE_HDC, SHARE_LOG_MAX_SIZE); \
    } while (0)
#define HDC_SHARE_LOG_DESTROY()                       \
    do {                                              \
        share_log_read_err(HAL_MODULE_TYPE_HDC);      \
        share_log_read_run_info(HAL_MODULE_TYPE_HDC); \
        share_log_destroy(HAL_MODULE_TYPE_HDC);       \
    } while (0)
#define HDC_SHARE_LOG_READ_ERR()                 \
    do {                                         \
        share_log_read_err(HAL_MODULE_TYPE_HDC); \
    } while (0)
#define HDC_SHARE_LOG_READ_INFO()                     \
    do {                                              \
        share_log_read_run_info(HAL_MODULE_TYPE_HDC); \
    } while (0)
#endif

#define HDC_LOG_ERR(format, ...)                             \
    do {                                                     \
        DRV_ERR(HAL_MODULE_TYPE_HDC, format, ##__VA_ARGS__); \
    } while (0);
#define HDC_LOG_WARN(format, ...)                             \
    do {                                                      \
        DRV_WARN(HAL_MODULE_TYPE_HDC, format, ##__VA_ARGS__); \
    } while (0);
#define HDC_LOG_INFO(format, ...)                             \
    do {                                                      \
        DRV_INFO(HAL_MODULE_TYPE_HDC, format, ##__VA_ARGS__); \
    } while (0);
#define HDC_LOG_DEBUG(format, ...)                             \
    do {                                                       \
        DRV_DEBUG(HAL_MODULE_TYPE_HDC, format, ##__VA_ARGS__); \
    } while (0);
/* alarm event log, non-alarm events use debug or run log */
#define HDC_LOG_EVENT(format, ...)                              \
    do {                                                        \
        DRV_NOTICE(HAL_MODULE_TYPE_HDC, format, ##__VA_ARGS__); \
    } while (0);
/* run log, the default log level is LOG_INFO. */
#define HDC_RUN_LOG_INFO(format, ...)                             \
    do {                                                          \
        DRV_RUN_INFO(HAL_MODULE_TYPE_HDC, format, ##__VA_ARGS__); \
    } while (0);

#endif /* __HDC_LOG_H__ */
