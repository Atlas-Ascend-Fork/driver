/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef SHARE_REPAIR_IOCTL_H
#define SHARE_REPAIR_IOCTL_H

#include "svm_addr_desc.h"

struct svm_share_repair_para {
    struct svm_global_va src_va;
    u64 rsv;
};

#endif
