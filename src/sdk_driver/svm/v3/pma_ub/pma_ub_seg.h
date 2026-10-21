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
#ifndef PMA_UB_SEG_H
#define PMA_UB_SEG_H

#include "ka_base_pub.h"
#include "ka_common_pub.h"

#include "pbl_range_rbtree.h"

struct pma_ub_seg_mng {
    ka_rw_semaphore_t rw_sem;
    struct range_rbtree range_tree;
    u64 num;

    u32 udevid;
    int tgid;
};

void pma_ub_seg_mng_init(struct pma_ub_seg_mng *seg_mng, u32 udevid, int tgid);
void pma_ub_seg_mng_uninit(struct pma_ub_seg_mng *seg_mng);
void pma_ub_seg_recycle(struct pma_ub_seg_mng *seg_mng, u64 start, u64 size);

void pma_ub_seg_mng_show(struct pma_ub_seg_mng *seg_mng, ka_seq_file_t *seq);

int pma_ub_seg_add(struct pma_ub_seg_mng *seg_mng, u64 start, u64 size, u32 token_id);
int pma_ub_seg_del(struct pma_ub_seg_mng *seg_mng, u64 start, u64 size);

int pma_ub_seg_query(struct pma_ub_seg_mng *seg_mng, u64 va, u64 *start, u64 *size, u32 *token_id);

int pma_ub_seg_acquire(struct pma_ub_seg_mng *seg_mng, u64 va, u64 size, int (*invalidate)(u64 invalidate_tag),
                       u64 invalidate_tag, u32 *token_id);
int pma_ub_seg_release(struct pma_ub_seg_mng *seg_mng, u64 va, u64 size);

#endif
