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

#include "ka_fs_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_common_pub.h"
#include "ka_system_pub.h"
#include "ka_sched_pub.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "casm_ctx.h"
#include "casm_key.h"
#include "casm_src.h"

static struct svm_casm_src_ops *casm_src_ops = NULL;

#define SHOW_NODE_MAX 100000

void svm_casm_register_src_ops(const struct svm_casm_src_ops *ops)
{
    casm_src_ops = (struct svm_casm_src_ops *)ops;
}

static int casm_src_add_handle(u32 udevid, struct svm_global_va *src_va, struct casm_src_ex *src_ex)
{
    if (casm_src_ops != NULL) {
        return casm_src_ops->add_src(udevid, src_va, src_ex);
    } else {
        return 0;
    }
}

static void casm_src_del_handle(u32 udevid, int tgid, struct svm_global_va *src_va, struct casm_src_ex *src_ex)
{
    if (casm_src_ops != NULL) {
        return casm_src_ops->del_src(udevid, tgid, src_va, src_ex);
    }
}

int casm_add_src_node(u32 udevid, int tgid, struct casm_src_node *node)
{
    struct casm_ctx *ctx = NULL;
    struct casm_src_ctx *src_ctx = NULL;
    int ret;

    node->src_ex.owner_tgid = tgid;
    ret = casm_src_add_handle(udevid, &node->src_va, &node->src_ex);
    if (ret != 0) {
        return ret;
    }

    ctx = casm_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        casm_src_del_handle(udevid, tgid, &node->src_va, &node->src_ex);
        svm_err("Get casm ctx failed. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    src_ctx = &ctx->src_ctx;
    ka_task_write_lock_bh(&src_ctx->lock);
    ka_list_add_tail(&node->node, &src_ctx->head);
    src_ctx->node_num++;
    ka_task_write_unlock_bh(&src_ctx->lock);
    casm_ctx_put(ctx);

    return 0;
}

void casm_del_src_node(u32 udevid, int tgid, struct casm_src_node *node)
{
    struct casm_ctx *ctx = NULL;
    struct casm_src_ctx *src_ctx = NULL;

    ctx = casm_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Get casm ctx failed. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return;
    }

    src_ctx = &ctx->src_ctx;
    ka_task_write_lock_bh(&src_ctx->lock);
    ka_list_del(&node->node);
    if (src_ctx->node_num > 0) {
        src_ctx->node_num--;
    }
    ka_task_write_unlock_bh(&src_ctx->lock);
    casm_ctx_put(ctx);

    casm_src_del_handle(udevid, tgid, &node->src_va, &node->src_ex);
}

void casm_src_ctx_show(struct casm_src_ctx *src_ctx, ka_seq_file_t *seq)
{
    struct casm_src_node *node = NULL;
    struct casm_src_node *dump_info = NULL;
    u64 node_num = src_ctx->node_num;
    u64 dump_num = 0;
    u64 i;

    if ((node_num == 0) || (node_num > SHOW_NODE_MAX)) {
        svm_info("Skip show src ctx. (node_num=%llu)\n", node_num);
        return;
    }

    dump_info = (struct casm_src_node *)svm_vzalloc(sizeof(*dump_info) * node_num);
    if (dump_info == NULL) {
        svm_warn("No mem for src show. (num=%llu)\n", node_num);
        return;
    }

    ka_task_read_lock_bh(&src_ctx->lock);
    ka_list_for_each_entry(node, &src_ctx->head, node)
    {
        if (dump_num >= node_num) {
            break;
        }

        dump_info[dump_num] = *node;
        dump_num++;
    }
    ka_task_read_unlock_bh(&src_ctx->lock);

    ka_fs_seq_printf(seq, "casm src info: index    key    owner_tgid  udevid   tgid   va   size    updated_va\n");
    for (i = 0; i < dump_num; i++) {
        struct svm_global_va *src_va = &dump_info[i].src_va;
        ka_fs_seq_printf(seq, "    %llu   0x%llx    %d   %u  %d  0x%llx   0x%llx   0x%llx\n", i, dump_info[i].key,
                         dump_info[i].src_ex.owner_tgid, src_va->udevid, src_va->tgid, src_va->va, src_va->size,
                         dump_info[i].src_ex.updated_va);
    }

    svm_vfree(dump_info);
}

void casm_src_ctx_init(u32 udevid, struct casm_src_ctx *src_ctx)
{
    ka_task_rwlock_init(&src_ctx->lock);
    KA_INIT_LIST_HEAD(&src_ctx->head);
    src_ctx->node_num = 0;
}

static int casm_get_first_src_node_key(struct casm_src_ctx *src_ctx, u64 *key)
{
    int ret = -EFAULT;

    ka_task_read_lock_bh(&src_ctx->lock);
    if (!ka_list_empty_careful(&src_ctx->head)) {
        struct casm_src_node *node = ka_list_first_entry(&src_ctx->head, struct casm_src_node, node);
        *key = node->key;
        ret = 0;
    }
    ka_task_read_unlock_bh(&src_ctx->lock);

    return ret;
}

void casm_src_ctx_uninit(u32 udevid, struct casm_src_ctx *src_ctx)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    u64 last_key = U64_MAX;
    int num = 0;

    while (1) {
        u64 key;

        if (casm_get_first_src_node_key(src_ctx, &key) != 0) {
            break;
        }

        if (last_key == key) {
            svm_warn("Recycle failed. (udevid=%u; key=0x%llx)\n", udevid, key);
            break;
        }

        (void)casm_destroy_key(key, true);
        last_key = key;
        num++;
        ka_try_cond_resched(&stamp);
    }

    if (num > 0) {
        svm_info("Recycle src key. (udevid=%u; num=%d)\n", udevid, num);
    }
}
