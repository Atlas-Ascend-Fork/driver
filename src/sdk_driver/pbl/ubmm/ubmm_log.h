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
#ifndef UBMM_LOG_H
#define UBMM_LOG_H

#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "dmc_kernel_interface.h"

#define MODULE_UBMM "ubmm"

#define ubmm_err(fmt, ...)                                                                                 \
    do {                                                                                                   \
        drv_err(MODULE_UBMM, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);               \
        share_log_err(DEVMM_SHARE_LOG_START, MODULE_UBMM, "<%s:%d> " fmt, ka_task_get_current_comm(),      \
                      ka_task_get_current_tgid(), ##__VA_ARGS__);                                          \
    } while (0)

#define ubmm_warn(fmt, ...)                                                                                 \
    do {                                                                                                    \
        drv_warn(MODULE_UBMM, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                 ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);               \
    } while (0)

#define ubmm_info(fmt, ...)                                                                                 \
    do {                                                                                                    \
        drv_info(MODULE_UBMM, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                 ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);               \
    } while (0)

#define ubmm_debug(fmt, ...)                                                                                    \
    do {                                                                                                        \
        drv_pr_debug(MODULE_UBMM, "<%s:%d:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                     ka_task_get_current_pid(), ka_system_raw_smp_processor_id(), ##__VA_ARGS__);               \
    } while (0)

#endif
