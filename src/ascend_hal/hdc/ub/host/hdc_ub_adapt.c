/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/mman.h>
#include <errno.h>
#include "ascend_hal.h"
#include "hdc_cmn.h"
#include "hdc_ub_drv.h"
#include "hdc_core.h"

#define HDC_UB_TOKEN_ID_INVALID 1

struct hdc_token_id_node {
    urma_context_t *urma_ctx;
    urma_token_id_t *token_id;
    int count;
    struct list_head node;
};

static struct list_head g_token_id_list[HDC_MAX_UB_DEV_CNT];

void hdc_token_id_list_init(void)
{
    int i;
    for (i = 0; i < HDC_MAX_UB_DEV_CNT; i++) {
        INIT_LIST_HEAD(&g_token_id_list[i]);
    }
}

int hdc_alloc_token_id(urma_context_t *urma_ctx, bool is_global, urma_token_id_t **token_id)
{
    if (is_global) {
        return 0;
    }

    if (token_id == NULL) {
        HDC_LOG_ERR("Input parameter is error.\n");
        return HDCDRV_ERR;
    }

    *token_id = NULL;

    *token_id = urma_alloc_token_id(urma_ctx);
    if (*token_id == NULL) {
        HDC_LOG_ERR("Failed to alloc token_id.\n");
        return HDCDRV_ERR;
    }
    HDC_LOG_INFO("alloc token_id success.(token_id=%u)\n", (*token_id)->token_id);
    return 0;
}

void hdc_free_token_id(urma_token_id_t *token_id)
{
    if (token_id != NULL) {
        HDC_LOG_INFO("free token_id.(token_id=%u)\n", token_id->token_id);
        (void)urma_free_token_id(token_id);
    }
}

int hdc_mem_res_init(struct hdc_mem_res_info *mem_info, int service_type)
{
    int fd = -1;
    int map_flag = 0;
    void *user_va;
    int ret = 0;

    mem_info->service_type = service_type;
    mem_info->bind_fd = hdc_ub_open();

    if (mem_info->bind_fd == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (hdc_use_kernel_pool(mem_info->service_type)) {
#ifndef EMU_ST
        fd = mem_info->bind_fd;
        map_flag = MAP_SHARED;
#else
        ret = hdc_mem_res_init_stub(mem_info->bind_fd, &mem_info->user_va, HDCDRV_UB_MEM_POOL_LEN);
        if (ret != 0) {
            goto va_mmap_failed;
        } else {
            return 0;
        }
#endif
    } else {
        fd = -1;
        map_flag = MAP_SHARED | MAP_ANONYMOUS;
    }

    user_va = mmap(NULL, HDCDRV_UB_MEM_POOL_LEN, PROT_READ | PROT_WRITE, map_flag, fd, 0);
    if (user_va == MAP_FAILED) {
        HDC_LOG_ERR("Failed to mmap tx_user memory.\n");
        ret = -HDCDRV_MEM_ALLOC_FAIL;
        goto va_mmap_failed;
    }

    mem_info->user_va = (uint64_t)(uintptr_t)user_va;

    return 0;

va_mmap_failed:
    hdc_ub_close(mem_info->bind_fd);

    return ret;
}

void hdc_mem_res_uninit(struct hdc_mem_res_info *mem_info, unsigned int dev_id)
{
    void *user_va = (void *)(uintptr_t)mem_info->user_va;
    (void)dev_id;

#ifndef EMU_ST
    (void)munmap(user_va, HDCDRV_UB_MEM_POOL_LEN);
#else
    if (hdc_use_kernel_pool(mem_info->service_type)) {
        hdc_mem_res_uninit_stub(mem_info->bind_fd, &mem_info->user_va);
    } else {
        (void)munmap(user_va, HDCDRV_UB_MEM_POOL_LEN);
    }
#endif

    hdc_ub_close(mem_info->bind_fd);

    return;
}

int hdc_register_share_urma_seg(struct hdcConfig *hdc_config, unsigned int dev_id, unsigned int token_val)
{
    (void)hdc_config;
    (void)dev_id;
    (void)token_val;
    return 0;
}

void hdc_unregister_share_urma_seg(urma_target_seg_t *tseg)
{
    (void)tseg;
    return;
}

STATIC void hdc_fill_seg_cfg(urma_seg_cfg_t *seg_cfg, hdc_ub_context_t *ctx, unsigned long long len,
                             unsigned long long va, int service_type)
{
    seg_cfg->flag.bs.token_policy = URMA_TOKEN_PLAIN_TEXT;
    seg_cfg->flag.bs.cacheable = URMA_NON_CACHEABLE;
    seg_cfg->flag.bs.access = URMA_ACCESS_LOCAL_ONLY;
    seg_cfg->flag.bs.reserved = 0;
    seg_cfg->flag.bs.token_id_valid = 1;
    if (hdc_use_kernel_pool(service_type)) {
        seg_cfg->flag.bs.non_pin = 1; /* alloc_pages memory need non pin, malloc need pin */
    } else {
        seg_cfg->flag.bs.non_pin = 0;
    }

    seg_cfg->va = (uint64_t)(uintptr_t)va;
    seg_cfg->len = len;
    seg_cfg->token_value.token = ctx->token.token;
    seg_cfg->user_ctx = (uintptr_t)NULL;
    seg_cfg->iova = 0;
}

STATIC int hdc_register_seg_with_reuse_tid(hdc_ub_context_t *ctx, urma_seg_cfg_t *seg_cfg,
                                           struct hdc_time_record_for_urma_init *record)
{
    struct hdc_token_id_node *node = NULL;
    int saved_errno;

    if (g_token_id_list[ctx->devid].prev != &g_token_id_list[ctx->devid]) {
        node = list_entry(g_token_id_list[ctx->devid].prev, struct hdc_token_id_node, node);
        seg_cfg->token_id = node->token_id;
        hdc_get_time_record(&record->urma_record.own_reg_re_start, &record->fail_times);
        ctx->tseg = urma_register_seg(ctx->urma_ctx, seg_cfg);
        hdc_get_time_record(&record->urma_record.own_reg_re_end, &record->fail_times);
        saved_errno = errno;
        if (ctx->tseg != NULL) {
            node->count++;
            ctx->token_id = node->token_id;
            ctx->token_id_node = node;
            return 0;
        }
        if (saved_errno != ERANGE) {
            HDC_LOG_ERR("Failed to register segment.(errno=%d)\n", saved_errno);
            return HDCDRV_ERR;
        }
    }
    return HDC_UB_TOKEN_ID_INVALID;
}

STATIC int hdc_register_seg_with_new_tid(hdc_ub_context_t *ctx, urma_seg_cfg_t *seg_cfg,
                                         struct hdc_time_record_for_urma_init *record)
{
    urma_token_id_t *new_token = NULL;
    struct hdc_token_id_node *new_node = NULL;

    if (hdc_alloc_token_id(ctx->urma_ctx, false, &new_token) != 0) {
        return HDCDRV_ERR;
    }
    hdc_get_time_record(&record->urma_record.alloc_tid_end, &record->fail_times);
    seg_cfg->token_id = new_token;
    ctx->tseg = urma_register_seg(ctx->urma_ctx, seg_cfg);
    hdc_get_time_record(&record->urma_record.own_reg_new_end, &record->fail_times);
    if (ctx->tseg == NULL) {
        HDC_LOG_ERR("Failed to register segment using new token_id.\n");
        goto reg_seg_failed;
    }
    new_node = calloc(1, sizeof(struct hdc_token_id_node));
    if (new_node == NULL) {
        HDC_LOG_ERR("Failed to alloc token_id node.\n");
        goto alloc_node_failed;
    }
    new_node->urma_ctx = ctx->urma_ctx;
    new_node->token_id = new_token;
    new_node->count = 1;
    drv_user_list_add_tail(&new_node->node, &g_token_id_list[ctx->devid]);
    ctx->token_id = new_token;
    ctx->token_id_node = new_node;
    return 0;

alloc_node_failed:
    (void)urma_unregister_seg(ctx->tseg);
    ctx->tseg = NULL;
    ctx->token_id = NULL;
    ctx->token_id_node = NULL;
reg_seg_failed:
    (void)hdc_free_token_id(new_token);
    return HDCDRV_ERR;
}

STATIC void hdc_token_id_release(struct hdc_token_id_node *node)
{
    node->count--;
    if (node->count <= 0) {
        drv_user_list_del(&node->node);
        (void)hdc_free_token_id(node->token_id);
        free(node);
    }
}

int hdc_register_own_urma_seg(hdc_ub_context_t *ctx, unsigned long long len, unsigned long long va, int service_type,
                              struct hdc_time_record_for_urma_init *record)
{
    urma_seg_cfg_t seg_cfg = {0};
    int ret;

    hdc_fill_seg_cfg(&seg_cfg, ctx, len, va, service_type);

    ret = hdc_register_seg_with_reuse_tid(ctx, &seg_cfg, record);
    if (ret == HDC_UB_TOKEN_ID_INVALID) {
        ret = hdc_register_seg_with_new_tid(ctx, &seg_cfg, record);
    }
    return ret;
}

void hdc_unregister_own_urma_seg(urma_target_seg_t *tseg, void *token_id_node, uint32_t dev_id, int service_type)
{
    (void)service_type;

    if (tseg != NULL) {
        (void)urma_unregister_seg(tseg);
    }

    if (token_id_node == NULL) {
        return;
    }

    (void)mmMutexLock(&g_hdcConfig.urma_attr_lock[dev_id]);
    hdc_token_id_release((struct hdc_token_id_node *)token_id_node);
    (void)mmMutexUnLock(&g_hdcConfig.urma_attr_lock[dev_id]);
}

int hdc_register_dev_dmp_urma_seg(hdc_ub_context_t *ctx, unsigned long long len, unsigned long long va,
                                  int service_type)
{
    (void)ctx;
    (void)len;
    (void)va;
    (void)service_type;
    return 0;
}

void hdc_unregister_dmp_urma_seg(urma_target_seg_t *tseg, int service_type)
{
    (void)tseg;
    (void)service_type;
    return;
}
