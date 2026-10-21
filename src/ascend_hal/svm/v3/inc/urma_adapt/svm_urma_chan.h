/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SVM_URMA_CHAN_H
#define SVM_URMA_CHAN_H

#include "svm_urma_def.h"
#include "svm_pub.h"

struct svm_urma_chan_submit_para {
    u64 src;
    u64 dst;
    u64 size;
    urma_opcode_t opcode;
    urma_target_seg_t *src_tseg;
    urma_target_seg_t *dst_tseg;
};

int svm_urma_chan_init(u32 devid);
void svm_urma_chan_uninit(u32 devid);

int svm_urma_chan_submit(u32 devid, u32 chan_id, struct svm_urma_chan_submit_para *para, u32 *wr_num);
int svm_urma_chan_wait(u32 devid, u32 chan_id, int wr_num, int timeout_ms);

int svm_urma_chan_alloc(u32 devid, u32 *chan_id);
int svm_urma_chan_try_alloc(u32 devid, u32 *chan_id, int64_t timeout_s);
void svm_urma_chan_free(u32 devid, u32 chan_id);
int svm_urma_chan_wait_one(u32 devid, u32 chan_id, int timeout_ms);
int svm_urma_chan_get_token(u32 devid, u32 chan_id, urma_token_id_t **token, urma_token_t *token_val);

/* Per-channel opaque user context: set on submit, retrieved on wait. The channel is exclusively
 * held between alloc and free, so no extra locking is required. */
void svm_urma_chan_set_user_ctx(u32 devid, u32 chan_id, void *ctx);
void *svm_urma_chan_get_user_ctx(u32 devid, u32 chan_id);

u64 svm_urma_chan_max_once_cpy_size(void);
#endif
