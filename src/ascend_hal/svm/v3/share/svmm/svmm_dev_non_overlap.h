/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SVMM_DEV_NON_OVERLAP_H
#define SVMM_DEV_NON_OVERLAP_H

#include "svmm.h"
#include "svmm_inst.h"
#include "svm_addr_desc.h"

void svmm_dev_non_overlap_init(struct svmm_inst *svmm_inst);
void svmm_dev_non_overlap_uninit(struct svmm_inst *svmm_inst);
u32 svmm_dev_non_overlap_show(struct svmm_inst *svmm_inst, char *buf, u32 buf_len);
int svmm_dev_non_overlap_add_seg(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 svm_flag,
                                 struct svm_global_va *src_info);
int svmm_dev_non_overlap_can_add_seg(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 size);
int svmm_dev_non_overlap_del_seg(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 size, bool force,
                                 u64 *svm_flag);
int svmm_dev_non_overlap_get_seg(struct svmm_inst *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                 struct svm_global_va *src_info);

u32 svmm_dev_non_overlap_get_seg_devid(void *seg_handle);
void svmm_dev_non_overlap_mod_seg_src_tgid(void *seg_handle, int tgid);

int svmm_dev_non_overlap_get_first_hole(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 size, u64 *hole_start,
                                        u64 *hole_size);

int svmm_dev_non_overlap_for_each_seg_handle(struct svmm_inst *svmm_inst,
                                             int (*func)(void *seg_handle, u64 start, struct svm_global_va *src_info,
                                                         void *priv),
                                             void *priv);
#endif
