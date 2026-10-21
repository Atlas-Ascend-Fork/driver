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
#include "svmm.h"
#include "svmm_inst.h"

struct svmm_overlap_seg {
    struct svmm_seg_head head;
    struct list_head node;
    pthread_rwlock_t rwlock;
    u32 devid;
    u64 start;
    u64 size;
    u64 svm_flag;
    struct svm_global_va src_info;
};

static struct svmm_overlap_seg *svmm_search_overlap_seg(struct svmm_inst *svmm_inst, u32 devid, u64 va, u64 size,
                                                        struct svm_global_va *src_info)
{
    struct list_head *node = NULL, *tmp = NULL;

    list_for_each_safe(node, tmp, &svmm_inst->head)
    {
        struct svmm_overlap_seg *seg = list_entry(node, struct svmm_overlap_seg, node);
        if (((devid == seg->devid) || (devid == SVM_INVALID_DEVID)) && ((va == seg->start) || (va == 0)) &&
            ((size == seg->size) || (size == 0)) &&
            ((src_info->udevid == seg->src_info.udevid) || (src_info->udevid == SVM_INVALID_UDEVID)) &&
            ((src_info->va == seg->src_info.va) || (src_info->va == 0)) &&
            ((src_info->size == seg->src_info.size) || (src_info->size == 0))) {
            return seg;
        }
    }

    return NULL;
}

static int svmm_add_overlap_seg(struct svmm_inst *svmm_inst, struct svmm_overlap_seg *seg)
{
    struct svm_global_va src_info = {0};
    src_info.udevid = SVM_INVALID_UDEVID;
    if (svmm_search_overlap_seg(svmm_inst, seg->devid, seg->start, seg->size, &src_info) != NULL) {
        return DRV_ERROR_REPEATED_INIT;
    }

    drv_user_list_add_tail(&seg->node, &svmm_inst->head);
    svmm_inst->seg_num++;
    return 0;
}

static void svmm_remove_overlap_seg(struct svmm_inst *svmm_inst, struct svmm_overlap_seg *seg)
{
    svmm_inst->seg_num--;
    drv_user_list_del(&seg->node);
}

static u32 svmm_overlap_show_seg(struct svmm_overlap_seg *seg, int id, char *buf, u32 buf_len)
{
    struct svm_global_va *src = &seg->src_info;
    int tmp_id = id;

    if (buf == NULL) {
        svm_info("id %d: seg info: devid %u start 0x%llx size 0x%llx, svm_flag 0x%llx\n"
                 "    seg src: udevid %u tgid %d va 0x%llx size 0x%llx\n",
                 tmp_id++, seg->devid, seg->start, seg->size, seg->svm_flag, src->udevid, src->tgid, src->va,
                 src->size);
        return 0;
    } else {
        if (buf_len == 0) {
            return 0;
        }
        int len = snprintf_s(buf, buf_len, buf_len - 1,
                             "id %d: seg info: devid %u start 0x%llx size 0x%llx, svm_flag 0x%llx\n"
                             "    seg src: udevid %u tgid %d va 0x%llx size 0x%llx\n",
                             tmp_id++, seg->devid, seg->start, seg->size, seg->svm_flag, src->udevid, src->tgid,
                             src->va, src->size);
        return (len < 0) ? 0 : (u32)len;
    }
}

u32 svmm_overlap_show(struct svmm_inst *svmm_inst, char *buf, u32 buf_len)
{
    struct list_head *node = NULL, *tmp = NULL;
    int id = 0;
    int len = 0;

    pthread_rwlock_rdlock(&svmm_inst->rwlock);
    list_for_each_safe(node, tmp, &svmm_inst->head)
    {
        struct svmm_overlap_seg *seg = list_entry(node, struct svmm_overlap_seg, node);
        len += (int)(svmm_overlap_show_seg(seg, id++, buf + len, buf_len - (u32)len));
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    return (u32)len;
}

static struct svmm_overlap_seg *svmm_overlap_seg_create(u32 devid, u64 start, u64 size, u64 svm_flag,
                                                        struct svm_global_va *src_info)
{
    struct svmm_overlap_seg *seg = NULL;

    seg = (struct svmm_overlap_seg *)svm_ua_calloc(1, sizeof(*seg));
    if (seg == NULL) {
        svm_err("Calloc seg failed. (size=0x%lx)\n", sizeof(*seg));
        return NULL;
    }

    seg->head.overlap_type = SVMM_OVERLAP;
    seg->devid = devid;
    seg->start = start;
    seg->size = size;
    seg->svm_flag = svm_flag;
    seg->src_info = *src_info;
    (void)pthread_rwlock_init(&seg->rwlock, NULL);

    return seg;
}

static void svmm_overlap_seg_destroy(struct svmm_overlap_seg *seg)
{
    svm_ua_free(seg);
}

int svmm_overlap_add_seg(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 svm_flag,
                         struct svm_global_va *src_info)
{
    struct svmm_overlap_seg *seg = NULL;
    u64 size = src_info->size;
    int ret;

    seg = svmm_overlap_seg_create(devid, start, size, svm_flag, src_info);
    if (seg == NULL) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    pthread_rwlock_wrlock(&svmm_inst->rwlock);
    ret = svmm_add_overlap_seg(svmm_inst, seg);
    pthread_rwlock_unlock(&svmm_inst->rwlock);
    if (ret != 0) {
        svmm_overlap_seg_destroy(seg);
        svm_err("Insert failed. (devid=%u; start=0x%llx; size=0x%llx)\n", devid, start, size);
    }

    return ret;
}

int svmm_overlap_can_add_seg(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 size)
{
    struct svm_global_va src_info = {0};
    struct svmm_overlap_seg *seg = NULL;

    src_info.udevid = SVM_INVALID_UDEVID;
    pthread_rwlock_rdlock(&svmm_inst->rwlock);
    seg = svmm_search_overlap_seg(svmm_inst, devid, start, size, &src_info);
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    return (seg == NULL) ? 0 : DRV_ERROR_REPEATED_INIT;
}

int svmm_overlap_del_seg(struct svmm_inst *svmm_inst, u32 devid, u64 start, u64 size, u64 *svm_flag)
{
    struct svmm_overlap_seg *seg = NULL;
    struct svm_global_va src_info = {0};

    pthread_rwlock_wrlock(&svmm_inst->rwlock);
    src_info.udevid = SVM_INVALID_UDEVID;
    seg = svmm_search_overlap_seg(svmm_inst, devid, start, size, &src_info);
    if (seg == NULL) {
        pthread_rwlock_unlock(&svmm_inst->rwlock);
        svm_err("Invalid para. (devid=%u; start=0x%llx; size=0x%llx)\n", devid, start, size);
        return DRV_ERROR_PARA_ERROR;
    }

    if (svm_flag != NULL) {
        *svm_flag = seg->svm_flag;
    }
    svmm_remove_overlap_seg(svmm_inst, seg);
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    svmm_overlap_seg_destroy(seg);
    return 0;
}

int svmm_overlap_get_seg(struct svmm_inst *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag,
                         struct svm_global_va *src_info)
{
    struct svmm_overlap_seg *seg = NULL;
    int ret = DRV_ERROR_NOT_EXIST;

    (void)pthread_rwlock_rdlock(&svmm_inst->rwlock);
    seg = svmm_search_overlap_seg(svmm_inst, *devid, *va, 0, src_info);
    if (seg != NULL) {
        *devid = seg->devid;
        *va = seg->start;
        *svm_flag = seg->svm_flag;
        *src_info = seg->src_info;
        ret = 0;
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    return ret;
}

void svmm_overlap_init(struct svmm_inst *svmm_inst)
{
    INIT_LIST_HEAD(&svmm_inst->head);
}

void svmm_overlap_uninit(struct svmm_inst *svmm_inst)
{
    struct list_head *node = NULL, *tmp = NULL;
    u32 recycle_num = 0;

    pthread_rwlock_wrlock(&svmm_inst->rwlock);
    list_for_each_safe(node, tmp, &svmm_inst->head)
    {
        struct svmm_overlap_seg *seg = list_entry(node, struct svmm_overlap_seg, node);
        svmm_overlap_seg_destroy(seg);
        recycle_num++;
    }
    pthread_rwlock_unlock(&svmm_inst->rwlock);

    if (recycle_num > 0) {
        svm_warn("Uninit with seg. (svmma_start=0x%llx; svmma_size=0x%llx; recycle_num=%u)\n", svmm_inst->svmma_start,
                 svmm_inst->svmma_size, recycle_num);
    }
}

void svmm_overlap_mod_seg_src_tgid(void *seg_handle, int tgid)
{
    struct svmm_overlap_seg *seg = (struct svmm_overlap_seg *)seg_handle;
    seg->src_info.tgid = tgid;
}