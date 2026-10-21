/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DEVMM_VIRT_COM_HEAP_H
#define DEVMM_VIRT_COM_HEAP_H

#include "devmm_virt_interface.h"
#include "devmm_rbtree/devmm_rbtree.h"

#define DEVMM_4K_PAGE_SIZE 4096U            /* 4k */
#define DEVMM_2M_PAGE_SIZE 0x200000U        /* 2M */
#define DEVMM_DVPP_ATTR_PAGE_SIZE 0x200000U /* 2M */

#define DEVMM_MAP_ALIGN_SIZE 0x200000U /* 2M */
/* normal page once map size */
#define DEVMM_CHUCK_ALLOC_MAP_SIZE 0x200000U /* 2M */
/* huge page once map size */
#define DEVMM_HUGE_ALLOC_MAP_SIZE 0x2400000U /* 36M */

/* threshold for cache
 * max of huge page once map size and normal page once map size
 * DEVMM_HUGE_ALLOC_MAP_SIZE  DEVMM_CHUCK_ALLOC_MAP_SIZE
 */
#define DEVMM_HUGE_CACHE_NODE_SIZE_THRES 0x2400000U /* 36M */
/* 24M: 1 readonly node(4M+4K) occpy 6M physical memory, cache total 4 node */
#define DEVMM_HUGE_READONLY_CACHE_NODE_SIZE_THRES 0x1800000U
#define DEVMM_CACHE_MIN_THRES_SIZE 0x2400000U  /* 36M */
#define DEVMM_CACHE_COMM_THRES_SIZE 0x1200000U /* 18M */
#define DEVMM_CACHE_TSDDR_THRES_SIZE 0x400000U /* 4M */

/* threshold for segment */
#define DEVMM_SEG_THRES_SIZE 4096U
#define DEVMM_SEG_THRES_PERCENT 20 /* segmemt threshold percent: 20% */

/*
 * node flag define
 * bit0:     first va
 * bit1:     mapped
 * bit2~5:   memtype 0x0:normal 0x1:readonly
 * bit6~9:   mem_side: MEM_SVM_VAL, MEM_DEV_VAL, MEM_HOST_VAL, MEM_DVPP_VAL, MEM_HOST_AGENT_VAL, MEM_RESERVE_VAL,
 * bit10:    side: host, device
 * bit11:    nocache_flag: 0x0:cache 0x1 nocache
 * bit12~14: phy_memtype: HBM, DDR, P2P_HBM, P2P_DDR, TS_DDR
 * bit15:    page_type: 0x0:normal 0x1:huge
 * bit16~23: devid
 * bit24~31: module_id
 */
#define DEVMM_NODE_FIRST_VA_BIT 0
#define DEVMM_NODE_MAPPED_BIT 1
#define DEVMM_NODE_MEMTYPE_SHIFT 2
#define DEVMM_NODE_MEM_VAL_SHIFT 6
#define DEVMM_NODE_SIDE_SHIFT 10
#define DEVMM_NODE_NOCACHE_SHIFT 11
#define DEVMM_NODE_PHY_MEMTYPE_SHIFT 12
#define DEVMM_NODE_PAGE_TYPE_SHIFT 15
#define DEVMM_NODE_DEVID_SHIFT 16
#define DEVMM_NODE_MODULE_ID_SHIFT 24

#define DEVMM_NODE_MEMTYPE_WID 4
#define DEVMM_NODE_MEM_VAL_WID 4
#define DEVMM_NODE_SIDE_WID 1
#define DEVMM_NODE_NOCACHE_WID 1
#define DEVMM_NODE_PHY_MEMTYPE_WID 3
#define DEVMM_NODE_PAGE_TYPE_WID 1
#define DEVMM_NODE_DEVID_WID 8
#define DEVMM_NODE_MODULE_ID_WID 8

#define DEVMM_NODE_FIRST_VA_FLG (1UL << DEVMM_NODE_FIRST_VA_BIT)
#define DEVMM_NODE_MAPPED_FLG (1UL << DEVMM_NODE_MAPPED_BIT)

#define DEVMM_DVPP_HEAP_CACHE_NODE_NUM 512
#define DEVMM_COMM_HEAP_CACHE_NODE_NUM 128
#define DEVMM_BASE_HEAP_CACHE_NODE_NUM 512

typedef struct drv_mem_handle {
    int id;
    uint32_t side;
    uint32_t sdid;
    uint32_t devid;
    uint32_t module_id;
    uint64_t pg_num;

    uint32_t pg_type;
    uint32_t phy_mem_type;

    struct drv_mem_prop prop;
    bool support_restore;
    uint64_t ref;
    uint64_t map_ref; /* halMemMap, map_ref+1 */
    uint64_t pa_pg_size;
    bool is_share;
} drv_mem_handle_t;

struct devmm_com_heap_ops {
    virt_addr_t (*heap_alloc)(struct devmm_virt_com_heap *heap, virt_addr_t va, size_t size, DVmem_advise advise);
    DVresult (*heap_free)(struct devmm_virt_com_heap *heap, virt_addr_t ptr);
};

static inline uint64_t align_up(uint64_t value, uint64_t align)
{
    if (align == 0) {
        return value;
    }

    return (value + (align - 1)) & ~(align - 1);
}

DVresult devmm_virt_init_com_heap(struct devmm_virt_com_heap *heap, struct devmm_virt_heap_type *heap_type,
                                  struct devmm_com_heap_ops *ops, struct devmm_virt_heap_para *heap_info);
DVresult devmm_virt_init_com_base_heap(struct devmm_virt_com_heap *heap, struct devmm_virt_heap_type *heap_type,
                                       struct devmm_com_heap_ops *ops, struct devmm_virt_heap_para *heap_info);
#ifndef UVM_OPEN
DVresult devmm_virt_init_com_uvm_heap(struct devmm_virt_com_heap *heap, struct devmm_virt_heap_type *heap_type,
                                      struct devmm_com_heap_ops *ops, struct devmm_virt_heap_para *heap_info);
#endif
DVresult devmm_alloc_mem(uint64_t *pp, size_t bytesize, DVmem_advise advise, struct devmm_virt_com_heap *heap);
DVresult devmm_free_mem(uint64_t va, struct devmm_virt_com_heap *heap, uint64_t *free_len);
bool devmm_is_mem_allocated(uint64_t va, struct devmm_virt_com_heap *heap);

DVresult devmm_rbtree_insert_idle_mapped_tree(struct devmm_rbtree_node *rbtree_node,
                                              struct devmm_heap_rbtree *rbtree_queue);
DVresult devmm_rbtree_erase_idle_mapped_tree(struct devmm_rbtree_node *rbtree_node,
                                             struct devmm_heap_rbtree *rbtree_queue);
uint32_t devmm_virt_get_cache_size_by_heap_type(struct devmm_virt_heap_type *virt_heap_type);

uint32_t devmm_get_module_id_by_advise(DVmem_advise advise);

void devmm_mem_mapped_size_inc(struct devmm_virt_com_heap *heap, uint64_t size);
void devmm_mem_mapped_size_dec(struct devmm_virt_com_heap *heap, uint64_t size);
void devmm_module_mem_stats_dec(struct devmm_rbtree_node *node);

DVdeviceptr devmm_alloc_from_tree(struct devmm_virt_com_heap *heap, size_t bytesize, DVmem_advise advise,
                                  uint32_t tree_type, uint64_t va);
DVdeviceptr devmm_alloc_from_size_tree(struct devmm_virt_com_heap *heap, size_t size, DVmem_advise advise, uint64_t va);

void devmm_get_addr_module_id(uint64_t va, uint32_t *module_id, uint64_t *module_id_size);

void devmm_rbtree_free_node_resources(struct devmm_rbtree_node *rbtree_node);
bool devmm_node_is_need_restore(struct devmm_rbtree_node *node);

struct devmm_mem_info {
    uint64_t start;
    uint64_t end;
    uint32_t module_id;
    uint32_t devid;
};
int devmm_get_reserve_addr_info(uint64_t va, struct devmm_mem_info *mem_info);
void devmm_get_svm_va_info(uint64_t va, struct devmm_mem_info *mem_info);

#endif /* _DEVMM_VIRT_COM_HEAP_H_ */
