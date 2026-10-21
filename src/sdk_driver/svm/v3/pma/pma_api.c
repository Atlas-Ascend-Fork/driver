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
#include "ka_kernel_def_pub.h"
#include "ka_common_pub.h"
#include "ka_task_pub.h"
#include "pbl_uda.h"

#include "svm_kern_log.h"
#include "svm_pub.h"
#include "svm_addr_desc.h"
#include "va_query.h"
#include "ksvmm.h"
#include "svm_smp.h"
#include "pmq_client.h"
#include "pma_ctx.h"
#include "pma_core.h"

static int pma_p2p_pages_get(int master_tgid, u64 va, u64 size, void (*free_callback)(void *data), void *data,
                             struct p2p_page_table **page_table)
{
    struct pma_mem_node *mem_node = NULL;
    struct pma_ctx *ctx = NULL;
    u32 udevid;
    int ret;

    ret = svm_query_va_udevid(master_tgid, va, size, &udevid);
    if (ret != 0) {
        svm_err("Get va udevid failed. (ret=%d; va=0x%llx; size=%llu)\n", ret, va, size);
        return ret;
    }
    if (udevid == uda_get_host_id()) {
        svm_err("Not support host svm addr. (va=0x%llx; size=%llu)\n", va, size);
        return -EINVAL;
    }

    ctx = pma_ctx_get(udevid, master_tgid);
    if (ctx == NULL) {
        svm_err("Get pma_ctx failed, task may have exited. (udevid=%u; master_tgid=%d)\n", udevid, master_tgid);
        return -ESRCH;
    }

    ret = pma_mem_node_create(ctx, va, size, free_callback, data, &mem_node);
    if (ret == 0) {
        *page_table = &mem_node->page_table;
    }
    pma_ctx_put(ctx);
    return ret;
}

static void pma_p2p_pages_put(struct p2p_page_table *page_table)
{
    struct pma_mem_node *mem_node = ka_container_of(page_table, struct pma_mem_node, page_table);
    struct pma_ctx *ctx = NULL;

    ctx = pma_ctx_get(mem_node->udevid, mem_node->tgid);
    if (ctx != NULL) {
        (void)pma_mem_node_erase(ctx, mem_node); /* Ignore fail, the node may have been deleted. */
        pma_ctx_put(ctx);
    }

    pma_mem_node_put(mem_node);
}

static int pma_p2p_pages_get_para_check(u64 va, u64 size, void (*free_callback)(void *data), void *data,
                                        struct p2p_page_table **page_table)
{
    /* does not need to check va, pma_get_va_udevid will check it */

    if (free_callback == NULL) {
        svm_err("Free_callback is null.\n");
        return -EINVAL;
    }

    if (page_table == NULL) {
        svm_err("Page_table is null.\n");
        return -EINVAL;
    }

    return 0;
}

int hal_kernel_p2p_get_pages(u64 va, u64 len, void (*free_callback)(void *data), void *data,
                             struct p2p_page_table **page_table)
{
    u64 size = len;
    int master_tgid = ka_task_get_current_tgid();
    int ret;

    ka_task_might_sleep();

    ret = pma_p2p_pages_get_para_check(va, size, free_callback, data, page_table);
    if (ret != 0) {
        return ret;
    }

    return pma_p2p_pages_get(master_tgid, va, size, free_callback, data, page_table);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_p2p_get_pages);

int hal_kernel_p2p_put_pages(struct p2p_page_table *page_table)
{
    if (page_table == NULL) {
        svm_err("Page_table is null.\n");
        return -EINVAL;
    }

    ka_task_might_sleep();

    pma_p2p_pages_put(page_table);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_p2p_put_pages);