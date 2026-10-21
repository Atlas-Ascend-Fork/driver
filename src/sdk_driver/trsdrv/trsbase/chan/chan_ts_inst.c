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
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"

#include "pbl/pbl_uda.h"
#include "pbl/pbl_soc_res.h"
#include "trs_id.h"

#include "chan_proc_fs.h"
#include "chan_rxtx.h"
#include "chan_ts_inst.h"

TRS_INIT_REBOOT_NOTIFY;

#define TRS_CHAN_MAX_IRQ_NUM 64

static ka_mutex_t chan_ts_inst_mutex;
static ka_task_spinlock_t chan_ts_inst_lock;

static struct trs_chan_ts_inst *chan_ts_inst[TRS_TS_INST_MAX_NUM] = {
    NULL,
};

static void trs_chan_ts_inst_irq_hw_res_uninit(struct trs_chan_ts_inst *ts_inst, struct trs_chan_irq_ctx *irq_ctx,
                                               u32 irq_num)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int ret;
    u32 i;

    if (irq_ctx != NULL) {
        for (i = 0; i < irq_num; i++) {
            ret = ts_inst->ops.free_irq(inst, irq_ctx[i].irq_type, irq_ctx[i].irq_index, &irq_ctx[i]);
            if (ret != 0) {
                trs_err("Free irq failed. (devid=%u; tsid=%u; id=%u)\n", inst->devid, inst->tsid, i);
            }
            ka_system_tasklet_kill(&irq_ctx[i].task);
        }
    }
}

static void trs_chan_ts_inst_irq_sw_res_uninit(struct trs_chan_irq_ctx *irq_ctx)
{
    if (irq_ctx != NULL) {
        trs_kfree(irq_ctx);
    }
}

static void trs_chan_ts_inst_irq_uninit(struct trs_chan_ts_inst *ts_inst, struct trs_chan_irq_ctx *irq_ctx, u32 irq_num)
{
    trs_chan_ts_inst_irq_hw_res_uninit(ts_inst, irq_ctx, irq_num);
    trs_chan_ts_inst_irq_sw_res_uninit(irq_ctx);
}

static int trs_chan_ts_inst_irq_init(struct trs_chan_ts_inst *ts_inst, u32 irq_type, struct trs_chan_irq_ctx **irq,
                                     u32 *num)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan_irq_ctx *irq_ctx = NULL;
    struct trs_chan_irq_ctx *tmp = NULL;
    u32 i, irq_num, irq_id[TRS_CHAN_MAX_IRQ_NUM];
    int ret;

    ret = ts_inst->ops.get_irq(inst, irq_type, irq_id, TRS_CHAN_MAX_IRQ_NUM, &irq_num);
    if ((ret != 0) || (irq_num > TRS_CHAN_MAX_IRQ_NUM)) {
        *num = 0;
        return (ret != 0) ? ret : -ERANGE;
    }

    irq_ctx = trs_kzalloc(sizeof(struct trs_chan_irq_ctx) * irq_num, KA_GFP_KERNEL);
    if (irq_ctx == NULL) {
        trs_err("Mem alloc failed. (size=%lx)", sizeof(struct trs_chan_irq_ctx) * irq_num);
        return -ENOMEM;
    }

    tmp = irq_ctx;
    for (i = 0; i < irq_num; i++) {
        irq_ctx[i].irq = irq_id[i];
        irq_ctx[i].irq_index = i;
        irq_ctx[i].chan_num = 0;
        irq_ctx[i].ts_inst = ts_inst;
        irq_ctx[i].irq_type = irq_type;
        ka_task_spin_lock_init(&irq_ctx[i].lock);
        KA_INIT_LIST_HEAD(&irq_ctx[i].chan_list);

        ka_system_tasklet_init(&irq_ctx[i].task, trs_chan_tasklet, (unsigned long)&irq_ctx[i]);

        ret = ts_inst->ops.request_irq(inst, irq_type, i, &irq_ctx[i], trs_chan_irq_proc);
        if (ret != 0) {
            trs_chan_ts_inst_irq_uninit(ts_inst, irq_ctx, i);
            trs_warn("Request irq result. (devid=%u; tsid=%u; id=%u; ret=%d)\n", inst->devid, inst->tsid, i, ret);
            return ret;
        }
    }

    *irq = tmp;
    *num = irq_num;

    return 0;
}

static int trs_chan_ts_inst_hw_cq_ctx_init(struct trs_chan_ts_inst *ts_inst)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    u32 cq_id, rsv_hw_cq_max_id;
    int ret;

    ret = trs_id_get_max_id(inst, TRS_HW_CQ_ID, &ts_inst->cq_max_id);
    if (ret != 0) {
        trs_err("Get cq max id failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }
    ret = trs_id_get_max_id(inst, TRS_RSV_HW_CQ_ID, &rsv_hw_cq_max_id);
    if (ret == 0) { /* Probably no RSV HW CQ */
        ts_inst->cq_max_id = ka_base_max_t(u32, ts_inst->cq_max_id, rsv_hw_cq_max_id);
    }

    ts_inst->hw_cq_ctx = trs_vzalloc(sizeof(struct trs_chan_hw_cq_ctx) * ts_inst->cq_max_id);
    if (ts_inst->hw_cq_ctx == NULL) {
        trs_err("Mem alloc failed. (size=%lx)\n", sizeof(int) * ts_inst->cq_max_id);
        return -ENOMEM;
    }

    for (cq_id = 0; cq_id < ts_inst->cq_max_id; cq_id++) {
        ts_inst->hw_cq_ctx[cq_id].irq_index = KA_U32_MAX;
        ts_inst->hw_cq_ctx[cq_id].valid = false;
    }
    return 0;
}

static void trs_chan_ts_inst_hw_cq_ctx_uninit(struct trs_chan_ts_inst *ts_inst)
{
    if (ts_inst->hw_cq_ctx != NULL) {
        trs_vfree(ts_inst->hw_cq_ctx);
        ts_inst->hw_cq_ctx = NULL;
    }
}

static int trs_chan_ts_inst_hw_sq_ctx_init(struct trs_chan_ts_inst *ts_inst)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    u32 i, rsv_hw_sq_max_id;
    int ret;

    ret = trs_id_get_max_id(inst, TRS_HW_SQ_ID, &ts_inst->sq_max_id);
    if (ret != 0) {
        trs_err("Get sq max id failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }
    ret = trs_id_get_max_id(inst, TRS_RSV_HW_SQ_ID, &rsv_hw_sq_max_id);
    if (ret == 0) { /* Probably no RSV HW SQ */
        ts_inst->sq_max_id = ka_base_max_t(u32, ts_inst->sq_max_id, rsv_hw_sq_max_id);
    }

    ts_inst->hw_sq_ctx = trs_vzalloc(sizeof(struct trs_chan_hw_sq_ctx) * ts_inst->sq_max_id);
    if (ts_inst->hw_sq_ctx == NULL) {
        trs_err("Mem alloc failed. (size=%lx)\n", sizeof(int) * ts_inst->sq_max_id);
        return -ENOMEM;
    }

    for (i = 0; i < ts_inst->sq_max_id; i++) {
        ts_inst->hw_sq_ctx[i].chan_id = -1;
    }

    return 0;
}

static void trs_chan_ts_inst_hw_sq_ctx_uninit(struct trs_chan_ts_inst *ts_inst)
{
    if (ts_inst->hw_sq_ctx != NULL) {
        trs_vfree(ts_inst->hw_sq_ctx);
        ts_inst->hw_sq_ctx = NULL;
    }
}

static int trs_chan_ts_inst_hw_sqcq_ctx_init(struct trs_chan_ts_inst *ts_inst)
{
    int ret;

    ret = trs_chan_ts_inst_hw_sq_ctx_init(ts_inst);
    if (ret != 0) {
        trs_err("Sq chan ctx init failed. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);
        return ret;
    }

    ret = trs_chan_ts_inst_hw_cq_ctx_init(ts_inst);
    if (ret != 0) {
        trs_chan_ts_inst_hw_sq_ctx_uninit(ts_inst);
        trs_err("Cq chan ctx init failed. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);
    }
    return ret;
}

static void trs_chan_ts_inst_hw_sqcq_ctx_uninit(struct trs_chan_ts_inst *ts_inst)
{
    trs_chan_ts_inst_hw_cq_ctx_uninit(ts_inst);
    trs_chan_ts_inst_hw_sq_ctx_uninit(ts_inst);
}

static int trs_chan_ts_inst_maint_sq_ctx_init(struct trs_chan_ts_inst *ts_inst)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int ret;
    u32 i;

    ret = trs_id_get_max_id(inst, TRS_MAINT_SQ_ID, &ts_inst->maint_sq_max_id);
    if ((ret != 0) || (ts_inst->maint_sq_max_id == 0)) {
#ifndef EMU_ST
        return 0;
#endif
    }

    ts_inst->maint_sq_ctx = trs_vzalloc(sizeof(struct trs_chan_maint_sq_ctx) * ts_inst->maint_sq_max_id);
    if (ts_inst->maint_sq_ctx == NULL) {
        trs_err("Mem alloc failed. (size=%lx)\n", sizeof(struct trs_chan_maint_sq_ctx) * ts_inst->maint_sq_max_id);
        return -ENOMEM;
    }

    for (i = 0; i < ts_inst->maint_sq_max_id; i++) {
        ts_inst->maint_sq_ctx[i].chan_id = -1;
    }

    return 0;
}

static void trs_chan_ts_inst_maint_sq_ctx_uninit(struct trs_chan_ts_inst *ts_inst)
{
    if (ts_inst->maint_sq_ctx != NULL) {
        trs_vfree(ts_inst->maint_sq_ctx);
        ts_inst->maint_sq_ctx = NULL;
        ts_inst->maint_sq_max_id = 0;
    }
}

static int trs_chan_ts_inst_maint_cq_ctx_init(struct trs_chan_ts_inst *ts_inst)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int ret;
    u32 i;

    ret = trs_id_get_max_id(inst, TRS_MAINT_CQ_ID, &ts_inst->maint_cq_max_id);
    if ((ret != 0) || (ts_inst->maint_cq_max_id == 0)) {
#ifndef EMU_ST
        return 0;
#endif
    }

    ts_inst->maint_cq_ctx = trs_vzalloc(sizeof(struct trs_chan_maint_cq_ctx) * ts_inst->maint_cq_max_id);
    if (ts_inst->maint_cq_ctx == NULL) {
        trs_err("Mem alloc failed. (size=%lx)\n", sizeof(struct trs_chan_maint_cq_ctx) * ts_inst->maint_cq_max_id);
        return -ENOMEM;
    }

    for (i = 0; i < ts_inst->maint_cq_max_id; i++) {
        ts_inst->maint_cq_ctx[i].chan_id = -1;
    }

    return 0;
}

static void trs_chan_ts_inst_maint_cq_ctx_uninit(struct trs_chan_ts_inst *ts_inst)
{
    if (ts_inst->maint_cq_ctx != NULL) {
        trs_vfree(ts_inst->maint_cq_ctx);
        ts_inst->maint_cq_ctx = NULL;
        ts_inst->maint_cq_max_id = 0;
    }
}

static int trs_chan_ts_inst_maint_sqcq_ctx_init(struct trs_chan_ts_inst *ts_inst)
{
    int ret;

    ret = trs_chan_ts_inst_maint_sq_ctx_init(ts_inst);
    if (ret != 0) {
        trs_err("Maint sq chan ctx init failed. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);
        return ret;
    }

    ret = trs_chan_ts_inst_maint_cq_ctx_init(ts_inst);
    if (ret != 0) {
        trs_chan_ts_inst_maint_sq_ctx_uninit(ts_inst);
        trs_err("Maint cq chan ctx init failed. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);
    }
    return ret;
}

static void trs_chan_ts_inst_maint_sqcq_ctx_uninit(struct trs_chan_ts_inst *ts_inst)
{
    trs_chan_ts_inst_maint_cq_ctx_uninit(ts_inst);
    trs_chan_ts_inst_maint_sq_ctx_uninit(ts_inst);
}

static void trs_chan_cq_guard_work_sched(ka_work_struct_t *p_work)
{
    struct trs_chan_guard_work *guard_work = ka_container_of(p_work, struct trs_chan_guard_work, cq_guard_work.work);
    struct trs_chan_ts_inst *ts_inst = guard_work->ts_inst;

    trs_debug("Sched cq guard work. (devid=%u; cq_max_id=%u)\n", ts_inst->inst.devid, ts_inst->cq_max_id);
    trs_chan_cq_guard_work_proc(ts_inst);
    if (ts_inst->guard_work.work_magic == TRS_CHAN_GUARD_WORK_MAGIC) {
        ka_task_schedule_delayed_work(&ts_inst->guard_work.cq_guard_work,
                                      ka_system_msecs_to_jiffies(TRS_CHAN_GUARD_WORK_DELAY_MS));
    }
}

static int trs_chan_ts_inst_create(struct trs_id_inst *inst, int hw_type, int location, struct trs_chan_adapt_ops *ops)
{
    u32 ts_inst_id = trs_id_inst_to_ts_inst(inst);
    struct trs_chan_ts_inst *ts_inst = NULL;
    int ret;

    if (chan_ts_inst[ts_inst_id] != NULL) {
        trs_err("Repeat register. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    ts_inst = trs_kzalloc(sizeof(struct trs_chan_ts_inst), KA_GFP_KERNEL);
    if (ts_inst == NULL) {
        trs_err("Mem alloc failed. (size=%lx)\n", sizeof(struct trs_chan_ts_inst));
        return -ENOMEM;
    }

    ts_inst->hw_type = hw_type;
    ts_inst->location = location;
    ts_inst->inst = *inst;
    ts_inst->ops = *ops;
    kref_safe_init(&ts_inst->ref);
    ka_task_mutex_init(&ts_inst->mutex);

    ret = trs_chan_ts_inst_hw_sqcq_ctx_init(ts_inst);
    if (ret != 0) {
        trs_kfree(ts_inst);
        trs_err("Sqcq chan ctx init failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    ret = trs_chan_ts_inst_maint_sqcq_ctx_init(ts_inst);
    if (ret != 0) {
        trs_chan_ts_inst_hw_sqcq_ctx_uninit(ts_inst);
        trs_kfree(ts_inst);
        trs_err("Sqcq chan ctx init failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    ret = trs_chan_ts_inst_irq_init(ts_inst, TS_CQ_UPDATE_IRQ, &ts_inst->normal_irq, &ts_inst->normal_irq_num);
    if (ret != 0) {
        trs_chan_ts_inst_maint_sqcq_ctx_uninit(ts_inst);
        trs_chan_ts_inst_hw_sqcq_ctx_uninit(ts_inst);
        trs_kfree(ts_inst);
        trs_err("Cq update irq init failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    ret = trs_chan_ts_inst_irq_init(ts_inst, TS_FUNC_CQ_IRQ, &ts_inst->maint_irq, &ts_inst->maint_irq_num);
    if (ret != 0) {
        trs_info("Not support maint irq. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
    }

    if ((ts_inst->location == UDA_NEAR) || (ts_inst->location == UDA_REMOTE)) {
        KA_TASK_INIT_DELAYED_WORK(&ts_inst->guard_work.cq_guard_work, trs_chan_cq_guard_work_sched);
        ts_inst->guard_work.work_magic = TRS_CHAN_GUARD_WORK_MAGIC;
        ts_inst->guard_work.ts_inst = ts_inst;
        ka_task_schedule_delayed_work(&ts_inst->guard_work.cq_guard_work, 0);
        trs_info("Create guard work. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
    }

    chan_proc_fs_add_ts_inst(ts_inst);

    chan_ts_inst[ts_inst_id] = ts_inst;

    trs_info("Ts inst create success. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);

    return 0;
}

static void trs_chan_ts_inst_release(struct kref_safe *kref)
{
    struct trs_chan_ts_inst *ts_inst = ka_container_of(kref, struct trs_chan_ts_inst, ref);

    trs_info("Ts inst release success. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);

    chan_proc_fs_del_ts_inst(ts_inst);
    if (((ts_inst->location == UDA_NEAR) || (ts_inst->location == UDA_REMOTE)) &&
        (ts_inst->guard_work.cq_guard_work.work.func != NULL) &&
        (ts_inst->guard_work.work_magic == TRS_CHAN_GUARD_WORK_MAGIC)) {
        ka_task_cancel_delayed_work_sync(&ts_inst->guard_work.cq_guard_work);
        ts_inst->guard_work.ts_inst = NULL;
        ts_inst->guard_work.work_magic = 0;
        trs_info("Destroy guard work. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);
    }
    trs_chan_ts_inst_irq_sw_res_uninit(ts_inst->maint_irq);
    trs_chan_ts_inst_irq_sw_res_uninit(ts_inst->normal_irq);
    trs_chan_ts_inst_maint_sqcq_ctx_uninit(ts_inst);
    trs_chan_ts_inst_hw_sqcq_ctx_uninit(ts_inst);
    trs_kfree(ts_inst);
}

static void trs_chan_ts_inst_destroy(struct trs_id_inst *inst)
{
    u32 ts_inst_id = trs_id_inst_to_ts_inst(inst);
    struct trs_chan_ts_inst *ts_inst = chan_ts_inst[ts_inst_id];

    if (ts_inst == NULL) {
        trs_err("Repeat unregister. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return;
    }

    ka_task_spin_lock_bh(&chan_ts_inst_lock);
    chan_ts_inst[ts_inst_id] = NULL; /* set inst invalid, so other thread will not get it */
    ka_task_spin_unlock_bh(&chan_ts_inst_lock);

    trs_info("Ts inst destroy success. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);

    trs_chan_ts_inst_irq_hw_res_uninit(ts_inst, ts_inst->maint_irq, ts_inst->maint_irq_num);
    trs_chan_ts_inst_irq_hw_res_uninit(ts_inst, ts_inst->normal_irq, ts_inst->normal_irq_num);
    trs_all_chan_cq_work_cancel(ts_inst);

    kref_safe_put(&ts_inst->ref, trs_chan_ts_inst_release);
}

static int trs_chan_check_ts_inst_ops(struct trs_chan_adapt_ops *ops)
{
    if (ops == NULL) {
        return -EINVAL;
    }

    if ((ops->sq_mem_alloc == NULL) || (ops->sq_mem_free == NULL) || (ops->cq_mem_alloc == NULL) ||
        (ops->cq_mem_free == NULL) || (ops->notice_ts == NULL) || (ops->get_sq_head_in_cqe == NULL) ||
        (ops->sqcq_ctrl == NULL) || (ops->cqe_is_valid == NULL) || (ops->get_irq == NULL) ||
        (ops->request_irq == NULL) || (ops->free_irq == NULL) || (ops->sqcq_query == NULL) || (ops->owner == NULL)) {
        return -EINVAL;
    }

    return 0;
}

int trs_chan_ts_inst_register(struct trs_id_inst *inst, int hw_type, int location, struct trs_chan_adapt_ops *ops)
{
    int ret;

    ret = trs_id_inst_check(inst);
    if (ret != 0) {
        return ret;
    }

    ret = trs_chan_check_ts_inst_ops(ops);
    if (ret != 0) {
        trs_err("Invalid ops. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    ka_task_mutex_lock(&chan_ts_inst_mutex);
    ret = trs_chan_ts_inst_create(inst, hw_type, location, ops);
    ka_task_mutex_unlock(&chan_ts_inst_mutex);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_ts_inst_register);

void trs_chan_ts_inst_unregister(struct trs_id_inst *inst)
{
    int ret;

    ret = trs_id_inst_check(inst);
    if (ret != 0) {
        return;
    }

    ka_task_mutex_lock(&chan_ts_inst_mutex);
    trs_chan_ts_inst_destroy(inst);
    ka_task_mutex_unlock(&chan_ts_inst_mutex);
}
KA_EXPORT_SYMBOL_GPL(trs_chan_ts_inst_unregister);

struct trs_chan_ts_inst *trs_chan_ts_inst_get(struct trs_id_inst *inst)
{
    u32 ts_inst_id = trs_id_inst_to_ts_inst(inst);
    struct trs_chan_ts_inst *ts_inst = NULL;

    ka_task_spin_lock_bh(&chan_ts_inst_lock);
    ts_inst = chan_ts_inst[ts_inst_id];
    if (ts_inst != NULL) {
        /* When chan ts inst is obtained, the module reference counting of ops must be added. */
        if (ka_system_try_module_get(ts_inst->ops.owner)) {
            kref_safe_get(&ts_inst->ref);
        } else {
            ts_inst = NULL;
        }
    }
    ka_task_spin_unlock_bh(&chan_ts_inst_lock);

    return ts_inst;
}

void trs_chan_ts_inst_put(struct trs_chan_ts_inst *ts_inst)
{
    ka_system_module_put(ts_inst->ops.owner);
    kref_safe_put(&ts_inst->ref, trs_chan_ts_inst_release);
}

int trs_chan_init_module(void)
{
    u32 i;

    for (i = 0; i < TRS_TS_INST_MAX_NUM; i++) {
        chan_ts_inst[i] = NULL;
    }

    ka_task_mutex_init(&chan_ts_inst_mutex);
    ka_task_spin_lock_init(&chan_ts_inst_lock);

    chan_proc_fs_init();
    TRS_REGISTER_REBOOT_NOTIFY;

    return 0;
}

void trs_chan_exit_module(void)
{
    TRS_UNREGISTER_REBOOT_NOTIFY;
    chan_proc_fs_uninit();
    ka_task_mutex_destroy(&chan_ts_inst_mutex);
}