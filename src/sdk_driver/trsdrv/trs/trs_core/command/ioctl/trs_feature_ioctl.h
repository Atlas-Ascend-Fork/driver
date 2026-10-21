/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef TRS_FEATURE_IOCTL_H
#define TRS_FEATURE_IOCTL_H

#define BITS_PER_U64 64U

enum trs_feature {
    TRS_FEATURE_MAX,
};

#define TRS_FEATURE_BITMAP_U64_NUM ((TRS_FEATURE_MAX + BITS_PER_U64) / BITS_PER_U64)

typedef u64 trs_feat_bitmap_t[TRS_FEATURE_BITMAP_U64_NUM];

static inline void trs_dev_feat_set(trs_feat_bitmap_t bitmap, u32 feat_bit)
{
    bitmap[feat_bit / BITS_PER_U64] |= (1ULL << (feat_bit % BITS_PER_U64));
}

static inline void trs_dev_feat_clear(trs_feat_bitmap_t bitmap, u32 feat_bit)
{
    bitmap[feat_bit / BITS_PER_U64] &= ~(1ULL << (feat_bit % BITS_PER_U64));
}

static inline bool trs_dev_feat_is_support(trs_feat_bitmap_t bitmap, u32 feat_bit)
{
    return (bitmap[feat_bit / BITS_PER_U64] & (1ULL << (feat_bit % BITS_PER_U64))) != 0;
}

#endif /* TRS_FEATURE_IOCTL_H */
