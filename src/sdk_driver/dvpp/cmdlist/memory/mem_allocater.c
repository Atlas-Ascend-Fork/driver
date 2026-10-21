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
#include "mem_allocater.h"

#include "ka_barrier_pub.h"
#include "ka_memory_pub.h"
#include "securec.h"
#include "dvpp_cmdlist_log.h"

void create_mem_pool(struct mem_pool_info **mem_pool, ka_mutex_t *mtx)
{
    if (*mem_pool != NULL) {
        return;
    }
    ka_task_mutex_lock(mtx); // mtx为全局锁，避免pool重复创建
    if (*mem_pool == NULL) {
        struct mem_pool_info *mp = (struct mem_pool_info *)ka_mm_vmalloc(sizeof(struct mem_pool_info));
        if (mp == NULL) {
            ka_task_mutex_unlock(mtx);
            return;
        }
        (void)memset_s(mp, sizeof(struct mem_pool_info), 0, sizeof(struct mem_pool_info));
        ka_barrier(); // 防止编译器优化
        ka_mb();      // 防止CPU乱序执行
        *mem_pool = mp;
    }
    ka_task_mutex_unlock(mtx);
}

int32_t init_mem_allocater(init func, struct mem_pool_info *mem_pool, uint32_t node_num, uint32_t node_size,
                           ka_mutex_t *mtx)
{
    int32_t ret = 0;
    if (ka_smp_load_acquire(&mem_pool->is_inited) == 1) {
        return ret;
    }
    ka_task_mutex_lock(mtx); // mtx为全局锁，避免pool重复初始化

    if (mem_pool->is_inited == 0) {
        // 赋初值
        mem_pool->mem_node_num = node_num;
        mem_pool->node_size = node_size;
        mem_pool->base_nodes_addr = NULL;
        mem_pool->mem_size = mem_pool->mem_node_num * node_size;
        mem_pool->base_vaddr = NULL;

        DVPP_CMDLIST_LOG_DEBUG("init mem_pool, mem_node_num %u, node_size %u\n", mem_pool->mem_node_num,
                               mem_pool->node_size);
        // 分配地址和初始化链表
        ret = (*func)(mem_pool);
        if (ret == 0) {
            // 初始化jpegd pool中的锁和信号量，并更改初始化状态
            ka_task_mutex_init(&mem_pool->mtx);
            ka_task_sema_init(&mem_pool->sem, mem_pool->mem_node_num);
            ka_barrier(); // 防止编译器优化，将is_inited先赋值1
            ka_mb();      // 防止CPU乱序执行，将is_inited先赋值1
            mem_pool->is_inited = 1;
        }
    }

    ka_task_mutex_unlock(mtx);
    return ret;
}

static void deinit_mem_allocater(deinit func, struct mem_pool_info *mem_pool)
{
    if (mem_pool->is_inited == 1) {
        // 销毁内存池子
        (*func)(mem_pool);
        mem_pool->is_inited = 0;
    }
}

void destroy_mem_pool(deinit func, struct mem_pool_info **mem_pool, ka_mutex_t *mtx)
{
    if (*mem_pool == NULL) {
        return;
    }
    ka_task_mutex_lock(mtx); // mtx为全局锁，避免pool重复销毁，pool使用时可以不用锁保护
                             // 由容器运行时不能删除已绑定的虚拟设备，因此pool使用时不会被销毁
    if (*mem_pool != NULL) {
        deinit_mem_allocater(func, *mem_pool);
        ka_mm_vfree(*mem_pool);
        *mem_pool = NULL;
    }
    ka_task_mutex_unlock(mtx);
}

struct mem_node *alloc_node_from_pool(struct mem_pool_info *mem_pool)
{
    struct mem_node *pos;

    // 参数校验
    if (mem_pool == NULL) {
        DVPP_CMDLIST_LOG_ERROR("mem_pool is null\n");
        return NULL;
    }

    // 等待池子中有可用的node节点
    if (ka_task_down_interruptible(&mem_pool->sem) != 0) {
        DVPP_CMDLIST_LOG_ERROR("waiting for mem_pool->sem fail, interrupted by signal\n");
        return NULL;
    }

    // 加锁
    ka_task_mutex_lock(&mem_pool->mtx);

    // 从链表中获取一个node节点
    pos = NULL;
    ka_list_for_each_entry(pos, &mem_pool->node_list.list, list)
    {
        if (pos->vaddr != NULL) {
            ka_list_del_init(&pos->list);
            break;
        }
    }

    ka_task_mutex_unlock(&mem_pool->mtx);
    DVPP_CMDLIST_LOG_DEBUG("alloc node success.\n");
    return pos;
}

void free_node_to_pool(struct mem_node *node, struct mem_pool_info *mem_pool)
{
    // 参数校验
    if ((node == NULL) || (node->vaddr == NULL) || (mem_pool == NULL)) {
        DVPP_CMDLIST_LOG_ERROR("node or vaddr or mem_pool is null\n");
        return;
    }

    // 归还节点node到池子pool
    ka_task_mutex_lock(&mem_pool->mtx);
    ka_list_add_tail(&node->list, &mem_pool->node_list.list);
    ka_task_mutex_unlock(&mem_pool->mtx);
    DVPP_CMDLIST_LOG_DEBUG("free node success.\n");
    ka_task_up(&mem_pool->sem);
}
