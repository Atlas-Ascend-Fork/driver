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

#ifndef _UDIS_DFX_H_
#define _UDIS_DFX_H_

#include "udis_define.h"
#include "udis.h"

#define UDIS_DFX_STAT_MAX 256
#define UDIS_DFX_WINDOW_GRANULARITY_MS 1000
#define UDIS_DFX_DEFAULT_WINDOW_SEC 1
#define UDIS_DFX_MAX_WINDOW_SEC 10

struct udis_dfx_stat_entry {
    ka_list_head_t list;
    UDIS_MODULE_TYPE module_type;
    char name[UDIS_MAX_NAME_LEN];
    unsigned int acc_ctrl;
    unsigned long long register_count;
    unsigned long long unregister_count;
    unsigned long long set_count_total;
    unsigned int set_count_window;
    unsigned long long last_window_count;
    unsigned long long last_window_start_ms;
    unsigned long long last_window_end_ms;
    unsigned long long window_start_ms;
    unsigned long long last_op_time_ms;
};

int udis_dfx_init(void);
void udis_dfx_uninit(void);

void udis_dfx_record_register(UDIS_MODULE_TYPE module_type, const char *name, unsigned int acc_ctrl);
void udis_dfx_record_unregister(UDIS_MODULE_TYPE module_type, const char *name);
void udis_dfx_record_set(UDIS_MODULE_TYPE module_type, const char *name, unsigned int acc_ctrl);
void udis_dfx_reset_stat(void);
void udis_dfx_set_window(unsigned int window_sec);

#endif
