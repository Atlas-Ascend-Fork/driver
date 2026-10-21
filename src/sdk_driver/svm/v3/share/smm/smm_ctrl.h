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

#ifndef SMM_CTRL_H
#define SMM_CTRL_H

#include "ka_task_pub.h"

#include "svm_addr_desc.h"
#include "smm_ctx.h"

struct smm_addr {
    u64 dst_va;
    u64 dst_size;
    struct svm_pa_seg *src_pa_seg;
    u64 seg_num;
    u64 dst_page_size;
};

static inline void smm_addr_packet(struct smm_addr *sa, u64 dst_va, u64 dst_size, struct svm_pa_seg *src_pa_seg,
                                   u64 seg_num)
{
    sa->dst_va = dst_va;
    sa->dst_size = dst_size;
    sa->src_pa_seg = src_pa_seg;
    sa->seg_num = seg_num;
    sa->dst_page_size = 0; /* output para for smm_external_mmap, smm_internal_mmap */
}

struct smm_split_target {
    struct smm_ctx *ctx;
    ka_task_struct_t *task;
    struct svm_global_va src_info;
    u64 dst_va;
    u64 flag;
};

struct smm_split_policy {
    u64 min_page_size;
    u64 max_single_size;
    bool need_pa_seg;
};

typedef int (*smm_split_op)(struct smm_ctx *ctx, ka_task_struct_t *task, struct svm_global_va *single_src_info,
                            struct smm_addr *single_sa, u64 flag);

int smm_split_walk(struct smm_split_target *target, struct smm_split_policy *policy, smm_split_op op,
                   smm_split_op rollback_op);

#endif
