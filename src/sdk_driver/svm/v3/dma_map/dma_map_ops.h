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

#ifndef DMA_MAP_OPS_H
#define DMA_MAP_OPS_H

#include "ka_type.h"

#include "svm_addr_desc.h"
#include "dma_map_ctx.h"

struct svm_dma_map_ops {
    int (*do_dma_map)(u32 udevid, struct dma_map_node *map_node, u64 dst_page_size);
    int (*do_dma_unmap)(struct dma_map_node *map_node);
    int (*do_dma_remap)(u32 udevid, struct dma_map_node *map_node, struct svm_global_va *remap_info, u64 start_seg_idx,
                        u64 remap_seg_num);
};

int svm_dma_map_register_ops(const struct svm_dma_map_ops *ops);

#endif
