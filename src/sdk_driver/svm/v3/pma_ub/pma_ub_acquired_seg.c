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
#include "ka_list_pub.h"
#include "ka_fs_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_sched_pub.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "pma_ub_acquired_seg.h"

#define ACQUIRED_SEG_DESTROY_WITH_INVALIDATE (1U << 0U)

struct pma_ub_acquired_seg {
    ka_list_head_t node;

    u64 va;
    u64 size;

    int (*invalidate)(u64 invalidate_tag);
    u64 invalidate_tag;
};

static struct pma_ub_acquired_seg *_pma_ub_acquired_seg_search(struct pma_ub_acquired_segs_mng *mng, u64 va, u64 size)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    struct pma_ub_acquired_seg *acquired_seg = NULL;
    struct pma_ub_acquired_seg *n = NULL;

    ka_list_for_each_entry_safe(acquired_seg, n, &mng->head, node)
    {
        if ((acquired_seg->va == va) && (acquired_seg->size == size)) {
            return acquired_seg;
        }
        ka_try_cond_resched(&stamp);
    }

    return NULL;
}

static void _pma_ub_acquired_seg_erase(struct pma_ub_acquired_segs_mng *mng, struct pma_ub_acquired_seg *acquired_seg)
{
    mng->num--;
    ka_list_del_init(&acquired_seg->node);
}

static int pma_ub_acquired_seg_insert(struct pma_ub_acquired_segs_mng *mng, struct pma_ub_acquired_seg *acquired_seg)
{
    struct pma_ub_acquired_seg *tmp_seg = NULL;

    ka_task_down_write(&mng->rw_sem);
    tmp_seg = _pma_ub_acquired_seg_search(mng, acquired_seg->va, acquired_seg->size);
    if (tmp_seg != NULL) {
        svm_err("Repeat insert. (va=0x%llx; size=%llu)\n", acquired_seg->va, acquired_seg->size);
        ka_task_up_write(&mng->rw_sem);
        return -EINVAL;
    }

    ka_list_add_tail(&acquired_seg->node, &mng->head);
    mng->num++;
    ka_task_up_write(&mng->rw_sem);

    return 0;
}

static struct pma_ub_acquired_seg *pma_ub_acquired_seg_erase(struct pma_ub_acquired_segs_mng *mng, u64 va, u64 size)
{
    struct pma_ub_acquired_seg *acquired_seg = NULL;

    ka_task_down_write(&mng->rw_sem);
    acquired_seg = _pma_ub_acquired_seg_search(mng, va, size);
    if (acquired_seg == NULL) {
        svm_err("Search acquired_seg failed. (va=0x%llx; size=%llu)\n", va, size);
        ka_task_up_write(&mng->rw_sem);
        return NULL;
    }

    _pma_ub_acquired_seg_erase(mng, acquired_seg);
    ka_task_up_write(&mng->rw_sem);

    return acquired_seg;
}

static struct pma_ub_acquired_seg *pma_ub_acquired_seg_erase_one(struct pma_ub_acquired_segs_mng *mng)
{
    struct pma_ub_acquired_seg *acquired_seg = NULL;

    ka_task_down_write(&mng->rw_sem);
    if (ka_list_empty_careful(&mng->head) == 1) {
        ka_task_up_write(&mng->rw_sem);
        return NULL;
    }

    acquired_seg = ka_list_first_entry(&mng->head, struct pma_ub_acquired_seg, node);
    _pma_ub_acquired_seg_erase(mng, acquired_seg);
    ka_task_up_write(&mng->rw_sem);

    return acquired_seg;
}

static struct pma_ub_acquired_seg *pma_ub_acquired_seg_create(u64 va, u64 size, int (*invalidate)(u64 invalidate_tag),
                                                              u64 invalidate_tag)
{
    struct pma_ub_acquired_seg *acquired_seg = NULL;

    acquired_seg = svm_kvzalloc(sizeof(struct pma_ub_acquired_seg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (acquired_seg == NULL) {
        svm_err("Alloc acquired_seg failed.\n");
        return NULL;
    }

    acquired_seg->va = va;
    acquired_seg->size = size;
    acquired_seg->invalidate = invalidate;
    acquired_seg->invalidate_tag = invalidate_tag;

    return acquired_seg;
}

static void pma_ub_acquired_seg_destroy(struct pma_ub_acquired_seg *acquired_seg, u32 flag)
{
    if (((flag & ACQUIRED_SEG_DESTROY_WITH_INVALIDATE) != 0) && (acquired_seg->invalidate != NULL)) {
        (void)acquired_seg->invalidate(acquired_seg->invalidate_tag);
    }

    svm_kvfree(acquired_seg);
}

int pma_ub_add_acquired_seg(struct pma_ub_acquired_segs_mng *mng, u64 va, u64 size,
                            int (*invalidate)(u64 invalidate_tag), u64 invalidate_tag)
{
    struct pma_ub_acquired_seg *acquired_seg = NULL;
    int ret;

    acquired_seg = pma_ub_acquired_seg_create(va, size, invalidate, invalidate_tag);
    if (acquired_seg == NULL) {
        return -ENOMEM;
    }

    ret = pma_ub_acquired_seg_insert(mng, acquired_seg);
    if (ret != 0) {
        pma_ub_acquired_seg_destroy(acquired_seg, 0);
        return ret;
    }

    return 0;
}

int pma_ub_del_acquired_seg(struct pma_ub_acquired_segs_mng *mng, u64 va, u64 size)
{
    struct pma_ub_acquired_seg *acquired_seg = NULL;

    acquired_seg = pma_ub_acquired_seg_erase(mng, va, size);
    if (acquired_seg == NULL) {
        svm_warn("No such acquired_seg. (va=0x%llx; size=%llu)\n", va, size);
        return -EINVAL;
    }

    pma_ub_acquired_seg_destroy(acquired_seg, 0);

    return 0;
}

void pma_ub_acquired_segs_mng_init(struct pma_ub_acquired_segs_mng *mng, u32 udevid, int tgid)
{
    ka_task_init_rwsem(&mng->rw_sem);
    KA_INIT_LIST_HEAD(&mng->head);
    mng->num = 0;
    mng->udevid = udevid;
    mng->tgid = tgid;
}

void pma_ub_acquired_segs_mng_uninit(struct pma_ub_acquired_segs_mng *mng)
{
    struct pma_ub_acquired_seg *acquired_seg = NULL;
    unsigned long stamp = (unsigned long)ka_jiffies;
    int recycle_num = 0;

    do {
        acquired_seg = pma_ub_acquired_seg_erase_one(mng);
        if (acquired_seg == NULL) {
            break;
        }

        /* Without invalidate, ubdevshm release api no tgid, net card driver ignore release fail. */
        pma_ub_acquired_seg_destroy(acquired_seg, ACQUIRED_SEG_DESTROY_WITH_INVALIDATE);
        recycle_num++;
        ka_try_cond_resched(&stamp);
    } while (1);

    if (recycle_num > 0) {
        svm_info("Recycle mem. (udevid=%u; tgid=%d; recycle_num=%d)\n", mng->udevid, mng->tgid, recycle_num);
    }
}

void pma_ub_acquired_segs_mng_show(struct pma_ub_acquired_segs_mng *mng, ka_seq_file_t *seq)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    struct pma_ub_acquired_seg *acquired_seg = NULL;
    struct pma_ub_acquired_seg *n = NULL;
    int i = 0;

    ka_task_down_read(&mng->rw_sem);

    ka_fs_seq_printf(seq, "    acquired_segs: udevid %u tgid %d num %llu\n", mng->udevid, mng->tgid, mng->num);

    ka_list_for_each_entry_safe(acquired_seg, n, &mng->head, node)
    {
        if (i == 0) {
            ka_fs_seq_printf(seq, "        %-5s %-17s %-15s\n", "id", "va", "size(Bytes)");
        }
        ka_fs_seq_printf(seq, "        %-5d 0x%-15llx %-15llu\n", i++, acquired_seg->va, acquired_seg->size);
        ka_try_cond_resched(&stamp);
    }

    ka_task_up_read(&mng->rw_sem);
}
