/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef QUEUE_KERNEL_API_H
#define QUEUE_KERNEL_API_H

#include <stdbool.h>

#define ATOMIC_INC(x) __sync_add_and_fetch((x), 1)
#define ATOMIC_DEC(x) __sync_sub_and_fetch((x), 1)
#define ATOMIC_ADD(x, y) __sync_add_and_fetch((x), (y))
#define ATOMIC_SUB(x, y) __sync_sub_and_fetch((x), (y))
#define ATOMIC_SET(x, y) __sync_lock_test_and_set((x), (y))

void queue_run_log_flow_ctrl_cnt_clear(void);
bool queue_proc_log_flow_ctrl_cnt_check(void);

#ifndef EMU_ST
#include "dmc_user_interface.h"
#else
#include "ut_log.h"
#endif

#define QUEUE_LOG_ERR(format, ...)                                     \
    do {                                                               \
        DRV_ERR(HAL_MODULE_TYPE_QUEUE_MANAGER, format, ##__VA_ARGS__); \
    } while (0)

#define QUEUE_LOG_WARN(format, ...)                                     \
    do {                                                                \
        DRV_WARN(HAL_MODULE_TYPE_QUEUE_MANAGER, format, ##__VA_ARGS__); \
    } while (0)

#define QUEUE_LOG_INFO(format, ...)                                     \
    do {                                                                \
        DRV_INFO(HAL_MODULE_TYPE_QUEUE_MANAGER, format, ##__VA_ARGS__); \
    } while (0)

#define QUEUE_LOG_DEBUG(format, ...)                                     \
    do {                                                                 \
        DRV_DEBUG(HAL_MODULE_TYPE_QUEUE_MANAGER, format, ##__VA_ARGS__); \
    } while (0)

/* alarm event log, non-alarm events use debug or run log */
#define QUEUE_LOG_EVENT(format, ...)                                      \
    do {                                                                  \
        DRV_NOTICE(HAL_MODULE_TYPE_QUEUE_MANAGER, format, ##__VA_ARGS__); \
    } while (0)
/* run log, the default log level is LOG_INFO. */
#define QUEUE_RUN_LOG_INFO(format, ...)                                     \
    do {                                                                    \
        DRV_RUN_INFO(HAL_MODULE_TYPE_QUEUE_MANAGER, format, ##__VA_ARGS__); \
    } while (0)

#define QUEUE_RUN_LOG_INFO_FLOWCTRL(format, ...)                                \
    do {                                                                        \
        if (queue_proc_log_flow_ctrl_cnt_check()) {                             \
            DRV_RUN_INFO(HAL_MODULE_TYPE_QUEUE_MANAGER, format, ##__VA_ARGS__); \
        }                                                                       \
    } while (0)

#define QUEUE_ERR_HEX_DUMP(buf, len, format, ...)                                         \
    do {                                                                                  \
        DRV_ERR_HEX_DUMP(HAL_MODULE_TYPE_QUEUE_MANAGER, buf, len, format, ##__VA_ARGS__); \
    } while (0)

#endif