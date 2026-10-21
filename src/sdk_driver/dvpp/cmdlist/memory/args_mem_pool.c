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

#include "args_mem_pool.h"

#include "ka_memory_pub.h"

#include "dvpp_cmdlist_log.h"

int32_t init_args_pool(struct mem_pool_info *args_pool)
{
    uint32_t i;
    uint32_t total_node_size = 0;
    uint32_t node_mem_size = 0;

    if (args_pool == NULL) {
        DVPP_CMDLIST_LOG_ERROR("args_pool is null\n");
        return -1;
    }

    total_node_size = sizeof(struct mem_node) * args_pool->mem_node_num;
    if ((total_node_size == 0) || (args_pool->mem_size == 0)) {
        DVPP_CMDLIST_LOG_ERROR("args_pool total_node_size = %u, mem_node_num = %u, mem_size = %u\n", total_node_size,
                               args_pool->mem_node_num, args_pool->mem_size);
        return -1;
    }

    // 初始化内存池链表
    KA_INIT_LIST_HEAD(&args_pool->node_list.list);

    // 申请内存池，并记录基地址，用于后续统一释放
    args_pool->base_vaddr = ka_mm_vmalloc(args_pool->mem_size);
    if (args_pool->base_vaddr == NULL) {
        DVPP_CMDLIST_LOG_ERROR("alloc args mem fail. mem_size:%u\n", args_pool->mem_size);
        return -1;
    }

    // 申请链表nodes，并记录基地址，用于后续统一释放
    args_pool->base_nodes_addr = ka_mm_vmalloc(sizeof(struct mem_node) * args_pool->mem_node_num);
    if (args_pool->base_nodes_addr == NULL) {
        DVPP_CMDLIST_LOG_ERROR("alloc args nodes fail. args_nodes_size = %lu\n",
                               (args_pool->mem_node_num * sizeof(struct mem_node)));
        ka_mm_vfree(args_pool->base_vaddr);
        args_pool->base_vaddr = NULL;
        return -1;
    }

    // 计算每个args node的内存大小
    node_mem_size = args_pool->mem_size / args_pool->mem_node_num;
    // 依据链表节点个数，组建链表
    for (i = 0; i < args_pool->mem_node_num; ++i) {
        struct mem_node *args_node = (struct mem_node *)(args_pool->base_nodes_addr + (i * sizeof(struct mem_node)));
        args_node->vaddr = args_pool->base_vaddr + (i * node_mem_size);
        ka_list_add_tail(&args_node->list, &args_pool->node_list.list);
    }

    return 0;
}

void deinit_args_pool(struct mem_pool_info *args_pool)
{
    if (args_pool == NULL) {
        DVPP_CMDLIST_LOG_ERROR("args_pool is null.\n");
        return;
    }

    // 恢复内存池链表到初始状态
    KA_INIT_LIST_HEAD(&args_pool->node_list.list);
    // 释放链表nodes的基地址
    if (args_pool->base_nodes_addr != NULL) {
        ka_mm_vfree(args_pool->base_nodes_addr);
        args_pool->base_nodes_addr = NULL;
    }

    // 释放内存池的基地址
    if (args_pool->base_vaddr != NULL) {
        ka_mm_vfree(args_pool->base_vaddr);
        args_pool->base_vaddr = NULL;
    }
}
