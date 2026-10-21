/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCEND_URMA_TOKEN_H
#define ASCEND_URMA_TOKEN_H

#include "urma_types.h"

#include "ascend_urma_pub.h"

#define ASCEND_URMA_TOKEN_FLAG_UNIQUE (1U << 0U)

struct ascend_urma_token_pool_attr {
    u32 token_num_default;
    u32 token_num_cache_up_thres;
    u32 max_acquired_num_per_token;
};

void *ascend_urma_token_pool_create(u32 devid, struct ascend_urma_token_pool_attr *attr);
void ascend_urma_token_pool_destroy(void *pool);

void *ascend_urma_token_acquire(void *pool, u32 token_flag);
void ascend_urma_token_acquire_existing(void *token);
void ascend_urma_token_release(void *token);

urma_token_id_t *ascend_urma_token_to_id(void *token);
urma_token_t ascend_urma_token_to_val(void *token);
#endif
