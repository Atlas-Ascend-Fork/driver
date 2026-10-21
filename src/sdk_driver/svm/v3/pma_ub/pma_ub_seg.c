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
#include "ka_system_pub.h"
#include "ka_fs_pub.h"
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_sched_pub.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "pma_ub_ubdevshm_wrapper.h"
#include "pma_ub_acquired_seg.h"
#include "pma_ub_seg.h"

struct pma_ub_seg {
    struct range_rbtree_node range_node;

    u32 token_id;

    struct pma_ub_acquired_segs_mng acquired_segs_mng;
};

static struct pma_ub_seg *_pma_ub_seg_search(struct pma_ub_seg_mng *seg_mng, u64 va, u64 size)
{
    struct pma_ub_seg *seg = NULL;
    struct range_rbtree_node *range_node = NULL;

    range_node = range_rbtree_search(&seg_mng->range_tree, va, size);
    if (range_node != NULL) {
        seg = ka_container_of(range_node, struct pma_ub_seg, range_node);
    }

    return seg;
}

static int pma_ub_seg_insert(struct pma_ub_seg_mng *seg_mng, struct pma_ub_seg *seg)
{
    int ret;

    ka_task_down_write(&seg_mng->rw_sem);
    ret = range_rbtree_insert(&seg_mng->range_tree, &seg->range_node);
    ka_task_up_write(&seg_mng->rw_sem);
    if (ret != 0) {
        svm_err("Insert failed. (ret=%d; va=0x%llx; size=%llu)\n", ret, seg->range_node.start, seg->range_node.size);
    }

    return ret;
}

static struct pma_ub_seg *pma_ub_seg_erase(struct pma_ub_seg_mng *seg_mng, u64 start, u64 size)
{
    struct pma_ub_seg *seg = NULL;

    ka_task_down_write(&seg_mng->rw_sem);
    seg = _pma_ub_seg_search(seg_mng, start, size);
    if (seg == NULL) {
        ka_task_up_write(&seg_mng->rw_sem);
        svm_err("Search failed. (udevid=%u; tgid=%d; start=0x%llx)\n", seg_mng->udevid, seg_mng->tgid, start);
        return NULL;
    }

    if ((seg->range_node.start != start) || (seg->range_node.size != size)) {
        ka_task_up_write(&seg_mng->rw_sem);
        svm_err("Not add addr. (udevid=%u; tgid=%d; start=0x%llx; size=%llu)\n", seg_mng->udevid, seg_mng->tgid, start,
                size);
        return NULL;
    }

    range_rbtree_erase(&seg_mng->range_tree, &seg->range_node);
    seg_mng->num--;
    ka_task_up_write(&seg_mng->rw_sem);

    return seg;
}

static struct pma_ub_seg *pma_ub_seg_erase_one(struct pma_ub_seg_mng *seg_mng)
{
    struct pma_ub_seg *seg = NULL;
    struct range_rbtree_node *range_node = NULL;

    ka_task_down_write(&seg_mng->rw_sem);
    range_node = range_rbtree_erase_one(&seg_mng->range_tree);
    if (range_node != NULL) {
        seg_mng->num--;
        seg = ka_container_of(range_node, struct pma_ub_seg, range_node);
    }
    ka_task_up_write(&seg_mng->rw_sem);

    return seg;
}

static bool pma_ub_seg_is_contained(const struct pma_ub_seg *seg, u64 start, u64 size)
{
    u64 offset;

    if (seg->range_node.start < start) {
        return false;
    }

    offset = seg->range_node.start - start;
    return (offset <= size) && (seg->range_node.size <= (size - offset));
}

static struct pma_ub_seg *pma_ub_seg_erase_contained_one(struct pma_ub_seg_mng *seg_mng, u64 start, u64 size)
{
    struct range_rbtree_node *range_node = NULL;
    struct pma_ub_seg *seg = NULL;
    ka_rb_node_t *node = NULL;

    ka_task_down_write(&seg_mng->rw_sem);
    node = ka_base_rb_first(&seg_mng->range_tree.root);
    while (node != NULL) {
        range_node = ka_base_rb_entry(node, struct range_rbtree_node, node);
        seg = ka_container_of(range_node, struct pma_ub_seg, range_node);
        if (pma_ub_seg_is_contained(seg, start, size)) {
            range_rbtree_erase(&seg_mng->range_tree, &seg->range_node);
            seg_mng->num--;
            ka_task_up_write(&seg_mng->rw_sem);
            return seg;
        }
        node = ka_base_rb_next(node);
    }
    ka_task_up_write(&seg_mng->rw_sem);

    return NULL;
}

static struct pma_ub_seg *pma_ub_seg_create(struct pma_ub_seg_mng *seg_mng, u64 start, u64 size, u32 token_id)
{
    struct pma_ub_seg *seg = NULL;

    seg = svm_kvzalloc(sizeof(struct pma_ub_seg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (seg == NULL) {
        svm_err("Alloc pma_ub_seg failed.\n");
        return NULL;
    }

    seg->range_node.start = start;
    seg->range_node.size = size;
    seg->token_id = token_id;
    pma_ub_acquired_segs_mng_init(&seg->acquired_segs_mng, seg_mng->udevid, seg_mng->tgid);

    return seg;
}

static void pma_ub_seg_destroy(struct pma_ub_seg *seg)
{
    pma_ub_acquired_segs_mng_uninit(&seg->acquired_segs_mng);

    svm_kvfree(seg);
}

int pma_ub_seg_add(struct pma_ub_seg_mng *seg_mng, u64 start, u64 size, u32 token_id)
{
    struct pma_ub_seg *seg = NULL;
    int ret;

    seg = pma_ub_seg_create(seg_mng, start, size, token_id);
    if (seg == NULL) {
        return -ENOMEM;
    }

    ret = pma_ub_seg_insert(seg_mng, seg);
    if (ret != 0) {
        pma_ub_seg_destroy(seg);
        return ret;
    }

    return 0;
}

int pma_ub_seg_del(struct pma_ub_seg_mng *seg_mng, u64 start, u64 size)
{
    struct pma_ub_seg *seg = NULL;

    seg = pma_ub_seg_erase(seg_mng, start, size);
    if (seg == NULL) {
        return -EINVAL;
    }

    pma_ub_seg_destroy(seg);

    return 0;
}

int pma_ub_seg_query(struct pma_ub_seg_mng *seg_mng, u64 va, u64 *start, u64 *size, u32 *token_id)
{
    struct pma_ub_seg *seg = NULL;

    ka_task_down_read(&seg_mng->rw_sem);
    seg = _pma_ub_seg_search(seg_mng, va, 1);
    if (seg == NULL) {
        ka_task_up_read(&seg_mng->rw_sem);
        svm_err("No such seg. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    *start = seg->range_node.start;
    *size = seg->range_node.size;
    *token_id = seg->token_id;
    ka_task_up_read(&seg_mng->rw_sem);

    return 0;
}

int pma_ub_seg_acquire(struct pma_ub_seg_mng *seg_mng, u64 va, u64 size, int (*invalidate)(u64 invalidate_tag),
                       u64 invalidate_tag, u32 *token_id)
{
    struct pma_ub_seg *seg = NULL;
    int ret;

    ka_task_down_read(&seg_mng->rw_sem);
    seg = _pma_ub_seg_search(seg_mng, va, size);
    if (seg == NULL) {
        ka_task_up_read(&seg_mng->rw_sem);
        svm_err("No such seg. (va=0x%llx; size=%llu)\n", va, size);
        return -EINVAL;
    }

    ret = pma_ub_add_acquired_seg(&seg->acquired_segs_mng, va, size, invalidate, invalidate_tag);
    if (ret == 0) {
        *token_id = seg->token_id;
    }
    ka_task_up_read(&seg_mng->rw_sem);

    return ret;
}

int pma_ub_seg_release(struct pma_ub_seg_mng *seg_mng, u64 va, u64 size)
{
    struct pma_ub_seg *seg = NULL;
    int ret;

    ka_task_down_read(&seg_mng->rw_sem);
    seg = _pma_ub_seg_search(seg_mng, va, size);
    if (seg == NULL) {
        ka_task_up_read(&seg_mng->rw_sem);
        svm_warn("No such seg. (va=0x%llx; size=%llu)\n", va, size);
        return -EINVAL;
    }

    ret = pma_ub_del_acquired_seg(&seg->acquired_segs_mng, va, size);
    ka_task_up_read(&seg_mng->rw_sem);

    return ret;
}

void pma_ub_seg_mng_init(struct pma_ub_seg_mng *seg_mng, u32 udevid, int tgid)
{
    ka_task_init_rwsem(&seg_mng->rw_sem);
    range_rbtree_init(&seg_mng->range_tree);
    seg_mng->num = 0;
    seg_mng->udevid = udevid;
    seg_mng->tgid = tgid;
}

void pma_ub_seg_mng_uninit(struct pma_ub_seg_mng *seg_mng)
{
    struct pma_ub_seg *seg = NULL;
    unsigned long stamp = (unsigned long)ka_jiffies;
    u64 va, size;
    int recycle_num = 0;

    do {
        seg = pma_ub_seg_erase_one(seg_mng);
        if (seg == NULL) {
            break;
        }

        va = seg->range_node.start;
        size = seg->range_node.size;

        pma_ub_seg_destroy(seg);
        pma_ub_ubdevshm_unregister_segment(seg_mng->udevid, seg_mng->tgid, va, size);
        recycle_num++;
        ka_try_cond_resched(&stamp);
    } while (1);

    if (recycle_num > 0) {
        svm_info("Recycle mem. (udevid=%u; tgid=%d; recycle_num=%d)\n", seg_mng->udevid, seg_mng->tgid, recycle_num);
    }
}

void pma_ub_seg_recycle(struct pma_ub_seg_mng *seg_mng, u64 start, u64 size)
{
    struct pma_ub_seg *seg = NULL;
    unsigned long stamp = (unsigned long)ka_jiffies;
    u64 seg_start, seg_size;

    do {
        seg = pma_ub_seg_erase_contained_one(seg_mng, start, size);
        if (seg == NULL) {
            break;
        }

        seg_start = seg->range_node.start;
        seg_size = seg->range_node.size;
        pma_ub_seg_destroy(seg);
        (void)pma_ub_ubdevshm_unregister_segment(seg_mng->udevid, seg_mng->tgid, seg_start, seg_size);
        ka_try_cond_resched(&stamp);
    } while (1);
}

void pma_ub_seg_mng_show(struct pma_ub_seg_mng *seg_mng, ka_seq_file_t *seq)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    struct range_rbtree_node *range_node, *next;
    int i = 0;

    ka_task_down_read(&seg_mng->rw_sem);

    ka_fs_seq_printf(seq, "pma_ub segs: udevid %u tgid %d segs_num %llu\n", seg_mng->udevid, seg_mng->tgid,
                     seg_mng->num);

    ka_base_rbtree_postorder_for_each_entry_safe(range_node, next, &seg_mng->range_tree.root, node)
    {
        struct pma_ub_seg *seg = ka_container_of(range_node, struct pma_ub_seg, range_node);
        if (i == 0) {
            ka_fs_seq_printf(seq, "    %-5s %-17s %-15s\n", "id", "va", "size(Bytes)");
        }
        ka_fs_seq_printf(seq, "    %-5d 0x%-15llx %-15llu\n", i++, seg->range_node.start, seg->range_node.size);
        pma_ub_acquired_segs_mng_show(&seg->acquired_segs_mng, seq);
        ka_try_cond_resched(&stamp);
    }

    ka_task_up_read(&seg_mng->rw_sem);
}
