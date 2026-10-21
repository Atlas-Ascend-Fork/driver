/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef SMM_UK_MSG_H
#define SMM_UK_MSG_H

#include "svm_pub.h"
#include "svm_addr_desc.h"

struct svm_uk_msg_head {
    u64 k2u_sync_handle;
    u64 rsv;
};

struct svm_smm_map_msg {
    struct svm_uk_msg_head head;
    u64 dst_va;
    u64 dst_size;
    u32 dst_task_type;
    u32 flag;
    struct svm_global_va src_info;
    u64 src_va;
};

struct svm_smm_unmap_msg {
    struct svm_uk_msg_head head;
    u64 dst_va;
    u64 dst_size;
    u32 dst_task_type;
    u32 flag;
    struct svm_global_va src_info;
};

#endif
