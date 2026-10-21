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

#ifndef URD_DEFINE_H
#define URD_DEFINE_H

#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "dmc_kernel_interface.h"
#include "urd_define_adapt.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#ifndef __KA_GFP_ACCOUNT
#ifdef __GFP_KMEMCG
#define __KA_GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif
#ifdef __GFP_NOACCOUNT
#define __KA_GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif
#endif

/*
 * kernel log
 */
#define MODULE_URD "urd_module"

#ifndef dms_err

#ifdef UT_VCAST /* for ut test */
#define dms_err(fmt, ...) drv_err(MODULE_URD, fmt, ##__VA_ARGS__)
#define dms_warn(fmt, ...) drv_warn(MODULE_URD, fmt, ##__VA_ARGS__)
#define dms_info(fmt, ...) drv_info(MODULE_URD, fmt, ##__VA_ARGS__)
#define dms_event(fmt, ...) drv_event(MODULE_URD, fmt, ##__VA_ARGS__)
#define dms_debug(fmt, ...) drv_pr_debug(MODULE_URD, fmt, ##__VA_ARGS__)
#else
#define dms_err(fmt, ...)                                                                              \
    do {                                                                                               \
        drv_err(MODULE_URD, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                ka_task_get_current_pid(), ##__VA_ARGS__);                                             \
        share_log_err(DEVMNG_SHARE_LOG_START, fmt, ##__VA_ARGS__);                                     \
    } while (0)
#define dms_warn(fmt, ...)                                                                          \
    drv_warn(MODULE_URD, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
             ka_task_get_current_pid(), ##__VA_ARGS__)
#define dms_info(fmt, ...)                                                                          \
    drv_info(MODULE_URD, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
             ka_task_get_current_pid(), ##__VA_ARGS__)
#define dms_event(fmt, ...)                                                                          \
    drv_event(MODULE_URD, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
              ka_task_get_current_pid(), ##__VA_ARGS__)
#define dms_debug(fmt, ...)                                                                             \
    drv_pr_debug(MODULE_URD, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                 ka_task_get_current_pid(), ##__VA_ARGS__)
#endif
#define dms_ex_notsupport_err(ret, fmt, ...)                                   \
    do {                                                                       \
        if (((ret) != (int)DRV_ERROR_NOT_SUPPORT) && ((ret) != -EOPNOTSUPP)) { \
            dms_err(fmt, ##__VA_ARGS__);                                       \
        }                                                                      \
    } while (0)
#endif

#endif /* __URD_DEFINE_H__ */
