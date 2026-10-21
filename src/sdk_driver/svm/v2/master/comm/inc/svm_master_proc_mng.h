/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
#ifndef SVM_MASTER_PROC_MNG_H
#define SVM_MASTER_PROC_MNG_H

#include "ka_type.h"

#include "svm_res_idr.h"
#include "svm_task_dev_res_mng.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"

struct devmm_proc_async_copy_record {
    struct svm_idr task_idr;
    ka_atomic_t task_cnt[SVM_MAX_AGENT_NUM];
    bool is_async_allow[SVM_MAX_AGENT_NUM];
};

#define DEVMM_CONVERT_TREE_NUM 128ul
struct devmm_convert_dma {
    ka_rb_root_t dma_rbtree;
    ka_spinlock_t rbtree_lock;
};

struct devmm_dma_desc_node_rb_info {
    ka_rb_root_t root;
    ka_spinlock_t spinlock;

    u64 node_num;
    u64 node_peak_num;
};

struct devmm_share_id_map_mng {
    ka_rw_semaphore_t sem;
    ka_rb_root_t rbtree;
};

struct devmm_register_dma_mng {
    ka_rwlock_t rbtree_rwlock;
    ka_rb_root_t rbtree;
};

struct devmm_mem_stats_va_mng {
    ka_semaphore_t sem;
    ka_page_t **pages;
    void *kva;
};

struct devmm_master_p2p_mem_mng {
    ka_mutex_t lock;
    ka_list_head_t list_head;
    u64 get_cnt;
    u64 put_cnt;
    u64 free_cb_cnt;
};

#define DMA_DESC_RB_INFO_CNT 64
struct devmm_pagefault_info_slot {
    ka_rwlock_t rwlock;           /* Kernel rwlock; write on record/clear, read on query */
    struct svmPagefaultInfo info; /* Latest pagefault failure info for this device */
    bool has_record;              /* Whether a record exists for this device */
};

struct devmm_svm_proc_master {
    ka_semaphore_t dev_setup_sem[SVM_MAX_AGENT_NUM];
    struct devmm_task_dev_res_info task_dev_res_info;

    struct devmm_proc_async_copy_record async_copy_record;
    struct devmm_convert_dma convert_dma[DEVMM_CONVERT_TREE_NUM];

    struct devmm_dma_desc_node_rb_info dma_desc_rb_info[DMA_DESC_RB_INFO_CNT];
    struct devmm_share_id_map_mng share_id_map_mng[SVM_MAX_AGENT_NUM];
    struct devmm_share_id_map_mng master_share_id_map_mng;

    struct devmm_register_dma_mng register_dma_mng[SVM_MAX_AGENT_NUM];
    struct devmm_mem_stats_va_mng mem_stats_va_mng[SVM_MAX_AGENT_NUM];
    struct devmm_master_p2p_mem_mng p2p_mem_mng[SVM_MAX_AGENT_NUM];

    /* Per-device pagefault info records -- per-process, stored in priv_data.
     * halMemCtl query is per-process, so records must be in priv_data. */
    struct devmm_pagefault_info_slot pagefault_info_records[SVM_MAX_AGENT_NUM];
    ka_mnt_namespace_t *mnt_ns;

    /* Timed-out DMA copy tasks that were not freed (callbacks may still fire).
     * Cleaned up during process release. */
    struct list_head timeout_task_list;
    ka_mutex_t timeout_task_lock;
};

u32 devmm_get_proc_dev_async_task_cnt(struct devmm_svm_process *svm_proc, u32 devid);

bool devmm_proc_dev_async_task_is_empty(struct devmm_svm_process *svm_proc, u32 devid);
bool devmm_proc_async_task_is_empty(struct devmm_svm_process *svm_proc);
void devmm_proc_dev_set_async_allow(struct devmm_svm_process *svm_proc, u32 devid, bool value);
bool devmm_proc_dev_is_async_allow(struct devmm_svm_process *svm_proc, u32 devid);
void devmm_timeout_task_add(struct devmm_svm_process *svm_proc, struct devmm_dma_copy_task *copy_task);

#endif /* SVM_MASTER_PROC_MNG_H */
