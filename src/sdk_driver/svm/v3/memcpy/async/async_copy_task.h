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

#ifndef ASYNC_COPY_TASK_H
#define ASYNC_COPY_TASK_H

#include "copy_pub.h"
#include "copy_task.h"
#include "async_copy_ctx.h"
#include "async_copy_ioctl.h"

#ifndef EMU_ST /* Simulation ST is required and cannot be deleted. */
#define SVM_ASYNC_TASK_MAX_ID KA_INT_MAX
#else
#define SVM_ASYNC_TASK_MAX_ID 8
#endif

#define ASYNC_COPY_FLAG_IS_SYNC(flag) (((flag) & SVM_COPY_FLAG_SYNC) != 0)

struct svm_copy_task *async_copy_task_create(struct async_copy_ctx *ctx, struct copy_va_info *info);
struct svm_copy_task *async_copy_task_create_2d(struct async_copy_ctx *ctx, struct copy_2d_va_info *info);
struct svm_copy_task *async_copy_task_create_batch(struct async_copy_ctx *ctx, struct copy_batch_va_info *info);

int async_copy_task_destroy(struct svm_copy_task *copy_task);
int async_copy_task_save(struct async_copy_ctx *ctx, struct svm_copy_task *copy_task, int *id_out);
struct svm_copy_task *async_copy_task_remove(struct async_copy_ctx *ctx, int id);

#endif
