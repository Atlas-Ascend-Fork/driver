/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef SMM_IOCTL_H
#define SMM_IOCTL_H

#include "svm_pub.h"
#include "svm_addr_desc.h"

struct svm_smm_map_para {
    u64 dst_va;                    /* input */
    u64 dst_size;                  /* input */
    u64 flag;                      /* input */
    struct svm_global_va src_info; /* input */
    u64 src_va;                    /* We save src_info->va because src_info->va will be updated to uba_info. */
};

struct svm_smm_unmap_para {
    u64 dst_va;                    /* input */
    u64 dst_size;                  /* input */
    u64 flag;                      /* input */
    struct svm_global_va src_info; /* input */
    u64 rsv;                       /* reserve */
};

#endif
