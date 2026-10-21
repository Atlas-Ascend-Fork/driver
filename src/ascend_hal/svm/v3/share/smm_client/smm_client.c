/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "svm_log.h"
#include "svm_user_adapt.h"
#include "svm_umc_client.h"
#include "svm_sub_event_type_uk_msg.h"
#include "smm_uk_msg.h"
#include "svm_addr_desc.h"
#include "smm_client.h"
#include "smm_flag.h"
#include "svm_dbi.h"
#include "svm_apbi.h"

static int svm_smm_remote_map(struct svm_dst_va *dst_info, struct svm_global_va *src_info, u32 flag)
{
    struct svm_umc_msg_head head;
    struct svm_smm_map_msg mmap_msg = {.head = {0},
                                       .dst_task_type = dst_info->task_type,
                                       .dst_size = dst_info->size,
                                       .dst_va = dst_info->va,
                                       .src_info = *src_info,
                                       .flag = flag,
                                       .src_va = src_info->va};
    struct svm_umc_msg msg = {.msg_in = (char *)(uintptr_t)&mmap_msg,
                              .msg_in_len = sizeof(struct svm_smm_map_msg),
                              .msg_out = (char *)(uintptr_t)&mmap_msg,
                              .msg_out_len = sizeof(struct svm_smm_map_msg)};
    struct svm_apbi apbi;
    int ret;

    ret = svm_apbi_query(dst_info->devid, (int)dst_info->task_type, &apbi);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_umc_msg_head_pack(dst_info->devid, apbi.tgid, apbi.grp_id, SVM_SMM_MMAP_EVENT, &head);
    ret = svm_umc_h2d_send(&head, &msg);
    if (ret != DRV_ERROR_NONE) {
        if (ret == DRV_ERROR_NO_PROCESS) {
            svm_apbi_clear(dst_info->devid, (int)dst_info->task_type);
        } else {
            svm_err("Smm mmap msg handle failed. (devid=%u; devpid=%d; ret=%d)\n", dst_info->devid, apbi.tgid, ret);
        }
        return ret;
    }

    dst_info->va = mmap_msg.dst_va;
    return DRV_ERROR_NONE;
}

static int svm_smm_remote_unmap(struct svm_dst_va *dst_info, struct svm_global_va *src_info, u32 flag)
{
    struct svm_umc_msg_head head;
    struct svm_smm_unmap_msg munmap_msg = {.head = {0},
                                           .dst_task_type = dst_info->task_type,
                                           .dst_size = dst_info->size,
                                           .dst_va = dst_info->va,
                                           .src_info = *src_info,
                                           .flag = flag};
    struct svm_umc_msg msg = {.msg_in = (char *)(uintptr_t)&munmap_msg,
                              .msg_in_len = sizeof(struct svm_smm_unmap_msg),
                              .msg_out = (char *)(uintptr_t)&munmap_msg, /* Need um post handle. */
                              .msg_out_len = sizeof(struct svm_smm_unmap_msg)};
    struct svm_apbi apbi;
    int ret;

    ret = svm_apbi_query(dst_info->devid, (int)dst_info->task_type, &apbi);
    if (ret != DRV_ERROR_NONE) {
        /* process exit, no need msg to unmap, return ok derictly */
        return (ret == DRV_ERROR_NO_PROCESS) ? DRV_ERROR_NONE : ret;
    }

    svm_umc_msg_head_pack(dst_info->devid, apbi.tgid, apbi.grp_id, SVM_SMM_MUNMAP_EVENT, &head);
    ret = svm_umc_h2d_send(&head, &msg);
    if (ret != DRV_ERROR_NONE) {
        if (ret == DRV_ERROR_NO_PROCESS) {
            svm_apbi_clear(dst_info->devid, (int)dst_info->task_type);
        } else {
            svm_err("Smm unmap msg handle failed. (devid=%u; devpid=%d; ret=%d)\n", dst_info->devid, apbi.tgid, ret);
        }
        return (ret == DRV_ERROR_NO_PROCESS) ? DRV_ERROR_NONE : ret;
    }

    return DRV_ERROR_NONE;
}

int svm_smm_client_map(struct svm_dst_va *dst_info, struct svm_global_va *src_info, u32 flag)
{
    if (dst_info->devid == svm_get_host_devid()) {
        return svm_smm_mmap(dst_info->devid, &dst_info->va, dst_info->size, flag, src_info);
    } else {
        return svm_smm_remote_map(dst_info, src_info, flag);
    }
}

int svm_smm_client_unmap(struct svm_dst_va *dst_info, struct svm_global_va *src_info, u32 flag)
{
    if (dst_info->devid == svm_get_host_devid()) {
        return svm_smm_munmap(dst_info->devid, dst_info->va, dst_info->size, flag, src_info);
    } else {
        return svm_smm_remote_unmap(dst_info, src_info, flag);
    }
}
