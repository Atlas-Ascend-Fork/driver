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
#include "ka_task_pub.h"
#include "ka_dfx_pub.h"
#include "ka_common_pub.h"
#include "ka_system_pub.h"
#include "ka_sched_pub.h"

#include "pbl_feature_loader.h"
#include "pbl_task_ctx.h"
#include "dpa_kernel_interface.h"
#include "pbl_uda.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "framework_task.h"
#include "smp_core.h"
#include "smp_ctx.h"

static u32 smp_feature_id;

struct smp_ctx *smp_ctx_get(u32 udevid, int tgid)
{
    struct smp_ctx *smp_ctx = NULL;
    void *task_ctx = NULL;

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        return NULL;
    }

    smp_ctx = (struct smp_ctx *)svm_task_get_feature_priv(task_ctx, smp_feature_id);
    if (smp_ctx == NULL) {
        svm_task_ctx_put(task_ctx);
    }

    return smp_ctx;
}

void smp_ctx_put(struct smp_ctx *smp_ctx)
{
    svm_task_ctx_put(smp_ctx->task_ctx);
}

static void smp_ctx_init(struct smp_ctx *smp_ctx)
{
    ka_task_rwlock_init(&smp_ctx->lock);
    range_rbtree_init(&smp_ctx->range_tree);
}

static void smp_ctx_release(void *priv)
{
    struct smp_ctx *ctx = (struct smp_ctx *)priv;
    svm_kfree(ctx);
}

int smp_init_task(u32 udevid, int tgid, void *start_time)
{
    struct smp_ctx *smp_ctx = NULL;
    void *task_ctx = NULL;
    int ret;

    smp_ctx = (struct smp_ctx *)svm_kzalloc(sizeof(*smp_ctx), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (smp_ctx == NULL) {
        svm_err("No mem. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ENOMEM;
    }

    smp_ctx->udevid = udevid;
    smp_ctx->tgid = tgid;
    smp_ctx_init(smp_ctx);

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        svm_kfree(smp_ctx);
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = svm_task_set_feature_priv(task_ctx, smp_feature_id, "smp", (void *)smp_ctx, smp_ctx_release);
    if (ret != 0) {
        svm_task_ctx_put(task_ctx);
        svm_kfree(smp_ctx);
        return ret;
    }

    smp_ctx->task_ctx = task_ctx;

    svm_inst_trace("Init task. (udevid=%u; tgid=%d)\n", udevid, tgid);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_TASK(smp_init_task, FEATURE_LOADER_STAGE_4);

static void smp_destroy_task(struct smp_ctx *smp_ctx)
{
    svm_inst_trace("Destroy task. (udevid=%u; tgid=%d)\n", smp_ctx->udevid, smp_ctx->tgid);
    svm_task_set_feature_invalid(smp_ctx->task_ctx, smp_feature_id);
    (void)smp_mem_recycle(smp_ctx, true);
    svm_task_ctx_put(smp_ctx->task_ctx); /* with init pair */
}

/*  when a resource is occupied by others, we will set abort the uninit process */
void smp_uninit_task(u32 udevid, int tgid, void *start_time)
{
    struct smp_ctx *smp_ctx = NULL;

    smp_ctx = smp_ctx_get(udevid, tgid);
    if (smp_ctx == NULL) {
        return;
    }

    smp_ctx_put(smp_ctx);

    if (!svm_task_is_exit_abort(smp_ctx->task_ctx)) {
        if ((!svm_task_is_exit_force(smp_ctx->task_ctx)) && (smp_mem_recycle(smp_ctx, false) == -EBUSY)) {
            svm_task_set_exit_abort(smp_ctx->task_ctx);
            return;
        }

        smp_destroy_task(smp_ctx);
        svm_inst_trace("Uninit task. (udevid=%u; tgid=%d)\n", udevid, tgid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(smp_uninit_task, FEATURE_LOADER_STAGE_4);

void smp_show_task(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    struct smp_ctx *smp_ctx = NULL;

    if (feature_id != (int)smp_feature_id) {
        return;
    }

    smp_ctx = smp_ctx_get(udevid, tgid);
    if (smp_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return;
    }

    smp_mem_show(smp_ctx, seq);

    smp_ctx_put(smp_ctx);
}
DECLAER_FEATURE_AUTO_SHOW_TASK(smp_show_task, FEATURE_LOADER_STAGE_4);

int svm_smp_init(void)
{
    smp_feature_id = svm_task_obtain_feature_id();
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_smp_init, FEATURE_LOADER_STAGE_4);

void svm_smp_uninit(void)
{}
DECLAER_FEATURE_AUTO_UNINIT(svm_smp_uninit, FEATURE_LOADER_STAGE_4);
