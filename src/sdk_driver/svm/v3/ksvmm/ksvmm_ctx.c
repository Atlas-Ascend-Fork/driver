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
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_sched_pub.h"

#include "pbl_feature_loader.h"
#include "pbl_task_ctx.h"
#include "pbl_uda.h"
#include "dpa_kernel_interface.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "framework_task.h"
#include "ksvmm_core.h"
#include "ksvmm_ctx.h"

static u32 ksvmm_feature_id;

struct ksvmm_ctx *ksvmm_ctx_get(u32 udevid, int tgid)
{
    struct ksvmm_ctx *ksvmm_ctx = NULL;
    void *task_ctx = NULL;

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        return NULL;
    }

    ksvmm_ctx = (struct ksvmm_ctx *)svm_task_get_feature_priv(task_ctx, ksvmm_feature_id);
    if (ksvmm_ctx == NULL) {
        svm_task_ctx_put(task_ctx);
    }

    return ksvmm_ctx;
}

void ksvmm_ctx_put(struct ksvmm_ctx *ksvmm_ctx)
{
    svm_task_ctx_put(ksvmm_ctx->task_ctx);
}

static void ksvmm_ctx_init(struct ksvmm_ctx *ksvmm_ctx)
{
    ka_task_rwlock_init(&ksvmm_ctx->lock);
    range_rbtree_init(&ksvmm_ctx->range_tree);
}

static void ksvmm_ctx_release(void *priv)
{
    struct ksvmm_ctx *ctx = (struct ksvmm_ctx *)priv;
    svm_kvfree(ctx);
}

int ksvmm_init_task(u32 udevid, int tgid, void *start_time)
{
    struct ksvmm_ctx *ksvmm_ctx = NULL;
    void *task_ctx = NULL;
    int ret;

    ksvmm_ctx = (struct ksvmm_ctx *)svm_kvzalloc(sizeof(struct ksvmm_ctx), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ksvmm_ctx == NULL) {
        svm_err("No mem. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ENOMEM;
    }

    ksvmm_ctx->udevid = udevid;
    ksvmm_ctx->tgid = tgid;
    ksvmm_ctx_init(ksvmm_ctx);

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        svm_kvfree(ksvmm_ctx);
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = svm_task_set_feature_priv(task_ctx, ksvmm_feature_id, "ksvmm", (void *)ksvmm_ctx, ksvmm_ctx_release);
    if (ret != 0) {
        svm_task_ctx_put(task_ctx);
        svm_kvfree(ksvmm_ctx);
        return ret;
    }

    ksvmm_ctx->task_ctx = task_ctx;

    svm_inst_trace("Init task. (udevid=%u; tgid=%d)\n", udevid, tgid);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_TASK(ksvmm_init_task, FEATURE_LOADER_STAGE_4);

static void ksvmm_destroy_task(struct ksvmm_ctx *ksvmm_ctx)
{
    svm_inst_trace("Destroy task. (udevid=%u; tgid=%d)\n", ksvmm_ctx->udevid, ksvmm_ctx->tgid);
    svm_task_set_feature_invalid(ksvmm_ctx->task_ctx, ksvmm_feature_id);
    ksvmm_seg_recycle(ksvmm_ctx);
    svm_task_ctx_put(ksvmm_ctx->task_ctx); /* with init pair */
}

void ksvmm_uninit_task(u32 udevid, int tgid, void *start_time)
{
    struct ksvmm_ctx *ksvmm_ctx = NULL;

    ksvmm_ctx = ksvmm_ctx_get(udevid, tgid);
    if (ksvmm_ctx == NULL) {
        return;
    }

    ksvmm_ctx_put(ksvmm_ctx);

    if (!svm_task_is_exit_abort(ksvmm_ctx->task_ctx)) {
        ksvmm_destroy_task(ksvmm_ctx);
        svm_inst_trace("Uninit task. (udevid=%u; tgid=%d)\n", udevid, tgid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(ksvmm_uninit_task, FEATURE_LOADER_STAGE_4);

void ksvmm_show_task(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    struct ksvmm_ctx *ksvmm_ctx = NULL;

    if (feature_id != (int)ksvmm_feature_id) {
        return;
    }

    ksvmm_ctx = ksvmm_ctx_get(udevid, tgid);
    if (ksvmm_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return;
    }

    ksvmm_seg_show(ksvmm_ctx, seq);

    ksvmm_ctx_put(ksvmm_ctx);
}
DECLAER_FEATURE_AUTO_SHOW_TASK(ksvmm_show_task, FEATURE_LOADER_STAGE_4);

int ksvmm_init(void)
{
    ksvmm_feature_id = svm_task_obtain_feature_id();
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(ksvmm_init, FEATURE_LOADER_STAGE_4);

void ksvmm_uninit(void)
{}
DECLAER_FEATURE_AUTO_UNINIT(ksvmm_uninit, FEATURE_LOADER_STAGE_4);