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

#include "securec.h"

#include "esched.h"
#include "esched_feature_check.h"
#include "esched_drv_adapt.h"
#include "esched_fops.h"
#include "topic_sched_drv.h"

#include "tsdrv_interface.h"
#include "trs_chan.h"
#include "comm_kernel_interface.h"
#include "hwts_task_info.h"
#include "topic_sched_drv_common.h"
#include "esched_adapt_platform.h"

static bool esched_drv_is_used_host_pid(u32 event_id, u32 topic_type)
{
    if (event_id == EVENT_SPLIT_KERNEL) {
#ifndef EMU_ST
        return false;
#endif
    }

    /* For AICPU or DATACPU tasks, [Host CTRLCPU PID] is used.
       For tasks with other types of CPUs, fills the dest_pid directly. */
    if ((topic_type == TOPIC_TYPE_AICPU_DEVICE_ONLY) || (topic_type == TOPIC_TYPE_AICPU_DEVICE_FIRST) ||
        (topic_type == TOPIC_TYPE_AICPU_HOST_ONLY) || (topic_type == TOPIC_TYPE_AICPU_HOST_FIRST) ||
        (topic_type == TOPIC_TYPE_DCPU_DEVICE)) {
        return true;
    }

    return false;
}

static inline bool esched_is_pid_hw_mapped(u32 chip_id)
{
    if (esched_is_phy_dev(chip_id) == false) {
        return false;
    } else {
        if (!esched_adapt_has_sia_map_topic_table(chip_id)) {
            return false;
        }
    }

    return true;
}

STATIC int esched_set_sqe_pid(u32 chip_id, u32 target_pid, struct topic_sched_sqe *sqe)
{
    if ((esched_is_pid_hw_mapped(chip_id) == false) || ((target_pid & PID_MAP_MASK) != 0)) {
        sqe->pid = esched_get_hw_vfid_from_devid(chip_id);
        /* pid store in rt_streamid and task_id */
        if (memcpy_s(&sqe->rt_streamid, sizeof(target_pid), &target_pid, sizeof(target_pid)) != 0) {
#ifndef EMU_ST
            sched_err("Failed to memcpy_s pid to rt_streamid. (dev_id=%u; pid=%d)\n", chip_id, target_pid);
            return DRV_ERROR_INNER_ERR;
#endif
        }
    } else {
        sqe->pid = target_pid;
    }

    return 0;
}

int esched_mb_user_data_subtract(struct topic_sched_mailbox *mb, u16 size, u32 *value)
{
    u16 i;

    if (mb->user_data_len < size) {
#ifndef EMU_ST
        sched_err("No more space. (topic_id=%u; subtopic_id=%u; pid=%d; user_data_len=%d; size=%u)\n", mb->topic_id,
                  mb->subtopic_id, mb->pid, mb->user_data_len, size);
        return -EFAULT;
#endif
    }

    mb->user_data_len -= size;

    *value = 0;
    for (i = 0; i < size; i++) {
        *value |= (u32)(((u8 *)mb->user_data)[mb->user_data_len + i]) << (i * 8); /* Shift 8 bit per Byte */
    }

    return 0;
}

int esched_restore_mb_user_data(struct topic_sched_mailbox *mb, u32 *dst_devid, u32 *tid, u32 *pid)
{
    int ret;

    if (mb->usr_pid_flag == 1) {
        ret = esched_adapt_restore_mb_usr_pid(mb, pid);
        if (ret != 0) {
            return ret;
        }
    }

    if (mb->tid_flag == 1) {
        mb->tid_flag = 0;
        ret = esched_mb_user_data_subtract(mb, (u16)sizeof(u16), tid);
        if (ret != 0) {
#ifndef EMU_ST
            return ret;
#endif
        }
    }

    if (mb->devid_flag == 1) {
#ifndef EMU_ST
        mb->devid_flag = 0;
        ret = esched_mb_user_data_subtract(mb, (u16)sizeof(u16), dst_devid);
        if (ret != 0) {
            return ret;
        }
#endif
    }

    return 0;
}

int esched_drv_map_host_dev_pid(struct sched_proc_ctx *proc_ctx, u32 identity)
{
    devdrv_host_pids_info_t pids_info = {0};

    if (proc_ctx->host_pid != 0) {
#ifndef EMU_ST
        return 0;
#endif
    }

    (void)devdrv_query_process_host_pids_by_pid(proc_ctx->pid, &pids_info);
    if (pids_info.vaild_num == 0) {
#ifdef CFG_FEATURE_NO_BIND_SCHED
        sched_info("Query host pid of process no bind. (pid=%d)\n", proc_ctx->pid);
        return 0;
#else
#ifndef CFG_ENV_HOST
        sched_warn("Can not query host pid of process. (pid=%d)\n", proc_ctx->pid);
#endif
        return DRV_ERROR_NO_PROCESS;
#endif
    }
    proc_ctx->host_pid = pids_info.host_pids[0];
    proc_ctx->cp_type = pids_info.cp_type[0];

    sched_info("Show details. (pid=%d; dev_id=%u; vfid=%u; cp_type=%d; host_pid=%d)\n", proc_ctx->pid,
               pids_info.chip_id[0], pids_info.vfid[0], (int)(proc_ctx->cp_type), proc_ctx->host_pid);

    if (identity != 0) {
#ifndef EMU_ST
        return DRV_ERROR_NONE;
#endif
    }

    if (proc_ctx->cp_type == DEVDRV_PROCESS_DEV_ONLY) {
#ifndef EMU_ST
        return 0;
#endif
    }

    return esched_drv_config_pid(proc_ctx, identity, &pids_info);
}

void esched_drv_unmap_host_dev_pid(struct sched_proc_ctx *proc_ctx, u32 identity)
{
    if ((proc_ctx->host_pid == 0) || (proc_ctx->cp_type == DEVDRV_PROCESS_DEV_ONLY)) {
#ifndef EMU_ST
        return;
#endif
    }

    if (identity != 0) {
#ifndef EMU_ST
        return;
#endif
    }

    esched_drv_del_pid(proc_ctx, identity);
}

int esched_drv_fill_sqe(u32 chip_id, u32 event_src, struct topic_sched_sqe *sqe,
                        struct sched_published_event_info *event_info, bool wait_thread_check)
{
    u32 host_pid, target_pid;
    int ret;

    sqe->type = TOPIC_SCHED_SQE_TYPE;
    sqe->wr_cqe = 0;
    sqe->blk_dim = 1;

    if (event_info->event_id == EVENT_SPLIT_KERNEL) {
        sqe->block_id = 1; /* split task only, no use. */
    }
    sqe->kernel_type = TOPIC_SCHED_DEFAULT_KERNEL_TYPE;
    sqe->batch_mode = 0; /* no use. */

    sqe->topic_type = esched_drv_get_topic_type(event_info->policy, event_info->dst_engine);
    if (esched_adapt_is_soc_miniv3(chip_id) &&
        ((sqe->topic_type == TOPIC_TYPE_AICPU_HOST_ONLY) || (sqe->topic_type == TOPIC_TYPE_AICPU_HOST_FIRST))) {
        sched_err("topic type is invalid. (pid=%d; gid=%u; eventid=%u; policy=%u; topic_type=%u)\n", event_info->pid,
                  event_info->gid, event_info->event_id, event_info->policy, event_info->dst_engine);
        return DRV_ERROR_NOT_SUPPORT;
    }
    sqe->kernel_credit = TOPIC_SCHED_SQE_KERNEL_CREDIT;

    sqe->subtopic_id = event_info->subevent_id;
    sqe->topic_id = event_info->event_id;
    sqe->gid = event_info->gid;
    sqe->user_data_len = event_info->msg_len;
    sqe->usr_pid_flag = 0;
    sqe->devid_flag = 0;
    sqe->tid_flag = 0;

    ret = esched_drv_fill_sqe_qos(chip_id, event_info, sqe, wait_thread_check);
    if (ret != 0) {
        sched_err("Failed to fill qos. (pid=%d; gid=%u; eventid=%u; ret=%d)\n", event_info->pid, event_info->gid,
                  event_info->event_id, ret);
        return ret;
    }

    if ((esched_drv_is_used_host_pid(event_info->event_id, sqe->topic_type)) &&
        (!esched_drv_is_sched_mode_change_task(sqe->topic_id, sqe->subtopic_id))) {
        ret = esched_adapt_fill_sqe_host_pid(chip_id, event_info, sqe, &host_pid);
        if (ret != 0) {
            return ret;
        }
        target_pid = host_pid;
    } else {
        target_pid = (u32)event_info->pid;
    }

    ret = esched_drv_fill_task_msg(chip_id, event_src, (void *)&sqe->user_data[0], event_info);
    if (ret != 0) {
        return ret;
    }

    ret = esched_update_sqe_user_data(sqe, event_info);
    if (ret != 0) {
#ifndef EMU_ST
        return ret;
#endif
    }

    return esched_set_sqe_pid(chip_id, target_pid, sqe);
}

int esched_drv_fill_split_task(u32 chip_id, u32 event_src, struct sched_published_event_info *event_info,
                               void *split_task)
{
    return esched_drv_fill_sqe(chip_id, event_src, (struct topic_sched_sqe *)split_task, event_info, true);
}

/*
 * Extract the destination pid from the mailbox.
 * Only the stream_id encoded case can carry a pid-as-destination flag,
 * so is_pid_dst is always false for the topic_id based cases.
 */
static u32 esched_get_mb_dst_pid(const struct topic_sched_mailbox *mb, bool *is_pid_dst)
{
    u32 dst_pid;

    *is_pid_dst = false;

    if ((mb->topic_id == EVENT_TS_HWTS_KERNEL) || (mb->topic_id == EVENT_TS_CTRL_MSG)) {
        return mb->user_data[TOPIC_SCHED_TS_PID_INDEX];
    }
    if (mb->topic_id == EVENT_TS_CALLBACK_MSG) {
        return mb->user_data[TOPIC_SCHED_TS_CALLBACK_PID_INDEX];
    }

    ka_mm_memcpy_fromio(&dst_pid, &mb->stream_id, sizeof(dst_pid)); /* pid store in stream_id and task_id */
    *is_pid_dst = ((dst_pid & PID_MAP_MASK) != 0);
    return (dst_pid & MB_PID_MASK);
}

static enum devdrv_process_type esched_get_mb_cp_type(const struct topic_sched_mailbox *mb)
{
    if ((mb->kernel_type == TOPIC_SCHED_CUSTOM_KERNEL_TYPE) ||
        (mb->kernel_type == TOPIC_SCHED_CUSTOM_KFC_KERNEL_TYPE)) {
        return DEVDRV_PROCESS_CP2;
    }
    return DEVDRV_PROCESS_CP1;
}

/*
 * Query the device-side pid for dst_pid and store it into mb->pid.
 * query_failed_pid remembers the last failing pid so that a repeatedly failing
 * pid is reported only once; it is cleared once the same pid succeeds again.
 */
static int esched_query_mb_real_pid(struct topic_sched_mailbox *mb, u32 devid, u32 dst_pid)
{
    enum devdrv_process_type cp_type = esched_get_mb_cp_type(mb);
    static u32 query_failed_pid = SCHED_INVALID_PID;
    int real_pid;
    int ret;

    ret = hal_kernel_devdrv_query_process_by_host_pid_kernel(dst_pid, devid, cp_type, 0, &real_pid);
    if (ret != 0) {
#ifndef EMU_ST
        /* kfc dispatch to cp2 has no qs fallback */
        if (cp_type != DEVDRV_PROCESS_CP2) {
            ret = hal_kernel_devdrv_query_process_by_host_pid_kernel(dst_pid, devid, DEVDRV_PROCESS_QS, 0, &real_pid);
        }
        if (ret != 0) {
            if ((query_failed_pid != dst_pid) && (!esched_log_limited(SCHED_LOG_LIMIT_GET_REAL_PID))) {
                sched_err("Failed to query target pid by pid. (devid=%u; vfid=%u; topic_id=%u; topic_type=%u; "
                          "kernel_type=%u; pid=%u; ret=%d)\n",
                          devid, mb->vfid + 1U, mb->topic_id, mb->topic_type, mb->kernel_type, dst_pid, ret);
            }
            query_failed_pid = dst_pid;
            return DRV_ERROR_NO_PROCESS;
        }
#endif
    }

    mb->pid = (u32)real_pid;
    if (query_failed_pid == dst_pid) {
#ifndef EMU_ST
        query_failed_pid = SCHED_INVALID_PID;
#endif
    }

    return DRV_ERROR_NONE;
}

int esched_get_real_pid(struct topic_sched_mailbox *mb, u32 devid, u32 pid)
{
    bool is_pid_dst = false;
    u32 dst_pid;

    /* The preferred way to passthrough pid is by usr_pid_flag,
       other pid fields in user_data can be normalized here. */
    if (pid != SCHED_INVALID_PID) {
#ifndef EMU_ST
        mb->pid = pid;
        return DRV_ERROR_NONE;
#endif
    }

    if ((esched_is_phy_dev(devid) == true) && (mb->pid != TOPIC_SCHED_ESCHED_COMM_PID)) {
        if (mb->kernel_type != TOPIC_SCHED_CUSTOM_KFC_KERNEL_TYPE) { /* kfc dispatch to cp2 */
            mb->pid &= MB_PID_MASK;
            return DRV_ERROR_NONE;
        }
    }

    dst_pid = esched_get_mb_dst_pid(mb, &is_pid_dst);

    if ((esched_drv_is_used_host_pid(mb->topic_id, mb->topic_type) == false) || is_pid_dst) {
        mb->pid = dst_pid;
        return DRV_ERROR_NONE;
    }

    return esched_query_mb_real_pid(mb, devid, dst_pid);
}

bool esched_drv_check_dst_is_support(u32 dst_engine)
{
    return true;
}

int esched_drv_get_ccpu_flag(u32 dst_engine)
{
    if ((dst_engine != CCPU_DEVICE) && (dst_engine != CCPU_HOST) && (dst_engine != CCPU_LOCAL)) {
        return SCHED_INVALID;
    }
    return SCHED_VALID;
}

void esched_drv_flush_mb_mbid(u8 *mb_id_ptr, u8 mb_id)
{}

int esched_drv_sqe_update(unsigned int devid, unsigned int tsid, int pid, unsigned int sqid, void *sqe)
{
    if (((struct topic_sched_sqe *)sqe)->type == TOPIC_SCHED_SQE_TYPE) {
        return 0;
    }

    return -EINVAL;
}
KA_EXPORT_SYMBOL_GPL(esched_drv_sqe_update);