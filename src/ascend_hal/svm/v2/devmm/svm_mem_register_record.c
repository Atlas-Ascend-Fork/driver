/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "rbtree.h"
#include "devmm_virt_list.h"
#include "devmm_svm.h"
#include "svm_define.h"
#include "svm_mem_register_record.h"
#include "devmm_virt_interface.h"

struct register_record_node {
    DVdeviceptr src_ptr;
    uint64_t count;
    uint32_t map_type;
    uint32_t flag;
    DVdeviceptr dst_ptr;
    struct rb_node node;
};

#define container_of(ptr, type, member)                    \
    ({                                                     \
        typeof(((type *)0)->member) *__mptr = (ptr);       \
        (type *)((char *)__mptr - offsetof(type, member)); \
    })

#define REG_RECORD_MAX_DEV_NUM DEVMM_MAX_LOGIC_DEVICE_NUM
static struct rbtree_root g_reg_record_tree[MEM_REGISTER_MAX_SIDE][RECORD_TREE_MAX_TYPE][REG_RECORD_MAX_DEV_NUM];
static pthread_rwlock_t g_reg_record_lock[MEM_REGISTER_MAX_SIDE][RECORD_TREE_MAX_TYPE][REG_RECORD_MAX_DEV_NUM];

static inline uint32_t svm_mem_register_get_mem_side(uint32_t map_type)
{
    if ((map_type == HOST_MEM_MAP_DEV) || (map_type == HOST_SVM_MAP_DEV) || (map_type == HOST_MEM_MAP_DEV_PCIE_TH) ||
        (map_type == HOST_IO_MAP_DEV) || (map_type == HOST_MEM_MAP_DMA)) {
        return HOST_MEM_REGISTER;
    } else {
        return DEV_MEM_REGISTER;
    }
}

static inline uint32_t svm_mem_register_get_tree_type(uint32_t map_type)
{
    if (map_type == HOST_MEM_MAP_DMA) {
        return DMA_MAP_RECORD_TREE;
    } else {
        return NO_DMA_MAP_RECORD_TREE;
    }
}

static inline bool svm_mem_register_range_is_overlap(DVdeviceptr ptr, uint64_t size,
                                                     struct register_record_node *record_node)
{
    if ((ptr + size <= record_node->src_ptr) || (ptr >= record_node->src_ptr + record_node->count)) {
        return false;
    }
    return true;
}

static inline struct register_record_node *svm_mem_register_get_record_node(struct rbtree_node *rb_node)
{
    struct rb_node *tmp = container_of(rb_node, struct rb_node, rbtree_node);
    return container_of(tmp, struct register_record_node, node);
}

static void svm_mem_register_record_get_range(struct rbtree_node *node, struct rb_range_handle *range_handle)
{
    struct register_record_node *record_node = svm_mem_register_get_record_node(node);
    range_handle->start = record_node->src_ptr;
    range_handle->end = record_node->src_ptr + record_node->count - 1;
}

static struct register_record_node *svm_mem_register_find_overlap_record_node(DVdeviceptr ptr, uint64_t size,
                                                                              uint32_t side, uint32_t tree_type,
                                                                              uint32_t devid)
{
    struct rbtree_root *root = &g_reg_record_tree[side][tree_type][devid];
    struct rbtree_node *tmp = root->rbtree_node;
    struct register_record_node *candidate = NULL;
    struct register_record_node *record_node = NULL;

    if (RB_EMPTY_ROOT(root)) {
        return NULL;
    }

    while (tmp != NULL) {
        record_node = svm_mem_register_get_record_node(tmp);
        if (ptr < record_node->src_ptr) {
            tmp = tmp->rbtree_left;
        } else {
            candidate = record_node;
            tmp = tmp->rbtree_right;
        }
    }

    if (candidate != NULL) {
        if (svm_mem_register_range_is_overlap(ptr, size, candidate)) {
            return candidate;
        }
        struct rbtree_node *next = rbtree_next(&candidate->node.rbtree_node);
        if (next != NULL) {
            record_node = svm_mem_register_get_record_node(next);
            if (svm_mem_register_range_is_overlap(ptr, size, record_node)) {
                return record_node;
            }
        }
    } else {
        struct rbtree_node *first = rbtree_first(root);
        if (first != NULL) {
            record_node = svm_mem_register_get_record_node(first);
            if (svm_mem_register_range_is_overlap(ptr, size, record_node)) {
                return record_node;
            }
        }
    }

    return NULL;
}

static inline struct register_record_node *svm_mem_register_find_record_node(DVdeviceptr ptr, uint32_t side,
                                                                             uint32_t tree_type, uint32_t devid)
{
    return svm_mem_register_find_overlap_record_node(ptr, 1, side, tree_type, devid);
}

static bool _svm_mem_register_record_is_exist(DVdeviceptr ptr, uint64_t size, uint32_t side, uint32_t tree_type,
                                              uint32_t devid)
{
    struct rb_range_handle range = {.start = ptr, .end = ptr + size - 1};

    return !rbtree_can_insert_range(&g_reg_record_tree[side][tree_type][devid], &range,
                                    svm_mem_register_record_get_range);
}

static DVresult _svm_mem_register_record_add(uint32_t devid, DVdeviceptr src_ptr, uint64_t size, uint32_t map_type,
                                             DVdeviceptr dst_ptr, uint32_t flag)
{
    uint32_t side = svm_mem_register_get_mem_side(map_type);
    uint32_t tree_type = svm_mem_register_get_tree_type(map_type);
    struct register_record_node *record_node = NULL;
    uint32_t tree_tmp;

    /* Duplicate check within the same side (cross-tree) */
    for (tree_tmp = NO_DMA_MAP_RECORD_TREE; tree_tmp < RECORD_TREE_MAX_TYPE; tree_tmp++) {
        if (_svm_mem_register_record_is_exist(src_ptr, size, side, tree_tmp, devid)) {
            return DRV_ERROR_BUSY;
        }
    }

    record_node = (struct register_record_node *)malloc(sizeof(struct register_record_node));
    if (record_node == NULL) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    record_node->src_ptr = src_ptr;
    record_node->count = size;
    record_node->map_type = map_type;
    record_node->flag = flag;
    record_node->dst_ptr = dst_ptr;
    record_node->node.key = src_ptr;

    if (rbtree_insert_by_range(&g_reg_record_tree[side][tree_type][devid], &record_node->node.rbtree_node,
                               svm_mem_register_record_get_range) != 0) {
        free(record_node);
        return DRV_ERROR_BUSY;
    }

    DEVMM_DRV_DEBUG_ARG(
        "Add mem register record. (map_type=%u; src_ptr=0x%llx; dst_ptr=0x%llx; count=%lu; side=%u; device=%u)\n",
        map_type, src_ptr, dst_ptr, size, side, devid);

    return DRV_ERROR_NONE;
}

DVresult svm_mem_register_record_add(uint32_t devid, DVdeviceptr src_ptr, uint64_t size, uint32_t map_type,
                                     DVdeviceptr dst_ptr, uint32_t flag)
{
    uint32_t side, tree_tmp;
    DVresult ret;

    if (devid >= REG_RECORD_MAX_DEV_NUM) {
        return DRV_ERROR_INVALID_DEVICE;
    }

    side = svm_mem_register_get_mem_side(map_type);

    /* Lock only the current side's trees to avoid blocking the other side
     * (host and dev may share the same local address space) */
    for (tree_tmp = NO_DMA_MAP_RECORD_TREE; tree_tmp < RECORD_TREE_MAX_TYPE; tree_tmp++) {
        pthread_rwlock_wrlock(&g_reg_record_lock[side][tree_tmp][devid]);
    }

    ret = _svm_mem_register_record_add(devid, src_ptr, size, map_type, dst_ptr, flag);

    for (tree_tmp = NO_DMA_MAP_RECORD_TREE; tree_tmp < RECORD_TREE_MAX_TYPE; tree_tmp++) {
        pthread_rwlock_unlock(&g_reg_record_lock[side][tree_tmp][devid]);
    }

    return ret;
}

static void _svm_mem_register_record_del(DVdeviceptr src_ptr, uint32_t tree_type, uint32_t side, uint32_t devid)
{
    struct register_record_node *record_node = NULL;
    struct rb_node *tmp = NULL;

    tmp = rbtree_get(src_ptr, &g_reg_record_tree[side][tree_type][devid]);
    if (tmp != NULL) {
        record_node = container_of(tmp, struct register_record_node, node);
        rbtree_erase(&g_reg_record_tree[side][tree_type][devid], &record_node->node);
        DEVMM_DRV_DEBUG_ARG("Del mem register record. (ptr=0x%llx; count=%lu; device=%u)\n", record_node->src_ptr,
                            record_node->count, devid);
        free(record_node);
    }
}

void svm_mem_register_record_del(uint32_t devid, DVdeviceptr src_ptr, uint32_t map_type)
{
    uint32_t side, tree_type;

    if (devid >= REG_RECORD_MAX_DEV_NUM) {
        return;
    }

    side = svm_mem_register_get_mem_side(map_type);
    tree_type = svm_mem_register_get_tree_type(map_type);

    pthread_rwlock_wrlock(&g_reg_record_lock[side][tree_type][devid]);
    _svm_mem_register_record_del(src_ptr, tree_type, side, devid);
    pthread_rwlock_unlock(&g_reg_record_lock[side][tree_type][devid]);
}

static void _svm_mem_register_records_del(uint32_t side, uint32_t tree_type, uint32_t devid)
{
    struct register_record_node *record_node = NULL;
    struct rbtree_node *cur = NULL;
    struct rbtree_node *tmp = NULL;

    rbtree_node_for_each_prev_safe(cur, tmp, &g_reg_record_tree[side][tree_type][devid])
    {
        record_node = svm_mem_register_get_record_node(cur);
        rbtree_erase(&g_reg_record_tree[side][tree_type][devid], &record_node->node);
        free(record_node);
    }

    DEVMM_DRV_DEBUG_ARG("Del mem register records. (device=%u; side=%u)\n", devid, side);
}

void svm_mem_register_records_del(uint32_t devid)
{
    uint32_t side, tree_type;

    if (devid >= REG_RECORD_MAX_DEV_NUM) {
        return;
    }

    for (side = HOST_MEM_REGISTER; side < MEM_REGISTER_MAX_SIDE; side++) {
        for (tree_type = NO_DMA_MAP_RECORD_TREE; tree_type < RECORD_TREE_MAX_TYPE; tree_type++) {
            pthread_rwlock_wrlock(&g_reg_record_lock[side][tree_type][devid]);
            _svm_mem_register_records_del(side, tree_type, devid);
            pthread_rwlock_unlock(&g_reg_record_lock[side][tree_type][devid]);
        }
    }
}

static DVresult _svm_mem_register_record_query(DVdeviceptr src_ptr, uint32_t side, uint32_t tree_type, uint32_t devid,
                                               DVdeviceptr *dst_ptr, uint32_t *flag)
{
    struct register_record_node *record_node = NULL;
    struct rb_node *tmp = NULL;

    tmp = rbtree_get(src_ptr, &g_reg_record_tree[side][tree_type][devid]);
    if (tmp == NULL) {
        return DRV_ERROR_NOT_EXIST;
    }

    record_node = container_of(tmp, struct register_record_node, node);
    if (dst_ptr != NULL) {
        *dst_ptr = record_node->dst_ptr;
    }
    if (flag != NULL) {
        *flag = record_node->flag;
    }
    return DRV_ERROR_NONE;
}

DVresult svm_mem_register_record_query(DVdeviceptr src_ptr, uint32_t devid, uint32_t side, DVdeviceptr *dst_ptr,
                                       uint32_t *flag)
{
    uint32_t tree_type = NO_DMA_MAP_RECORD_TREE;
    DVresult ret;

    if ((devid >= REG_RECORD_MAX_DEV_NUM) || (side >= MEM_REGISTER_MAX_SIDE)) {
        return DRV_ERROR_NOT_EXIST;
    }

    for (tree_type = NO_DMA_MAP_RECORD_TREE; tree_type < RECORD_TREE_MAX_TYPE; tree_type++) {
        pthread_rwlock_rdlock(&g_reg_record_lock[side][tree_type][devid]);
        ret = _svm_mem_register_record_query(src_ptr, side, tree_type, devid, dst_ptr, flag);
        pthread_rwlock_unlock(&g_reg_record_lock[side][tree_type][devid]);

        if (ret == DRV_ERROR_NONE) {
            return DRV_ERROR_NONE;
        }
    }

    return ret;
}

static DVresult _svm_mem_register_record_range_query(DVdeviceptr src_ptr, uint32_t side, uint32_t tree_type,
                                                     uint32_t devid, uint32_t *map_type, DVdeviceptr *dst_ptr)
{
    struct register_record_node *record_node = NULL;

    record_node = svm_mem_register_find_record_node(src_ptr, side, tree_type, devid);
    if (record_node == NULL) {
        return DRV_ERROR_NOT_EXIST;
    }

    if (map_type != NULL) {
        *map_type = record_node->map_type;
    }
    if (dst_ptr != NULL) {
        uint64_t offset = src_ptr - record_node->src_ptr;
        *dst_ptr = (record_node->dst_ptr == 0) ? 0 : (record_node->dst_ptr + offset);
    }

    return DRV_ERROR_NONE;
}

DVresult svm_mem_register_record_range_query(DVdeviceptr src_ptr, uint32_t devid, uint32_t side, uint32_t *map_type,
                                             DVdeviceptr *dst_ptr)
{
    uint32_t tree_type = NO_DMA_MAP_RECORD_TREE;
    DVresult ret;

    if ((devid >= REG_RECORD_MAX_DEV_NUM) || (side >= MEM_REGISTER_MAX_SIDE)) {
        return DRV_ERROR_NOT_EXIST;
    }

    for (tree_type = NO_DMA_MAP_RECORD_TREE; tree_type < RECORD_TREE_MAX_TYPE; tree_type++) {
        pthread_rwlock_rdlock(&g_reg_record_lock[side][tree_type][devid]);
        ret = _svm_mem_register_record_range_query(src_ptr, side, tree_type, devid, map_type, dst_ptr);
        pthread_rwlock_unlock(&g_reg_record_lock[side][tree_type][devid]);

        if (ret == DRV_ERROR_NONE) {
            return DRV_ERROR_NONE;
        }
    }

    return ret;
}

DVresult svm_mem_register_record_range_query_single(DVdeviceptr src_ptr, uint32_t devid, uint32_t side,
                                                    uint32_t tree_type, uint32_t *map_type, DVdeviceptr *dst_ptr)
{
    DVresult ret;

    if ((devid >= REG_RECORD_MAX_DEV_NUM) || (side >= MEM_REGISTER_MAX_SIDE) || (tree_type >= RECORD_TREE_MAX_TYPE)) {
        return DRV_ERROR_NOT_EXIST;
    }

    pthread_rwlock_rdlock(&g_reg_record_lock[side][tree_type][devid]);
    ret = _svm_mem_register_record_range_query(src_ptr, side, tree_type, devid, map_type, dst_ptr);
    pthread_rwlock_unlock(&g_reg_record_lock[side][tree_type][devid]);

    return ret;
}

bool svm_mem_register_record_is_exist(DVdeviceptr ptr, uint32_t side)
{
    uint32_t i, j;

    if (side >= MEM_REGISTER_MAX_SIDE) {
        return false;
    }

    for (i = NO_DMA_MAP_RECORD_TREE; i < RECORD_TREE_MAX_TYPE; i++) {
        for (j = 0; j < REG_RECORD_MAX_DEV_NUM; j++) {
            if (!devmm_dev_is_inited(j)) {
                continue;
            }
            pthread_rwlock_rdlock(&g_reg_record_lock[side][i][j]);
            bool is_exist = _svm_mem_register_record_is_exist(ptr, 1, side, i, j);
            pthread_rwlock_unlock(&g_reg_record_lock[side][i][j]);
            if (is_exist) {
                return true;
            }
        }
    }

    return false;
}

static void __attribute__((constructor)) svm_mem_register_record_init(void)
{
    uint32_t i, j, k;

    for (i = HOST_MEM_REGISTER; i < MEM_REGISTER_MAX_SIDE; i++) {
        for (j = NO_DMA_MAP_RECORD_TREE; j < RECORD_TREE_MAX_TYPE; j++) {
            for (k = 0; k < REG_RECORD_MAX_DEV_NUM; k++) {
                rbtree_init(&g_reg_record_tree[i][j][k]);
                pthread_rwlock_init(&g_reg_record_lock[i][j][k], NULL);
            }
        }
    }
}
