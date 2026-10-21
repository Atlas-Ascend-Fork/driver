/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SVM_PAGEFAULT_INFO_H
#define SVM_PAGEFAULT_INFO_H

#include <pthread.h>
#include <stdbool.h>

#include "ascend_hal_define.h"

/* halMemCtl handler interface (registered in svm_mem_ctl.c) */
int mem_ctrl_pagefault_info_query(void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret);
int mem_ctrl_pagefault_info_clear(void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret);

/* Internal interface (called by svm_pagefault.c) */
void svm_pagefault_info_record_access(unsigned int devid, unsigned long long va);

#endif /* SVM_PAGEFAULT_INFO_H */
