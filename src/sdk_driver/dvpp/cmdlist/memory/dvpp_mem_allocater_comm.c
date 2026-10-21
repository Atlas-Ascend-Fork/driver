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

#include "dvpp_mem_allocater_comm.h"

#include "ka_task_pub.h"
#include "ka_barrier_pub.h"
#include "mem_allocater.h"
#include "dvpp_cmdlist_log.h"

static KA_TASK_DEFINE_MUTEX(g_jpegd_mutex);
static KA_TASK_DEFINE_MUTEX(g_jpege_mutex);
static KA_TASK_DEFINE_MUTEX(g_vpc_mutex);

ka_mutex_t *get_mutex(enum dvpp_sqe_type mod_type)
{
    // 获取不同模块的锁
    switch (mod_type) {
        case DVPP_SQE_VPC:
            return &g_vpc_mutex;

        case DVPP_SQE_JPEGE:
            return &g_jpege_mutex;

        case DVPP_SQE_JPEGD:
            return &g_jpegd_mutex;

        default:
            DVPP_CMDLIST_LOG_ERROR("get mutex fail, mod_type = %d not support.\n", mod_type);
            return NULL;
    }
}

void *alloc_node(enum dvpp_sqe_type mod_type, enum mem_node_type node_type, uint32_t pool_id, uint32_t size)
{
    init func = NULL;
    uint32_t node_num = 0;
    ka_mutex_t *mtx = NULL;
    struct mem_pool_info *mod_mem_pool = get_mem_pool(mod_type, node_type, pool_id);
    // 校验是否为空
    if (mod_mem_pool == NULL) {
        return NULL;
    }

    // 内存池已经初始化过，直接返回池中节点
    if (ka_smp_load_acquire(&mod_mem_pool->is_inited) == 1) {
        return alloc_node_from_pool(mod_mem_pool);
    }

    // 获取初始化接口
    func = get_init_func(node_type);
    if (mod_mem_pool == NULL) {
        return NULL;
    }

    // 获取内存池节点个数
    node_num = get_node_num(mod_type);
    if (node_num == 0) {
        return NULL;
    }

    // 获取内存池锁
    mtx = get_mutex(mod_type);
    if (mtx == NULL) {
        return NULL;
    }

    // 首个node申请，初始化allocater
    mod_mem_pool->pool_id = pool_id;
    DVPP_CMDLIST_LOG_DEBUG("init_mem_allocater, mod_type %d, node_type %u, node_num %u, size %u\n", mod_type, node_type,
                           node_num, size);
    if (init_mem_allocater(func, mod_mem_pool, node_num, size, mtx) != 0) {
        return NULL;
    }

    // 返回池中节点
    return alloc_node_from_pool(mod_mem_pool);
}

void free_node(enum dvpp_sqe_type mod_type, enum mem_node_type node_type, uint32_t pool_id, struct mem_node *node)
{
    // 获取内存池
    struct mem_pool_info *mod_mem_pool = get_mem_pool(mod_type, node_type, pool_id);
    // 校验是否为空
    if (mod_mem_pool == NULL) {
        DVPP_CMDLIST_LOG_ERROR("free node fail, mod_type = %d, node_type = %d, pool_id = %u.\n", mod_type, node_type,
                               pool_id);
        return;
    }

    // 内存池未初始化，报错返回
    if (mod_mem_pool->is_inited == 0) {
        DVPP_CMDLIST_LOG_ERROR("mem_pool[%u] not be inited\n", pool_id);
        return;
    }

    // 归还args节点到内存池
    free_node_to_pool(node, mod_mem_pool);
}
