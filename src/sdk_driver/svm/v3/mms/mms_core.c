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
#include "ka_dfx_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_sched_pub.h"

#include "kernel_version_adapt.h"
#include "pbl_uda.h"
#include "dpa_kernel_interface.h"
#include "framework_task.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "mms_ctx.h"
#include "mms_core.h"
#include "mms_kern.h"
#include "svm_gup.h"

static int _mms_stats_mem_cfg(struct mms_ctx *mms_ctx, u64 va)
{
    u64 uva = va;
    u32 udevid = mms_ctx->udevid;
    u32 gup_flag = SVM_GUP_FLAG_ACCESS_WRITE | SVM_GUP_FLAG_CHECK_PA_LOCAL;
    int ret;

    mms_ctx->uva = uva;
    mms_ctx->npage_num = ka_base_round_up(sizeof(struct mms_stats), KA_MM_PAGE_SIZE) / KA_MM_PAGE_SIZE;

    mms_ctx->pages = (ka_page_t **)svm_kvmalloc(mms_ctx->npage_num * sizeof(ka_page_t *),
                                                KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (mms_ctx->pages == NULL) {
        svm_err("mms pages array failed. (udevid=%u; va=0x%llx; num=%llu)\n", udevid, uva, mms_ctx->npage_num);
        return -ENOMEM;
    }

    ret = svm_pin_uva_npages(uva, mms_ctx->npage_num, gup_flag, mms_ctx->pages, &mms_ctx->is_pfn_map);
    if (ret != 0) {
        svm_err("mms pin user pages failed. (ret=%d; udevid=%u; va=0x%llx; num=%llu)\n", ret, udevid, uva,
                mms_ctx->npage_num);
        svm_kvfree(mms_ctx->pages);
        mms_ctx->pages = NULL;
        return ret;
    }

    mms_ctx->stats = ka_mm_vmap(mms_ctx->pages, mms_ctx->npage_num, 0, KA_PAGE_KERNEL);
    if (mms_ctx->stats == NULL) {
        svm_err("mms vmap failed. (va=0x%llx; num=%llu)\n", uva, mms_ctx->npage_num);
        svm_unpin_uva_npages(mms_ctx->is_pfn_map, mms_ctx->pages, mms_ctx->npage_num, mms_ctx->npage_num);
        svm_kvfree(mms_ctx->pages);
        mms_ctx->pages = NULL;
        return -ENOMEM;
    }

    return 0;
}

int mms_stats_mem_cfg(struct mms_ctx *mms_ctx, u64 va)
{
    int ret;

    ka_task_down_write(&mms_ctx->rw_sem);
    ret = _mms_stats_mem_cfg(mms_ctx, va);
    ka_task_up_write(&mms_ctx->rw_sem);

    return ret;
}

void mms_stats_mem_decfg(struct mms_ctx *mms_ctx)
{
    if (mms_ctx == NULL) {
        return;
    }

    ka_task_down_write(&mms_ctx->rw_sem);

    if (mms_ctx->stats != NULL) {
        ka_mm_vunmap(mms_ctx->stats);
        mms_ctx->stats = NULL;
    }

    if (mms_ctx->pages != NULL) {
        svm_unpin_uva_npages(mms_ctx->is_pfn_map, mms_ctx->pages, mms_ctx->npage_num, mms_ctx->npage_num);
        svm_kvfree(mms_ctx->pages);
        mms_ctx->pages = NULL;
    }

    ka_task_up_write(&mms_ctx->rw_sem);
}

static void mms_print_mem_stats(ka_seq_file_t *seq, u32 udevid, u32 module_id, const char *type_name,
                                const char *module_name, struct mms_type_stats *type_stats)
{
    if (seq != NULL) {
        ka_fs_seq_printf(seq, "dev%-5d%-24s%-16s%-16u%-24llu%-24llu%-16llu%-16llu\n", udevid, type_name, module_name,
                         module_id, type_stats->alloced_size, type_stats->alloced_peak_size, type_stats->alloc_cnt,
                         type_stats->free_cnt);
    } else {
        svm_info("udevid=dev%u; type_name=%s; module_name=%s; module_id=%u; alloced_size=%llu; alloc_cnt=%llu; "
                 "free_cnt=%llu;\n",
                 udevid, type_name, module_name, module_id, type_stats->alloced_size, type_stats->alloc_cnt,
                 type_stats->free_cnt);
    }
}

SVM_DECLARE_MODULE_NAME(svm_module_name);
static void _mms_mem_task_show(u32 udevid, struct mms_stats *mms_stats, ka_seq_file_t *seq)
{
    u32 module_id;
    u32 mms_type;

    if (seq != NULL) {
        ka_fs_seq_printf(
            seq,
            "\nMem stats(Bytes):\nDevid   Mem_type           Module_name        Module_id       Current_alloced_size "
            "    Alloced_peak_size       Alloc_cnt       Free_cnt\n");
    } else {
        svm_info("\nMem stats(Bytes):\nDevid   Mem_type                Module_name     Module_id       "
                 "Current_alloced_size    Alloc_cnt       Free_cnt\n");
    }

    for (module_id = 0; module_id < MMS_MODULE_ID_MAX; module_id++) {
        for (mms_type = 0; mms_type < MMS_TYPE_MAX; mms_type++) {
            struct mms_type_stats *type_stats = &mms_stats->module_stats[module_id].type_stats[mms_type];
            if (type_stats == NULL) {
                continue;
            }
            if (type_stats->alloc_cnt == 0) {
                continue;
            }
            mms_print_mem_stats(seq, udevid, module_id, get_mms_type_name(mms_type),
                                SVM_GET_MODULE_NAME(svm_module_name, module_id), type_stats);
        }
    }
}

void mms_mem_task_show(u32 udevid, struct mms_stats *mms_stats, ka_seq_file_t *seq)
{
    if (mms_stats == NULL) {
        return;
    }
    _mms_mem_task_show(udevid, mms_stats, seq);
}

static void mms_get_stats(void *ctx, void *priv)
{
    struct mms_stats *stats = (struct mms_stats *)priv;
    struct mms_ctx *mms_ctx = (struct mms_ctx *)svm_task_get_feature_priv(ctx, mms_get_feature_id());
    u32 module_id;
    u32 mms_type;

    if (mms_ctx == NULL) {
        return;
    }

    ka_task_down_read(&mms_ctx->rw_sem);
    if (mms_ctx->stats == NULL) {
        ka_task_up_read(&mms_ctx->rw_sem);
        return;
    }

    for (module_id = 0; module_id < MMS_MODULE_ID_MAX; module_id++) {
        for (mms_type = 0; mms_type < MMS_TYPE_MAX; mms_type++) {
            struct mms_type_stats *src_stats = &mms_ctx->stats->module_stats[module_id].type_stats[mms_type];
            struct mms_type_stats *dst_stats = &stats->module_stats[module_id].type_stats[mms_type];

            if (src_stats->alloced_peak_size == 0) {
                continue;
            }

            dst_stats->alloced_size += src_stats->alloced_size;
            dst_stats->alloc_cnt += src_stats->alloc_cnt;
            dst_stats->free_cnt += src_stats->free_cnt;
        }
    }
    ka_task_up_read(&mms_ctx->rw_sem);
}

void svm_mms_dev_show(u32 udevid)
{
    struct mms_stats *stats;

    stats = (struct mms_stats *)svm_vzalloc(sizeof(struct mms_stats));
    if (stats == NULL) {
        svm_warn("mms_stats vzalloc Failed.(udevid=%u)\n", udevid);
        return;
    }

    svm_task_ctx_for_each(udevid, (void *)stats, mms_get_stats);
    mms_mem_task_show(udevid, stats, NULL);
    svm_vfree(stats);
}
