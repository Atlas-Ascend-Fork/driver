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
#ifndef _ASDRV_UB_COMMON_H_
#define _ASDRV_UB_COMMON_H_

#include "ka_dfx_pub.h"
#ifdef CFG_FEATURE_DRV_LOG
#include "dmc_kernel_interface.h"
#endif
#include "ascend_hal_error.h"
#include "asdrv_ub_load.h"
#include "asdrv_ub_mem_alloc.h"

#define ASCEND_UB_INVALID 0
#define ASCEND_UB_VALID 1
#define UBDRV_WORK_MAGIC 0x4567abcd
#define UBDRV_DECIMAL 10
#define ubdrv_name "asdrv_ub"
void ubdrv_flush_p2p(int pid); /* used in user process crush */

#ifdef CFG_FEATURE_DRV_LOG
#define ubdrv_info(fmt, ...)                                                                            \
    do {                                                                                                \
        drv_info(ubdrv_name, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                 ka_task_get_current_pid(), ##__VA_ARGS__);                                             \
    } while (0)
#define ubdrv_warn(fmt, ...)                                                                            \
    do {                                                                                                \
        drv_warn(ubdrv_name, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                 ka_task_get_current_pid(), ##__VA_ARGS__);                                             \
    } while (0)
#define ubdrv_err(fmt, ...)                                                                            \
    do {                                                                                               \
        drv_err(ubdrv_name, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                ka_task_get_current_pid(), ##__VA_ARGS__);                                             \
    } while (0)
#define ubdrv_event(fmt, ...)                                                                            \
    do {                                                                                                 \
        drv_event(ubdrv_name, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                  ka_task_get_current_pid(), ##__VA_ARGS__);                                             \
    } while (0)
#define ubdrv_debug(fmt, ...)                                                                            \
    do {                                                                                                 \
        drv_debug(ubdrv_name, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                  ka_task_get_current_pid(), ##__VA_ARGS__);                                             \
    } while (0)
#define ubdrv_err_hex_dump(buf, len, fmt, ...)                                                  \
    do {                                                                                        \
        drv_err_hex_dump(ubdrv_name, buf, len, "<%s:%d:%d> " fmt, ka_task_get_current_comm(),   \
                         ka_task_get_current_tgid(), ka_task_get_current_pid(), ##__VA_ARGS__); \
    } while (0)

#else // CFG_FEATURE_DRV_LOG

#define ubdrv_info(fmt, ...)                                                                                \
    do {                                                                                                    \
        ka_dfx_printk(KA_KERN_INFO "[ascend] [%s] [%s %d] <%s:%d:%d> " fmt, ubdrv_name, __func__, __LINE__, \
                      ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(),    \
                      ##__VA_ARGS__);                                                                       \
    } while (0)
#define ubdrv_warn(fmt, ...)                                                                                   \
    do {                                                                                                       \
        ka_dfx_printk(KA_KERN_WARNING "[ascend] [%s] [%s %d] <%s:%d:%d> " fmt, ubdrv_name, __func__, __LINE__, \
                      ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(),       \
                      ##__VA_ARGS__);                                                                          \
    } while (0)
#define ubdrv_err(fmt, ...)                                                                                \
    do {                                                                                                   \
        ka_dfx_printk(KA_KERN_ERR "[ascend] [%s] [%s %d] <%s:%d:%d> " fmt, ubdrv_name, __func__, __LINE__, \
                      ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(),   \
                      ##__VA_ARGS__);                                                                      \
    } while (0)
#define ubdrv_event(fmt, ...)                                                                                 \
    do {                                                                                                      \
        ka_dfx_printk(KA_KERN_NOTICE "[ascend] [%s] [%s %d] <%s:%d:%d> " fmt, ubdrv_name, __func__, __LINE__, \
                      ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(),      \
                      ##__VA_ARGS__);                                                                         \
    } while (0)
#define ubdrv_debug(fmt, ...)                                                                                \
    do {                                                                                                     \
        ka_dfx_printk(KA_KERN_DEBUG "[ascend] [%s] [%s %d] <%s:%d:%d> " fmt, ubdrv_name, __func__, __LINE__, \
                      ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(),     \
                      ##__VA_ARGS__);                                                                        \
    } while (0)
#define ubdrv_err_hex_dump(buf, len, fmt, ...)                                                               \
    do {                                                                                                     \
        ka_dfx_printk(KA_KERN_DEBUG "[ascend] [%s] [%s %d] <%s:%d:%d> " fmt, ubdrv_name, __func__, __LINE__, \
                      ka_task_get_current_comm(), ka_task_get_current_tgid(), ka_task_get_current_pid(),     \
                      ##__VA_ARGS__);                                                                        \
    } while (0)
#endif // CFG_FEATURE_DRV_LOG

enum ubdrv_log_level {
    UBDRV_DEBUG_LEVEL = 0,
    UBDRV_INFO_LEVEL,
    UBDRV_WARN_LEVEL,
    UBDRV_ERR_LEVEL,
    UBDRV_NONE_LEVEL,
    UBDRV_MAX_LEVEL
};

#define UBDRV_LOG_LEVEL(level, fmt, ...)               \
    do {                                               \
        if ((level) == (int)UBDRV_DEBUG_LEVEL) {       \
            ubdrv_debug(fmt, ##__VA_ARGS__);           \
        } else if ((level) == (int)UBDRV_INFO_LEVEL) { \
            ubdrv_info(fmt, ##__VA_ARGS__);            \
        } else if ((level) == (int)UBDRV_WARN_LEVEL) { \
            ubdrv_warn(fmt, ##__VA_ARGS__);            \
        } else if ((level) == (int)UBDRV_ERR_LEVEL) {  \
            ubdrv_err(fmt, ##__VA_ARGS__);             \
        }                                              \
    } while (0)

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#endif