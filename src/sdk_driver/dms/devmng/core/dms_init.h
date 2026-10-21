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

#ifndef __DMS_INIT_H__
#define __DMS_INIT_H__
#include "dms_define.h"

#define HOST_LOCAL 1000

struct sub_module_ops {
    int (*init)(void);
    void (*uninit)(void);
};
int get_eventinfo_from_config(void);

int dms_init(void);
void dms_exit(void);

#endif
