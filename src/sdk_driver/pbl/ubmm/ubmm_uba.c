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
#include "ka_base_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_kernel_def_pub.h"

#include "pbl_feature_loader.h"
#include "pbl_soc_res.h"
#include "ubmm_log.h"

struct ubmm_genpool_data {
    unsigned long base;
    unsigned long size;
};

struct ubmm_sub_pool {
    ka_gen_pool_t *uba_pool;
    struct ubmm_genpool_data pool_data;
};

struct ubmm_gen_pool {
    struct ubmm_sub_pool npage_align_pool;
    struct ubmm_sub_pool hpage_align_pool;
    u64 uba_base;
    u64 uba_size;
};

#define UBMM_BYTES_PER_KB 1024ULL
#define UBMM_BYTES_PER_MB (1024ULL * UBMM_BYTES_PER_KB)
#define UBMM_BYTES_PER_GB (1024ULL * UBMM_BYTES_PER_MB)
#define UBMM_UBA_NPAGE_ALIGN_POOL_SIZE (9ULL * UBMM_BYTES_PER_GB)

static struct ubmm_gen_pool g_uba_pool;

static int ubmm_create_sub_pool(struct ubmm_sub_pool *sub_pool, u64 uba_base, u64 uba_size, u64 align)
{
    ka_gen_pool_t *pool = NULL;
    int min_alloc_order = (align == KA_MM_PAGE_SIZE) ? KA_MM_PAGE_SHIFT : KA_MM_HPAGE_SHIFT; /* Hardware constraints. */
    int ret;

    pool = ka_base_gen_pool_create(min_alloc_order, KA_NUMA_NO_NODE);
    if (pool == NULL) {
        ubmm_err("Create gen pool failed. (uba_base=0x%llx; uba_size=0x%llx)\n", uba_base, uba_size);
        return -EFAULT;
    }

    sub_pool->pool_data.base = uba_base;
    sub_pool->pool_data.size = uba_size;
    /* Under the current uba configuration, there may be overlapping uba which can lead to performance degradation
     * because chip constraints. Therefore, we will try our best to start from uba_base each time.
     */
    ka_base_gen_pool_set_algo(pool, ka_base_gen_pool_first_fit, NULL);

    ret = ka_base_gen_pool_add(pool, (unsigned long)uba_base, (unsigned long)uba_size, KA_NUMA_NO_NODE);
    if (ret != 0) {
        ka_base_gen_pool_destroy(pool);
        ubmm_err("Add to gen pool failed. (uba_base=0x%llx; uba_size=0x%llx)\n", uba_base, uba_size);
        return ret;
    }

    sub_pool->uba_pool = pool;
    return 0;
}

static void ubmm_destroy_sub_pool(struct ubmm_sub_pool *sub_pool)
{
    size_t avail, total;

    if (sub_pool->uba_pool == NULL) {
        return;
    }

    avail = ka_base_gen_pool_avail(sub_pool->uba_pool);
    total = ka_base_gen_pool_size(sub_pool->uba_pool);
    if (avail != total) {
        ubmm_warn("Leak uba mem in sub pool. (pool_base=0x%lx; avail_size=0x%lx; total_size=0x%lx)\n",
                  sub_pool->pool_data.base, avail, total);
    }

    ka_base_gen_pool_destroy(sub_pool->uba_pool);
    sub_pool->uba_pool = NULL;
    sub_pool->pool_data.base = 0;
    sub_pool->pool_data.size = 0;
}

static bool ubmm_uba_is_in_sub_pool_range(const struct ubmm_sub_pool *sub_pool, u64 uba, u64 size)
{
    u64 start = uba;
    u64 end = uba + size;
    u64 base = (u64)sub_pool->pool_data.base;
    u64 pool_end = base + (u64)sub_pool->pool_data.size;

    if (sub_pool->uba_pool == NULL) {
        return false;
    }

    return (start >= base) && (end > start) && (end <= pool_end);
}

static bool ubmm_uba_pool_is_created(const struct ubmm_gen_pool *uba_pool)
{
    return (uba_pool->npage_align_pool.uba_pool != NULL) || (uba_pool->hpage_align_pool.uba_pool != NULL);
}

static int _ubmm_uba_pool_create(u64 uba_base, u64 uba_size)
{
    u64 npage_align_pool_size;
    u64 hpage_align_pool_base;
    u64 hpage_align_pool_size;
    int ret;

    npage_align_pool_size = (uba_size < UBMM_UBA_NPAGE_ALIGN_POOL_SIZE) ? uba_size : UBMM_UBA_NPAGE_ALIGN_POOL_SIZE;
    ret = ubmm_create_sub_pool(&g_uba_pool.npage_align_pool, uba_base, npage_align_pool_size, KA_MM_PAGE_SIZE);
    if (ret != 0) {
        return ret;
    }

    if (uba_size > UBMM_UBA_NPAGE_ALIGN_POOL_SIZE) {
        hpage_align_pool_base = uba_base + UBMM_UBA_NPAGE_ALIGN_POOL_SIZE;
        hpage_align_pool_size = uba_size - UBMM_UBA_NPAGE_ALIGN_POOL_SIZE;
        ret = ubmm_create_sub_pool(&g_uba_pool.hpage_align_pool, hpage_align_pool_base, hpage_align_pool_size,
                                   KA_HPAGE_SIZE);
        if (ret != 0) {
            ubmm_destroy_sub_pool(&g_uba_pool.npage_align_pool);
            return ret;
        }
    }

    g_uba_pool.uba_base = uba_base;
    g_uba_pool.uba_size = uba_size;
    return 0;
}

int ubmm_uba_pool_create(u64 uba_base, u64 uba_size)
{
    static KA_TASK_DEFINE_MUTEX(mutex);
    int ret = 0;

    ka_task_mutex_lock(&mutex);
    if (!ubmm_uba_pool_is_created(&g_uba_pool)) {
        ret = _ubmm_uba_pool_create(uba_base, uba_size);
    }
    ka_task_mutex_unlock(&mutex);

    return ret;
}

void ubmm_uba_pool_destroy(void)
{
    if (!ubmm_uba_pool_is_created(&g_uba_pool)) {
        return;
    }

    ubmm_destroy_sub_pool(&g_uba_pool.hpage_align_pool);
    ubmm_destroy_sub_pool(&g_uba_pool.npage_align_pool);
    g_uba_pool.uba_base = 0;
    g_uba_pool.uba_size = 0;
}

static int ubmm_alloc_from_sub_pool(struct ubmm_sub_pool *sub_pool, u64 size, u64 *uba)
{
    if (sub_pool->uba_pool == NULL) {
        return -ENODEV;
    }

    if (size > sub_pool->pool_data.size) {
        return -EINVAL;
    }

    *uba = (u64)ka_base_gen_pool_alloc(sub_pool->uba_pool, size);
    if (*uba == 0) {
        return -ENOMEM;
    }

    return 0;
}

int ubmm_alloc_uba(u64 size, u64 prefer_align, u64 *uba)
{
    int ret;

    if (!ubmm_uba_pool_is_created(&g_uba_pool)) {
        return -ENODEV;
    }

    if ((size == 0) || (uba == NULL) || (size > g_uba_pool.uba_size)) {
        ubmm_err("Invalid value. (size=0x%llx; align=0x%llx)\n", size, prefer_align);
        return -EINVAL;
    }

    if (prefer_align == KA_MM_PAGE_SIZE) {
        ret = ubmm_alloc_from_sub_pool(&g_uba_pool.npage_align_pool, size, uba);
        if (ret != 0) {
            ret = ubmm_alloc_from_sub_pool(&g_uba_pool.hpage_align_pool, size, uba);
        }
    } else {
        ret = ubmm_alloc_from_sub_pool(&g_uba_pool.hpage_align_pool, size, uba);
        if (ret != 0) {
            ret = ubmm_alloc_from_sub_pool(&g_uba_pool.npage_align_pool, size, uba);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(ubmm_alloc_uba);

int ubmm_free_uba(u64 uba, u64 size)
{
    u64 start = uba;
    u64 end = uba + size;

    if (!ubmm_uba_pool_is_created(&g_uba_pool)) {
        return -ENODEV;
    }

    if ((start < g_uba_pool.uba_base) || (end > g_uba_pool.uba_base + g_uba_pool.uba_size) || (end <= start)) {
        ubmm_err("Invalid value. (uba=0x%llx; size=0x%llx)\n", uba, size);
        return -EINVAL;
    }

    if (ubmm_uba_is_in_sub_pool_range(&g_uba_pool.hpage_align_pool, uba, size)) {
        ka_base_gen_pool_free(g_uba_pool.hpage_align_pool.uba_pool, (unsigned long)uba, (unsigned long)size);
        return 0;
    }

    if (ubmm_uba_is_in_sub_pool_range(&g_uba_pool.npage_align_pool, uba, size)) {
        ka_base_gen_pool_free(g_uba_pool.npage_align_pool.uba_pool, (unsigned long)uba, (unsigned long)size);
        return 0;
    }

    ubmm_err("Invalid uba pool. (uba=0x%llx; size=0x%llx)\n", uba, size);
    return -EINVAL;
}
KA_EXPORT_SYMBOL_GPL(ubmm_free_uba);

int ubmm_get_uba_pool(u64 *uba_base, u64 *total_size, u64 *avail_size)
{
    if (!ubmm_uba_pool_is_created(&g_uba_pool)) {
        return -ENODEV;
    }

    *uba_base = g_uba_pool.uba_base;
    *total_size = g_uba_pool.uba_size;
    *avail_size = (g_uba_pool.npage_align_pool.uba_pool != NULL) ?
                      (u64)ka_base_gen_pool_avail(g_uba_pool.npage_align_pool.uba_pool) :
                      0;
    *avail_size += (g_uba_pool.hpage_align_pool.uba_pool != NULL) ?
                       (u64)ka_base_gen_pool_avail(g_uba_pool.hpage_align_pool.uba_pool) :
                       0;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(ubmm_get_uba_pool);

int ubmm_query_mem_id(u32 udevid, u32 *mem_id)
{
    u64 value = 0;
    int ret = 0;

    if (mem_id == NULL) {
        ubmm_err("Invalid param, mem_id is NULL. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ret = soc_resmng_dev_get_key_value(udevid, "UB_MEM_ID", &value);
    if (ret != 0) {
        ubmm_err("Get mem_id failed. (udevid=%u)\n", udevid);
        return ret;
    }
    *mem_id = (u32)value;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(ubmm_query_mem_id);

void ubmm_uba_uninit(void)
{
    ubmm_uba_pool_destroy();
}
DECLAER_FEATURE_AUTO_UNINIT(ubmm_uba_uninit, FEATURE_LOADER_STAGE_2);
