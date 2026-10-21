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

#ifndef _DVT_DOMAIN_MANAGE_H_
#define _DVT_DOMAIN_MANAGE_H_

#include "hw_vdavinci.h"
#include "dma_pool_map.h"

#define DOMAIN_PIN_STATUS_INVALID 0x0
#define DOMAIN_PIN_STATUS_READY 0x1
#define DOMAIN_MAP_STATUS_INVALID 0x2
#define DOMAIN_MAP_STATUS_READY 0x4
#define DOMAIN_DMA_BIT_MASK_32 32

struct dev_dom_info {
    struct hw_vdavinci *vdavinci;
    int status;
    bool is_passthrough;
    ka_kref_t ref;
    struct vm_dom_info *vm_dom;
    ka_list_head_t list;
    struct {
        int (*dev_dma_map_ram_range)(struct hw_vdavinci *vdavinci, struct ram_range_info *ram_info);
        void (*dev_dma_unmap_ram_range)(struct hw_vdavinci *vdavinci, struct ram_range_info *ram_info);
        int (*hw_vdavinci_get_iova_sg)(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom, unsigned long gfn,
                                       unsigned long size, ka_sg_table_t **dma_sgt);
        int (*hw_vdavinci_get_iova_array)(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom, unsigned long *gfn,
                                          unsigned long *dma_addr, unsigned long count);
    } ops;
};

struct reserve_mem {
    ka_list_head_t node;
    unsigned long pfn_lo;
    unsigned long pfn_hi;
};

struct vm_dom_info {
    ka_list_head_t node;
    ka_kvm_t *kvm;
    atomic_t status;
    struct rw_semaphore sem;
    ka_kref_t ref;
    ka_list_head_t dev_dom_list_head;
    struct ram_range_info_list *ram_info_list;
    ka_iova_domain_t iovad;
#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
    ka_rb_root_cached_t pfn_root;
#endif
};

struct vm_pfn_node {
    ka_rb_node_t rb;
    unsigned long start;
    unsigned long end;
    unsigned long subtree_last;
};

ka_list_head_t *get_vm_domains_list(void);
ka_mutex_t *get_vm_domains_lock(void);

void dev_dom_release(ka_kref_t *ref);
struct dev_dom_info *dev_dom_info_find(struct vm_dom_info *vm_dom, struct hw_vdavinci *vdavinci);
struct dev_dom_info *dev_dom_info_new(struct vm_dom_info *vm_dom, struct hw_vdavinci *vdavinci);
struct dev_dom_info *dev_dom_info_get(struct vm_dom_info *vm_dom, struct hw_vdavinci *vdavinci);
void dev_dom_info_put(struct dev_dom_info *dev_dom, struct hw_vdavinci *vdavinci);
int dma_domain_add_reserve_mem(struct vm_dom_info *vm_dom, unsigned long pfn_lo, unsigned long pfn_hi);
void vm_dom_info_release(ka_kref_t *ref);
struct vm_dom_info *vm_dom_info_find(const ka_kvm_t *kvm);
struct vm_dom_info *vm_dom_info_new(ka_kvm_t *kvm);
struct vm_dom_info *vm_dom_info_get(ka_kvm_t *kvm);
int vm_add_and_merge_pfn(struct vm_dom_info *vm_dom, unsigned long pfn, unsigned long size);
bool is_vm_pfn_managed(struct vm_dom_info *vm_dom, unsigned long pfn, unsigned long size);

#endif
