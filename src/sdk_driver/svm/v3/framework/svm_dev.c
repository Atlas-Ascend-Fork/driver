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
#include "pbl_feature_loader.h"
#include "pbl_uda.h"

#include "svm_kern_log.h"
#include "svm_proc_fs.h"
#include "svm_task.h"
#include "framework_dev.h"
#include "svm_slab.h"
#include "svm_dev.h"

static u32 svm_udev_max_num;
static struct array_ctx_domain *svm_dev_domain = NULL;
static u32 dev_feature_num = 0;

u32 svm_dev_obtain_feature_id(void)
{
    return dev_feature_num++;
}

int svm_dev_set_feature_priv(void *dev_ctx, u32 feature_id, const char *feature_name, void *priv)
{
    struct svm_dev_ctx *d_ctx = (struct svm_dev_ctx *)dev_ctx;

    if (feature_id >= dev_feature_num) {
        return -EINVAL;
    }

    d_ctx->feature[feature_id].name = feature_name;
    d_ctx->feature[feature_id].priv = priv;
    return 0;
}

void *svm_dev_get_feature_priv(void *dev_ctx, u32 feature_id)
{
    struct svm_dev_ctx *d_ctx = (struct svm_dev_ctx *)dev_ctx;

    if (feature_id >= dev_feature_num) {
        return NULL;
    }

    return d_ctx->feature[feature_id].priv;
}

static void svm_dev_release_check_feature(struct svm_dev_ctx *d_ctx)
{
    u32 i;

    for (i = 0; i < dev_feature_num; i++) {
        struct svm_dev_feature *feature = &d_ctx->feature[i];
        if (feature->name != NULL) {
            svm_err("Feature not exist. (udevid=%u; name=%s)\n", d_ctx->udevid, feature->name);
        }
    }
}

static struct svm_dev_ctx *_svm_dev_ctx_get(u32 udevid)
{
    struct array_ctx *ctx = array_ctx_get(svm_dev_domain, udevid);
    if (ctx == NULL) {
        return NULL;
    }

    return (struct svm_dev_ctx *)array_ctx_priv(ctx);
}

void *svm_dev_ctx_get(u32 udevid)
{
    return _svm_dev_ctx_get(udevid);
}

static void _svm_dev_ctx_put(struct svm_dev_ctx *dev_ctx)
{
    array_ctx_put(dev_ctx->ctx);
}

void svm_dev_ctx_put(void *dev_ctx)
{
    _svm_dev_ctx_put((struct svm_dev_ctx *)dev_ctx);
}

static void svm_add_dev_feature_fs(struct svm_dev_ctx *d_ctx)
{
    static u32 inst = 0;

    d_ctx->entry = svm_proc_fs_add_dev(d_ctx->udevid, inst++);
    if (d_ctx->entry != NULL) {
        u32 i;
        for (i = 0; i < dev_feature_num; i++) {
            if (d_ctx->feature[i].name != NULL) {
                svm_proc_dev_add_feature(d_ctx->udevid, d_ctx->entry, i, d_ctx->feature[i].name);
            }
        }
    }
}

static void svm_del_dev_feature_fs(struct svm_dev_ctx *d_ctx)
{
    svm_proc_fs_del_dev(d_ctx->entry);
}

static void svm_dev_ctx_release(struct array_ctx *ctx)
{
    struct svm_dev_ctx *d_ctx = (struct svm_dev_ctx *)array_ctx_priv(ctx);

    svm_inst_trace("Release dev success. (udevid=%u)\n", d_ctx->udevid);

    svm_dev_release_check_feature(d_ctx);
    svm_del_dev_feature_fs(d_ctx);
    task_ctx_domain_destroy(d_ctx->domain);
    svm_vfree(d_ctx);
}

static u32 svm_get_max_task_per_dev(u32 udevid)
{
    return (udevid == uda_get_host_id()) ? SVM_MAX_TASK_PER_HOST_AGENT : SVM_MAX_TASK_PER_DEV_AGENT;
}

static int _svm_add_dev(u32 udevid)
{
    struct svm_dev_ctx *d_ctx = NULL;
    unsigned long size = sizeof(*d_ctx) + dev_feature_num * sizeof(struct svm_dev_feature);
    int ret;

    d_ctx = svm_vzalloc(size);
    if (d_ctx == NULL) {
        svm_err("Vmalloc dev ctx fail. (size=%lu)\n", size);
        return -ENOMEM;
    }

    d_ctx->udevid = udevid;
    d_ctx->domain = task_ctx_domain_create("svm_task", svm_get_max_task_per_dev(udevid));
    if (d_ctx->domain == NULL) {
        svm_vfree(d_ctx);
        svm_err("Create task domain fail. (udevid=%u)\n", udevid);
        return -ENOMEM;
    }

    ret = array_ctx_create(svm_dev_domain, udevid, d_ctx, svm_dev_ctx_release);
    if (ret != 0) {
        task_ctx_domain_destroy(d_ctx->domain);
        svm_vfree(d_ctx);
        svm_err("Create fail. (ret=%d; udevid=%u)\n", ret, udevid);
        return ret;
    }

    d_ctx->ctx = array_ctx_get(svm_dev_domain, udevid);
    array_ctx_put(d_ctx->ctx);

    return 0;
}

static int _svm_del_dev(u32 udevid)
{
    struct svm_dev_ctx *d_ctx = _svm_dev_ctx_get(udevid);
    if (d_ctx == NULL) {
        return -EINVAL;
    }

    _svm_dev_ctx_put(d_ctx);
    svm_inst_trace("Before destroy. (udevid=%u; ref_cnt=%d)\n", udevid, array_ctx_ref_cnt(d_ctx->ctx));
    array_ctx_destroy(d_ctx->ctx); /* array_ctx_create */

    return 0;
}

int svm_add_dev(u32 udevid)
{
    struct svm_dev_ctx *dev_ctx = NULL;
    int ret;

    ret = _svm_add_dev(udevid);
    if (ret != 0) {
        return ret;
    }

    ret = module_feature_auto_init_dev(udevid);
    if (ret != 0) {
        svm_err("Init feature failed. (udevid=%u)\n", udevid);
        (void)_svm_del_dev(udevid);
        return ret;
    }

    dev_ctx = _svm_dev_ctx_get(udevid);
    svm_add_dev_feature_fs(dev_ctx);
    _svm_dev_ctx_put(dev_ctx);

    svm_inst_trace("Add dev success. (udevid=%u)\n", udevid);

    return 0;
}

int svm_del_dev(u32 udevid)
{
    struct svm_dev_ctx *dev_ctx = _svm_dev_ctx_get(udevid);
    if (dev_ctx != NULL) {
        task_ctx_for_each_safe(dev_ctx->domain, NULL, svm_task_ctx_recycle);
        module_feature_auto_uninit_dev(udevid);
        _svm_dev_ctx_put(dev_ctx);
    }

    return _svm_del_dev(udevid);
}

static void svm_dev_ctx_recycle(struct array_ctx *ctx, void *priv)
{
    svm_warn("Recycle dev ctx. (udevid=%u)\n", ctx->id);
    (void)svm_del_dev(ctx->id);
}

static int svm_pre_hotreset(u32 udevid)
{
    struct svm_dev_ctx *dev_ctx = NULL;
    int ret;

    if (uda_is_phy_dev(udevid) == false) {
        return 0;
    }

    dev_ctx = _svm_dev_ctx_get(udevid);
    if (dev_ctx == NULL) {
        return -ENODEV;
    }

    ret = task_ctx_domain_set_inactive(dev_ctx->domain);
    _svm_dev_ctx_put(dev_ctx);
    return ret;
}

static int svm_pre_hotreset_cancel(u32 udevid)
{
    struct svm_dev_ctx *dev_ctx = NULL;

    if (uda_is_phy_dev(udevid) == false) {
        return 0;
    }

    dev_ctx = _svm_dev_ctx_get(udevid);
    if (dev_ctx == NULL) {
        return -ENODEV;
    }

    task_ctx_domain_set_active(dev_ctx->domain);
    _svm_dev_ctx_put(dev_ctx);
    return 0;
}

static int svm_uda_notifier(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= svm_udev_max_num) {
        svm_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = svm_add_dev(udevid);
    } else if (action == UDA_UNINIT) {
        ret = svm_del_dev(udevid);
    } else if (action == UDA_PRE_HOTRESET) {
        ret = svm_pre_hotreset(udevid);
    } else if (action == UDA_HOTRESET) {
        ret = svm_pre_hotreset(udevid);
    } else if (action == UDA_PRE_HOTRESET_CANCEL) {
        ret = svm_pre_hotreset_cancel(udevid);
    } else if (action == UDA_HOTRESET_CANCEL) {
        ret = svm_pre_hotreset_cancel(udevid);
    }

    svm_inst_trace("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}

static int svm_uda_notifier_register(void)
{
    struct uda_dev_type type;
    int ret;

    uda_davinci_local_real_agent_type_pack(&type);
    ret = uda_real_virtual_notifier_register("svm_agent", &type, UDA_PRI2, svm_uda_notifier);
    if (ret != 0) {
        svm_err("Uda register agent notifier failed. (ret=%d)\n", ret);
        return ret;
    }

    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_real_virtual_notifier_register("svm_master", &type, UDA_PRI2, svm_uda_notifier);
    if (ret != 0) {
        svm_err("Uda register master notifier failed. (ret=%d)\n", ret);
        uda_davinci_local_real_agent_type_pack(&type);
        (void)uda_real_virtual_notifier_unregister("svm_agent", &type);
        return ret;
    }

    return 0;
}

static void svm_uda_notifier_unregister(void)
{
    struct uda_dev_type type;
    uda_davinci_local_real_agent_type_pack(&type);
    (void)uda_real_virtual_notifier_unregister("svm_agent", &type);
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_real_virtual_notifier_unregister("svm_master", &type);
}

int svm_dev_init(void)
{
    int ret;

    svm_udev_max_num = uda_get_udev_max_num();
    svm_dev_domain = array_ctx_domain_create("svm_dev", svm_udev_max_num);
    if (svm_dev_domain == NULL) {
        svm_err("Create dev domain fail.\n");
        return -ENOMEM;
    }

    ret = svm_uda_notifier_register();
    if (ret != 0) {
        array_ctx_domain_destroy(svm_dev_domain);
        svm_dev_domain = NULL;
        return ret;
    }

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_dev_init, FEATURE_LOADER_STAGE_7);

void svm_dev_uninit(void)
{
    svm_uda_notifier_unregister();
    array_ctx_for_each_safe(svm_dev_domain, NULL, svm_dev_ctx_recycle);
    array_ctx_domain_destroy(svm_dev_domain);
    svm_dev_domain = NULL;
}
DECLAER_FEATURE_AUTO_UNINIT(svm_dev_uninit, FEATURE_LOADER_STAGE_7);
