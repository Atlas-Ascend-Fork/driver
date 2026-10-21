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

#include "esched_adapt_platform.h"
#include "dpa/dpa_pids_map.h"

int esched_adapt_query_master_pid(u32 chip_id, u32 dst_pid, u32 *master_pid)
{
    return devdrv_query_master_pid_by_device_slave(chip_id, dst_pid, master_pid);
}

int esched_drv_map_host_dev_pid(struct sched_proc_ctx *proc_ctx, u32 identity)
{
    (void)proc_ctx;
    (void)identity;
    return 0;
}

int esched_get_real_pid(struct topic_sched_mailbox *mb, u32 devid, u32 pid)
{
    int ret;
    u32 host_pid;

    (void)pid;
    ret = devdrv_query_master_pid_by_device_slave(devid, mb->pid, &host_pid);
    if (ret != 0) {
        if (!esched_log_limited(SCHED_LOG_LIMIT_GET_REAL_PID)) {
            sched_err("Failed to get host master pid. (mb->pid=%u; ret=%d)\n", mb->pid, ret);
        }
        return DRV_ERROR_NO_PROCESS;
    }

    mb->pid = host_pid;
    sched_debug("Get host slave pid(mb->pid=%u; master_pid=%u)\n", mb->pid, host_pid);
    return DRV_ERROR_NONE;
}
