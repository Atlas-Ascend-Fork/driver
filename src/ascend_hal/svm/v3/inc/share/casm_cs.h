/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CASM_CS_H
#define CASM_CS_H

#include "svm_addr_desc.h"
#include "casm_def.h"

int svm_casm_cs_query_src_info(u64 key, struct svm_global_va *src_va, int *owner_pid);
int svm_casm_cs_set_src_info(u32 devid, u64 key, const struct svm_global_va *src_va, int owner_pid,
                             const struct casm_src_prop *prop);
int svm_casm_cs_clr_src_info(u32 devid, u64 key);

#endif
