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
#include "ka_dfx_pub.h"
#include "ka_common_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_sched_pub.h"

#include "pbl_feature_loader.h"
#include "pbl_task_ctx.h"
#include "dpa_kernel_interface.h"
#include "pbl_uda.h"

#include "svm_pub.h"
#include "svm_pgtable.h"
#include "svm_kern_log.h"
#include "svm_slab.h"
#include "framework_task.h"
#include "framework_vma.h"
#include "svm_mm.h"
#include "va_mng.h"
#include "pmm.h"
#include "pmm_core.h"
#include "pmm_ctx.h"

u32 pmm_feature_id;

struct pmm_ctx *pmm_ctx_get(u32 udevid, int tgid)
{
    struct pmm_ctx *pmm_ctx = NULL;
    void *task_ctx = NULL;

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        return NULL;
    }

    pmm_ctx = (struct pmm_ctx *)svm_task_get_feature_priv(task_ctx, pmm_feature_id);
    if (pmm_ctx == NULL) {
        svm_task_ctx_put(task_ctx);
    }

    return pmm_ctx;
}

void pmm_ctx_put(struct pmm_ctx *pmm_ctx)
{
    svm_task_ctx_put(pmm_ctx->task_ctx);
}

#if (PMM_MEM_SIZE_PER_BIT > KA_UINT_MAX)
#error "PMM_MEM_SIZE_PER_BIT is out of u32 range.!"
#endif

static int pmm_ctx_init(struct pmm_ctx *pmm_ctx)
{
    u64 va, size;

    pmm_ctx->mm = ka_task_get_current_mm();
    if (pmm_ctx->mm == NULL) {
        svm_err("Get mm failed. (tgid=%d)\n", pmm_ctx->tgid);
        return -ESRCH;
    }

    svm_get_va_range(&va, &size);
    if (pmm_ctx->udevid == uda_get_host_id()) {
        /* include pci th va range */
        u64 pci_th_va, pci_th_size;
        svm_get_pcie_th_va_range(&pci_th_va, &pci_th_size);
        size += (va - pci_th_va);
        va = pci_th_va;
    }

    pmm_ctx->base_va = va;
    pmm_ctx->total_size = size;
    pmm_ctx->size_per_bit = PMM_MEM_SIZE_PER_BIT;
    pmm_ctx->nbits = size / PMM_MEM_SIZE_PER_BIT;
    pmm_ctx->recycling_vma = NULL;
    ka_task_init_rwsem(&pmm_ctx->rwsem);

    pmm_ctx->bit_size_stats = svm_vzalloc(pmm_ctx->nbits * sizeof(u32)); // clear zero
    if (pmm_ctx->bit_size_stats == NULL) {
        return -ENOMEM;
    }

    ka_base_atomic64_set(&pmm_ctx->pa_size, 0);
    return 0;
}

static void pmm_ctx_uninit(struct pmm_ctx *pmm_ctx)
{
    svm_vfree(pmm_ctx->bit_size_stats);
}

static void pmm_ctx_release(void *priv)
{
    struct pmm_ctx *ctx = (struct pmm_ctx *)priv;
    pmm_ctx_uninit(ctx);
    svm_kfree(ctx);
}

int pmm_init_task(u32 udevid, int tgid, void *start_time)
{
    struct pmm_ctx *pmm_ctx = NULL;
    void *task_ctx = NULL;
    int ret;

    pmm_ctx = (struct pmm_ctx *)svm_kzalloc(sizeof(*pmm_ctx), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pmm_ctx == NULL) {
        svm_err("No mem. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -ENOMEM;
    }

    pmm_ctx->udevid = udevid;
    pmm_ctx->tgid = tgid;
    ret = pmm_ctx_init(pmm_ctx);
    if (ret != 0) {
        svm_err("Pmm ctx init failed. (ret=%d)\n", ret);
        svm_kfree(pmm_ctx);
        return ret;
    }

    task_ctx = svm_task_ctx_get(udevid, tgid);
    if (task_ctx == NULL) {
        pmm_ctx_uninit(pmm_ctx);
        svm_kfree(pmm_ctx);
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EINVAL;
    }

    pmm_ctx->task_ctx = task_ctx;

    ret = svm_task_set_feature_priv(task_ctx, pmm_feature_id, "pmm", (void *)pmm_ctx, pmm_ctx_release);
    if (ret != 0) {
        svm_task_ctx_put(task_ctx);
        pmm_ctx_uninit(pmm_ctx);
        svm_kfree(pmm_ctx);
        return ret;
    }

    svm_inst_trace("Init task. (udevid=%u; tgid=%d)\n", udevid, tgid);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_TASK(pmm_init_task, FEATURE_LOADER_STAGE_2);

static void pmm_destroy_task(struct pmm_ctx *pmm_ctx)
{
    svm_inst_trace("Destroy task. (udevid=%u; tgid=%d)\n", pmm_ctx->udevid, pmm_ctx->tgid);
    svm_task_set_feature_invalid(pmm_ctx->task_ctx, pmm_feature_id);
    pmm_mem_recycle(pmm_ctx);
    svm_task_ctx_put(pmm_ctx->task_ctx); /* with init pair */
}

void pmm_uninit_task(u32 udevid, int tgid, void *start_time)
{
    struct pmm_ctx *pmm_ctx = NULL;

    pmm_ctx = pmm_ctx_get(udevid, tgid);
    if (pmm_ctx == NULL) {
        return;
    }

    pmm_ctx_put(pmm_ctx);

    if (!svm_task_is_exit_abort(pmm_ctx->task_ctx)) {
        pmm_destroy_task(pmm_ctx);
        svm_inst_trace("Uninit task. (udevid=%u; tgid=%d)\n", udevid, tgid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(pmm_uninit_task, FEATURE_LOADER_STAGE_2);

void pmm_show_task(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    struct pmm_ctx *pmm_ctx = NULL;

    if (feature_id != pmm_feature_id) {
        return;
    }

    pmm_ctx = pmm_ctx_get(udevid, tgid);
    if (pmm_ctx == NULL) {
        svm_err("Invalid dev task. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return;
    }

    pmm_mem_show(pmm_ctx, seq);

    pmm_ctx_put(pmm_ctx);
}
DECLAER_FEATURE_AUTO_SHOW_TASK(pmm_show_task, FEATURE_LOADER_STAGE_2);

static u64 pmm_mem_recycle_without_mm_lock(ka_vm_area_struct_t *vma, int tgid)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    u32 max_udev_num = uda_get_udev_max_num();
    u64 recycle_size = 0;
    u32 udevid;

    for (udevid = 0; udevid < max_udev_num; udevid++) {
        recycle_size += pmm_mem_recycle_by_vma(vma, udevid, tgid);
        ka_try_cond_resched(&stamp);
    }

    return recycle_size;
}

int pmm_init(void)
{
    pmm_feature_id = svm_task_obtain_feature_id();
    svm_mem_recycle_register(pmm_mem_recycle_without_mm_lock);
    (void)apm_proc_mem_query_handle_register(pmm_mem_query);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(pmm_init, FEATURE_LOADER_STAGE_2);

void pmm_uninit(void)
{
    apm_proc_mem_query_handle_unregister();
}
DECLAER_FEATURE_AUTO_UNINIT(pmm_uninit, FEATURE_LOADER_STAGE_2);
