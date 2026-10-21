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

#include "pbl_uda.h"
#include "pbl_runenv_config.h"
#include "pbl_feature_loader.h"
#include "ka_sched_pub.h"

#include "framework_dev.h"
#include "svm_kern_log.h"
#include "svm_slab.h"
#include "svm_dev_topology.h"
#include "smm_kernel.h"
#include "smm_ctx.h"

static u32 smm_ops_feature_id;

u32 smm_get_src_max_udev_num(void)
{
    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        return uda_get_udev_max_num();
    } else {
        return uda_get_remote_udev_max_num();
    }
}

struct smm_ctx *smm_ctx_get(u32 udevid)
{
    struct smm_ctx *ctx = NULL;
    void *dev_ctx = NULL;

    dev_ctx = svm_dev_ctx_get(udevid);
    if (dev_ctx == NULL) {
        svm_err("No dev. (udevid=%u)\n", udevid);
        return NULL;
    }

    ctx = (struct smm_ctx *)svm_dev_get_feature_priv(dev_ctx, smm_ops_feature_id);
    if (ctx == NULL) {
        svm_err("Smm get priv failed. (feature_id=%u)\n", smm_ops_feature_id);
        svm_dev_ctx_put(dev_ctx);
        return NULL;
    }

    return ctx;
}

void smm_ctx_put(struct smm_ctx *ctx)
{
    svm_dev_ctx_put(ctx->dev_ctx);
}

static int smm_ops_para_check(struct svm_global_va *src_info, struct svm_pa_seg pa_seg[], u64 *seg_num)
{
    u32 src_max_udev_num = smm_get_src_max_udev_num();
    if ((src_info->udevid >= src_max_udev_num) || (pa_seg == NULL) || (seg_num == NULL) || (*seg_num == 0)) {
        svm_err("Smm pa handle input check failed. (src_udev=%u; pa_seg_is_null=%u; seg_num_is_null=%u)\n",
                src_info->udevid, (pa_seg == NULL), (seg_num == NULL));
        return -EINVAL;
    }

    return 0;
}

static int smm_cross_server_pa_get(struct smm_ctx *ctx, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[],
                                   u64 *seg_num, u64 flag)
{
    struct smm_ops *ops = NULL;
    u32 udevid = ctx->udevid;
    int ret;

    ops = ctx->cs_ops;
    if (ops == NULL) {
        svm_err("Smm cs ops not register. (udev=%u; src_udev=%u)\n", udevid, src_info->udevid);
        return -EINVAL;
    }

    if (ops->pa_get != NULL) {
        ret = ops->pa_get(udevid, src_info, pa_seg, seg_num, flag);
        if (ret != 0) {
            svm_err("Smm cs pa get failed. (ret=%d; udev=%u; src_udev=%u)\n", ret, udevid, src_info->udevid);
        }
    } else {
        svm_err("Smm cs pa_get fun not register. (src_udevid=%u)\n", src_info->udevid);
        ret = -EINVAL;
    }

    return ret;
}

static int smm_cross_server_pa_put(struct smm_ctx *ctx, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[],
                                   u64 seg_num)
{
    struct smm_ops *ops = NULL;
    u32 udevid = ctx->udevid;
    int ret;

    ops = ctx->cs_ops;
    if (ops == NULL) {
        svm_err("Smm cs ops not register. (udev=%u; src_udev=%u)\n", udevid, src_info->udevid);
        return -EINVAL;
    }

    if (ops->pa_put != NULL) {
        ret = ops->pa_put(udevid, src_info, pa_seg, seg_num);
        if (ret != 0) {
            svm_err("Smm cs pa put failed. (ret=%d; udev=%u; src_udev=%u)\n", ret, udevid, src_info->udevid);
        }
    } else {
        svm_err("Smm cs pa_put fun not register. (src_udevid=%u)\n", src_info->udevid);
        ret = -EINVAL;
    }

    return ret;
}

static int smm_in_server_pa_get(struct smm_ctx *ctx, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[],
                                u64 *seg_num, u64 flag)
{
    struct smm_ops *ops = NULL;
    u32 udevid = ctx->udevid;
    int ret;

    ops = ctx->ops[src_info->udevid];
    if (ops == NULL) {
        svm_err("Smm ops not register. (udev=%u; src_udev=%u)\n", udevid, src_info->udevid);
        return -EINVAL;
    }

    if (ops->pa_get != NULL) {
        ret = ops->pa_get(udevid, src_info, pa_seg, seg_num, flag);
        if (ret != 0) {
            svm_err("Smm pa get failed. (ret=%d; udev=%u; src_udev=%u)\n", ret, udevid, src_info->udevid);
        }
    } else {
        svm_err("Smm pa_get fun not register. (src_udevid=%u)\n", src_info->udevid);
        ret = -EINVAL;
    }

    return ret;
}

static int smm_in_server_pa_put(struct smm_ctx *ctx, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[],
                                u64 seg_num)
{
    struct smm_ops *ops = NULL;
    u32 udevid = ctx->udevid;
    int ret;

    if (src_info->udevid >= smm_get_src_max_udev_num()) {
        svm_err("Invalid src udevid. (src_udevid=%u)\n", src_info->udevid);
        return -EINVAL;
    }

    ops = ctx->ops[src_info->udevid];
    if (ops == NULL) {
        svm_err("Smm ops not register. (udev=%u; src_udev=%u)\n", udevid, src_info->udevid);
        return -EINVAL;
    }

    if (ops->pa_put != NULL) {
        ret = ops->pa_put(udevid, src_info, pa_seg, seg_num);
        if (ret != 0) {
            svm_err("Smm pa get failed. (ret=%d; udev=%u; src_udev=%u)\n", ret, udevid, src_info->udevid);
        }
    } else {
        svm_err("Smm pa_put fun not register. (src_udevid=%u)\n", src_info->udevid);
        ret = -EINVAL;
    }

    return ret;
}

int smm_pa_get(struct smm_ctx *ctx, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[], u64 *seg_num, u64 flag)
{
    int ret = smm_ops_para_check(src_info, pa_seg, seg_num);
    if (ret != 0) {
        svm_err("Smm pa handle check failed. (ret=%d)\n", ret);
        return ret;
    }

    if (svm_is_cross_server(ctx->udevid, src_info->server_id)) {
        return smm_cross_server_pa_get(ctx, src_info, pa_seg, seg_num, flag);
    } else {
        return smm_in_server_pa_get(ctx, src_info, pa_seg, seg_num, flag);
    }
}

int smm_pa_put(struct smm_ctx *ctx, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[], u64 seg_num)
{
    bool is_cs = svm_is_cross_server(ctx->udevid, src_info->server_id);
    if (is_cs || !smm_is_ops_support_remap(ctx, src_info->udevid)) {
        int ret = smm_ops_para_check(src_info, pa_seg, &seg_num);
        if (ret != 0) {
            svm_err("Smm pa handle check failed. (ret=%d)\n", ret);
            return ret;
        }
    }

    if (is_cs) {
        return smm_cross_server_pa_put(ctx, src_info, pa_seg, seg_num);
    } else {
        return smm_in_server_pa_put(ctx, src_info, pa_seg, seg_num);
    }
}

int svm_smm_register_ops(u32 udevid, u32 src_udevid, const struct smm_ops *dev_ops)
{
    u32 src_max_udev_num = smm_get_src_max_udev_num();
    struct smm_ctx *ctx = NULL;

    if (src_udevid >= src_max_udev_num) {
        svm_err("Invalid para.(src_udev=%u; src_max_udev_num=%u)\n", src_udevid, src_max_udev_num);
        return -EINVAL;
    }

    ctx = smm_ctx_get(udevid);
    if (ctx == NULL) {
        svm_err("Get ctx failed. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ctx->ops[src_udevid] = (struct smm_ops *)dev_ops;

    smm_ctx_put(ctx);
    return 0;
}

int svm_smm_get_ops(u32 udevid, u32 src_udevid, struct smm_ops **dev_ops)
{
    u32 src_max_udev_num = smm_get_src_max_udev_num();
    struct smm_ctx *ctx = NULL;

    if (src_udevid >= src_max_udev_num) {
        svm_err("Invalid para.(src_udev=%u; src_max_udev_num=%u)\n", src_udevid, src_max_udev_num);
        return -EINVAL;
    }

    ctx = smm_ctx_get(udevid);
    if (ctx == NULL) {
        svm_err("Get ctx failed. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    *dev_ops = ctx->ops[src_udevid];

    smm_ctx_put(ctx);
    return 0;
}

int svm_smm_register_cs_ops(u32 udevid, const struct smm_ops *cs_ops)
{
    struct smm_ctx *ctx = NULL;

    ctx = smm_ctx_get(udevid);
    if (ctx == NULL) {
        svm_err("Get ctx failed. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ctx->cs_ops = (struct smm_ops *)cs_ops;

    smm_ctx_put(ctx);
    return 0;
}

int smm_ops_init_dev(u32 udevid)
{
    u32 src_max_udev_num = smm_get_src_max_udev_num();
    u64 size = sizeof(struct smm_ctx) + sizeof(struct smm_ops *) * src_max_udev_num;
    struct smm_ctx *ctx = NULL;
    void *dev_ctx = NULL;
    int ret;

    ctx = (struct smm_ctx *)svm_kvzalloc(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ctx == NULL) {
        svm_err("Smm kernel init alloc pa_get_handle failed.\n");
        return -ENOMEM;
    }

    ctx->udevid = udevid;
    ctx->ops = (struct smm_ops **)(void *)(ctx + 1);

    dev_ctx = svm_dev_ctx_get(udevid);
    if (dev_ctx == NULL) {
        ret = -EINVAL;
        goto free_pa_handle;
    }

    ret = svm_dev_set_feature_priv(dev_ctx, smm_ops_feature_id, NULL, ctx);
    if (ret != 0) {
        svm_err("Set dev feature priv failed. (udevid=%u; ret=%d; feature_id=%u)\n", udevid, ret, smm_ops_feature_id);
        goto put_ctx;
    }

    ctx->dev_ctx = dev_ctx;

    return 0;

put_ctx:
    svm_dev_ctx_put(dev_ctx);
free_pa_handle:
    svm_kvfree(ctx);

    return ret;
}
DECLAER_FEATURE_AUTO_INIT_DEV(smm_ops_init_dev, FEATURE_LOADER_STAGE_2);

void smm_ops_uninit_dev(u32 udevid)
{
    struct smm_ctx *ctx = NULL;

    ctx = smm_ctx_get(udevid);
    if (ctx != NULL) {
        (void)svm_dev_set_feature_priv(ctx->dev_ctx, smm_ops_feature_id, NULL, NULL);
        smm_ctx_put(ctx);
        svm_dev_ctx_put(ctx->dev_ctx); /* paired with init */
        svm_kvfree(ctx);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(smm_ops_uninit_dev, FEATURE_LOADER_STAGE_2);

int smm_ops_feature_init(void)
{
    smm_ops_feature_id = svm_dev_obtain_feature_id();
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(smm_ops_feature_init, FEATURE_LOADER_STAGE_2);

void smm_ops_feature_uninit(void)
{}
DECLAER_FEATURE_AUTO_UNINIT(smm_ops_feature_uninit, FEATURE_LOADER_STAGE_2);
