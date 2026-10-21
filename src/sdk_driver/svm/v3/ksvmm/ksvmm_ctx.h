/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef KSVMM_CTX_H
#define KSVMM_CTX_H

#include "ka_base_pub.h"
#include "ka_common_pub.h"

#include "pbl_range_rbtree.h"
#include "pbl_kref_safe.h"

#include "svm_addr_desc.h"

struct ksvmm_ctx {
    u32 udevid;
    int tgid;
    void *task_ctx;

    ka_rwlock_t lock;
    struct range_rbtree range_tree;
};

struct ksvmm_seg {
    struct range_rbtree_node range_node;
    ka_atomic64_t refcnt;

    struct svm_global_va src_info;
};

struct ksvmm_ctx *ksvmm_ctx_get(u32 udevid, int tgid);
void ksvmm_ctx_put(struct ksvmm_ctx *ctx);

int ksvmm_init_task(u32 udevid, int tgid, void *start_time);
void ksvmm_uninit_task(u32 udevid, int tgid, void *start_time);
void ksvmm_show_task(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq);
int ksvmm_init(void);
void ksvmm_uninit(void);

#endif
