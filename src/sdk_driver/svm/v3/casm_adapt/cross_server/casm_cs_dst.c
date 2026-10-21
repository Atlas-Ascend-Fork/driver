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
#include "ka_hashtable_pub.h"
#include "ka_fs_pub.h"
#include "ka_compiler_pub.h"
#include "ka_ioctl_pub.h"

#include "pbl_feature_loader.h"
#include "pbl_uda.h"
#include "dpa_kernel_interface.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "framework_task.h"
#include "framework_cmd.h"
#include "mwl.h"
#include "casm_kernel.h"
#include "casm_cs_key.h"
#include "svm_ioctl_ex.h"
#include "casm_ioctl.h"
#include "casm_cs_dst.h"

#define CASM_CS_DST_NODE_HASH_BIT 8

struct casm_cs_dst_ctx {
    u32 udevid;
    int tgid;
    void *task_ctx;
    ka_mutex_t mutex;
    KA_DECLARE_HASHTABLE(key_htable, CASM_CS_DST_NODE_HASH_BIT);
};

struct casm_cs_dst_node {
    ka_hlist_node_t link;
    u64 key;
    int owner_tgid;
    int ref_cnt;
    struct svm_global_va src_va;
    struct casm_src_prop src_prop;
};

static u32 casm_cs_dst_feature_id;

static struct casm_cs_dst_ctx *casm_cs_dst_ctx_get(u32 udevid, int tgid)
{
    struct casm_cs_dst_ctx *cs_ctx = NULL;
    void *task_ctx = NULL;

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        return NULL;
    }

    cs_ctx = (struct casm_cs_dst_ctx *)svm_task_get_feature_priv(task_ctx, casm_cs_dst_feature_id);
    if (cs_ctx == NULL) {
        svm_task_ctx_put(task_ctx);
    }

    return cs_ctx;
}

static void casm_cs_dst_ctx_put(struct casm_cs_dst_ctx *cs_ctx)
{
    svm_task_ctx_put(cs_ctx->task_ctx);
}

static inline u32 casm_cs_dst_get_bkt(u64 key)
{
    return ka_hash_min((u32)key, CASM_CS_DST_NODE_HASH_BIT);
}

static inline void casm_cs_dst_node_add(struct casm_cs_dst_ctx *cs_ctx, struct casm_cs_dst_node *node)
{
    u32 bkt = casm_cs_dst_get_bkt(node->key);
    ka_hash_add(cs_ctx->key_htable, &node->link, bkt);
}

static inline void casm_cs_dst_node_del(struct casm_cs_dst_node *node)
{
    ka_hash_del(&node->link);
}

static inline struct casm_cs_dst_node *casm_cs_dst_node_find(struct casm_cs_dst_ctx *cs_ctx, u64 key)
{
    u32 bkt = casm_cs_dst_get_bkt(key);
    struct casm_cs_dst_node *node = NULL;

    ka_hash_for_each_possible(cs_ctx->key_htable, node, link, bkt)
    {
        if (node->key == key) {
            return node;
        }
    }
    return NULL;
}

static void casm_cs_recycle_dst_node(struct casm_cs_dst_ctx *cs_ctx)
{
    struct casm_cs_dst_node *node = NULL;
    ka_hlist_node_t *hnode = NULL;
    u32 bkt, num = 0;

    ka_task_mutex_lock(&cs_ctx->mutex);
    ka_hash_for_each_safe(cs_ctx->key_htable, bkt, hnode, node, link)
    {
        casm_cs_dst_node_del(node);
        svm_kfree(node);
        num++;
    }
    ka_task_mutex_unlock(&cs_ctx->mutex);

    if (num > 0) {
        svm_warn("Recycle key. (udevid=%u; tgid=%d; num=%u)\n", cs_ctx->udevid, cs_ctx->tgid, num);
    }
}

static void casm_cs_show_dst_node(struct casm_cs_dst_ctx *cs_ctx, ka_seq_file_t *seq)
{
    struct casm_cs_dst_node *node = NULL;
    ka_hlist_node_t *hnode = NULL;
    u32 bkt;
    int i = 0;

    ka_task_mutex_lock(&cs_ctx->mutex);
    ka_hash_for_each_safe(cs_ctx->key_htable, bkt, hnode, node, link)
    {
        struct svm_global_va *src_va = &node->src_va;
        if (i == 0) {
            ka_fs_seq_printf(
                seq, "casm cs:   index     key     owner_tgid    server_id   udevid    tgid     va     size \n");
        }
        ka_fs_seq_printf(seq, "   %d       %llx      %d      %u      %u      %d      %llx     %llx \n", i++, node->key,
                         node->owner_tgid, src_va->server_id, src_va->udevid, src_va->tgid, src_va->va, src_va->size);
        if (i >= 128) { /* show max 128 item */
            break;
        }
    }
    ka_task_mutex_unlock(&cs_ctx->mutex);
}

static int _casm_cs_set_src_info(struct casm_cs_dst_ctx *cs_ctx, u64 key, struct svm_global_va *src_va, int owner_tgid,
                                 const struct casm_src_prop *src_prop)
{
    struct casm_cs_dst_node *node = NULL;

    node = casm_cs_dst_node_find(cs_ctx, key);
    if (node != NULL) {
        node->ref_cnt++;
        return 0;
    }

    node = (struct casm_cs_dst_node *)svm_kzalloc(sizeof(*node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        svm_err("No mem. (udevid=%u; tgid=%d; key=0x%llx)\n", cs_ctx->udevid, cs_ctx->tgid, key);
        return -ENOMEM;
    }

    node->key = key;
    node->owner_tgid = owner_tgid;
    node->ref_cnt = 1;
    node->src_va = *src_va;
    node->src_prop = *src_prop;
    casm_cs_dst_node_add(cs_ctx, node);

    return 0;
}

static int casm_cs_set_src_info(u32 udevid, int tgid, u64 key, struct svm_global_va *src_va, int owner_tgid,
                                const struct casm_src_prop *src_prop)
{
    struct casm_cs_dst_ctx *cs_ctx = NULL;
    int ret = 0;

    cs_ctx = casm_cs_dst_ctx_get(udevid, tgid);
    if (cs_ctx == NULL) {
        svm_err("Invalid para. (udevid=%u; tgid=%d; key=0x%llx)\n", udevid, tgid, key);
        return -EINVAL;
    }

    if (!casm_cs_is_local_key(key)) {
        ka_task_mutex_lock(&cs_ctx->mutex);
        ret = _casm_cs_set_src_info(cs_ctx, key, src_va, owner_tgid, src_prop);
        ka_task_mutex_unlock(&cs_ctx->mutex);
    }

    casm_cs_dst_ctx_put(cs_ctx);

    return ret;
}

static int _casm_cs_clr_src_info(struct casm_cs_dst_ctx *cs_ctx, u64 key)
{
    struct casm_cs_dst_node *node = NULL;

    node = casm_cs_dst_node_find(cs_ctx, key);
    if (node == NULL) {
        svm_err("No key. (udevid=%u; tgid=%d; key=0x%llx)\n", cs_ctx->udevid, cs_ctx->tgid, key);
        return -EINVAL;
    }

    node->ref_cnt--;
    if (node->ref_cnt <= 0) {
        casm_cs_dst_node_del(node);
        svm_kfree(node);
    }

    return 0;
}

static int casm_cs_clr_src_info(u32 udevid, int tgid, u64 key)
{
    struct casm_cs_dst_ctx *cs_ctx = NULL;
    int ret = 0;

    cs_ctx = casm_cs_dst_ctx_get(udevid, tgid);
    if (cs_ctx == NULL) {
        svm_err("Invalid para. (udevid=%u; tgid=%d; key=0x%llx)\n", udevid, tgid, key);
        return -EINVAL;
    }

    if (!casm_cs_is_local_key(key)) {
        ka_task_mutex_lock(&cs_ctx->mutex);
        ret = _casm_cs_clr_src_info(cs_ctx, key);
        ka_task_mutex_unlock(&cs_ctx->mutex);
    }
    casm_cs_dst_ctx_put(cs_ctx);

    return ret;
}

static int _casm_cs_query_src(struct casm_cs_dst_ctx *cs_ctx, u64 key, struct svm_global_va *src_va, int *owner_tgid,
                              struct casm_src_prop *src_prop)
{
    struct casm_cs_dst_node *node = NULL;

    node = casm_cs_dst_node_find(cs_ctx, key);
    if (node == NULL) {
        svm_err("No key. (udevid=%u; tgid=%d; key=0x%llx)\n", cs_ctx->udevid, cs_ctx->tgid, key);
        /* -EFAULT is mapped to DRV_ERROR_INVALID_HANDLE for the upper-layer errmsg,
         * align with the local-key path in _casm_get_src_va(). */
        return -EFAULT;
    }

    *owner_tgid = node->owner_tgid;
    *src_va = node->src_va;
    if (src_prop != NULL) {
        *src_prop = node->src_prop;
    }

    return 0;
}

static int casm_cs_query_src(u32 udevid, int tgid, u64 key, struct svm_global_va *src_va, int *owner_tgid,
                             struct casm_src_prop *src_prop)
{
    struct casm_cs_dst_ctx *cs_ctx = NULL;
    int ret;

    cs_ctx = casm_cs_dst_ctx_get(udevid, tgid);
    if (cs_ctx == NULL) {
        svm_err("Invalid para. (udevid=%u; tgid=%d; key=0x%llx)\n", udevid, tgid, key);
        return -EINVAL;
    }

    ka_task_mutex_lock(&cs_ctx->mutex);
    ret = _casm_cs_query_src(cs_ctx, key, src_va, owner_tgid, src_prop);
    ka_task_mutex_unlock(&cs_ctx->mutex);
    casm_cs_dst_ctx_put(cs_ctx);

    return ret;
}

static int casm_cs_src_info_query(u32 udevid, u64 key, struct svm_global_va *src_va, struct casm_src_prop *src_prop)
{
    int owner_tgid;
    return casm_cs_query_src(udevid, ka_task_get_current_tgid(), key, src_va, &owner_tgid, src_prop);
}

static int casm_cs_src_info_get(u32 udevid, u64 key, struct svm_global_va *src_va, int *owner_tgid)
{
    return casm_cs_query_src(udevid, ka_task_get_current_tgid(), key, src_va, owner_tgid, NULL);
}

static void casm_cs_src_info_put(u32 udevid, u64 key, struct svm_global_va *src_va, int owner_tgid)
{ /* do nothing */
}

static const struct svm_casm_dst_ops g_casm_cs_dst_ops = {
    .src_info_query = casm_cs_src_info_query,
    .src_info_get = casm_cs_src_info_get,
    .src_info_put = casm_cs_src_info_put,
};

static void casm_cs_dst_ctx_init(struct casm_cs_dst_ctx *cs_ctx)
{
    ka_hash_init(cs_ctx->key_htable);
    ka_task_mutex_init(&cs_ctx->mutex);
}

static void casm_cs_dst_ctx_release(void *priv)
{
    struct casm_cs_dst_ctx *ctx = (struct casm_cs_dst_ctx *)priv;
    svm_kfree(ctx);
}

int casm_cs_dst_init_task(u32 udevid, int tgid, void *start_time)
{
    struct casm_cs_dst_ctx *cs_ctx = NULL;
    void *task_ctx = NULL;
    int ret;

    cs_ctx = (struct casm_cs_dst_ctx *)svm_kzalloc(sizeof(*cs_ctx), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (cs_ctx == NULL) {
        svm_err("No mem. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ENOMEM;
    }

    cs_ctx->udevid = udevid;
    cs_ctx->tgid = tgid;
    casm_cs_dst_ctx_init(cs_ctx);

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        svm_kfree(cs_ctx);
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = svm_task_set_feature_priv(task_ctx, casm_cs_dst_feature_id, "casm_cs_dst", (void *)cs_ctx,
                                    casm_cs_dst_ctx_release);
    if (ret != 0) {
        svm_task_ctx_put(task_ctx);
        svm_kfree(cs_ctx);
        return ret;
    }

    cs_ctx->task_ctx = task_ctx;

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_TASK(casm_cs_dst_init_task, FEATURE_LOADER_STAGE_7);

static void casm_cs_dst_destroy_task(struct casm_cs_dst_ctx *cs_ctx)
{
    svm_task_set_feature_invalid(cs_ctx->task_ctx, casm_cs_dst_feature_id);
    casm_cs_recycle_dst_node(cs_ctx);
    svm_task_ctx_put(cs_ctx->task_ctx); /* with init pair */
}

void casm_cs_dst_uninit_task(u32 udevid, int tgid, void *start_time)
{
    struct casm_cs_dst_ctx *cs_ctx = NULL;

    cs_ctx = casm_cs_dst_ctx_get(udevid, tgid);
    if (cs_ctx == NULL) {
        return;
    }

    casm_cs_dst_ctx_put(cs_ctx);

    if (!svm_task_is_exit_abort(cs_ctx->task_ctx)) {
        casm_cs_dst_destroy_task(cs_ctx);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(casm_cs_dst_uninit_task, FEATURE_LOADER_STAGE_7);

void casm_cs_dst_show_task(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    struct casm_cs_dst_ctx *cs_ctx = NULL;

    if (feature_id != (int)casm_cs_dst_feature_id) {
        return;
    }

    cs_ctx = casm_cs_dst_ctx_get(udevid, tgid);
    if (cs_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return;
    }

    casm_cs_show_dst_node(cs_ctx, seq);

    casm_cs_dst_ctx_put(cs_ctx);
}
DECLAER_FEATURE_AUTO_SHOW_TASK(casm_cs_dst_show_task, FEATURE_LOADER_STAGE_7);

static int casm_ioctl_cs_set_src(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_casm_cs_set_src_para para;
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    if (casm_cs_parse_server_id_from_key(para.key) != para.src_va.server_id) {
        svm_err("Key and src info server id not match. (key=%llx; server_id=%u)\n", para.key, para.src_va.server_id);
        return -EINVAL;
    }

    if ((u32)para.src_prop.casm_pg_type >= CASM_PG_TYPE_MAX) {
        svm_err("Invalid source page type. (pg_type=%u)\n", (u32)para.src_prop.casm_pg_type);
        return -EINVAL;
    }

    return casm_cs_set_src_info(udevid, ka_task_get_current_tgid(), para.key, &para.src_va, para.owner_pid,
                                &para.src_prop);
}

static int casm_ioctl_cs_clr_src(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_casm_cs_clr_src_para para;
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    return casm_cs_clr_src_info(udevid, ka_task_get_current_tgid(), para.key);
}

int casm_cs_dst_init(void)
{
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_CASM_CS_SET_SRC), casm_ioctl_cs_set_src);
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_CASM_CS_CLR_SRC), casm_ioctl_cs_clr_src);

    casm_cs_dst_feature_id = svm_task_obtain_feature_id();
    svm_casm_register_dst_ops(&g_casm_cs_dst_ops);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(casm_cs_dst_init, FEATURE_LOADER_STAGE_7);

void svm_casm_cs_dst_uninit(void)
{}
DECLAER_FEATURE_AUTO_UNINIT(svm_casm_cs_dst_uninit, FEATURE_LOADER_STAGE_7);
