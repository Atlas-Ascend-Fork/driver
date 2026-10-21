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

#ifndef FEATURE_LOG_H
#define FEATURE_LOG_H

#include "ka_task_pub.h"
#include "dmc_kernel_interface.h"

#define MODULE_FEATURE "feature"

#define feature_err(fmt, ...) \
    drv_err(MODULE_FEATURE, "<%s:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ##__VA_ARGS__)

#define feature_warn(fmt, ...) \
    drv_warn(MODULE_FEATURE, "<%s:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ##__VA_ARGS__)

#define feature_info(fmt, ...) \
    drv_info(MODULE_FEATURE, "<%s:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ##__VA_ARGS__)

#define feature_debug(fmt, ...) \
    drv_pr_debug(MODULE_FEATURE, "<%s:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), ##__VA_ARGS__)

#endif /* FEATURE_LOG_H */
