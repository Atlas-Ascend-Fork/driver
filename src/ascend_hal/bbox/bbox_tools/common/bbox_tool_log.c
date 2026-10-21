/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "bbox_tool_log.h"
#include "bbox_print.h"
#include "bbox_utils.h"
#include "bbox_log_common.h"
#include "bbox_system_api.h"
#include "bbox_tool_fs.h"

static FILE *g_log_fp = NULL;
struct bbox_log_config {
    u32 print_mode;
    u32 level;
};

static struct bbox_log_config g_bbox_log_config = {PRINT_SYSLOG, LOG_INFO};
/**
 * @brief       get log file stream
 * @return      FILE * filestream
 */
FILE *bbox_dump_log_get_stream(void)
{
    return (g_log_fp == NULL) ? stdout : g_log_fp;
}

/**
 * @brief       get current timestamp string. Do not print log inside func
 * @return      time string
 */
const char *bbox_dump_log_get_time(void)
{
    static char time_str[DATE_MAXLEN] = {0};
    struct timeval tv = {0};

    if (bbox_get_time_of_day(&tv) != BBOX_SUCCESS) {
        BBOX_PERROR("gettimeofday", "");
    }

    bbox_log_time_to_str(time_str, sizeof(time_str), &tv);
    return time_str;
}

void bbox_dump_log_set_log_config(u32 print_mode, u32 level)
{
    g_bbox_log_config.print_mode = print_mode;
    g_bbox_log_config.level = level;
}

u32 bbox_print_get_log_level(void)
{
    return g_bbox_log_config.level;
}

u32 bbox_print_get_log_mode(void)
{
    return g_bbox_log_config.print_mode;
}