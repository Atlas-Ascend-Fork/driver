/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>

#include "ascend_hal_error.h"

#include "bitmap.h"

#include "trs_user_pub_def.h"
#include "trs_dev_drv.h"
#include "trs_sq_mem_raw.h"
#include "trs_sq_mem_cache.h"

#define TRS_SQ_MEM_CACHE_MAX_SIZE (2ULL * TRS_BYTES_PER_MB)
#define TRS_SQ_MEM_CACHE_ALLOC_THRES TRS_SQ_MEM_CACHE_MAX_SIZE

/* ca: cache allocator */
struct trs_sq_mem_ca {
    uint32_t devid;

    uint64_t start;
    uint64_t size;

    uint64_t align;

    pthread_rwlock_t rwlock;
    uint64_t bitnum;
    bitmap_t *bitmap;
};

static pthread_rwlock_t g_rwlock[TRS_DEV_NUM];
struct trs_sq_mem_ca *g_ca[TRS_DEV_NUM];

static uint64_t trs_sq_mem_ca_get_align(void)
{
    return (uint64_t)getpagesize();
}

static int trs_sq_mem_ca_va_to_bit(struct trs_sq_mem_ca *ca, uint64_t va)
{
    return (int)((va - ca->start) / ca->align);
}

static uint64_t trs_sq_mem_ca_bit_to_va(struct trs_sq_mem_ca *ca, int bit_start)
{
    return ca->start + ((uint64_t)bit_start) * ca->align;
}

static struct trs_sq_mem_ca *trs_sq_mem_ca_alloc(uint32_t devid)
{
    struct trs_sq_mem_ca *ca = NULL;

    ca = calloc(1, sizeof(struct trs_sq_mem_ca));
    if (ca == NULL) {
        trs_err("Calloc failed. (size=%llu)\n", (uint64_t)sizeof(struct trs_sq_mem_ca));
        return NULL;
    }

    ca->devid = devid;
    ca->size = TRS_SQ_MEM_CACHE_MAX_SIZE;
    ca->align = trs_sq_mem_ca_get_align();
    ca->bitnum = ca->size / ca->align;
    (void)pthread_rwlock_init(&ca->rwlock, NULL);

    ca->bitmap = calloc(ca->bitnum / (sizeof(bitmap_t) * TRS_BITS_PER_BYTE), sizeof(bitmap_t));
    if (ca->bitmap == NULL) {
        trs_err("Calloc failed. (bitnum=%llu)\n", ca->bitnum);
        free(ca);
        return NULL;
    }

    bitmap_clear((unsigned long *)ca->bitmap, 0, (int)ca->bitnum);

    return ca;
}

static void trs_sq_mem_ca_free(struct trs_sq_mem_ca *ca)
{
    free(ca->bitmap);
    free(ca);
}

static struct trs_sq_mem_ca *_trs_sq_mem_ca_create(uint32_t devid)
{
    struct trs_sq_mem_ca *ca = NULL;
    int ret;

    ca = trs_sq_mem_ca_alloc(devid);
    if (ca == NULL) {
        return NULL;
    }

    ret = trs_sq_mem_raw_alloc(devid, &ca->start, ca->size);
    if (ret != DRV_ERROR_NONE) {
        trs_sq_mem_ca_free(ca);
        return NULL;
    }

    return ca;
}

static void _trs_sq_mem_ca_destroy(struct trs_sq_mem_ca *ca)
{
    trs_sq_mem_raw_free(ca->devid, ca->start);
    trs_sq_mem_ca_free(ca);
}

static struct trs_sq_mem_ca *trs_sq_mem_ca_create(uint32_t devid)
{
    (void)pthread_rwlock_wrlock(&g_rwlock[devid]);
    if (g_ca[devid] == NULL) {
        g_ca[devid] = _trs_sq_mem_ca_create(devid);
    }
    (void)pthread_rwlock_unlock(&g_rwlock[devid]);

    return g_ca[devid];
}

static void trs_sq_mem_ca_destroy(uint32_t devid)
{
    (void)pthread_rwlock_wrlock(&g_rwlock[devid]);
    if (g_ca[devid] != NULL) {
        _trs_sq_mem_ca_destroy(g_ca[devid]);
        g_ca[devid] = NULL;
    }
    (void)pthread_rwlock_unlock(&g_rwlock[devid]);
}

static struct trs_sq_mem_ca *trs_sq_mem_ca_get(uint32_t devid)
{
    (void)pthread_rwlock_rdlock(&g_rwlock[devid]);
    if (g_ca[devid] == NULL) {
        (void)pthread_rwlock_unlock(&g_rwlock[devid]);
        return NULL;
    }
    return g_ca[devid];
}

static void trs_sq_mem_ca_put(uint32_t devid)
{
    (void)pthread_rwlock_unlock(&g_rwlock[devid]);
}

static int _trs_sq_mem_cache_alloc(struct trs_sq_mem_ca *ca, uint64_t *va, uint64_t size)
{
    uint64_t bit_start;
    uint32_t nr = (uint32_t)(size / ca->align);

    (void)pthread_rwlock_wrlock(&ca->rwlock);
    bit_start = bitmap_find_next_zero_area(ca->bitmap, ca->bitnum, 0, nr, 0);
    if (bit_start >= ca->bitnum) {
        (void)pthread_rwlock_unlock(&ca->rwlock);
        return DRV_ERROR_NO_RESOURCES;
    }

    bitmap_set(ca->bitmap, (int)bit_start, (int)nr);
    *va = trs_sq_mem_ca_bit_to_va(ca, (int)bit_start);
    (void)pthread_rwlock_unlock(&ca->rwlock);
    return DRV_ERROR_NONE;
}

static void _trs_sq_mem_cache_free(struct trs_sq_mem_ca *ca, uint64_t va, uint64_t size)
{
    (void)pthread_rwlock_wrlock(&ca->rwlock);
    bitmap_clear(ca->bitmap, (int)trs_sq_mem_ca_va_to_bit(ca, va), (int)(size / ca->align));
    (void)pthread_rwlock_unlock(&ca->rwlock);
}

static bool trs_sq_mem_cache_is_support(uint32_t devid, uint64_t size)
{
    return ((devid < TRS_DEV_NUM) && (size <= TRS_SQ_MEM_CACHE_ALLOC_THRES) &&
            TRS_IS_ALIGNED(size, trs_sq_mem_ca_get_align()));
}

int trs_sq_mem_cache_alloc(uint32_t devid, uint64_t *va, uint64_t size)
{
    struct trs_sq_mem_ca *ca = NULL;
    int ret;

    if (!trs_sq_mem_cache_is_support(devid, size)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ca = trs_sq_mem_ca_get(devid);
    if (ca == NULL) {
        ca = trs_sq_mem_ca_create(devid);
        if (ca == NULL) {
            trs_err("Create cache allocator failed. (devid=%u)\n", devid);
            return DRV_ERROR_INNER_ERR;
        }
        ca = trs_sq_mem_ca_get(devid);
        if (ca == NULL) {
            return DRV_ERROR_INNER_ERR;
        }
    }

    ret = _trs_sq_mem_cache_alloc(ca, va, size);
    trs_sq_mem_ca_put(devid);
    return ret;
}

int trs_sq_mem_cache_free(uint32_t devid, uint64_t va, uint64_t size)
{
    struct trs_sq_mem_ca *ca = NULL;

    if (devid >= TRS_DEV_NUM) {
        trs_err("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ca = trs_sq_mem_ca_get(devid);
    if (ca == NULL) {
        trs_err("Get cache allocator failed. (devid=%u)\n", devid);
        return DRV_ERROR_INNER_ERR;
    }

    if (!TRS_IS_ALIGNED(va, ca->align) || !TRS_IS_ALIGNED(size, ca->align)) {
        trs_sq_mem_ca_put(devid);
        trs_err("Va & size should be aligned. (va=0x%llx; size=0x%llx; align=0x%llx)\n", va, size, ca->align);
        return DRV_ERROR_INVALID_VALUE;
    }

    _trs_sq_mem_cache_free(ca, va, size);
    trs_sq_mem_ca_put(devid);
    return DRV_ERROR_NONE;
}

int trs_sq_mem_cache_check_exists(uint32_t devid, uint64_t va, uint64_t size)
{
    struct trs_sq_mem_ca *ca = NULL;
    int ret;

    if (devid >= TRS_DEV_NUM) {
        return DRV_ERROR_INVALID_VALUE;
    }

    ca = trs_sq_mem_ca_get(devid);
    if (ca == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = ((va >= ca->start) && (size <= ca->size) && ((va + size) <= (ca->start + ca->size))) ? DRV_ERROR_NONE :
                                                                                                 DRV_ERROR_NOT_EXIST;
    trs_sq_mem_ca_put(devid);
    return ret;
}

static int trs_sq_mem_cache_dev_init(uint32_t devid)
{
    (void)devid;
    return DRV_ERROR_NONE;
}

static void trs_sq_mem_cache_dev_uninit(uint32_t devid)
{
    trs_sq_mem_ca_destroy(devid);
}

static struct trs_dev_init_ops trs_sq_mem_cache_dev_ops = {.dev_init = trs_sq_mem_cache_dev_init,
                                                           .dev_uninit = trs_sq_mem_cache_dev_uninit};

static void __attribute__((constructor)) trs_sq_mem_cache_set_up(void)
{
    int i;
    int ret;

    for (i = 0; i < TRS_DEV_NUM; i++) {
        (void)pthread_rwlock_init(&g_rwlock[i], NULL);
    }

    ret = trs_register_dev_init_ops(&trs_sq_mem_cache_dev_ops);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Register sq mem cache dev ops failed. (ret=%d)\n", ret);
    }
}