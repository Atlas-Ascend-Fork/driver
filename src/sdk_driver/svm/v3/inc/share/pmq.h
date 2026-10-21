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

#ifndef PMQ_H
#define PMQ_H

#include "svm_addr_desc.h"
#include "svm_pub.h"
#include "svm_kern_log.h"
#include "svm_gfp.h"

/*
    PMQ: physical memory query
*/

/* queried pa seg is <= size accord to seg num space, will merge continues pa */
int svm_pmq_pa_query(int tgid, u64 va, u64 size, struct svm_pa_seg pa_seg[], u64 *seg_num);
int svm_pmq_pa_get(int tgid, u64 va, u64 size, struct svm_pa_seg pa_seg[], u64 *seg_num);
void svm_pmq_pa_put(struct svm_pa_seg pa_seg[], u64 seg_num);
int svm_pmq_query_va(int tgid, u64 pa, u64 start_va, u64 end_va, u64 *matched_va);

/* Will not merge continues pa */
static inline int svm_pmq_get_pa(int tgid, u64 va, u64 size, struct svm_pa_seg pa_seg[], u64 *seg_num)
{
    int ret = svm_pmq_pa_get(tgid, va, size, pa_seg, seg_num);
    if (ret == 0) {
        svm_pmq_pa_put(pa_seg, *seg_num);
    }
    return ret;
}

static inline void pmq_try_recycle_local_mem(struct svm_global_va *src_info, struct svm_pa_seg pa_seg[], u64 seg_num)
{
    if (svm_pa_is_local_mem(pa_seg[0].pa)) {
        svm_pmq_pa_put(pa_seg, seg_num);
        svm_warn("Recycled local memory. (udevid=%u, va=0x%llx, size=%llx)\n", src_info->udevid, src_info->va,
                 src_info->size);
    }
}

#endif