/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NORMAL_MALLOC_H
#define NORMAL_MALLOC_H

#include "svm_pub.h"

#define SVM_NORMAL_MALLOC_FLAG_VA_ONLY (1U << 0U)
#define SVM_NORMAL_MALLOC_FLAG_VA_DVPP (1U << 1U)
#define SVM_NORMAL_MALLOC_FLAG_DEV_CP_ONLY (1U << 2U)
#define SVM_NORMAL_MALLOC_FLAG_SPACIFIED_VA (1U << 3U)
#define SVM_NORMAL_MALLOC_FLAG_VA_WITH_MASTER (1U << 4U)
#define SVM_NORMAL_MALLOC_FLAG_MASTER_UVA (1U << 5U)
#define SVM_NORMAL_MALLOC_FLAG_FIXED_NUMA (1U << 6U)
#define SVM_NORMAL_MALLOC_FLAG_POPULATE_ONLY (1U << 7U)

#define SVM_NORMAL_MALLOC_FLAG_PA_HPAGE (1U << 10U)
#define SVM_NORMAL_MALLOC_FLAG_PA_GPAGE (1U << 11U)
#define SVM_NORMAL_MALLOC_FLAG_PA_CONTINUOUS (1U << 12U)
#define SVM_NORMAL_MALLOC_FLAG_PA_P2P (1U << 13U)
#define SVM_NORMAL_MALLOC_FLAG_PG_NC (1U << 14U)
#define SVM_NORMAL_MALLOC_FLAG_PG_RDONLY (1U << 15U)
#define SVM_NORMAL_MALLOC_FLAG_VA_WITHOUT_POPULATE (1U << 16U)
#define SVM_NORMAL_MALLOC_FLAG_DEV_PRIVATE (1U << 17U)

#define SVM_NORMAL_MALLOC_FLAG_CAP_COPY (1U << 20U)

/* numa id: bit24~31 */
#define SVM_NORMAL_MALLOC_FLAG_NUMA_ID_BIT 24U
#define SVM_NORMAL_MALLOC_FLAG_NUMA_ID_WIDTH 8U
#define SVM_NORMAL_MALLOC_FLAG_NUMA_ID_MASK ((1U << SVM_NORMAL_MALLOC_FLAG_NUMA_ID_WIDTH) - 1)

static inline void normal_malloc_flag_set_numa_id(u32 *flag, u32 numa_id)
{
    *flag |= ((numa_id & SVM_NORMAL_MALLOC_FLAG_NUMA_ID_MASK) << SVM_NORMAL_MALLOC_FLAG_NUMA_ID_BIT);
}

static inline u32 normal_malloc_flag_get_numa_id(u32 flag)
{
    return ((flag >> SVM_NORMAL_MALLOC_FLAG_NUMA_ID_BIT) & SVM_NORMAL_MALLOC_FLAG_NUMA_ID_MASK);
}

int svm_normal_malloc(u32 devid, u32 flag, u64 align, u64 *va, u64 size);
int svm_normal_free(u32 devid, u32 flag, u64 align, u64 va,
                    u64 size); /* return DRV_ERROR_BUSY, means va is pinned by others task. */

/* malloc free ops */
struct svm_normal_ops {
    void (*post_malloc)(u32 devid, u64 va, u64 size, u32 flag);
    void (*pre_free)(u32 devid, u64 va, u64 size, u32 flag);
};

void svm_normal_set_ops(struct svm_normal_ops *ops);
#endif
