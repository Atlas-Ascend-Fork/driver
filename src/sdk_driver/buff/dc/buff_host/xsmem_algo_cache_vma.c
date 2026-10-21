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

#define pr_fmt(fmt) "XSMEM_CACHE_VMA: <%s:%d> " fmt, __func__, __LINE__

#include "xsmem_framework_log.h"
#include "xsmem_framework.h"
#include "xsmem_algo_vma.h"
#include "ka_barrier_pub.h"
#include "xsmem_algo_cache_vma.h"

struct cache_vma_ctrl {
    unsigned int cache_alloc_flag;
    unsigned long long addr;
    unsigned long long size;
    void *vma_ctrl;
};

static int cache_vma_algo_pool_init(struct xsm_pool *xp, struct xsm_reg_arg *arg)
{
    struct cache_vma_ctrl *cache_ctrl = NULL;
    void *vma_ctrl = NULL;

    cache_ctrl = xsmem_drv_kmalloc(sizeof(struct cache_vma_ctrl), KA_GFP_KERNEL | __KA_GFP_ACCOUNT | __KA_GFP_ZERO);
    if (cache_ctrl == NULL) {
        xsmem_err("Alloc memory for cache_vma_ctrl failed.\n");
        return -ENOMEM;
    }

    vma_ctrl = vma_inst_create(arg->pool_size);
    if (vma_ctrl == NULL) {
        xsmem_drv_kfree(cache_ctrl);
        return -EINVAL;
    }

    cache_ctrl->vma_ctrl = vma_ctrl;
    xp->private = (void *)cache_ctrl;
    return 0;
}

static int cache_vma_algo_pool_free(struct xsm_pool *xp)
{
    struct cache_vma_ctrl *cache_ctrl = (struct cache_vma_ctrl *)xp->private;
    vma_inst_destroy(cache_ctrl->vma_ctrl);
    xsmem_drv_kfree(cache_ctrl);
    xp->private = NULL;
    return 0;
}

static int cache_vma_algo_cache_create(struct xsm_pool *xp, struct xsm_cache_create_arg *arg)
{
    struct cache_vma_ctrl *cache_ctrl = (struct cache_vma_ctrl *)xp->private;
    if (arg->mem_size == 0) {
        xsmem_err("Invalid mem_size (mem_size=0x%llu)\n", arg->mem_size);
        return -EINVAL;
    }
    if (cache_ctrl->cache_alloc_flag != 0) {
        xsmem_err("Cache already created. \n");
        return -EEXIST;
    }
    cache_ctrl->size = arg->mem_size;
    ka_smp_wmb();
    cache_ctrl->cache_alloc_flag = 1;

    xsmem_info("Vma cache create. (size=0x%llx)\n", cache_ctrl->size);
    return 0;
}

static int cache_vma_algo_cache_query(struct xsm_pool *xp, unsigned int dev_id, GrpQueryGroupAddrInfo *cache_buff,
                                      unsigned int *cache_cnt)
{
    struct cache_vma_ctrl *cache_ctrl = (struct cache_vma_ctrl *)xp->private;

    if (cache_ctrl->cache_alloc_flag == 0) {
        *cache_cnt = 0;
        return 0;
    }
    cache_buff[0].size = cache_ctrl->size;
    cache_buff[0].addr = 0;
    *cache_cnt = 1;
    return 0;
}

static void cache_vma_algo_pool_show(struct xsm_pool *xp, ka_seq_file_t *seq)
{
    struct cache_vma_ctrl *cache_ctrl = (struct cache_vma_ctrl *)xp->private;
    vma_algo_show(cache_ctrl->vma_ctrl, seq);
}

static int cache_vma_algo_block_alloc(struct xsm_pool *xp, struct xsm_block *blk)
{
    struct cache_vma_ctrl *cache_ctrl = (struct cache_vma_ctrl *)xp->private;

    if (cache_ctrl->cache_alloc_flag == 0) {
        xsmem_err("No cache memory.\n");
        return -ENOSPC;
    }

    return vma_algo_alloc(cache_ctrl->vma_ctrl, blk->alloc_size, &blk->offset, &blk->real_size);
}

static int cache_vma_algo_block_free(struct xsm_pool *xp, struct xsm_block *blk)
{
    struct cache_vma_ctrl *cache_ctrl = (struct cache_vma_ctrl *)xp->private;
    return vma_algo_free(cache_ctrl->vma_ctrl, blk->offset, blk->real_size);
}

static struct xsm_pool_algo cache_vma_algo = {
    .num = XSMEM_ALGO_CACHE_VMA,
    .name = "cache_vma_algo",
    .xsm_pool_init = cache_vma_algo_pool_init,
    .xsm_pool_free = cache_vma_algo_pool_free,
    .xsm_pool_cache_create = cache_vma_algo_cache_create,
    .xsm_pool_cache_query = cache_vma_algo_cache_query,
    .xsm_pool_show = cache_vma_algo_pool_show,
    .xsm_block_alloc = cache_vma_algo_block_alloc,
    .xsm_block_free = cache_vma_algo_block_free,
};

struct xsm_pool_algo *xsm_get_cache_vma_algo(void)
{
    return &cache_vma_algo;
}