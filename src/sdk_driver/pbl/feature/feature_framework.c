/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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

#include "securec.h"
#include "ka_type.h"
#include "pbl/pbl_urd.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_module_feature_decl.h"
#include "ascend_dev_num.h"
#include "feature_log.h"

const struct module_chip_features *module_get_chip_features(const struct module_chip_features *const *table,
                                                            u32 table_size, u32 chip_type)
{
    if (table == NULL || chip_type >= table_size) {
        return NULL;
    }
    return table[chip_type];
}

static const struct module_chip_features *module_get_verified_features(const struct module_feature_descriptor *desc,
                                                                       u32 dev_id)
{
    u32 chip_type;

    if (desc == NULL || dev_id >= ASCEND_DEV_MAX_NUM) {
        feature_err("Invalid parameter. (desc_is_null=%d; dev_id=%u)\n", (desc == NULL), dev_id);
        return NULL;
    }

    chip_type = desc->dev_chip_map[dev_id];
    if (chip_type == HISI_CHIP_UNKNOWN) {
        return NULL;
    }
    return module_get_chip_features(desc->chip_table, desc->chip_table_size, chip_type);
}

u32 module_get_feature_bitmap(const struct module_feature_descriptor *desc, u32 dev_id, u64 *out, u32 out_words)
{
    int ret = 0;
    u32 fill_words, copy_bytes;
    const struct module_chip_features *feat = NULL;

    if (desc == NULL || out == NULL || out_words == 0) {
        feature_err("Invalid parameter. (desc_is_null=%d; out_is_null=%d; out_words=%u)\n", (desc == NULL),
                    (out == NULL), out_words);
        return 0;
    }

    (void)memset_s(out, out_words * sizeof(u64), 0, out_words * sizeof(u64));

    feat = module_get_verified_features(desc, dev_id);
    if (feat == NULL || feat->bitmap == NULL || feat->bitmap_words == 0) {
        return 0;
    }

    fill_words = out_words;
    if (fill_words > desc->bitmap_len) {
        fill_words = desc->bitmap_len;
    }
    if (fill_words > feat->bitmap_words) {
        fill_words = feat->bitmap_words;
    }

    copy_bytes = fill_words * (u32)sizeof(u64);
    ret = memcpy_s(out, out_words * (u32)sizeof(u64), feat->bitmap, copy_bytes);
    if (ret != 0) {
        feature_err("Failed to copy bitmap. (ret=%d; copy_bytes=%u)\n", ret, copy_bytes);
        return 0;
    }
    return fill_words;
}

u32 module_get_feature_bitmap_batch(const struct module_feature_descriptor *desc, u32 dev_num, u64 *out,
                                    u32 out_words_per_dev)
{
    u32 dev_id, per_dev_fill;
    u64 *dev_bitmap = NULL;

    if (desc == NULL || out == NULL || dev_num > ASCEND_DEV_MAX_NUM || out_words_per_dev == 0) {
        feature_err("Invalid parameter. (desc_is_null=%d; out_is_null=%d; dev_num=%d; out_words_per_dev=%u)\n",
                    (desc == NULL), (out == NULL), dev_num, out_words_per_dev);
        return 0;
    }

    per_dev_fill = out_words_per_dev;
    if (per_dev_fill > desc->bitmap_len) {
        per_dev_fill = desc->bitmap_len;
    }

    for (dev_id = 0; dev_id < dev_num; dev_id++) {
        dev_bitmap = out + (dev_id * out_words_per_dev);
        (void)module_get_feature_bitmap(desc, dev_id, dev_bitmap, out_words_per_dev);
    }
    return per_dev_fill;
}

bool module_has_feature(const struct module_feature_descriptor *desc, u32 dev_id, u32 feature_id)
{
    u32 word_idx;
    const struct module_chip_features *feat = module_get_verified_features(desc, dev_id);

    if (feat == NULL || feat->bitmap == NULL) {
        return false;
    }

    word_idx = feature_id / 64U;
    if (word_idx >= feat->bitmap_words) {
        return false;
    }

    return (feat->bitmap[word_idx] & (1ULL << (feature_id % 64U))) != 0;
}

KA_EXPORT_SYMBOL(module_get_chip_features);
KA_EXPORT_SYMBOL(module_get_feature_bitmap);
KA_EXPORT_SYMBOL(module_get_feature_bitmap_batch);
KA_EXPORT_SYMBOL(module_has_feature);
