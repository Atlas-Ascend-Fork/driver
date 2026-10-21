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

#ifndef __DMS_EVENT_DISTRIBUTE_H__
#define __DMS_EVENT_DISTRIBUTE_H__
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "dms/dms_cmd_def.h"
#include "fms/fms_smf.h"

#define DMS_EVENT_DISTRIBUTE_FUNC_MAX 64
#define DISTRIBUTE_FUNC_HEAD_INDEX(priority) (DMS_EVENT_DISTRIBUTE_FUNC_MAX / DMS_DISTRIBUTE_PRIORITY_MAX * (priority))
#define DISTRIBUTE_FUNC_TAIL_INDEX(priority) \
    (DMS_EVENT_DISTRIBUTE_FUNC_MAX / DMS_DISTRIBUTE_PRIORITY_MAX * (priority + 1))

#define DMS_EVENT_MAX_PROC_NUM_DSMI 64
/* means max 64 device, each device has 64 process */
#define DMS_EVENT_MAX_PROC_NUM_HAL (64 * 64)

/* halGetFaultEvent occupy 1 */
#define DMS_EVENT_EXCEPTION_PROCESS_MAX (DMS_EVENT_MAX_PROC_NUM_DSMI + DMS_EVENT_MAX_PROC_NUM_HAL + 1)

#define DMS_EVENT_EXCEPTION_PROCESS_KERNEL_START 0
#define DMS_EVENT_EXCEPTION_PROCESS_KERNEL_END (DMS_EVENT_EXCEPTION_PROCESS_KERNEL_START + 1)
#define DMS_EVENT_EXCEPTION_PROCESS_DSMI_START DMS_EVENT_EXCEPTION_PROCESS_KERNEL_END
#define DMS_EVENT_EXCEPTION_PROCESS_DSMI_END (DMS_EVENT_EXCEPTION_PROCESS_DSMI_START + DMS_EVENT_MAX_PROC_NUM_DSMI)
#define DMS_EVENT_EXCEPTION_PROCESS_HAL_START DMS_EVENT_EXCEPTION_PROCESS_DSMI_END
#define DMS_EVENT_EXCEPTION_PROCESS_HAL_END (DMS_EVENT_EXCEPTION_PROCESS_HAL_START + DMS_EVENT_MAX_PROC_NUM_HAL)

#define DMS_EVENT_PROCESS_STATUS_IDLE 0
#define DMS_EVENT_PROCESS_STATUS_WORK 1

#define DMS_EVENT_NO_TIMEOUT_FLAG (-1)
#define DMS_EVENT_WAIT_TIME 1000      /* 1000ms */
#define DMS_EVENT_WAIT_TIME_MAX 30000 /* 30000ms(30s) */

#define DMS_EVENT_FILTER_FLAG_EVENT_ID (1UL << 0)
#define DMS_EVENT_FILTER_FLAG_SERVERITY (1UL << 1)
#define DMS_EVENT_FILTER_FLAG_NODE_TYPE (1UL << 2)
#define DMS_EVENT_FILTER_FLAG_HOST_PID (1UL << 3)

typedef struct {
    ka_kfifo_t event_fifo;
    ka_wait_queue_head_t event_wait;
    ka_mutex_t process_mutex;

    u32 exception_num;
    ka_pid_t process_pid;
    ka_pid_t process_tid;
    u64 start_time; /* pid start time, not tgid */
    u8 process_status;
    u32 subscribe_udevids[ASCEND_PDEV_MAX_NUM];
    u32 subscribe_udevids_num;
    struct dms_event_filter filter;
    enum cmd_source cmd_src;
} DMS_EVENT_PROCESS_STRU;

typedef struct {
    DMS_EVENT_PROCESS_STRU event_process[DMS_EVENT_EXCEPTION_PROCESS_MAX];
    u32 process_num;
    u32 process_max_idx_dsmi;
    u32 process_max_idx_hal;
    u32 ns_id;
} DMS_EVENT_STRU_T;

int dms_event_clear_exception(u32 devid);
int dms_event_distribute_to_process(DMS_EVENT_NODE_STRU *exception_node);
void dms_event_subscribe_unregister_all(void);
void dms_event_distribute_stru_init(void);
void dms_event_distribute_stru_exit(void);
void dms_device_fault_event_init(void);
void dms_device_fault_event_exit(void);

#endif
