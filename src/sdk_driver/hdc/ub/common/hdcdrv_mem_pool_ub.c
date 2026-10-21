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

#include "kernel_version_adapt.h"
#include "hdcdrv_core_com_ub.h"
#include "hdcdrv_cmd.h"
#include "hdcdrv_core_ub.h"
#include "hdcdrv_proc_fs_ub.h"
#include "hdcdrv_mem_pool_ub.h"

int hdcdrv_init_dev_mem_pool(struct hdcdrv_dev *hdc_dev)
{
    int i, page_idx;
    u32 power = (u32)ka_mm_get_order(HDCDRV_UB_MEM_POOL_LEN);
    ka_gfp_t gfp_mask = KA_GFP_NOWAIT | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT;

    for (i = 0; i < HDCDRV_MEM_POOL_NUM; i++) {
        hdc_dev->mem_pool_list[i].pool.page = hdcdrv_ub_alloc_pages_node(hdc_dev->dev_id, gfp_mask, power);
        if (hdc_dev->mem_pool_list[i].pool.page == NULL) {
            page_idx = i;
            hdcdrv_err("Calling kcalloc rx_pool failed. (dev_id=%d; index=%d)\n", hdc_dev->dev_id, i);
            goto hdc_mem_pool_init_fail;
        }

        hdc_dev->mem_pool_list[i].ctx = NULL;
        hdc_dev->mem_pool_list[i].valid = HDCDRV_MEM_POOL_IDLE;
    }

    return 0;

hdc_mem_pool_init_fail:
    for (i = 0; i < page_idx; i++) {
        hdcdrv_free_pages_ex(hdc_dev->mem_pool_list[i].pool.page, power, KA_SUB_MODULE_TYPE_2);
        hdc_dev->mem_pool_list[i].pool.page = NULL;
    }

    return HDCDRV_DMA_MEM_ALLOC_FAIL;
}

// Before the physical memory is released, the mapping between the page and the user-mode VA needs to be removed.
void hdcdrv_uninit_dev_mem_pool(struct hdcdrv_dev *hdc_dev)
{
    int i, ret;
    u32 power = (u32)ka_mm_get_order(HDCDRV_UB_MEM_POOL_LEN);
    struct hdcdrv_mem_pool_list_node *mem_pool = NULL;
    struct hdcdrv_ctx *pool_ctx = NULL;

    // if the mem pool is still in used, need to unmap first
    for (i = 0; i < HDCDRV_MEM_POOL_NUM; i++) {
        mem_pool = &hdc_dev->mem_pool_list[i];
        // If uninit_dev_mem_pool enter and pool is not used, exit with no operation
        if (mem_pool->valid == HDCDRV_MEM_POOL_BUSY) {
            ret = hdcdrv_unmap_mem_pool_va(mem_pool->ctx, &mem_pool->pool, hdc_dev->dev_id);
            if (ret != HDCDRV_OK) {
                hdcdrv_err("Calling hdcdrv_unmap_mem_pool_va for tx_pool failed. (dev_id=%d; ret=%d)\n",
                           hdc_dev->dev_id, ret);
                // unmap failed, this page will not free until process release
                continue;
            }
        }
        mem_pool->valid = HDCDRV_MEM_POOL_DISABLE;
        if (mem_pool->ctx != NULL) {
            pool_ctx = (struct hdcdrv_ctx *)mem_pool->ctx;
            pool_ctx->pool_info.pool_page = NULL;
            pool_ctx->pool_info.dev_ref = 0;
            pool_ctx->pool_info.idx = 0;
        }
        mem_pool->ctx = NULL;
        hdcdrv_free_pages_ex(mem_pool->pool.page, power, KA_SUB_MODULE_TYPE_2);
        mem_pool->pool.page = NULL;
    }

    return;
}

int hdcdrv_alloc_mem_pool_for_session(struct hdcdrv_session *session, struct hdcdrv_dev *hdc_dev,
                                      struct hdcdrv_ctx *ctx, u64 user_va)
{
    int i, ret, idx;
    struct hdcdrv_mem_pool_list_node *mem_pool = NULL;

    // Only service dmp can use kernel mem pool
    if (!hdcdrv_use_kernel_mem_pool(session->service_type)) {
        return 0;
    }

    ka_task_mutex_lock(&g_hdc_ctrl.dev_lock[hdc_dev->dev_id]);
    if (hdc_dev->valid == HDCDRV_INVALID) {
        ka_task_mutex_unlock(&g_hdc_ctrl.dev_lock[hdc_dev->dev_id]);
        return -HDCDRV_DEVICE_NOT_READY;
    }

    for (i = 0; i < HDCDRV_MEM_POOL_NUM; i++) {
        if (hdc_dev->mem_pool_list[i].valid == HDCDRV_MEM_POOL_IDLE) {
            hdc_dev->mem_pool_list[i].valid = HDCDRV_MEM_POOL_BUSY;
            mem_pool = &hdc_dev->mem_pool_list[i];
            idx = i;
            break;
        }
    }

    if (mem_pool == NULL) {
#ifndef DRV_UT
        hdcdrv_warn("mem pool resources exhausted. (devid=%d)\n", hdc_dev->dev_id);
#endif
        ret = -HDCDRV_DMA_MEM_ALLOC_FAIL;
        goto no_mem_pool;
    }

    ret = hdcdrv_remap_mem_pool_va((void *)ctx, &mem_pool->pool, hdc_dev->dev_id, user_va);
    if (ret != 0) {
        hdcdrv_err("Calling remap_mem_pool_va for tx_pool failed. (dev_id=%d; ret=%d)\n", hdc_dev->dev_id, ret);
        goto pool_map_fail;
    }

    mem_pool->ctx = ctx;
    mem_pool->session_id = session->local_session_fd;
    mem_pool->pool.pid = session->owner_pid;
    mem_pool->pool.vnr = ka_task_tgid_vnr(ka_task_get_current());
    session->mem_pool_idx = idx;
    ctx->pool_info.pool_page = mem_pool->pool.page;
    ctx->pool_info.dev_ref = g_hdc_ctrl.dev_ref[hdc_dev->dev_id];
    ctx->pool_info.idx = idx;
    ka_task_mutex_unlock(&g_hdc_ctrl.dev_lock[hdc_dev->dev_id]);

    return 0;

pool_map_fail:
    mem_pool->valid = HDCDRV_MEM_POOL_IDLE;

no_mem_pool:
    ka_task_mutex_unlock(&g_hdc_ctrl.dev_lock[hdc_dev->dev_id]);

    return ret;
}

int hdcdrv_free_mem_pool_for_session(struct hdcdrv_session *session, struct hdcdrv_dev *hdc_dev, struct hdcdrv_ctx *ctx)
{
    int idx = session->mem_pool_idx;
    int ret;
    struct hdcdrv_mem_pool_list_node *mem_pool = &hdc_dev->mem_pool_list[idx];
    struct hdcdrv_ctx *pool_ctx = NULL;

    ka_task_mutex_lock(&g_hdc_ctrl.dev_lock[hdc_dev->dev_id]);
    ret = hdcdrv_unmap_mem_pool_va(ctx, &mem_pool->pool, hdc_dev->dev_id);
    if (ret != 0) {
        hdcdrv_err("Calling hdcdrv_unmap_mem_pool_va for tx_pool failed. (dev_id=%d; ret=%d)\n", hdc_dev->dev_id, ret);
        ka_task_mutex_unlock(&g_hdc_ctrl.dev_lock[hdc_dev->dev_id]);
        // unmap failed, mem_pool not release until process release
        return HDCDRV_VA_UNMAP_FAILED;
    }

    session->mem_pool_idx = 0;
    mem_pool->pool.pid = 0;
    mem_pool->pool.vnr = 0;
    if (mem_pool->ctx != NULL) {
        pool_ctx = (struct hdcdrv_ctx *)mem_pool->ctx;
        pool_ctx->pool_info.pool_page = NULL;
        pool_ctx->pool_info.dev_ref = 0;
        pool_ctx->pool_info.idx = 0;
    }
    mem_pool->ctx = NULL;
    mem_pool->session_id = 0;
    mem_pool->valid = HDCDRV_MEM_POOL_IDLE;
    ctx->pool_info.pool_page = NULL;
    ka_task_mutex_unlock(&g_hdc_ctrl.dev_lock[hdc_dev->dev_id]);

    return 0;
}

STATIC void hdcdrv_ub_clear_page(struct hdcdrv_ctx *ctx)
{
    struct hdcdrv_dev *hdc_dev;

    if ((ctx->dev_id >= HDCDRV_SUPPORT_MAX_DEV) || (ctx->dev_id < 0)) {
        return;
    }

    ka_task_mutex_lock(&g_hdc_ctrl.dev_lock[ctx->dev_id]);
    hdc_dev = g_hdc_ctrl.dev[ctx->dev_id];
    if (ctx->pool_info.pool_page != NULL) {
        if ((hdc_dev != NULL) && (ctx->pool_info.dev_ref == g_hdc_ctrl.dev_ref[ctx->dev_id])) {
            // Dev is in uninit but not free, just make mem_pool status is idle
            hdc_dev->mem_pool_list[ctx->pool_info.idx].ctx = NULL;
            hdc_dev->mem_pool_list[ctx->pool_info.idx].valid = HDCDRV_MEM_POOL_IDLE;
        } else {
            // Dev has been uninited, this page should be free by fd release
            hdcdrv_free_pages_ex(ctx->pool_info.pool_page, (u32)ka_mm_get_order(HDCDRV_UB_MEM_POOL_LEN),
                                 KA_SUB_MODULE_TYPE_2);
            ctx->pool_info.pool_page = NULL;
        }
    }
    ka_task_mutex_unlock(&g_hdc_ctrl.dev_lock[ctx->dev_id]);
}

// only used for release
void hdcdrv_clear_mem_pool_by_ctx(struct hdcdrv_ctx *ctx)
{
    struct hdcdrv_dev *hdc_dev = NULL;
    int i;
    struct hdcdrv_mem_pool_list_node *mem_pool = NULL;

    // mem_pool has been returned to dev, no need to clear or free
    if (ctx->pool_info.pool_page == NULL) {
        return;
    }

    hdc_dev = hdcdrv_ub_get_dev(ctx->dev_id);
    if (hdc_dev == NULL) {
        hdcdrv_ub_clear_page(ctx);
        return;
    }

    /* All page tables are cleared during release. Therefore, the demapping function does not need to be invoked in
     * release. */
    ka_task_mutex_lock(&g_hdc_ctrl.dev_lock[ctx->dev_id]);
    if (ctx->pool_info.dev_ref != g_hdc_ctrl.dev_ref[ctx->dev_id]) {
        ka_task_mutex_unlock(&g_hdc_ctrl.dev_lock[ctx->dev_id]);
        hdcdrv_put_dev(ctx->dev_id);
        hdcdrv_ub_clear_page(ctx);
        return;
    }

    for (i = 0; i < HDCDRV_MEM_POOL_NUM; i++) {
        mem_pool = &hdc_dev->mem_pool_list[i];
        if (mem_pool->ctx == ctx) {
            hdcdrv_info("process released but mem_pool not released, force release mem_pool."
                        " (dev_id=%d; idx=%d; session=%u)\n",
                        hdc_dev->dev_id, i, hdc_dev->mem_pool_list[i].session_id);
            mem_pool->ctx = NULL;
            mem_pool->session_id = 0;
            mem_pool->pool.pid = 0;
            mem_pool->pool.vnr = 0;
            mem_pool->valid = HDCDRV_MEM_POOL_IDLE;
        }
    }
    ctx->pool_info.pool_page = NULL;
    ka_task_mutex_unlock(&g_hdc_ctrl.dev_lock[ctx->dev_id]);
    hdcdrv_put_dev(ctx->dev_id);
    return;
}