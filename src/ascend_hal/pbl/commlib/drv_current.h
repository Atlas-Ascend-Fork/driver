/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DRV_CURRENT_H
#define DRV_CURRENT_H

/* Saves the previous HAL API when entering a nested public API. */
struct drv_current_api_scope {
    const char *previous_hal_api_name;
};

struct drv_current_api_scope drv_current_api_enter(const char *hal_api_name);
void drv_current_api_leave(struct drv_current_api_scope *scope);

/* Marks the lifetime of the current public HAL API. */
#define DRV_CURRENT_API_SCOPE()                         \
    struct drv_current_api_scope _drv_current_api_scope \
        __attribute__((cleanup(drv_current_api_leave), unused)) = drv_current_api_enter(__func__)

/*
 * Before accessing drv_current->hal_api_name, ensure the current public HAL API calls
 * DRV_CURRENT_API_SCOPE().
 */
struct drv_thread_ctx {
    const char *hal_api_name;
};

struct drv_thread_ctx *drv_get_current(void);

#define drv_current drv_get_current()

#endif
