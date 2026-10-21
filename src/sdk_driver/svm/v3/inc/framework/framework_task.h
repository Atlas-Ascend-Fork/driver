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

#ifndef FRAMEWORK_TASK_H
#define FRAMEWORK_TASK_H

#include "pbl_task_ctx.h"

#include "svm_pub.h"

void *svm_task_ctx_get(u32 udevid, int tgid);
void svm_task_ctx_put(void *task_ctx);

u32 svm_task_obtain_feature_id(void); /* return feature_id */
void svm_task_set_feature_invalid(void *task_ctx, u32 feature_id);
int svm_task_set_feature_priv(void *task_ctx, u32 feature_id, const char *feature_name, void *priv,
                              void (*release)(void *priv));
void *svm_task_get_feature_priv(void *task_ctx, u32 feature_id);

void svm_task_ctx_for_each(u32 udevid, void *priv, void (*func)(void *task_ctx, void *priv));
u32 svm_get_task_max_num(u32 udevid);
void svm_get_all_task_tgids(u32 udevid, int tgid[], u32 num);

void *svm_get_task_mm(void *task_ctx);
int svm_get_task_tgid(void *task_ctx);

bool svm_task_is_exit_force(void *task_ctx);
void svm_task_set_exit_abort(void *task_ctx);
bool svm_task_is_exit_abort(void *task_ctx);
bool svm_task_is_exiting(u32 udevid, int tgid);

int svm_get_task_start_time(u32 udevid, int tgid, struct task_start_time *start_time);

#endif
