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
#include "svm_atomic.h"
#include "svm_log.h"
#include "svmm.h"
#include "svmm_inst.h"
#include "svmm_dev_non_overlap.h"

struct svmm_dev_non_overlap_seg {
    struct svmm_seg_head head;
    struct rbtree_node node;
    pthread_rwlock_t rwlock;
    u32 ref;
    u32 devid;
    u64 start;
    u64 size;
    u64 svm_flag;
    struct svm_global_va src_info;
};

static void rb_range_of_svmm_dev_non_overlap_seg(struct rbtree_node *node, struct rb_range_handle *range)
{
    struct svmm_dev_non_overlap_seg *seg = rb_entry(node, struct svmm_dev_non_overlap_seg, node);

    range->start = seg->start;
    range->end = seg->start + seg->size - 1ULL;
}

static struct svmm_dev_non_overlap_seg *svmm_dev_non_overlap_seg_search(struct svmm_inst *svmm_inst, u32 devid, u64 va,
                                                                        u64 size)
{
    struct rb_range_handle rb_range = {.start = va, .end = va + size - 1};
    struct rbtree_node *node = NULL;

    node = rbtree_search_by_range(&svmm_inst->dev_root[devid], &rb_range, rb_range_of_svmm_dev_non_overlap_seg);
    if (node == NULL) {
        return NULL;
    }

    return rb_entry(node, struct svmm_dev_non_overlap_seg, node);
}

static struct svmm_dev_non_overlap_seg *svmm_dev_non_overlap_upper_bound_seg_search(struct svmm_inst *svmm_inst,
                                                                                    u32 devid, u64 va)
{
    struct rbtree_node *node = NULL;

    node = rbtree_search_upper_bound_range(&svmm_inst->dev_root[devid], va, rb_range_of_svmm_dev_non_overlap_seg);
    if (node == NULL) {
        return NULL;
    }

    return rb_entry(node, struct svmm_dev_non_overlap_seg, node);
}

static struct svmm_dev_non_overlap_seg *svmm_dev_non_overlap_seg_next(struct svmm_dev_non_overlap_seg *seg)
{
    struct rbtree_node *next = NULL;

    next = rbtree_next(&seg->node);
    if (next == NULL) {
        return NULL;
    }

    return rb_entry(next, struct svmm_dev_non_overlap_seg, node);
}

static int svmm_dev_non_overlap_seg_insert(struct svmm_inst *svmm_inst, struct svmm_dev_non_overlap_seg *seg)
{
    int ret = rbtree_insert_by_range(&svmm_inst->dev_root[seg->devid], &seg->node,
                                     rb_range_of_svmm_dev_non_overlap_seg);
    if (ret == 0) {
        svmm_inst->seg_num++;
    }

    return (ret != 0) ? DRV_ERROR_REPEATED_USERD : 0;
}

static void svmm_dev_non_overlap_seg_erase(struct svmm_inst *svmm_inst, struct svmm_dev_non_overlap_seg *seg)
{
    svmm_inst->seg_num--;
    _rbtree_erase(&svmm_inst->dev_root[seg->devid], &seg->node);
}

static u32 svmm_dev_non_overlap_seg_show(struct svmm_dev_non_overlap_seg *seg, int id, char *buf, u32 buf_len)
{
    struct svm_global_va *src = &seg->src_info;
    int tmp_id = id;

    if (buf == NULL) {
        svm_info("id %d: seg info: ref %d devid %u start 0x%llx size 0x%llx, svm_flag 0x%llx\n"
                 "    seg src: udevid %u tgid %d va 0x%llx size 0x%llx\n",
                 tmp_id++, seg->ref, seg->devid, seg->start, seg->size, seg->svm_flag, src->udevid, src->tgid, src->va,
                 src->size);
        return 0;
    } else {
        if (buf_len == 0) {
            return 0;
        }
        int len = snprintf_s(buf, buf_len, buf_len - 1,
                             "id %d: seg info: ref %d devid %u start 0x%llx size 0x%llx, svm_flag 0x%llx\n"
                             "    seg src: udevid %u tgid %d va 0x%llx size 0x%llx\n",
                             tmp_id++, seg->ref, seg->devid, seg->start, seg->size, seg->svm_flag, src->udevid,
                             src->tgid, src->va, src->size);
        return (len < 0) ? 0 : (u32)len;
    }
}

u32 svmm_dev_non_overlap_show(struct svmm_inst *svmm_inst, char *buf, u32 buf_len)
{
    struct rbtree_node *node = NULL;
    u32 devid;
    int id = 0;
    u32 len = 0;

    (void)pthread_rwlock_rdlock(&svmm_inst->rwlock);
    for (devid = 0; devid < SVM_MAX_DEV_NUM; devid++) {
        rbtree_node_for_each(node, &svmm_inst->dev_root[devid])
        {
            struct svmm_dev_non_overlap_seg *seg = rb_entry(node, struct svmm_dev_non_overlap_seg, node);
            len += svmm_dev_non_overlap_seg_show(seg, id++, buf + len, buf_len - len);
        }
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    return len;
}

static struct svmm_dev_non_overlap_seg *svmm_dev_non_overlap_seg_create(u32 devid, u64 start, u64 size, u64 svm_flag,
                                                                        struct svm_global_va *src_info)
{
    struct svmm_dev_non_overlap_seg *seg = NULL;

    seg = (struct svmm_dev_non_overlap_seg *)svm_ua_calloc(1, sizeof(struct svmm_dev_non_overlap_seg));
    if (seg == NULL) {
        svm_err("Calloc seg failed. (size=0x%lx)\n", sizeof(*seg));
        return NULL;
    }

    seg->head.overlap_type = SVMM_DEV_NON_OVERLAP;
    seg->ref = 0;
    seg->devid = devid;
    seg->start = start;
    seg->size = size;
    seg->svm_flag = svm_flag;
    seg->src_info = *src_info;
    RB_CLEAR_NODE(&seg->node);
    (void)pthread_rwlock_init(&seg->rwlock, NULL);

    return seg;
}

static void svmm_dev_non_overlap_seg_destroy(struct svmm_dev_non_overlap_seg *seg)
{
    svm_ua_free(seg);
}

int svmm_dev_non_overlap_add_seg(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 svm_flag,
                                 struct svm_global_va *src_info)
{
    struct svmm_dev_non_overlap_seg *seg = NULL;
    u64 size = src_info->size;
    int ret;

    seg = svmm_dev_non_overlap_seg_create(devid, start, size, svm_flag, src_info);
    if (seg == NULL) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    pthread_rwlock_wrlock(&svmm_inst->rwlock);
    ret = svmm_dev_non_overlap_seg_insert(svmm_inst, seg);
    pthread_rwlock_unlock(&svmm_inst->rwlock);
    if (ret != 0) {
        svmm_dev_non_overlap_seg_destroy(seg);
        svm_err("Insert failed. (devid=%u; start=0x%llx; size=0x%llx)\n", devid, start, size);
    }

    return ret;
}

int svmm_dev_non_overlap_can_add_seg(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 size)
{
    struct rb_range_handle range = {.start = start, .end = start + size - 1ULL};
    bool can_insert;

    pthread_rwlock_rdlock(&svmm_inst->rwlock);
    can_insert = rbtree_can_insert_range(&svmm_inst->dev_root[devid], &range, rb_range_of_svmm_dev_non_overlap_seg);
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    return can_insert ? 0 : DRV_ERROR_REPEATED_USERD;
}

int svmm_dev_non_overlap_del_seg(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 size, bool force, u64 *svm_flag)
{
    struct svmm_dev_non_overlap_seg *seg = NULL;
    u32 ref;
    SVM_UNUSED(force);

    pthread_rwlock_wrlock(&svmm_inst->rwlock);
    seg = svmm_dev_non_overlap_seg_search(svmm_inst, devid, start, size);
    if (seg == NULL) {
        pthread_rwlock_unlock(&svmm_inst->rwlock);
        svm_err("Invalid para. (devid=%u; start=0x%llx; size=0x%llx)\n", devid, start, size);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((start != seg->start) || (size != seg->size)) {
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

    if (svm_flag != NULL) {
        *svm_flag = seg->svm_flag;
    }
    svmm_dev_non_overlap_seg_erase(svmm_inst, seg);
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    svmm_dev_non_overlap_seg_destroy(seg);
    return 0;
}

static void svmm_dev_non_overlap_get_seg_info(struct svmm_dev_non_overlap_seg *seg, u32 *devid, u64 *va, u64 *svm_flag,
                                              struct svm_global_va *src_info)
{
    *devid = seg->devid;
    *va = seg->start;
    *svm_flag = seg->svm_flag;
    *src_info = seg->src_info;
}

static int svmm_dev_non_overlap_get_seg_by_va(struct svmm_inst *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                              struct svm_global_va *src_info)
{
    SVM_UNUSED(svmm_inst);
    SVM_UNUSED(devid);
    SVM_UNUSED(va);
    SVM_UNUSED(svm_flag);
    SVM_UNUSED(src_info);

    return DRV_ERROR_INNER_ERR;
}

static int svmm_dev_non_overlap_get_seg_by_devid(struct svmm_inst *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                                 struct svm_global_va *src_info)
{
    struct rbtree_node *node = NULL;

    if (*devid >= SVM_MAX_DEV_NUM) {
        return DRV_ERROR_NOT_EXIST;
    }

    (void)pthread_rwlock_rdlock(&svmm_inst->rwlock);
    rbtree_node_for_each(node, &svmm_inst->dev_root[*devid])
    {
        struct svmm_dev_non_overlap_seg *seg = rb_entry(node, struct svmm_dev_non_overlap_seg, node);
        if (((src_info->udevid == seg->src_info.udevid) || (src_info->udevid == SVM_INVALID_UDEVID)) &&
            ((src_info->va == seg->src_info.va) || (src_info->va == 0)) &&
            ((src_info->size == seg->src_info.size) || (src_info->size == 0))) {
            svmm_dev_non_overlap_get_seg_info(seg, devid, va, svm_flag, src_info);
            pthread_rwlock_unlock(&svmm_inst->rwlock);
            return 0;
        }
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);
    return DRV_ERROR_NOT_EXIST;
}

static int svmm_dev_non_overlap_get_first_seg(struct svmm_inst *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                              struct svm_global_va *src_info)
{
    struct rbtree_node *node = NULL;
    u32 tmp_devid;

    (void)pthread_rwlock_rdlock(&svmm_inst->rwlock);
    for (tmp_devid = 0; tmp_devid < SVM_MAX_DEV_NUM; tmp_devid++) {
        node = rbtree_first(&svmm_inst->dev_root[tmp_devid]);
        if (node != NULL) {
            struct svmm_dev_non_overlap_seg *seg = rb_entry(node, struct svmm_dev_non_overlap_seg, node);
            svmm_dev_non_overlap_get_seg_info(seg, devid, va, svm_flag, src_info);
            pthread_rwlock_unlock(&svmm_inst->rwlock);
            return 0;
        }
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);
    return DRV_ERROR_NOT_EXIST;
}

int svmm_dev_non_overlap_get_seg(struct svmm_inst *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                                 struct svm_global_va *src_info)
{
    if (*va != 0) {
        return svmm_dev_non_overlap_get_seg_by_va(svmm_inst, devid, va, svm_flag, src_info);
    } else if ((*devid != SVM_INVALID_DEVID) || (src_info->udevid != SVM_INVALID_UDEVID)) {
        return svmm_dev_non_overlap_get_seg_by_devid(svmm_inst, devid, va, svm_flag, src_info);
    } else {
        return svmm_dev_non_overlap_get_first_seg(svmm_inst, devid, va, svm_flag, src_info);
    }
}

void svmm_dev_non_overlap_init(struct svmm_inst *svmm_inst)
{
    u32 devid;

    for (devid = 0; devid < SVM_MAX_DEV_NUM; devid++) {
        rbtree_init(&svmm_inst->dev_root[devid]);
    }
}

void svmm_dev_non_overlap_uninit(struct svmm_inst *svmm_inst)
{
    struct rbtree_node *node = NULL, *tmp = NULL;
    u32 recycle_num = 0;
    u32 devid;

    pthread_rwlock_wrlock(&svmm_inst->rwlock);
    for (devid = 0; devid < SVM_MAX_DEV_NUM; devid++) {
        rbtree_node_for_each_prev_safe(node, tmp, &svmm_inst->dev_root[devid])
        {
            struct svmm_dev_non_overlap_seg *seg = rb_entry(node, struct svmm_dev_non_overlap_seg, node);
            svmm_dev_non_overlap_seg_erase(svmm_inst, seg);
            svmm_dev_non_overlap_seg_destroy(seg);
            recycle_num++;
        }
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    if (recycle_num > 0) {
        svm_warn("Uninit with seg. (svmma_start=0x%llx; svmma_size=0x%llx; recycle_num=%u)\n", svmm_inst->svmma_start,
                 svmm_inst->svmma_size, recycle_num);
    }
}

static int svmm_dev_non_overlap_get_first_hole_by_upper(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 size,
                                                        u64 *hole_start, u64 *hole_size)
{
    struct svmm_dev_non_overlap_seg *seg = NULL;
    u64 hole_end;

    seg = svmm_dev_non_overlap_upper_bound_seg_search(svmm_inst, devid, start);
    hole_end = (seg != NULL) ? svm_min(start + size, seg->start) : start + size;

    *hole_start = start;
    *hole_size = hole_end - *hole_start;
    return DRV_ERROR_NONE;
}

static int _svmm_dev_non_overlap_get_first_hole(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 size,
                                                u64 *hole_start, u64 *hole_size)
{
    struct svmm_dev_non_overlap_seg *seg = NULL;
    u64 cur_va = start;

    seg = svmm_dev_non_overlap_seg_search(svmm_inst, devid, start, 1);
    if (seg == NULL) {
        return svmm_dev_non_overlap_get_first_hole_by_upper(svmm_inst, devid, start, size, hole_start, hole_size);
    }

    while (1) {
        if (cur_va < seg->start) {
            *hole_start = cur_va;
            *hole_size = seg->start - *hole_start;
            return DRV_ERROR_NONE;
        }

        cur_va = seg->start + seg->size;
        if (cur_va >= (start + size)) {
            break;
        }

        seg = svmm_dev_non_overlap_seg_next(seg);
        if (seg == NULL) {
            *hole_start = cur_va;
            *hole_size = start + size - *hole_start;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_NOT_EXIST;
}

int svmm_dev_non_overlap_get_first_hole(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 size, u64 *hole_start,
                                        u64 *hole_size)
{
    int ret;

    pthread_rwlock_rdlock(&svmm_inst->rwlock);
    ret = _svmm_dev_non_overlap_get_first_hole(svmm_inst, devid, start, size, hole_start, hole_size);
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    return ret;
}

static int svmm_dev_non_overlap_for_each_seg_handle_by_dev_no_locked(
    struct svmm_inst *svmm_inst, u32 devid,
    int (*func)(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv), void *priv)
{
    struct rbtree_node *node = NULL, *tmp = NULL;

    rbtree_node_for_each_prev_safe(node, tmp, &svmm_inst->dev_root[devid])
    {
        struct svmm_dev_non_overlap_seg *seg = rb_entry(node, struct svmm_dev_non_overlap_seg, node);
        struct svm_global_va src_info = seg->src_info;
        int ret = func((void *)seg, seg->start, &src_info, priv);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

int svmm_dev_non_overlap_for_each_seg_handle(struct svmm_inst *svmm_inst,
                                             int (*func)(void *seg_handle, u64 start, struct svm_global_va *src_info,
                                                         void *priv),
                                             void *priv)
{
    u32 devid;
    int ret = 0;

    pthread_rwlock_wrlock(&svmm_inst->rwlock);
    for (devid = 0; devid < SVM_MAX_DEV_NUM; devid++) {
        ret = svmm_dev_non_overlap_for_each_seg_handle_by_dev_no_locked(svmm_inst, devid, func, priv);
        if (ret != 0) {
            break;
        }
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    return ret;
}

u32 svmm_dev_non_overlap_get_seg_devid(void *seg_handle)
{
    struct svmm_dev_non_overlap_seg *seg = (struct svmm_dev_non_overlap_seg *)seg_handle;
    return seg->devid;
}

void svmm_dev_non_overlap_mod_seg_src_tgid(void *seg_handle, int tgid)
{
    struct svmm_dev_non_overlap_seg *seg = (struct svmm_dev_non_overlap_seg *)seg_handle;
    seg->src_info.tgid = tgid;
}