/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#include "ascend_hal_error.h"
#include "ascend_hal.h"

#include "svm_log.h"
#include "gen_allocator.h"
#include "cache_pub.h"
#include "svm_dbi.h"
#include "cache_allocator.h"
#include "cache_recycle_seg.h"
#include "cache_malloc.h"

#define CACHE_MALLOC_PEAK_ALLOCED_TIME_INTERVAL_S 1ULL
#define CACHE_SHRINK_DEFAULT_SIZE (128ULL * SVM_BYTES_PER_MB)

struct svm_cache_ops *cache_ops = NULL;

void svm_cache_set_ops(struct svm_cache_ops *ops)
{
    cache_ops = ops;
}

static int cache_ops_post_expand(u32 devid, u64 va, u64 size)
{
    if ((cache_ops != NULL) && (cache_ops->post_expand != NULL)) {
        return cache_ops->post_expand(devid, va, size);
    }
    return 0;
}

static void cache_ops_pre_shrink(u32 devid, u64 va, u64 size)
{
    if ((cache_ops != NULL) && (cache_ops->pre_shrink != NULL)) {
        cache_ops->pre_shrink(devid, va, size);
    }
}

u64 svm_cache_get_stats_idle_size(u32 devid, u32 flag)
{
    struct cache_allocator *ca = cache_get_allocator(devid, flag);
    return (ca != NULL) ? ca->stats.idle : 0;
}

static void cache_strategy_update(struct cache_allocator *ca)
{
    u64 shrink_thres_default, cur_alloced, peak_alloced;

    pthread_rwlock_wrlock(&ca->rwlock);
    shrink_thres_default = ca->strategy.shrink_thres_default;
    cur_alloced = ca->stats.cur_alloced;
    peak_alloced = ca->stats.peak_alloced;
    ca->strategy.shrink_thres_cur = svm_max(svm_min(2ull * cur_alloced, (peak_alloced + cur_alloced) / 2ull),
                                            shrink_thres_default); /* multiple is 2, peak_alloced's weight is 1/2 */
    pthread_rwlock_unlock(&ca->rwlock);
}

static void cache_malloc_stats_update(struct cache_allocator *ca, u64 size)
{
    time_t cur_time = time(NULL);

    pthread_rwlock_wrlock(&ca->rwlock);
    ca->stats.idle -= size;
    ca->stats.cur_alloced += size;
    if (ca->stats.cur_alloced > ca->stats.peak_alloced) {
        ca->stats.peak_alloced = ca->stats.cur_alloced;
        ca->stats.peak_alloced_last_update_time = cur_time;
    }
    if ((u64)(cur_time - ca->stats.peak_alloced_last_update_time) >= CACHE_MALLOC_PEAK_ALLOCED_TIME_INTERVAL_S) {
        u64 tmp_size = ca->stats.peak_alloced -
                       ca->stats.peak_alloced / 8; /* peak bigger than cur 1/8 refresh cache thres */
        if (tmp_size > ca->stats.cur_alloced) {
            ca->stats.peak_alloced = tmp_size;
            ca->stats.peak_alloced_last_update_time = cur_time;
        }
    }
    pthread_rwlock_unlock(&ca->rwlock);
}

static void cache_free_stats_update(struct cache_allocator *ca, u64 size)
{
    time_t cur_time = time(NULL);

    pthread_rwlock_wrlock(&ca->rwlock);
    ca->stats.idle += size;
    ca->stats.cur_alloced -= size;
    if ((u64)(cur_time - ca->stats.peak_alloced_last_update_time) >= CACHE_MALLOC_PEAK_ALLOCED_TIME_INTERVAL_S) {
        ca->stats.peak_alloced = ca->stats.cur_alloced;
    }
    pthread_rwlock_unlock(&ca->rwlock);
}

static void cache_expand_stats_update(struct cache_allocator *ca, u64 size)
{
    pthread_rwlock_wrlock(&ca->rwlock);
    ca->stats.total += size;
    ca->stats.idle += size;
    pthread_rwlock_unlock(&ca->rwlock);
}

static void cache_shrink_stats_update(struct cache_allocator *ca, u64 size)
{
    pthread_rwlock_wrlock(&ca->rwlock);
    ca->stats.total -= size;
    ca->stats.idle -= size;
    pthread_rwlock_unlock(&ca->rwlock);
}

static bool cache_should_shrink(struct cache_allocator *ca)
{
    u64 shrink_thres = svm_min((ca->strategy.shrink_thres_cur * 2),
                               (ca->strategy.shrink_thres_cur + CACHE_SHRINK_DEFAULT_SIZE));

    return (ca->stats.idle > shrink_thres) ? true : false;
}

static bool cache_try_shrink(struct cache_allocator *ca, u64 va)
{
    return cache_should_shrink(ca) ? svm_ga_owner_range_is_idle(ca->ga_inst, va) : false;
}

static int _cache_malloc(struct cache_allocator *ca, u64 *va, u64 size)
{
    int ret;

    ret = svm_ga_alloc(ca->ga_inst, 0, va, size);
    if (ret == DRV_ERROR_NONE) {
        cache_malloc_stats_update(ca, size);
    }
    return ret;
}

static int _cache_free(struct cache_allocator *ca, u64 va, u64 size)
{
    int ret;

    ret = svm_ga_free(ca->ga_inst, va, size);
    if (ret == DRV_ERROR_NONE) {
        cache_free_stats_update(ca, size);
    }

    return ret;
}

static int cache_malloc_raw(u32 devid, u64 align, u64 *start, u64 size, u32 normal_flag)
{
    u64 va = *start;
    int ret;

    ret = svm_normal_malloc(devid, normal_flag, align, &va, size);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = cache_ops_post_expand(devid, va, size);
    if (ret != DRV_ERROR_NONE) {
        (void)svm_normal_free(devid, normal_flag, align, va, size);
        return ret;
    }

    *start = va;
    return DRV_ERROR_NONE;
}

static int cache_expand(struct cache_allocator *ca, u64 size, u64 *start)
{
    u32 normal_flag = cache_flag_to_normal_flag(ca->flag);
    u64 align = ca->strategy.alloc_gran;
    return cache_malloc_raw(ca->devid, align, start, size, normal_flag);
}

static int cache_shrink(struct cache_allocator *ca, u64 start, u64 size)
{
    u32 normal_flag = cache_flag_to_normal_flag(ca->flag);
    u64 align = ca->strategy.alloc_gran;

    cache_ops_pre_shrink(ca->devid, start, size);
    return svm_normal_free(ca->devid, normal_flag, align, start, size);
}

static int cache_expand_once(struct cache_allocator *ca, u64 size)
{
    u64 start = 0;
    int ret;

    ret = cache_expand(ca, size, &start);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_ga_add_range(ca->ga_inst, start, size);
    if (ret != DRV_ERROR_NONE) {
        (void)cache_shrink(ca, start, size);
        return ret;
    }

    cache_expand_stats_update(ca, size);
    return DRV_ERROR_NONE;
}

static int cache_shrink_once(struct cache_allocator *ca, u64 *out_size)
{
    u64 align = ca->strategy.alloc_gran;
    u64 start, size;
    int ret;

    ret = svm_ga_recycle_one_idle_range(ca->ga_inst, &start, &size);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    cache_shrink_stats_update(ca, size);

    ret = cache_shrink(ca, start, size);
    if (ret == DRV_ERROR_BUSY) {
        cache_recycle_add_seg(start, size, align, ca->devid, ca->flag);
    }

    *out_size = (ret == DRV_ERROR_NONE) ? size : 0;
    return ((ret == DRV_ERROR_BUSY) ? DRV_ERROR_NONE : ret);
}

static int cache_malloc(struct cache_allocator *ca, u64 *va, u64 size)
{
    u64 expand_size;
    int ret;

    ret = _cache_malloc(ca, va, size);
    while (ret == DRV_ERROR_OUT_OF_MEMORY) {
        expand_size = svm_align_up(svm_align_up(size, ca->strategy.expand_granularity), ca->strategy.alloc_gran);
        ret = cache_expand_once(ca, expand_size);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }

        /* Might be alloced by other threads, should retry. */
        ret = _cache_malloc(ca, va, size);
    }

    return ret;
}

static int cache_free(struct cache_allocator *ca, u64 va, u64 size)
{
    u64 shrink_size = 0;
    int ret;

    ret = _cache_free(ca, va, size);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    cache_strategy_update(ca);

    if (cache_try_shrink(ca, va)) {
        while (cache_should_shrink(ca)) {
            ret = cache_shrink_once(ca, &shrink_size);
            if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_BUSY)) {
                break;
            }
        }
    }

    return DRV_ERROR_NONE;
}

bool svm_cache_is_support(u32 devid, u32 flag, u64 align, u32 size)
{
    struct cache_allocator *ca = NULL;

    ca = cache_get_allocator(devid, flag);
    return (((ca != NULL) && (size <= ca->strategy.alloc_thres) && (align == ca->strategy.alloc_gran)) ? true : false);
}

int svm_cache_malloc(u32 devid, u32 flag, u64 align, u64 *va, u64 size)
{
    struct cache_allocator *ca = NULL;

    ca = cache_get_allocator(devid, flag);
    if (ca == NULL) {
        svm_err("Get cache allocator failed. (devid=%u; flag=0x%x)\n", devid, flag);
        return DRV_ERROR_INNER_ERR;
    }

    if (align != ca->strategy.alloc_gran) {
        svm_err("Cur cache flag does not support such align. (devid=%u; flag=0x%x; align=%llu)\n", devid, flag, align);
        return DRV_ERROR_INVALID_VALUE;
    }

    return cache_malloc(ca, va, size);
}

int svm_cache_free(u32 devid, u32 flag, u64 align, u64 va, u64 size)
{
    struct cache_allocator *ca = NULL;

    ca = cache_get_allocator(devid, flag);
    if (ca == NULL) {
        svm_err("Get cache allocator failed. (devid=%u; flag=0x%x)\n", devid, flag);
        return DRV_ERROR_INNER_ERR;
    }

    if (align != ca->strategy.alloc_gran) {
        svm_err("Cur cache flag does not support such align. (devid=%u; flag=0x%x; align=%llu)\n", devid, flag, align);
        return DRV_ERROR_INVALID_VALUE;
    }

    return cache_free(ca, va, size);
}

void svm_cache_shrink(u32 devid, u32 flag, u64 *size)
{
    struct cache_allocator *ca = NULL;
    u64 shrink_size = 0;
    int ret;

    *size = 0;
    ca = cache_get_allocator(devid, flag);
    if (ca == NULL) {
        return;
    }

    do {
        ret = cache_shrink_once(ca, &shrink_size);
        *size += (ret == DRV_ERROR_NONE) ? shrink_size : 0;
    } while (ret == DRV_ERROR_NONE);
}

void svm_show_cache(u32 devid, char *buf, u32 buf_len)
{
    u32 tmp_devid;
    u32 len = 0;

    tmp_devid = (devid >= SVM_MAX_DEV_NUM) ? svm_get_host_devid() : devid;

    len += cache_allocator_show(tmp_devid, buf, buf_len);
    len += cache_recycle_seg_show(tmp_devid, buf + len, buf_len - len);
}

int cache_restore(u32 devid, u64 start, u64 size, u32 flag)
{
    u32 normal_flag = cache_flag_to_normal_flag(flag) | SVM_NORMAL_MALLOC_FLAG_POPULATE_ONLY;
    return cache_malloc_raw(devid, 0, &start, size, normal_flag);
}