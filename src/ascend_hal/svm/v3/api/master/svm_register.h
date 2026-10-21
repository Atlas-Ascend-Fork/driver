/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SVM_REGISTER_H
#define SVM_REGISTER_H

#include <stdint.h>
#include <stdbool.h>

#include "svm_pub.h"
#include "svm_addr_desc.h"

/*
    register svm addr to peer, it is like with prefetch witch share addr between device
    For the same peer device, the address can be mapped only once.
    case1: register svm host addr to device, the devid is dst id, we can register to multi device
    case2: register svm device addr to host, the devid is host id
*/

/* devid is peer devid */
#define SVM_REGISTER_TO_PEER_FLAG_READ_ONLY (1U << 0U)
int svm_register_to_peer(u64 va, u64 size, u32 devid, u32 register_to_peer_flag, u64 *dst_va);
int svm_unregister_to_peer(u64 va, u32 devid);

void svm_register_recycle(u32 devid);

struct svm_register_ops {
    int (*post_map)(struct svm_global_va *src_info, u32 devid, u64 dst_va, u32 register_to_peer_flag);
    void (*pre_unmap)(struct svm_global_va *src_info, u32 devid, u64 dst_va);
};

void svm_register_set_ops(struct svm_register_ops *ops);
void svm_register_compute_aligned(const struct svm_global_va *src_info, u64 npage_size,
                                  struct svm_global_va *aligned_info, u64 *aligned_size);
int svm_register_for_each_seg(u32 dst_devid,
                              int (*handle)(void *va_handle, u64 start, struct svm_global_va *src_info, void *priv),
                              void *priv);
bool svm_is_register_to_peer_src_range(u64 src_va, u64 size, u32 dst_devid);
int svm_register_peer_lookup(struct svm_global_va *src_info, u32 peer_devid, u64 *peer_va);
bool svm_va_can_register_peer(u64 src_va, u64 size, u32 devid);
#endif
