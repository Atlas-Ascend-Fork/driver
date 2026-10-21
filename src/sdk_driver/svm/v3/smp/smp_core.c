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
#include "ka_system_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_common_pub.h"
#include "ka_fs_pub.h"
#include "ka_sched_pub.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "svm_gfp.h"
#include "smp_event.h"
#include "svm_smp.h"
#include "smp_ctx.h"
#include "smp_core.h"

#define SMP_SHOW_NODE_MAX 100000

static struct smp_mem_node *smp_mem_node_create(struct smp_ctx *smp_ctx, u64 start, u64 size, u32 flag)
{
    struct smp_mem_node *mem_node = svm_kzalloc(sizeof(*mem_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (mem_node == NULL) {
        svm_err("Alloc mem node failed. (udevid=%u; tgid=%d; start=0x%llx; size=%llu)\n", smp_ctx->udevid,
                smp_ctx->tgid, start, size);
        return NULL;
    }

    mem_node->range_node.start = start;
    mem_node->range_node.size = size;
    mem_node->flag = flag;
    mem_node->status = 1;
    ka_base_atomic_set(&mem_node->refcnt, 0);
    ka_base_atomic_set(&mem_node->cross_app_refcnt, 0);
    mem_node->pa_seg = NULL;
    mem_node->seg_num = 0;
    return mem_node;
}

static inline void smp_mem_node_destroy(struct smp_mem_node *mem_node)
{
    if (mem_node->pa_seg != NULL) {
        svm_vfree(mem_node->pa_seg);
    }
    svm_kfree(mem_node);
}

static struct smp_mem_node *smp_mem_node_search(struct smp_ctx *smp_ctx, u64 va, u64 size)
{
    struct smp_mem_node *mem_node = NULL;
    struct range_rbtree_node *range_node = NULL;

    range_node = range_rbtree_search(&smp_ctx->range_tree, va, size);
    if (range_node != NULL) {
        mem_node = ka_container_of(range_node, struct smp_mem_node, range_node);
    }

    return mem_node;
}

static int smp_add_mem(struct smp_ctx *smp_ctx, u64 start, u64 size, u32 flag)
{
    struct smp_mem_node *mem_node = NULL;
    int ret;

    mem_node = smp_mem_node_create(smp_ctx, start, size, flag);
    if (mem_node == NULL) {
        return -ENOMEM;
    }

    ka_task_write_lock_bh(&smp_ctx->lock);
    ret = range_rbtree_insert(&smp_ctx->range_tree, &mem_node->range_node);
    ka_task_write_unlock_bh(&smp_ctx->lock);
    if (ret != 0) {
        smp_mem_node_destroy(mem_node);
        if (ret == -EEXIST) {
            return 0;
        }

        svm_err("Insert failed. (udevid=%u; tgid=%d; start=%llx; size=%llx)\n", smp_ctx->udevid, smp_ctx->tgid, start,
                size);
    }

    return ret;
}

int svm_smp_add_mem(u32 udevid, int tgid, u64 start, u64 size, u32 flag)
{
    struct smp_ctx *smp_ctx = NULL;
    int ret;

    smp_ctx = smp_ctx_get(udevid, tgid);
    if (smp_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = smp_add_mem(smp_ctx, start, size, flag);
    smp_ctx_put(smp_ctx);

    return ret;
}

static int smp_del_mem(struct smp_ctx *smp_ctx, u64 start)
{
    struct smp_mem_node *mem_node = NULL;

    ka_task_write_lock_bh(&smp_ctx->lock);

    mem_node = smp_mem_node_search(smp_ctx, start, 1);
    if (mem_node == NULL) {
        ka_task_write_unlock_bh(&smp_ctx->lock);
        svm_err("Search failed. (udevid=%u; tgid=%d; start=0x%llx)\n", smp_ctx->udevid, smp_ctx->tgid, start);
        return -EINVAL;
    }

    if (mem_node->range_node.start != start) {
        ka_task_write_unlock_bh(&smp_ctx->lock);
        svm_err("Not add addr. (udevid=%u; tgid=%d; start=0x%llx)\n", smp_ctx->udevid, smp_ctx->tgid, start);
        return -EINVAL;
    }

    mem_node->status = 0;
    if (ka_base_atomic_read(&mem_node->refcnt) > 0) {
        ka_task_write_unlock_bh(&smp_ctx->lock);
        return -EBUSY;
    }

    range_rbtree_erase(&smp_ctx->range_tree, &mem_node->range_node);

    ka_task_write_unlock_bh(&smp_ctx->lock);

    smp_mem_node_destroy(mem_node);

    return 0;
}

int svm_smp_del_mem(u32 udevid, int tgid, u64 start)
{
    struct smp_ctx *smp_ctx = NULL;
    int ret;

    smp_ctx = smp_ctx_get(udevid, tgid);
    if (smp_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = smp_del_mem(smp_ctx, start);
    smp_ctx_put(smp_ctx);

    return ret;
}

static int smp_pin_mem(struct smp_ctx *smp_ctx, u64 va, u64 size, bool is_dev_cp_only, bool is_cross_app)
{
    struct smp_mem_node *mem_node = NULL;
    int ret = 0;

    ka_task_read_lock_bh(&smp_ctx->lock);

    mem_node = smp_mem_node_search(smp_ctx, va, size);
    if (mem_node == NULL) {
        ka_task_read_unlock_bh(&smp_ctx->lock);
        return -EINVAL;
    }
    if (((mem_node->flag & SVM_SMP_FLAG_DEV_CP_ONLY) != 0) != is_dev_cp_only) {
        ret = -EINVAL;
    } else if (mem_node->status == 0) {
        ret = -EOWNERDEAD;
    } else {
        ka_base_atomic_inc(&mem_node->refcnt);
        if (is_cross_app) {
            ka_base_atomic_inc(&mem_node->cross_app_refcnt);
        }
    }

    ka_task_read_unlock_bh(&smp_ctx->lock);

    return ret;
}

static int _svm_smp_pin_mem(u32 udevid, int tgid, u64 va, u64 size, bool is_dev_cp_only, bool is_cross_app)
{
    struct smp_ctx *smp_ctx = NULL;
    int ret;
    smp_ctx = smp_ctx_get(udevid, tgid);
    if (smp_ctx == NULL) {
        return -ESRCH;
    }

    ret = smp_pin_mem(smp_ctx, va, size, is_dev_cp_only, is_cross_app);
    smp_ctx_put(smp_ctx);

    return ret;
}

int svm_smp_pin_mem(u32 udevid, int tgid, u64 va, u64 size, bool is_cross_app)
{
    return _svm_smp_pin_mem(udevid, tgid, va, size, false, is_cross_app);
}
KA_EXPORT_SYMBOL_GPL(svm_smp_pin_mem);

int svm_smp_pin_dev_cp_only_mem(u32 udevid, int tgid, u64 va, u64 size)
{
    return _svm_smp_pin_mem(udevid, tgid, va, size, true, false);
}
KA_EXPORT_SYMBOL_GPL(svm_smp_pin_dev_cp_only_mem);

static int smp_unpin_mem(struct smp_ctx *smp_ctx, u64 va, u64 size, bool is_dev_cp_only, bool is_cross_app)
{
    struct smp_mem_node *mem_node = NULL;
    int refcnt, trigger_event = 0;
    u64 range_start, range_size;

    ka_task_read_lock_bh(&smp_ctx->lock);

    mem_node = smp_mem_node_search(smp_ctx, va, size);
    if (mem_node == NULL) {
        ka_task_read_unlock_bh(&smp_ctx->lock);
        svm_err("Search failed. (udevid=%u; tgid=%d; va=%llx; size=%llx)\n", smp_ctx->udevid, smp_ctx->tgid, va, size);
        return -EINVAL;
    }

    if (((mem_node->flag & SVM_SMP_FLAG_DEV_CP_ONLY) != 0) != is_dev_cp_only) {
        ka_task_read_unlock_bh(&smp_ctx->lock);
        return -EINVAL;
    }

    refcnt = ka_base_atomic_dec_return(&mem_node->refcnt);
    if (refcnt < 0) {
        ka_base_atomic_inc(&mem_node->refcnt); /* restore refcnt, hold read lock, del can not access refcnt same time */
        ka_task_read_unlock_bh(&smp_ctx->lock);
        svm_err("No pin, can not unpin. (udevid=%u; tgid=%d; va=%llx; size=%llx; refcnt=%d)\n", smp_ctx->udevid,
                smp_ctx->tgid, va, size, refcnt);
        return -EINVAL;
    } else if ((refcnt == 0) && (mem_node->status == 0)) {
        trigger_event = 1;
    }
    if (is_cross_app) {
        (void)ka_base_atomic_dec_if_positive(&mem_node->cross_app_refcnt);
    }

    range_start = mem_node->range_node.start;
    range_size = mem_node->range_node.size;
    ka_task_read_unlock_bh(&smp_ctx->lock);

    if (trigger_event == 1) {
        smp_trigger_event(smp_ctx->udevid, smp_ctx->tgid, range_start, range_size);
    }

    return 0;
}

static int _svm_smp_unpin_mem(u32 udevid, int tgid, u64 va, u64 size, bool is_dev_cp_only, bool is_cross_app)
{
    struct smp_ctx *smp_ctx = NULL;
    int ret;

    smp_ctx = smp_ctx_get(udevid, tgid);
    if (smp_ctx == NULL) {
        svm_debug("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = smp_unpin_mem(smp_ctx, va, size, is_dev_cp_only, is_cross_app);
    smp_ctx_put(smp_ctx);

    return ret;
}

int svm_smp_unpin_mem(u32 udevid, int tgid, u64 va, u64 size, bool is_cross_app)
{
    return _svm_smp_unpin_mem(udevid, tgid, va, size, false, is_cross_app);
}
KA_EXPORT_SYMBOL_GPL(svm_smp_unpin_mem);

int svm_smp_unpin_dev_cp_only_mem(u32 udevid, int tgid, u64 va, u64 size)
{
    return _svm_smp_unpin_mem(udevid, tgid, va, size, true, false);
}
KA_EXPORT_SYMBOL_GPL(svm_smp_unpin_dev_cp_only_mem);

static int smp_check_mem(struct smp_ctx *smp_ctx, u64 va, u64 size, bool is_dev_cp_only)
{
    struct smp_mem_node *mem_node = NULL;
    int ret = 0;

    ka_task_read_lock_bh(&smp_ctx->lock);

    mem_node = smp_mem_node_search(smp_ctx, va, size);
    if (mem_node == NULL) {
        ka_task_read_unlock_bh(&smp_ctx->lock);
        return -EINVAL;
    }

    if (((mem_node->flag & SVM_SMP_FLAG_DEV_CP_ONLY) != 0) != is_dev_cp_only) {
        svm_err("Flag not match. (flag=%u; is_dev_cp_only=%d)\n", mem_node->flag, is_dev_cp_only);
        ret = -EINVAL;
    } else if (mem_node->status == 0) {
        ret = -EOWNERDEAD;
    }

    ka_task_read_unlock_bh(&smp_ctx->lock);

    return ret;
}

static int _svm_smp_check_mem(u32 udevid, int tgid, u64 va, u64 size, bool is_dev_cp_only)
{
    struct smp_ctx *smp_ctx = NULL;
    int ret;

    smp_ctx = smp_ctx_get(udevid, tgid);
    if (smp_ctx == NULL) {
        return -EINVAL;
    }

    ret = smp_check_mem(smp_ctx, va, size, is_dev_cp_only);
    smp_ctx_put(smp_ctx);

    return ret;
}

int svm_smp_check_mem(u32 udevid, int tgid, u64 va, u64 size)
{
    return _svm_smp_check_mem(udevid, tgid, va, size, false);
}

int svm_smp_check_dev_cp_only_mem(u32 udevid, int tgid, u64 va, u64 size)
{
    return _svm_smp_check_mem(udevid, tgid, va, size, true);
}

void smp_mem_show(struct smp_ctx *smp_ctx, ka_seq_file_t *seq)
{
    struct range_rbtree_node *range_node, *next;
    struct smp_mem_node *dump_info = NULL;
    u32 dump_num = 0;
    u32 node_num, i;

    node_num = smp_ctx->range_tree.node_num;

    if ((node_num == 0) || (node_num > SMP_SHOW_NODE_MAX)) {
        svm_info("Skip show smp mem. (udevid=%u; tgid=%d; node_num=%u)\n", smp_ctx->udevid, smp_ctx->tgid, node_num);
        return;
    }

    dump_info = (struct smp_mem_node *)svm_vzalloc(sizeof(*dump_info) * node_num);
    if (dump_info == NULL) {
        svm_warn("No mem for smp mem show. (udevid=%u; tgid=%d; num=%u)\n", smp_ctx->udevid, smp_ctx->tgid, node_num);
        return;
    }

    ka_task_read_lock_bh(&smp_ctx->lock);
    ka_base_rbtree_postorder_for_each_entry_safe(range_node, next, &smp_ctx->range_tree.root, node)
    {
        struct smp_mem_node *mem_node = ka_container_of(range_node, struct smp_mem_node, range_node);
        if (dump_num >= node_num) {
            break;
        }
        dump_info[dump_num] = *mem_node;
        dump_num++;
    }
    ka_task_read_unlock_bh(&smp_ctx->lock);

    ka_fs_seq_printf(seq, "smp: udevid %u tgid %d mem num %u\n", smp_ctx->udevid, smp_ctx->tgid, node_num);
    ka_fs_seq_printf(seq, "   index   va      size     status   refcnt  cross_app_refcnt  flag\n");
    for (i = 0; i < dump_num; i++) {
        ka_fs_seq_printf(seq, "   %u       %llx     %llx     %d   %d    %d      %u\n", i, dump_info[i].range_node.start,
                         dump_info[i].range_node.size, dump_info[i].status, ka_base_atomic_read(&dump_info[i].refcnt),
                         ka_base_atomic_read(&dump_info[i].cross_app_refcnt), dump_info[i].flag);
    }
    svm_vfree(dump_info);
}

static int _smp_mem_recycle(struct smp_ctx *smp_ctx, bool force)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    int recycle_num = 0;

    do {
        struct smp_mem_node *mem_node = NULL;
        struct range_rbtree_node *range_node = NULL;

        ka_task_write_lock_bh(&smp_ctx->lock);
        range_node = range_rbtree_get_first(&smp_ctx->range_tree);
        if (range_node == NULL) {
            ka_task_write_unlock_bh(&smp_ctx->lock);
            break;
        }
        mem_node = ka_container_of(range_node, struct smp_mem_node, range_node);
        if ((force == false) && (ka_base_atomic_read(&mem_node->cross_app_refcnt) > 0)) {
            recycle_num = -EBUSY;
            ka_task_write_unlock_bh(&smp_ctx->lock);
            break;
        }
        recycle_num++;
        range_rbtree_erase(&smp_ctx->range_tree, range_node);
        ka_task_write_unlock_bh(&smp_ctx->lock);
        smp_mem_node_destroy(mem_node);
        ka_try_cond_resched(&stamp);
    } while (1);

    return recycle_num;
}

int smp_mem_recycle(struct smp_ctx *smp_ctx, bool force)
{
    int recycle_num = 0;

    recycle_num = _smp_mem_recycle(smp_ctx, force);
    if (recycle_num > 0) {
        svm_warn("Recycle mem. (udevid=%u; tgid=%d; recycle_num=%d)\n", smp_ctx->udevid, smp_ctx->tgid, recycle_num);
    }
    return recycle_num;
}

static int smp_set_pa_seg(struct smp_mem_node *mem_node, u64 va, u64 size, struct svm_pa_seg *pa_seg, u64 seg_num)
{
    u64 total_size = svm_get_pa_size(pa_seg, seg_num);
    if ((va != mem_node->range_node.start) || (size != mem_node->range_node.size) ||
        (total_size != mem_node->range_node.size)) {
        svm_err("Set pa seg failed. (va=0x%llx; size=%llu; total_size=%llu; mem_node va=0x%llx; size=%llu)\n", va, size,
                total_size, mem_node->range_node.start, mem_node->range_node.size);
        return -EINVAL;
    }

    mem_node->seg_num = seg_num;
    mem_node->pa_seg = pa_seg;
    return 0;
}

static int smp_set_mem_pa(struct smp_ctx *smp_ctx, u64 va, u64 size, struct svm_pa_seg *pa_seg, u64 seg_num)
{
    struct smp_mem_node *mem_node = NULL;
    struct svm_pa_seg *tmp_seg = NULL;
    int ret;

    tmp_seg = svm_vzalloc(seg_num * sizeof(struct svm_pa_seg));
    if (tmp_seg == NULL) {
        svm_err("Alloc pa_seg failed. (seg_num=%llu)\n", seg_num);
        return -ENOMEM;
    }
    (void)memcpy_s(tmp_seg, seg_num * sizeof(struct svm_pa_seg), pa_seg, seg_num * sizeof(struct svm_pa_seg));

    ka_task_write_lock_bh(&smp_ctx->lock);
    mem_node = smp_mem_node_search(smp_ctx, va, size);
    if (mem_node == NULL) {
        ka_task_write_unlock_bh(&smp_ctx->lock);
        svm_vfree(tmp_seg);
        svm_err("Invalid va. (udevid=%u; tgid=%d; va=0x%llx; size=%llu)\n", smp_ctx->udevid, smp_ctx->tgid, va, size);
        return -EINVAL;
    }

    ret = smp_set_pa_seg(mem_node, va, size, tmp_seg, seg_num);
    ka_task_write_unlock_bh(&smp_ctx->lock);
    if (ret != 0) {
        svm_vfree(tmp_seg);
    }
    return ret;
}

int svm_smp_set_mem_pa(u32 udevid, int tgid, u64 va, u64 size, struct svm_pa_seg *pa_seg, u64 seg_num)
{
    struct smp_ctx *smp_ctx = NULL;
    int ret;

    smp_ctx = smp_ctx_get(udevid, tgid);
    if (smp_ctx == NULL) {
        svm_err("Get smp ctx failed. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = smp_set_mem_pa(smp_ctx, va, size, pa_seg, seg_num);
    smp_ctx_put(smp_ctx);
    return ret;
}

static u64 smp_query_pa_seg(struct smp_mem_node *mem_node, u64 va, u64 size, struct svm_pa_seg *pa_seg, u64 seg_num)
{
    u64 need_seg_num = ka_base_min_t(u64, seg_num, mem_node->seg_num);
    u64 i, last_seg_index = 0, first_seg_offset = 0, last_seg_offset = 0;
    u64 cur_off = 0, out_idx = 0;

    for (i = 0; (i < mem_node->seg_num) && (out_idx < need_seg_num); i++) {
        u64 seg_size = mem_node->pa_seg[i].size;
        u64 seg_va = mem_node->range_node.start + cur_off;

        /* no overlap */
        if (seg_va + seg_size <= va) {
            cur_off += seg_size;
            continue;
        }
        if (seg_va >= va + size) {
            break;
        }

        /* get first seg info */
        if (out_idx == 0) {
            first_seg_offset = va - seg_va;
        }
        /* get last seg info, use the last assignment */
        last_seg_offset = (seg_va + seg_size) - (va + size);
        last_seg_index = out_idx;

        pa_seg[out_idx].pa = mem_node->pa_seg[i].pa;
        pa_seg[out_idx].size = mem_node->pa_seg[i].size;
        out_idx++;
        cur_off += seg_size;
    }

    /* handle first seg unaligned address */
    pa_seg[0].pa += first_seg_offset;
    pa_seg[0].size -= first_seg_offset;
    /* handle last seg unaligned size */
    pa_seg[last_seg_index].size -= last_seg_offset;
    return out_idx;
}

/* When mem_node exists but pa_seg is not filled (pa query failed during populate),
   seg_num will be 0. Returning failure is normal in this case; callers will retry
   or fall back to other query paths. This prevents downstream dma_map from
   underflowing on pa_seg[seg_num - 1] when seg_num is 0. */
static int smp_query_mem_pa(struct smp_ctx *smp_ctx, u64 va, u64 size, struct svm_pa_seg *pa_seg, u64 *seg_num)
{
    struct smp_mem_node *mem_node = NULL;

    ka_task_read_lock_bh(&smp_ctx->lock);

    mem_node = smp_mem_node_search(smp_ctx, va, size);
    if (mem_node == NULL) {
        ka_task_read_unlock_bh(&smp_ctx->lock);
        svm_warn("Find mem node not succ. (udevid=%u; tgid=%d; va=0x%llx; size=%llu)\n", smp_ctx->udevid, smp_ctx->tgid,
                 va, size);
        return -EINVAL;
    }
    if (mem_node->status == 0) {
        ka_task_read_unlock_bh(&smp_ctx->lock);
        svm_err("Va is freed. (udevid=%u; tgid=%d; va=0x%llx; size=%llu)\n", smp_ctx->udevid, smp_ctx->tgid, va, size);
        return -EOWNERDEAD;
    }

    *seg_num = smp_query_pa_seg(mem_node, va, size, pa_seg, *seg_num);
    svm_debug("Smp node. (udevid=%u; tgid=%d; start=0x%llx; size=%llu; real_seg_num=%llu; need_seg_num=%llu)\n",
              smp_ctx->udevid, smp_ctx->tgid, va, size, mem_node->seg_num, *seg_num);
    ka_task_read_unlock_bh(&smp_ctx->lock);
    return (*seg_num == 0) ? -ENODEV : 0;
}

int svm_smp_query_mem_pa(u32 udevid, int tgid, u64 va, u64 size, struct svm_pa_seg *pa_seg, u64 *seg_num)
{
    struct smp_ctx *smp_ctx = NULL;
    int ret;

    smp_ctx = smp_ctx_get(udevid, tgid);
    if (smp_ctx == NULL) {
        svm_warn("Get smp ctx not succ. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    ret = smp_query_mem_pa(smp_ctx, va, size, pa_seg, seg_num);
    smp_ctx_put(smp_ctx);
    return ret;
}
