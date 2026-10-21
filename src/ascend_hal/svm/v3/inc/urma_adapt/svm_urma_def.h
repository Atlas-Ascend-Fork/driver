/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SVM_URMA_DEF_H
#define SVM_URMA_DEF_H

#ifndef CFG_FEATURE_NOT_SUPPORT_URMA
#include "urma_types.h"
#include "urma_api.h"

#include "svm_urma_to_ascend_flag.h"
#else

#define SVM_STUB_URMA_SEG_SIZE 48ULL
typedef struct urma_seg {
    char val[SVM_STUB_URMA_SEG_SIZE];
} urma_seg_t;

#define SVM_STUB_URMA_TARGET_SEG_SIZE 128ULL
typedef struct urma_target_seg {
    urma_seg_t seg;
    char val[SVM_STUB_URMA_TARGET_SEG_SIZE];
} urma_target_seg_t;

typedef enum urma_opcode {
    URMA_OPC_STUB
} urma_opcode_t;

#define SVM_STUB_URMA_TOKEN_ID_SIZE 40ULL
typedef struct urma_token_id {
    char val[SVM_STUB_URMA_TOKEN_ID_SIZE];
} urma_token_id_t;

#define SVM_STUB_URMA_TOKEN_SIZE 32ULL
typedef struct urma_token {
    char val[SVM_STUB_URMA_TOKEN_SIZE];
} urma_token_t;
#endif

#endif
