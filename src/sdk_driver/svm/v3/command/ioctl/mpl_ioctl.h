/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef MPL_IOCTL_H
#define MPL_IOCTL_H

#include "svm_pub.h"

#define POPULATE_MAX_VA 4
struct va_array {
    u32 count;
    u64 array[POPULATE_MAX_VA]; /* PA mapped to multiple VAs */
};

struct svm_mpl_populate_para {
    u64 va;   /* input */
    u64 size; /* input */
    u32 flag; /* input */
    u64 rsv;  /* reserve */
    struct va_array va_batch;
};

struct svm_mpl_depopulate_para {
    u64 va;   /* input */
    u64 size; /* input */
    u32 flag; /* input */
    u32 rsv;  /* reserve */
    struct va_array va_batch;
};

#endif
