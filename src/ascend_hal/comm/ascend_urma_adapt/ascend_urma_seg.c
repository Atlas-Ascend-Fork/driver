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
#include <unistd.h>
#include <stdatomic.h>

#include "urma_api.h"
#include "urma_types.h"

#include "ascend_hal.h"
#include "ascend_hal_error.h"
#include "rbtree.h"
#include "comm_user_interface.h"

#include "ascend_urma_pub.h"
#include "ascend_urma_log.h"
#include "ascend_urma_dev.h"
#include "ascend_urma_token.h"

struct ascend_urma_seg_mng {
    pthread_rwlock_t rwlock;
    struct rbtree_root rb_root;

    u32 devid;
    void *token_pool;
    struct ascend_urma_seg_mng_attr attr;
};

struct ascend_urma_seg {
    struct rbtree_node node;
    atomic_int ref;

    u64 start;
    u64 size;
    u32 seg_flag;
    void *token;

    urma_target_seg_t *tseg;
};

static inline bool ascend_urma_seg_flag_bit_is_set(u32 flag, u32 bit_mask)
{
    return ((flag & bit_mask) != 0);
}

static inline bool ascend_urma_seg_flag_is_access_write(u32 flag)
{
    return ascend_urma_seg_flag_bit_is_set(flag, ASCEND_URMA_SEG_FLAG_ACCESS_WRITE);
}

static inline bool ascend_urma_seg_flag_is_pin(u32 flag)
{
    return ascend_urma_seg_flag_bit_is_set(flag, ASCEND_URMA_SEG_FLAG_PIN);
}

static inline bool ascend_urma_seg_flag_is_without_token_val(u32 flag)
{
    return ascend_urma_seg_flag_bit_is_set(flag, ASCEND_URMA_SEG_FLAG_WITHOUT_TOKEN_VAL);
}

static void rb_range_handle_of_ascend_urma_seg(struct rbtree_node *node, struct rb_range_handle *range_handle)
{
    struct ascend_urma_seg *seg = rb_entry(node, struct ascend_urma_seg, node);

    range_handle->start = seg->start;
    range_handle->end = seg->start + seg->size - 1;
}

static urma_target_seg_t *_ascend_urma_register_segment(void *urma_ctx, u64 start, u64 size, u32 seg_flag, void *token)
{
    urma_target_seg_t *tseg = NULL;
    urma_seg_cfg_t seg_cfg = {0};

    seg_cfg.va = ascend_urma_adapt_align_down(start, (u64)getpagesize());
    seg_cfg.len = ascend_urma_adapt_align_up(start + size, (u64)getpagesize()) - seg_cfg.va;
    seg_cfg.token_id = ascend_urma_token_to_id(token);
    seg_cfg.token_value = ascend_urma_token_to_val(token);

    seg_cfg.flag.bs.token_policy = ascend_urma_seg_flag_is_without_token_val(seg_flag) ? URMA_TOKEN_NONE :
                                                                                         URMA_TOKEN_PLAIN_TEXT;
    seg_cfg.flag.bs.cacheable = URMA_CACHEABLE;
    seg_cfg.flag.bs.dsva = 0;

    if (ascend_urma_seg_flag_is_access_write(seg_flag)) {
        seg_cfg.flag.bs.access = URMA_ACCESS_READ | URMA_ACCESS_WRITE | URMA_ACCESS_ATOMIC;
    } else {
        seg_cfg.flag.bs.access = URMA_ACCESS_READ;
    }

    seg_cfg.flag.bs.non_pin = ascend_urma_seg_flag_is_pin(seg_flag) ? 0 : 1;
    seg_cfg.flag.bs.user_iova = 0;
    seg_cfg.flag.bs.token_id_valid = 1;
    seg_cfg.flag.bs.reserved = 0;

    tseg = urma_register_seg(ascend_to_urma_ctx(urma_ctx), &seg_cfg);
    if ((tseg == NULL) && (errno != ERANGE)) {
        ascend_urma_err("Urma register seg failed.\n");
    }

    return tseg;
}

urma_target_seg_t *ascend_urma_register_segment(uint32_t devid, uint64_t start, uint64_t size, uint32_t seg_flag,
                                                void *token)
{
    void *urma_ctx = NULL;
    urma_target_seg_t *tseg = NULL;

    urma_ctx = ascend_urma_ctx_get(devid);
    if (urma_ctx == NULL) {
        ascend_urma_err("Get ascend_urma_ctx failed. (devid=%u)\n", devid);
        return NULL;
    }

    tseg = _ascend_urma_register_segment(urma_ctx, start, size, seg_flag, token);
    ascend_urma_ctx_put(urma_ctx);
    return tseg;
}

void ascend_urma_unregister_segment(urma_target_seg_t *tseg)
{
    (void)urma_unregister_seg(tseg);
}

static struct ascend_urma_seg *ascend_urma_seg_alloc(u32 devid, u64 start, u64 size, u32 seg_flag, void *token)
{
    struct ascend_urma_seg *seg = NULL;
    urma_target_seg_t *tseg = NULL;

    seg = calloc(1, sizeof(struct ascend_urma_seg));
    if (seg == NULL) {
        ascend_urma_err("Calloc ascend_urma_seg_inst failed. (size=%zu)\n", sizeof(struct ascend_urma_seg));
        return NULL;
    }

    tseg = ascend_urma_register_segment(devid, start, size, seg_flag, token);
    if (tseg == NULL) {
        free(seg);
        return NULL;
    }

    RB_CLEAR_NODE(&seg->node);
    seg->start = start;
    seg->size = size;
    seg->seg_flag = seg_flag;
    seg->token = token;
    seg->tseg = tseg;
    atomic_init(&seg->ref, 0);

    return seg;
}

static void ascend_urma_seg_free(struct ascend_urma_seg *seg)
{
    ascend_urma_unregister_segment(seg->tseg);
    free(seg);
}

static void ascend_urma_seg_erase(struct ascend_urma_seg_mng *seg_mng, struct ascend_urma_seg *seg)
{
    _rbtree_erase(&seg_mng->rb_root, &seg->node);
}

static int ascend_urma_seg_insert(struct ascend_urma_seg_mng *seg_mng, struct ascend_urma_seg *seg)
{
    int ret;

    ret = rbtree_insert_by_range(&seg_mng->rb_root, &seg->node, rb_range_handle_of_ascend_urma_seg);
    return (ret != 0) ? DRV_ERROR_BUSY : DRV_ERROR_NONE;
}

static struct ascend_urma_seg *ascend_urma_get_seg(struct ascend_urma_seg_mng *seg_mng, u64 start, u64 size)
{
    struct rbtree_node *rb_node = NULL;
    struct rb_range_handle range = {.start = start, .end = start + size - 1};

    rb_node = rbtree_search_by_range(&seg_mng->rb_root, &range, rb_range_handle_of_ascend_urma_seg);
    if (rb_node == NULL) {
        return NULL;
    }

    return rb_entry(rb_node, struct ascend_urma_seg, node);
}

static bool ascend_urma_exist_registered_seg_in_range(struct ascend_urma_seg_mng *seg_mng, u64 start, u64 size)
{
    struct rb_range_handle range_handle = {.start = start, .end = start + size - 1};

    return (rbtree_can_insert_range(&seg_mng->rb_root, &range_handle, rb_range_handle_of_ascend_urma_seg) == false);
}

static void ascend_urma_seg_to_info_ex(struct ascend_urma_seg *seg, struct ascend_urma_seg_info_ex *info)
{
    info->start = seg->start;
    info->size = seg->size;
    info->seg_flag = seg->seg_flag;
    info->token = seg->token;
    info->tseg = seg->tseg;
}

/* Get the segment immediately before va (the one with the largest start < va). */
int ascend_urma_seg_get_prev(void *seg_mng, uint64_t va, struct ascend_urma_seg_info_ex *info)
{
    struct ascend_urma_seg_mng *mng = (struct ascend_urma_seg_mng *)seg_mng;
    struct rbtree_node *node = NULL;
    struct rbtree_node *best = NULL;

    for (node = rbtree_first(&mng->rb_root); node != NULL; node = rbtree_next(node)) {
        struct ascend_urma_seg *seg = rb_entry(node, struct ascend_urma_seg, node);
        if (seg->start < va) {
            best = node;
        } else {
            break;
        }
    }
    if (best != NULL) {
        ascend_urma_seg_to_info_ex(rb_entry(best, struct ascend_urma_seg, node), info);
    }
    return (best != NULL) ? 0 : -1;
}

/* Get the segment immediately after va (the one with the smallest start > va). */
int ascend_urma_seg_get_next(void *seg_mng, uint64_t va, struct ascend_urma_seg_info_ex *info)
{
    struct ascend_urma_seg_mng *mng = (struct ascend_urma_seg_mng *)seg_mng;
    struct rbtree_node *node = NULL;

    for (node = rbtree_first(&mng->rb_root); node != NULL; node = rbtree_next(node)) {
        struct ascend_urma_seg *seg = rb_entry(node, struct ascend_urma_seg, node);
        if (seg->start > va) {
            ascend_urma_seg_to_info_ex(seg, info);
            return 0;
        }
    }
    return -1;
}

/* Reuse token from adjacent segment in the same reserved VA range; allocate new token if none found.
 * Overlapping ranges use a unique token (UB does not support overlapping registration).
 * On ERANGE (token segment limit reached): retry with a new unique token, unless already unique. */
static struct ascend_urma_seg *_ascend_urma_seg_register(struct ascend_urma_seg_mng *seg_mng, u64 start, u64 size,
                                                         u32 seg_flag)
{
    u64 aligned_va = ascend_urma_adapt_align_down(start, (u64)getpagesize());
    u64 aligned_size = ascend_urma_adapt_align_up(start + size, (u64)getpagesize()) - aligned_va;
    struct ascend_urma_seg *seg = NULL;
    u32 devid = seg_mng->devid;
    void *token = NULL;
    u32 token_flag = 0;

    /* Overlapping ranges must use a unique token (UB does not support overlapping registration). */
    if (ascend_urma_exist_registered_seg_in_range(seg_mng, aligned_va, aligned_size)) {
        token_flag |= ASCEND_URMA_TOKEN_FLAG_UNIQUE;
    } else if (seg_mng->attr.acquire_token != NULL) {
        /* Try to reuse token from an existing segment in the same reserve range. */
        token = seg_mng->attr.acquire_token(seg_mng, aligned_va, aligned_size);
        if (token != NULL) {
            ascend_urma_token_acquire_existing(token);
        }
    }

    if (token == NULL) {
        token = ascend_urma_token_acquire(seg_mng->token_pool, token_flag);
    }
    if (token == NULL) {
        ascend_urma_err("Acquire token failed. (token_flag=0x%x)\n", token_flag);
        return NULL;
    }

    seg = ascend_urma_seg_alloc(devid, start, size, seg_flag, token);
    if (seg == NULL) {
        int saved_errno = errno;

        ascend_urma_token_release(token);
        /* Retry with a new unique token on ERANGE, unless already unique. */
        if ((saved_errno == ERANGE) && ((token_flag & ASCEND_URMA_TOKEN_FLAG_UNIQUE) == 0)) {
            token_flag |= ASCEND_URMA_TOKEN_FLAG_UNIQUE;
            token = ascend_urma_token_acquire(seg_mng->token_pool, token_flag);
            if (token == NULL) {
                ascend_urma_err("Acquire new token failed on retry. (token_flag=0x%x)\n", token_flag);
                return NULL;
            }
            seg = ascend_urma_seg_alloc(devid, start, size, seg_flag, token);
        }
    }
    if (seg == NULL) {
        ascend_urma_token_release(token);
        ascend_urma_err("Alloc seg failed. (devid=%u; start=0x%llx; size=%llu; seg_flag=0x%x)\n", devid, start, size,
                        seg_flag);
        return NULL;
    }

    return seg;
}

static void _ascend_urma_seg_unregister(struct ascend_urma_seg_mng *seg_mng, struct ascend_urma_seg *seg)
{
    void *token = seg->token;

    ascend_urma_seg_free(seg);
    ascend_urma_token_release(token);
}

static int ascend_urma_seg_register(struct ascend_urma_seg_mng *seg_mng, u64 start, u64 size, u32 seg_flag)
{
    struct ascend_urma_seg *seg = NULL, *tmp_seg = NULL;
    u32 devid = seg_mng->devid;
    int ret;

    tmp_seg = ascend_urma_get_seg(seg_mng, start, size);
    if (tmp_seg != NULL) {
        if ((tmp_seg->start != start) || (tmp_seg->size != size)) {
            ascend_urma_debug("Urma seg info is overlap. (start=0x%llx; size=%llu; seg_start=0x%llx; seg_size=%llu)\n",
                              start, size, tmp_seg->start, tmp_seg->size);
            return DRV_ERROR_BUSY;
        }
        atomic_fetch_add(&tmp_seg->ref, 1);
        return 0;
    }

    seg = _ascend_urma_seg_register(seg_mng, start, size, seg_flag);
    if (seg == NULL) {
        ascend_urma_err("Alloc seg failed. (devid=%u; start=0x%llx; size=%llu; flag=0x%x)\n", devid, start, size,
                        seg_flag);
        return DRV_ERROR_INNER_ERR;
    }

    ret = ascend_urma_seg_insert(seg_mng, seg);
    if (ret != DRV_ERROR_NONE) {
        ascend_urma_debug("Insert seg check. (devid=%u; start=0x%llx; size=%llu; flag=0x%x)\n", devid, start, size,
                          seg_flag);
        _ascend_urma_seg_unregister(seg_mng, seg);
        return ret;
    }

    atomic_fetch_add(&seg->ref, 1);
    return DRV_ERROR_NONE;
}

static int ascend_urma_seg_unregister(struct ascend_urma_seg_mng *seg_mng, u64 start, u64 size)
{
    struct ascend_urma_seg *seg = NULL;

    seg = ascend_urma_get_seg(seg_mng, start, size);
    if (seg == NULL) {
        ascend_urma_err("Get seg failed. (start=0x%llx; size=%llu)\n", start, size);
        return DRV_ERROR_INVALID_VALUE;
    }
    if ((start != seg->start) || (size != seg->size)) {
        ascend_urma_err("Addr not match. (start=0x%llx; size=0x%llx; seg.start=0x%llx; seg.size=0x%llx)\n", start, size,
                        seg->start, seg->size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (atomic_load(&seg->ref) > 1) {
        atomic_fetch_sub(&seg->ref, 1);
        return DRV_ERROR_NONE;
    }
    ascend_urma_seg_erase(seg_mng, seg);
    _ascend_urma_seg_unregister(seg_mng, seg);
    return DRV_ERROR_NONE;
}

int ascend_urma_seg_get_info(struct ascend_urma_seg_mng *seg_mng, u64 va, struct ascend_urma_seg_info *info)
{
    struct ascend_urma_seg *seg = NULL;
    urma_token_id_t *urma_token_id = NULL;
    urma_token_t urma_token_val;

    seg = ascend_urma_get_seg(seg_mng, va, 1ULL);
    if (seg == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }

    urma_token_id = ascend_urma_token_to_id(seg->token);
    urma_token_val = ascend_urma_token_to_val(seg->token);

    info->start = seg->start;
    info->size = seg->size;
    info->token_id = urma_token_id->token_id;
    info->token_val = urma_token_val.token;
    info->seg_flag = seg->seg_flag;
    info->tseg = seg->tseg;

    return DRV_ERROR_NONE;
}

static void ascend_urma_seg_mng_init(struct ascend_urma_seg_mng *seg_mng, u32 devid)
{
    (void)pthread_rwlock_init(&seg_mng->rwlock, NULL);
    rbtree_init(&seg_mng->rb_root);
    seg_mng->devid = devid;
}

static void ascend_urma_seg_mng_uninit(struct ascend_urma_seg_mng *seg_mng)
{
    struct ascend_urma_seg *seg = NULL;
    struct rbtree_node *cur = NULL;
    struct rbtree_node *n = NULL;

    (void)pthread_rwlock_wrlock(&seg_mng->rwlock);
    rbtree_node_for_each_prev_safe(cur, n, &seg_mng->rb_root)
    {
        seg = rb_entry(cur, struct ascend_urma_seg, node);
        ascend_urma_seg_erase(seg_mng, seg);
        _ascend_urma_seg_unregister(seg_mng, seg);
    }
    (void)pthread_rwlock_unlock(&seg_mng->rwlock);
}

static struct ascend_urma_seg_mng *_ascend_urma_seg_mng_create(u32 devid, void *token_pool,
                                                               struct ascend_urma_seg_mng_attr *attr)
{
    struct ascend_urma_seg_mng *seg_mng = NULL;

    seg_mng = calloc(1, sizeof(struct ascend_urma_seg_mng));
    if (seg_mng == NULL) {
        ascend_urma_err("Malloc failed. (size=%llu)\n", (u64)sizeof(struct ascend_urma_seg_mng));
        return NULL;
    }

    seg_mng->attr = *attr;
    seg_mng->token_pool = token_pool;
    ascend_urma_seg_mng_init(seg_mng, devid);
    return seg_mng;
}

static void _ascend_urma_seg_mng_destroy(struct ascend_urma_seg_mng *seg_mng)
{
    ascend_urma_seg_mng_uninit(seg_mng);
    free(seg_mng);
}

static void ascend_urma_seg_mng_to_token_pool_attr(struct ascend_urma_seg_mng_attr *seg_mng_attr,
                                                   struct ascend_urma_token_pool_attr *token_pool_attr)
{
    token_pool_attr->token_num_default = seg_mng_attr->token_num_default;
    token_pool_attr->token_num_cache_up_thres = seg_mng_attr->token_num_cache_up_thres;
    token_pool_attr->max_acquired_num_per_token = seg_mng_attr->max_seg_num_per_token;
}

void *ascend_urma_seg_mng_create(uint32_t devid, struct ascend_urma_seg_mng_attr *attr)
{
    struct ascend_urma_token_pool_attr token_pool_attr;
    struct ascend_urma_seg_mng *seg_mng = NULL;
    void *token_pool;

    ascend_urma_seg_mng_to_token_pool_attr(attr, &token_pool_attr);
    token_pool = ascend_urma_token_pool_create(devid, &token_pool_attr);
    if (token_pool == NULL) {
        ascend_urma_err("Create token pool failed. (devid=%u)\n", devid);
        return NULL;
    }

    seg_mng = _ascend_urma_seg_mng_create(devid, token_pool, attr);
    if (seg_mng == NULL) {
        ascend_urma_token_pool_destroy(token_pool);
        return NULL;
    }

    return seg_mng;
}

void ascend_urma_seg_mng_destroy(void *seg_mng)
{
    struct ascend_urma_seg_mng *ascend_seg_mng = (struct ascend_urma_seg_mng *)seg_mng;
    void *token_pool = ascend_seg_mng->token_pool;

    _ascend_urma_seg_mng_destroy(ascend_seg_mng);
    ascend_urma_token_pool_destroy(token_pool);
}

int ascend_urma_register_seg(void *seg_mng, uint64_t start, uint64_t size, uint32_t seg_flag)
{
    struct ascend_urma_seg_mng *ascend_seg_mng = (struct ascend_urma_seg_mng *)seg_mng;
    int ret;

    (void)pthread_rwlock_wrlock(&ascend_seg_mng->rwlock);
    ret = ascend_urma_seg_register(ascend_seg_mng, start, size, seg_flag);
    (void)pthread_rwlock_unlock(&ascend_seg_mng->rwlock);
    return ret;
}

int ascend_urma_unregister_seg(void *seg_mng, uint64_t start, uint64_t size)
{
    struct ascend_urma_seg_mng *ascend_seg_mng = (struct ascend_urma_seg_mng *)seg_mng;
    int ret;

    (void)pthread_rwlock_wrlock(&ascend_seg_mng->rwlock);
    ret = ascend_urma_seg_unregister(ascend_seg_mng, start, size);
    (void)pthread_rwlock_unlock(&ascend_seg_mng->rwlock);
    return ret;
}

int ascend_urma_get_seg_info(void *seg_mng, uint64_t va, struct ascend_urma_seg_info *info)
{
    struct ascend_urma_seg_mng *ascend_seg_mng = (struct ascend_urma_seg_mng *)seg_mng;
    int ret;

    (void)pthread_rwlock_rdlock(&ascend_seg_mng->rwlock);
    ret = ascend_urma_seg_get_info(ascend_seg_mng, va, info);
    (void)pthread_rwlock_unlock(&ascend_seg_mng->rwlock);
    return ret;
}
