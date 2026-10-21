/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "devmng_common.h"
#include "devdrv_shm_comm.h"
#include "devdrv_flush_cache.h"
#ifndef DEVDRV_UT
void mb(void)
{
    asm volatile("dsb sy" : : : "memory");
}

void drvFlushCache(uint64_t base, uint32_t len)
{
    uint32_t i;
    uint32_t l = len / 64;

    if (base == 0) {
        DEVDRV_DRV_ERR("Invalid para, addr base is 0.\n");
        return;
    }

    DRV_ATRACE_BEGIN(drvFlushCache);
    asm volatile("dsb st" : : : "memory");
    for (i = 0; i < l; i++) {
        asm volatile("DC CIVAC ,%x0" ::"r"(base + i * 64));
        mb();
    }
    asm volatile("dsb st" : : : "memory");
    DRV_ATRACE_END();
}

void devdrv_flush_cache(uint64_t base, uint32_t len)
{
    uint64_t addr_loop, addr_end;
    DRV_ATRACE_BEGIN(devdrv_flush_cache);
    addr_loop = base & (~DEVDRV_CACHELINE_MASK);
    addr_end = (base + len) & (~DEVDRV_CACHELINE_MASK);

    asm volatile("dsb st" : : : "memory");
    for (; addr_loop < addr_end;) {
        asm volatile("DC CIVAC ,%x0" ::"r"(addr_loop));
        mb();
        addr_loop += DEVDRV_CACHELINE_SIZE;
    }
    asm volatile("dsb st" : : : "memory");
    DRV_ATRACE_END();
}
#else
void drvFlushCache2(uint64_t base, uint32_t len)
{}
#endif
