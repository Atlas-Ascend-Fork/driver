/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>

#include "svm_log.h"
#include "svm_user_adapt.h"
#include "gen_allocator.h"
#include "mga.h"

enum mga_align_type {
    MGA_ALIGN_TYPE_4K = 0U,
    MGA_ALIGN_TYPE_64K,
    MGA_ALIGN_TYPE_2M,
    MGA_ALIGN_TYPE_1G,
    MGA_ALIGN_TYPE_MAX
};

struct mga_inst {
    struct mga_attr attr;

    u32 base_align_type;
    u64 total_size;

    pthread_rwlock_t rwlock;
    void *ga_inst[MGA_ALIGN_TYPE_MAX];
};

static u64 mga_align_type_to_size(u32 align_type)
{
    static u64 align_type_to_size[MGA_ALIGN_TYPE_MAX] = {[MGA_ALIGN_TYPE_4K] = 4ULL * SVM_BYTES_PER_KB,
                                                         [MGA_ALIGN_TYPE_64K] = 64ULL * SVM_BYTES_PER_KB,
                                                         [MGA_ALIGN_TYPE_2M] = 2ULL * SVM_BYTES_PER_MB,
                                                         [MGA_ALIGN_TYPE_1G] = 1ULL * SVM_BYTES_PER_GB};

    return align_type_to_size[align_type];
}

static u32 mga_align_size_to_type(u64 align_size)
{
    switch (align_size) {
        case (4ULL * SVM_BYTES_PER_KB):
            return MGA_ALIGN_TYPE_4K;
        case (64ULL * SVM_BYTES_PER_KB):
            return MGA_ALIGN_TYPE_64K;
        case (2ULL * SVM_BYTES_PER_MB):
            return MGA_ALIGN_TYPE_2M;
        case (1ULL * SVM_BYTES_PER_GB):
            return MGA_ALIGN_TYPE_1G;
        default:
            return MGA_ALIGN_TYPE_MAX;
    }
}

static void mga_inst_uninit(struct mga_inst *inst)
{
    u32 align_type;

    for (align_type = 0; align_type < MGA_ALIGN_TYPE_MAX; align_type++) {
        if (inst->ga_inst[align_type] != NULL) {
            svm_ga_inst_destroy(inst->ga_inst[align_type]);
            inst->ga_inst[align_type] = NULL;
        }
    }
}

static int mga_inst_init(struct mga_inst *inst, struct mga_attr *attr)
{
    struct svm_ga_attr ga_attr;
    void *ga_inst = NULL;
    u32 align_type;

    inst->attr = *attr;
    inst->total_size = 0;
    (void)pthread_rwlock_init(&inst->rwlock, NULL);

    inst->base_align_type = mga_align_size_to_type(attr->max_align_size);
    if (inst->base_align_type >= MGA_ALIGN_TYPE_MAX) {
        svm_err("Invalid para. (max_align_size=0x%llx)\n", attr->max_align_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    for (align_type = 0; align_type < MGA_ALIGN_TYPE_MAX; align_type++) {
        svm_ga_attr_pack(mga_align_type_to_size(align_type), &ga_attr);
        ga_inst = svm_ga_inst_create(&ga_attr);
        if (ga_inst == NULL) {
            svm_debug("Create ga_inst completed. (ga_inst_is_null=%d)\n", (ga_inst == NULL));
            mga_inst_uninit(inst);
            return DRV_ERROR_OUT_OF_MEMORY;
        }

        inst->ga_inst[align_type] = ga_inst;
    }

    return DRV_ERROR_NONE;
}

static int mga_attr_check(struct mga_attr *attr)
{
    if ((attr->expand == NULL) || (attr->shrink == NULL)) {
        svm_err("Ops is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (attr->expand_gran == 0ULL) {
        svm_err("Invalid expand gran.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

void *mga_inst_create(struct mga_attr *attr)
{
    struct mga_inst *inst = NULL;
    int ret;

    ret = mga_attr_check(attr);
    if (ret != 0) {
        return NULL;
    }

    inst = svm_ua_calloc(1, sizeof(struct mga_inst));
    if (inst == NULL) {
        svm_err("svm_ua_calloc mga_inst failed. (size=%llu)\n", sizeof(struct mga_inst));
        return NULL;
    }

    ret = mga_inst_init(inst, attr);
    if (ret != 0) {
        svm_ua_free(inst);
        return NULL;
    }

    return (void *)inst;
}

static int mga_release_va(u64 start, u64 size, void *priv)
{
    struct mga_inst *inst = (struct mga_inst *)priv;
    int ret = inst->attr.shrink((void *)inst, start, size);
    if (ret != 0) {
        svm_warn("Free va failed. (ret=%d; start=0x%llx; size=%llu)\n", ret, start, size);
    }
    /* delete range in destroy ga init, not delete here because: 1. deadlock, 2. range may be not idle */
    return 0;
}

static void mga_release_all_va(struct mga_inst *inst)
{
    if (inst->ga_inst[inst->base_align_type] != NULL) {
        (void)svm_ga_for_each_range(inst->ga_inst[inst->base_align_type], mga_release_va, inst);
    }
}

void mga_inst_destroy(void *mga_inst)
{
    struct mga_inst *inst = (struct mga_inst *)mga_inst;

    mga_release_all_va(inst);

    mga_inst_uninit(inst);
    svm_ua_free(inst);
}

static int mga_shrink_sub_ga_once(struct mga_inst *inst, u32 align_type)
{
    void *base_ga_inst = inst->ga_inst[inst->base_align_type];
    void *sub_ga_inst = inst->ga_inst[align_type];
    u64 start, size;
    int ret;

    ret = svm_ga_recycle_one_idle_range(sub_ga_inst, &start, &size);
    if (ret == DRV_ERROR_NONE) {
        ret = svm_ga_free(base_ga_inst, start, size);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Unlikely. (start=0x%llx; size=%llu)\n", start, size);
            return DRV_ERROR_INNER_ERR;
        }
    }

    return ret;
}

static int mga_expand_sub_ga_once(struct mga_inst *inst, u32 align_type, u64 size)
{
    void *base_ga_inst = inst->ga_inst[inst->base_align_type];
    void *sub_ga_inst = inst->ga_inst[align_type];
    u64 align = mga_align_type_to_size(inst->base_align_type);
    u64 expand_size = svm_align_up(size, align);
    u64 start;
    int ret;

    ret = svm_ga_alloc(base_ga_inst, 0, &start, expand_size);
    if (ret == DRV_ERROR_NONE) {
        ret = svm_ga_add_range(sub_ga_inst, start, expand_size);
        if (ret != DRV_ERROR_NONE) {
            (void)svm_ga_free(base_ga_inst, start, expand_size);
        }
    }

    return ret;
}

static void mga_shrink_sub_ga(struct mga_inst *inst, u32 align_type)
{
    int ret;

    do {
        ret = mga_shrink_sub_ga_once(inst, align_type);
    } while (ret == DRV_ERROR_NONE);
}

static void mga_shrink_all_sub_ga(struct mga_inst *inst)
{
    u32 align_type;

    for (align_type = MGA_ALIGN_TYPE_4K; align_type < MGA_ALIGN_TYPE_MAX; align_type++) {
        if (align_type == inst->base_align_type) {
            continue;
        }

        mga_shrink_sub_ga(inst, align_type);
    }
}

static int mga_expand(struct mga_inst *inst)
{
    u64 va = 0, size;
    int ret;

    size = inst->attr.expand_gran;
    ret = inst->attr.expand((void *)inst, &size, &va);
    if (ret != 0) {
        svm_no_err_if((ret == DRV_ERROR_OUT_OF_MEMORY), "Expand va not succ. (ret=%d; size=%llu)\n", ret, size);
        return ret;
    }

    ret = svm_ga_add_range(inst->ga_inst[inst->base_align_type], va, size);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Ga inst add range failed. (ret=%d; start=0x%llx; size=%llu)\n", ret, va, size);
        (void)inst->attr.shrink((void *)inst, va, size);
        return ret;
    }

    inst->total_size += size;

    return DRV_ERROR_NONE;
}

static void mga_shrink(struct mga_inst *inst)
{
    u64 va, size;
    int ret;

    mga_shrink_all_sub_ga(inst);

    ret = svm_ga_recycle_one_idle_range(inst->ga_inst[inst->base_align_type], &va, &size);
    if (ret == DRV_ERROR_NONE) {
        inst->total_size -= size;
        (void)inst->attr.shrink((void *)inst, va, size);
    }
}

static int mga_alloc(struct mga_inst *inst, u32 ga_flag, u32 align_type, u64 size, u64 *va)
{
    void *ga_inst = inst->ga_inst[align_type];
    int ret;

    ret = svm_ga_alloc(ga_inst, ga_flag, va, size);
    if (ret == DRV_ERROR_NONE) {
        return DRV_ERROR_NONE;
    }

    if (align_type == inst->base_align_type) {
        mga_shrink_all_sub_ga(inst);
    } else {
        ret = mga_expand_sub_ga_once(inst, align_type, size);
        if (ret != DRV_ERROR_NONE) {
            mga_shrink_all_sub_ga(inst);
            ret = mga_expand_sub_ga_once(inst, align_type, size);
            if (ret != DRV_ERROR_NONE) {
                return DRV_ERROR_OUT_OF_MEMORY;
            }
        }
    }

    return svm_ga_alloc(ga_inst, ga_flag, va, size);
}

static int mga_free(struct mga_inst *inst, u64 va, u64 size, u32 align_type)
{
    return svm_ga_free(inst->ga_inst[align_type], va, size);
}

static int _mga_va_alloc(struct mga_inst *inst, u32 align_type, u64 size, u64 *va)
{
    int ret;

    (void)pthread_rwlock_wrlock(&inst->rwlock); /* To ensure the expand is for cur thread */
    ret = mga_alloc(inst, 0, align_type, size, va);
    if (ret != DRV_ERROR_NONE) {
        if (inst->total_size < inst->attr.expand_thres) {
            ret = mga_expand(inst);
            if (ret == DRV_ERROR_NONE) {
                ret = mga_alloc(inst, 0, align_type, size, va);
            }
        }
    }
    (void)pthread_rwlock_unlock(&inst->rwlock);

    return ret;
}

static int _mga_va_free(struct mga_inst *inst, u64 va, u64 size, u32 align_type)
{
    int ret;

    (void)pthread_rwlock_wrlock(&inst->rwlock);
    ret = mga_free(inst, va, size, align_type);
    if (ret == DRV_ERROR_NONE) {
        if (inst->total_size > inst->attr.shrink_thres) {
            mga_shrink(inst);
        }
    }
    (void)pthread_rwlock_unlock(&inst->rwlock);

    return ret;
}

int mga_va_alloc(void *mga_inst, u64 align, u64 size, u64 *va)
{
    struct mga_inst *inst = (struct mga_inst *)mga_inst;
    u32 align_type = mga_align_size_to_type(align);

    if (inst == NULL) {
        svm_err("Mga_inst is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (align_type >= MGA_ALIGN_TYPE_MAX) {
        svm_err("Align size is invalid. (align=%llu)\n", align);
        return DRV_ERROR_INVALID_VALUE;
    }

    return _mga_va_alloc(inst, align_type, size, va);
}

int mga_va_free(void *mga_inst, u64 va, u64 size, u64 align)
{
    struct mga_inst *inst = (struct mga_inst *)mga_inst;
    u32 align_type = mga_align_size_to_type(align);

    if (inst == NULL) {
        svm_err("Mga_inst is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (align_type >= MGA_ALIGN_TYPE_MAX) {
        svm_err("Align size is invalid. (align=%llu)\n", align);
        return DRV_ERROR_INVALID_VALUE;
    }

    return _mga_va_free(inst, va, size, align_type);
}

u64 mga_get_max_align_size(void *mga_inst)
{
    struct mga_inst *inst = (struct mga_inst *)mga_inst;
    return inst->attr.max_align_size;
}

u64 mga_get_total_size(void *mga_inst)
{
    struct mga_inst *inst = (struct mga_inst *)mga_inst;
    return inst->total_size;
}
