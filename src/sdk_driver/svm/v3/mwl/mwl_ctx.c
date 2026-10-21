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
#include "ka_common_pub.h"
#include "ka_dfx_pub.h"
#include "ka_system_pub.h"
#include "ka_sched_pub.h"

#include "pbl_feature_loader.h"
#include "pbl_task_ctx.h"
#include "dpa_kernel_interface.h"
#include "pbl_uda.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "framework_task.h"
#include "mwl_core.h"
#include "mwl_ctx.h"

static u32 mwl_feature_id;

struct mwl_ctx *mwl_ctx_get(u32 udevid, int tgid)
{
    struct mwl_ctx *mwl_ctx = NULL;
    void *task_ctx = NULL;

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        return NULL;
    }

    mwl_ctx = (struct mwl_ctx *)svm_task_get_feature_priv(task_ctx, mwl_feature_id);
    if (mwl_ctx == NULL) {
        svm_task_ctx_put(task_ctx);
    }

    return mwl_ctx;
}

void mwl_ctx_put(struct mwl_ctx *mwl_ctx)
{
    svm_task_ctx_put(mwl_ctx->task_ctx);
}

static void mwl_ctx_init(struct mwl_ctx *mwl_ctx)
{
    ka_task_init_rwsem(&mwl_ctx->rwsem);
    range_rbtree_init(&mwl_ctx->id_range_tree);
    range_rbtree_init(&mwl_ctx->va_range_tree);
    return;
}

static void mwl_ctx_release(void *priv)
{
    struct mwl_ctx *ctx = (struct mwl_ctx *)priv;
    svm_kfree(ctx);
}

int mwl_init_task(u32 udevid, int tgid, void *start_time)
{
    struct mwl_ctx *mwl_ctx = NULL;
    void *task_ctx = NULL;
    int ret;

    mwl_ctx = (struct mwl_ctx *)svm_kzalloc(sizeof(*mwl_ctx), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (mwl_ctx == NULL) {
        svm_err("No mem. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ENOMEM;
    }

    mwl_ctx->udevid = udevid;
    mwl_ctx->tgid = tgid;
    mwl_ctx_init(mwl_ctx);

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        svm_kfree(mwl_ctx);
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = svm_task_set_feature_priv(task_ctx, mwl_feature_id, "mwl", (void *)mwl_ctx, mwl_ctx_release);
    if (ret != 0) {
        svm_task_ctx_put(task_ctx);
        svm_kfree(mwl_ctx);
        return ret;
    }

    mwl_ctx->task_ctx = task_ctx;

    svm_inst_trace("Init task. (udevid=%u; tgid=%d)\n", udevid, tgid);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_TASK(mwl_init_task, FEATURE_LOADER_STAGE_4);

static void mwl_destroy_task(struct mwl_ctx *mwl_ctx)
{
    svm_inst_trace("Destroy task. (udevid=%u; tgid=%d)\n", mwl_ctx->udevid, mwl_ctx->tgid);
    svm_task_set_feature_invalid(mwl_ctx->task_ctx, mwl_feature_id);
    mwl_mem_recycle(mwl_ctx);
    svm_task_ctx_put(mwl_ctx->task_ctx); /* with init pair */
}

void mwl_uninit_task(u32 udevid, int tgid, void *start_time)
{
    struct mwl_ctx *mwl_ctx = NULL;

    mwl_ctx = mwl_ctx_get(udevid, tgid);
    if (mwl_ctx == NULL) {
        return;
    }

    mwl_ctx_put(mwl_ctx);

    if (!svm_task_is_exit_abort(mwl_ctx->task_ctx)) {
        mwl_destroy_task(mwl_ctx);
        svm_inst_trace("Uninit task. (udevid=%u; tgid=%d)\n", udevid, tgid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(mwl_uninit_task, FEATURE_LOADER_STAGE_4);

void mwl_show_task(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    struct mwl_ctx *mwl_ctx = NULL;

    if (feature_id != (int)mwl_feature_id) {
        return;
    }

    mwl_ctx = mwl_ctx_get(udevid, tgid);
    if (mwl_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return;
    }

    mwl_mem_show(mwl_ctx, seq);

    mwl_ctx_put(mwl_ctx);
}
DECLAER_FEATURE_AUTO_SHOW_TASK(mwl_show_task, FEATURE_LOADER_STAGE_4);

int svm_mwl_init(void)
{
    mwl_feature_id = svm_task_obtain_feature_id();
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_mwl_init, FEATURE_LOADER_STAGE_4);

void svm_mwl_uninit(void)
{}
DECLAER_FEATURE_AUTO_UNINIT(svm_mwl_uninit, FEATURE_LOADER_STAGE_4);
