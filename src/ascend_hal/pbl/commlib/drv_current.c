/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>

#include "drv_current.h"

#define DRV_UNKNOWN_HAL_API_NAME "Unknown"

static pthread_key_t g_drv_thread_ctx_key;
static pthread_once_t g_drv_thread_ctx_once = PTHREAD_ONCE_INIT;
static struct drv_thread_ctx g_drv_thread_ctx_fallback = {
    .hal_api_name = DRV_UNKNOWN_HAL_API_NAME,
};

static void drv_thread_ctx_key_init(void)
{
    (void)pthread_key_create(&g_drv_thread_ctx_key, free);
}

struct drv_thread_ctx *drv_get_current(void)
{
    struct drv_thread_ctx *ctx = NULL;

    (void)pthread_once(&g_drv_thread_ctx_once, drv_thread_ctx_key_init);
    ctx = (struct drv_thread_ctx *)pthread_getspecific(g_drv_thread_ctx_key);
    if (ctx == NULL) {
        ctx = (struct drv_thread_ctx *)calloc(1, sizeof(struct drv_thread_ctx));
        if (ctx != NULL) {
            ctx->hal_api_name = DRV_UNKNOWN_HAL_API_NAME;
            (void)pthread_setspecific(g_drv_thread_ctx_key, ctx);
        } else {
            return &g_drv_thread_ctx_fallback;
        }
    }
    return ctx;
}

struct drv_current_api_scope drv_current_api_enter(const char *hal_api_name)
{
    struct drv_current_api_scope scope = {
        .previous_hal_api_name = drv_current->hal_api_name,
    };

    if (hal_api_name == NULL) {
        hal_api_name = DRV_UNKNOWN_HAL_API_NAME;
    }
    drv_current->hal_api_name = hal_api_name;
    return scope;
}

void drv_current_api_leave(struct drv_current_api_scope *scope)
{
    drv_current->hal_api_name = scope->previous_hal_api_name;
}
