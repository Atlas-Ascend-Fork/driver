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

#ifndef DMA_MAP_KERNEL_H
#define DMA_MAP_KERNEL_H

#include "ka_type.h"

#include "svm_pub.h"
#include "svm_addr_desc.h"
#include "dma_map_flag.h"

struct svm_dma_addr_info {
    bool is_write;
    bool is_local_mem;

    u64 dma_addr_seg_num;
    u64 first_seg_offset;
    u64 last_seg_len;
    struct svm_dma_addr_seg *seg;
};

/*
    may be not in current context when recycle, so add tgid para
    dst_page_size is the page granularity of dst_va

 * if handle is not null when dma map, must use svm_dma_addr_put_by_handle to unmap
*/
int svm_dma_map_addr(u32 udevid, int tgid, struct svm_global_va *dst_va, u64 dst_page_size, u32 flag, void **handle);
int svm_dma_unmap_addr(u32 udevid, int tgid, struct svm_global_va *dst_va);
/* The remap range must exactly cover one or more complete DMA segments. */
int svm_dma_remap_addr(u32 udevid, int tgid, struct svm_global_va *dst_va);
void svm_dma_addr_put_by_handle(void *handle);
int svm_dma_addr_query_by_handle(void *handle, struct svm_global_va *dst_va, struct svm_dma_addr_info *dma_info);

int svm_dma_addr_get(u32 udevid, int tgid, struct svm_global_va *dst_va, struct svm_dma_addr_info *dma_info,
                     void **handle);
void svm_dma_addr_put(u32 udevid, int tgid, struct svm_global_va *dst_va);

void svm_dma_addr_sync_for_device(u32 udevid, const struct svm_dma_addr_info *dma_info);
void svm_dma_addr_sync_for_cpu(u32 udevid, const struct svm_dma_addr_info *dma_info);

static inline int svm_dma_addr_query(u32 udevid, int tgid, struct svm_global_va *dst_va,
                                     struct svm_dma_addr_info *dma_info)
{
    int ret = svm_dma_addr_get(udevid, tgid, dst_va, dma_info, NULL);
    if (ret == 0) {
        svm_dma_addr_put(udevid, tgid, dst_va);
    }
    return ret;
}

#endif
