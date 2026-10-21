/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SOMA_POOL_CLIENT_H
#define SOMA_POOL_CLIENT_H

#include "svm_pub.h"

#define SOMA_LOG_TAG "[SOMA]: "
#define svm_soma_err(fmt, ...) svm_err(SOMA_LOG_TAG fmt, ##__VA_ARGS__)

int mem_pool_client_create(u32 devid, u64 pool_id, u64 va, u64 size, u64 global_va);
int mem_pool_client_destory(u32 devid, u64 pool_id, u64 *va);
int mem_pool_client_set_attr(u32 devid, u64 pool_id, u64 attr, u64 value);
int mem_pool_client_get_attr(u32 devid, u64 pool_id, u64 attr, u64 *value);
int mem_pool_client_mem_free(u32 devid, u64 pool_id, u64 size, u64 va);
#endif