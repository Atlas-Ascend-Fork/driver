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

#ifndef UDA_PROC_FS_H
#define UDA_PROC_FS_H

#include "ka_common_pub.h"

int uda_notifier_open(ka_inode_t *inode, ka_file_t *file);
int uda_ns_node_open(ka_inode_t *inode, ka_file_t *file);
int uda_udev_open(ka_inode_t *inode, ka_file_t *file);
int uda_detected_device_open(ka_inode_t *inode, ka_file_t *file);

void uda_proc_fs_init(void);
void uda_proc_fs_uninit(void);

#endif
