/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#ifndef APM_KERN_LOG_H
#define APM_KERN_LOG_H

#include "ka_task_pub.h"
#include "ka_system_pub.h"

#ifndef EMU_ST
#include "dmc_kernel_interface.h"
#else
#include "ut_log.h"
#endif

#define MODULE_APM "apm"

void apm_share_log_err(const char *fmt, ...);
void apm_share_log_run_info(const char *fmt, ...);

#define apm_err(fmt, ...)                                                                                 \
    do {                                                                                                  \
        drv_err(MODULE_APM, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
        apm_share_log_err(fmt, ##__VA_ARGS__);                                                            \
    } while (0)

#define apm_warn(fmt, ...)                                                                                 \
    do {                                                                                                   \
        drv_warn(MODULE_APM, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                 ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
    } while (0)

#define apm_info(fmt, ...)                                                                                 \
    do {                                                                                                   \
        drv_info(MODULE_APM, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                 ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
        apm_share_log_run_info(fmt, ##__VA_ARGS__);                                                        \
    } while (0)

#define apm_debug(fmt, ...)                                                                                    \
    do {                                                                                                       \
        drv_pr_debug(MODULE_APM, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                     ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
    } while (0)

#define apm_err_if(cond, fmt, ...)       \
    do {                                 \
        if (cond)                        \
            apm_err(fmt, ##__VA_ARGS__); \
    } while (0)

#define apm_info_if(cond, fmt, ...)       \
    do {                                  \
        if (cond)                         \
            apm_info(fmt, ##__VA_ARGS__); \
    } while (0)

#define apm_debug_if(cond, fmt, ...)       \
    do {                                   \
        if (cond)                          \
            apm_debug(fmt, ##__VA_ARGS__); \
    } while (0)

#define apm_warn_ratelimited(fmt, ...)                                                                                 \
    do {                                                                                                               \
        drv_warn_ratelimited(MODULE_APM, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                             ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
    } while (0)

#define apm_info_ratelimited(fmt, ...)                                                                                 \
    do {                                                                                                               \
        drv_info_ratelimited(MODULE_APM, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                             ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
    } while (0)

#define apm_err_ratelimited(fmt, ...)                                                                                 \
    do {                                                                                                              \
        drv_err_ratelimited(MODULE_APM, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                            ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);              \
    } while (0)

#endif
