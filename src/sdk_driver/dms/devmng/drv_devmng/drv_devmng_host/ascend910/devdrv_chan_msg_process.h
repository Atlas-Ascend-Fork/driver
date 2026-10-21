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

#ifndef DEVDRV_CHAN_MSG_PROCESS_H
#define DEVDRV_CHAN_MSG_PROCESS_H

#include "devdrv_manager_common.h"
#include "comm_kernel_interface.h"
#include "securec.h"
#include "kernel_version_adapt.h"
#include "ka_task_pub.h"

void devdrv_manager_common_chan_init(void);
void devdrv_manager_common_chan_uninit(void);
void *devdrv_manager_get_no_trans_chan(u32 dev_id);
void devdrv_manager_set_no_trans_chan(u32 dev_id, void *no_trans_chan);
int devdrv_manager_none_trans_init(u32 dev_id);
void devdrv_manager_non_trans_uninit(u32 dev_id);
int devdrv_manager_init_common_chan(u32 dev_id);
struct devdrv_common_msg_client *devdrv_manager_get_common_chan(u32 dev_id);

#endif
