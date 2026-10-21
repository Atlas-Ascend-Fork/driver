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
#include "ka_common_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_fs_pub.h"
#include "ka_list_pub.h"
#include "ka_compiler_pub.h"
#include "ka_sched_pub.h"

#include "svm_pub.h"
#include "svm_kern_log.h"
#include "svm_slab.h"
#include "mwl.h"
#include "mwl_ctx.h"
#include "mwl_core.h"

#define MWL_TASK_ADD 0U
#define MWL_TASK_DEL 1U

static void mwl_mem_node_get_range(u64 id, u64 va, u64 size, u64 *range_start, u64 *range_size)
{
    *range_start = (id == MWL_ID_NO_USE) ? va : id;
    *range_size = (id == MWL_ID_NO_USE) ? size : 1;
}

static struct range_rbtree *mwl_mem_get_range_tree(struct mwl_ctx *mwl_ctx, u64 id)
{
    return (id == MWL_ID_NO_USE) ? &mwl_ctx->va_range_tree : &mwl_ctx->id_range_tree;
}

static struct range_rbtree_node *mwl_mem_get_range_node(struct mwl_mem_node *mem_node, u64 id)
{
    return (id == MWL_ID_NO_USE) ? &mem_node->va_range_node : &mem_node->id_range_node;
}

static struct mwl_mem_node *mwl_mem_get_mem_node(struct range_rbtree_node *range_node, bool use_id_key)
{
    if (use_id_key) {
        return ka_container_of(range_node, struct mwl_mem_node, id_range_node);
    }

    return ka_container_of(range_node, struct mwl_mem_node, va_range_node);
}

static struct range_rbtree_node *mwl_mem_get_key_range_node(struct mwl_mem_node *mem_node)
{
    return mem_node->use_id_key ? &mem_node->id_range_node : &mem_node->va_range_node;
}

static u64 mwl_mem_get_key_start(struct mwl_mem_node *mem_node)
{
    return mwl_mem_get_key_range_node(mem_node)->start;
}

static u64 mwl_mem_get_key_size(struct mwl_mem_node *mem_node)
{
    return mwl_mem_get_key_range_node(mem_node)->size;
}

static struct mwl_mem_node *mwl_mem_node_create(struct mwl_ctx *mwl_ctx, u64 id, u64 va, u64 size)
{
    struct mwl_mem_node *mem_node = svm_kzalloc(sizeof(struct mwl_mem_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (mem_node == NULL) {
        svm_err("Alloc mem node failed, (udevid=%u; tgid=%d; va=%llx; size=%llx)\n", mwl_ctx->udevid, mwl_ctx->tgid, va,
                size);
        return NULL;
    }

    mem_node->use_id_key = (id != MWL_ID_NO_USE);
    mwl_mem_node_get_range(id, va, size, &mem_node->id_range_node.start, &mem_node->id_range_node.size);
    mem_node->va_range_node.start = va;
    mem_node->va_range_node.size = size;
    mem_node->start = va;
    mem_node->size = size;
    KA_INIT_LIST_HEAD(&mem_node->task_list);

    return mem_node;
}

static void mwl_mem_node_destroy(struct mwl_mem_node *mem_node)
{
    svm_kfree(mem_node);
}

static int mwl_add_mem(struct mwl_ctx *mwl_ctx, u64 id, u64 va, u64 size)
{
    struct mwl_mem_node *mem_node = NULL;
    struct range_rbtree *range_tree = NULL;
    int ret;

    mem_node = mwl_mem_node_create(mwl_ctx, id, va, size);
    if (mem_node == NULL) {
        return -ENOMEM;
    }

    range_tree = mwl_mem_get_range_tree(mwl_ctx, id);
    ka_task_down_write(&mwl_ctx->rwsem);
    ret = range_rbtree_insert(range_tree, mwl_mem_get_range_node(mem_node, id));
    ka_task_up_write(&mwl_ctx->rwsem);
    if (ret != 0) {
        mwl_mem_node_destroy(mem_node);
        svm_err("Insert failed. (udevid=%u; tgid=%d; va=%llx; size=%llx)\n", mwl_ctx->udevid, mwl_ctx->tgid, va, size);
    }
    return ret;
}

int svm_mwl_add_mem(u32 udevid, int tgid, u64 id, u64 va, u64 size)
{
    struct mwl_ctx *mwl_ctx = NULL;
    int ret;

    mwl_ctx = mwl_ctx_get(udevid, tgid);
    if (mwl_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = mwl_add_mem(mwl_ctx, id, va, size);
    mwl_ctx_put(mwl_ctx);

    return ret;
}

#define MWL_MAX_TASK_NODE_NUM 65535
static int mwl_add_task_list(struct mwl_mem_node *mem_node, u32 server_id, int tgid)
{
    struct task_node *task_node = NULL, *entry = NULL;

    /* same pid insert, skip it. */
    ka_list_for_each_entry(entry, &mem_node->task_list, list)
    {
        if ((entry->server_id == server_id) && (entry->tgid == tgid)) {
            entry->set_time = ka_system_ktime_get_ns(); // should update set time
            return 0;
        }

        if ((entry->tgid == SVM_ANY_TASK_ID) && (tgid != SVM_ANY_TASK_ID)) {
            svm_err("A task has been enabled; adding a task is not permitted.\n");
            return -EPERM;
        }

        if ((entry->tgid != SVM_ANY_TASK_ID) && (tgid == SVM_ANY_TASK_ID)) {
            svm_err("Had set pid; not allowed to enable any task.\n");
            return -EPERM;
        }
    }

    if (mem_node->node_num >= MWL_MAX_TASK_NODE_NUM) {
        svm_err("Task node num out of range. (tgid=%d; va=%llx; size=%llx; node_num=%u; max=%u)\n", tgid,
                mwl_mem_get_key_start(mem_node), mwl_mem_get_key_size(mem_node), mem_node->node_num,
                (u32)MWL_MAX_TASK_NODE_NUM);
        return -EINVAL;
    }

    task_node = svm_kvzalloc(sizeof(struct task_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (task_node == NULL) {
        svm_err("Alloc task node failed, ( tgid=%d; va=%llx; size=%llx)\n", tgid, mwl_mem_get_key_start(mem_node),
                mwl_mem_get_key_size(mem_node));
        return -ENOMEM;
    }

    task_node->server_id = server_id;
    task_node->tgid = tgid;
    task_node->set_time = ka_system_ktime_get_ns();

    ka_list_add(&task_node->list, &mem_node->task_list);
    mem_node->node_num++;

    return 0;
}

static int mwl_del_task_list(struct mwl_mem_node *mem_node, u32 server_id, int tgid)
{
    struct task_node *entry = NULL;

    ka_list_for_each_entry(entry, &mem_node->task_list, list)
    {
        if ((entry->server_id == server_id) && (entry->tgid == tgid)) {
            ka_list_del(&entry->list);
            svm_kvfree(entry);
            mem_node->node_num--;
            return 0;
        }
    }

    return -EINVAL;
}

static void mwl_del_task_list_all(struct mwl_mem_node *mem_node)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    struct task_node *entry, *tmp;

    ka_list_for_each_entry_safe(entry, tmp, &mem_node->task_list, list)
    {
        ka_try_cond_resched(&stamp);
        ka_list_del(&entry->list);
        svm_kvfree(entry);
        mem_node->node_num--;
    }
}

static struct mwl_mem_node *mwl_mem_node_search(struct mwl_ctx *mwl_ctx, u64 id, u64 va, u64 size)
{
    struct mwl_mem_node *mem_node = NULL;
    struct range_rbtree_node *range_node = NULL;
    struct range_rbtree *range_tree = NULL;
    u64 range_start, range_size;

    mwl_mem_node_get_range(id, va, size, &range_start, &range_size);
    range_tree = mwl_mem_get_range_tree(mwl_ctx, id);
    range_node = range_rbtree_search(range_tree, range_start, range_size);
    if (range_node != NULL) {
        mem_node = mwl_mem_get_mem_node(range_node, id != MWL_ID_NO_USE);
    }

    return mem_node;
}

static int mwl_check_mem(struct mwl_ctx *mwl_ctx, u64 id, u64 va, u64 size)
{
    struct mwl_mem_node *mem_node = NULL;
    int ret;

    ka_task_down_read(&mwl_ctx->rwsem);
    mem_node = mwl_mem_node_search(mwl_ctx, id, va, size);
    ret = (mem_node == NULL) ? -EINVAL : 0;
    ka_task_up_read(&mwl_ctx->rwsem);

    return ret;
}

static bool mwl_can_del_mem(struct mwl_ctx *mwl_ctx, struct mwl_mem_node *mem_node, u64 id, u64 va, u64 size)
{
    if (mem_node->use_id_key) {
        return true;
    }
    if ((mem_node->va_range_node.start == va) && (mem_node->va_range_node.size == size)) {
        return true;
    }

    svm_err("Mem range not match. (udevid=%u; tgid=%d; id=%llu; va=0x%llx; size=%llu; "
            "mem_va=0x%llx; mem_size=%llu)\n",
            mwl_ctx->udevid, mwl_ctx->tgid, id, va, size, mem_node->va_range_node.start, mem_node->va_range_node.size);
    return false;
}

int svm_mwl_check_mem(u32 udevid, int tgid, u64 id, u64 va, u64 size)
{
    struct mwl_ctx *mwl_ctx = NULL;
    int ret;

    mwl_ctx = mwl_ctx_get(udevid, tgid);
    if (mwl_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = mwl_check_mem(mwl_ctx, id, va, size);
    mwl_ctx_put(mwl_ctx);

    return ret;
}

static int mwl_del_mem(struct mwl_ctx *mwl_ctx, u64 id, u64 va, u64 size)
{
    struct mwl_mem_node *mem_node = NULL;
    struct range_rbtree *range_tree = NULL;

    ka_task_down_write(&mwl_ctx->rwsem);
    mem_node = mwl_mem_node_search(mwl_ctx, id, va, size);
    if ((mem_node == NULL) || !mwl_can_del_mem(mwl_ctx, mem_node, id, va, size)) {
        ka_task_up_write(&mwl_ctx->rwsem);
        svm_err("Search failed. (udevid=%u; tgid=%d; id=%llu; va=%llx; size=%llu)\n", mwl_ctx->udevid, mwl_ctx->tgid,
                id, va, size);
        return -EINVAL;
    }

    mwl_del_task_list_all(mem_node);

    range_tree = mwl_mem_get_range_tree(mwl_ctx, id);
    range_rbtree_erase(range_tree, mwl_mem_get_range_node(mem_node, id));

    ka_task_up_write(&mwl_ctx->rwsem);

    mwl_mem_node_destroy(mem_node);

    return 0;
}

int svm_mwl_del_mem(u32 udevid, int tgid, u64 id, u64 va, u64 size)
{
    struct mwl_ctx *mwl_ctx = NULL;
    int ret;

    mwl_ctx = mwl_ctx_get(udevid, tgid);
    if (mwl_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = mwl_del_mem(mwl_ctx, id, va, size);
    mwl_ctx_put(mwl_ctx);
    return ret;
}

static int mwl_op_trusted_task(struct mwl_ctx *mwl_ctx, u64 id, u32 trusted_server_id, int trusted_tgid, u32 op)
{
    struct mwl_mem_node *mem_node = NULL;
    int ret;

    ka_task_down_write(&mwl_ctx->rwsem);
    mem_node = mwl_mem_node_search(mwl_ctx, id, 0, 0);
    if (mem_node == NULL) {
        ka_task_up_write(&mwl_ctx->rwsem);
        svm_err("Search failed. (udevid=%u; tgid=%d; id=%llu)\n", mwl_ctx->udevid, mwl_ctx->tgid, id);
        return -EINVAL;
    }

    if (op == MWL_TASK_ADD) {
        ret = mwl_add_task_list(mem_node, trusted_server_id, trusted_tgid);
    } else {
        ret = mwl_del_task_list(mem_node, trusted_server_id, trusted_tgid);
    }
    ka_task_up_write(&mwl_ctx->rwsem);

    return ret;
}

static int svm_mwl_op_trusted_task(u32 udevid, int tgid, u64 id, u32 trusted_server_id, int trusted_tgid, u32 op)
{
    struct mwl_ctx *mwl_ctx = NULL;
    int ret;

    if (ka_unlikely(ka_task_get_current_tgid() != tgid)) {
        svm_err("Non-creator has no permission to operate the wlist. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EPERM;
    }

    mwl_ctx = mwl_ctx_get(udevid, tgid);
    if (mwl_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = mwl_op_trusted_task(mwl_ctx, id, trusted_server_id, trusted_tgid, op);

    mwl_ctx_put(mwl_ctx);

    return ret;
}

int svm_mwl_add_trusted_task(u32 udevid, int tgid, u64 id, u32 trusted_server_id, int trusted_tgid)
{
    return svm_mwl_op_trusted_task(udevid, tgid, id, trusted_server_id, trusted_tgid, MWL_TASK_ADD);
}

int svm_mwl_del_trusted_task(u32 udevid, int tgid, u64 id, u32 trusted_server_id, int trusted_tgid)
{
    return svm_mwl_op_trusted_task(udevid, tgid, id, trusted_server_id, trusted_tgid, MWL_TASK_DEL);
}

static unsigned long mwl_get_start_time(void)
{
    return ka_task_get_current_group_starttime();
}

static bool mwl_task_is_trusted(struct mwl_ctx *mwl_ctx, u64 id, u32 checked_server_id, int checked_tgid)
{
    struct mwl_mem_node *mem_node = NULL;
    struct task_node *entry;

    ka_task_down_read(&mwl_ctx->rwsem);
    mem_node = mwl_mem_node_search(mwl_ctx, id, 0, 0);
    if (mem_node == NULL) {
        ka_task_up_read(&mwl_ctx->rwsem);
        svm_err("Search failed. (udevid=%u; tgid=%d; id=%llu)\n", mwl_ctx->udevid, mwl_ctx->tgid, id);
        return false;
    }

    ka_list_for_each_entry(entry, &mem_node->task_list, list)
    {
        if ((entry->tgid == SVM_ANY_TASK_ID) ||
            ((entry->server_id == checked_server_id) && (entry->tgid == checked_tgid) &&
             (ka_system_time_after(entry->set_time, mwl_get_start_time())))) {
            ka_task_up_read(&mwl_ctx->rwsem);
            return true;
        }
    }

    ka_task_up_read(&mwl_ctx->rwsem);

    return (checked_tgid == mwl_ctx->tgid) ? true : false; /* owner self is trusted */
}

bool svm_mwl_task_is_trusted(u32 udevid, int tgid, u64 id, u32 checked_server_id, int checked_tgid)
{
    struct mwl_ctx *mwl_ctx = NULL;
    bool flag;

    mwl_ctx = mwl_ctx_get(udevid, tgid);
    if (mwl_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return false;
    }

    flag = mwl_task_is_trusted(mwl_ctx, id, checked_server_id, checked_tgid);

    mwl_ctx_put(mwl_ctx);

    return flag;
}

static void mwl_mem_show_header(ka_seq_file_t *seq)
{
    ka_fs_seq_printf(seq, "   index     va     size    trusted pid(serverid-pid1, serverid-pid2, ...) \n");
}

static void mwl_mem_show_node(ka_seq_file_t *seq, struct mwl_mem_node *mem_node, int *index)
{
    struct task_node *entry;
    int j = 0;

    ka_fs_seq_printf(seq, "   %d       %llx       %llx (", (*index)++, mem_node->start, mem_node->size);
    ka_list_for_each_entry(entry, &mem_node->task_list, list)
    {
        if (j++ == 0) {
            ka_fs_seq_printf(seq, "%u-%d", entry->server_id, entry->tgid);
        } else {
            ka_fs_seq_printf(seq, ", %u-%d", entry->server_id, entry->tgid);
        }
    }
    ka_fs_seq_printf(seq, ")\n");
}

static void mwl_mem_show_tree(struct range_rbtree *range_tree, bool use_id_key, ka_seq_file_t *seq, int *index)
{
    struct range_rbtree_node *range_node, *next;

    ka_base_rbtree_postorder_for_each_entry_safe(range_node, next, &range_tree->root, node)
    {
        struct mwl_mem_node *mem_node = mwl_mem_get_mem_node(range_node, use_id_key);
        if (*index == 0) {
            mwl_mem_show_header(seq);
        }
        mwl_mem_show_node(seq, mem_node, index);
    }
}

void mwl_mem_show(struct mwl_ctx *mwl_ctx, ka_seq_file_t *seq)
{
    int i = 0;

    ka_task_down_read(&mwl_ctx->rwsem);

    ka_fs_seq_printf(seq, "mwl: udevid %u tgid %d mem num %u\n", mwl_ctx->udevid, mwl_ctx->tgid,
                     mwl_ctx->id_range_tree.node_num + mwl_ctx->va_range_tree.node_num);

    mwl_mem_show_tree(&mwl_ctx->id_range_tree, true, seq, &i);
    mwl_mem_show_tree(&mwl_ctx->va_range_tree, false, seq, &i);

    ka_task_up_read(&mwl_ctx->rwsem);
}

static struct mwl_mem_node *mwl_mem_get_recycle_node(struct mwl_ctx *mwl_ctx, struct range_rbtree **range_tree,
                                                     struct range_rbtree_node **range_node)
{
    *range_tree = &mwl_ctx->id_range_tree;
    *range_node = range_rbtree_get_first(*range_tree);
    if (*range_node != NULL) {
        return mwl_mem_get_mem_node(*range_node, true);
    }

    *range_tree = &mwl_ctx->va_range_tree;
    *range_node = range_rbtree_get_first(*range_tree);
    if (*range_node != NULL) {
        return mwl_mem_get_mem_node(*range_node, false);
    }

    return NULL;
}

static int _mwl_mem_recycle(struct mwl_ctx *mwl_ctx)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    int recycle_num = 0;

    do {
        struct range_rbtree *range_tree = NULL;
        struct range_rbtree_node *range_node = NULL;
        struct mwl_mem_node *mem_node = mwl_mem_get_recycle_node(mwl_ctx, &range_tree, &range_node);

        if (mem_node == NULL) {
            break;
        }
        recycle_num++;
        mwl_del_task_list_all(mem_node);
        range_rbtree_erase(range_tree, range_node);
        mwl_mem_node_destroy(mem_node);
        ka_try_cond_resched(&stamp);
    } while (1);

    return recycle_num;
}

void mwl_mem_recycle(struct mwl_ctx *mwl_ctx)
{
    int recycle_num = 0;

    ka_task_down_write(&mwl_ctx->rwsem);
    recycle_num = _mwl_mem_recycle(mwl_ctx);
    ka_task_up_write(&mwl_ctx->rwsem);

    if (recycle_num > 0) {
        svm_warn("Recycle mem. (udevid=%u; tgid=%d; recycle_num=%d)\n", mwl_ctx->udevid, mwl_ctx->tgid, recycle_num);
    }
}
