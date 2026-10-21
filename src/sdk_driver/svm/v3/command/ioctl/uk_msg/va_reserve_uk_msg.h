/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef VA_RESERVE_UK_MSG_H
#define VA_RESERVE_UK_MSG_H

#include "svm_pub.h"

#define SVM_MMAP_FLAG_PRIVATE (1U << 0U)

#define VA_RESERVE_STATUS_OK 1
#define VA_RESERVE_STATUS_FAIL 2
#define VA_RESERVE_STATUS_FAIL_WITH_SUGGEST 3

/* SVM_VA_RESERVE_EVENT */
struct svm_va_reserve_msg {
    int op; /* 1. mmap, 0. munmap */
    u32 flag;
    u64 va;
    u64 size;
    u64 gap_size;
    int status;
};

#endif
