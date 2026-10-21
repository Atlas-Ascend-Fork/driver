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

#ifndef TRSBASE_FEATURE_H
#define TRSBASE_FEATURE_H

#include "ka_type.h"

#define BITS_PER_U64 64U

enum trsbase_feature {
    TRSBASE_FEATURE_MAX,
};

#define TRSBASE_FEATURE_BITMAP_U64_NUM ((TRSBASE_FEATURE_MAX + BITS_PER_U64) / BITS_PER_U64)

typedef u64 trsbase_feat_bitmap_t[TRSBASE_FEATURE_BITMAP_U64_NUM];

static inline void trsbase_dev_feat_set(trsbase_feat_bitmap_t map, u32 feat_bit)
{
    map[feat_bit / BITS_PER_U64] |= (1ULL << (feat_bit % BITS_PER_U64));
}

static inline void trsbase_dev_feat_clear(trsbase_feat_bitmap_t map, u32 feat_bit)
{
    map[feat_bit / BITS_PER_U64] &= ~(1ULL << (feat_bit % BITS_PER_U64));
}

static inline bool trsbase_dev_feat_is_support(trsbase_feat_bitmap_t map, u32 feat_bit)
{
    return (map[feat_bit / BITS_PER_U64] & (1ULL << (feat_bit % BITS_PER_U64))) != 0;
}

int trsbase_feature_init(u32 devid);
void trsbase_feature_uninit(u32 devid);

#endif /* TRSBASE_FEATURE_H */
