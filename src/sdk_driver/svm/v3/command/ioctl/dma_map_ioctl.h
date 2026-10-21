/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef DMA_MAP_IOCTL_H
#define DMA_MAP_IOCTL_H

#include "ascend_hal_define.h"

#include "svm_pub.h"
#include "svm_addr_desc.h"

struct svm_dma_map_para {
    struct svm_dst_va dst_va; /* input */
    u32 flag;
    u64 rsv; /* reserve */
};

struct svm_dma_unmap_para {
    struct svm_dst_va dst_va; /* input */
    u64 rsv;                  /* reserve */
};

#endif
