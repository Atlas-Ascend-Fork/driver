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

#ifndef DEVDRV_BLACK_BOX_DUMP_H
#define DEVDRV_BLACK_BOX_DUMP_H

#include "ka_fs_pub.h"
#include "devdrv_common.h"
#include "devdrv_manager_common_msg.h"

int devdrv_manager_black_box_get_exception(ka_file_t *filep, unsigned int cmd, unsigned long arg);
int devdrv_manager_device_memory_dump(ka_file_t *filep, unsigned int cmd, unsigned long arg);
int devmng_devlog_addr_init(void);
void devdrv_devlog_init(struct devdrv_info *dev_info, struct devdrv_device_info *drv_info);
int devdrv_manager_receive_devlog_addr(void *msg, u32 *ack_len);
int devdrv_manager_receive_tslog_addr(void *msg, u32 *ack_len);
int devdrv_manager_device_reset_inform(ka_file_t *filep, unsigned int cmd, unsigned long arg);
int devdrv_manager_reg_ddr_read(ka_file_t *filep, unsigned int cmd, unsigned long arg);
int devdrv_manager_tslog_dump_process(struct devdrv_black_box_user *black_box_user, void **buffer);
int devdrv_manager_device_vmcore_dump(ka_file_t *filep, unsigned int cmd, unsigned long arg);
void devmng_devlog_addr_uninit(void);
int devdrv_manager_tslog_dump(ka_file_t *filep, unsigned int cmd, unsigned long arg);
int align_to_4k(u32 size_in, u64 *aligned_size);

#endif