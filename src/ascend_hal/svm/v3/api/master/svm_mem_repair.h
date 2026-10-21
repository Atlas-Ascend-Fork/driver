/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SVM_MEM_REPAIR_H
#define SVM_MEM_REPAIR_H

#include <stdlib.h>

#include "ascend_hal_define.h"

#include "malloc_mng.h"
#include "svm_pub.h"

/* repair map unmap ops for task grp. */
struct svm_mem_repair_ops {
    int (*normal_map)(u64 va, struct svm_prop *prop);
    void (*normal_unmap)(u64 va, struct svm_prop *prop);
    int (*vmm_map)(u32 devid, u64 va, u64 svm_flag, struct svm_global_va *src_info);
    void (*vmm_unmap)(u32 devid, u64 va, u64 svm_flag, struct svm_global_va *src_info);
};

void svm_mem_repair_set_ops(struct svm_mem_repair_ops *ops);

int svm_mem_repair(struct MemRepairInPara *para);

#endif
