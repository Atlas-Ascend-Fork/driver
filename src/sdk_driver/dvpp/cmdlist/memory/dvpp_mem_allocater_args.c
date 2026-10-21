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

#include "dvpp_mem_allocater_args.h"

#include "ka_memory_pub.h"
#include "args_mem_pool.h"
#include "mem_allocater.h"
#include "dvpp_cmdlist_log.h"
#include "dvpp_mem_allocater_comm.h"

uint64_t alloc_args_memory(uint32_t pool_id, uint32_t args_size, dvpp_sqe_args *sqe_args)
{
    enum dvpp_sqe_type type;
    uint64_t args_addr = 0;
    struct mem_node *node = NULL;
    if (args_size == 0) {
        DVPP_CMDLIST_LOG_ERROR("args_size = %u\n", args_size);
        return args_addr;
    }

    if (sqe_args->is_use_mem_pool == 0) {
        // btach场景下动态申请内存
        args_addr = (uint64_t)(uintptr_t)ka_mm_vmalloc(args_size);
        if (args_addr == 0) {
            DVPP_CMDLIST_LOG_ERROR("malloc batch args buf fail, pool_id = %u, size = %u\n", pool_id, args_size);
        }

        return args_addr;
    }

    // 校验pool id是否合法
    if (check_pool_id(pool_id) != 0) {
        return 0;
    }

    // 非batch场景使用内存池
    type = (enum dvpp_sqe_type)(sqe_args->sqe_type);
    DVPP_CMDLIST_LOG_DEBUG("alloc args node, pool_id = %u, type(%u).\n", pool_id, type);

    // 依据不同的type，获取不同模块内存池的node
    node = alloc_node(type, ARGS_MEM_NODE, pool_id, DVPP_DEFAULT_MEM_BLK_SIZE);
    if (node == NULL) { // 检查获取到的node是否为空
        DVPP_CMDLIST_LOG_ERROR("alloc args node fail, pool_id = %u, size = %u, type= %u\n", pool_id,
                               DVPP_DEFAULT_MEM_BLK_SIZE, type);
        return 0;
    }

    // 申请到args node，并将信息赋值给sqe_args
    args_addr = (uint64_t)(uintptr_t)(node->vaddr);
    sqe_args->args_node = (uint64_t)(uintptr_t)(node);

    return args_addr;
}

void free_args_memory(uint32_t pool_id, dvpp_sqe_args *sqe_args, uint64_t args_addr)
{
    enum dvpp_sqe_type type;
    if (sqe_args->is_use_mem_pool == 0) {
        // btach场景下动态释放内存
        if (args_addr != 0) {
            ka_mm_vfree((void *)args_addr);
        }
        return;
    }

    // 校验pool id是否合法
    if (check_pool_id(pool_id) != 0) {
        return;
    }

    // 非batch场景归还节点到内存池
    if (sqe_args->args_node == 0) {
        DVPP_CMDLIST_LOG_ERROR("free args node fail, pool_id = %u, node is NULL\n", pool_id);
        return;
    }

    // 从sqe中获取type，当前type仅支持下面三个模块
    type = (enum dvpp_sqe_type)(sqe_args->sqe_type);
    DVPP_CMDLIST_LOG_DEBUG("free args node, pool_id = %u, type(%u).\n", pool_id, type);
    // 依据不同的type，归还node到不同模块内存池
    free_node(type, ARGS_MEM_NODE, pool_id, (struct mem_node *)(sqe_args->args_node));

    // 重置sqe_args中的args_node
    sqe_args->args_node = 0;

    return;
}
