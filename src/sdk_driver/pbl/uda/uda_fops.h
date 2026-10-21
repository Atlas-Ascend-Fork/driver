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

#ifndef UDA_FOPS_H
#define UDA_FOPS_H

#ifdef CFG_FEATURE_ASCEND950_STUB
u32 uda_get_raw_proc_is_contain_flag(void);
#endif

int uda_init_module(void);
void uda_exit_module(void);

#endif
