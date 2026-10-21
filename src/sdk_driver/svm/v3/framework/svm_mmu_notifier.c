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
#include "ka_memory_pub.h"
#include "ka_errno_pub.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "framework_vma.h"
#include "svm_mem_recycle.h"
#include "svm_mmap_fops.h"
#include "svm_mmu_notifier.h"

static struct svm_mmu_notifier_ctx *svm_mmu_notifier_ctx_alloc(void)
{
    struct svm_mmu_notifier_ctx *mn_ctx = NULL;

    mn_ctx = svm_kzalloc(sizeof(struct svm_mmu_notifier_ctx), KA_GFP_ATOMIC | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT);
    if (mn_ctx == NULL) {
        svm_err("Failed to kzalloc svm_mmu_notifier_ctx.\n");
        return NULL;
    }

    mn_ctx->status = VMA_STATUS_IDLE;
    mn_ctx->tgid = ka_task_get_current_tgid();
    return mn_ctx;
}

static void svm_mmu_notifier_ctx_free(struct svm_mmu_notifier_ctx *mn_ctx)
{
    svm_kfree(mn_ctx);
}

static void svm_mmu_notifier_mem_recycle(ka_vm_area_struct_t *vma, int tgid)
{
    /*  munmap full range, but not depopulate all mem. */
    u64 recycle_size = svm_mem_recycle(vma, tgid);
    if (recycle_size > 0) {
        svm_warn("Unnormal munmap, recycle. (vm_start=0x%lx; vm_end=0x%lx; recycle_size=0x%llx)\n",
                 ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma), recycle_size);
    }
}

/*
   os call this ops(mmu_notifier_ops.invalidate_range_start) when destroy page table:
     1. call munmap partial/full in user
     2. call zap_vma_ptes partial/full in kernel
     3. call mmu_notifier_invalidate_range_start in kernel when needed, for example: free huge table devmm_zap_vma_pmds
   call munmap partial in user illegal, os will call ops devmm_vm_open(vm_operations_struct.open) also,
       use this to check illegal call
   we only recycle mem when user not depopulate all mem before munmap full range,
      munmap partly range will recycle in svm_vma_open
*/
static int _svm_notifier_invalid_start(ka_mmu_notifier_t *mn, ka_mm_struct_t *mm, unsigned long start,
                                       unsigned long end, bool blockable)
{
    struct svm_mmu_notifier_ctx *mn_ctx = ka_container_of(mn, struct svm_mmu_notifier_ctx, mn);
    ka_vm_area_struct_t *vma = mn_ctx->vma;

    if (!svm_is_svm_vma(vma)) {
        /* not svm vma */
        return 0;
    }

    /* invalid may be bigger than vma */
    if ((start <= ka_mm_get_vm_start(vma)) && (end >= ka_mm_get_vm_end(vma))) {
        if (mn_ctx->status == VMA_STATUS_IDLE) {
            /*
             * If blockable argument is set to false then the callback cannot
             * sleep and has to return with -EAGAIN. 0 should be returned
             * otherwise
             */
            if (blockable == false) {
                return -EAGAIN;
            }

            svm_mmu_notifier_mem_recycle(vma, mn_ctx->tgid);
        }
    }

    return 0;
}

/* called either by ka_mm_mmu_notifier_unregister or when the mm is being destroyed by exit_mmap,
   always before all pages are freed. */
static void svm_notifier_release(ka_mmu_notifier_t *mn, ka_mm_struct_t *mm)
{
    /* do nothing, recycle page in _svm_notifier_invalid_start */
}

KA_DEFINE_MMU_NOTIFIER_INVALIDATE_RANGE_START_FN(_svm_notifier_invalid_start)

ka_mmu_notifier_ops_t svm_mmu_notifier = {
    KA_MM_MMU_NOTIFIER_OPS_INIT_INVALIDATE_RANGE_START(_svm_notifier_invalid_start) /* For user munmap. */
        .release = svm_notifier_release,                                            /* For task exit. */
};

int svm_mmu_notifier_register(ka_mm_struct_t *mm, ka_vm_area_struct_t *vma, struct svm_mmu_notifier_ctx **mn_ctx)
{
    struct svm_mmu_notifier_ctx *tmp_mn_ctx = NULL;
    int ret;

    tmp_mn_ctx = svm_mmu_notifier_ctx_alloc();
    if (tmp_mn_ctx == NULL) {
        return -ENOMEM;
    }

    tmp_mn_ctx->mn.ops = &svm_mmu_notifier;
    tmp_mn_ctx->vma = vma;

    /* we have locked mm, so not use mmu_notifier_register */
    ret = __ka_mm_mmu_notifier_register(&tmp_mn_ctx->mn, mm);
    if (ret != 0) {
        svm_mmu_notifier_ctx_free(tmp_mn_ctx);
        return ret;
    }

    *mn_ctx = tmp_mn_ctx;
    return 0;
}

void svm_mmu_notifier_unregister(struct svm_mmu_notifier_ctx *mn_ctx)
{
    ka_mm_mmu_notifier_unregister(&mn_ctx->mn, mn_ctx->vma->vm_mm);
    svm_mmu_notifier_ctx_free(mn_ctx);
}
