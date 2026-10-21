/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SVM_URMA_SEG_MNG_H
#define SVM_URMA_SEG_MNG_H

#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

#include "ascend_hal_error.h"

#include "svm_urma_def.h"
#include "svm_pub.h"
#include "svm_addr_desc.h"
#include "svm_urma_seg_flag.h"

struct svm_urma_client_seg {
    u32 token_id;
    u32 token_val;
    u32 seg_flag;
    urma_seg_t seg;
    urma_target_seg_t *tseg;
};

int svm_urma_register_seg(u32 user_devid, struct svm_dst_va *dst_va, u32 seg_flag);
/* Unregister only care about if SVM_URMA_SEG_FLAG_SELF_USER. */
int svm_urma_unregister_seg(u32 user_devid, struct svm_dst_va *dst_va, u32 seg_flag);
int svm_urma_get_tseg(u32 user_devid, struct svm_dst_va *dst_va, urma_target_seg_t **tseg);
int svm_urma_tseg_get(u32 user_devid, struct svm_dst_va *dst_va, urma_target_seg_t **tseg);
void svm_urma_tseg_put(u32 user_devid, struct svm_dst_va *dst_va);
int svm_urma_get_token_info(u32 user_devid, struct svm_dst_va *dst_va, u32 *token_id, u32 *token_val);
int svm_urma_get_seg_with_token_info(u32 user_devid, struct svm_dst_va *dst_va, urma_seg_t *seg, u32 *token_id,
                                     u32 *token_val);

#endif
