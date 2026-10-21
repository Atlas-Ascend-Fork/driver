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
#include "ka_common_pub.h"
#include "ka_system_pub.h"
#include "ka_fs_pub.h"
#include "ka_compiler_pub.h"
#include "ka_sched_pub.h"
#include "ka_ioctl_pub.h"

#include "pbl_feature_loader.h"
#include "dpa_kernel_interface.h"

#include "casm_kernel.h"
#include "svm_kern_log.h"
#include "svm_addr_desc.h"
#include "svm_ioctl_ex.h"
#include "svm_slab.h"
#include "framework_cmd.h"
#include "mwl.h"
#include "dbi_kern.h"
#include "svm_smp.h"
#include "svm_dev_topology.h"
#include "casm_ctx.h"
#include "casm_key.h"
#include "casm_dst.h"

struct casm_src_ex_info {
    bool is_local;
    int owner_tgid;
    u64 updated_va;
};

struct casm_dst_node {
    struct range_rbtree_node range_node;
    u64 key;
    struct svm_global_va src_va;
    struct casm_src_ex_info ex_info;
};

static struct svm_casm_dst_ops *casm_dst_ops = NULL;
static int (*casm_get_src_va_ex_info_handle)(u32 udevid, struct svm_global_va *src_va, struct casm_src_ex *src_ex,
                                             u64 *ex_info) = NULL;

void svm_casm_register_dst_ops(const struct svm_casm_dst_ops *ops)
{
    casm_dst_ops = (struct svm_casm_dst_ops *)ops;
}

static int casm_remote_src_info_query_handle(u32 udevid, u64 key, struct svm_global_va *src_va,
                                             struct casm_src_prop *src_prop)
{
    if (casm_dst_ops == NULL) {
        svm_err("No casm dst ops. (key=0x%llx)\n", key);
        return -EINVAL;
    }

    return casm_dst_ops->src_info_query(udevid, key, src_va, src_prop);
}

static int casm_remote_src_info_get_handle(u32 udevid, u64 key, struct svm_global_va *src_va, int *owner_tgid)
{
    if (casm_dst_ops == NULL) {
        svm_err("No casm dst ops. (key=0x%llx)\n", key);
        return -EINVAL;
    }

    return casm_dst_ops->src_info_get(udevid, key, src_va, owner_tgid);
}

static void casm_remote_src_info_put_handle(u32 udevid, u64 key, struct svm_global_va *src_va, int owner_tgid)
{
    if (casm_dst_ops == NULL) {
        svm_err("No dst casm ops.\n");
        return;
    }

    casm_dst_ops->src_info_put(udevid, key, src_va, owner_tgid);
}

static int casm_src_info_query(u32 udevid, u64 key, struct svm_global_va *src_va, struct casm_src_ex *src_ex)
{
    int ret;

    if (casm_is_local_key(key)) {
        int tgid = ka_task_get_current_tgid();
        ret = casm_get_src_va(key, src_va, src_ex);
        if (ret != 0) {
            return ret;
        }

        if (!svm_mwl_task_is_trusted(src_va->udevid, src_ex->owner_tgid, key, SVM_INVALID_SERVER_ID, tgid)) {
            svm_err("No permission. (udevid=%u; key=0x%llx; va=0x%llx; owner_tgid=%d; checked_tgid=%d)\n", udevid, key,
                    src_va->va, src_ex->owner_tgid, tgid);
            return -EINVAL;
        }
    } else {
        ret = casm_remote_src_info_query_handle(udevid, key, src_va, &src_ex->src_prop);
        if (ret != 0) {
            return ret;
        }
        src_ex->updated_va = src_va->va;
        src_ex->owner_tgid = src_va->tgid;
    }

    return 0;
}

static int casm_src_info_get(u32 udevid, u64 key, struct svm_global_va *src_va, struct casm_src_ex_info *ex_info)
{
    int ret;

    if (casm_is_local_key(key)) {
        struct casm_src_ex src_ex;
        int tgid = ka_task_get_current_tgid();

        ret = casm_get_src_va(key, src_va, &src_ex);
        if (ret != 0) {
            return ret;
        }

        if (!svm_mwl_task_is_trusted(src_va->udevid, src_ex.owner_tgid, key, SVM_INVALID_SERVER_ID, tgid)) {
            svm_err("No permission. (udevid=%u; key=0x%llx; va=0x%llx; owner_tgid=%d; checked_tgid=%d)\n", udevid, key,
                    src_va->va, src_ex.owner_tgid, tgid);
            return -EPERM;
        }

        ret = svm_smp_pin_mem(src_va->udevid, src_ex.owner_tgid, src_va->va, src_va->size, true);
        if (ret != 0) {
            svm_err("Mem pin failed. (udevid=%u; va=0x%llx; size=0x%llx; tgid=%d)\n", src_va->udevid, src_va->va,
                    src_va->size, src_ex.owner_tgid);
            return ret;
        }

        ex_info->is_local = true;
        ex_info->owner_tgid = src_ex.owner_tgid;
        ex_info->updated_va = src_ex.updated_va;
    } else {
        ret = casm_remote_src_info_get_handle(udevid, key, src_va, &ex_info->owner_tgid);
        if (ret != 0) {
            return ret;
        }

        ex_info->is_local = false;
        ex_info->updated_va = 0;
    }

    return 0;
}

static void casm_src_info_put(u32 udevid, u64 key, struct svm_global_va *src_va, struct casm_src_ex_info *ex_info)
{
    u32 host_id = uda_get_host_id();
    int tgid, ret;

    if (ex_info->is_local) {
#ifndef EMU_ST /* UT have not proc bind info, do not delete. */
        if (src_va->udevid != host_id) {
            ret = hal_kernel_apm_query_slave_tgid_by_master(ex_info->owner_tgid, src_va->udevid, PROCESS_CP1, &tgid);
        }
        if ((src_va->udevid == host_id) || (ret != 0) || (src_va->tgid == tgid)) {
#endif
            ret = svm_smp_unpin_mem(src_va->udevid, ex_info->owner_tgid, src_va->va, src_va->size, true);
            if (ret != 0) {
                svm_warn("Mem unpin failed. (udevid=%u; va=0x%llx; size=0x%llx; tgid=%d)\n", src_va->udevid, src_va->va,
                         src_va->size, ex_info->owner_tgid);
            }
#ifndef EMU_ST /* UT have not proc bind info, do not delete. */
        }
#endif
    } else {
        casm_remote_src_info_put_handle(udevid, key, src_va, ex_info->owner_tgid);
    }
}

static int casm_get_first_dst_va(struct casm_dst_ctx *dst_ctx, u64 *va, u64 *size)
{
    struct casm_dst_node *node = NULL;
    struct range_rbtree_node *range_node = NULL;
    int ret = -EINVAL;

    ka_task_read_lock_bh(&dst_ctx->lock);
    range_node = range_rbtree_get_first(&dst_ctx->range_tree);
    if (range_node != NULL) {
        node = ka_container_of(range_node, struct casm_dst_node, range_node);
        *va = node->range_node.start;
        *size = node->range_node.size;
        ret = 0;
    }
    ka_task_read_unlock_bh(&dst_ctx->lock);

    return ret;
}

static struct casm_dst_node *casm_dst_node_search(struct casm_dst_ctx *dst_ctx, u64 va, u64 size)
{
    struct casm_dst_node *node = NULL;
    struct range_rbtree_node *range_node = NULL;

    range_node = range_rbtree_search(&dst_ctx->range_tree, va, size);
    if (range_node != NULL) {
        node = ka_container_of(range_node, struct casm_dst_node, range_node);
        if ((node->range_node.start != va) || (node->range_node.size != size)) {
            node = NULL;
        }
    }

    return node;
}

static int casm_get_src_info(struct casm_dst_ctx *dst_ctx, struct range_rbtree_node *range_node, u64 *key,
                             struct svm_global_va *src_info, struct casm_src_ex_info *ex_info)
{
    struct casm_dst_node *node = NULL;
    int ret = -EINVAL;

    ka_task_read_lock_bh(&dst_ctx->lock);
    node = casm_dst_node_search(dst_ctx, range_node->start, range_node->size);
    if (node != NULL) {
        *key = node->key;
        *src_info = node->src_va;
        *ex_info = node->ex_info;
        ret = 0;
    }
    ka_task_read_unlock_bh(&dst_ctx->lock);

    return ret;
}

int svm_casm_get_src_info(u32 udevid, u64 va, u64 size, struct svm_global_va *src_info)
{
    struct casm_ctx *ctx = NULL;
    struct casm_dst_ctx *dst_ctx = NULL;
    struct range_rbtree_node range_node = {.start = va, .size = size};
    struct casm_src_ex_info ex_info;
    u64 key;
    int ret;

    ctx = casm_ctx_get(udevid, ka_task_get_current_tgid());
    if (ctx == NULL) {
        svm_err("Get task ctx failed. (udevid=%u; tgid=%d)\n", udevid, ka_task_get_current_tgid());
        return -EINVAL;
    }

    dst_ctx = &ctx->dst_ctx;
    ret = casm_get_src_info(dst_ctx, &range_node, &key, src_info, &ex_info);
    casm_ctx_put(ctx);

    if (ret != 0) {
        svm_err("Get failed. (udevid=%u; va=0x%llx; size=0x%llx)\n", udevid, va, size);
        return ret;
    }

    /* Only use the ub-updated va when src and dst are interconnected by UB.
       In PCIe cases, keep the original source va so the normal mapping path is used. */
    if (svm_dev_is_ub_connect(udevid, src_info->server_id, src_info->udevid)) {
        casm_try_to_update_src_va(src_info, ex_info.updated_va);
    }

    return 0;
}

static int casm_add_dst_node(struct casm_dst_ctx *dst_ctx, struct range_rbtree_node *range_node, u64 key,
                             struct svm_global_va *src_va, struct casm_src_ex_info *ex_info)
{
    struct casm_dst_node *node = NULL;
    int ret;

    node = (struct casm_dst_node *)svm_kzalloc(sizeof(*node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        svm_err("No mem. (va=0x%llx)\n", range_node->start);
        return -ENOMEM;
    }

    node->range_node.start = range_node->start;
    node->range_node.size = range_node->size;
    node->key = key;
    node->src_va = *src_va;
    node->ex_info = *ex_info;

    ka_task_write_lock_bh(&dst_ctx->lock);
    ret = range_rbtree_insert(&dst_ctx->range_tree, &node->range_node);
    ka_task_write_unlock_bh(&dst_ctx->lock);
    if (ret != 0) {
        svm_kfree(node);
        svm_err("Insert failed. (va=0x%llx)\n", range_node->start);
    }

    return ret;
}

static int casm_del_dst_node(struct casm_dst_ctx *dst_ctx, u64 va, u64 size)
{
    struct casm_dst_node *node = NULL;

    ka_task_write_lock_bh(&dst_ctx->lock);
    node = casm_dst_node_search(dst_ctx, va, size);
    if (node == NULL) {
        ka_task_write_unlock_bh(&dst_ctx->lock);
        return -EINVAL;
    }

    range_rbtree_erase(&dst_ctx->range_tree, &node->range_node);
    ka_task_write_unlock_bh(&dst_ctx->lock);

    svm_kfree(node);
    return 0;
}

int casm_mem_pin(u32 udevid, u64 va, u64 size, u64 key)
{
    struct casm_ctx *ctx = NULL;
    struct casm_dst_ctx *dst_ctx = NULL;
    struct svm_global_va src_va;
    struct range_rbtree_node range_node = {.start = va, .size = size};
    struct casm_src_ex_info ex_info;
    int ret;

    ret = casm_src_info_get(udevid, key, &src_va, &ex_info);
    if (ret != 0) {
        svm_err("Get src va failed. (udevid=%u; va=0x%llx; key=0x%llx)\n", udevid, va, key);
        return ret;
    }

    if ((size == 0) || ((va + size) < va) || (size != src_va.size)) {
        svm_err("Size is invalid. (va=0x%llx; size=0x%llx; src_size=0x%llx)\n", va, size, src_va.size);
        (void)casm_src_info_put(udevid, key, &src_va, &ex_info);
        return -EINVAL;
    }

    ctx = casm_ctx_get(udevid, ka_task_get_current_tgid());
    if (ctx == NULL) {
        svm_err("Get task ctx failed. (udevid=%u; tgid=%d)\n", udevid, ka_task_get_current_tgid());
        (void)casm_src_info_put(udevid, key, &src_va, &ex_info);
        return -EINVAL;
    }

    dst_ctx = &ctx->dst_ctx;
    ret = casm_add_dst_node(dst_ctx, &range_node, key, &src_va, &ex_info);
    casm_ctx_put(ctx);

    if (ret != 0) {
        (void)casm_src_info_put(udevid, key, &src_va, &ex_info);
    }

    return ret;
}

int casm_mem_unpin(u32 udevid, int tgid, u64 va, u64 size) /* for ut test, cannot add static */
{
    struct casm_ctx *ctx = NULL;
    struct casm_dst_ctx *dst_ctx = NULL;
    struct svm_global_va src_va;
    struct range_rbtree_node range_node = {.start = va, .size = size};
    struct casm_src_ex_info ex_info;
    u64 key;
    int ret;

    if ((size == 0) || ((va + size) < va)) {
        svm_err("Size is invalid. (va=0x%llx; size=0x%llx)\n", va, size);
        return -EINVAL;
    }

    ctx = casm_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Get task ctx failed. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    dst_ctx = &ctx->dst_ctx;
    ret = casm_get_src_info(dst_ctx, &range_node, &key, &src_va, &ex_info);
    if (ret != 0) {
        casm_ctx_put(ctx);
        svm_err("Get src failed. (udevid=%u; va=0x%llx; size=0x%llx)\n", udevid, va, size);
        return ret;
    }

    ret = casm_del_dst_node(dst_ctx, va, size);
    casm_ctx_put(ctx);
    if (ret != 0) {
        return ret;
    }

    (void)casm_src_info_put(udevid, key, &src_va, &ex_info);

    return 0;
}

void casm_dst_ctx_show(struct casm_dst_ctx *dst_ctx, ka_seq_file_t *seq)
{
    struct range_rbtree_node *range_node, *next;
    int i = 0;

    ka_task_read_lock_bh(&dst_ctx->lock);

    ka_base_rbtree_postorder_for_each_entry_safe(range_node, next, &dst_ctx->range_tree.root, node)
    {
        struct casm_dst_node *node = ka_container_of(range_node, struct casm_dst_node, range_node);
        struct svm_global_va *src_va = &node->src_va;

        if (i == 0) {
            ka_fs_seq_printf(seq, "casm dst info:\n");
            ka_fs_seq_printf(seq, "   index  va              size     owner_tgid  "
                                  "src_va(udevid    tgid   va      size)     updated_va    key\n");
        }
        ka_fs_seq_printf(
            seq, "   %d     0x%llx     0x%llx      %d     (%u  %d      0x%llx      0x%llx)    0x%llx   0x%llx\n", i++,
            node->range_node.start, node->range_node.size, node->ex_info.owner_tgid, src_va->udevid, src_va->tgid,
            src_va->va, src_va->size, node->ex_info.updated_va, node->key);
    }

    ka_task_read_unlock_bh(&dst_ctx->lock);
}

void casm_dst_ctx_init(u32 udevid, struct casm_dst_ctx *dst_ctx)
{
    ka_task_rwlock_init(&dst_ctx->lock);
    range_rbtree_init(&dst_ctx->range_tree);
}

static int casm_dst_node_recycle(u32 udevid, int tgid, struct casm_dst_ctx *dst_ctx)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    u64 last_va = 0;
    int recycle_num = 0;

    do {
        u64 va, size;
        int ret = casm_get_first_dst_va(dst_ctx, &va, &size);
        if (ret != 0) {
            break;
        }

        if (last_va == va) {
            svm_warn("Recycle failed. (udevid=%u; va=0x%llx; size=0x%llx)\n", udevid, va, size);
            break;
        }

        (void)casm_mem_unpin(udevid, tgid, va, size);
        last_va = va;
        recycle_num++;
        ka_try_cond_resched(&stamp);
    } while (1);

    return recycle_num;
}

void casm_dst_ctx_uninit(u32 udevid, int tgid, struct casm_dst_ctx *dst_ctx)
{
    int recycle_num = casm_dst_node_recycle(udevid, tgid, dst_ctx);
    if (recycle_num > 0) {
        svm_warn("Recycle mem. (udevid=%u; recycle_num=%d)\n", udevid, recycle_num);
    }
}

void svm_casm_register_get_src_va_ex_info_handle(int (*handle)(u32 udevid, struct svm_global_va *src_va,
                                                               struct casm_src_ex *src_ex, u64 *ex_info))
{
    casm_get_src_va_ex_info_handle = handle;
}

#ifndef EMU_ST /* ctrl for linear_mem enable. */
static int casm_get_src_va_ex_info(u32 udevid, struct svm_global_va *src_va, struct casm_src_ex *src_ex, u64 *ex_info)
{
    return casm_get_src_va_ex_info_handle(udevid, src_va, src_ex, ex_info);
}
#endif

static int casm_ioctl_get_src_va(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_casm_get_src_va_para para;
    struct casm_src_ex src_ex = {0};
    int ret;

    ret = (int)ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    ret = casm_src_info_query(udevid, para.key, &para.src_va, &src_ex);
    if (ret != 0) {
        return ret;
    }
    para.updated_va = src_ex.updated_va;
    para.src_prop = src_ex.src_prop;

    para.ex_info = 0;
    if (casm_get_src_va_ex_info_handle != NULL) {
        ret = casm_get_src_va_ex_info(udevid, &para.src_va, &src_ex, &para.ex_info);
        if (ret != 0) {
            svm_err("Get src va ex info failed. (udevid=%u)\n", udevid);
            return ret;
        }
    }

    if (ka_base_copy_to_user((void __ka_user *)(uintptr_t)arg, &para, sizeof(para)) != 0) {
        svm_err("copy_to_user fail.\n");
        return -EINVAL;
    }

    return ret;
}

static int casm_ioctl_mem_pin(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_casm_mem_pin_para para;
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    return casm_mem_pin(udevid, para.va, para.size, para.key);
}

static int casm_ioctl_mem_unpin(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_casm_mem_unpin_para para;
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    return casm_mem_unpin(udevid, ka_task_get_current_tgid(), para.va, para.size);
}

int casm_dst_init(void)
{
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_CASM_GET_SRC_VA), casm_ioctl_get_src_va);
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_CASM_MEM_PIN), casm_ioctl_mem_pin);
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_CASM_MEM_UNPIN), casm_ioctl_mem_unpin);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(casm_dst_init, FEATURE_LOADER_STAGE_5);
