/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
#ifndef SVM_VMM_ACCESS_COMM_H
#define SVM_VMM_ACCESS_COMM_H

#include "ka_type.h"

#include "devmm_proc_info.h"
#include "svm_heap_mng.h"
#include "svm_pa_seg.h"
#include "svm_vmm_access_msg.h"

#define SVM_VMM_ACCESS_PA_QUERY_FLAG_RAW_PA (1u << 0)
#define SVM_VMM_ACCESS_PA_QUERY_FLAG_HOST_BAR (1u << 1)

static inline bool svm_vmm_access_query_flag_is_raw_pa(u32 query_flag)
{
    return ((query_flag & SVM_VMM_ACCESS_PA_QUERY_FLAG_RAW_PA) != 0);
}

int svm_vmm_access_va_map_local(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, u64 va, u64 size,
                                const struct svm_pa_seg pa_seg[], u32 seg_num);
int svm_vmm_access_va_unmap_local(struct devmm_svm_process *svm_proc, u64 va, u64 size);
int svm_vmm_access_pa_query_local(struct devmm_svm_process *svm_proc, int phy_addr_blk_id, u64 offset,
                                  struct svm_pa_seg pa_seg[], u32 *seg_num);
int svm_vmm_pa_ref_inc_local(struct devmm_svm_process *svm_proc, int phy_addr_blk_id);
void svm_vmm_pa_ref_dec_local(struct devmm_svm_process *svm_proc, int phy_addr_blk_id);
#ifdef DRV_HOST
int svm_vmm_pa_ref_op_remote(struct devmm_svm_process *svm_proc, const struct svm_vmm_pa_owner_desc *pa_owner_desc,
                             u32 op);
#endif
int svm_vmm_access_pa_query_remote(struct devmm_svm_process *svm_proc,
                                   const struct svm_vmm_pa_owner_desc *pa_owner_desc, u64 offset,
                                   struct svm_pa_seg pa_seg[], u32 *seg_num, u32 query_flag);
void svm_vmm_access_heap_unmap_local(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap);

static inline int svm_vmm_access_query_raw_pa_remote(struct devmm_svm_process *svm_proc,
                                                     const struct svm_vmm_pa_owner_desc *pa_owner_desc, u64 offset,
                                                     struct svm_pa_seg pa_seg[], u32 *seg_num)
{
    return svm_vmm_access_pa_query_remote(svm_proc, pa_owner_desc, offset, pa_seg, seg_num,
                                          SVM_VMM_ACCESS_PA_QUERY_FLAG_RAW_PA);
}

static inline int svm_vmm_access_query_host_bar_remote(struct devmm_svm_process *svm_proc,
                                                       const struct svm_vmm_pa_owner_desc *pa_owner_desc, u64 offset,
                                                       struct svm_pa_seg pa_seg[], u32 *seg_num)
{
    return svm_vmm_access_pa_query_remote(svm_proc, pa_owner_desc, offset, pa_seg, seg_num,
                                          SVM_VMM_ACCESS_PA_QUERY_FLAG_HOST_BAR);
}

#endif /* SVM_VMM_ACCESS_COMM_H */
