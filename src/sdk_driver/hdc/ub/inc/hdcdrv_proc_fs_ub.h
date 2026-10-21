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
#ifndef _HDCDRV_PROC_FS_H_
#define _HDCDRV_PROC_FS_H_

#include "hdcdrv_core_ub.h"

#define HDCDRV_ATTR_RD (KA_S_IRUSR | KA_S_IRGRP)
#define HDCDRV_ATTR_WR (KA_S_IWUSR | KA_S_IWGRP)
#define HDCDRV_ATTR_RW (HDCDRV_ATTR_RD | HDCDRV_ATTR_WR)

struct hdcdrv_user_input {
    u32 dev_id;
};

struct hdcdrv_procfs_entry {
    ka_proc_dir_entry_t *dev_id;
    ka_proc_dir_entry_t *hdcdrv_dev_stat;
};

struct hdcdrv_session_list_stat {
    int active_num;
    int *active_list;
    int idle_num;
    int *idle_list;
};

int hdcdrv_proc_fs_init(void);
void hdcdrv_proc_fs_uninit(void);
#endif