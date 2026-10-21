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
#include "ka_common_pub.h"
#include "ka_fs_pub.h"
#include "ka_task_pub.h"

#include "apm_task_group_def.h"
#include "apm_res_map_ctx.h"
#include "apm_slab.h"
#include "apm_res_map_ops.h"
#include "apm_sched.h"
#include "apm_res_map.h"

#define APM_RES_MAP_CMP(a, b) (((a) > (b)) - ((a) < (b)))

static int apm_res_map_info_compare(const struct apm_res_map_info *tar, const struct apm_res_map_info *cur)
{
    int ret;

    ret = APM_RES_MAP_CMP(tar->udevid, cur->udevid);
    if (ret != 0) {
        return ret;
    }

    ret = APM_RES_MAP_CMP(tar->res_info.id, cur->res_info.id);
    if (ret != 0) {
        return ret;
    }

    ret = APM_RES_MAP_CMP(tar->res_info.target_proc_type, cur->res_info.target_proc_type);
    if (ret != 0) {
        return ret;
    }

    ret = APM_RES_MAP_CMP(apm_res_use_svm_va(tar->res_info.flag), apm_res_use_svm_va(cur->res_info.flag));
    if (ret != 0) {
        return ret;
    }

    ret = APM_RES_MAP_CMP(tar->res_info.res_type, cur->res_info.res_type);
    if (ret != 0) {
        return ret;
    }

    return APM_RES_MAP_CMP(tar->res_info.res_id, cur->res_info.res_id);
}

static int apm_res_map_node_rb_compare_of_insert(ka_rb_node_t *tar, ka_rb_node_t *cur)
{
    struct apm_res_map_node *tar_node = ka_container_of(tar, struct apm_res_map_node, rb_node);
    struct apm_res_map_node *cur_node = ka_container_of(cur, struct apm_res_map_node, rb_node);

    return apm_res_map_info_compare(&tar_node->ctx, &cur_node->ctx);
}

static int apm_res_map_node_rb_compare_of_search(void *handle, ka_rb_node_t *cur)
{
    struct apm_res_map_info *tar_ctx = (struct apm_res_map_info *)handle;
    struct apm_res_map_node *cur_node = ka_container_of(cur, struct apm_res_map_node, rb_node);

    return apm_res_map_info_compare(tar_ctx, &cur_node->ctx);
}

void apm_res_map_free_node_pa_array(struct apm_res_map_info *res_map_info)
{
    if (res_map_info->pa_array != NULL) {
        apm_put_res_addr(res_map_info->udevid, &res_map_info->res_info, res_map_info->pa_array, res_map_info->len);
        apm_kvfree(res_map_info->pa_array);
        res_map_info->pa_array = NULL;
    }
}

static void apm_res_map_ctx_try_recycle_node(struct apm_res_map_ctx *map_ctx)
{
    ka_rb_node_t *rb_node = ka_base_rb_first(&map_ctx->rb_root);
    unsigned long stamp = ka_jiffies;

    while (rb_node != NULL) {
        struct apm_res_map_node *node = ka_base_rb_entry(rb_node, struct apm_res_map_node, rb_node);
        int proc_type = node->ctx.res_info.target_proc_type;
        int ret;

        ret = pbl_rb_erase(&map_ctx->rb_root, &node->rb_node);
        if (ret != 0) {
            apm_err("Erase map node failed. (ret=%d; udevid=%u; res_type=%d; res_id=%u)\n", ret, node->ctx.udevid,
                    node->ctx.res_info.res_type, node->ctx.res_info.res_id);
            break;
        }

        apm_debug("Recycle map node. (udevid=%u; proc_type=%d(%s); res_type=%d; res_id=%u; va=0x%lx; len=%u\n",
                  node->ctx.udevid, proc_type, apm_proc_type_to_name(proc_type), node->ctx.res_info.res_type,
                  node->ctx.res_info.res_id, node->ctx.va, node->ctx.len);
        apm_res_map_free_node_pa_array(&node->ctx);
        apm_kvfree(node);

        apm_try_cond_resched(&stamp);
        rb_node = ka_base_rb_first(&map_ctx->rb_root);
    }
}

static void apm_res_map_ctx_release(struct task_ctx *ctx)
{
    struct apm_res_map_ctx *map_ctx = (struct apm_res_map_ctx *)ctx->priv;

    apm_res_map_ctx_try_recycle_node(map_ctx);
    apm_proc_fs_del_task(map_ctx->entry);
    apm_vfree(ctx->priv);
    ctx->priv = NULL;
}

static int _apm_res_map_ctx_create(struct task_ctx_domain *domain, int tgid)
{
    struct apm_res_map_ctx *map_ctx = NULL;
    long size = sizeof(*map_ctx);
    int ret;

    map_ctx = apm_vzalloc(size);
    if (map_ctx == NULL) {
        apm_err("Vmalloc res map ctx fail. (size=%ld)\n", size);
        return -ENOMEM;
    }

    map_ctx->rb_root = KA_RB_ROOT;

    ret = task_ctx_create(domain, tgid, map_ctx, apm_res_map_ctx_release);
    if (ret != 0) {
        apm_err("Create fail. (ret=%d; tgid=%d)\n", ret, tgid);
        apm_vfree(map_ctx);
        return ret;
    }

    map_ctx->entry = apm_proc_fs_add_task(domain->name, tgid);

    return 0;
}

int apm_res_map_ctx_create(struct task_ctx_domain *domain, int tgid)
{
    struct task_ctx *ctx = NULL;
    int ret;

    ka_task_mutex_lock(&domain->mutex);
    ctx = task_ctx_get(domain, tgid);
    if (ctx == NULL) {
        ret = _apm_res_map_ctx_create(domain, tgid);
        if (ret != 0) {
            ka_task_mutex_unlock(&domain->mutex);
            return ret;
        }
    } else {
        task_ctx_put(ctx);
    }
    ka_task_mutex_unlock(&domain->mutex);

    return 0;
}

void apm_res_map_ctx_destroy(struct task_ctx_domain *domain, int tgid)
{
    struct task_ctx *ctx = NULL;

    ka_task_mutex_lock(&domain->mutex);
    ctx = task_ctx_get(domain, tgid);
    if (ctx == NULL) {
        ka_task_mutex_unlock(&domain->mutex);
        return;
    }

    task_ctx_destroy(ctx);
    task_ctx_put(ctx);
    ka_task_mutex_unlock(&domain->mutex);
}

static struct apm_res_map_node *apm_res_map_ctx_find_node(struct apm_res_map_ctx *map_ctx,
                                                          struct apm_res_map_info *para)
{
    ka_rb_node_t *rb_node = pbl_rb_search(&map_ctx->rb_root, (void *)para, apm_res_map_node_rb_compare_of_search);
    if (rb_node != NULL) {
        return ka_container_of(rb_node, struct apm_res_map_node, rb_node);
    }

    return NULL;
}

static int apm_res_map_ctx_add_node(struct task_ctx *ctx, void *priv)
{
    struct apm_res_map_ctx *map_ctx = (struct apm_res_map_ctx *)ctx->priv;
    struct apm_res_map_info *para = (struct apm_res_map_info *)priv;
    struct apm_res_map_node *node = apm_res_map_ctx_find_node(map_ctx, para);
    int ret;

    if (node != NULL) {
        ka_base_atomic_inc(&node->ctx.ref);
        apm_debug("Already mapped. (res_type=%d; res_id=%u; va=0x%lx; ref=%d)\n", para->res_info.res_type,
                  para->res_info.res_id, node->ctx.va, ka_base_atomic_read(&node->ctx.ref));
        return 0;
    }

    node = apm_kvzalloc(sizeof(struct apm_res_map_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        apm_err("Alloc node failed. (res_type=%d; res_id=%u)\n", para->res_info.res_type, para->res_info.res_id);
        return -ENOMEM;
    }

    ka_base_atomic_set(&para->ref, 1);
    node->ctx = *para;
    ret = pbl_rb_insert(&map_ctx->rb_root, &node->rb_node, apm_res_map_node_rb_compare_of_insert);
    if (ret != 0) {
        apm_kvfree(node);
        return ret;
    }

    apm_debug("Add map node success. (res_type=%d; res_id=%u; va=0x%lx; ref=%d)\n", para->res_info.res_type,
              para->res_info.res_id, node->ctx.va, ka_base_atomic_read(&node->ctx.ref));

    return 0;
}

int apm_res_map_add_node(struct task_ctx_domain *domain, int tgid, struct apm_res_map_info *para)
{
    int ret = apm_res_map_ctx_create(domain, tgid);
    if (ret != 0) {
        return ret;
    }

    return task_ctx_lock_call_func(domain, tgid, apm_res_map_ctx_add_node, (void *)para);
}

static int apm_res_map_ctx_del_node(struct task_ctx *ctx, void *priv)
{
    struct apm_res_map_ctx *map_ctx = (struct apm_res_map_ctx *)ctx->priv;
    struct apm_res_map_info *para = (struct apm_res_map_info *)priv;
    struct apm_res_map_node *node = apm_res_map_ctx_find_node(map_ctx, para);
    int ref;

    if (node == NULL) {
#ifndef EMU_ST
        apm_err("Not mapped. (res_type=%d; res_id=%u)\n", para->res_info.res_type, para->res_info.res_id);
#endif
        return -EINVAL;
    }

    if (ka_base_atomic_dec_and_test(&node->ctx.ref)) {
        apm_debug("Del success. (res_type=%d; res_id=%u; va=0x%lx; ref=%d)\n", para->res_info.res_type,
                  para->res_info.res_id, node->ctx.va, ka_base_atomic_read(&node->ctx.ref));
        ref = (int)ka_base_atomic_read(&node->ctx.ref);
        (void)pbl_rb_erase(&map_ctx->rb_root, &node->rb_node); /* Won't fail. */
        apm_res_map_free_node_pa_array(&node->ctx);
        apm_kvfree(node);
    } else {
        ref = (int)ka_base_atomic_read(&node->ctx.ref);
        apm_debug("Not del for ref. (res_type=%d; res_id=%u; va=0x%lx; ref=%d)\n", para->res_info.res_type,
                  para->res_info.res_id, node->ctx.va, ka_base_atomic_read(&node->ctx.ref));
    }
    return (ref == 0) ? 0 : -EBUSY;
}

int apm_res_map_del_node(struct task_ctx_domain *domain, int tgid, struct apm_res_map_info *para)
{
    return task_ctx_lock_call_func(domain, tgid, apm_res_map_ctx_del_node, (void *)para);
}

static int apm_res_map_ctx_query_node(struct task_ctx *ctx, void *priv)
{
    struct apm_res_map_ctx *map_ctx = (struct apm_res_map_ctx *)ctx->priv;
    struct apm_res_map_info *para = (struct apm_res_map_info *)priv;
    struct apm_res_map_node *node = apm_res_map_ctx_find_node(map_ctx, para);
    if (node == NULL) {
        apm_debug("Not mapped. (res_type=%d; res_id=%u)\n", para->res_info.res_type, para->res_info.res_id);
        return -ENXIO;
    }

    para->slave_tgid = node->ctx.slave_tgid;
    para->va = node->ctx.va;
    para->pa = node->ctx.pa;
    para->pa_array = NULL;
    para->len = node->ctx.len;
    para->ref = node->ctx.ref;
    return 0;
}

int apm_res_map_query_node(struct task_ctx_domain *domain, int tgid, struct apm_res_map_info *para)
{
    return task_ctx_lock_call_func(domain, tgid, apm_res_map_ctx_query_node, (void *)para);
}

static int apm_res_map_ctx_show_details(struct task_ctx *ctx, void *priv)
{
    struct apm_res_map_ctx *map_ctx = (struct apm_res_map_ctx *)ctx->priv;
    struct apm_res_map_node *node = NULL, *next = NULL;
    ka_seq_file_t *seq = (ka_seq_file_t *)priv;
    unsigned long stamp = ka_jiffies;

    ka_fs_seq_printf(seq, "domain: %s tgid %d(%s)\n", ctx->domain->name, ctx->tgid, ctx->name);

    ka_base_rbtree_postorder_for_each_entry_safe(node, next, &map_ctx->rb_root, rb_node)
    {
        int proc_type = node->ctx.res_info.target_proc_type;

        ka_fs_seq_printf(seq, "    udevid %u proc_type %d(%s) res_type %d res_id %u va 0x%lx pa %pK len %u\n",
                         node->ctx.udevid, proc_type, apm_proc_type_to_name(proc_type), node->ctx.res_info.res_type,
                         node->ctx.res_info.res_id, node->ctx.va, (void *)(uintptr_t)node->ctx.pa, node->ctx.len);

        apm_try_cond_resched(&stamp);
    }

    ka_fs_seq_printf(seq, "\n");
    return 0;
}

void apm_res_map_ctx_show(struct task_ctx_domain *domain, int tgid, ka_seq_file_t *seq)
{
    (void)task_ctx_lock_call_func(domain, tgid, apm_res_map_ctx_show_details, (void *)seq);
}
