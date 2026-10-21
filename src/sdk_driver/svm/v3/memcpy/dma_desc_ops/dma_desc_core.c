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
#include "ka_ioctl_pub.h"
#include "ka_system_pub.h"
#include "ka_sched_pub.h"

#include "pbl_feature_loader.h"
#include "pbl_task_ctx.h"
#include "dpa_kernel_interface.h"
#include "pbl_uda.h"

#include "svm_pub.h"
#include "svm_kern_log.h"
#include "copy_pub.h"
#include "dma_desc_ctx.h"
#include "dma_desc_node.h"
#include "dma_desc_core.h"

#define SVM_CONVERT_SUBTASK_GRAIN_SIZE_128MB (128ULL * SVM_BYTES_PER_MB)
static inline u64 svm_get_convert_subtask_grain_size(void)
{
    return SVM_CONVERT_SUBTASK_GRAIN_SIZE_128MB;
}

static void dma_desc_copy_subtask_info_pack(struct copy_va_info *info, u64 finished_size, u64 grain_size,
                                            struct copy_va_info *sub_info)
{
    sub_info->size = ka_base_min(info->size - finished_size, grain_size);
    sub_info->src_va = info->src_va + finished_size;
    sub_info->dst_va = info->dst_va + finished_size;
    sub_info->src_udevid = info->src_udevid;
    sub_info->dst_udevid = info->dst_udevid;
    sub_info->src_host_tgid = info->src_host_tgid;
    sub_info->dst_host_tgid = info->dst_host_tgid;
}

static int dma_desc_copy_subtask_convert(struct svm_copy_task *copy_task, struct copy_va_info *info)
{
    struct svm_copy_subtask *subtask = NULL;
    struct copy_va_info sub_info;
    u64 grain_size = svm_get_convert_subtask_grain_size();
    u64 finished_size;
    unsigned long stamp = (unsigned long)ka_jiffies;

    for (finished_size = 0; finished_size < info->size; finished_size += sub_info.size) {
        dma_desc_copy_subtask_info_pack(info, finished_size, grain_size, &sub_info);
        subtask = svm_copy_subtask_create(copy_task, &sub_info, 0);
        if (subtask == NULL) {
            svm_err("Create subtask failed. (sub_dst=0x%llx; sub_src=0x%llx; size=%llu)\n", sub_info.dst_va,
                    sub_info.src_va, sub_info.size);
            /* no need destroy copy_subtask, later will destroy copy_task */
            return -EINVAL;
        }
        ka_try_cond_resched(&stamp);
    }

    return 0;
}

static struct svm_copy_task *dma_desc_copy_task_convert_2d(struct dma_desc_ctx *ctx, struct copy_2d_va_info *info)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    struct svm_copy_task *copy_task = NULL;
    struct copy_va_info sub_info;
    u32 i;
    int ret;

    copy_task = svm_copy_task_create(ctx->udevid);
    if (copy_task == NULL) {
        return NULL;
    }

    for (i = 0; i < info->height; i++) {
        sub_info.src_va = info->src_va + i * info->spitch;
        sub_info.dst_va = info->dst_va + i * info->dpitch;
        sub_info.size = info->width;
        sub_info.src_udevid = info->src_udevid;
        sub_info.dst_udevid = info->dst_udevid;
        sub_info.src_host_tgid = 0;
        sub_info.dst_host_tgid = 0;

        ret = copy_va_info_check(ctx->udevid, &sub_info);
        if (ret != 0) {
            svm_copy_task_destroy(copy_task);
            return NULL;
        }

        ret = dma_desc_copy_subtask_convert(copy_task, &sub_info);
        if (ret != 0) {
            svm_copy_task_destroy(copy_task);
            return NULL;
        }
        ka_try_cond_resched(&stamp);
    }

    return copy_task;
}

static void dma_desc_copy_task_destroy_2d(struct svm_copy_task *copy_task)
{
    svm_copy_task_destroy(copy_task);
}

int dma_desc_convert_2d(struct dma_desc_ctx *ctx, struct copy_2d_va_info *info, struct DMA_ADDR *dma_desc)
{
    struct svm_copy_task *copy_task = NULL;
    int ret;

    copy_task = dma_desc_copy_task_convert_2d(ctx, info);
    if (copy_task == NULL) {
        svm_err("Convert 2d failed.\n");
        return -EINVAL;
    }

    ret = dma_desc_node_create(ctx, copy_task, dma_desc);
    if (ret != 0) {
        dma_desc_copy_task_destroy_2d(copy_task);
    }

    return ret;
}

static struct svm_copy_task *dma_desc_copy_task_convert(struct dma_desc_ctx *ctx, struct copy_va_info *info)
{
    struct svm_copy_task *copy_task = NULL;
    int ret;

    copy_task = svm_copy_task_create(ctx->udevid);
    if (copy_task == NULL) {
        return NULL;
    }

    ret = dma_desc_copy_subtask_convert(copy_task, info);
    if (ret != 0) {
        svm_copy_task_destroy(copy_task);
        return NULL;
    }

    return copy_task;
}

static void dma_desc_copy_task_destroy(struct svm_copy_task *copy_task)
{
    svm_copy_task_destroy(copy_task);
}

int dma_desc_convert(struct dma_desc_ctx *ctx, struct copy_va_info *info, struct DMA_ADDR *dma_desc)
{
    struct svm_copy_task *copy_task = NULL;
    int ret;

    copy_task = dma_desc_copy_task_convert(ctx, info);
    if (copy_task == NULL) {
        svm_err("Convert failed.\n");
        return -EINVAL;
    }

    ret = dma_desc_node_create(ctx, copy_task, dma_desc);
    if (ret != 0) {
        dma_desc_copy_task_destroy(copy_task);
    }

    return ret;
}

int dma_desc_destroy(struct dma_desc_ctx *ctx, struct DMA_ADDR *dma_desc)
{
    struct svm_copy_task *copy_task = NULL;
    struct dma_desc_node *node = NULL;
    int ret;

    node = dma_desc_node_get(ctx, (u64)(uintptr_t)dma_desc->phyAddr.priv);
    if (node == NULL) {
        svm_err("Get dma desc node failed.\n");
        return -EINVAL;
    }

    ret = dma_desc_node_state_trans(node, DMA_DESC_NODE_IDLE, DMA_DESC_NODE_FREEING);
    if (ret != 0) {
        svm_err("Node state trans from IDLE to FREEING failed.\n");
        dma_desc_node_put(node);
        return ret;
    }

    copy_task = node->copy_task;
    dma_desc_node_destroy(ctx, node);
    dma_desc_node_put(node);
    dma_desc_copy_task_destroy(copy_task);
    return 0;
}

int dma_desc_submit(struct dma_desc_ctx *ctx, struct DMA_ADDR *dma_desc, int sync_flag)
{
    struct dma_desc_node *node = NULL;
    int ret;

    node = dma_desc_node_get(ctx, (u64)(uintptr_t)dma_desc->phyAddr.priv);
    if (node == NULL) {
        svm_err("Get dma desc node failed.\n");
        return -EINVAL;
    }

    ret = dma_desc_node_state_trans(node, DMA_DESC_NODE_IDLE, DMA_DESC_NODE_SUBMITING);
    if (ret != 0) {
        svm_err("Dma desc node state trans IDLE to SUBMITTING failed. (ret=%d)\n", ret);
        goto put_dma_desc_node;
    }

    ret = svm_copy_task_submit(node->copy_task);
    if (ret != 0) {
        svm_err("Submit copy task failed. (ret=%d)\n", ret);
        (void)dma_desc_node_state_trans(node, DMA_DESC_NODE_SUBMITING, DMA_DESC_NODE_IDLE);
        goto put_dma_desc_node;
    }

    if ((ret == 0) && (sync_flag == MEMCPY_SUMBIT_SYNC)) {
        ret = svm_copy_task_wait(node->copy_task);
        (void)dma_desc_node_state_trans(node, DMA_DESC_NODE_SUBMITING, DMA_DESC_NODE_IDLE);
        goto put_dma_desc_node;
    }

    (void)dma_desc_node_state_trans(node, DMA_DESC_NODE_SUBMITING, DMA_DESC_NODE_COPYING);
put_dma_desc_node:
    dma_desc_node_put(node);
    return ret;
}

int dma_desc_wait(struct dma_desc_ctx *ctx, struct DMA_ADDR *dma_desc)
{
    struct dma_desc_node *node = NULL;
    enum dma_desc_node_state dst_state = DMA_DESC_NODE_IDLE;
    int ret;

    node = dma_desc_node_get(ctx, (u64)(uintptr_t)dma_desc->phyAddr.priv);
    if (node == NULL) {
        svm_err("Get dma desc node failed.\n");
        return -EINVAL;
    }

    ret = dma_desc_node_state_trans(node, DMA_DESC_NODE_COPYING, DMA_DESC_NODE_WAITING);
    if (ret != 0) {
        svm_err("Dma desc node state trans IDLE to SUBMITTING failed. (ret=%d)\n", ret);
        goto put_dma_desc_node;
    }

    ret = svm_copy_task_wait(node->copy_task);
    if (ret != 0) {
        svm_err("Wait copy task finish failed. (ret=%d)\n", ret);
        dst_state = (ret == -ETIMEDOUT) ? DMA_DESC_NODE_COPYING : DMA_DESC_NODE_IDLE;
        goto trans_dma_desc_node_state;
    }

trans_dma_desc_node_state:
    (void)dma_desc_node_state_trans(node, DMA_DESC_NODE_WAITING, dst_state);
put_dma_desc_node:
    dma_desc_node_put(node);
    return ret;
}