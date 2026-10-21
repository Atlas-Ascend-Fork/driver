/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include "ka_dfx_pub.h"
#include "ka_common_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_fs_pub.h"
#include "ka_barrier_pub.h"

#include "securec.h"
#include "pbl_feature_loader.h"
#include "pbl_task_ctx.h"
#include "dpa_kernel_interface.h"
#include "pbl_uda.h"

#include "svm_kern_log.h"
#include "svm_proc_fs.h"
#include "svm_slab.h"
#include "framework_dev.h"
#include "framework_task.h"
#include "svm_dev.h"
#include "svm_task.h"

struct svm_task_feature {
    const char *name;
    bool valid;
    void *priv;
    void (*release)(void *priv);
};

enum svm_task_state {
    SVM_TASK_STATE_RUNNING = 0,
    SVM_TASK_STATE_SELF_RECYCLE,
    SVM_TASK_STATE_APM_RECYCLE,
};

struct svm_task_ctx {
    u32 udevid;
    int tgid;
    u32 task_id;
    u32 valid;
    ka_atomic_t state;
    bool exit_force;
    bool exit_abort;
    struct task_start_time start_time;
    ka_mm_struct_t *mm;
    struct task_ctx *ctx;
    struct svm_dev_ctx *d_ctx;
    ka_proc_dir_entry_t *entry;
    struct svm_task_feature feature[];
};

struct task_tgids_num {
    int *tgid;
    u32 arr_num;
    u32 get_num;
};

struct svm_task_ctx_for_each_priv {
    void *priv;
    void (*func)(void *task_ctx, void *priv);
};

static u32 task_feature_num = 0;

static void svm_task_ctx_for_each_func(struct task_ctx *ctx, void *priv)
{
    struct svm_task_ctx_for_each_priv *t_priv = (struct svm_task_ctx_for_each_priv *)priv;
    struct svm_task_ctx *t_ctx = (struct svm_task_ctx *)task_ctx_priv(ctx);

    t_priv->func((void *)t_ctx, t_priv->priv);
}

void svm_task_ctx_for_each(u32 udevid, void *priv, void (*func)(void *task_ctx, void *priv))
{
    struct svm_dev_ctx *d_ctx = (struct svm_dev_ctx *)svm_dev_ctx_get(udevid);
    if (d_ctx != NULL) {
        struct svm_task_ctx_for_each_priv priv_data = {.priv = priv, .func = func};
        task_ctx_for_each_safe(d_ctx->domain, (void *)&priv_data, svm_task_ctx_for_each_func);
        svm_dev_ctx_put(d_ctx);
    }
}

static void svm_get_task_tgids_func(struct task_ctx *ctx, void *priv)
{
    struct task_tgids_num *tgids = (struct task_tgids_num *)priv;

    if (tgids->get_num < tgids->arr_num) {
        tgids->tgid[tgids->get_num] = ctx->tgid;
        tgids->get_num++;
    }
}

u32 svm_get_task_max_num(u32 udevid)
{
    struct svm_dev_ctx *d_ctx = (struct svm_dev_ctx *)svm_dev_ctx_get(udevid);
    u32 task_max_num = 0;

    if (d_ctx != NULL) {
        task_max_num = d_ctx->domain->task_max_num;
        svm_dev_ctx_put(d_ctx);
    }
    return task_max_num;
}

void svm_get_all_task_tgids(u32 udevid, int tgid[], u32 num)
{
    struct svm_dev_ctx *d_ctx = (struct svm_dev_ctx *)svm_dev_ctx_get(udevid);
    if (d_ctx != NULL) {
        struct task_tgids_num tgids = {0};
        tgids.tgid = tgid;
        tgids.arr_num = num;
        task_ctx_for_each_safe(d_ctx->domain, (void *)&tgids, svm_get_task_tgids_func);
        svm_dev_ctx_put(d_ctx);
    }
}

void *svm_get_task_mm(void *task_ctx)
{
    struct svm_task_ctx *t_ctx = (struct svm_task_ctx *)task_ctx;
    return (void *)t_ctx->mm;
}

int svm_get_task_tgid(void *task_ctx)
{
    struct svm_task_ctx *t_ctx = (struct svm_task_ctx *)task_ctx;
    return t_ctx->tgid;
}

u32 svm_task_obtain_feature_id(void)
{
    return task_feature_num++;
}

int svm_task_set_feature_priv(void *task_ctx, u32 feature_id, const char *feature_name, void *priv,
                              void (*release)(void *priv))
{
    struct svm_task_ctx *t_ctx = (struct svm_task_ctx *)task_ctx;

    if (feature_id >= task_feature_num) {
        return -EINVAL;
    }

    t_ctx->feature[feature_id].name = feature_name;
    t_ctx->feature[feature_id].valid = true;
    t_ctx->feature[feature_id].priv = priv;
    t_ctx->feature[feature_id].release = release;
    return 0;
}

void *svm_task_get_feature_priv(void *task_ctx, u32 feature_id)
{
    struct svm_task_ctx *t_ctx = (struct svm_task_ctx *)task_ctx;

    if ((feature_id >= task_feature_num) || (!t_ctx->feature[feature_id].valid)) {
        return NULL;
    }

    return t_ctx->feature[feature_id].priv;
}

void svm_task_set_feature_invalid(void *task_ctx, u32 feature_id)
{
    struct svm_task_ctx *t_ctx = (struct svm_task_ctx *)task_ctx;

    if (feature_id < task_feature_num) {
        t_ctx->feature[feature_id].valid = false;
    }
}

static void svm_task_release_check_feature(struct svm_task_ctx *t_ctx)
{
    u32 i;

    for (i = 0; i < task_feature_num; i++) {
        struct svm_task_feature *feature = &t_ctx->feature[i];
        if (feature->valid) {
            svm_warn("Feature not exist. (udevid=%u; tgid=%d; name=%s)\n", t_ctx->udevid, t_ctx->tgid, feature->name);
        }
    }
}

static void svm_task_clear_feature(struct svm_task_feature *feature)
{
    feature->name = NULL;
    feature->valid = false;
    feature->priv = NULL;
    feature->release = NULL;
}

static void svm_task_release_feature(struct svm_task_ctx *t_ctx)
{
    u32 i;

    for (i = 0; i < task_feature_num; i++) {
        struct svm_task_feature *feature = &t_ctx->feature[i];
        if (feature->release != NULL) {
            feature->release(feature->priv);
        }
        svm_task_clear_feature(feature);
    }
}

static struct svm_task_ctx *_svm_task_ctx_get(u32 udevid, int tgid)
{
    struct svm_dev_ctx *d_ctx = (struct svm_dev_ctx *)svm_dev_ctx_get(udevid);
    if (d_ctx != NULL) {
        struct svm_task_ctx *t_ctx = NULL;
        struct task_ctx *ctx = task_ctx_get(d_ctx->domain, tgid);
        if (ctx != NULL) {
            t_ctx = (struct svm_task_ctx *)task_ctx_priv(ctx);
        }

        svm_dev_ctx_put(d_ctx);
        return t_ctx;
    }

    return NULL;
}

void *svm_task_ctx_get(u32 udevid, int tgid)
{
    return (void *)_svm_task_ctx_get(udevid, tgid);
}

static void _svm_task_ctx_put(struct svm_task_ctx *t_ctx)
{
    task_ctx_put(t_ctx->ctx);
}

void svm_task_ctx_put(void *task_ctx)
{
    _svm_task_ctx_put((struct svm_task_ctx *)task_ctx);
}

int svm_get_task_start_time(u32 udevid, int tgid, struct task_start_time *start_time)
{
    struct svm_task_ctx *t_ctx = NULL;

    t_ctx = _svm_task_ctx_get(udevid, tgid);
    if (t_ctx != NULL) {
        *start_time = t_ctx->start_time;
        _svm_task_ctx_put(t_ctx);
        return 0;
    }

    return -ESRCH;
}

static void svm_show_task_ctx(struct svm_task_ctx *t_ctx, ka_seq_file_t *seq)
{
    ka_fs_seq_printf(seq, "    tgid %d task_id %u valid %u state %d\n", t_ctx->tgid, t_ctx->task_id, t_ctx->valid,
                     ka_base_atomic_read(&t_ctx->state));
}

static void _svm_show_task(u32 udevid, int tgid, ka_seq_file_t *seq)
{
    struct svm_task_ctx *t_ctx = _svm_task_ctx_get(udevid, tgid);
    if (t_ctx != NULL) {
        task_ctx_item_show(t_ctx->ctx, seq);
        svm_show_task_ctx(t_ctx, seq);
        _svm_task_ctx_put(t_ctx);
    }
}

static void svm_task_ctx_init(struct svm_task_ctx *t_ctx)
{
    t_ctx->mm = ka_task_get_current_mm();
    ka_mm_mmget(t_ctx->mm);
    t_ctx->exit_force = false;
    t_ctx->exit_abort = false;
    ka_wmb();
    t_ctx->valid = 1;
}

static void svm_task_ctx_uninit(struct svm_task_ctx *t_ctx)
{
    if (t_ctx->valid == 1) {
        t_ctx->valid = 0;
        ka_wmb();
        ka_mm_mmput(t_ctx->mm);
    }
}

static void svm_add_task_feature_fs(ka_proc_dir_entry_t *dev_entry, struct svm_task_ctx *t_ctx)
{
    t_ctx->entry = svm_proc_fs_add_task(t_ctx->udevid, dev_entry, t_ctx->tgid, t_ctx->task_id);
    if (t_ctx->entry != NULL) {
        u32 i;
        for (i = 0; i < task_feature_num; i++) {
            if (t_ctx->feature[i].name != NULL) {
                svm_proc_task_add_feature(t_ctx->udevid, t_ctx->tgid, t_ctx->entry, i, t_ctx->feature[i].name);
            }
        }
        svm_proc_task_add_feature(t_ctx->udevid, t_ctx->tgid, t_ctx->entry, i, "task");
    }
}

static void svm_del_task_feature_fs(struct svm_task_ctx *t_ctx)
{
    svm_proc_fs_del_task(t_ctx->entry);
}

static void svm_task_ctx_release(struct task_ctx *ctx)
{
    struct svm_task_ctx *t_ctx = (struct svm_task_ctx *)task_ctx_priv(ctx);

    svm_inst_trace("Release success. (udevid=%u; tgid=%d)\n", t_ctx->udevid, t_ctx->tgid);
    svm_task_release_check_feature(t_ctx);
    svm_task_release_feature(t_ctx);
    svm_del_task_feature_fs(t_ctx);
    svm_task_ctx_uninit(t_ctx);
    svm_vfree(t_ctx);
}

static int svm_task_ctx_create(struct svm_dev_ctx *d_ctx, int tgid, struct task_start_time *start_time)
{
    struct svm_task_ctx *t_ctx = NULL;
    unsigned long size = sizeof(*t_ctx) + task_feature_num * sizeof(struct svm_task_feature);
    int ret;

    t_ctx = svm_vzalloc(size);
    if (t_ctx == NULL) {
        svm_err("Vmalloc ctx fail. (size=%lu)\n", size);
        return -ENOMEM;
    }

    t_ctx->tgid = tgid;
    t_ctx->start_time = *start_time;
    t_ctx->task_id = d_ctx->cur_task_id++;
    t_ctx->udevid = d_ctx->udevid;
    t_ctx->d_ctx = d_ctx;
    ka_base_atomic_set(&t_ctx->state, SVM_TASK_STATE_RUNNING);
    svm_task_ctx_init(t_ctx);

    ret = task_ctx_create_ex(d_ctx->domain, tgid, t_ctx, svm_task_ctx_release, &t_ctx->ctx);
    if (ret != 0) {
        svm_err("Create fail. (ret=%d; tgid=%d)\n", ret, tgid);
        svm_vfree(t_ctx);
        return ret;
    }

    (void)apm_master_use(tgid, d_ctx->udevid);

    svm_inst_trace("Create success. (udevid=%u; tgid=%d)\n", d_ctx->udevid, tgid);

    return 0;
}

static void svm_task_ctx_destroy(struct svm_task_ctx *t_ctx)
{
    (void)apm_master_unuse(t_ctx->tgid, t_ctx->udevid);
    task_ctx_destroy(t_ctx->ctx); /* task_ctx_create */
}

static int svm_dev_add_task(struct svm_dev_ctx *d_ctx, int tgid, struct task_start_time *start_time)
{
    struct task_ctx_domain *domain = d_ctx->domain;
    struct task_ctx *ctx = NULL;
    int ret;

    ka_task_mutex_lock(&domain->mutex);
    ctx = task_ctx_get(domain, tgid);
    if (ctx == NULL) {
        ret = svm_task_ctx_create(d_ctx, tgid, start_time);
    } else {
        task_ctx_put(ctx);
        ret = -EEXIST;
    }
    ka_task_mutex_unlock(&domain->mutex);

    return ret;
}

static void svm_dev_del_task(struct svm_dev_ctx *d_ctx, int tgid)
{
    struct task_ctx_domain *domain = d_ctx->domain;
    struct task_ctx *ctx = NULL;

    ka_task_mutex_lock(&domain->mutex);
    ctx = task_ctx_get(domain, tgid);
    if (ctx != NULL) {
        task_ctx_put(ctx);
        svm_task_ctx_destroy((struct svm_task_ctx *)task_ctx_priv(ctx));
    }
    ka_task_mutex_unlock(&domain->mutex);
}

int svm_add_task(u32 udevid, int tgid, struct task_start_time *start_time)
{
    int ret;
    struct svm_dev_ctx *d_ctx = (struct svm_dev_ctx *)svm_dev_ctx_get(udevid);
    if (d_ctx == NULL) {
        svm_err("Invalid dev. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = svm_dev_add_task(d_ctx, tgid, start_time);
    if (ret == 0) {
        struct svm_task_ctx *t_ctx = _svm_task_ctx_get(udevid, tgid);
        ret = module_feature_auto_init_task(udevid, tgid, &t_ctx->start_time);
        if (ret != 0) {
            svm_dev_del_task(d_ctx, tgid);
            svm_err("Feature init failed. (udevid=%u; tgid=%d)\n", udevid, tgid);
        } else {
            svm_add_task_feature_fs(d_ctx->entry, t_ctx);
            svm_inst_trace("Add task success. (udevid=%u; tgid=%d)\n", udevid, tgid);
        }
        _svm_task_ctx_put(t_ctx);
    } else {
        svm_err("Add task failed. (ret=%d; udevid=%u; tgid=%d)\n", ret, udevid, tgid);
    }

    svm_dev_ctx_put(d_ctx);

    return ret;
}

static bool svm_task_go_apm_recycle(struct svm_task_ctx *t_ctx)
{
    if (ka_base_atomic_read(&t_ctx->state) == SVM_TASK_STATE_APM_RECYCLE) {
        return true;
    }

    if (ka_base_atomic_cmpxchg(&t_ctx->state, SVM_TASK_STATE_RUNNING, SVM_TASK_STATE_APM_RECYCLE) ==
        SVM_TASK_STATE_RUNNING) {
        return true;
    }

    return false;
}

static void svm_task_set_exit_force(struct svm_task_ctx *t_ctx, bool exit_force)
{
    t_ctx->exit_force = exit_force;
}

bool svm_task_is_exit_force(void *task_ctx)
{
    struct svm_task_ctx *t_ctx = (struct svm_task_ctx *)task_ctx;

    if (ka_base_atomic_read(&t_ctx->state) == SVM_TASK_STATE_APM_RECYCLE) {
        return t_ctx->exit_force;
    }

    return true;
}

static void svm_task_set_exit_abort_flag(struct svm_task_ctx *t_ctx, bool exit_abort)
{
    t_ctx->exit_abort = exit_abort;
}

void svm_task_set_exit_abort(void *task_ctx)
{
    svm_task_set_exit_abort_flag((struct svm_task_ctx *)task_ctx, true);
}

bool svm_task_is_exit_abort(void *task_ctx)
{
    struct svm_task_ctx *t_ctx = (struct svm_task_ctx *)task_ctx;
    return t_ctx->exit_abort;
}

bool svm_task_is_exiting(u32 udevid, int tgid)
{
    struct svm_task_ctx *t_ctx = NULL;
    bool is_exiting = true;

    t_ctx = _svm_task_ctx_get(udevid, tgid);
    if (t_ctx != NULL) {
        is_exiting = (ka_base_atomic_read(&t_ctx->state) != SVM_TASK_STATE_RUNNING);
        _svm_task_ctx_put(t_ctx);
    }

    return is_exiting;
}

static int _svm_del_task(u32 udevid, int tgid, struct task_start_time *start_time)
{
    struct svm_task_ctx *t_ctx = _svm_task_ctx_get(udevid, tgid);
    if (t_ctx == NULL) {
        return -EINVAL;
    }

    if (!ka_system_timespec_equal(start_time->time, t_ctx->start_time.time)) {
        _svm_task_ctx_put(t_ctx);
        svm_err("Task start time not equal. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    if (ka_base_atomic_cmpxchg(&t_ctx->state, SVM_TASK_STATE_RUNNING, SVM_TASK_STATE_SELF_RECYCLE) !=
        SVM_TASK_STATE_RUNNING) {
        svm_warn("Task can't go self recycle. (udevid=%u; tgid=%d; state=%d)\n", udevid, tgid,
                 ka_base_atomic_read(&t_ctx->state));
        _svm_task_ctx_put(t_ctx);
        return 0;
    }

    module_feature_auto_uninit_task(udevid, tgid, &t_ctx->start_time);
    svm_dev_del_task(t_ctx->d_ctx, tgid);
    _svm_task_ctx_put(t_ctx);
    return 0;
}

/*
    There are three ways to destroy a process context:
    1. fd release handle(svm_del_task) and task is unbinded from APM or task exit, destroy the context.
    2. task exit handle(svm_del_task) and task is binded from APM, the task context is destroyed in the notify handle
       of the amp(svm_task_exit_notifier). (If the context is used by other features, the notify function of the APM
       cannot be called back. In this case, APM will retried.)
    3. Destroy the device(svm_task_ctx_recycle).  forcibly destroy the device context,
       assume that the device occupation by other modules has been cleared.
*/
int svm_del_task(u32 udevid, int tgid, struct task_start_time *start_time)
{
    if (task_is_exit(tgid, start_time) && apm_notify_task_exit(tgid, start_time)) {
        svm_warn("Task exit use apm recycle. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return 0;
    }

    return _svm_del_task(udevid, tgid, start_time);
}

void svm_show_task(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    if (feature_id == (int)task_feature_num) {
        _svm_show_task(udevid, tgid, seq);
    }
}
DECLAER_FEATURE_AUTO_SHOW_TASK(svm_show_task, FEATURE_LOADER_STAGE_9);

void svm_task_ctx_recycle(struct task_ctx *ctx, void *priv)
{
    struct svm_task_ctx *t_ctx = (struct svm_task_ctx *)task_ctx_priv(ctx);
    int ret = _svm_del_task(t_ctx->udevid, t_ctx->tgid, &t_ctx->start_time);
    svm_warn("Recycle task. (name=%s; tgid=%d; ret=%d)\n", ctx->name, ctx->tgid, ret);
}

static int svm_destroy_task_from_all_dev(int tgid, bool exit_force)
{
    u32 dev_num = uda_get_udev_max_num();
    u32 udevid;
    int ret = 0;

    for (udevid = 0; udevid < dev_num; udevid++) {
        struct svm_task_ctx *t_ctx = _svm_task_ctx_get(udevid, tgid);
        if (t_ctx == NULL) {
            continue;
        }

        if (svm_task_go_apm_recycle(t_ctx)) {
            svm_task_set_exit_force(t_ctx, exit_force);
            svm_task_set_exit_abort_flag(t_ctx, false);

            /* call svm all features uninit */
            module_feature_auto_uninit_task(udevid, tgid, &t_ctx->start_time);
            if (svm_task_is_exit_abort((void *)t_ctx)) {
                ret = -EBUSY;
            } else {
                svm_dev_del_task(t_ctx->d_ctx, tgid);
                svm_info("Apm Recycle task. (udevid=%u; tgid=%d)\n", udevid, tgid);
            }
        }
        _svm_task_ctx_put(t_ctx);
    }

    return ret;
}

/* role 0 master 1 slave */
static int svm_task_exit_notifier(int role, int tgid, int stage, bool exit_force)
{
    int ret = KA_NOTIFY_OK;

    svm_debug("Exit notifier in. (role=%d; tgid=%d; stage=%d; exit_force=%d)\n", role, tgid, stage, exit_force);

    ret = ka_dfx_notifier_from_errno(svm_destroy_task_from_all_dev(tgid, exit_force));
    if (ret == 0) {
        svm_info("Exit notifier success. (role=%d; tgid=%d; exit_force=%d)\n", role, tgid, exit_force);
    }

    return ret;
}

static int svm_task_master_exit_notifier(ka_notifier_block_t *self, unsigned long val, void *data)
{
    int stage = apm_get_exit_stage(val);
    if (stage == APM_STAGE_RECYCLE_RES) {
        return svm_task_exit_notifier(0, apm_get_exit_tgid(val), stage, apm_get_exit_force_flag(val));
    }

    return KA_NOTIFY_OK;
}

static int svm_task_slave_exit_notifier(ka_notifier_block_t *self, unsigned long val, void *data)
{
    int stage = apm_get_exit_stage(val);
    if (stage == APM_STAGE_POST_RECYCLE_RES) {
        return svm_task_exit_notifier(1, apm_get_exit_tgid(val), stage, apm_get_exit_force_flag(val));
    }

    return KA_NOTIFY_OK;
}

static ka_notifier_block_t svm_master_task_exit_nb = {
    .notifier_call = svm_task_master_exit_notifier,
    .priority = APM_EXIT_NOTIFIY_PRI_SVM,
};

static ka_notifier_block_t svm_agent_task_exit_nb = {
    .notifier_call = svm_task_slave_exit_notifier,
    .priority = APM_EXIT_NOTIFIY_PRI_SVM,
};

int svm_task_init(void)
{
    return apm_task_exit_register(&svm_master_task_exit_nb, &svm_agent_task_exit_nb);
}
DECLAER_FEATURE_AUTO_INIT(svm_task_init, FEATURE_LOADER_STAGE_9);

void svm_task_uninit(void)
{
    apm_task_exit_unregister(&svm_master_task_exit_nb, &svm_agent_task_exit_nb);
}
DECLAER_FEATURE_AUTO_UNINIT(svm_task_uninit, FEATURE_LOADER_STAGE_9);
