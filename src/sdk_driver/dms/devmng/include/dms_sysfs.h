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

#ifndef __DMS_SYSFS_H__
#define __DMS_SYSFS_H__
#include "dms_define.h"

struct dsmi_dfx {
    u32 buffer_size;
    u64 write_cnt;
    u64 write_bytes;
    u64 overwrite_cnt;
    u32 buf_used;
    u32 buf_free;
    u32 w_pos;
    int last_write_ret;
};

#define DSMI_LOG_BUF_SIZE 4095
struct dsmi_log_ctrl {
    struct dsmi_dfx dfx;
    char buf[DSMI_LOG_BUF_SIZE];
    ka_mutex_t lock;
};

/* dms */
void dms_sysfs_init(void);
void dms_sysfs_uninit(void);
#endif