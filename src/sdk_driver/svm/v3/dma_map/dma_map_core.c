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
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_fs_pub.h"
#include "ka_compiler_pub.h"
#include "ka_sched_pub.h"
#include "ka_ioctl_pub.h"

#include "pbl_feature_loader.h"
#include "pbl_uda.h"

#include "svm_slab.h"
#include "svm_kern_log.h"
#include "dma_map_flag.h"
#include "dma_map_ioctl.h"
#include "dma_map_ops.h"
#include "dma_map_ctx.h"
#include "dma_map_core.h"

static const struct svm_dma_map_ops *g_dma_map_ops;

int svm_dma_map_register_ops(const struct svm_dma_map_ops *ops)
{
    if ((ops == NULL) || (ops->do_dma_map == NULL) || (ops->do_dma_unmap == NULL) || (ops->do_dma_remap == NULL)) {
        svm_err("Invalid dma map ops.\n");
        return -EINVAL;
    }

    g_dma_map_ops = ops;
    return 0;
}

static struct dma_map_node *dma_map_node_create(struct svm_global_va *dst_va, u64 dst_page_size, u32 flag)
{
    struct dma_map_node *map_node = svm_kvzalloc(sizeof(*map_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (map_node == NULL) {
        svm_err("Alloc map_node node failed. (start=0x%llx; size=0x%llx)\n", dst_va->va, dst_va->size);
        return NULL;
    }

    map_node->range_node.start = dst_va->va;
    map_node->range_node.size = dst_va->size;
    map_node->raw_va = dst_va->va;
    map_node->raw_size = dst_va->size;
    map_node->align_dst_va = *dst_va;
    map_node->align_dst_va.va = ka_base_round_down(dst_va->va, dst_page_size);
    map_node->align_dst_va.size = ka_base_round_up(map_node->raw_va - map_node->align_dst_va.va + dst_va->size,
                                                   dst_page_size);
    map_node->is_write = svm_dma_map_flag_is_access_write(flag) ? true : false;
    map_node->flag = flag;
    ka_base_atomic_set(&map_node->refcnt, 0);
    ka_base_kref_init(&map_node->kref);

    return map_node;
}

static inline void dma_map_node_destroy(struct dma_map_node *map_node)
{
    svm_kvfree(map_node);
}

static void svm_dma_map_node_release(ka_kref_t *kref)
{
    struct dma_map_node *map_node = ka_container_of(kref, struct dma_map_node, kref);

    (void)g_dma_map_ops->do_dma_unmap(map_node);
    dma_map_node_destroy(map_node);
}

static void svm_dma_kref_put(struct dma_map_node *map_node)
{
    ka_base_kref_put(&map_node->kref, svm_dma_map_node_release);
}

static struct dma_map_node *dma_map_node_search(struct dma_map_ctx *ctx, struct svm_global_va *dst_va)
{
    struct dma_map_node *map_node = NULL;
    struct range_rbtree_node *range_node = NULL;

    range_node = range_rbtree_search(&ctx->range_tree, dst_va->va, dst_va->size);
    if (range_node != NULL) {
        map_node = ka_container_of(range_node, struct dma_map_node, range_node);
        if ((map_node->align_dst_va.udevid == dst_va->udevid) && (map_node->align_dst_va.tgid == dst_va->tgid)) {
            return map_node;
        }
    }

    return NULL;
}

static int svm_dma_map_addr_create_node(u32 udevid, int tgid, struct svm_global_va *dst_va, u64 dst_page_size, u32 flag,
                                        struct dma_map_node **map_node_out)
{
    struct dma_map_node *map_node = NULL;
    int ret;

    if (g_dma_map_ops == NULL) {
        svm_err("Dma map ops is not registered.\n");
        return -ENODEV;
    }

    map_node = dma_map_node_create(dst_va, dst_page_size, flag);
    if (map_node == NULL) {
        return -ENOMEM;
    }

    map_node->udevid = udevid;
    map_node->tgid = tgid;

    ret = g_dma_map_ops->do_dma_map(udevid, map_node, dst_page_size);
    if (ret != 0) {
        dma_map_node_destroy(map_node);
        return ret;
    }

    *map_node_out = map_node;
    return 0;
}

static int svm_dma_map_addr_by_handle(u32 udevid, int tgid, struct svm_global_va *dst_va, u64 dst_page_size, u32 flag,
                                      void **handle)
{
    struct dma_map_node *map_node = NULL;
    int ret = svm_dma_map_addr_create_node(udevid, tgid, dst_va, dst_page_size, flag, &map_node);
    if (ret == 0) {
        *handle = (void *)map_node;
    }

    return ret;
}

static int _svm_dma_map_addr(struct dma_map_ctx *ctx, struct svm_global_va *dst_va, u64 dst_page_size, u32 flag)
{
    struct dma_map_node *map_node = NULL;
    int ret;

    ka_task_read_lock_bh(&ctx->lock);
    map_node = dma_map_node_search(ctx, dst_va);
    if (map_node != NULL) { /* host_register_post_malloc may insert already, do not print. */
        ka_task_read_unlock_bh(&ctx->lock);
        return -EBUSY;
    }
    ka_task_read_unlock_bh(&ctx->lock);

    ret = svm_dma_map_addr_create_node(ctx->udevid, ctx->tgid, dst_va, dst_page_size, flag, &map_node);
    if (ret != 0) {
        return ret;
    }

    ka_task_write_lock_bh(&ctx->lock);
    ret = range_rbtree_insert(&ctx->range_tree, &map_node->range_node);
    ka_task_write_unlock_bh(&ctx->lock);
    if (ret != 0) {
        (void)g_dma_map_ops->do_dma_unmap(map_node);
        dma_map_node_destroy(map_node);
        svm_debug("Insert failed. (udevid=%u; tgid=%d; start=0x%llx; size=0x%llx)\n", ctx->udevid, ctx->tgid,
                  dst_va->va, dst_va->size);
        ret = -EBUSY;
    }

    return ret;
}

static int svm_dma_map_addr_by_ctx(u32 udevid, int tgid, struct svm_global_va *dst_va, u64 dst_page_size, u32 flag)
{
    struct dma_map_ctx *ctx = NULL;
    int ret;

    ctx = dma_map_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = _svm_dma_map_addr(ctx, dst_va, dst_page_size, flag);
    dma_map_ctx_put(ctx);

    return ret;
}

int svm_dma_map_addr(u32 udevid, int tgid, struct svm_global_va *dst_va, u64 dst_page_size, u32 flag, void **handle)
{
    u64 page_num;

    if (dst_page_size == 0ULL) {
        svm_err("Invalid page size. (page_size=%llu)\n", dst_page_size);
        return -EINVAL;
    }

    page_num = svm_get_align_up_num(dst_va->va, dst_va->size, dst_page_size);
    if ((page_num == 0ULL) || (page_num >= KA_U32_MAX)) {
        svm_err("Invalid size. (size=0x%llx; page_size=%llu)\n", dst_va->size, dst_page_size);
        return -EINVAL;
    }

    if (handle != NULL) {
        return svm_dma_map_addr_by_handle(udevid, tgid, dst_va, dst_page_size, flag, handle);
    } else {
        return svm_dma_map_addr_by_ctx(udevid, tgid, dst_va, dst_page_size, flag);
    }
}

void svm_dma_addr_put_by_handle(void *handle)
{
    svm_dma_kref_put((struct dma_map_node *)handle);
}

static int _svm_dma_unmap_addr(struct dma_map_ctx *ctx, struct svm_global_va *dst_va)
{
    struct dma_map_node *map_node = NULL;
    int ret = 0;

    ka_task_write_lock_bh(&ctx->lock);

    map_node = dma_map_node_search(ctx, dst_va);
    if (map_node == NULL) { /* host_unregister_pre_free may erase already, do not print. */
        ka_task_write_unlock_bh(&ctx->lock);
        return -EINVAL;
    }

    if ((map_node->raw_va != dst_va->va) || (map_node->raw_size != dst_va->size)) {
        ka_task_write_unlock_bh(&ctx->lock);
        svm_err("Not map addr. (udevid=%u; tgid=%d; start=0x%llx; size=0x%llx)\n", ctx->udevid, ctx->tgid, dst_va->va,
                dst_va->size);
        return -EINVAL;
    }

    if ((ka_base_atomic_read(&map_node->refcnt) > 0) || map_node->is_remapping) {
        ka_task_write_unlock_bh(&ctx->lock);
        return -EBUSY;
    }

    range_rbtree_erase(&ctx->range_tree, &map_node->range_node);
    ka_task_write_unlock_bh(&ctx->lock);

    svm_dma_kref_put(map_node);

    return ret;
}

int svm_dma_unmap_addr(u32 udevid, int tgid, struct svm_global_va *dst_va)
{
    struct dma_map_ctx *ctx = NULL;
    int ret;

    ctx = dma_map_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = _svm_dma_unmap_addr(ctx, dst_va);
    dma_map_ctx_put(ctx);

    return ret;
}

int svm_dma_map_get_remap_seg_range(struct dma_map_node *map_node, u64 remap_va, u64 remap_size, u64 *start_seg_idx,
                                    u64 *remap_seg_num)
{
    struct svm_dma_addr_info *addr_info = &map_node->addr_info;
    u64 seg_va = map_node->align_dst_va.va;
    u64 matched_size = 0;
    bool matched = false;
    u64 i;

    for (i = 0; i < addr_info->dma_addr_seg_num; i++) {
        u64 seg_size = addr_info->seg[i].size;

        if (!matched) {
            if (seg_va != remap_va) {
                seg_va += seg_size;
                continue;
            }
            *start_seg_idx = i;
            matched = true;
        }

        if ((seg_size == 0) || (matched_size >= remap_size) || (seg_size > (remap_size - matched_size))) {
            return -EINVAL;
        }
        matched_size += seg_size;
        if (matched_size == remap_size) {
            *remap_seg_num = i - *start_seg_idx + 1;
            return 0;
        }
    }

    return -EINVAL;
}

static int _svm_dma_remap_addr(struct dma_map_ctx *ctx, struct svm_global_va *dst_va)
{
    struct dma_map_node *map_node = NULL;
    u64 start_seg_idx;
    u64 remap_seg_num;
    int ret;

    ka_task_write_lock_bh(&ctx->lock);
    map_node = dma_map_node_search(ctx, dst_va);
    if (map_node == NULL) {
        ka_task_write_unlock_bh(&ctx->lock);
        return -EINVAL;
    }

    ret = svm_dma_map_get_remap_seg_range(map_node, dst_va->va, dst_va->size, &start_seg_idx, &remap_seg_num);
    if (ret != 0) {
        ka_task_write_unlock_bh(&ctx->lock);
        svm_err("Remap range does not align with dma seg boundaries. "
                "(udevid=%u; tgid=%d; va=0x%llx; size=0x%llx)\n",
                ctx->udevid, ctx->tgid, dst_va->va, dst_va->size);
        return ret;
    }

    if ((ka_base_atomic_read(&map_node->refcnt) > 0) || map_node->is_remapping) {
        ka_task_write_unlock_bh(&ctx->lock);
        return -EBUSY;
    }
    map_node->is_remapping = true;
    ka_task_write_unlock_bh(&ctx->lock);

    ret = g_dma_map_ops->do_dma_remap(ctx->udevid, map_node, dst_va, start_seg_idx, remap_seg_num);

    ka_task_write_lock_bh(&ctx->lock);
    map_node->is_remapping = false;
    ka_task_write_unlock_bh(&ctx->lock);

    return ret;
}

int svm_dma_remap_addr(u32 udevid, int tgid, struct svm_global_va *dst_va)
{
    struct dma_map_ctx *ctx = NULL;
    int ret;

    ctx = dma_map_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = _svm_dma_remap_addr(ctx, dst_va);
    dma_map_ctx_put(ctx);

    return ret;
}

static void svm_fill_dma_addr_info(struct dma_map_node *map_node, struct svm_global_va *dst_va,
                                   struct svm_dma_addr_info *dma_info)
{
    struct svm_dma_addr_info *addr_info = &map_node->addr_info;
    u64 pre_first_segid_size = 0;
    u64 pre_last_segid_size;
    u32 first_segid_offset = 0;
    u32 last_segid_offset;

    dma_info->is_write = map_node->is_write;
    dma_info->is_local_mem = map_node->is_local_mem;

    while (dst_va->va >= (map_node->align_dst_va.va + pre_first_segid_size + addr_info->seg[first_segid_offset].size)) {
        pre_first_segid_size += addr_info->seg[first_segid_offset].size;
        first_segid_offset++;
    }

    pre_last_segid_size = pre_first_segid_size;
    last_segid_offset = first_segid_offset;
    while ((dst_va->va + dst_va->size) >
           (map_node->align_dst_va.va + pre_last_segid_size + addr_info->seg[last_segid_offset].size)) {
        pre_last_segid_size += addr_info->seg[last_segid_offset].size;
        last_segid_offset++;
    }

    dma_info->dma_addr_seg_num = last_segid_offset - first_segid_offset + 1;
    dma_info->first_seg_offset = dst_va->va - map_node->align_dst_va.va - pre_first_segid_size;
    dma_info->last_seg_len = dst_va->va - map_node->align_dst_va.va + dst_va->size - pre_last_segid_size;
    if (dma_info->dma_addr_seg_num == 1) {
        dma_info->last_seg_len -= dma_info->first_seg_offset;
    }
    dma_info->seg = &addr_info->seg[first_segid_offset];
}

int svm_dma_addr_query_by_handle(void *handle, struct svm_global_va *dst_va, struct svm_dma_addr_info *dma_info)
{
    struct dma_map_node *map_node = (struct dma_map_node *)handle;

    if ((dst_va->va < map_node->raw_va) || ((dst_va->va + dst_va->size) > (map_node->raw_va + map_node->raw_size))) {
        svm_err("Invalid dst va. (map: va=0x%llx; size=0x%llx; dst: va=0x%llx; size=0x%llx)\n", map_node->raw_va,
                map_node->raw_size, dst_va->va, dst_va->size);
        return -EINVAL;
    }

    svm_fill_dma_addr_info(map_node, dst_va, dma_info);
    return 0;
}

static int _svm_dma_addr_get(struct dma_map_ctx *ctx, struct svm_global_va *dst_va, struct svm_dma_addr_info *dma_info,
                             void **handle)
{
    struct dma_map_node *map_node = NULL;
    int refcnt;

    ka_task_read_lock_bh(&ctx->lock);

    map_node = dma_map_node_search(ctx, dst_va);
    if (map_node == NULL) {
        ka_task_read_unlock_bh(&ctx->lock);
        svm_debug("Search completed. (udevid=%u; tgid=%d; start=0x%llx; size=0x%llx; map_node is null=%u)\n",
                  ctx->udevid, ctx->tgid, dst_va->va, dst_va->size, (u32)(map_node == NULL));
        return -EINVAL;
    }

    if (map_node->is_remapping) {
        ka_task_read_unlock_bh(&ctx->lock);
        return -EBUSY;
    }

    if (handle != NULL) {
        ka_base_kref_get(&map_node->kref);
        *handle = (void *)map_node;
    } else {
        refcnt = ka_base_atomic_inc_return(&map_node->refcnt);
        if (refcnt < 0) {
            ka_base_atomic_dec(&map_node->refcnt);
            ka_task_read_unlock_bh(&ctx->lock);
            svm_err("Get too much. (udevid=%u; tgid=%d; va=%llx; size=%llx; refcnt=%d)\n", ctx->udevid, ctx->tgid,
                    dst_va->va, dst_va->size, refcnt);
            return -EINVAL;
        }
    }

    ka_task_read_unlock_bh(&ctx->lock);

    svm_fill_dma_addr_info(map_node, dst_va, dma_info);

    return 0;
}

int svm_dma_addr_get(u32 udevid, int tgid, struct svm_global_va *dst_va, struct svm_dma_addr_info *dma_info,
                     void **handle)
{
    struct dma_map_ctx *ctx = NULL;
    int ret;

    ctx = dma_map_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = _svm_dma_addr_get(ctx, dst_va, dma_info, handle);
    dma_map_ctx_put(ctx);

    return ret;
}

static void _svm_dma_addr_put(struct dma_map_ctx *ctx, struct svm_global_va *dst_va)
{
    struct dma_map_node *map_node = NULL;
    int refcnt;

    ka_task_read_lock_bh(&ctx->lock);

    map_node = dma_map_node_search(ctx, dst_va);
    if (map_node == NULL) {
        ka_task_read_unlock_bh(&ctx->lock);
        svm_info("Search completed. (udevid=%u; tgid=%d; start=%llx; map_node is null=%u)\n", ctx->udevid, ctx->tgid,
                 dst_va->va, (u32)(map_node == NULL));
        return;
    }

    refcnt = ka_base_atomic_dec_return(&map_node->refcnt);
    if (refcnt < 0) {
        ka_base_atomic_inc(&map_node->refcnt); /* restore refcnt, hold read lock, del can not access refcnt same time */
        svm_err("No map, can not unmap. (udevid=%u; tgid=%d; va=%llx; size=%llx; refcnt=%d)\n", ctx->udevid, ctx->tgid,
                dst_va->va, dst_va->size, refcnt);
    }

    ka_task_read_unlock_bh(&ctx->lock);
}

void svm_dma_addr_put(u32 udevid, int tgid, struct svm_global_va *dst_va)
{
    struct dma_map_ctx *ctx = NULL;

    ctx = dma_map_ctx_get(udevid, tgid);
    if (ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return;
    }

    _svm_dma_addr_put(ctx, dst_va);
    dma_map_ctx_put(ctx);
}

static void svm_dma_addr_sync(u32 udevid, const struct svm_dma_addr_info *dma_info, bool for_device)
{
    ka_device_t *dev = uda_get_device(udevid);
    u64 i;

    if ((dev == NULL) || (dma_info == NULL) || !dma_info->is_local_mem) {
        return;
    }

    for (i = 0; i < dma_info->dma_addr_seg_num; i++) {
        u64 offset = (i == 0) ? dma_info->first_seg_offset : 0;
        u64 size = (i == (dma_info->dma_addr_seg_num - 1)) ? dma_info->last_seg_len : dma_info->seg[i].size - offset;
        ka_dma_addr_t dma_addr = (ka_dma_addr_t)(dma_info->seg[i].dma_addr + offset);

        if (for_device) {
            ka_mm_dma_sync_single_for_device(dev, dma_addr, size, KA_DMA_TO_DEVICE);
        } else {
            ka_mm_dma_sync_single_for_cpu(dev, dma_addr, size, KA_DMA_FROM_DEVICE);
        }
    }
}

void svm_dma_addr_sync_for_device(u32 udevid, const struct svm_dma_addr_info *dma_info)
{
    svm_dma_addr_sync(udevid, dma_info, true);
}

void svm_dma_addr_sync_for_cpu(u32 udevid, const struct svm_dma_addr_info *dma_info)
{
    svm_dma_addr_sync(udevid, dma_info, false);
}

void dma_map_addr_show(struct dma_map_ctx *ctx, ka_seq_file_t *seq)
{
    struct range_rbtree_node *range_node, *next;
    int i = 0;

    ka_task_read_lock_bh(&ctx->lock);

    ka_fs_seq_printf(seq, "dma map: udevid %u tgid %d mem num %u\n", ctx->udevid, ctx->tgid, ctx->range_tree.node_num);

    ka_base_rbtree_postorder_for_each_entry_safe(range_node, next, &ctx->range_tree.root, node)
    {
        struct dma_map_node *map_node = ka_container_of(range_node, struct dma_map_node, range_node);
        struct svm_global_va *dst_va = &map_node->align_dst_va;
        struct svm_dma_addr_info *addr_info = &map_node->addr_info;

        if (i == 0) {
            ka_fs_seq_printf(seq, "   index   va               size      raw(va          size)"
                                  "     udevid   tgid dma_addr_seg_num first_seg_offset last_seg_len\n");
        }
        ka_fs_seq_printf(
            seq, "   %d       0x%llx     0x%llx      (0x%llx     0x%llx)     %u    %d     %llu     %llx    %llx\n", i++,
            dst_va->va, dst_va->size, map_node->raw_va, map_node->raw_size, dst_va->udevid, dst_va->tgid,
            addr_info->dma_addr_seg_num, addr_info->first_seg_offset, addr_info->last_seg_len);
    }

    ka_task_read_unlock_bh(&ctx->lock);
}

static int _dma_map_addr_recycle(struct dma_map_ctx *ctx)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    int recycle_num = 0;

    do {
        struct dma_map_node *map_node = NULL;
        struct range_rbtree_node *range_node = NULL;

        ka_task_write_lock_bh(&ctx->lock);
        range_node = range_rbtree_get_first(&ctx->range_tree);
        if (range_node == NULL) {
            ka_task_write_unlock_bh(&ctx->lock);
            break;
        }
        recycle_num++;
        map_node = ka_container_of(range_node, struct dma_map_node, range_node);
        range_rbtree_erase(&ctx->range_tree, range_node);
        ka_task_write_unlock_bh(&ctx->lock);

        (void)g_dma_map_ops->do_dma_unmap(map_node);
        dma_map_node_destroy(map_node);
        ka_try_cond_resched(&stamp);
    } while (1);

    return recycle_num;
}

void dma_map_addr_recycle(struct dma_map_ctx *ctx)
{
    int recycle_num = 0;

    recycle_num = _dma_map_addr_recycle(ctx);
    if (recycle_num > 0) {
        svm_warn("Recycle mem. (udevid=%u; tgid=%d; recycle_num=%d)\n", ctx->udevid, ctx->tgid, recycle_num);
    }
}
