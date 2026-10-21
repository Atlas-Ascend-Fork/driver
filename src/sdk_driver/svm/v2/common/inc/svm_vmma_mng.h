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
#ifndef SVM_VMMA_MNG_H
#define SVM_VMMA_MNG_H

#include "devmm_proc_info.h"
#include "svm_define.h"

#define DEVMM_MAX_ACCESS_DEVICE_NUM SVM_UAGENT_MAX_NUM

struct devmm_vmma_info {
    u64 va;
    u64 size;
    u64 pg_num;
    u32 pg_size;
    u32 pg_type;

    u32 side;
    u32 logic_devid;
    u32 devid;
    u32 vfid;
    u32 page_insert_dev_id;
    u32 local_handle_flag;
    u64 vmm_map_flag; /* Immutable initial map semantics after the VMMA is created. */
    bool owner_mapped;
    drv_mem_access_type type;

    u32 module_id;
    int phy_addr_blk_id;
    u64 offset_pg_num;
    u64 phy_addr_blk_pg_num;
    u64 pa_pg_size; /* Source physical-memory granularity used by VMM access. */
};

static inline bool devmm_vmma_is_owner_no_access(const struct devmm_vmma_info *info)
{
    return (info->vmm_map_flag & DEVMM_VMM_MAP_FLAG_OWNER_NO_ACCESS) != 0;
}

/*
 * This struct describes a virtual memory map area(vmma).
 */
struct devmm_vmma_struct {
    ka_rb_node_t rbnode;
    ka_kref_t ref;

    ka_rw_semaphore_t rw_sem;
    ka_mutex_t mutex;

    struct devmm_vmma_info info;
#ifdef DRV_HOST
    drv_mem_access_type device_access_type[DEVMM_MAX_ACCESS_DEVICE_NUM]; /* Indexed by udevid. */
#endif
};

struct devmm_vmma_mng {
    u64 va;
    u64 size;

    ka_rw_semaphore_t rw_sem;
    ka_rb_root_t root;
};

int devmm_vmma_mng_init(struct devmm_vmma_mng *mng, u64 va, u64 size);
void devmm_vmma_mng_uninit(struct devmm_vmma_mng *mng);

int devmm_vmma_create(struct devmm_vmma_mng *mng, struct devmm_vmma_info *info);
void devmm_vmma_destroy(struct devmm_vmma_mng *mng, struct devmm_vmma_struct *vmma);
struct devmm_vmma_struct *devmm_vmma_get(struct devmm_vmma_mng *mng, u64 va);
void devmm_vmma_put(struct devmm_vmma_struct *vmma);

void devmm_vmmas_destroy(struct devmm_svm_process *svm_proc, struct devmm_vmma_mng *mng);

int devmm_vmmas_occupy_inc(struct devmm_vmma_mng *mng, u64 va, u64 size);
void devmm_vmmas_occupy_dec(struct devmm_vmma_mng *mng, u64 va, u64 size);
int devmm_vmma_shared_set(struct devmm_vmma_mng *mng, struct devmm_vmma_struct *vmma);
void devmm_vmma_shared_clear(struct devmm_vmma_struct *vmma);
int devmm_vmma_exclusive_set(struct devmm_vmma_struct *vmma);
void devmm_vmma_exclusive_clear(struct devmm_vmma_struct *vmma);

bool devmm_addr_range_is_in_vmma(struct devmm_vmma_mng *mng, u64 va, u64 size);
#endif
