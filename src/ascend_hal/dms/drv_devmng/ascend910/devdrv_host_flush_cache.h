/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEVDRV_HOST_FLUSH_CACHE_H
#define DEVDRV_HOST_FLUSH_CACHE_H

#include <stdint.h>

#define rmb() dsb(ld)  /* read fence */
#define wmb() dsb(st)  /* write fence */
#define rwmb() dsb(st) /* write fence */
#define dsb(opt)

void mb(void);
void devdrv_flush_cache(uint64_t base, uint32_t len);
#endif
