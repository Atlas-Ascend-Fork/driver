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

#include "esched.h"
#include "ascend_kernel_hal.h"
#include "trs_chan.h"

void esched_setup_dev_hw_ops(struct sched_dev_ops *ops)
{}

int trs_chan_unregister_abnormal_handle(u32 task_type)
{
    return 0;
}

int esched_drv_abnormal_task_handle(struct trs_id_inst *inst, u32 sqid, void *sqe, void *info)
{
    return 0;
}

int trs_chan_register_abnormal_handle(u32 task_type, trs_chan_abnormal_handle handle)
{
    return 0;
}
