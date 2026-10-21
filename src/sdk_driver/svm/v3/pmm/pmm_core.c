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
#include "ka_common_pub.h"
#include "ka_system_pub.h"
#include "ka_fs_pub.h"
#include "ka_memory_pub.h"
#include "ka_sched_pub.h"

#include "kernel_version_adapt.h"

#include "svm_pub.h"
#include "svm_kern_log.h"
#include "framework_vma.h"
#include "svm_pgtable.h"
#include "svm_gfp.h"
#include "svm_slab.h"
#include "svm_mc.h"
#include "pmm.h"
#include "pmm_ctx.h"
#include "pmm_core.h"

#define PMM_SEG_BIT_STATS_OP_ADD 0U
#define PMM_SEG_BIT_STATS_OP_SUB 1U

struct pmm_recycle_priv {
    struct pmm_ctx *pmm_ctx;
    ka_vm_area_struct_t *vma;
    u32 udevid;
    int tgid;
    u64 recycle_size;
};

static int _pmm_seg_bit_stats_op(struct pmm_ctx *pmm_ctx, u64 bit, u64 size, u32 op)
{
    if (op == PMM_SEG_BIT_STATS_OP_ADD) {
        if ((u64)(pmm_ctx->bit_size_stats[bit] + (u32)size) > PMM_MEM_SIZE_PER_BIT) {
            svm_err("Pmm add fail. (stats.size=%u; sz=%llu)\n", pmm_ctx->bit_size_stats[bit], size);
            return -EINVAL;
        }

        pmm_ctx->bit_size_stats[bit] += (u32)size;
    } else {
        if (pmm_ctx->bit_size_stats[bit] < (u32)size) {
            svm_err("Pmm del fail. (stats.size=%u; sz=%llu)\n", pmm_ctx->bit_size_stats[bit], size);
            return -EINVAL;
        }

        pmm_ctx->bit_size_stats[bit] -= (u32)size;
    }

    return 0;
}

static u64 pmm_seg_bit_stats_op(struct pmm_ctx *pmm_ctx, u64 va, u64 size, u32 op)
{
    u64 start_bit = (va - pmm_ctx->base_va) / pmm_ctx->size_per_bit;
    u64 end_bit = ((va + size - 1) - pmm_ctx->base_va) / pmm_ctx->size_per_bit;
    u64 nbits = end_bit - start_bit + 1;
    u64 i, sz, op_size = 0;
    int ret;

    for (i = start_bit; i <= end_bit; i++) {
        if (nbits == 1) {
            sz = size;
        } else if (i == start_bit) {
            sz = pmm_ctx->size_per_bit - (va - ka_base_round_down(va, pmm_ctx->size_per_bit));
        } else if (i == end_bit) {
            sz = va + size - ka_base_round_down(va + size - 1, pmm_ctx->size_per_bit);
        } else {
            sz = pmm_ctx->size_per_bit;
        }

        ret = _pmm_seg_bit_stats_op(pmm_ctx, i, sz, op);
        if (ret != 0) {
            svm_err("Ops bit stats failed. (va=0x%llx; size=%llu; "
                    "nbits=%llu; start_bit=%llu; end_bit=%llu; i=%llu; op=%u; sz=%llu)\n",
                    va, size, nbits, start_bit, end_bit, i, op, sz);
            break;
        }
        op_size += sz;
    }

    return op_size;
}

static int pmm_seg_bit_stats_add(struct pmm_ctx *pmm_ctx, u64 va, u64 size)
{
    u64 op_size = pmm_seg_bit_stats_op(pmm_ctx, va, size, PMM_SEG_BIT_STATS_OP_ADD);
    if (op_size != size) {
        (void)pmm_seg_bit_stats_op(pmm_ctx, va, op_size, PMM_SEG_BIT_STATS_OP_SUB);
        return -EINVAL;
    }

    return 0;
}

static int pmm_seg_bit_stats_sub(struct pmm_ctx *pmm_ctx, u64 va, u64 size)
{
    u64 op_size = pmm_seg_bit_stats_op(pmm_ctx, va, size, PMM_SEG_BIT_STATS_OP_SUB);
    if (op_size != size) {
        (void)pmm_seg_bit_stats_op(pmm_ctx, va, op_size, PMM_SEG_BIT_STATS_OP_ADD);
        return -EINVAL;
    }

    return 0;
}

static int _pmm_add_seg(struct pmm_ctx *pmm_ctx, u64 va, u64 size, u32 flag)
{
    int ret;

    if (svm_check_va_range(va, size, pmm_ctx->base_va, pmm_ctx->base_va + pmm_ctx->total_size) != 0) {
        svm_err("Not in range. (udevid=%u; va=0x%llx; size=0x%llx; pmm va=0x%llx; size=0x%llx)\n", pmm_ctx->udevid, va,
                size, pmm_ctx->base_va, pmm_ctx->total_size);
        return -EINVAL;
    }

    ka_task_down_write(&pmm_ctx->rwsem);
    ret = pmm_seg_bit_stats_add(pmm_ctx, va, size);
    ka_task_up_write(&pmm_ctx->rwsem);
    if (ret != 0) {
        return ret;
    }

    if ((flag & PMM_SEG_WITH_PA) != 0) {
        ka_base_atomic64_add(size, &pmm_ctx->pa_size);
    }

    return 0;
}

static int _pmm_del_seg(struct pmm_ctx *pmm_ctx, u64 va, u64 size, u32 flag)
{
    int ret;

    if (svm_check_va_range(va, size, pmm_ctx->base_va, pmm_ctx->base_va + pmm_ctx->total_size) != 0) {
        svm_err("Not in range. (udevid=%u; va=0x%llx; size=0x%llx; pmm va=0x%llx; size=0x%llx)\n", pmm_ctx->udevid, va,
                size, pmm_ctx->base_va, pmm_ctx->total_size);
        return -EINVAL;
    }

    ka_task_down_write(&pmm_ctx->rwsem);
    ret = pmm_seg_bit_stats_sub(pmm_ctx, va, size);
    ka_task_up_write(&pmm_ctx->rwsem);
    if (ret != 0) {
        return ret;
    }

    if ((flag & PMM_SEG_WITH_PA) != 0) {
        ka_base_atomic64_sub(size, &pmm_ctx->pa_size);
    }

    return 0;
}

int pmm_add_seg(u32 udevid, u64 va, u64 size, u32 flag)
{
    struct pmm_ctx *pmm_ctx = NULL;
    int ret;

    pmm_ctx = pmm_ctx_get(udevid, ka_task_get_current_tgid());
    if (pmm_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ret = _pmm_add_seg(pmm_ctx, va, size, flag);
    pmm_ctx_put(pmm_ctx);

    return ret;
}

int pmm_del_seg(u32 udevid, u64 va, u64 size, u32 flag)
{
    struct pmm_ctx *pmm_ctx = NULL;
    int ret;

    pmm_ctx = pmm_ctx_get(udevid, ka_task_get_current_tgid());
    if (pmm_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ret = _pmm_del_seg(pmm_ctx, va, size, flag);
    pmm_ctx_put(pmm_ctx);

    return ret;
}

static int _pmm_for_each_seg(struct pmm_ctx *pmm_ctx, u64 start, u64 size, int (*func)(void *priv, u64 va, u64 size),
                             void *priv)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    u64 start_bit = (start - pmm_ctx->base_va) / pmm_ctx->size_per_bit;
    u64 end_bit = (start + size - pmm_ctx->base_va) / pmm_ctx->size_per_bit;
    u64 va, i;
    int ret;

    for (i = start_bit; i < end_bit; i++) {
        if (pmm_ctx->bit_size_stats[i] != 0) {
            va = pmm_ctx->base_va + i * pmm_ctx->size_per_bit;

            ret = func(priv, va, pmm_ctx->size_per_bit);
            if (ret != 0) {
                return ret;
            }
        }
        ka_try_cond_resched(&stamp);
    }

    return 0;
}

int pmm_for_each_seg(u32 udevid, int tgid, int (*func)(void *priv, u64 va, u64 size), void *priv)
{
    struct pmm_ctx *pmm_ctx = NULL;
    int ret;

    pmm_ctx = pmm_ctx_get(udevid, tgid);
    if (pmm_ctx == NULL) { /* Process may exit already, don't print. */
        return -EINVAL;
    }

    ret = _pmm_for_each_seg(pmm_ctx, pmm_ctx->base_va, pmm_ctx->nbits * pmm_ctx->size_per_bit, func, priv);
    pmm_ctx_put(pmm_ctx);

    return ret;
}

static void _pmm_mem_recycle_single_addr(ka_vm_area_struct_t *vma, u64 va, u64 size, struct svm_pa_seg *pa_seg,
                                         u64 seg_num, u64 page_size)
{
    bool is_local_mem = svm_pa_is_local_mem(pa_seg[0].pa);
    u32 free_page_flag = 0;
    u64 i, j;
    int ret;

    /* dose not unmap full vma to avoid dead lock
       call trace: pmm_recycle->_svm_notifier_invalid_start->pmm_recycle */
    if ((ka_mm_get_vm_start(vma) == va) && (ka_mm_get_vm_end(vma) == (va + size))) {
        u64 unmap_size = size / 2; /* 2: full unmap to 2 partly unmap */

        svm_info("Recycle full vma range. (va=0x%llx; size=0x%llx)\n", va, size);

        ret = svm_unmap_addr(vma, va, unmap_size, page_size);
        if (ret != 0) {
            svm_warn("Unmap failed. (va=0x%llx; size=0x%llx; page_size=0x%llx)\n", va, unmap_size, page_size);
        }
        ret = svm_unmap_addr(vma, va + unmap_size, unmap_size, page_size);
        if (ret != 0) {
            svm_warn("Unmap failed. (va=0x%llx; size=0x%llx; page_size=0x%llx)\n", va + unmap_size, unmap_size,
                     page_size);
        }
    } else {
        ret = svm_unmap_addr(vma, va, size, page_size);
        if (ret != 0) {
            svm_warn("Unmap failed. (va=0x%llx; size=0x%llx; page_size=0x%llx)\n", va, size, page_size);
        }
    }

    if (!is_local_mem) {
        return;
    }

    for (i = 0; i < seg_num; i++) {
        for (j = 0; j < pa_seg[i].size; j += page_size) {
            ka_page_t *page = svm_pa_to_page(pa_seg[i].pa + j);
            svm_free_pages(svm_page_size_to_page_gran(page_size), &page, 1, free_page_flag);
        }
    }
}

static void pmm_mem_recycle_single_addr(ka_vm_area_struct_t *vma, u64 va, u64 size, struct svm_pa_seg *pa_seg,
                                        u64 seg_num)
{
    u64 i, page_size, cur_size;
    u64 cur_va = va;

    for (i = 0; i < seg_num;) {
        u64 tmp_seg_num = svm_get_same_page_size_seg_num_by_pa_seg(cur_va, &pa_seg[i], seg_num - i, &cur_size,
                                                                   &page_size);
        if (tmp_seg_num == 0) {
            svm_warn("Invalid pa seg. (va=0x%llx; size=0x%llx; cur_seg_num=%llu)\n", cur_va, pa_seg[i].size,
                     seg_num - i);
            return;
        }

        _pmm_mem_recycle_single_addr(vma, cur_va, cur_size, &pa_seg[i], tmp_seg_num, page_size);
        cur_va += cur_size;
        i += tmp_seg_num;
    }
}

static int pmm_mem_recycle_singe_seg(void *priv, u64 va, u64 size)
{
#define PMM_RECYCLE_DEFAULT_PA_SEG_NUM 16
    unsigned long stamp = ka_jiffies;
    struct pmm_recycle_priv *recycle_priv = (struct pmm_recycle_priv *)priv;
    ka_vm_area_struct_t *vma = recycle_priv->vma;
    struct pmm_ctx *pmm_ctx = recycle_priv->pmm_ctx;
    struct svm_pa_seg pa_seg_tmp[PMM_RECYCLE_DEFAULT_PA_SEG_NUM];
    struct svm_pa_seg *pa_seg = NULL;
    u64 seg_num;
    u64 cur_va = va;
    u64 end = va + size;

    if (vma == NULL) {
        vma = ka_mm_find_vma(pmm_ctx->mm, va);
        if (vma == NULL) {
            svm_warn("Find vma failed. (va=0x%llx)\n", va);
            return 0;
        }

        if (svm_check_vma(vma, va, size) != 0) {
            svm_warn("Check vma failed. (vm_start=0x%lx; vm_end=0x%lx; va=0x%llx; size=0x%llx)\n",
                     ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma), va, size);
            return 0;
        }
    }

    pmm_ctx->recycling_vma = vma;

    seg_num = svm_get_align_up_num(va, size, KA_MM_PAGE_SIZE);
    pa_seg = (struct svm_pa_seg *)svm_kvmalloc(sizeof(struct svm_pa_seg) * seg_num,
                                               KA_GFP_ATOMIC | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT);
    if (pa_seg == NULL) {
        /* use small resource to recycle */
        seg_num = PMM_RECYCLE_DEFAULT_PA_SEG_NUM;
        pa_seg = pa_seg_tmp;
    }

    while (cur_va < end) {
        u64 cur_seg_num, query_size;

        ka_try_cond_resched(&stamp);
        cur_seg_num = seg_num;
        query_size = svm_query_phys(vma, cur_va, end - cur_va, pa_seg, &cur_seg_num);
        if (query_size == 0) { /* cur va is hole */
            cur_va += KA_MM_PAGE_SIZE;
            continue;
        }

        pmm_mem_recycle_single_addr(vma, cur_va, query_size, pa_seg, cur_seg_num);
        pmm_seg_bit_stats_sub(pmm_ctx, cur_va, query_size);
        cur_va += query_size;
        recycle_priv->recycle_size += query_size;
    }

    if (pa_seg != pa_seg_tmp) {
        svm_kvfree(pa_seg);
    }

    pmm_ctx->recycling_vma = NULL;

    return 0;
}

static u64 _pmm_mem_recycle(ka_vm_area_struct_t *vma, struct pmm_ctx *pmm_ctx)
{
    struct pmm_recycle_priv recycle_priv = {
        .pmm_ctx = pmm_ctx, .vma = vma, .udevid = pmm_ctx->tgid, .tgid = pmm_ctx->tgid, .recycle_size = 0};
    u64 start, size;

    if (vma != NULL) {
        start = ka_mm_get_vm_start(vma);
        size = ka_mm_get_vm_end(vma) - ka_mm_get_vm_start(vma);
    } else {
        start = pmm_ctx->base_va;
        size = pmm_ctx->nbits * pmm_ctx->size_per_bit;
    }

    (void)_pmm_for_each_seg(pmm_ctx, start, size, pmm_mem_recycle_singe_seg, (void *)&recycle_priv);
    return recycle_priv.recycle_size;
}

u64 pmm_mem_recycle_by_vma(ka_vm_area_struct_t *vma, u32 udevid, int tgid)
{
    struct pmm_ctx *pmm_ctx = NULL;
    u64 recycle_size = 0;

    pmm_ctx = pmm_ctx_get(udevid, tgid);
    if (pmm_ctx != NULL) {
        /* if recycling_vma is not null, Indicating that we are in the recycling process
           call trace: pmm_mem_recycle-->_svm_notifier_invalid_start--->pmm_mem_recycle_by_vma */
        if ((pmm_ctx->recycling_vma == NULL) && (ka_mm_get_vm_start(vma) >= pmm_ctx->base_va) &&
            (ka_mm_get_vm_end(vma) <= (pmm_ctx->base_va + pmm_ctx->nbits * pmm_ctx->size_per_bit))) {
            recycle_size = _pmm_mem_recycle(vma, pmm_ctx);
        }
        pmm_ctx_put(pmm_ctx);
    }

    return recycle_size;
}

void pmm_mem_recycle(struct pmm_ctx *pmm_ctx)
{
    ka_task_down_write(get_mmap_sem(pmm_ctx->mm));
    ka_task_down_write(&pmm_ctx->rwsem);
    (void)_pmm_mem_recycle(NULL, pmm_ctx); /* now we has not a vma, find vma when we have a va */
    ka_task_up_write(&pmm_ctx->rwsem);
    ka_task_up_write(get_mmap_sem(pmm_ctx->mm));
}

void pmm_mem_show(struct pmm_ctx *pmm_ctx, ka_seq_file_t *seq)
{
    u64 va, size, i;

    ka_task_down_read(&pmm_ctx->rwsem);

    ka_fs_seq_printf(seq, "udevid %u tgid %d nbits %llu pa_size %llu\n", pmm_ctx->udevid, pmm_ctx->tgid, pmm_ctx->nbits,
                     (u64)ka_base_atomic64_read(&pmm_ctx->pa_size));

    for (i = 0; i < pmm_ctx->nbits; i++) {
        if (pmm_ctx->bit_size_stats[i] != 0) {
            va = pmm_ctx->base_va + i * pmm_ctx->size_per_bit;
            size = pmm_ctx->size_per_bit;

            ka_fs_seq_printf(seq, "   bit %llu va %llx size %llx size_stats %u Bytes\n", i, va, size,
                             pmm_ctx->bit_size_stats[i]);
        }
    }

    ka_task_up_read(&pmm_ctx->rwsem);
}

int pmm_mem_query(u32 udevid, int tgid, u64 *out_size)
{
    struct pmm_ctx *pmm_ctx = NULL;

    pmm_ctx = pmm_ctx_get(udevid, tgid);
    if (pmm_ctx == NULL) {
        *out_size = 0ULL;
        return 0;
    }

    *out_size = (u64)ka_base_atomic64_read(&pmm_ctx->pa_size);
    pmm_ctx_put(pmm_ctx);

    return 0;
}
