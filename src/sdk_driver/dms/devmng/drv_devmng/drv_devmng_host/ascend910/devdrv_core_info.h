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

#ifndef DEVDRV_CORE_INFO_H
#define DEVDRV_CORE_INFO_H

#include "devdrv_manager.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager_msg.h"
#include "devdrv_common.h"
#include "comm_kernel_interface.h"
#include "dms_hotreset.h"
#include "devdrv_manager_container.h"

int devdrv_manager_get_core_utilization(unsigned long arg);
int devdrv_manager_get_core(ka_file_t *filep, unsigned int cmd, unsigned long arg);
int devdrv_manager_host_get_group_info(struct devdrv_manager_msg_info *dev_manager_msg_info,
                                       struct get_ts_group_para *group_para, struct devdrv_info *info);
int devdrv_manager_get_group_para(struct devdrv_ioctl_info *ioctl_buf, struct get_ts_group_para *group_para,
                                  unsigned long arg);
int devdrv_manager_get_ts_group_info(ka_file_t *filep, unsigned int cmd, unsigned long arg);
int devdrv_manager_get_tsdrv_dev_com_info(ka_file_t *filep, unsigned int cmd, unsigned long arg);
#ifdef CFG_FEATURE_DEVMNG_IOCTL
int devdrv_manager_get_h2d_devinfo(unsigned long arg);
#endif
u32 devdrv_get_ts_num(void);

#endif