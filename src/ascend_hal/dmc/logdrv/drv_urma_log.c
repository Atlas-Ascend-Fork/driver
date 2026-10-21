/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <syslog.h>
#include "ascend_hal_define.h"
#include "urma_types.h"
#include "log_types.h"
#include "dmc/dmc_log_user.h"
#include "dmc/drv_log_urma.h"

__attribute__((weak)) urma_status_t urma_register_log_func(urma_log_cb_t cb);
__attribute__((weak)) urma_status_t urma_unregister_log_func(void);
__attribute__((weak)) void urma_log_set_level(urma_vlog_level_t level);
__attribute__((weak)) urma_vlog_level_t urma_log_get_level(void);

static const int32_t g_ub2alog_level[URMA_VLOG_LEVEL_MAX] = {
    [URMA_VLOG_LEVEL_EMERG] = DLOG_ERROR, [URMA_VLOG_LEVEL_ALERT] = DLOG_ERROR,  [URMA_VLOG_LEVEL_CRIT] = DLOG_ERROR,
    [URMA_VLOG_LEVEL_ERR] = DLOG_ERROR,   [URMA_VLOG_LEVEL_WARNING] = DLOG_WARN, [URMA_VLOG_LEVEL_NOTICE] = DLOG_INFO,
    [URMA_VLOG_LEVEL_INFO] = DLOG_INFO,   [URMA_VLOG_LEVEL_DEBUG] = DLOG_DEBUG,
};
#define DLOG_MAX ((uint32_t)(DLOG_NULL + 1U))
static const urma_vlog_level_t g_alog2ub_level[DLOG_MAX] = {
    [DLOG_DEBUG] = URMA_VLOG_LEVEL_DEBUG, [DLOG_INFO] = URMA_VLOG_LEVEL_INFO, [DLOG_WARN] = URMA_VLOG_LEVEL_WARNING,
    [DLOG_ERROR] = URMA_VLOG_LEVEL_ERR,   [DLOG_NULL] = URMA_VLOG_LEVEL_ERR,
};

static alog_print_func g_urma_log_print = NULL;

static void urma_log_report_func(int level, char *message)
{
    if ((g_urma_log_print == NULL) || (level < 0) || (level >= (int)URMA_VLOG_LEVEL_MAX) || (message == NULL)) {
        return;
    }
    if (urma_log_get_level == NULL) {
        return;
    }
    if (level <= (int)urma_log_get_level()) {
        g_urma_log_print(UNIFIEDBUS, g_ub2alog_level[level], "%s", message);
    }
}

int32_t drv_log_urma_register(alog_print_func log_print, uint32_t log_level)
{
    int32_t rgs_ret;

    if ((log_print == NULL) || (log_level >= DLOG_MAX)) {
        DRV_LOG_ERR("Invalid param. (log_print_is_null=%d, log_level=%u)\n", (int)(log_print == NULL), log_level);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (urma_register_log_func == NULL) {
        DRV_LOG_DEBUG("Urma register log func not support.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }
    g_urma_log_print = log_print;
    rgs_ret = urma_register_log_func(urma_log_report_func);
    if (rgs_ret != 0) {
        g_urma_log_print = NULL;
        DRV_LOG_ERR("Failed to register urma log func. (rgs_ret=%d)\n", rgs_ret);
        return DRV_ERROR_INNER_ERR;
    }
    (void)drv_log_set_urma_log_level(log_level);
    DRV_LOG_INFO("Register urma log successfully.\n");
    return DRV_ERROR_NONE;
}

void drv_log_urma_unregister(void)
{
    if (urma_unregister_log_func == NULL) {
        return;
    }
    (void)urma_unregister_log_func();
    g_urma_log_print = NULL;
}

int32_t drv_log_set_urma_log_level(uint32_t level)
{
    if (level >= DLOG_MAX) {
        DRV_LOG_ERR("Invalid param. (level=%u)\n", level);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (urma_log_set_level == NULL) {
        DRV_LOG_DEBUG("Urma log set level not support.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }
    urma_log_set_level(g_alog2ub_level[level]);
    DRV_LOG_INFO("Set urma log level successfully. (level=%u)\n", level);
    return DRV_ERROR_NONE;
}
