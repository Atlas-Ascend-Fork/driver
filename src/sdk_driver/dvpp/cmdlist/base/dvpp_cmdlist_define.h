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

#ifndef DVPP_CMDLIST_BASE_DVPP_DEFINE_H
#define DVPP_CMDLIST_BASE_DVPP_DEFINE_H

#define UNUSED(x) (void)(x)

#define DVPP_ALIGN_UP(x, a) ((((x) + ((a) - 1U)) / (a)) * (a))
#define DVPP_ALIGN_DOWN(x, a) (((x) / (a)) * (a))

#define GET_HIGH_ADDR(val) (((val) >> 32) & 0xffffffff)
#define GET_LOW_ADDR(val) ((val) & 0xffffffff)
#define GEN_64BIT_ADDR(high, low) (((uint64_t)(high) << 32) | (uint64_t)(low))

#define DVPP_MIN_BATCH_NUM (1U)
#define DVPP_MAX_BATCH_NUM (16U)
#define DVPP_MIN_NODE_NUM (1U)
#define DVPP_MAX_NODE_NUM (256U)
#define DVPP_DEFAULT_NODE_NUM (4U) // 默认4个及以下走内存池
// args_size + DVPP_DEFAULT_NODE_NUM * DVPP_MAX_TLV_TYPE * sizeof(void*) + dma_info_size,
// 8192 + 4 * 50 * 8 + 72 = 9864, 对齐到10240U
#define DVPP_DEFAULT_MEM_BLK_SIZE (10240U)
#define DVPP_DEFAULT_DMA_MEM_BLK_SIZE (8192U)

typedef enum DVPP_MOD_TYPE {
    DVPP_MOD_TYPE_VPC = 0,
    DVPP_MOD_TYPE_JPEGD = 1,
    DVPP_MOD_TYPE_JPEGE = 2,
    DVPP_MOD_TYPE_NSC = 3,
    DVPP_MOD_TYPE_BUTT
} dvpp_mod_type;

#endif // #ifndef DVPP_CMDLIST_BASE_DVPP_DEFINE_H