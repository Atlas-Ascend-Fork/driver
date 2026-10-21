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

#ifndef SVM_VMM_ACCESS_MSG_H
#define SVM_VMM_ACCESS_MSG_H

#include "svm_kernel_msg.h"
#include "svm_pa_seg.h"

struct svm_vmm_pa_owner_desc {
    int host_pid;
    u32 udevid; /* Host view */
    int phy_addr_blk_id;
};

static inline void svm_vmm_pa_owner_desc_pack(int host_pid, u32 udevid, int phy_addr_blk_id,
                                              struct svm_vmm_pa_owner_desc *desc)
{
    desc->host_pid = host_pid;
    desc->udevid = udevid;
    desc->phy_addr_blk_id = phy_addr_blk_id;
}

struct svm_vmm_access_pa_query_msg {
    struct devmm_chan_msg_head head;
    struct svm_vmm_pa_owner_desc pa_owner_desc;
    u32 query_flag;
    u64 offset;
    struct svm_pa_seg pa_seg[];
};

struct svm_vmm_access_va_map_msg {
    struct devmm_chan_msg_head head;
    u64 va;
    u64 size;
    u32 access_type;
    struct svm_pa_seg pa_seg[];
};

struct svm_vmm_access_va_unmap_msg {
    struct devmm_chan_msg_head head;
    u64 va;
    u64 size;
    u64 reserved;
};

struct svm_vmm_set_d2d_access_msg {
    struct devmm_chan_msg_head head;
    u64 va;
    u64 size;
    struct svm_vmm_pa_owner_desc pa_owner_desc;
    u32 access_type;
    u64 pa_offset;
    u64 pa_pg_size;
    u64 reserved;
};

enum svm_vmm_pa_ref_op {
    SVM_VMM_PA_REF_OP_DEC = 0,
    SVM_VMM_PA_REF_OP_INC
};

struct svm_vmm_pa_ref_op_msg {
    struct devmm_chan_msg_head head;
    struct svm_vmm_pa_owner_desc pa_owner_desc;
    u32 op;
};

enum svm_vmm_vmma_ref_op {
    SVM_VMM_VMMA_REF_OP_PUT = 0,
    SVM_VMM_VMMA_REF_OP_GET
};

struct svm_vmm_vmma_ref_op_msg {
    struct devmm_chan_msg_head head;
    u64 va;
    u64 size;
    u32 op;
    u64 rsv;
};

#endif /* SVM_VMM_ACCESS_MSG_H */
