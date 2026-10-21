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
#ifndef SVM_GFP_H
#define SVM_GFP_H

#include "ka_memory_pub.h"
#include "ka_common_pub.h"

#include "pbl_runenv_config.h"

#include "svm_pub.h"
#include "svm_addr_desc.h"

#define _svm_alloc_pages_node ka_mm_alloc_pages_node
#define _svm_free_pages __ka_mm_free_pages

static inline ka_gfp_t svm_get_alloc_page_mask(bool is_comp, bool no_retry)
{
    ka_gfp_t gfp_mask;
    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        /* arm host can not use __GFP_THISNODE flag */
        gfp_mask = (KA_GFP_KERNEL | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT);
    } else {
        gfp_mask = (KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_THISNODE | KA_GFP_HIGHUSER_MOVABLE | __KA_GFP_NOWARN |
                    __KA_GFP_ACCOUNT);
    }

    if (no_retry) {
        gfp_mask |= __KA_GFP_NORETRY;
    }

    if (is_comp) {
        gfp_mask |= __KA_GFP_COMP;
    }

    return gfp_mask;
}

#define SVM_GFP_FLAG_CONTINUOUS (1U << 0)
#define SVM_GFP_FLAG_CLEAR (1U << 1)
#define SVM_GFP_FLAG_P2P (1U << 2)
#define SVM_GFP_FLAG_FIXED_NUMA (1U << 3)
#define SVM_GFP_FLAG_CONTIGUOUS_PER_2M (1U << 4)

/* numa id: bit24~31 */
#define SVM_GFP_FLAG_NUMA_ID_BIT 24U
#define SVM_GFP_FLAG_NUMA_ID_WIDTH 8U
#define SVM_GFP_FLAG_NUMA_ID_MASK ((1U << SVM_GFP_FLAG_NUMA_ID_WIDTH) - 1)

static inline void gfp_flag_set_numa_id(u32 *flag, u32 numa_id)
{
    *flag |= ((numa_id & SVM_GFP_FLAG_NUMA_ID_MASK) << SVM_GFP_FLAG_NUMA_ID_BIT);
}

static inline u32 gfp_flag_get_numa_id(u32 flag)
{
    return ((flag >> SVM_GFP_FLAG_NUMA_ID_BIT) & SVM_GFP_FLAG_NUMA_ID_MASK);
}

#define SVM_GPAGE_SHIFT PUD_SHIFT
#define SVM_GPAGE_SIZE (1ULL << SVM_GPAGE_SHIFT)

static inline bool svm_page_is_continuous(u32 flag)
{
    return ((flag & SVM_GFP_FLAG_CONTINUOUS) != 0);
}

static inline bool svm_page_is_continuous_per_2M(u32 flag)
{
    return ((flag & SVM_GFP_FLAG_CONTIGUOUS_PER_2M) != 0);
}

static inline bool svm_page_is_need_clear(u32 flag)
{
    return ((flag & SVM_GFP_FLAG_CLEAR) != 0);
}

enum svm_page_granularity {
    SVM_PAGE_GRAN_NORMAL = 0u,
    SVM_PAGE_GRAN_HUGE,
    SVM_PAGE_GRAN_GIANT,
    SVM_PAGE_GRAN_MAX,
};

static inline u64 svm_page_gran_to_page_size(enum svm_page_granularity pg_gran)
{
    switch (pg_gran) {
        case SVM_PAGE_GRAN_NORMAL:
            return KA_MM_PAGE_SIZE;
        case SVM_PAGE_GRAN_HUGE:
            return KA_HPAGE_SIZE;
        case SVM_PAGE_GRAN_GIANT:
            return SVM_GPAGE_SIZE;
        default:
            return 0;
    }
}

static inline enum svm_page_granularity svm_page_size_to_page_gran(u64 page_size)
{
    switch (page_size) {
        case KA_MM_PAGE_SIZE:
            return SVM_PAGE_GRAN_NORMAL;
        case KA_HPAGE_SIZE:
            return SVM_PAGE_GRAN_HUGE;
        case SVM_GPAGE_SIZE:
            return SVM_PAGE_GRAN_GIANT;
        default:
            return SVM_PAGE_GRAN_MAX;
    }
}

int svm_alloc_pages(u32 udevid, enum svm_page_granularity gran, ka_page_t **pages, u64 pg_num, u32 flag);
void svm_free_pages(enum svm_page_granularity gran, ka_page_t **pages, u64 pg_num, u32 flag);

/* page ops */
struct svm_page_ops {
    /* flag is SVM_GFP_FLAG_ */
    int (*alloc)(int nids[], u32 nid_num, ka_page_t **pages, u64 page_num, u32 flag);
    void (*free)(ka_page_t **pages, u64 page_num, u32 flag);
};

typedef void (*svm_print_nodes_info_ops)(void);

void svm_register_page_ops(enum svm_page_granularity gran, const struct svm_page_ops *ops);
void svm_register_numa_id_handle(int (*handle)(u32 udevid, u32 memtype, u32 sub_memtype, int nids[], int *out_num));
void svm_register_print_nodes_info_ops(svm_print_nodes_info_ops print_ops);

void svm_clear_single_page(ka_page_t *page, u64 page_size);
void svm_page_ref_dec(ka_page_t *pg, void (*clear_func)(ka_page_t *pg), void (*dec_func)(ka_page_t *pg));

u64 svm_make_pa_continues(struct svm_pa_seg *pa_seg, u64 seg_num);
u64 svm_get_pa_size(struct svm_pa_seg *pa_seg, u64 seg_num);
u64 svm_get_page_size_by_pa_seg(u64 va, u64 size, struct svm_pa_seg *pa_seg, u64 seg_num);
/* This func is time-consuming operation, may cause performance problem. */
bool svm_pa_is_local_mem(u64 pa);
u64 svm_get_max_page_size_by_pa_seg(struct svm_pa_seg *pa_seg, u64 seg_num);
u64 svm_get_same_page_size_seg_num_by_pa_seg(u64 va, struct svm_pa_seg *pa_seg, u64 seg_num, u64 *seg_sum_size,
                                             u64 *page_size);

#endif
