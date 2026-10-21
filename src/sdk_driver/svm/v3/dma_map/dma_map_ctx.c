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
#include "pbl_uda.h"
#include "dpa_kernel_interface.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "framework_task.h"
#include "dma_map_core.h"
#include "dma_map_ctx.h"

static u32 dma_map_task_feature_id;

struct dma_map_ctx *dma_map_ctx_get(u32 udevid, int tgid)
{
    struct dma_map_ctx *ctx = NULL;
    void *task_ctx = NULL;

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        return NULL;
    }

    ctx = (struct dma_map_ctx *)svm_task_get_feature_priv(task_ctx, dma_map_task_feature_id);
    if (ctx == NULL) {
        svm_task_ctx_put(task_ctx);
    }

    return ctx;
}

void dma_map_ctx_put(struct dma_map_ctx *ctx)
{
    svm_task_ctx_put(ctx->task_ctx);
}

static void dma_map_ctx_init(u32 udevid, struct dma_map_ctx *ctx)
{
    ka_task_rwlock_init(&ctx->lock);
    range_rbtree_init(&ctx->range_tree);
}

static void dma_map_ctx_release(void *priv)
{
    struct dma_map_ctx *ctx = (struct dma_map_ctx *)priv;
    svm_kfree(ctx);
}

int dma_map_init_task(u32 udevid, int tgid, void *start_time)
{
    struct dma_map_ctx *ctx = NULL;
    void *task_ctx = NULL;
    int ret;

    ctx = (struct dma_map_ctx *)svm_kzalloc(sizeof(*ctx), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ctx == NULL) {
        svm_err("No mem. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ENOMEM;
    }

    ctx->udevid = udevid;
    ctx->tgid = tgid;
    dma_map_ctx_init(udevid, ctx);

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        svm_kfree(ctx);
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = svm_task_set_feature_priv(task_ctx, dma_map_task_feature_id, "dma_map", (void *)ctx, dma_map_ctx_release);
    if (ret != 0) {
        svm_task_ctx_put(task_ctx);
        svm_kfree(ctx);
        return ret;
    }

    ctx->task_ctx = task_ctx;

    svm_inst_trace("Init task. (udevid=%u; tgid=%d)\n", udevid, tgid);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_TASK(dma_map_init_task, FEATURE_LOADER_STAGE_5);

static void dma_map_destroy_task(struct dma_map_ctx *ctx)
{
    svm_inst_trace("Destroy task. (udevid=%u; tgid=%d)\n", ctx->udevid, ctx->tgid);
    svm_task_set_feature_invalid(ctx->task_ctx, dma_map_task_feature_id);
    dma_map_addr_recycle(ctx);
    svm_task_ctx_put(ctx->task_ctx); /* with init pair */
}

void dma_map_uninit_task(u32 udevid, int tgid, void *start_time)
{
    struct dma_map_ctx *ctx = NULL;

    ctx = dma_map_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        return;
    }

    dma_map_ctx_put(ctx);

    if (!svm_task_is_exit_abort(ctx->task_ctx)) {
        dma_map_destroy_task(ctx);
        svm_inst_trace("Uninit task. (udevid=%u; tgid=%d)\n", udevid, tgid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(dma_map_uninit_task, FEATURE_LOADER_STAGE_5);

void dma_map_show_task(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    struct dma_map_ctx *ctx = NULL;

    if (feature_id != dma_map_task_feature_id) {
        return;
    }

    ctx = dma_map_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return;
    }

    dma_map_addr_show(ctx, seq);
    dma_map_ctx_put(ctx);
}
DECLAER_FEATURE_AUTO_SHOW_TASK(dma_map_show_task, FEATURE_LOADER_STAGE_5);

int svm_dma_map_init(void)
{
    dma_map_task_feature_id = svm_task_obtain_feature_id();
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_dma_map_init, FEATURE_LOADER_STAGE_5);

void svm_dma_map_uninit(void)
{}
DECLAER_FEATURE_AUTO_UNINIT(svm_dma_map_uninit, FEATURE_LOADER_STAGE_5);