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

#ifndef DMA_MAP_MSG_SEND_H
#define DMA_MAP_MSG_SEND_H

#include "svm_addr_desc.h"

int svm_dma_map_addr_query_remote(u32 udevid, const struct svm_global_va *src_info, struct svm_pa_seg *pa_seg,
                                  u64 max_seg_num, u64 *seg_num);

#endif
