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

#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_kernel_def_pub.h"

#include "trs_proc_fs.h"
#include "trs_proc.h"
#include "trs_ts_inst.h"

ka_mutex_t core_ts_inst_mutex;
static ka_rwlock_t core_ts_inst_lock;

struct trs_core_ts_inst *core_ts_inst[TRS_TS_INST_MAX_NUM] = {
    NULL,
};

static int trs_core_ts_inst_init(struct trs_core_ts_inst *ts_inst)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int ret;

    kref_safe_init(&ts_inst->ref);
    ka_task_init_rwsem(&ts_inst->sem);
    ka_task_init_rwsem(&ts_inst->ctrl_sem);
    ka_base_atomic_set(&ts_inst->ctx_num, 0);
    KA_INIT_LIST_HEAD(&ts_inst->proc_list_head);
    KA_INIT_LIST_HEAD(&ts_inst->exit_proc_list_head);
    ts_inst->sq_work_retry_time = 0;

    ts_inst->support_proc_num = 0;
    if (ts_inst->ops.get_res_support_proc_num != NULL) {
        (void)ts_inst->ops.get_res_support_proc_num(inst, &ts_inst->support_proc_num);
    }

    ret = trs_res_mng_init(ts_inst);
    if (ret != 0) {
        trs_err("Res mng init failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    ret = trs_cb_sqcq_init(ts_inst);
    if (ret != 0) {
        trs_err("Cb sqcq init failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        goto res_mng_uninit;
    }

    ret = trs_logic_cq_init(ts_inst);
    if (ret != 0) {
        trs_err("Logic cq init failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        goto cb_sqcq_uninit;
    }

    ret = trs_shm_sqcq_init(ts_inst);
    if (ret != 0) {
        trs_err("Shm sqcq init failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        goto logic_cq_uninit;
    }

    ret = trs_hw_sqcq_init(ts_inst);
    if (ret != 0) {
        trs_err("Hw sqcq init failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        goto shm_sqcq_uninit;
    }

    ret = trs_sw_sqcq_init(ts_inst);
    if (ret != 0) {
        trs_err("Sw sqcq init failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        goto hw_sqcq_uninit;
    }

    ret = trs_maint_sqcq_init(ts_inst);
    if (ret != 0) {
        trs_err("Maint sqcq init failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        goto sw_sqcq_uninit;
    }

    ret = trs_shr_ctx_mng_init(inst);
    if (ret != 0) {
        trs_err("Shr sq ctx init failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        goto shr_ctx_mng_uninit;
    }

    return 0;

shr_ctx_mng_uninit:
    trs_maint_sqcq_uninit(ts_inst);
sw_sqcq_uninit:
    trs_sw_sqcq_uninit(ts_inst);
hw_sqcq_uninit:
    trs_hw_sqcq_uninit(ts_inst);
shm_sqcq_uninit:
    trs_shm_sqcq_uninit(ts_inst);
logic_cq_uninit:
    trs_logic_cq_uninit(ts_inst);
cb_sqcq_uninit:
    trs_cb_sqcq_uninit(ts_inst);
res_mng_uninit:
    trs_res_mng_uninit(ts_inst);
    return ret;
}

static void trs_core_ts_inst_uninit(struct trs_core_ts_inst *ts_inst)
{
    trs_shr_ctx_mng_uninit(&ts_inst->inst);
    trs_maint_sqcq_uninit(ts_inst);
    trs_sw_sqcq_uninit(ts_inst);
    trs_hw_sqcq_uninit(ts_inst);
    trs_shm_sqcq_uninit(ts_inst);
    trs_logic_cq_uninit(ts_inst);
    trs_cb_sqcq_uninit(ts_inst);
    trs_res_mng_uninit(ts_inst);
}

static int trs_core_ts_inst_create(struct trs_id_inst *inst, int hw_type, int location, u32 ts_inst_flag,
                                   struct trs_core_adapt_ops *ops)
{
    u32 ts_inst_id = trs_id_inst_to_ts_inst(inst);
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret;

    if (core_ts_inst[ts_inst_id] != NULL) {
        trs_err("Repeat register. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    ts_inst = trs_vzalloc(sizeof(struct trs_core_ts_inst));
    if (ts_inst == NULL) {
        trs_err("Mem alloc failed. (devid=%u; tsid=%u; size=%lx)\n", inst->devid, inst->tsid,
                sizeof(struct trs_core_ts_inst));
        return -ENOMEM;
    }

    ts_inst->inst = *inst;
    ts_inst->hw_type = hw_type;
    ts_inst->location = location;
    ts_inst->ts_inst_flag = ts_inst_flag;
    ts_inst->ops = *ops;
    ts_inst->featur_mode = TRS_INST_ALL_FEATUR_MODE;
    ret = trs_core_ts_inst_init(ts_inst);
    if (ret != 0) {
        trs_vfree(ts_inst);
        trs_err("Ts inst create failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    proc_fs_add_ts_inst(ts_inst);
    core_ts_inst[ts_inst_id] = ts_inst;

    trs_debug("Ts inst create success. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);

    return 0;
}

static void trs_core_ts_inst_release(struct kref_safe *kref)
{
    struct trs_core_ts_inst *ts_inst = ka_container_of(kref, struct trs_core_ts_inst, ref);

    trs_info("Ts inst release success. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);

    trs_clear_exit_proc_list(ts_inst);
    proc_fs_del_ts_inst(ts_inst);
    trs_core_ts_inst_uninit(ts_inst);
    trs_vfree(ts_inst);
}

static void trs_core_ts_inst_destroy(struct trs_id_inst *inst)
{
    u32 ts_inst_id = trs_id_inst_to_ts_inst(inst);
    struct trs_core_ts_inst *ts_inst = core_ts_inst[ts_inst_id];

    if (core_ts_inst[ts_inst_id] == NULL) {
        trs_err("Repeat unregister. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return;
    }

    ka_task_write_lock_bh(&core_ts_inst_lock);
    core_ts_inst[ts_inst_id] = NULL; /* set inst invalid, so other thread will not get it */
    ka_task_write_unlock_bh(&core_ts_inst_lock);

    trs_info("Ts inst destroy success. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);

    trs_hw_sq_trigger_irq_hw_res_uninit(ts_inst);
    trs_thread_bind_irq_hw_res_uninit(ts_inst, ts_inst->logic_cq_ctx.intr_mng.irq_num);

    kref_safe_put(&ts_inst->ref, trs_core_ts_inst_release);
}

static int trs_core_check_ts_inst_ops(struct trs_core_adapt_ops *ops)
{
    if (ops == NULL) {
        return -EINVAL;
    }

    if ((ops->notice_ts == NULL) || (ops->ssid_query == NULL) || (ops->owner == NULL)) {
        return -EINVAL;
    }

    return 0;
}

int trs_core_ts_inst_register(struct trs_id_inst *inst, int hw_type, int location, u32 ts_inst_flag,
                              struct trs_core_adapt_ops *ops)
{
    int ret;

    ret = trs_id_inst_check(inst);
    if (ret != 0) {
        return ret;
    }

    ret = trs_core_check_ts_inst_ops(ops);
    if (ret != 0) {
        trs_err("Invalid ops. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    ka_task_mutex_lock(&core_ts_inst_mutex);
    ret = trs_core_ts_inst_create(inst, hw_type, location, ts_inst_flag, ops);
    ka_task_mutex_unlock(&core_ts_inst_mutex);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_core_ts_inst_register);

void trs_core_ts_inst_unregister(struct trs_id_inst *inst)
{
    int ret;

    ret = trs_id_inst_check(inst);
    if (ret != 0) {
        return;
    }

    ka_task_mutex_lock(&core_ts_inst_mutex);
    trs_core_ts_inst_destroy(inst);
    ka_task_mutex_unlock(&core_ts_inst_mutex);
}
KA_EXPORT_SYMBOL_GPL(trs_core_ts_inst_unregister);

struct trs_core_ts_inst *trs_core_ts_inst_get(struct trs_id_inst *inst)
{
    struct trs_core_ts_inst *ts_inst = NULL;

    if (trs_id_inst_check(inst) != 0) {
        return NULL;
    }

    ka_task_read_lock_bh(&core_ts_inst_lock);
    ts_inst = core_ts_inst[trs_id_inst_to_ts_inst(inst)];
    if (ts_inst != NULL) {
        kref_safe_get(&ts_inst->ref);
    }
    ka_task_read_unlock_bh(&core_ts_inst_lock);
    return ts_inst;
}

void trs_core_ts_inst_put(struct trs_core_ts_inst *ts_inst)
{
    kref_safe_put(&ts_inst->ref, trs_core_ts_inst_release);
}

static void trs_core_destroy_all_proc(struct trs_core_ts_inst *ts_inst)
{
    struct trs_proc_ctx *proc_ctx = NULL;
    struct trs_proc_ctx *tmp = NULL;

    ka_task_down_write(&ts_inst->sem);

    ka_list_for_each_entry_safe(proc_ctx, tmp, &ts_inst->proc_list_head, node)
    {
        trs_info("Recycle proc. (pid=%d, name=%s)", proc_ctx->pid, proc_ctx->name);
        ka_list_del(&proc_ctx->node);
        trs_proc_ctx_put(proc_ctx);
    }

    ka_list_for_each_entry_safe(proc_ctx, tmp, &ts_inst->exit_proc_list_head, node)
    {
        trs_info("Recycle proc. (pid=%d, name=%s)", proc_ctx->pid, proc_ctx->name);
        ka_list_del(&proc_ctx->node);
        trs_proc_ctx_put(proc_ctx);
    }

    ka_task_up_write(&ts_inst->sem);
}

void trs_core_inst_init(void)
{
    u32 i;

    ka_task_mutex_init(&core_ts_inst_mutex);
    ka_task_rwlock_init(&core_ts_inst_lock);

    for (i = 0; i < TRS_TS_INST_MAX_NUM; i++) {
        core_ts_inst[i] = NULL;
    }
}

void trs_core_inst_uninit(void)
{
    u32 i;

    for (i = 0; i < TRS_TS_INST_MAX_NUM; i++) {
        struct trs_core_ts_inst *ts_inst = core_ts_inst[i];
        if (ts_inst != NULL) {
            trs_core_destroy_all_proc(ts_inst);
            trs_core_ts_inst_unregister(&ts_inst->inst);
        }
    }
    ka_task_mutex_destroy(&core_ts_inst_mutex);
}

bool trs_still_has_proc(struct trs_core_ts_inst *ts_inst, u32 check_level)
{
    if ((check_level <= 1) && !ka_list_empty(&ts_inst->proc_list_head)) {
        return true;
    }

    if ((check_level <= 0) && !ka_list_empty(&ts_inst->exit_proc_list_head)) {
        return true;
    }

    return false;
}

int trs_set_ts_inst_feature_mode(struct trs_id_inst *inst, u32 mode, u32 force)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    if (mode == TRS_INST_PART_FEATUR_MODE) {
        if (force < (u32)TRS_SET_TS_INST_MODE_FORCE_LEVEL_ALL) {
            bool has_proc;

            ka_task_down_read(&ts_inst->sem);
            has_proc = trs_still_has_proc(ts_inst, force);
            ka_task_up_read(&ts_inst->sem);
            if (has_proc) {
                trs_core_inst_put(ts_inst);
                trs_err("Pf device still has proc, can not set mode. (devid=%u)\n", inst->devid);
                return -EBUSY;
            }
        }

        if (ts_inst->ops.get_sq_trigger_irq != NULL) {
            if (ts_inst->work_queue != NULL) {
                (void)ka_task_cancel_work_sync(&ts_inst->sq_trigger_work);
            }

            if (ts_inst->fair_work_queue != NULL) {
                (void)ka_task_cancel_work_sync(&ts_inst->sq_fair_work);
            }

            trs_hw_sq_send_thread_destroy(ts_inst);
        }
    }

    if (mode == TRS_INST_ALL_FEATUR_MODE) {
        if (ts_inst->ops.get_sq_trigger_irq != NULL) {
            ret = trs_hw_sq_send_thread_create(ts_inst);
            if (ret != 0) {
                trs_core_inst_put(ts_inst);
                trs_err("Failed to create hw sq send thread. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
                return ret;
            }
        }
    }

    ts_inst->featur_mode = mode;
    trs_core_inst_put(ts_inst);

    trs_info("Set device run status. (devid=%u; status=%u)\n", inst->devid, mode);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_set_ts_inst_feature_mode);

bool trs_check_ts_inst_has_proc(struct trs_id_inst *inst)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    bool is_has_proc = false;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        return false;
    }

    ka_task_down_read(&ts_inst->sem);
    if (trs_still_has_proc(ts_inst, 0)) {
        is_has_proc = true;
    }
    ka_task_up_read(&ts_inst->sem);

    trs_core_inst_put(ts_inst);
    return is_has_proc;
}
KA_EXPORT_SYMBOL_GPL(trs_check_ts_inst_has_proc);
