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
#ifndef UVM_MASTER_PREFETCH_H
#define UVM_MASTER_PREFETCH_H
#include "devmm_proc_info.h"
#include "devmm_addr_mng.h"
#include "uvm_heap_mng.h"
#include "svm_ioctl.h"

struct uvm_prefetch_batch_info {
    uint64_t id;
    uint64_t va;
    uint16_t cnt;
    struct drv_uvm_location src_location;
    struct drv_uvm_location dst_location;
    bool is_readmostly;
};

struct prefetching_va_seg {
    uint64_t start_va;
    uint32_t cnt;
};

struct prefetching_task {
    uint64_t id;
    struct hlist_node hnode;
    struct devmm_addr_info *addr_infos;
    uint32_t addr_cnt;
    uint32_t addr_cap;
    struct prefetching_va_seg *va_segs;
    uint32_t seg_cnt;
    uint32_t seg_cap;
    uint32_t chunk_page_size;
    uint32_t dst_devid;
    struct devmm_uvm_heap *uvm_heap;
};

struct uvm_prefetch_type {
    bool h2d_readmostly;
};

void uvm_hnode_add_to_prefetching_task_hashtable(struct prefetching_task *task);
struct prefetching_task *uvm_hnode_find_from_prefetching_task_hashtable(uint64_t id);
void uvm_hnode_del_from_prefetching_task_hashtable(struct prefetching_task *task);

bool uvm_prefetch_task_try_append_addr(uint64_t id, const struct devmm_addr_info *addr_info, uint32_t cnt);
bool uvm_prefetch_task_try_append_va_seg(struct prefetching_task *task, uint64_t start_va, uint32_t cnt);
void uvm_prefetch_task_cleanup(struct prefetching_task *task);

int devmm_uvm_ioctl_prefetch(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);
int devmm_uvm_ioctl_prefetch_batch(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);

#endif /* UVM_MASTER_PREFETCH_H */