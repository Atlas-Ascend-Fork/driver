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

#include "dvpp_mem_allocater_dma.h"

#include "ka_memory_pub.h"
#include "mem_allocater.h"
#include "args_mem_pool.h"
#include "dma_mem_pool.h"
#include "dvpp_cmdlist_log.h"
#include "dvpp_mem_allocater_comm.h"

static uint32_t get_args_node_size(uint32_t pool_id, enum dvpp_sqe_type type)
{
    uint32_t size = 0;
    struct mem_pool_info *mem_pool = NULL;
    // 校验pool id是否合法
    if (check_pool_id(pool_id) != 0) {
        return 0;
    }

    // 依据type和pool_id获取args内存池
    mem_pool = get_mem_pool(type, ARGS_MEM_NODE, pool_id);
    if (mem_pool != NULL) {
        size = mem_pool->node_size;
    }

    return size;
}

int32_t alloc_dma_memory(uint32_t pool_id, dvpp_sqe_args *sqe_args, uint32_t size, dvpp_dma_mem *dma_mem)
{
    enum dvpp_sqe_type type;
    struct mem_node *node = NULL;
    uint32_t args_node_size = 0;
    // 判断是否是batch场景
    if (sqe_args->is_use_mem_pool == 0) {
        // btach场景下动态申请内存
        dma_mem->vir_addr = (uint64_t)(uintptr_t)dvpp_dma_alloc(pool_id, size, &dma_mem->phy_addr, KA_GFP_ATOMIC);
        if (dma_mem->vir_addr == 0) {
            DVPP_CMDLIST_LOG_ERROR("alloc host dma memory fail, pool_id = %u, size:%d\n", pool_id,
                                   sqe_args->cmdbuf_size);
            return -1;
        }

        return 0;
    }

    // 校验pool id是否合法
    if (check_pool_id(pool_id) != 0) {
        return -1;
    }

    // 非batch场景使用内存池
    type = (enum dvpp_sqe_type)(sqe_args->sqe_type);
    DVPP_CMDLIST_LOG_DEBUG("alloc dma node, pool_id = %u, type(%u).\n", pool_id, type);
    // 依据不同的type，获取不同模块内存池的node
    node = alloc_node(type, DMA_MEM_NODE, pool_id, DVPP_DEFAULT_DMA_MEM_BLK_SIZE);
    if (node == NULL) { // 检验获取的node合法性
        DVPP_CMDLIST_LOG_ERROR("alloc dma node fail, pool_id = %u, type= %u\n", pool_id, type);
        return -1;
    }

    // 申请到dma node，并将信息赋值给sqe_args
    dma_mem->vir_addr = (uint64_t)(uintptr_t)(node->vaddr);
    dma_mem->phy_addr = node->paddr;
    // 获取每个args node的内存大小
    args_node_size = get_args_node_size(pool_id, type);
    *(uint64_t *)(((struct mem_node *)(sqe_args->args_node))->vaddr +
                  (args_node_size - 8)) = (uint64_t)(uintptr_t)(node); // 8

    return 0;
}

void free_dma_memory(uint32_t pool_id, dvpp_sqe_args *sqe_args, dvpp_dma_mem *dma_mem)
{
    enum dvpp_sqe_type type;
    uint32_t args_node_size = 0;
    uint64_t dma_node = 0;
    // 依据flag判断是否是batch场景
    if (sqe_args->is_use_mem_pool == 0) {
        // btach场景下动态释放内存
        if (dma_mem->vir_addr != 0) {
            dvpp_dma_free(pool_id, sqe_args->cmdbuf_size, (void *)(dma_mem->vir_addr), dma_mem->phy_addr);
        }
        return;
    }

    // 校验pool id是否合法
    if (check_pool_id(pool_id) != 0) {
        return;
    }

    // 从sqe中获取type，当前type仅支持下面三个模块
    type = (enum dvpp_sqe_type)(sqe_args->sqe_type);

    // 非batch场景归还节点到内存池
    // 获取每个args node的内存大小
    args_node_size = get_args_node_size(pool_id, type);
    if (args_node_size == 0) {
        DVPP_CMDLIST_LOG_ERROR("pool_id = %u, args node size is 0\n", pool_id);
        return;
    }

    // 检验args_node和vaddr是否为空指针
    if ((sqe_args->args_node == 0) || ((struct mem_node *)(sqe_args->args_node))->vaddr == NULL) {
        DVPP_CMDLIST_LOG_ERROR("pool_id = %u, args_node or vaddr is NULL\n", pool_id);
        return;
    }

    dma_node = *(uint64_t *)(((struct mem_node *)(sqe_args->args_node))->vaddr + (args_node_size - 8)); // 8
    if (dma_node == 0) {
        DVPP_CMDLIST_LOG_ERROR("free dma node fail, pool_id = %u, node is NULL\n", pool_id);
        return;
    }

    DVPP_CMDLIST_LOG_DEBUG("free dma node, pool_id = %u, type(%u).\n", pool_id, type);
    // 依据不同的type，归还node到不同模块内存池
    free_node(type, DMA_MEM_NODE, pool_id, (struct mem_node *)dma_node);

    *(uint64_t *)(((struct mem_node *)(sqe_args->args_node))->vaddr + (args_node_size - 8)) = 0; // 8

    return;
}
