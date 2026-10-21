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

#ifndef DMA_MAP_CORE_H
#define DMA_MAP_CORE_H

#include "dma_map_ctx.h"
#include "ka_common_pub.h"

void dma_map_addr_show(struct dma_map_ctx *ctx, ka_seq_file_t *seq);
void dma_map_addr_recycle(struct dma_map_ctx *ctx);
int svm_dma_map_get_remap_seg_range(struct dma_map_node *map_node, u64 remap_va, u64 remap_size, u64 *start_seg_idx,
                                    u64 *remap_seg_num);

int svm_dma_map_core_init(void);

#endif
