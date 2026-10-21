/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MALLOC_MNG_H
#define MALLOC_MNG_H

#include <stddef.h>

#include "svm_pub.h"
#include "svm_flag.h"

#define SVM_MALLOC_NUMA_NO_NODE 0xFFU

struct svm_prop {
    u32 devid; /* If no map, is invalid */
    int tgid;  /* If no map, is invalid */
    u64 flag;
    u64 start;
    u64 size;
    u64 aligned_size;
    bool is_from_cache;
};

struct svm_priv_ops {
    int (*release)(void *priv, bool force);
    int (*get_prop)(void *priv, u64 va, struct svm_prop *prop); /* not must */
    u32 (*show)(void *priv, char *buf, u32 buf_len); /* not must, return format string len when buf is valid */
    bool (*should_criu_reset)(void *priv); /* not must, release the inited handle during criu reset when return true */
};

struct svm_malloc_location {
    u32 devid;
    u32 numa_id;
};

static inline void svm_malloc_location_pack(u32 devid, u32 numa_id, struct svm_malloc_location *location)
{
    location->devid = devid;
    location->numa_id = numa_id;
}

int svm_get_dev_tgid(u32 devid, int *tgid);

int svm_malloc(u64 *start, u64 size, u64 align, u64 flag, struct svm_malloc_location *location);
/*
 * 1. If mem ref is occupied by other task, free method will return DRV_ERROR_BUSY,
 * and set mem invalid, then get* method will return failed.
 * 2. If no such va will return DRV_ERROR_NOT_EXIST.
 * 3. If it is already handle_get, will return DRV_ERROR_CLIENT_BUSY.
 * 4. Will call priv->release(force=false), if priv->release return DRV_ERROR_BUSY, svm_free will return
 * DRV_ERROR_CLIENT_BUSY.
 */
int svm_free(u64 start);

/*
 * If it is already handle_get, will return fail, otherwise will call priv->release(force=true)
 */
int svm_recycle_mem_by_dev(u32 devid);
void svm_show_mem(void);
void svm_show_dev_mem(u32 devid, char *buf, u32 buf_len);

int svm_get_prop(u64 va, struct svm_prop *prop);
int svm_get_nearby_prop(u64 va, struct svm_prop *left_prop, bool *has_left, struct svm_prop *right_prop,
                        bool *has_right);

/* It will include memory that has been released by svm_free but is still in a busy state. */
int svm_for_each_handle(int (*func)(void *handle, u64 start, struct svm_prop *prop, void *priv), void *priv);
int svm_for_each_valid_handle(int (*func)(void *handle, u64 start, struct svm_prop *prop, void *priv), void *priv);
void *svm_handle_get(u64 va);
void svm_handle_put(void *handle);

/* If already set, return DRV_ERROR_REPEATED_INIT. */
int svm_set_priv(void *handle, void *priv, struct svm_priv_ops *priv_ops);
void *svm_get_priv(void *handle);
void svm_set_priv_ops(void *handle, struct svm_priv_ops *priv_ops);
void svm_mod_prop_flag(void *handle, u64 flag);
void svm_mod_prop_devid(void *handle, u32 devid);
void svm_add_prop_flag(void *handle, u64 flag);

static inline void svm_mod_va_devid(u64 va, u32 devid)
{
    void *handle = svm_handle_get(va);
    if (handle != NULL) {
        svm_mod_prop_devid(handle, devid);
        svm_handle_put(handle);
    }
}

/* malloc free ops */
struct svm_mng_ops {
    int (*post_malloc)(void *handle, u64 start, struct svm_prop *prop);
    void (*pre_free)(void *handle, u64 start, struct svm_prop *prop);
};

void svm_mng_set_ops(struct svm_mng_ops *ops);
void svm_set_task_bitmap(void *handle, u32 task_bitmap);
u32 svm_get_task_bitmap(void *handle);
bool svm_handle_mem_is_cache(void *handle);

void svm_set_mem_source(void *handle, u32 source_type);
u32 svm_get_mem_source(void *handle);

#endif
