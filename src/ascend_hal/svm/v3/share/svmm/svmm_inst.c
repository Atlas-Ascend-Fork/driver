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
#include "svm_addr_desc.h"
#include "svmm.h"
#include "svmm_inst.h"
#include "svmm_overlap.h"
#include "svmm_non_overlap.h"
#include "svmm_dev_non_overlap.h"

void svm_svmm_inst_occupy_pipeline(void *svmm_inst)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;
    pthread_rwlock_wrlock(&inst->pipeline_rwlock);
}

void svm_svmm_inst_release_pipeline(void *svmm_inst)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;
    pthread_rwlock_unlock(&inst->pipeline_rwlock);
}

void *svm_svmm_create_inst(u64 svmma_start, u64 svmma_size, enum svmm_overlap_type overlap_type, u64 svm_flag)
{
    struct svmm_inst *inst = NULL;

    if (overlap_type >= SVMM_INVALID_OVERLAP_TYPE) {
        svm_err("Invalid overlap type. (overlap_type=%u)\n", overlap_type);
        return NULL;
    }

    inst = (struct svmm_inst *)svm_ua_calloc(1, sizeof(*inst));
    if (inst == NULL) {
        svm_err("Calloc inst failed. (size=%llu)\n", sizeof(*inst));
        return NULL;
    }

    inst->svmma_start = svmma_start;
    inst->svmma_size = svmma_size;
    inst->overlap_type = overlap_type;
    inst->svm_flag = svm_flag;
    (void)pthread_rwlock_init(&inst->rwlock, NULL);
    (void)pthread_rwlock_init(&inst->pipeline_rwlock, NULL);

    if (overlap_type == SVMM_OVERLAP) {
        svmm_overlap_init(inst);
    } else if (overlap_type == SVMM_NON_OVERLAP) {
        svmm_non_overlap_init(inst);
    } else if (overlap_type == SVMM_DEV_NON_OVERLAP) {
        svmm_dev_non_overlap_init(inst);
    }

    return (void *)inst;
}

void svm_svmm_destroy_inst(void *svmm_inst)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;

    if (inst->overlap_type == SVMM_OVERLAP) {
        svmm_overlap_uninit(inst);
    } else if (inst->overlap_type == SVMM_NON_OVERLAP) {
        svmm_non_overlap_uninit(inst);
    } else if (inst->overlap_type == SVMM_DEV_NON_OVERLAP) {
        svmm_dev_non_overlap_uninit(inst);
    }

    svm_ua_free(inst);
}

void svm_svmm_parse_inst_info(void *svmm_inst, u64 *svmma_start, u64 *svmma_size, u64 *svm_flag)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;

    *svmma_start = inst->svmma_start;
    *svmma_size = inst->svmma_size;
    *svm_flag = inst->svm_flag;
}

u32 svm_svmm_inst_show_detail(void *svmm_inst, char *buf, u32 buf_len)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;
    u32 head_len = 0, data_len = 0;

    if (buf == NULL) {
        svm_info("svmma_start 0x%llx svmma_size 0x%llx svm_flag 0x%llx overlap_type %d seg_num %llu \n",
                 inst->svmma_start, inst->svmma_size, inst->svm_flag, inst->overlap_type, inst->seg_num);
    } else {
        if (buf_len == 0) {
            return 0;
        }
        int len = snprintf_s(buf, buf_len, buf_len - 1,
                             "svmma_start 0x%llx svmma_size 0x%llx svm_flag 0x%llx overlap_type %d seg_num %llu \n",
                             inst->svmma_start, inst->svmma_size, inst->svm_flag, inst->overlap_type, inst->seg_num);
        if (len < 0) {
            return 0;
        }
        head_len = (u32)len;
    }

    if (inst->overlap_type == SVMM_OVERLAP) {
        data_len = svmm_overlap_show(inst, buf + head_len, buf_len - head_len);
    } else if (inst->overlap_type == SVMM_NON_OVERLAP) {
        data_len = svmm_non_overlap_show(inst, buf + head_len, buf_len - head_len);
    } else if (inst->overlap_type == SVMM_DEV_NON_OVERLAP) {
        data_len = svmm_dev_non_overlap_show(inst, buf + head_len, buf_len - head_len);
    }

    return (head_len + data_len);
}

static int svmm_seg_para_check(struct svmm_inst *inst, u64 start, u64 size)
{
    if ((start < inst->svmma_start) || ((start + size) > (inst->svmma_start + inst->svmma_size))) {
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

int svm_svmm_add_seg(void *svmm_inst, u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;
    int ret;

    ret = svmm_seg_para_check(inst, start, src_info->size);
    if (ret != 0) {
        svm_err("Invalid seg para. (svmma_start=0x%llx; svmma_size=0x%llx; seg_start=0x%llx; seg_size=0x%llx)\n",
                inst->svmma_start, inst->svmma_size, start, src_info->size);
        return ret;
    }

    if (inst->overlap_type == SVMM_OVERLAP) {
        return svmm_overlap_add_seg(inst, devid, start, svm_flag, src_info);
    } else if (inst->overlap_type == SVMM_NON_OVERLAP) {
        return svmm_non_overlap_add_seg(inst, devid, start, svm_flag, src_info);
    } else if (inst->overlap_type == SVMM_DEV_NON_OVERLAP) {
        return svmm_dev_non_overlap_add_seg(inst, devid, start, svm_flag, src_info);
    } else {
        return DRV_ERROR_INNER_ERR;
    }
}

int svm_svmm_can_add_seg(void *svmm_inst, u32 devid, u64 start, u64 size)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;
    int ret;

    ret = svmm_seg_para_check(inst, start, size);
    if (ret != 0) {
        svm_err("Invalid seg para. (svmma_start=0x%llx; svmma_size=0x%llx; seg_start=0x%llx; seg_size=0x%llx)\n",
                inst->svmma_start, inst->svmma_size, start, size);
        return ret;
    }

    if (inst->overlap_type == SVMM_OVERLAP) {
        return svmm_overlap_can_add_seg(inst, devid, start, size);
    } else if (inst->overlap_type == SVMM_NON_OVERLAP) {
        return svmm_non_overlap_can_add_seg(inst, start, size);
    } else if (inst->overlap_type == SVMM_DEV_NON_OVERLAP) {
        return svmm_dev_non_overlap_can_add_seg(inst, devid, start, size);
    } else {
        return DRV_ERROR_INNER_ERR;
    }
}

int svm_svmm_del_seg_with_flag(void *svmm_inst, u32 devid, u64 start, u64 size, bool force, u64 *svm_flag)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;
    int ret;

    ret = svmm_seg_para_check(inst, start, size);
    if (ret != 0) {
        svm_err("Invalid seg para. (svmma_start=0x%llx; svmma_size=0x%llx; seg_start=0x%llx; seg_size=0x%llx)\n",
                inst->svmma_start, inst->svmma_size, start, size);
        return ret;
    }

    if (inst->overlap_type == SVMM_OVERLAP) {
        return svmm_overlap_del_seg(inst, devid, start, size, svm_flag);
    } else if (inst->overlap_type == SVMM_NON_OVERLAP) {
        return svmm_non_overlap_del_seg(inst, devid, start, size, force, svm_flag);
    } else if (inst->overlap_type == SVMM_DEV_NON_OVERLAP) {
        return svmm_dev_non_overlap_del_seg(inst, devid, start, size, force, svm_flag);
    } else {
        return DRV_ERROR_INNER_ERR;
    }
}

/*
    (1) va=0: get first devid match segment
    (2) devid=SVM_INVALID_DEVID: get first va match segment
    (3) va=0 && devid=SVM_INVALID_DEVID: get first segment
*/
int svm_svmm_get_seg(void *svmm_inst, u32 *devid, u64 *va, u64 *svm_flag, struct svm_global_va *src_info)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;

    if (*va != 0) {
        int ret = svmm_seg_para_check(inst, *va, 1);
        if (ret != 0) {
            svm_debug("Invalid va. (svmma_start=0x%llx; svmma_size=0x%llx; va=0x%llx)\n", inst->svmma_start,
                      inst->svmma_size, *va);
            return ret;
        }
    }

    if (inst->overlap_type == SVMM_OVERLAP) {
        return svmm_overlap_get_seg(inst, devid, va, svm_flag, src_info);
    } else if (inst->overlap_type == SVMM_NON_OVERLAP) {
        return svmm_non_overlap_get_seg(inst, devid, va, svm_flag, src_info);
    } else if (inst->overlap_type == SVMM_DEV_NON_OVERLAP) {
        return svmm_dev_non_overlap_get_seg(inst, devid, va, svm_flag, src_info);
    } else {
        return DRV_ERROR_INNER_ERR;
    }
}

int svm_svmm_for_each_seg_handle(void *svmm_inst,
                                 int (*func)(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv),
                                 void *priv)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;

    if (inst->overlap_type == SVMM_OVERLAP) {
        return DRV_ERROR_INVALID_VALUE;
    } else if (inst->overlap_type == SVMM_NON_OVERLAP) {
        return svmm_for_each_seg_handle(inst, func, priv);
    } else if (inst->overlap_type == SVMM_DEV_NON_OVERLAP) {
        return svmm_dev_non_overlap_for_each_seg_handle(inst, func, priv);
    } else {
        return DRV_ERROR_INNER_ERR;
    }
}

void *svm_svmm_seg_handle_get(void *svmm_inst, u64 va)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;
    int ret;

    ret = svmm_seg_para_check(inst, va, 1);
    if (ret != 0) {
        svm_err("Invalid seg para. (svmma_start=0x%llx; svmma_size=0x%llx; va=0x%llx)\n", inst->svmma_start,
                inst->svmma_size, va);
        return NULL;
    }

    if ((inst->overlap_type == SVMM_OVERLAP) || (inst->overlap_type == SVMM_DEV_NON_OVERLAP)) {
        return NULL;
    }

    return svmm_seg_handle_get(inst, va);
}

void svm_svmm_seg_handle_put(void *seg_handle)
{
    svmm_seg_handle_put(seg_handle);
}

int svm_svmm_set_seg_priv(void *seg_handle, void *priv, struct svm_svmm_seg_priv_ops *priv_ops)
{
    return svmm_set_seg_priv(seg_handle, priv, priv_ops);
}

void *svm_svmm_get_seg_priv(void *seg_handle)
{
    return svmm_get_seg_priv(seg_handle);
}

u32 svm_svmm_get_seg_devid(void *seg_handle)
{
    struct svmm_seg_head *head = (struct svmm_seg_head *)seg_handle;

    if (head->overlap_type == SVMM_OVERLAP) {
        return SVM_INVALID_DEVID;
    } else if (head->overlap_type == SVMM_NON_OVERLAP) {
        return svmm_get_seg_devid(seg_handle);
    } else if (head->overlap_type == SVMM_DEV_NON_OVERLAP) {
        return svmm_dev_non_overlap_get_seg_devid(seg_handle);
    } else {
        return SVM_INVALID_DEVID;
    }
}

u64 svm_svmm_get_seg_svm_flag(void *seg_handle)
{
    return svmm_get_seg_svm_flag(seg_handle);
}

void svm_svmm_mod_seg_svm_flag(void *seg_handle, u64 flag)
{
    svmm_mod_seg_svm_flag(seg_handle, flag);
}

void svm_svmm_mod_seg_src_tgid(void *seg_handle, int tgid)
{
    struct svmm_seg_head *head = (struct svmm_seg_head *)seg_handle;

    if (head->overlap_type == SVMM_OVERLAP) {
        svmm_overlap_mod_seg_src_tgid(seg_handle, tgid);
    } else if (head->overlap_type == SVMM_NON_OVERLAP) {
        svmm_mod_seg_src_tgid(seg_handle, tgid);
    } else if (head->overlap_type == SVMM_DEV_NON_OVERLAP) {
        svmm_dev_non_overlap_mod_seg_src_tgid(seg_handle, tgid);
    }
}

void svm_svmm_del_seg_handle(void *svmm_inst, void *seg_handle)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;

    if ((inst->overlap_type == SVMM_OVERLAP) || (inst->overlap_type == SVMM_DEV_NON_OVERLAP)) {
        return;
    }

    svmm_non_overlap_del_seg_handle(svmm_inst, seg_handle);
}

int svm_svmm_get_first_hole(void *svmm_inst, u32 devid, u64 start, u64 size, u64 *hole_start, u64 *hole_size)
{
    struct svmm_inst *inst = (struct svmm_inst *)svmm_inst;

    if ((inst->overlap_type == SVMM_OVERLAP) || (inst->overlap_type == SVMM_NON_OVERLAP)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return svmm_dev_non_overlap_get_first_hole(inst, devid, start, size, hole_start, hole_size);
}
