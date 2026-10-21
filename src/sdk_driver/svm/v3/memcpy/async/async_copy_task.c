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
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_sched_pub.h"

#include "svm_pub.h"
#include "svm_kern_log.h"
#include "svm_idr.h"
#include "copy_pub.h"
#include "copy_task.h"
#include "async_copy_task.h"
#include "async_copy_ioctl.h"

int async_copy_task_save(struct async_copy_ctx *ctx, struct svm_copy_task *copy_task, int *id_out)
{
    int ret;

    ka_task_down_write(&ctx->rw_sem);
    ret = svm_idr_alloc(&ctx->idr, (void *)copy_task, id_out);
    ka_task_up_write(&ctx->rw_sem);

    return ret;
}

struct svm_copy_task *async_copy_task_remove(struct async_copy_ctx *ctx, int id)
{
    struct svm_copy_task *copy_task = NULL;

    ka_task_down_write(&ctx->rw_sem);
    copy_task = (struct svm_copy_task *)svm_idr_remove(&ctx->idr, id);
    ka_task_up_write(&ctx->rw_sem);

    return copy_task;
}

static void async_copy_subtask_get_info(struct copy_va_info *info, u64 finished_size, u64 grain_size,
                                        struct copy_va_info *sub_info)
{
    sub_info->size = svm_get_subtask_size(info->size - finished_size, grain_size);
    sub_info->src_va = info->src_va + finished_size;
    sub_info->dst_va = info->dst_va + finished_size;
    sub_info->src_udevid = info->src_udevid;
    sub_info->dst_udevid = info->dst_udevid;
    sub_info->src_host_tgid = info->src_host_tgid;
    sub_info->dst_host_tgid = info->dst_host_tgid;
    sub_info->flag = info->flag;
}

static int async_copy_subtask_submit(struct svm_copy_task *copy_task, struct copy_va_info *info)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    struct svm_copy_subtask *subtask = NULL;
    struct copy_va_info sub_info = {0};
    u64 grain_size = svm_get_subtask_grain_size(info->size);
    u64 finished_size;
    int ret;
    u32 subtask_flag = ASYNC_COPY_FLAG_IS_SYNC(info->flag) ? SVM_COPY_SUBTASK_SYNC : SVM_COPY_SUBTASK_AUTO_RECYCLE;

    for (finished_size = 0; finished_size < info->size; finished_size += sub_info.size) {
        ka_try_cond_resched(&stamp);
        async_copy_subtask_get_info(info, finished_size, grain_size, &sub_info);

        subtask = svm_copy_subtask_create(copy_task, &sub_info, subtask_flag);
        if (subtask == NULL) {
            svm_err("Create subtask failed. (sub_dst=0x%llx; sub_src=0x%llx; size=%llu)\n", sub_info.dst_va,
                    sub_info.src_va, sub_info.size);
            (void)svm_copy_task_wait(copy_task);
            return -EINVAL;
        }
        ret = svm_copy_subtask_submit(subtask);
        if (ret != 0) {
            svm_err("Submit subtask failed. (ret=%d)\n", ret);
            svm_copy_subtask_destroy(subtask);
            (void)svm_copy_task_wait(copy_task);
            return ret;
        }
    }

    return 0;
}

struct svm_copy_task *async_copy_task_create(struct async_copy_ctx *ctx, struct copy_va_info *info)
{
    struct svm_copy_task *copy_task = NULL;
    u32 exec_udevid = copy_va_info_get_exec_udevid(info);
    int ret;

    copy_task = svm_copy_task_create(exec_udevid);
    if (copy_task == NULL) {
        return NULL;
    }

    ret = async_copy_subtask_submit(copy_task, info);
    if (ret != 0) {
        svm_copy_task_destroy(copy_task);
        return NULL;
    }

    return copy_task;
}

struct svm_copy_task *async_copy_task_create_2d(struct async_copy_ctx *ctx, struct copy_2d_va_info *info)
{
    struct svm_copy_task *copy_task = NULL;
    struct copy_va_info sub_info = {0};
    u64 i;
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
            goto recycle_sub_task;
        }

        ret = async_copy_subtask_submit(copy_task, &sub_info);
        if (ret != 0) {
            goto recycle_sub_task;
        }
    }

    return copy_task;
recycle_sub_task:
    (void)svm_copy_task_wait(copy_task);
    svm_copy_task_destroy(copy_task);
    return NULL;
}

struct svm_copy_task *async_copy_task_create_batch(struct async_copy_ctx *ctx, struct copy_batch_va_info *info)
{
    struct svm_copy_task *copy_task = NULL;
    struct copy_va_info sub_info = {0};
    u32 i;
    int ret;

    copy_task = svm_copy_task_create(ctx->udevid);
    if (copy_task == NULL) {
        return NULL;
    }

    for (i = 0; i < info->count; i++) {
        sub_info.src_va = info->src_va[i];
        sub_info.dst_va = info->dst_va[i];
        sub_info.size = info->size[i];
        sub_info.src_udevid = info->src_udevid;
        sub_info.dst_udevid = info->dst_udevid;
        sub_info.src_host_tgid = 0;
        sub_info.dst_host_tgid = 0;

        ret = copy_va_info_check(ctx->udevid, &sub_info);
        if (ret != 0) {
            svm_err("Va check failed. (ret=%d; index=%u; src_va=0x%llx; dst_va=0x%llx; size=%llu)\n", ret, i,
                    sub_info.src_va, sub_info.dst_va, sub_info.size);
            goto recycle_sub_task;
        }

        ret = async_copy_subtask_submit(copy_task, &sub_info);
        if (ret != 0) {
            svm_err("Batch cpy submit failed. (ret=%d; index=%u; src_va=0x%llx; dst_va=0x%llx; size=%llu)\n", ret, i,
                    sub_info.src_va, sub_info.dst_va, sub_info.size);
            goto recycle_sub_task;
        }
    }

    return copy_task;
recycle_sub_task:
    (void)svm_copy_task_wait(copy_task);
    svm_copy_task_destroy(copy_task);
    return NULL;
}

int async_copy_task_destroy(struct svm_copy_task *copy_task)
{
    int ret;

    ret = svm_copy_task_wait(copy_task);
    svm_copy_task_destroy(copy_task);
    return ret;
}