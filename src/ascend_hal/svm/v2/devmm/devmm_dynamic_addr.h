/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DEVMM_DYNAMIC_ADDR_H
#define DEVMM_DYNAMIC_ADDR_H

int svm_da_add_dev(uint32_t devid);
void svm_da_del_dev(uint32_t devid);

#define SVM_DA_FLAG_WITH_MASTER (0x1 << 0)
int svm_da_alloc(uint64_t *va, uint64_t size, uint32_t flag);
int svm_da_free(uint64_t va);
int svm_da_query_size(uint64_t va, uint64_t *size);
bool svm_is_dyn_addr(uint64_t va);
void svm_da_free_set_ops(void (*pre_free)(uint64_t va, uint64_t size));

#endif
