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
#include "ka_hashtable_pub.h"
#include "ka_system_pub.h"
#include "ka_fs_pub.h"
#include "ka_sched_pub.h"

#include "pbl_feature_loader.h"
#include "pbl/pbl_soc_res.h"
#include "pbl_uda.h"
#include "pbl_soc_res.h"
#include "pbl_runenv_config.h"
#include "pbl_ubmm.h"
#include "dpa_kernel_interface.h"

#include "svm_kern_log.h"
#include "svm_pub.h"
#include "svm_slab.h"
#include "framework_dev.h"
#include "framework_task.h"
#include "dbi_kern.h"
#include "ubmm_core.h"
#include "ubmm_usage_counter.h"
#include "ubmm_ctx.h"

static u32 ubmm_dev_feature_id;
static u32 ubmm_task_feature_id;
static bool g_ubmm_is_inited = false;
static bool g_ubmm_is_need = false;

struct ubmm_ctx *ubmm_ctx_get(u32 udevid, int tgid)
{
    struct ubmm_ctx *ctx = NULL;
    void *task_ctx = NULL;

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        return NULL;
    }

    ctx = (struct ubmm_ctx *)svm_task_get_feature_priv(task_ctx, ubmm_task_feature_id);
    if (ctx == NULL) {
        svm_task_ctx_put(task_ctx);
    }

    return ctx;
}

void ubmm_ctx_put(struct ubmm_ctx *ctx)
{
    svm_task_ctx_put(ctx->task_ctx);
}

static void ubmm_ctx_init(struct ubmm_ctx *ctx)
{
    ctx->ubdev_task_added = false;
    ka_hash_init(ctx->htable);
    ka_task_mutex_init(&ctx->mutex);
}

/* return -EAGAIN: UBMEM has not been configured. */
static int _ubmm_init_dev(u32 udevid)
{
    int ret;

    if (!g_ubmm_is_inited) {
        ret = ubmm_init_dev_res(udevid);
        if (ret != 0) {
            return ret;
        }

        ret = svm_enable_ubmem(udevid);
        if (ret != 0) {
            return ret;
        }
        g_ubmm_is_inited = true;
    }

    return 0;
}

static void _ubmm_uninit_dev(u32 udevid)
{
    if (g_ubmm_is_inited) {
        ubmm_uninit_dev_res(udevid);
        g_ubmm_is_inited = false;
    }
}

static void ubmm_try_init_dev(u32 udevid)
{
    u32 udevid_ex = 0;
    int ret;

    if (g_ubmm_is_need) {
        (void)uda_get_cur_ns_udevids(&udevid_ex, 1);
        udevid_ex = (udevid == uda_get_host_id()) ? udevid_ex :
                                                    udevid; /* Not host_id real dev, change to first udevid. */
        /* trigger ub dev adapt to update uba addr */
        ret = uda_dev_ctrl(udevid_ex, UDA_CTRL_UPDATE_P2P_ADDR);
        if (ret == 0) {
            (void)_ubmm_init_dev(udevid);
        }
    }
}

static void ubmm_ctx_release(void *priv)
{
    struct ubmm_ctx *ctx = (struct ubmm_ctx *)priv;
    svm_vfree(ctx);
}

static int ubmm_ctx_create(u32 udevid, int tgid)
{
    struct ubmm_ctx *ctx = NULL;
    void *task_ctx = NULL;
    int ret;

    ctx = (struct ubmm_ctx *)svm_vzalloc(sizeof(*ctx));
    if (ctx == NULL) {
        svm_err("No mem. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ENOMEM;
    }

    ctx->udevid = udevid;
    ctx->tgid = tgid;
    ubmm_ctx_init(ctx);

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        svm_vfree(ctx);
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = svm_task_set_feature_priv(task_ctx, ubmm_task_feature_id, "ubmm", (void *)ctx, ubmm_ctx_release);
    if (ret != 0) {
        svm_task_ctx_put(task_ctx);
        svm_vfree(ctx);
        return ret;
    }

    ctx->task_ctx = task_ctx;
    return 0;
}

int ubmm_init_try(u32 udevid, int tgid)
{
    static KA_TASK_DEFINE_MUTEX(mutex);
    int ret;

    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        if (udevid != uda_get_host_id()) {
            return -ENOTSUPP;
        }
    } else {
        /* only support cp */
        processType_t proc_type = 0;
        ret = apm_query_proc_type_by_slave(tgid, &proc_type);
        if ((ret != 0) || (proc_type != PROCESS_CP1)) {
            return -ENOTSUPP;
        }
    }

    ka_task_mutex_lock(&mutex);
    if (ubmm_ctx_get(udevid, tgid) != NULL) {
        ka_task_mutex_unlock(&mutex);
        return 0;
    }

    ubmm_try_init_dev(udevid);
    if (!g_ubmm_is_inited) {
        ka_task_mutex_unlock(&mutex);
        return -ENOTSUPP;
    }

    ret = ubmm_ctx_create(udevid, tgid);
    ka_task_mutex_unlock(&mutex);
    return ret;
}

int ubmm_init_task(u32 udevid, int tgid, void *start_time)
{
    int ret;

    SVM_UNUSED(start_time);
    ret = ubmm_init_try(udevid, tgid);
    return (ret == -ENOTSUPP) ? 0 : ret;
}
DECLAER_FEATURE_AUTO_INIT_TASK(ubmm_init_task, FEATURE_LOADER_STAGE_2);

static void ubmm_destroy_task(struct ubmm_ctx *ctx)
{
    svm_task_set_feature_invalid(ctx->task_ctx, ubmm_task_feature_id);
    ubmm_usage_counter_recycle(ctx);
    ubmm_node_recycle(ctx);
    svm_task_ctx_put(ctx->task_ctx); /* with init pair */
}

void ubmm_uninit_task(u32 udevid, int tgid, void *start_time)
{
    struct ubmm_ctx *ctx = NULL;

    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        if (udevid != uda_get_host_id()) {
            return;
        }
    }

    ctx = ubmm_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        return;
    }

    ubmm_ctx_put(ctx);

    if (!svm_task_is_exit_abort(ctx->task_ctx)) {
        ubmm_destroy_task(ctx);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(ubmm_uninit_task, FEATURE_LOADER_STAGE_2);

void ubmm_show_task(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    struct ubmm_ctx *ctx = NULL;

    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        if (udevid != uda_get_host_id()) {
            return;
        }
    }

    if ((feature_id != (int)ubmm_task_feature_id) || !g_ubmm_is_inited) {
        return;
    }

    ctx = ubmm_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return;
    }

    ubmm_node_show(ctx, seq);
    ubmm_ctx_put(ctx);
}
DECLAER_FEATURE_AUTO_SHOW_TASK(ubmm_show_task, FEATURE_LOADER_STAGE_2);

void ubmm_show_dev(u32 udevid, int feature_id, ka_seq_file_t *seq)
{
    u64 uba_base, total_size, avail_size;

    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        if (udevid != uda_get_host_id()) {
            return;
        }
    }

    if ((feature_id != ubmm_dev_feature_id) || !g_ubmm_is_inited) {
        return;
    }

    if (ubmm_get_uba_pool(&uba_base, &total_size, &avail_size) == 0) {
        ka_fs_seq_printf(seq, "ubmm info:\n");
        ka_fs_seq_printf(seq, "    uba_base 0x%llx\n", uba_base);
        ka_fs_seq_printf(seq, "    total_size 0x%llx\n", total_size);
        ka_fs_seq_printf(seq, "    avail_size 0x%llx\n", avail_size);
    }
}
DECLAER_FEATURE_AUTO_SHOW_DEV(ubmm_show_dev, FEATURE_LOADER_STAGE_2);

int ubmm_init_dev(u32 udevid)
{
    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        if (udevid != uda_get_host_id()) {
            return 0;
        }
    }

    g_ubmm_is_need = true;
    (void)_ubmm_init_dev(udevid); /* Ignore fail, UBMEM may not have been configured yet. */
    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(ubmm_init_dev, FEATURE_LOADER_STAGE_2);

int ubmm_uninit_dev(u32 udevid)
{
    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        if (udevid != uda_get_host_id()) {
            return 0;
        }
    }

    if (g_ubmm_is_inited) {
        _ubmm_uninit_dev(udevid);
    }

    return 0;
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(ubmm_uninit_dev, FEATURE_LOADER_STAGE_2);

int ubmm_init(void)
{
    ubmm_dev_feature_id = svm_dev_obtain_feature_id();
    ubmm_task_feature_id = svm_task_obtain_feature_id();
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(ubmm_init, FEATURE_LOADER_STAGE_2);

void ubmm_uninit(void)
{
    g_ubmm_is_inited = false;
}
DECLAER_FEATURE_AUTO_UNINIT(ubmm_uninit, FEATURE_LOADER_STAGE_2);
