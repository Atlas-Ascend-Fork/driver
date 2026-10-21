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

#ifndef __COMM_KERNEL_INTERFACE_H__
#define __COMM_KERNEL_INTERFACE_H__

#include "comm/comm_msg_chan.h"
#include "comm/comm_pcie.h"

int esched_drv_sqe_update(unsigned int devid, unsigned int tsid, int pid, unsigned int sqid, void *sqe);

#endif /* __COMM_KERNEL_INTERFACE_H__ */