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

/* Should only be included by devmm_adapt.h */

#ifndef SVM_HOST_ADAPT_H
#define SVM_HOST_ADAPT_H

#include <securec.h>

#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"

#include "svm_ioctl.h"

static inline bool devmm_is_normal_node(int nid)
{
    return true;
}

static inline u32 devmm_get_alloc_mask(bool is_compound_page, bool no_retry)
{
    /* arm host can not use __GFP_THISNODE flag */
    u32 alloc_mask = KA_GFP_KERNEL | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT;

    if (no_retry) {
        alloc_mask |= __KA_GFP_NORETRY;
    } else {
        /* Costly order (e.g. order-9 for 2MiB compound pages) without no_retry:
         * kernel does one reclaim+compact then goto nopage.
         * __GFP_RETRY_MAYFAIL adds up to 4 retries with reclaim+compaction,
         * does NOT trigger OOM killer, real OOM still returns -ENOMEM. */
        alloc_mask |= __KA_GFP_RETRY_MAYFAIL;
    }

    if (is_compound_page) {
        alloc_mask |= __KA_GFP_COMP;
    }

    return alloc_mask;
}

static inline int devmm_get_nids(u32 devid, u32 vfid, u32 mem_type, int nids[], u32 *nid_num)
{
    *nid_num = 1;
    nids[0] = ka_mm_numa_mem_id();
    return 0;
}

static inline u32 devmm_get_hugetlb_alloc_flag(ka_page_t *hpage)
{
    return 0;
}

static inline ka_page_t *_devmm_alloc_hpage(int nid, u32 flag)
{
    return NULL;
}

static inline u64 devmm_get_alloc_threshold(u32 devid, u32 vfid)
{
    return 0;
}

static inline u64 devmm_get_nid_free_size(int nid)
{
    return 0;
}

static inline u32 devmm_is_support_update_numa_order(void)
{
    return 0;
}

static inline u64 devmm_get_double_pgtable_offset(void)
{
    return 0;
}

static inline bool devmm_is_support_double_pgtable(void)
{
    return false;
}

static inline u64 devmm_double_pgtable_get_offset_addr(u64 normal_va)
{
    return 0;
}

static inline bool devmm_is_support_giant_page_feature(void)
{
    return false;
}
#endif /* SVM_HOST_ADAPT_H */
