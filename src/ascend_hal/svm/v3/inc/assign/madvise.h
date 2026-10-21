/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MADVISE_H
#define MADVISE_H

#include "svm_pub.h"
#include "madvise_flag.h"

/*
    MADVISE: memory advise
*/

int svm_madvise(u32 devid, u64 va, u64 size, u32 flag);
/* Returns saved access for a HOST_RW_DEV_RO allocation, using its aligned size. */
u32 svm_madvise_get_access(u32 devid, u64 va, u64 size);
void svm_madvise_recycle(u32 devid);

#endif
