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

#include "pbl_range_rbtree.h"

#include "svm_kern_log.h"
#include "pma_ub.h"
#include "pma_ub_seg.h"
#include "pma_ub_ctx.h"
#include "pma_ub_core.h"

int pma_ub_register_seg(u32 udevid, int tgid, u64 start, u64 size, u32 token_id)
{
    struct pma_ub_ctx *ctx = NULL;
    int ret;

    ctx = pma_ub_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Get pma_ub_ctx failed, task may have exited. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ESRCH;
    }

    ret = pma_ub_seg_add(&ctx->seg_mng, start, size, token_id);
    pma_ub_ctx_put(ctx);

    return ret;
}

int pma_ub_unregister_seg(u32 udevid, int tgid, u64 start, u64 size)
{
    struct pma_ub_ctx *ctx = NULL;
    int ret;

    ctx = pma_ub_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Get pma_ub_ctx failed, task may have exited. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ESRCH;
    }

    ret = pma_ub_seg_del(&ctx->seg_mng, start, size);
    pma_ub_ctx_put(ctx);

    return ret;
}

int pma_ub_get_register_seg_info(u32 udevid, int tgid, u64 va, u64 *start, u64 *size, u32 *token_id)
{
    struct pma_ub_ctx *ctx = NULL;
    int ret;

    ctx = pma_ub_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Get pma_ub_ctx failed, task may have exited. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ESRCH;
    }

    ret = pma_ub_seg_query(&ctx->seg_mng, va, start, size, token_id);
    pma_ub_ctx_put(ctx);

    return ret;
}

int pma_ub_acquire_seg(u32 udevid, int tgid, u64 va, u64 size, int (*invalidate)(u64 invalidate_tag),
                       u64 invalidate_tag, u32 *token_id)
{
    struct pma_ub_ctx *ctx = NULL;
    int ret;

    ctx = pma_ub_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Get pma_ub_ctx failed, task may have exited. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ESRCH;
    }

    ret = pma_ub_seg_acquire(&ctx->seg_mng, va, size, invalidate, invalidate_tag, token_id);
    pma_ub_ctx_put(ctx);

    return ret;
}

int pma_ub_release_seg(u32 udevid, int tgid, u64 va, u64 size)
{
    struct pma_ub_ctx *ctx = NULL;
    int ret;

    ctx = pma_ub_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_warn("Get pma_ub_ctx check, task may have exited. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ESRCH;
    }

    ret = pma_ub_seg_release(&ctx->seg_mng, va, size);
    pma_ub_ctx_put(ctx);

    return ret;
}

void pma_ub_mem_recycle_notify(u32 udevid, int tgid, u64 va, u64 size)
{
    struct pma_ub_ctx *ctx = pma_ub_ctx_get(udevid, tgid);

    if (ctx == NULL) {
        return;
    }

    pma_ub_seg_recycle(&ctx->seg_mng, va, size);
    pma_ub_ctx_put(ctx);
}
