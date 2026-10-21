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
#include "casm_src.h"
#include "casm_dst.h"
#include "casm_ctx.h"

static u32 casm_task_feature_id;

struct casm_ctx *casm_ctx_get(u32 udevid, int tgid)
{
    struct casm_ctx *ctx = NULL;
    void *task_ctx = NULL;

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        return NULL;
    }

    ctx = (struct casm_ctx *)svm_task_get_feature_priv(task_ctx, casm_task_feature_id);
    if (ctx == NULL) {
        svm_task_ctx_put(task_ctx);
    }

    return ctx;
}

void casm_ctx_put(struct casm_ctx *ctx)
{
    svm_task_ctx_put(ctx->task_ctx);
}

static void casm_ctx_init(u32 udevid, struct casm_ctx *ctx)
{
    casm_src_ctx_init(udevid, &ctx->src_ctx);
    casm_dst_ctx_init(udevid, &ctx->dst_ctx);
}

static void casm_ctx_release(void *priv)
{
    struct casm_ctx *ctx = (struct casm_ctx *)priv;
    svm_kfree(ctx);
}

int casm_init_task(u32 udevid, int tgid, void *start_time)
{
    struct casm_ctx *ctx = NULL;
    void *task_ctx = NULL;
    int ret;

    ctx = (struct casm_ctx *)svm_kzalloc(sizeof(*ctx), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ctx == NULL) {
        svm_err("No mem. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ENOMEM;
    }

    ctx->udevid = udevid;
    ctx->tgid = tgid;
    casm_ctx_init(udevid, ctx);

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        svm_kfree(ctx);
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = svm_task_set_feature_priv(task_ctx, casm_task_feature_id, "casm", (void *)ctx, casm_ctx_release);
    if (ret != 0) {
        svm_task_ctx_put(task_ctx);
        svm_kfree(ctx);
        return ret;
    }

    ctx->task_ctx = task_ctx;

    svm_inst_trace("Init task. (udevid=%u; tgid=%d)\n", udevid, tgid);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_TASK(casm_init_task, FEATURE_LOADER_STAGE_5);

static void casm_destroy_task(struct casm_ctx *ctx)
{
    svm_inst_trace("Destroy task. (udevid=%u; tgid=%d)\n", ctx->udevid, ctx->tgid);
    casm_dst_ctx_uninit(ctx->udevid, ctx->tgid, &ctx->dst_ctx);
    casm_src_ctx_uninit(ctx->udevid, &ctx->src_ctx);
    svm_task_set_feature_invalid(ctx->task_ctx, casm_task_feature_id);
    svm_task_ctx_put(ctx->task_ctx); /* with init pair */
}

void casm_uninit_task(u32 udevid, int tgid, void *start_time)
{
    struct casm_ctx *ctx = NULL;

    ctx = casm_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        return;
    }

    casm_ctx_put(ctx);

    if (!svm_task_is_exit_abort(ctx->task_ctx)) {
        casm_destroy_task(ctx);
        svm_inst_trace("Uninit task. (udevid=%u; tgid=%d)\n", udevid, tgid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(casm_uninit_task, FEATURE_LOADER_STAGE_5);

void casm_show_task(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    struct casm_ctx *ctx = NULL;

    if (feature_id != (int)casm_task_feature_id) {
        return;
    }

    ctx = casm_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return;
    }

    casm_src_ctx_show(&ctx->src_ctx, seq);
    casm_dst_ctx_show(&ctx->dst_ctx, seq);

    casm_ctx_put(ctx);
}
DECLAER_FEATURE_AUTO_SHOW_TASK(casm_show_task, FEATURE_LOADER_STAGE_5);

int svm_casm_init(void)
{
    casm_task_feature_id = svm_task_obtain_feature_id();
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_casm_init, FEATURE_LOADER_STAGE_5);

void svm_casm_uninit(void)
{}
DECLAER_FEATURE_AUTO_UNINIT(svm_casm_uninit, FEATURE_LOADER_STAGE_5);
