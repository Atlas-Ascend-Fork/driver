/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SVM_REGISTER_ONLY_H
#define SVM_REGISTER_ONLY_H

#include <stdbool.h>

#include "svm_pub.h"
#include "svm_addr_desc.h"

int svm_register_lookup(struct svm_global_va *src_info, u32 peer_devid, u64 *peer_va);

static inline bool svm_va_is_registered(u64 src_va, u64 size, u32 devid)
{
    struct svm_global_va src_info;
    u64 peer_va;

    svm_global_va_pack(SVM_DEFAULT_HOST_DEVID, 0, src_va, size, &src_info);
    return (svm_register_lookup(&src_info, devid, &peer_va) == DRV_ERROR_NONE);
}

#endif
