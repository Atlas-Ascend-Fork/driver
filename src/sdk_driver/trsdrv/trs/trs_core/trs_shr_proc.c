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

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_fs_pub.h"
#include "ka_ioctl_pub.h"

#include "trs_ts_inst.h"
#include "trs_shr_proc.h"

bool trs_proc_cp2_type_check(struct trs_core_ts_inst *ts_inst)
{
    if (ts_inst->ops.proc_cp2_type_check != NULL) {
        if (ts_inst->ops.proc_cp2_type_check() == true) {
            return true;
        }
    }

    return false;
}

int trs_shr_proc_get_share_pid(struct trs_core_ts_inst *ts_inst, u32 *share_pid)
{
#ifndef EMU_ST
    u32 pid = 0;
    int ret;

    if (ts_inst->ops.proc_cp2_get_shr_pid != NULL) {
        ret = ts_inst->ops.proc_cp2_get_shr_pid(&ts_inst->inst, &pid);
        if (ret != 0) {
            return ret;
        }
    }

    if (pid == 0) {
        trs_err("Not support. (devid=%u)\n", ts_inst->inst.devid);
        return -ESRCH;
    }

    *share_pid = pid;
    return 0;
#endif
}

int trs_shr_proc_open(ka_file_t *file, struct trs_core_ts_inst *ts_inst, struct trs_task_info_struct *task_info)
{
#ifndef EMU_ST
    struct trs_proc_ctx *proc_ctx = NULL;
    u32 share_pid = 0;
    int ret;

    ret = trs_shr_proc_get_share_pid(ts_inst, &share_pid);
    if (ret != 0) {
        return ret;
    }

    proc_ctx = trs_get_share_ctx(ts_inst, share_pid);
    if (proc_ctx == NULL) {
        trs_err("Current proc share ctx failed. (devid=%u)\n", ts_inst->inst.devid);
        return -ESRCH;
    }

    ka_task_write_lock_bh(&proc_ctx->ctx_rwlock);
    if (proc_ctx->cp2_pid != 0) {
        ka_task_write_unlock_bh(&proc_ctx->ctx_rwlock);
        trs_err("Shr ctx only supports one process share.\n");
        trs_put_share_ctx(proc_ctx);
        return -EMFILE;
    }

    proc_ctx->cp2_pid = ka_task_get_current_tgid();
    proc_ctx->cp2_task_id = ka_base_atomic64_inc_return(&ts_inst->cur_task_id);
    ka_task_write_unlock_bh(&proc_ctx->ctx_rwlock);

    task_info->proc_ctx = proc_ctx;
    task_info->unique_id = proc_ctx->cp2_task_id;
    ka_fs_set_file_private_data(file, task_info);

    trs_debug(
        "Share open success. (devid=%u; tgid=%u; task_info_unique_id=%lld; proc_ctx_taskid=%lld; cp2_taskid=%lld)\n",
        ts_inst->inst.devid, proc_ctx->cp2_pid, task_info->unique_id, proc_ctx->task_id, proc_ctx->cp2_task_id);
    return 0;
#endif
}

int trs_shr_proc_close(struct trs_task_info_struct *task_info)
{
#ifndef EMU_ST
    struct trs_proc_ctx *proc_ctx = task_info->proc_ctx;

    trs_proc_ctx_put(proc_ctx);
    return 0;
#endif
}

bool trs_shr_proc_check(struct trs_task_info_struct *task_info)
{
    struct trs_proc_ctx *proc_ctx = task_info->proc_ctx;

    if ((proc_ctx->cp2_pid != 0) && (task_info->unique_id == proc_ctx->cp2_task_id)) {
        return true;
    }

    return false;
}

bool trs_shr_proc_support_cmd_check(unsigned int cmd)
{
#ifndef EMU_ST
    if ((cmd == TRS_RES_ID_ENABLE) || (cmd == TRS_RES_ID_DISABLE) || (cmd == TRS_RES_ID_NUM_QUERY) ||
        (cmd == TRS_RES_ID_MAX_QUERY) || (cmd == TRS_RES_ID_USED_NUM_QUERY) || (cmd == TRS_RES_ID_AVAIL_NUM_QUERY) ||
        (cmd == TRS_RES_ID_REG_OFFSET_QUERY) || (cmd == TRS_RES_ID_REG_SIZE_QUERY) || (cmd == TRS_RES_ID_CFG) ||
        (cmd == TRS_HW_INFO_QUERY) || (cmd == TRS_MSG_CTRL) || (cmd == TRS_SQCQ_CONFIG) || (cmd == TRS_SQCQ_QUERY) ||
        (cmd == TRS_SQCQ_SEND) || (cmd == TRS_SQCQ_RECV) || (cmd == TRS_ID_SQCQ_GET) || (cmd == TRS_ID_SQCQ_RESTORE)) {
        return true;
    }

    trs_debug("Not support cmd. (cmd=%d)\n", _KA_IOC_NR(cmd));
    return false;
#endif
}

bool trs_proc_support_cmd_check(struct trs_task_info_struct *task_info, unsigned int cmd)
{
#ifndef EMU_ST
    if (trs_shr_proc_check(task_info) == true) {
        if (trs_shr_proc_support_cmd_check(cmd) == false) {
            return false;
        }
    } else {
        if (cmd == TRS_ID_SQCQ_RESTORE) {
            trs_debug("Not support cmd. (cmd=%d)\n", _KA_IOC_NR(cmd));
            return false;
        }
    }
#endif
    return true;
}
