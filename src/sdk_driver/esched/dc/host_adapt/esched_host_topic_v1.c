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

int esched_adapt_restore_mb_usr_pid(struct topic_sched_mailbox *mb, u32 *pid)
{
#ifndef EMU_ST
    mb->usr_pid_flag = 0;
    return esched_mb_user_data_subtract(mb, (u16)sizeof(u32), pid);
#endif
    return 0;
}

int esched_update_sqe_user_data(struct topic_sched_sqe *sqe, struct sched_published_event_info *event_info)
{
    (void)sqe;
    (void)event_info;
    return 0;
}

int esched_adapt_fill_sqe_host_pid(u32 chip_id, struct sched_published_event_info *event_info,
                                   struct topic_sched_sqe *sqe, u32 *host_pid)
{
    int ret;

    (void)sqe;
    *host_pid = (u32)ka_task_get_current()->tgid;
    ret = devdrv_query_master_pid_by_device_slave(chip_id, event_info->pid, host_pid);
    if (ret == 0) {
        sched_debug("Query master pid success. (pid=%d; host_pid=%u; current_pid=%d)\n", event_info->pid, *host_pid,
                    ka_task_get_current()->tgid);
    }
    return 0;
}
