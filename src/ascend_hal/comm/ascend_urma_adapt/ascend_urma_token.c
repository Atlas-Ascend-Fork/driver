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
#include <stdlib.h>

#include "urma_api.h"

#include "ascend_hal_error.h"
#include "dms_misc_interface.h"
#include "drv_user_common.h"
#include "comm_user_interface.h"

#include "ascend_urma_pub.h"
#include "ascend_urma_log.h"
#include "ascend_urma_ctx.h"
#include "ascend_urma_dev.h"
#include "ascend_urma_token.h"

struct ascend_urma_token_pool {
    pthread_rwlock_t rwlock;
    struct list_head head;

    struct ascend_urma_token_pool_attr attr;
    u32 devid;
    u32 token_val;

    u32 total_num;
    struct ascend_urma_token *cur_token;
};

static inline bool ascend_urma_token_flag_bit_is_set(u32 flag, u32 bit_mask)
{
    return ((flag & bit_mask) != 0);
}

static inline bool ascend_urma_token_flag_is_unique(u32 flag)
{
    return ascend_urma_token_flag_bit_is_set(flag, ASCEND_URMA_TOKEN_FLAG_UNIQUE);
}

static void ascend_urma_token_acquired_num_inc(struct ascend_urma_token *token)
{
    token->acquired_num++;
}

static void ascend_urma_token_acquired_num_dec(struct ascend_urma_token *token)
{
    token->acquired_num--;
}

static struct ascend_urma_token *ascend_urma_token_alloc(void *urma_ctx, struct ascend_urma_token_pool *pool)
{
    struct ascend_urma_token *token = NULL;

    token = calloc(1, sizeof(struct ascend_urma_token));
    if (token == NULL) {
        ascend_urma_err("Malloc failed. (size=%llu)\n", (u64)sizeof(struct ascend_urma_token));
        return NULL;
    }

    INIT_LIST_HEAD(&token->node);
    token->pool = (void *)pool;
    token->acquired_num = 0;
    token->token_val.token = pool->token_val;
    token->token_id = urma_alloc_token_id(ascend_to_urma_ctx(urma_ctx));
    if (token->token_id == NULL) {
        ascend_urma_err("Urma alloc token id failed.\n");
        free(token);
        return NULL;
    }

    return token;
}

static void ascend_urma_token_free(struct ascend_urma_token *token)
{
    (void)urma_free_token_id(token->token_id);
    free(token);
}

static struct ascend_urma_token *_ascend_urma_get_first_token(struct ascend_urma_token_pool *pool)
{
    return ((drv_user_list_empty(&pool->head) == 1) ? NULL :
                                                      container_of(pool->head.next, struct ascend_urma_token, node));
}

static void _ascend_urma_token_insert(struct ascend_urma_token_pool *pool, struct ascend_urma_token *token)
{
    drv_user_list_add_head(&token->node, &pool->head);
    pool->total_num++;
    pool->cur_token = token;
}

static void _ascend_urma_token_erase(struct ascend_urma_token_pool *pool, struct ascend_urma_token *token)
{
    drv_user_list_del(&token->node);
    pool->total_num--;
    if (pool->cur_token == token) {
        pool->cur_token = _ascend_urma_get_first_token(pool);
    }
}

static struct ascend_urma_token_pool *ascend_urma_token_pool_alloc(u32 devid, struct ascend_urma_token_pool_attr *attr)
{
    struct ascend_urma_token_pool *pool = NULL;

    pool = calloc(1, sizeof(struct ascend_urma_token_pool));
    if (pool == NULL) {
        ascend_urma_err("Malloc failed. (size=%llu)\n", (u64)sizeof(struct ascend_urma_token_pool));
        return NULL;
    }

    (void)pthread_rwlock_init(&pool->rwlock, NULL);
    INIT_LIST_HEAD(&pool->head);
    pool->attr = *attr;
    pool->devid = devid;
    pool->total_num = 0;
    pool->cur_token = NULL;

    return pool;
}

static void ascend_urma_token_pool_free(struct ascend_urma_token_pool *pool)
{
    free(pool);
}

static void ascend_urma_token_pool_uninit(struct ascend_urma_token_pool *pool)
{
    struct ascend_urma_token *token = NULL;
    struct list_head *pos = NULL, *n = NULL;

    list_for_each_safe(pos, n, &pool->head)
    {
        token = container_of(pos, struct ascend_urma_token, node);
        _ascend_urma_token_erase(pool, token);
        ascend_urma_token_free(token);
    }
}

static int _ascend_urma_token_pool_init(void *urma_ctx, struct ascend_urma_token_pool *pool)
{
    struct ascend_urma_token *token = NULL;
    u32 token_val, i = 0;
    int ret;

    ret = ascend_urma_get_token_val(pool->devid, &token_val);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    pool->token_val = token_val;

    for (i = 0; i < pool->attr.token_num_default; i++) {
        token = ascend_urma_token_alloc(urma_ctx, pool);
        if (token == NULL) {
            ascend_urma_token_pool_uninit(pool);
            return DRV_ERROR_INNER_ERR;
        }

        _ascend_urma_token_insert(pool, token);
    }

    return DRV_ERROR_NONE;
}

static int ascend_urma_token_pool_init(struct ascend_urma_token_pool *pool)
{
    void *urma_ctx = NULL;
    int ret;

    urma_ctx = ascend_urma_ctx_get(pool->devid);
    if (urma_ctx == NULL) {
        ascend_urma_err("Get ascend_urma_ctx failed. (devid=%u)\n", pool->devid);
        return DRV_ERROR_UNINIT;
    }

    ret = _ascend_urma_token_pool_init(urma_ctx, pool);
    ascend_urma_ctx_put(urma_ctx);
    return ret;
}

struct ascend_urma_token *ascend_urma_token_acquir_new(struct ascend_urma_token_pool *pool)
{
    void *urma_ctx = NULL;
    struct ascend_urma_token *token = NULL;
    u32 devid = pool->devid;

    urma_ctx = ascend_urma_ctx_get(devid);
    if (urma_ctx == NULL) {
        ascend_urma_err("Get ascend_urma_ctx failed. (devid=%u)\n", devid);
        return NULL;
    }

    token = ascend_urma_token_alloc(urma_ctx, pool);
    ascend_urma_ctx_put(urma_ctx);
    if (token != NULL) {
        ascend_urma_token_acquired_num_inc(token);
        _ascend_urma_token_insert(pool, token);
    }

    return token;
}

static struct ascend_urma_token *ascend_urma_token_acquire_old(struct ascend_urma_token_pool *pool)
{
    struct ascend_urma_token *token = NULL;
    struct list_head *pos = NULL, *n = NULL;

    if ((pool->cur_token != NULL) && (pool->cur_token->acquired_num < pool->attr.max_acquired_num_per_token)) {
        ascend_urma_token_acquired_num_inc(pool->cur_token);
        return pool->cur_token;
    }

    list_for_each_safe(pos, n, &pool->head)
    {
        token = container_of(pos, struct ascend_urma_token, node);
        if (token->acquired_num < pool->attr.max_acquired_num_per_token) {
            ascend_urma_token_acquired_num_inc(token);
            pool->cur_token = token;
            return token;
        }
    }

    return NULL;
}

static struct ascend_urma_token *_ascend_urma_token_acquire(struct ascend_urma_token_pool *pool, u32 token_flag)
{
    struct ascend_urma_token *token = NULL;

    if (!ascend_urma_token_flag_is_unique(token_flag)) {
        token = ascend_urma_token_acquire_old(pool);
        if (token != NULL) {
            return token;
        }
    }

    return ascend_urma_token_acquir_new(pool);
}

static bool ascend_urma_token_pool_is_to_cache_up_thres(struct ascend_urma_token_pool *pool)
{
    return (pool->total_num >= pool->attr.token_num_cache_up_thres);
}

static void _ascend_urma_token_release(struct ascend_urma_token_pool *pool, struct ascend_urma_token *token)
{
    ascend_urma_token_acquired_num_dec(token);
    if ((token->acquired_num == 0) && ascend_urma_token_pool_is_to_cache_up_thres(pool)) {
        _ascend_urma_token_erase(pool, token);
        ascend_urma_token_free(token);
    }
}

void *ascend_urma_token_pool_create(u32 devid, struct ascend_urma_token_pool_attr *attr)
{
    struct ascend_urma_token_pool *pool = NULL;
    int ret;

    pool = ascend_urma_token_pool_alloc(devid, attr);
    if (pool == NULL) {
        return NULL;
    }

    ret = ascend_urma_token_pool_init(pool);
    if (ret != DRV_ERROR_NONE) {
        ascend_urma_token_pool_free(pool);
        return NULL;
    }

    return (void *)pool;
}

void ascend_urma_token_pool_destroy(void *pool)
{
    struct ascend_urma_token_pool *token_pool = (struct ascend_urma_token_pool *)pool;
    ascend_urma_token_pool_uninit(token_pool);
    ascend_urma_token_pool_free(token_pool);
}

void *ascend_urma_token_acquire(void *pool, u32 token_flag)
{
    struct ascend_urma_token_pool *token_pool = (struct ascend_urma_token_pool *)pool;
    struct ascend_urma_token *token = NULL;

    (void)pthread_rwlock_wrlock(&token_pool->rwlock);
    token = _ascend_urma_token_acquire(token_pool, token_flag);
    (void)pthread_rwlock_unlock(&token_pool->rwlock);

    return (void *)token;
}

void ascend_urma_token_acquire_existing(void *token)
{
    struct ascend_urma_token *ascend_urma_token = (struct ascend_urma_token *)token;
    struct ascend_urma_token_pool *token_pool = (struct ascend_urma_token_pool *)ascend_urma_token->pool;

    (void)pthread_rwlock_wrlock(&token_pool->rwlock);
    ascend_urma_token_acquired_num_inc(ascend_urma_token);
    (void)pthread_rwlock_unlock(&token_pool->rwlock);
}

void ascend_urma_token_release(void *token)
{
    struct ascend_urma_token *ascend_urma_token = (struct ascend_urma_token *)token;
    struct ascend_urma_token_pool *token_pool = (struct ascend_urma_token_pool *)ascend_urma_token->pool;

    (void)pthread_rwlock_wrlock(&token_pool->rwlock);
    _ascend_urma_token_release(token_pool, ascend_urma_token);
    (void)pthread_rwlock_unlock(&token_pool->rwlock);
}

urma_token_id_t *ascend_urma_token_to_id(void *token)
{
    struct ascend_urma_token *ascend_urma_token = (struct ascend_urma_token *)token;
    return ascend_urma_token->token_id;
}

urma_token_t ascend_urma_token_to_val(void *token)
{
    struct ascend_urma_token *ascend_urma_token = (struct ascend_urma_token *)token;
    return ascend_urma_token->token_val;
}

urma_token_id_t *ascend_urma_alloc_token_id(void *ctx)
{
    return urma_alloc_token_id(ascend_to_urma_ctx(ctx));
}

void ascend_urma_free_token_id(urma_token_id_t *token_id)
{
    (void)urma_free_token_id(token_id);
}
