/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef DBI_IOCTL_H
#define DBI_IOCTL_H

#include "dbi_def.h"

struct svm_dbi_query_para {
    struct svm_device_basic_info dbi; /* output */
};

struct svm_dbi_cap_query_para {
    u32 devid;
    u32 bitmap_size;
    svm_dev_cap_t cap_bitmap;
};

#endif
