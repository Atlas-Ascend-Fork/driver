/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stddef.h>

#include "securec.h"

#include "svm_user_adapt.h"
#include "svm_log.h"
#include "svm_atomic.h"
#include "svmm.h"
#include "svmm_inst.h"
#include "svmm_non_overlap.h"

struct svmm_seg {
    struct svmm_seg_head head;
    struct rbtree_node node;
    pthread_rwlock_t rwlock;
    u32 ref;
    u32 devid;
    u64 start;
    u64 size;
    u64 svm_flag;
    struct svm_global_va src_info;
    void *priv;
    struct svm_svmm_seg_priv_ops *priv_ops;
};

static void svmm_seg_rb_range(struct rbtree_node *node, struct rb_range_handle *range)
{
    struct svmm_seg *seg = rb_entry(node, struct svmm_seg, node);

    range->start = seg->start;
    range->end = seg->start + seg->size - 1;
}

static struct svmm_seg *svmm_search_seg(struct svmm_inst *svmm_inst, u64 va, u64 size)
{
    struct rb_range_handle rb_range = {.start = va, .end = va + size - 1};
    struct rbtree_node *node = NULL;

    node = rbtree_search_by_range(&svmm_inst->root, &rb_range, svmm_seg_rb_range);
    if (node == NULL) {
        return NULL;
    }

    return rb_entry(node, struct svmm_seg, node);
}

static int svmm_add_seg(struct svmm_inst *svmm_inst, struct svmm_seg *seg)
{
    int ret = rbtree_insert_by_range(&svmm_inst->root, &seg->node, svmm_seg_rb_range);
    if (ret == 0) {
        svmm_inst->seg_num++;
    }

    return (ret != 0) ? DRV_ERROR_REPEATED_USERD : 0;
}

static void svmm_remove_seg(struct svmm_inst *svmm_inst, struct svmm_seg *seg)
{
    svmm_inst->seg_num--;
    _rbtree_erase(&svmm_inst->root, &seg->node);
}

static int svmm_seg_priv_release(struct svmm_seg *seg, bool force)
{
    int ret = 0;
    if ((seg->priv_ops != NULL) && (seg->priv_ops->release != NULL)) {
        ret = seg->priv_ops->release(seg->priv, force);
    }

    return ret;
}

static void svmm_seg_priv_show(struct svmm_seg *seg)
{
    if ((seg->priv_ops != NULL) && (seg->priv_ops->show != NULL)) {
        (void)seg->priv_ops->show(seg->priv, NULL, 0);
    }
}

static u32 svmm_non_overlap_show_seg(struct svmm_seg *seg, int id, char *buf, u32 buf_len)
{
    struct svm_global_va *src = &seg->src_info;
    int tmp_id = id;

    if (buf == NULL) {
        svm_info("id %d: seg info: ref %d devid %u start 0x%llx size 0x%llx, svm_flag 0x%llx\n"
                 "    seg src: udevid %u tgid %d va 0x%llx size 0x%llx\n",
                 tmp_id, seg->ref, seg->devid, seg->start, seg->size, seg->svm_flag, src->udevid, src->tgid, src->va,
                 src->size);
        svmm_seg_priv_show(seg);
        return 0;
    } else {
        if (buf_len == 0) {
            return 0;
        }
        int len = snprintf_s(buf, buf_len, buf_len - 1,
                             "id %d: seg info: ref %d devid %u start 0x%llx size 0x%llx, svm_flag 0x%llx\n"
                             "    seg src: udevid %u tgid %d va 0x%llx size 0x%llx\n",
                             tmp_id, seg->ref, seg->devid, seg->start, seg->size, seg->svm_flag, src->udevid, src->tgid,
                             src->va, src->size);
        return (len < 0) ? 0 : (u32)len;
    }
}

u32 svmm_non_overlap_show(struct svmm_inst *svmm_inst, char *buf, u32 buf_len)
{
    struct rbtree_node *node = NULL;
    int id = 0;
    u32 len = 0;

    (void)pthread_rwlock_rdlock(&svmm_inst->rwlock);
    rbtree_node_for_each(node, &svmm_inst->root)
    {
        struct svmm_seg *seg = rb_entry(node, struct svmm_seg, node);
        len += svmm_non_overlap_show_seg(seg, id++, buf + len, buf_len - len);
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    return len;
}

static struct svmm_seg *svmm_seg_create(u32 devid, u64 start, u64 size, u64 svm_flag, struct svm_global_va *src_info)
{
    struct svmm_seg *seg = NULL;

    seg = (struct svmm_seg *)svm_ua_calloc(1, sizeof(*seg));
    if (seg == NULL) {
        svm_err("Calloc seg failed. (size=0x%lx)\n", sizeof(*seg));
        return NULL;
    }

    seg->head.overlap_type = SVMM_NON_OVERLAP;
    seg->ref = 0;
    seg->devid = devid;
    seg->start = start;
    seg->size = size;
    seg->svm_flag = svm_flag;
    seg->src_info = *src_info;
    seg->priv = NULL;
    seg->priv_ops = NULL;
    (void)pthread_rwlock_init(&seg->rwlock, NULL);

    return seg;
}

static void svmm_seg_destroy(struct svmm_seg *seg)
{
    svm_ua_free(seg);
}

int svmm_non_overlap_add_seg(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 svm_flag,
                             struct svm_global_va *src_info)
{
    struct svmm_seg *seg = NULL;
    u64 size = src_info->size;
    int ret;

    seg = svmm_seg_create(devid, start, size, svm_flag, src_info);
    if (seg == NULL) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    pthread_rwlock_wrlock(&svmm_inst->rwlock);
    ret = svmm_add_seg(svmm_inst, seg);
    pthread_rwlock_unlock(&svmm_inst->rwlock);
    if (ret != 0) {
        svmm_seg_destroy(seg);
        (void)svmm_non_overlap_show(svmm_inst, NULL, 0);
        svm_err("Insert failed. (devid=%u; start=0x%llx; size=0x%llx)\n", devid, start, size);
    }

    return ret;
}

int svmm_non_overlap_can_add_seg(struct svmm_inst *svmm_inst, u64 start, u64 size)
{
    struct rb_range_handle range = {.start = start, .end = start + size - 1ULL};
    bool can_insert;

    pthread_rwlock_rdlock(&svmm_inst->rwlock);
    can_insert = rbtree_can_insert_range(&svmm_inst->root, &range, svmm_seg_rb_range);
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    return can_insert ? 0 : DRV_ERROR_REPEATED_USERD;
}

int svmm_non_overlap_del_seg(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 size, bool force, u64 *svm_flag)
{
    struct svmm_seg *seg = NULL;
    int ret;
    u32 ref;

    pthread_rwlock_wrlock(&svmm_inst->rwlock);
    seg = svmm_search_seg(svmm_inst, start, size);
    if (seg == NULL) {
        pthread_rwlock_unlock(&svmm_inst->rwlock);
        svm_err("Invalid para. (devid=%u; start=0x%llx; size=0x%llx)\n", devid, start, size);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((devid != seg->devid) || (start != seg->start) || (size != seg->size)) {
        svm_err("Invalid para. (devid=%u; start=0x%llx; size=0x%llx; seg: devid=%u; start=0x%llx; size=0x%llx)\n",
                devid, start, size, seg->devid, seg->start, seg->size);
        pthread_rwlock_unlock(&svmm_inst->rwlock);
        return DRV_ERROR_PARA_ERROR;
    }

    ref = svm_atomic_read(&seg->ref);
    if (ref > 0) {
        pthread_rwlock_unlock(&svmm_inst->rwlock);
        svm_err("Seg is in use. (devid=%u; start=0x%llx; size=%llx; ref=%u)\n", devid, start, size, ref);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = svmm_seg_priv_release(seg, force);
    if (ret != DRV_ERROR_NONE) {
        pthread_rwlock_unlock(&svmm_inst->rwlock);
        svm_err("Seg priv_ops.release failed. (ret=%d; devid=%u; start=0x%llx; size=%llu)\n", ret, devid, start, size);
        return ret;
    }

    if (svm_flag != NULL) {
        *svm_flag = seg->svm_flag;
    }
    svmm_remove_seg(svmm_inst, seg);
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    svmm_seg_destroy(seg);
    return 0;
}

static void svmm_get_seg_info(struct svmm_seg *seg, u32 *devid, u64 *va, u64 *svm_flag, struct svm_global_va *src_info)
{
    *devid = seg->devid;
    *va = seg->start;
    *svm_flag = seg->svm_flag;
    *src_info = seg->src_info;
}

static int svmm_non_overlap_get_seg_by_va(struct svmm_inst *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                          struct svm_global_va *src_info)
{
    struct svmm_seg *seg = (struct svmm_seg *)svmm_seg_handle_get(svmm_inst, *va);
    if (seg == NULL) {
        return DRV_ERROR_NOT_EXIST;
    }

    if (*devid != SVM_INVALID_DEVID) {
        if (*devid != seg->devid) {
            svmm_seg_handle_put(seg);
            return DRV_ERROR_NOT_EXIST;
        }
    }

    if (src_info->udevid != SVM_INVALID_UDEVID) {
        if (src_info->udevid != seg->src_info.udevid) {
            svmm_seg_handle_put(seg);
            return DRV_ERROR_NOT_EXIST;
        }
    }

    svmm_get_seg_info(seg, devid, va, svm_flag, src_info);
    svmm_seg_handle_put(seg);

    return 0;
}

static int svmm_non_overlap_get_seg_by_devid(struct svmm_inst *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                             struct svm_global_va *src_info)
{
    struct rbtree_node *node = NULL;

    (void)pthread_rwlock_rdlock(&svmm_inst->rwlock);
    rbtree_node_for_each(node, &svmm_inst->root)
    {
        struct svmm_seg *seg = rb_entry(node, struct svmm_seg, node);
        if (((*devid == seg->devid) || (*devid == SVM_INVALID_DEVID)) &&
            ((src_info->udevid == seg->src_info.udevid) || (src_info->udevid == SVM_INVALID_UDEVID)) &&
            ((src_info->va == seg->src_info.va) || (src_info->va == 0)) &&
            ((src_info->size == seg->src_info.size) || (src_info->size == 0))) {
            svmm_get_seg_info(seg, devid, va, svm_flag, src_info);
            pthread_rwlock_unlock(&svmm_inst->rwlock);
            return 0;
        }
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);
    return DRV_ERROR_NOT_EXIST;
}

static int svmm_non_overlap_get_first_seg(struct svmm_inst *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                          struct svm_global_va *src_info)
{
    struct rbtree_node *node = NULL;

    (void)pthread_rwlock_rdlock(&svmm_inst->rwlock);
    node = rbtree_first(&svmm_inst->root);
    if (node != NULL) {
        struct svmm_seg *seg = rb_entry(node, struct svmm_seg, node);
        svmm_get_seg_info(seg, devid, va, svm_flag, src_info);
        pthread_rwlock_unlock(&svmm_inst->rwlock);
        return 0;
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);
    return DRV_ERROR_NOT_EXIST;
}

int svmm_non_overlap_get_seg(struct svmm_inst *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                             struct svm_global_va *src_info)
{
    if (*va != 0) {
        return svmm_non_overlap_get_seg_by_va(svmm_inst, devid, va, svm_flag, src_info);
    } else if ((*devid != SVM_INVALID_DEVID) || (src_info->udevid != SVM_INVALID_UDEVID)) {
        return svmm_non_overlap_get_seg_by_devid(svmm_inst, devid, va, svm_flag, src_info);
    } else {
        return svmm_non_overlap_get_first_seg(svmm_inst, devid, va, svm_flag, src_info);
    }
}

void *svmm_seg_handle_get(struct svmm_inst *svmm_inst, u64 va)
{
    struct svmm_seg *seg = NULL;

    (void)pthread_rwlock_rdlock(&svmm_inst->rwlock);
    seg = svmm_search_seg(svmm_inst, va, 1);
    if (seg != NULL) {
        svm_atomic_inc(&seg->ref); /* must add ref in svmm inst lock */
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    return (void *)seg;
}

void svmm_seg_handle_put(void *seg_handle)
{
    struct svmm_seg *seg = (struct svmm_seg *)seg_handle;
    svm_atomic_dec(&seg->ref);
}

int svmm_set_seg_priv(void *seg_handle, void *priv, struct svm_svmm_seg_priv_ops *priv_ops)
{
    struct svmm_seg *seg = (struct svmm_seg *)seg_handle;

    pthread_rwlock_wrlock(&seg->rwlock);
    if ((seg->priv != NULL) || (seg->priv_ops != NULL)) {
        pthread_rwlock_unlock(&seg->rwlock);
        return DRV_ERROR_REPEATED_INIT;
    }
    seg->priv = priv;
    seg->priv_ops = priv_ops;
    pthread_rwlock_unlock(&seg->rwlock);

    return 0;
}

void *svmm_get_seg_priv(void *seg_handle)
{
    struct svmm_seg *seg = (struct svmm_seg *)seg_handle;
    return seg->priv;
}

u32 svmm_get_seg_devid(void *seg_handle)
{
    struct svmm_seg *seg = (struct svmm_seg *)seg_handle;
    return seg->devid;
}

u64 svmm_get_seg_svm_flag(void *seg_handle)
{
    struct svmm_seg *seg = (struct svmm_seg *)seg_handle;
    return seg->svm_flag;
}

void svmm_mod_seg_svm_flag(void *seg_handle, u64 flag)
{
    struct svmm_seg *seg = (struct svmm_seg *)seg_handle;
    seg->svm_flag = flag;
}

void svmm_mod_seg_src_tgid(void *seg_handle, int tgid)
{
    struct svmm_seg *seg = (struct svmm_seg *)seg_handle;
    seg->src_info.tgid = tgid;
}

int svmm_for_each_seg_handle(struct svmm_inst *svmm_inst,
                             int (*func)(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv),
                             void *priv)
{
    struct rbtree_node *node = NULL, *tmp = NULL;
    int ret = 0;

    (void)pthread_rwlock_rdlock(&svmm_inst->rwlock);
    rbtree_node_for_each_prev_safe(node, tmp, &svmm_inst->root)
    {
        struct svmm_seg *seg = rb_entry(node, struct svmm_seg, node);
        struct svm_global_va src_info = seg->src_info;
        ret = func((void *)seg, seg->start, &src_info, priv);
        if (ret != 0) {
            break;
        }
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    return ret;
}

void svmm_non_overlap_init(struct svmm_inst *svmm_inst)
{
    rbtree_init(&svmm_inst->root);
}

static void _svmm_non_overlap_del_seg_handle(struct svmm_inst *svmm_inst, struct svmm_seg *seg)
{
    svmm_remove_seg(svmm_inst, seg);
    (void)svmm_seg_priv_release(seg, true);
    svmm_seg_destroy(seg);
}

void svmm_non_overlap_uninit(struct svmm_inst *svmm_inst)
{
    struct rbtree_node *node = NULL, *tmp = NULL;
    u32 recycle_num = 0;

    pthread_rwlock_wrlock(&svmm_inst->rwlock);
    rbtree_node_for_each_prev_safe(node, tmp, &svmm_inst->root)
    {
        struct svmm_seg *seg = rb_entry(node, struct svmm_seg, node);
        _svmm_non_overlap_del_seg_handle(svmm_inst, seg);
        recycle_num++;
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    if (recycle_num > 0) {
        svm_warn("Uninit with seg. (svmma_start=0x%llx; svmma_size=0x%llx; recycle_num=%u)\n", svmm_inst->svmma_start,
                 svmm_inst->svmma_size, recycle_num);
    }
}

void svmm_non_overlap_del_seg_handle(struct svmm_inst *svmm_inst, void *seg_handle)
{
    struct svmm_seg *seg = (struct svmm_seg *)seg_handle;
    _svmm_non_overlap_del_seg_handle(svmm_inst, seg);
}