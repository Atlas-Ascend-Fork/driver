/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BBOX_TOOL_LOG_H
#define BBOX_TOOL_LOG_H

#include <stdio.h>
#include "bbox_int.h"

#define DATE_MAXLEN 32

#ifdef NOMSG_PRINT
#define LOG_FPRINTF(format, ...)    \
    do {                            \
        bbox_dump_log_get_stream(); \
        bbox_dump_log_get_time();   \
    } while (0)
#define BBOX_PRINT(level, msg, ...)
#else
#define LOG_FPRINTF(format, ...)                                                                    \
    do {                                                                                            \
        fprintf(bbox_dump_log_get_stream(), "%s " format, bbox_dump_log_get_time(), ##__VA_ARGS__); \
        fflush(bbox_dump_log_get_stream());                                                         \
    } while (0)
#define BBOX_PRINT(level, msg, ...) \
    printf("%s [%s] %s:%d: " msg "\n", bbox_dump_log_get_time(), level, __func__, __LINE__, ##__VA_ARGS__)
#endif

FILE *bbox_dump_log_get_stream(void);
const char *bbox_dump_log_get_time(void);

void bbox_dump_log_set_log_config(u32 print_mode, u32 level);
u32 bbox_print_get_log_level(void);
u32 bbox_print_get_log_mode(void);

#endif
