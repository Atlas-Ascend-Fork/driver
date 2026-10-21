/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SVMM_H
#define SVMM_H

#include <stdbool.h>

#include "svm_pub.h"
#include "svm_addr_desc.h"

enum svmm_overlap_type {
    SVMM_OVERLAP = 0U,
    SVMM_NON_OVERLAP,
    SVMM_DEV_NON_OVERLAP,
    SVMM_INVALID_OVERLAP_TYPE,
};

/*
    SVMM: share virtual mem map, record mapped va and it`s src addr
    create/destroy: create or destroy a svmm inst
    svm_svmm_add_seg: add a new segment to svmm inst
    svm_svmm_del_seg: del a segment from svmm inst
    svm_svmm_get_seg: query a segment
*/
void *svm_svmm_create_inst(u64 svmma_start, u64 svmma_size, enum svmm_overlap_type overlap_type, u64 svm_flag);
void svm_svmm_destroy_inst(void *svmm_inst);
void svm_svmm_parse_inst_info(void *svmm_inst, u64 *svmma_start, u64 *svmma_size, u64 *svm_flag);
u32 svm_svmm_inst_show_detail(void *svmm_inst, char *buf, u32 buf_len);
static inline void svm_svmm_inst_show(void *svmm_inst)
{
    (void)svm_svmm_inst_show_detail(svmm_inst, NULL, 0);
}

/*
    svm_flag is seg's flag,
    svm_flag.cap is belong to map dst start,
    svm_flag.attr.pa* is belong to src start, svm_flag.attr.pg* is belong to map dst start
*/
int svm_svmm_add_seg(void *svmm_inst, u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info);
int svm_svmm_can_add_seg(void *svmm_inst, u32 devid, u64 start, u64 size);
/*
 * The svm_flag saved when adding a segment may be modified later. Pass a non-NULL svm_flag to get the final state
 * when deleting the segment. The output is valid only on success.
 */
int svm_svmm_del_seg_with_flag(void *svmm_inst, u32 devid, u64 start, u64 size, bool force, u64 *svm_flag);
static inline int svm_svmm_del_seg(void *svmm_inst, u32 devid, u64 start, u64 size, bool force)
{
    return svm_svmm_del_seg_with_flag(svmm_inst, devid, start, size, force, NULL);
}
/*
    match va, devid and src_info->udevid
    if all is invalid(va=0, devid=SVM_INVALID_DEVID, src_info->udevid=SVM_INVALID_DEVID), get first segment
*/
int svm_svmm_get_seg(void *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag, struct svm_global_va *src_info);

static inline int svm_svmm_get_seg_by_va(void *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                         struct svm_global_va *src_info)
{
    *devid = SVM_INVALID_DEVID;
    src_info->udevid = SVM_INVALID_UDEVID;
    src_info->va = 0;
    src_info->size = 0;
    return svm_svmm_get_seg(svmm_inst, devid, va, svm_flag, src_info);
}

static inline int svm_svmm_get_seg_by_devid(void *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                            struct svm_global_va *src_info)
{
    *va = 0;
    src_info->udevid = SVM_INVALID_UDEVID;
    src_info->va = 0;
    src_info->size = 0;
    return svm_svmm_get_seg(svmm_inst, devid, va, svm_flag, src_info);
}

static inline int svm_svmm_get_seg_by_src_udevid(void *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                                 struct svm_global_va *src_info)
{
    *va = 0;
    *devid = SVM_INVALID_DEVID;
    src_info->va = 0;
    src_info->size = 0;
    return svm_svmm_get_seg(svmm_inst, devid, va, svm_flag, src_info);
}

static inline int svm_svmm_get_seg_by_src_udevid_and_va(void *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                                        struct svm_global_va *src_info)
{
    *va = 0;
    return svm_svmm_get_seg(svmm_inst, devid, va, svm_flag, src_info);
}

static inline int svm_svmm_get_first_seg(void *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                         struct svm_global_va *src_info)
{
    *va = 0;
    *devid = SVM_INVALID_DEVID;
    src_info->udevid = SVM_INVALID_UDEVID;
    src_info->va = 0;
    src_info->size = 0;
    return svm_svmm_get_seg(svmm_inst, devid, va, svm_flag, src_info);
}

struct svm_svmm_seg_priv_ops {
    int (*release)(void *priv, bool force);
    u32 (*show)(void *priv, char *buf, u32 buf_len);
};

int svm_svmm_for_each_seg_handle(void *svmm_inst,
                                 int (*func)(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv),
                                 void *priv);
void *svm_svmm_seg_handle_get(void *svmm_inst, u64 va);
void svm_svmm_seg_handle_put(void *seg_handle);

int svm_svmm_set_seg_priv(void *seg_handle, void *priv, struct svm_svmm_seg_priv_ops *priv_ops);
void *svm_svmm_get_seg_priv(void *seg_handle);
u32 svm_svmm_get_seg_devid(void *seg_handle);
u64 svm_svmm_get_seg_svm_flag(void *seg_handle);
void svm_svmm_mod_seg_svm_flag(void *seg_handle, u64 flag);
void svm_svmm_mod_seg_src_tgid(void *seg_handle, int tgid);

void svm_svmm_del_seg_handle(void *svmm_inst, void *seg_handle); /* only support non_overlap */

void svm_svmm_inst_occupy_pipeline(void *svmm_inst);
void svm_svmm_inst_release_pipeline(void *svmm_inst);

/* DRV_ERROR_NOT_EXIST: no hole */
int svm_svmm_get_first_hole(void *svmm_inst, u32 devid, u64 start, u64 size, u64 *hole_start, u64 *hole_size);

#endif
