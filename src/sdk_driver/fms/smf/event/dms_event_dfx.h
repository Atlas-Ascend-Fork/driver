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

#ifndef __DMS_EVENT_DFX_H__
#define __DMS_EVENT_DFX_H__

#include "fms/fms_smf.h"

#define EVENT_DFX_BUF_SIZE_MAX (1024 * 4)
#define EVENT_DFX_CHECK_DO_SOMETHING(condition, something) \
    do {                                                   \
        if (condition) {                                   \
            something;                                     \
        }                                                  \
    } while (0)

extern int sscanf_s(const char *buffer, const char *format, ...);

ssize_t dms_event_print_convergent_diagrams(u32 devid, char opt, char *str);
ssize_t dms_event_print_event_list(u32 devid, char *str);
ssize_t dms_event_print_mask_list(u32 devid, char *str);
ssize_t dms_event_print_subscribe_handle(char *str);
ssize_t dms_event_print_subscribe_process(char *str);

#endif
