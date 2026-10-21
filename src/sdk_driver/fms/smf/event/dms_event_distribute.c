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

#include "ka_system_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_feature_pub.h"
#include "ka_errno_pub.h"

#include "pbl/pbl_runenv_config.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_task_ctx.h"

#include "dms_event_converge.h"
#include "fms_define.h"
#include "dms_event.h"
#include "dms_event_dfx.h"
#include "pbl_mem_alloc_interface.h"
#include "devdrv_user_common.h"
#include "smf_event_adapt.h"
#include "dms_event_distribute.h"

struct pid_info_t {
    ka_pid_t tgid;
    ka_pid_t pid;
};

#define DMS_INVALID_PID (-1)

#define DMS_EXCEPTION_KFIFO_CELL (sizeof(DMS_EVENT_NODE_STRU))
#define DMS_EXCEPTION_KFIFO_SIZE (DMS_EXCEPTION_KFIFO_CELL * DMS_MAX_EVENT_NUM)
#define DMS_FAULT_DEVICE_ALL (-1)

/* ns_id 0-127 for normal container, 128 for host or admin container */
STATIC DMS_EVENT_STRU_T *g_dms_event_ns_stu[UDA_NS_NUM + 1];
static DMS_EVENT_DISTRIBUTE_HANDLE_T g_dms_event_handle_list[DMS_EVENT_DISTRIBUTE_FUNC_MAX];
static ka_mutex_t g_dms_event_handle_mutex;
static ka_mutex_t g_dms_event_ns_node_mutex;
static struct dms_device_event g_remote_fault_event[ASCEND_PDEV_MAX_NUM];
static struct dms_device_event g_remote_notify_event[ASCEND_PDEV_MAX_NUM];
static ka_mutex_t g_remote_fault_event_mutex;

STATIC void release_one_event_proc(DMS_EVENT_STRU_T *event_ns_stu, u32 proc_idx);

struct dms_device_event *dms_get_device_event(u32 dev_id)
{
    if (dev_id >= ASCEND_PDEV_MAX_NUM) {
        return NULL;
    }

    return &g_remote_fault_event[dev_id];
}
KA_EXPORT_SYMBOL(dms_get_device_event);

struct dms_device_event *dms_get_device_notify_event(u32 dev_id)
{
    if (dev_id >= ASCEND_PDEV_MAX_NUM) {
        return NULL;
    }

    return &g_remote_notify_event[dev_id];
}
KA_EXPORT_SYMBOL(dms_get_device_notify_event);

void dms_device_fault_event_init(void)
{
    int i;

    ka_task_mutex_init(&g_remote_fault_event_mutex);
    ka_task_mutex_lock(&g_remote_fault_event_mutex);
    for (i = 0; i < ASCEND_PDEV_MAX_NUM; i++) {
        ka_task_mutex_init(&g_remote_fault_event[i].lock);
        KA_INIT_LIST_HEAD(&g_remote_fault_event[i].head);
        g_remote_fault_event[i].event_num = 0;

        ka_task_mutex_init(&g_remote_notify_event[i].lock);
        KA_INIT_LIST_HEAD(&g_remote_notify_event[i].head);
        g_remote_notify_event[i].event_num = 0;
    }
    ka_task_mutex_unlock(&g_remote_fault_event_mutex);
}

STATIC void release_one_device_remote_event(unsigned int dev_id, struct dms_device_event *device_event)
{
    DMS_EVENT_NODE_STRU *pos = NULL;
    DMS_EVENT_NODE_STRU *n = NULL;

    if (device_event == NULL) {
        return;
    }

    ka_task_mutex_lock(&device_event->lock);
    ka_list_for_each_entry_safe(pos, n, &device_event->head, node)
    {
        ka_list_del(&pos->node);
        dbl_kfree(pos);
        pos = NULL;
    }

    device_event->event_num = 0;
    device_event->highest_severity = 0;
    ka_task_mutex_unlock(&device_event->lock);
    device_event = NULL;
    dms_info("Release one device remote event. (dev_id=%u)\n", dev_id);
}

void dms_release_one_device_remote_event(unsigned int dev_id)
{
    release_one_device_remote_event(dev_id, dms_get_device_event(dev_id));
    release_one_device_remote_event(dev_id, dms_get_device_notify_event(dev_id));
}
KA_EXPORT_SYMBOL(dms_release_one_device_remote_event);

void dms_device_fault_event_exit(void)
{
    u32 i;

    ka_task_mutex_lock(&g_remote_fault_event_mutex);
    for (i = 0; i < ASCEND_PDEV_MAX_NUM; i++) {
        dms_release_one_device_remote_event(i);
        ka_task_mutex_destroy(&g_remote_fault_event[i].lock);
        ka_task_mutex_destroy(&g_remote_notify_event[i].lock);
    }
    ka_task_mutex_unlock(&g_remote_fault_event_mutex);
    ka_task_mutex_destroy(&g_remote_fault_event_mutex);
}

void dms_event_distribute_stru_init(void)
{
    u32 i;

    ka_task_mutex_init(&g_dms_event_handle_mutex);
    ka_task_mutex_init(&g_dms_event_ns_node_mutex);

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    for (i = 0; i < (UDA_NS_NUM + 1); i++) {
        g_dms_event_ns_stu[i] = NULL;
    }
    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = 0; i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        g_dms_event_handle_list[i] = NULL;
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);
}

static void dms_event_ns_stu_free(u32 ns_id)
{
    if (g_dms_event_ns_stu[ns_id] != NULL) {
        dbl_vfree(g_dms_event_ns_stu[ns_id]);
        g_dms_event_ns_stu[ns_id] = NULL;
    }
}

STATIC void dms_event_release_one_ns_node(u32 ns_id)
{
    u32 i;

    if (g_dms_event_ns_stu[ns_id] == NULL) {
        return;
    }

    for (i = 0; i < DMS_EVENT_EXCEPTION_PROCESS_MAX; i++) {
        release_one_event_proc(g_dms_event_ns_stu[ns_id], i);
    }
    if (g_dms_event_ns_stu[ns_id]->process_num == 0) {
        dms_event_ns_stu_free(ns_id);
    }
}

void dms_event_distribute_stru_exit(void)
{
    u32 i;

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    for (i = 0; i < (UDA_NS_NUM + 1); i++) {
        dms_event_release_one_ns_node(i);
    }
    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = 0; i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        g_dms_event_handle_list[i] = NULL;
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);

    ka_task_mutex_destroy(&g_dms_event_handle_mutex);
    ka_task_mutex_destroy(&g_dms_event_ns_node_mutex);
}

STATIC bool is_in_subscribe_dev_list(u32 *subscribe_udevids, u32 subscribe_udevids_num, u32 dev_id)
{
    unsigned int i;

    if (subscribe_udevids == NULL) {
        dms_err("Subscribe_udevids is NULL, subscribe_udevids_num=%u.\n", subscribe_udevids_num);
        return false;
    }
    for (i = 0; i < subscribe_udevids_num; i++) {
        if (subscribe_udevids[i] == dev_id) {
            return true;
        }
    }
    return false;
}

STATIC bool dms_filter_event_node_type(DMS_EVENT_PROCESS_STRU *event_proc, struct dms_event_para *event)
{
    unsigned short node_type, sub_node_type;
    bool sub_node_type_valid = false;

    /* Check sub_node_type, if sub_node_type is 0, then it is not valid, filter don't check it */
    sub_node_type_valid = event->sub_node_type == 0 ? false : true;

    if (event_proc->cmd_src == FROM_DSMI) {
        node_type = event->node_type & 0xFF; /* DSMI filter node type is unsigned char*/
        sub_node_type = event->sub_node_type & 0xFF;
    } else {
        node_type = event->node_type;
        sub_node_type = event->sub_node_type;
    }

    if (((event_proc->filter.filter_flag & DMS_EVENT_FILTER_FLAG_NODE_TYPE) != 0) &&
        ((node_type != event_proc->filter.node_type) &&
         (!sub_node_type_valid || (sub_node_type != event_proc->filter.node_type)))) {
        dms_debug("Node_type of event does not match subscribe node_type. "
                  "(subscribe_node_type=0x%x; node_type=0x%x; sub_node_type=0x%x; proc_tgid=%d; proc_pid=%d)\n",
                  event_proc->filter.node_type, node_type, sub_node_type, event_proc->process_pid,
                  event_proc->process_tid);
        return true;
    }

    return false;
}

STATIC bool dms_filter_event_from_device(DMS_EVENT_PROCESS_STRU *event_proc, struct dms_event_para *event)
{
    if (event_proc->cmd_src == FROM_KERNEL) { // cmd_src is FROM_KERNEL, get all events without filter
        return false;
    }

    if (!is_in_subscribe_dev_list(event_proc->subscribe_udevids, event_proc->subscribe_udevids_num,
                                  (u32)event->deviceid)) {
        dms_debug("Dev_id of event is not in subscribe_udevids. (dev_id=%u; proc_tgid=%d; proc_pid=%d)\n",
                  event->deviceid, event_proc->process_pid, event_proc->process_tid);
        return true;
    }

    if (event_proc->filter.filter_flag == 0) {
        return false;
    }

    if (((event_proc->filter.filter_flag & DMS_EVENT_FILTER_FLAG_EVENT_ID) != 0) &&
        (event->event_id != event_proc->filter.event_id)) {
        dms_debug("Event_id of event does not match subscribe event_id. (subscribe_event_id=%x; event_id=%x; "
                  "proc_tgid=%d; proc_pid=%d)\n",
                  event_proc->filter.event_id, event->event_id, event_proc->process_pid, event_proc->process_tid);
        return true;
    }

    if ((((event_proc->filter.filter_flag & DMS_EVENT_FILTER_FLAG_SERVERITY) != 0) &&
         (event->severity < event_proc->filter.severity))) {
        dms_debug("Severity of event does not match subscribe severity. (subscribe_severity=%u; severity=%u; "
                  "proc_tgid=%d; proc_pid=%d)\n",
                  event_proc->filter.severity, event->severity, event_proc->process_pid, event_proc->process_tid);
        return true;
    }

    if (dms_filter_event_node_type(event_proc, event)) {
        return true;
    }

    /* Only hal need check host_pid */
    if (((event_proc->cmd_src == FROM_HAL) && (event_proc->filter.filter_flag & DMS_EVENT_FILTER_FLAG_HOST_PID) != 0)) {
        if (*(int *)event->event_info != event_proc->process_pid) {
            dms_debug("Process_pid of event does not match subscribe process_pid. (proc_tgid=%d; proc_pid=%d; "
                      "process_pid=%d)\n",
                      event_proc->process_pid, event_proc->process_tid, *(int *)event->event_info);
            return true;
        }
    }
    return false;
}

#define ONE_YEAR_MS_CNT 31536000000ULL
static void dms_adjust_alarm_raised_time(struct dms_event_para *dms_event)
{
#ifdef CFG_EDGE_HOST
    int ret;
    ka_timespec64_t ts;
    u64 real_time_ms;
    int pre_len = 0;
    char *time_prt = NULL;
    char *assertion_prt = NULL;
    char assertion_buff[DMS_MAX_EVENT_NAME_LENGTH] = {0};

    /* 2: DMS_EVENT_TYPE_ONE_TIME, Notification Event, others Event return */
    if (dms_event->assertion != DMS_EVENT_TYPE_ONE_TIME || dms_event->alarm_raised_time > ONE_YEAR_MS_CNT) {
        return;
    }

    ka_system_ktime_get_real_ts64(&ts);
    real_time_ms = ((u64)ts.tv_sec * KA_MSEC_PER_SEC) + ((u64)ts.tv_nsec / KA_USEC_PER_SEC);
    if (dms_event->alarm_raised_time > real_time_ms) {
        return;
    }
    dms_event->alarm_raised_time = real_time_ms;

    /* event_name: "xxx, time=xxx, event assertion=xxx.", now need to rewrite the part of "time=". */
    time_prt = strstr(dms_event->event_name, "time=");
    if (time_prt == NULL) {
        dms_warn("Unable to invoke the strstr.\n");
        return;
    }
    pre_len = (int)(time_prt - dms_event->event_name);

    assertion_prt = strstr(dms_event->event_name, "event assertion=");
    if (assertion_prt == NULL) {
        dms_warn("Unable to invoke the strstr.\n");
        return;
    }
    ret = strcpy_s(assertion_buff, DMS_MAX_EVENT_NAME_LENGTH, assertion_prt);
    if (ret != 0) {
        dms_warn("Unable to invoke the strcpy_s. (ret=%d).\n", ret);
        return;
    }

    ret = snprintf_s(dms_event->event_name + pre_len, (size_t)(DMS_MAX_EVENT_NAME_LENGTH - pre_len),
                     (size_t)(DMS_MAX_EVENT_NAME_LENGTH - 1 - pre_len), "time=%llu ms, %s",
                     dms_event->alarm_raised_time, assertion_buff);
    if (ret < 0) {
        dms_warn("Unable to invoke the sprintf_s. (ret=%d).\n", ret);
        return;
    }
#else
    (void)dms_event;
    /* device not check report time */
#endif
}

STATIC int add_exception_to_event_proc(DMS_EVENT_PROCESS_STRU *event_proc, DMS_EVENT_NODE_STRU *exception_node)
{
    DMS_EVENT_NODE_STRU exception_buf = {{0}, {0}};

    ka_task_mutex_lock(&event_proc->process_mutex);
    if (event_proc->process_status != DMS_EVENT_PROCESS_STATUS_WORK) {
        ka_task_mutex_unlock(&event_proc->process_mutex);
        return DRV_ERROR_NONE;
    }

    if (dms_filter_event_from_device(event_proc, &exception_node->event)) {
        ka_task_mutex_unlock(&event_proc->process_mutex);
        return DRV_ERROR_NONE;
    }

    while (ka_base_kfifo_avail(&event_proc->event_fifo) < DMS_EXCEPTION_KFIFO_CELL) {
        if (!ka_base_kfifo_out(&event_proc->event_fifo, &exception_buf, DMS_EXCEPTION_KFIFO_CELL)) {
            dms_warn("ka_base_kfifo_out warn.\n");
        }
        dms_event("Dms event is covered. (dev_id=%u; event_id=0x%X; event_serial_num=%d; notify_serial_num=%d; "
                  "pid=%d; tid=%d)\n",
                  exception_buf.event.deviceid, exception_buf.event.event_id, exception_buf.event.event_serial_num,
                  exception_buf.event.notify_serial_num, event_proc->process_pid, event_proc->process_tid);
    }

    ka_base_kfifo_in(&event_proc->event_fifo, exception_node, DMS_EXCEPTION_KFIFO_CELL);
    event_proc->exception_num = ka_base_kfifo_len(&event_proc->event_fifo) / DMS_EXCEPTION_KFIFO_CELL;
    ka_task_mutex_unlock(&event_proc->process_mutex);

    ka_task_wake_up_interruptible(&event_proc->event_wait);
    return DRV_ERROR_NONE;
}

int dms_event_distribute_to_process(DMS_EVENT_NODE_STRU *exception_node)
{
    int ret;
    int fail_num = 0;
    u32 ns_id;
    u32 idx;

    dms_info("Receive an event. (dev_id=%u; event_id=0x%x; node_id=%u; assertion=%u; alarm_raised_time=%llu ms)\n",
             exception_node->event.deviceid, exception_node->event.event_id, exception_node->event.node_id,
             exception_node->event.assertion, exception_node->event.alarm_raised_time);

    dms_adjust_alarm_raised_time(&exception_node->event);

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    for (ns_id = 0; ns_id <= UDA_NS_NUM; ns_id++) {
        if (g_dms_event_ns_stu[ns_id] == NULL) {
            continue;
        }

        if (g_dms_event_ns_stu[ns_id]->process_max_idx_hal < g_dms_event_ns_stu[ns_id]->process_num) {
            fail_num++;
            dms_err("Check process_max_idx_hal fail. "
                    "(ns_id=%u; process_num=%u; process_max_idx_dsmi:%u; process_max_idx_hal:%u)\n",
                    ns_id, g_dms_event_ns_stu[ns_id]->process_num, g_dms_event_ns_stu[ns_id]->process_max_idx_dsmi,
                    g_dms_event_ns_stu[ns_id]->process_max_idx_hal);
        }
        for (idx = 0; idx < DMS_EVENT_EXCEPTION_PROCESS_MAX; idx++) {
            if (idx < DMS_EVENT_EXCEPTION_PROCESS_DSMI_END && idx > g_dms_event_ns_stu[ns_id]->process_max_idx_dsmi) {
                continue;
            } else if (idx > g_dms_event_ns_stu[ns_id]->process_max_idx_hal) {
                break;
            }

            ret = add_exception_to_event_proc(&g_dms_event_ns_stu[ns_id]->event_process[idx], exception_node);
            if (ret != 0) {
                fail_num++;
                dms_err("Add event code to process failed. "
                        "(dev_id=%u; event_code=0x%x; ns_id=%u; process_index=%u)\n",
                        exception_node->event.deviceid, exception_node->event.event_code, ns_id, idx);
            }
        }
    }

    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
    if (fail_num != 0) {
        dms_err("Distribute event code to process list failed. "
                "(dev_id=%u; event_code=0x%x; fail_num=%d)\n",
                exception_node->event.deviceid, exception_node->event.event_code, fail_num);
    }
    return fail_num;
}

static int get_process_index_range(enum cmd_source cmd_src, u32 *start_index, u32 *end_index)
{
    switch (cmd_src) {
        case FROM_DSMI:
            *start_index = DMS_EVENT_EXCEPTION_PROCESS_DSMI_START;
            *end_index = DMS_EVENT_EXCEPTION_PROCESS_DSMI_END;
            return 0;
        case FROM_HAL:
            *start_index = DMS_EVENT_EXCEPTION_PROCESS_HAL_START;
            *end_index = DMS_EVENT_EXCEPTION_PROCESS_HAL_END;
            return 0;
        case FROM_KERNEL:
            *start_index = DMS_EVENT_EXCEPTION_PROCESS_KERNEL_START;
            *end_index = DMS_EVENT_EXCEPTION_PROCESS_KERNEL_END;
            return 0;
        default:
            return DRV_ERROR_PARA_ERROR;
    }
}

STATIC void increase_process_max_idx(DMS_EVENT_STRU_T *event_ns_stu, u32 idx)
{
    if (idx >= DMS_EVENT_EXCEPTION_PROCESS_DSMI_START && idx < DMS_EVENT_EXCEPTION_PROCESS_DSMI_END) {
        event_ns_stu->process_max_idx_dsmi = idx > event_ns_stu->process_max_idx_dsmi ?
                                                 idx :
                                                 event_ns_stu->process_max_idx_dsmi;
    } else if (idx >= DMS_EVENT_EXCEPTION_PROCESS_HAL_START && idx < DMS_EVENT_EXCEPTION_PROCESS_HAL_END) {
        event_ns_stu->process_max_idx_hal = idx > event_ns_stu->process_max_idx_hal ? idx :
                                                                                      event_ns_stu->process_max_idx_hal;
    }
}

static DMS_EVENT_PROCESS_STRU *alloc_new_event_proc(DMS_EVENT_STRU_T *event_ns_stu, struct pid_info_t pid_info,
                                                    u32 start_index, u32 end_index)
{
    u32 i;

    for (i = start_index; i < end_index; i++) {
        ka_task_mutex_lock(&event_ns_stu->event_process[i].process_mutex);
        if (event_ns_stu->event_process[i].process_status != DMS_EVENT_PROCESS_STATUS_IDLE) {
            ka_task_mutex_unlock(&event_ns_stu->event_process[i].process_mutex);
            continue;
        }
        if (ka_base_kfifo_alloc(&event_ns_stu->event_process[i].event_fifo, DMS_EXCEPTION_KFIFO_SIZE,
                                KA_GFP_KERNEL | __KA_GFP_ACCOUNT) != 0) {
            ka_task_mutex_unlock(&event_ns_stu->event_process[i].process_mutex);
            dms_err("ka_base_kfifo_alloc failed. (ns_id=%u; tgid=%d; pid=%d; index=%u)\n", event_ns_stu->ns_id,
                    pid_info.tgid, pid_info.pid, i);
            return NULL;
        }
        ka_base_kfifo_reset(&event_ns_stu->event_process[i].event_fifo);
        event_ns_stu->event_process[i].exception_num = 0;
        event_ns_stu->event_process[i].process_pid = pid_info.tgid;
        event_ns_stu->event_process[i].process_tid = pid_info.pid;
        event_ns_stu->event_process[i].start_time = ka_task_get_current_starttime();
        event_ns_stu->event_process[i].process_status = DMS_EVENT_PROCESS_STATUS_WORK;
        ka_task_mutex_unlock(&event_ns_stu->event_process[i].process_mutex);
        event_ns_stu->process_num++;
        increase_process_max_idx(event_ns_stu, i);

        dms_debug("Get a process success. (ns_id=%u; tgid=%d; pid=%d; index=%u)\n", event_ns_stu->ns_id, pid_info.tgid,
                  pid_info.pid, i);
        return &event_ns_stu->event_process[i];
    }
    return NULL;
}

STATIC bool get_pid_start_time(u32 pid, u64 *start_time)
{
#if !defined(UT_VCAST) && !defined(DMS_UT)
    ka_struct_pid_t *pid_struct;
    ka_task_struct_t *task = NULL;

    if (run_in_normal_docker()) {
        pid_struct = ka_task_find_get_pid(pid);
        if (pid_struct == NULL) {
            return false;
        }

        ka_task_rcu_read_lock();
        task = pid_task(pid_struct, KA_PIDTYPE_PID);
        if (task == NULL) {
            ka_task_rcu_read_unlock();
            ka_task_put_pid(pid_struct);
            return false;
        }
    } else {
        task = task_get_by_tgid(pid);
        if (task == NULL) {
            return false;
        }
    }

    *start_time = task->start_time;
    if (run_in_normal_docker()) {
        ka_task_rcu_read_unlock();
        ka_task_put_pid(pid_struct);
    } else {
        ka_task_put_task_struct(task);
    }
#endif
    return true;
}

STATIC void check_and_release_event_procs(DMS_EVENT_STRU_T *event_ns_stu)
{
    u32 i;
    ka_pid_t pid, tgid;
    u64 record_start_time = 0, pid_start_time = 0, tgid_start_time = 0;
    u8 process_status;

    if (event_ns_stu == NULL) {
        return;
    }

    for (i = 0; i < DMS_EVENT_EXCEPTION_PROCESS_MAX; i++) {
        if (i < DMS_EVENT_EXCEPTION_PROCESS_DSMI_END && i > event_ns_stu->process_max_idx_dsmi) {
            continue;
        } else if (i > event_ns_stu->process_max_idx_hal) {
            break;
        }

        tgid = -1;
        pid = -1;
        process_status = event_ns_stu->event_process[i].process_status;
        if (process_status == DMS_EVENT_PROCESS_STATUS_IDLE) {
            continue;
        }

        if (run_in_normal_docker()) {
            if ((dbl_host_pid_to_container_pid(event_ns_stu->event_process[i].process_pid, &tgid) != 0) ||
                (dbl_host_pid_to_container_pid(event_ns_stu->event_process[i].process_tid, &pid) != 0)) {
                dms_debug("Get container pid failed. (tgid=%d; pid=%d)\n", event_ns_stu->event_process[i].process_pid,
                          event_ns_stu->event_process[i].process_tid);
            }
        } else {
            tgid = event_ns_stu->event_process[i].process_pid;
            pid = event_ns_stu->event_process[i].process_tid;
        }
        pid_start_time = 0;
        record_start_time = event_ns_stu->event_process[i].start_time;

        if (get_pid_start_time(tgid, &tgid_start_time) && get_pid_start_time(pid, &pid_start_time) &&
            pid_start_time == record_start_time) {
            continue;
        } else {
            dms_debug("Release one process. (ns_id=%d; process_pid=%d; tgid=%d; process_tid=%d; pid=%d; "
                      "record_start_time=%llu; current_time=%llu; index=%u)\n",
                      event_ns_stu->ns_id, event_ns_stu->event_process[i].process_pid, tgid,
                      event_ns_stu->event_process[i].process_tid, pid, record_start_time, pid_start_time, i);
            release_one_event_proc(event_ns_stu, i);
        }
    }
    return;
}

static void get_old_event_proc(DMS_EVENT_STRU_T *event_ns_stu, struct pid_info_t pid_info, u32 start_index,
                               u32 end_index, DMS_EVENT_PROCESS_STRU **event_proc)
{
    u32 i;

    for (i = start_index; i < end_index; i++) {
        if (i < DMS_EVENT_EXCEPTION_PROCESS_DSMI_END && i > event_ns_stu->process_max_idx_dsmi) {
            continue;
        } else if (i > event_ns_stu->process_max_idx_hal) {
            break;
        }

        if (event_ns_stu->event_process[i].process_pid != pid_info.tgid ||
            event_ns_stu->event_process[i].process_tid != pid_info.pid) {
            continue;
        }

        event_ns_stu->event_process[i].process_status = DMS_EVENT_PROCESS_STATUS_WORK;
        *event_proc = &event_ns_stu->event_process[i];
        return;
    }
    *event_proc = NULL;
}

static int dms_get_cur_ns_id(u32 *ns_id)
{
#ifdef CFG_FEATURE_NOT_SUPPORT_NS_ID
    *ns_id = 0;
#else
    int ret;

    if (run_in_normal_docker()) {
#ifdef CFG_FEATURE_NOT_SUPPORT_UDA_DEVID_TRANS
        ret = smf_get_container_ns_id(ns_id);
        if (ret != 0) {
            return DRV_ERROR_INNER_ERR;
        }
#else
        ret = uda_get_cur_ns_id(ns_id);
        if (ret != 0) {
            return DRV_ERROR_INNER_ERR;
        }
#endif // CFG_FEATURE_NOT_SUPPORT_UDA_DEVID_TRANS
    } else {
        *ns_id = UDA_NS_NUM;
    }

#endif // CFG_FEATURE_NOT_SUPPORT_NS_ID
    return 0;
}

static DMS_EVENT_STRU_T *dms_event_ns_stu_create(u32 ns_id)
{
    int i;
    size_t udevids_size;

    g_dms_event_ns_stu[ns_id] = dbl_vzalloc(sizeof(DMS_EVENT_STRU_T));
    if (g_dms_event_ns_stu[ns_id] == NULL) {
        dms_err("Call ka_mm_vzalloc failed. (ns_id=%u)\n", ns_id);
        return NULL;
    }

    g_dms_event_ns_stu[ns_id]->process_num = 0;
    g_dms_event_ns_stu[ns_id]->process_max_idx_dsmi = DMS_EVENT_EXCEPTION_PROCESS_DSMI_START;
    g_dms_event_ns_stu[ns_id]->process_max_idx_hal = DMS_EVENT_EXCEPTION_PROCESS_HAL_START;
    g_dms_event_ns_stu[ns_id]->ns_id = ns_id;
    for (i = 0; i < DMS_EVENT_EXCEPTION_PROCESS_MAX; i++) {
        ka_task_mutex_init(&g_dms_event_ns_stu[ns_id]->event_process[i].process_mutex);
        ka_task_mutex_lock(&g_dms_event_ns_stu[ns_id]->event_process[i].process_mutex);
        ka_task_init_waitqueue_head(&g_dms_event_ns_stu[ns_id]->event_process[i].event_wait);
        g_dms_event_ns_stu[ns_id]->event_process[i].exception_num = 0;
        g_dms_event_ns_stu[ns_id]->event_process[i].process_pid = DMS_INVALID_PID;
        g_dms_event_ns_stu[ns_id]->event_process[i].process_tid = DMS_INVALID_PID;
        g_dms_event_ns_stu[ns_id]->event_process[i].start_time = 0;
        g_dms_event_ns_stu[ns_id]->event_process[i].process_status = DMS_EVENT_PROCESS_STATUS_IDLE;
        g_dms_event_ns_stu[ns_id]->event_process[i].subscribe_udevids_num = 0;
        udevids_size = sizeof(u32) * ASCEND_PDEV_MAX_NUM;
        (void)memset_s(&(g_dms_event_ns_stu[ns_id]->event_process[i].subscribe_udevids), udevids_size, 0, udevids_size);
        (void)memset_s(&(g_dms_event_ns_stu[ns_id]->event_process[i].filter), sizeof(struct dms_event_filter), 0,
                       sizeof(struct dms_event_filter));
        g_dms_event_ns_stu[ns_id]->event_process[i].cmd_src = INVALID_SOURCE;
        ka_task_mutex_unlock(&g_dms_event_ns_stu[ns_id]->event_process[i].process_mutex);
    }

    return g_dms_event_ns_stu[ns_id];
}

static int get_or_create_event_ns_stu(DMS_EVENT_STRU_T **event_ns_stu)
{
    u32 ns_id = UDA_NS_NUM + 1;
    int ret;

    ret = dms_get_cur_ns_id(&ns_id);
    if (ret != 0 || ns_id > UDA_NS_NUM) {
        dms_err("Get ns id failed. (ns_id=%u; ret=%d)\n", ns_id, ret);
        return ret;
    }

    if (g_dms_event_ns_stu[ns_id] == NULL) {
        *event_ns_stu = dms_event_ns_stu_create(ns_id);
        if (*event_ns_stu == NULL) {
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    } else {
        if (ka_feature_is_enable()) {
            check_and_release_event_procs(g_dms_event_ns_stu[ns_id]);
        }
        *event_ns_stu = g_dms_event_ns_stu[ns_id];
    }

    return 0;
}

STATIC int create_udevid_list(int dev_id, u32 *subscribe_udevids, u32 *subscribe_udevids_num)
{
    u32 udevid;
    int ret;

    if (dev_id == DMS_FAULT_DEVICE_ALL) {
        *subscribe_udevids_num = (uda_get_cur_ns_dev_num() > ASCEND_PDEV_MAX_NUM) ? ASCEND_PDEV_MAX_NUM :
                                                                                    uda_get_cur_ns_dev_num();
        if (*subscribe_udevids_num == 0) {
            dms_err("Current namespace udevice list is null.\n");
            return DRV_ERROR_INNER_ERR;
        }
        ret = uda_get_cur_ns_udevids(subscribe_udevids, *subscribe_udevids_num);
        if (ret != 0) {
            dms_err("Get cur ns udevids failed. (ret=%d)\n", ret);
            return ret;
        }
    } else {
        ret = uda_devid_to_udevid((u32)dev_id, &udevid);
        if (ret != 0) {
            dms_err("Get udevid failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }
        *subscribe_udevids_num = 1;
        subscribe_udevids[0] = udevid;
    }
    return 0;
}

static int set_event_proc_filter(DMS_EVENT_PROCESS_STRU *event_proc, u32 *udevids, u32 dev_num,
                                 struct dms_event_filter *filter, enum cmd_source cmd_src)
{
    int ret;

    ka_task_mutex_lock(&event_proc->process_mutex);
    ret = memcpy_s(event_proc->subscribe_udevids, sizeof(u32) * ASCEND_PDEV_MAX_NUM, udevids, sizeof(u32) * dev_num);
    if (ret != 0) {
        ka_task_mutex_unlock(&event_proc->process_mutex);
        dms_err("memcpy_s udevid list failed. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    event_proc->subscribe_udevids_num = dev_num;
    event_proc->filter = *filter;
    event_proc->cmd_src = cmd_src;
    ka_task_mutex_unlock(&event_proc->process_mutex);
    return 0;
}

static int get_current_ns_event_proc(int dev_id, struct dms_event_filter *filter, struct pid_info_t pid_info,
                                     enum cmd_source cmd_src, DMS_EVENT_PROCESS_STRU **event_proc)
{
    int ret;
    u32 start_index = 0, end_index = 0, dev_num = 0;
    DMS_EVENT_STRU_T *event_ns_stu = NULL;
    u32 udevids[ASCEND_PDEV_MAX_NUM] = {0};

    *event_proc = NULL;
    ret = get_process_index_range(cmd_src, &start_index, &end_index);
    if (ret != 0 || start_index >= DMS_EVENT_EXCEPTION_PROCESS_MAX || end_index > DMS_EVENT_EXCEPTION_PROCESS_MAX) {
        dms_err("Invalid para. (pid=%d; cmd_src=%d; start_index=%u; end_index=%u; max_index=%d; ret=%d)\n",
                pid_info.tgid, cmd_src, start_index, end_index, DMS_EVENT_EXCEPTION_PROCESS_MAX, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    if (cmd_src != FROM_KERNEL) {
        ret = create_udevid_list(dev_id, udevids, &dev_num);
        if (ret != 0) {
            dms_err("Create udevid list failed. (ret=%d)\n", ret);
            return ret;
        }
    }

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    ret = get_or_create_event_ns_stu(&event_ns_stu);
    if (ret != 0) {
        ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
        dms_err("Get current event ns stu failed. (ret=%d)\n", ret);
        return ret;
    }

    get_old_event_proc(event_ns_stu, pid_info, start_index, end_index, event_proc);

    /* this thread has already registered before, return the old event proc directly */
    if (*event_proc != NULL) {
        ret = set_event_proc_filter(*event_proc, udevids, dev_num, filter, cmd_src);
        ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
        if (ret != 0) {
            dms_err("Set event proc filter failed. (ret=%d)\n", ret);
        }
        return ret;
    }

    /* this is a new thread, try to alloc a new event_proc */
    *event_proc = alloc_new_event_proc(event_ns_stu, pid_info, start_index, end_index);
    if (*event_proc != NULL) {
        ret = set_event_proc_filter(*event_proc, udevids, dev_num, filter, cmd_src);
        ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
        if (ret != 0) {
            dms_err("Set event proc filter failed. (ret=%d)\n", ret);
        }
        return ret;
    } else {
        ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
        dms_err("Process list is full. (tgid=%d)\n", pid_info.tgid);
        return DRV_ERROR_RESOURCE_OCCUPIED;
    }
}

static int get_event_para_from_event_proc(DMS_EVENT_PROCESS_STRU *event_proc, struct dms_event_para *fault_event)
{
    DMS_EVENT_NODE_STRU exception_node = {{0}, {0}};
    int ret = DRV_ERROR_NONE;

    ka_task_mutex_lock(&event_proc->process_mutex);
    if (ka_base_kfifo_len(&event_proc->event_fifo) < DMS_EXCEPTION_KFIFO_CELL) {
        dms_warn("The event proc does not have event. (tgid=%d; pid=%d)\n", event_proc->process_pid,
                 event_proc->process_tid);
        ret = -ETIMEDOUT;
        goto out;
    }

    if (!ka_base_kfifo_out(&event_proc->event_fifo, &exception_node, DMS_EXCEPTION_KFIFO_CELL)) {
        dms_err("ka_base_kfifo_out failed. (tgid=%d; pid=%d)\n", event_proc->process_pid, event_proc->process_tid);
        ret = DRV_ERROR_INNER_ERR;
        goto out;
    }

    if (memcpy_s(fault_event, sizeof(struct dms_event_para), &exception_node.event, sizeof(struct dms_event_para)) !=
        0) {
        dms_err("Call memcpy_s failed. (tgid=%d; pid=%d)\n", event_proc->process_pid, event_proc->process_tid);
        ret = DRV_ERROR_INNER_ERR;
        goto out;
    }

out:
    event_proc->exception_num = ka_base_kfifo_len(&event_proc->event_fifo) / DMS_EXCEPTION_KFIFO_CELL;
    ka_task_mutex_unlock(&event_proc->process_mutex);
    return ret;
}

STATIC int dms_event_clear_exception_by_devid(DMS_EVENT_PROCESS_STRU *event_proc, u32 devid)
{
    DMS_EVENT_NODE_STRU exception_node = {{0}, {0}};
    ka_kfifo_t fifo_buf;

    ka_task_mutex_lock(&event_proc->process_mutex);
    if (event_proc->process_status != DMS_EVENT_PROCESS_STATUS_WORK) {
        ka_task_mutex_unlock(&event_proc->process_mutex);
        return DRV_ERROR_NONE;
    }

    if (ka_base_kfifo_is_empty(&event_proc->event_fifo) != 0) {
        ka_task_mutex_unlock(&event_proc->process_mutex);
        return DRV_ERROR_NONE;
    }

    if (ka_base_kfifo_alloc(&fifo_buf, DMS_EXCEPTION_KFIFO_SIZE, KA_GFP_KERNEL) != 0) {
        ka_task_mutex_unlock(&event_proc->process_mutex);
        dms_err("ka_base_kfifo_alloc failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    ka_base_kfifo_reset(&fifo_buf);

    while (ka_base_kfifo_len(&event_proc->event_fifo) >= DMS_EXCEPTION_KFIFO_CELL) {
        if (!ka_base_kfifo_out(&event_proc->event_fifo, &exception_node, DMS_EXCEPTION_KFIFO_CELL)) {
            dms_warn("ka_base_kfifo_out from event_proc warn.\n");
            continue;
        }
        if (exception_node.event.deviceid != (unsigned short)devid) {
            ka_base_kfifo_in(&fifo_buf, &exception_node, DMS_EXCEPTION_KFIFO_CELL);
        }
    }
    ka_base_kfifo_reset(&event_proc->event_fifo);

    while (ka_base_kfifo_len(&fifo_buf) >= DMS_EXCEPTION_KFIFO_CELL) {
        if (!ka_base_kfifo_out(&fifo_buf, &exception_node, DMS_EXCEPTION_KFIFO_CELL)) {
            dms_warn("ka_base_kfifo_out from fifo_buf warn.\n");
            continue;
        }
        ka_base_kfifo_in(&event_proc->event_fifo, &exception_node, DMS_EXCEPTION_KFIFO_CELL);
    }
    event_proc->exception_num = ka_base_kfifo_len(&event_proc->event_fifo) / DMS_EXCEPTION_KFIFO_CELL;
    ka_task_mutex_unlock(&event_proc->process_mutex);
    ka_base_kfifo_free(&fifo_buf);
    return DRV_ERROR_NONE;
}

/* return > 0 means success, return 0 means no event, return < 0 means error */
static int dms_event_wait_interrupt(int timeout, DMS_EVENT_PROCESS_STRU *event_proc)
{
    int ret;

    if (timeout == DMS_EVENT_NO_TIMEOUT_FLAG) {
        do {
            ret = ka_task_wait_event_interruptible_timeout(event_proc->event_wait, (event_proc->exception_num > 0),
                                                           ka_system_msecs_to_jiffies(DMS_EVENT_WAIT_TIME));
        } while (ret == 0);
    } else if ((timeout >= 0) && (timeout <= DMS_EVENT_WAIT_TIME_MAX)) {
        ret = ka_task_wait_event_interruptible_timeout(event_proc->event_wait, (event_proc->exception_num > 0),
                                                       ka_system_msecs_to_jiffies((unsigned int)timeout));
    } else {
        dms_err("Invalid parameter. (timeout=%dms)\n", timeout);
        return -EINVAL;
    }

    return ret;
}

int dms_event_get_exception(int dev_id, struct dms_event_filter *filter, struct dms_event_para *fault_event,
                            int timeout, enum cmd_source cmd_src)
{
    DMS_EVENT_PROCESS_STRU *event_proc = NULL;
    struct pid_info_t pid_info;
    int ret;

    if (fault_event == NULL) {
        dms_err("Invalid parameter, fault_event is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    pid_info.tgid = ka_task_get_current_tgid();
    pid_info.pid = ka_task_get_current_pid();
    ret = get_current_ns_event_proc(dev_id, filter, pid_info, cmd_src, &event_proc);
    if (ret != 0 || event_proc == NULL) {
        dms_err("Alloc an event_proc failed. (tgid=%d; pid=%d; cmd_src=%d)\n", ka_task_get_current_tgid(),
                ka_task_get_current_pid(), cmd_src);
        return ret;
    }

    ret = dms_event_wait_interrupt(timeout, event_proc);
    if (ret < 0) {
        if (ret == -EINVAL) {
            ret = DRV_ERROR_PARA_ERROR;
        }
        return ret;
    } else if (ret == 0) {
        return -ETIMEDOUT;
    }

    ret = get_event_para_from_event_proc(event_proc, fault_event);
    if (ret == -ETIMEDOUT) {
        return ret;
    } else if (ret != 0) {
        dms_err("Get exception by process index failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
KA_EXPORT_SYMBOL(dms_event_get_exception);

int dms_event_clear_exception(u32 devid)
{
    u32 ns_id;
    u32 idx;
    int ret;

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    for (ns_id = 0; ns_id <= UDA_NS_NUM; ns_id++) {
        if (g_dms_event_ns_stu[ns_id] == NULL) {
            continue;
        }

        for (idx = 0; idx < DMS_EVENT_EXCEPTION_PROCESS_MAX; idx++) {
            ret = dms_event_clear_exception_by_devid(&g_dms_event_ns_stu[ns_id]->event_process[idx], devid);
            if (ret != 0) {
                ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
                return ret;
            }
        }
    }
    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);

    return DRV_ERROR_NONE;
}

STATIC void decrease_process_max_idx(DMS_EVENT_STRU_T *event_ns_stu, u32 idx)
{
    u32 i = 0;

    if (idx >= DMS_EVENT_EXCEPTION_PROCESS_DSMI_START && idx < DMS_EVENT_EXCEPTION_PROCESS_DSMI_END) {
        if (idx == event_ns_stu->process_max_idx_dsmi) {
            for (i = idx; i > DMS_EVENT_EXCEPTION_PROCESS_DSMI_START &&
                          event_ns_stu->event_process[i].process_status == DMS_EVENT_PROCESS_STATUS_IDLE;
                 i--) {
                event_ns_stu->process_max_idx_dsmi--;
            }
        }
    } else if (idx >= DMS_EVENT_EXCEPTION_PROCESS_HAL_START && idx < DMS_EVENT_EXCEPTION_PROCESS_HAL_END) {
        if (idx == event_ns_stu->process_max_idx_hal) {
            for (i = idx; i > DMS_EVENT_EXCEPTION_PROCESS_HAL_START &&
                          event_ns_stu->event_process[i].process_status == DMS_EVENT_PROCESS_STATUS_IDLE;
                 i--) {
                event_ns_stu->process_max_idx_hal--;
            }
        }
    }
}

STATIC void release_one_event_proc(DMS_EVENT_STRU_T *event_ns_stu, u32 proc_idx)
{
    size_t udevids_size;

    ka_task_mutex_lock(&event_ns_stu->event_process[proc_idx].process_mutex);
    if (event_ns_stu->event_process[proc_idx].process_status != DMS_EVENT_PROCESS_STATUS_WORK) {
        ka_task_mutex_unlock(&event_ns_stu->event_process[proc_idx].process_mutex);
        return;
    }

    ka_base_kfifo_free(&event_ns_stu->event_process[proc_idx].event_fifo);
    event_ns_stu->event_process[proc_idx].exception_num = 0;
    event_ns_stu->event_process[proc_idx].process_pid = DMS_INVALID_PID;
    event_ns_stu->event_process[proc_idx].process_tid = DMS_INVALID_PID;
    event_ns_stu->event_process[proc_idx].start_time = 0;
    event_ns_stu->event_process[proc_idx].process_status = DMS_EVENT_PROCESS_STATUS_IDLE;
    event_ns_stu->event_process[proc_idx].subscribe_udevids_num = 0;
    udevids_size = sizeof(u32) * ASCEND_PDEV_MAX_NUM;
    (void)memset_s(&(event_ns_stu->event_process[proc_idx].subscribe_udevids), udevids_size, 0, udevids_size);
    (void)memset_s(&(event_ns_stu->event_process[proc_idx].filter), sizeof(struct dms_event_filter), 0,
                   sizeof(struct dms_event_filter));
    event_ns_stu->event_process[proc_idx].cmd_src = INVALID_SOURCE;
    ka_task_mutex_unlock(&event_ns_stu->event_process[proc_idx].process_mutex);

    event_ns_stu->process_num--;
    decrease_process_max_idx(event_ns_stu, proc_idx);

    return;
}

void dms_event_release_proc(int tgid, int pid)
{
    int ret;
    u32 proc_idx;
    u32 ns_id = UDA_NS_NUM + 1;
    DMS_EVENT_STRU_T *event_ns_stu = NULL;

    ret = dms_get_cur_ns_id(&ns_id);
    if ((ret != 0) || (ns_id > UDA_NS_NUM)) {
        /* can not printf error log, because this function may be called by a no-ns_id thread */
        return;
    }

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);
    event_ns_stu = g_dms_event_ns_stu[ns_id];
    if (event_ns_stu == NULL) {
        ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
        return;
    }

    for (proc_idx = 0; proc_idx < DMS_EVENT_EXCEPTION_PROCESS_MAX; proc_idx++) {
        if ((event_ns_stu->event_process[proc_idx].process_pid == tgid) &&
            (event_ns_stu->event_process[proc_idx].process_tid == pid)) {
            release_one_event_proc(event_ns_stu, proc_idx);
            if (event_ns_stu->process_num == 0) {
                dms_event_ns_stu_free(ns_id);
                event_ns_stu = NULL;
            }
            ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
            dms_debug("Release one process. (ns_id=%d; tgid=%d; pid=%d; index=%u)\n", ns_id, ka_task_get_current_tgid(),
                      ka_task_get_current_pid(), proc_idx);
            return;
        }
    }
    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
}
KA_EXPORT_SYMBOL(dms_event_release_proc);

int dms_event_subscribe_register(DMS_EVENT_DISTRIBUTE_HANDLE_T handle_func, DMS_DISTRIBUTE_PRIORITY priority)
{
    u32 i;

    if ((priority >= DMS_DISTRIBUTE_PRIORITY_MAX) || (handle_func == NULL)) {
        dms_err("Invalid parameter. (priority=%u; handle_func=%s).\n", priority, (handle_func == NULL) ? "NULL" : "OK");
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = DISTRIBUTE_FUNC_HEAD_INDEX(priority); i < DISTRIBUTE_FUNC_TAIL_INDEX(priority); i++) {
        if (g_dms_event_handle_list[i] == NULL) {
            g_dms_event_handle_list[i] = handle_func;
            ka_task_mutex_unlock(&g_dms_event_handle_mutex);
            return DRV_ERROR_NONE;
        }
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);

    dms_err("The distribute handle list is full. (priority=%u).\n", priority);
    return DRV_ERROR_NO_RESOURCES;
}
KA_EXPORT_SYMBOL(dms_event_subscribe_register);

void dms_event_subscribe_unregister(DMS_EVENT_DISTRIBUTE_HANDLE_T handle_func)
{
    u32 i;

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = 0; i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        if (g_dms_event_handle_list[i] == handle_func) {
            g_dms_event_handle_list[i] = NULL;
            break;
        }
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);
}
KA_EXPORT_SYMBOL(dms_event_subscribe_unregister);

void dms_event_subscribe_unregister_all(void)
{
    u32 i;

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = 0; i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        g_dms_event_handle_list[i] = NULL;
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);
}

int dms_event_distribute_handle(DMS_EVENT_NODE_STRU *exception_node, DMS_DISTRIBUTE_PRIORITY priority)
{
    int ret;
    u32 i;

    if ((priority >= DMS_DISTRIBUTE_PRIORITY_MAX) || (exception_node == NULL)) {
        dms_err("Invalid parameter. (priority=%u; exception=%s).\n", priority,
                (exception_node == NULL) ? "NULL" : "OK");
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_mutex_lock(&g_dms_event_handle_mutex);
    for (i = DISTRIBUTE_FUNC_HEAD_INDEX(priority); i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        if (g_dms_event_handle_list[i] != NULL) {
            ret = g_dms_event_handle_list[i](exception_node);
            if (ret != 0) {
                dms_err("Distribute event failed. (handle_index=%u; ret=%d)\n", i, ret);
                ka_task_mutex_unlock(&g_dms_event_handle_mutex);
                return ret;
            }
        }
    }
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);

    return DRV_ERROR_NONE;
}
KA_EXPORT_SYMBOL(dms_event_distribute_handle);

ssize_t dms_event_print_subscribe_handle(char *str)
{
    int len, avl_len = EVENT_DFX_BUF_SIZE_MAX;
    char *refill_buf = str;
    u32 i;

    len = snprintf_s(str, avl_len, avl_len - 1, "Print subscribe handle function begin:\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
    str += len;
    avl_len -= len;

    ka_task_mutex_lock(&g_dms_event_handle_mutex);

    for (i = 0; i < DMS_EVENT_DISTRIBUTE_FUNC_MAX; i++) {
        if (g_dms_event_handle_list[i] != NULL) {
            len = snprintf_s(str, avl_len, avl_len - 1, "event handle[%u]=ok\n", i);
            EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
            str += len;
            avl_len -= len;
        }
    }

    len = snprintf_s(str, avl_len, avl_len - 1, "Print subscribe handle function end.\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
    str += len;

    ka_task_mutex_unlock(&g_dms_event_handle_mutex);
    return str - refill_buf;
out:
    ka_task_mutex_unlock(&g_dms_event_handle_mutex);
    dms_warn("snprintf_s warn.\n");
    return 0;
}

ssize_t dms_event_print_subscribe_process(char *str)
{
    int len, avl_len = EVENT_DFX_BUF_SIZE_MAX;
    char *refill_buf = str;
    u32 ns_id;
    u32 idx;
    DMS_EVENT_PROCESS_STRU *event_proc = NULL;

    len = snprintf_s(str, avl_len, avl_len - 1, "Print subscribe process begin:\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, return 0);
    str += len;
    avl_len -= len;

    ka_task_mutex_lock(&g_dms_event_ns_node_mutex);

    for (ns_id = 0; ns_id <= UDA_NS_NUM; ns_id++) {
        if (g_dms_event_ns_stu[ns_id] == NULL) {
            continue;
        }
        for (idx = 0; idx < DMS_EVENT_EXCEPTION_PROCESS_MAX; idx++) {
            event_proc = &g_dms_event_ns_stu[ns_id]->event_process[idx];
            ka_task_mutex_lock(&event_proc->process_mutex);
            if (event_proc->process_status != DMS_EVENT_PROCESS_STATUS_WORK) {
                ka_task_mutex_unlock(&event_proc->process_mutex);
                continue;
            }
            len = snprintf_s(str, avl_len, avl_len - 1, "ns_id[%u]; proc_id[%u]: tgid=%d; pid=%d; event_num=%u\n",
                             ns_id, idx, event_proc->process_pid, event_proc->process_tid, event_proc->exception_num);
            ka_task_mutex_unlock(&event_proc->process_mutex);
            EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
            str += len;
            avl_len -= len;
        }
    }

    len = snprintf_s(str, avl_len, avl_len - 1, "Print subscribe process end.\n");
    EVENT_DFX_CHECK_DO_SOMETHING(len < 0, goto out);
    str += len;

    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
    return str - refill_buf;
out:
    ka_task_mutex_unlock(&g_dms_event_ns_node_mutex);
    dms_warn("snprintf_s warn.\n");
    return 0;
}
