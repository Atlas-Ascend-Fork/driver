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

#include "ka_memory_pub.h"
#include "ka_sched_pub.h"

#include "svm_gfp.h"
#include "svm_kern_log.h"
#include "svm_pub.h"
#include "svm_slab.h"
#include "smm_ctrl.h"

static u64 _smm_try_get_page_gran_aligned_size(u64 dst_va, u64 size, u64 gran_size)
{
    u64 aligned_up_start = svm_align_up(dst_va, gran_size);
    u64 aligned_down_end = svm_align_down(dst_va + size, gran_size);

    if (aligned_down_end <= aligned_up_start) {
        return size;
    }

    /* To ensure va and size is both aligned. */
    return (aligned_up_start > dst_va) ? (aligned_up_start - dst_va) : (aligned_down_end - dst_va);
}

static u64 smm_try_get_page_gran_aligned_size(u64 dst_va, u64 size, enum svm_page_granularity gran)
{
    u64 gran_size = svm_page_gran_to_page_size(gran);
    u64 size_ret;

    size_ret = _smm_try_get_page_gran_aligned_size(dst_va, size, gran_size);
    if ((SVM_IS_ALIGNED(dst_va, gran_size) == false) || (SVM_IS_ALIGNED(size_ret, gran_size) == false)) {
        if (gran > SVM_PAGE_GRAN_HUGE) {
            gran_size = svm_page_gran_to_page_size(gran - 1);
            size_ret = _smm_try_get_page_gran_aligned_size(dst_va, size, gran_size);
        }
    }

    return size_ret;
}

static u64 svm_try_get_max_single_aligned_size(u64 dst_va, u64 size, u64 max_single_size)
{
    u64 head_size;

    if (SVM_IS_ALIGNED(dst_va, max_single_size)) {
        return svm_min(size, max_single_size);
    }

    head_size = (svm_align_up(dst_va, max_single_size) - dst_va);
    return svm_min(size, head_size);
}

static u64 smm_try_get_page_aligned_single_size(struct smm_split_target *target, struct smm_split_policy *policy,
                                                u64 dst_va, u64 src_va, u64 size)
{
    enum svm_page_granularity max_pg_gran = (smm_get_pa_location(target->ctx, target->src_info.udevid,
                                                                 target->src_info.server_id) == SMM_LOCAL_PA) ?
                                                SVM_PAGE_GRAN_GIANT :
                                                SVM_PAGE_GRAN_HUGE;
    u64 ret_size;
    SVM_UNUSED(src_va);

    ret_size = svm_try_get_max_single_aligned_size(dst_va, size, policy->max_single_size);
    return smm_try_get_page_gran_aligned_size(dst_va, ret_size, max_pg_gran);
}

static int _smm_split_walk(struct smm_split_target *target, struct smm_split_policy *policy, smm_split_op op,
                           smm_split_op rollback_op, struct svm_pa_seg *pa_seg, u64 seg_num)
{
    unsigned long stamp = ka_jiffies;
    u64 total_size = target->src_info.size;
    u64 finished_size = 0;
    int ret = 0;

    while (finished_size < total_size) {
        struct svm_global_va single_src_info = target->src_info;
        struct smm_addr single_sa = {0};
        u64 single_dst_va = target->dst_va + finished_size;

        single_src_info.va += finished_size;
        single_src_info.size = smm_try_get_page_aligned_single_size(target, policy, single_dst_va, single_src_info.va,
                                                                    total_size - finished_size);

        smm_addr_packet(&single_sa, single_dst_va, single_src_info.size, pa_seg, seg_num);
        ret = op(target->ctx, target->task, &single_src_info, &single_sa, target->flag);
        if (ret != 0) {
            if (rollback_op != NULL) {
                struct smm_split_target roll_back_target = *target;
                roll_back_target.src_info.size = finished_size;
                (void)_smm_split_walk(&roll_back_target, policy, rollback_op, NULL, pa_seg, seg_num);
            }
            break;
        }
        finished_size += single_src_info.size;
        ka_try_cond_resched(&stamp);
    }

    return ret;
}

int smm_split_walk(struct smm_split_target *target, struct smm_split_policy *policy, smm_split_op op,
                   smm_split_op rollback_op)
{
    struct svm_pa_seg *pa_seg = NULL;
    u64 seg_num = 0;
    int ret;

    if (policy->need_pa_seg) {
        /* use min_page_size is reserve enough seg num, real seg num will be got in pmq */
        seg_num = svm_min(svm_get_align_up_num(target->src_info.va, target->src_info.size, policy->min_page_size),
                          policy->max_single_size / policy->min_page_size);

        pa_seg = (struct svm_pa_seg *)svm_kvmalloc(sizeof(struct svm_pa_seg) * seg_num,
                                                   KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (pa_seg == NULL) {
            svm_err("Smm kvmalloc pa_seg failed. (alloc_seg_num=%llu)\n", seg_num);
            return -ENOMEM;
        }
    }

    ret = _smm_split_walk(target, policy, op, rollback_op, pa_seg, seg_num);
    if (pa_seg != NULL) {
        svm_kvfree(pa_seg);
    }
    return ret;
}
