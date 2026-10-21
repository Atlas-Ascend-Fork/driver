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

#ifndef DMA_MAP_CTX_H
#define DMA_MAP_CTX_H

#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_common_pub.h"

#include "pbl_range_rbtree.h"
#include "pbl_kref_safe.h"

#include "dma_map_kernel.h"

struct dma_map_ctx {
    u32 udevid;
    int tgid;
    void *task_ctx;
    ka_rwlock_t lock;
    struct range_rbtree range_tree;
};

struct dma_map_node {
    union {
        struct range_rbtree_node range_node;
    };
    ka_atomic_t refcnt;
    ka_kref_t kref;

    u32 udevid;
    int tgid;

    u32 flag;

    struct svm_global_va align_dst_va;
    u64 raw_va;
    u64 raw_size;

    bool is_write;
    bool is_svm_va;
    bool is_mpl_va;
    bool is_pfn_map;
    bool is_local_mem;
    bool is_remapping;

    ka_page_t **pages;
    struct svm_dma_addr_info addr_info;
};

struct dma_map_ctx *dma_map_ctx_get(u32 udevid, int tgid);
void dma_map_ctx_put(struct dma_map_ctx *ctx);

int dma_map_init_task(u32 udevid, int tgid, void *start_time);
void dma_map_uninit_task(u32 udevid, int tgid, void *start_time);
void dma_map_show_task(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq);
int svm_dma_map_init(void);
void svm_dma_map_uninit(void);

#endif
