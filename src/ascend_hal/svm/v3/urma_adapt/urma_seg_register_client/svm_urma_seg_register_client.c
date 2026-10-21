/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "svm_urma_def.h"
#include "svm_pub.h"
#include "svm_addr_desc.h"
#include "svm_log.h"
#include "svm_user_adapt.h"
#include "svm_umc_client.h"
#include "svm_sub_event_type_uk_msg.h"
#include "svm_dbi.h"
#include "svm_apbi.h"
#include "svm_urma_seg_msg.h"
#include "svm_urma_seg_local.h"
#include "svm_urma_seg_register_client.h"

static int svm_urma_remote_register_seg(struct svm_dst_va *dst_va, u32 seg_flag, urma_seg_t *seg, u32 *token_id,
                                        u32 *token_val)
{
    struct svm_umc_msg_head head;
    struct svm_urma_seg_register_msg reg_msg = {
        .head.va = dst_va->va, .head.size = dst_va->size, .head.flag = seg_flag};
    struct svm_umc_msg msg = {.msg_in = (char *)(uintptr_t)&reg_msg,
                              .msg_in_len = sizeof(struct svm_urma_seg_register_msg),
                              .msg_out = (char *)(uintptr_t)&reg_msg,
                              .msg_out_len = sizeof(struct svm_urma_seg_register_msg)};
    struct svm_apbi apbi;
    int ret;

    ret = svm_apbi_query(dst_va->devid, DEVDRV_PROCESS_CP1, &apbi);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_umc_msg_head_pack(dst_va->devid, apbi.tgid, apbi.grp_id, SVM_URMA_SEG_REGISTER_EVENT, &head);
    ret = svm_umc_h2d_send(&head, &msg);
    if (ret == DRV_ERROR_NONE) {
        if (reg_msg.valid == 1) {
            *seg = reg_msg.seg;
            *token_id = reg_msg.head.token_id;
            *token_val = reg_msg.token_val;
        } else {
            ret = DRV_ERROR_NOT_SUPPORT;
        }
    } else if (ret == DRV_ERROR_NO_PROCESS) {
        (void)svm_apbi_clear(dst_va->devid, DEVDRV_PROCESS_CP1);
    }

    return ret;
}

static int svm_urma_remote_unregister_seg(u32 devid, u64 va, u64 size, u32 seg_flag)
{
    struct svm_umc_msg_head head;
    struct svm_urma_seg_unregister_msg unreg_msg = {.head.va = va, .head.size = size, .head.flag = seg_flag};
    struct svm_umc_msg msg = {.msg_in = (char *)(uintptr_t)&unreg_msg,
                              .msg_in_len = sizeof(struct svm_urma_seg_unregister_msg),
                              .msg_out = NULL,
                              .msg_out_len = 0};
    struct svm_apbi apbi;
    int ret;

    ret = svm_apbi_query(devid, DEVDRV_PROCESS_CP1, &apbi);
    if (ret != DRV_ERROR_NONE) {
        /* process exit, no need msg to unregister seg, return ok derictly */
        return (ret == DRV_ERROR_NO_PROCESS) ? DRV_ERROR_NONE : ret;
    }

    svm_umc_msg_head_pack(devid, apbi.tgid, apbi.grp_id, SVM_URMA_SEG_UNREGISTER_EVENT, &head);
    ret = svm_umc_h2d_send(&head, &msg);
    if (ret == DRV_ERROR_NO_PROCESS) {
        (void)svm_apbi_clear(devid, DEVDRV_PROCESS_CP1);
    }

    return (ret == DRV_ERROR_NO_PROCESS) ? DRV_ERROR_NONE : ret;
}

#ifdef CFG_FEATURE_SUPPORT_UB
static int svm_urma_register_seg_client_local(u32 user_devid, struct svm_dst_va *dst_va,
                                              struct svm_urma_client_seg *client_seg, u32 seg_flag)
{
    struct svm_urma_seg_info seg_info;
    u64 start = dst_va->va;
    u64 size = dst_va->size;
    u32 devid = user_devid;
    int ret;

    ret = svm_urma_seg_local_register(devid, start, size, seg_flag);
    if (ret != DRV_ERROR_NONE) {
        svm_debug("Register seg check. (devid=%u; start=0x%llx; size=%llu; flag=0x%x)\n", devid, start, size, seg_flag);
        return ret;
    }

    ret = svm_urma_seg_local_get_info(devid, start, seg_flag, &seg_info);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get seg info failed. (devid=%u; start=0x%llx; size=%llu; flag=0x%x)\n", devid, start, size, seg_flag);
        (void)svm_urma_seg_local_unregister(devid, start, size, seg_flag);
        return ret;
    }

    client_seg->tseg = seg_info.tseg;
    client_seg->seg = seg_info.tseg->seg;
    client_seg->token_id = seg_info.token_id;
    client_seg->token_val = seg_info.token_val;
    client_seg->seg_flag = seg_info.seg_flag;
    return DRV_ERROR_NONE;
}

static int svm_urma_unregister_seg_client_local(u32 user_devid, u64 va, u64 size, u32 seg_flag)
{
    return svm_urma_seg_local_unregister(user_devid, va, size, seg_flag);
}

static urma_target_seg_t *svm_urma_seg_import(u32 devid, u32 token_val, urma_seg_t *seg)
{
    void *urma_ctx = NULL;
    urma_target_seg_t *tseg = NULL;
    urma_import_seg_flag_t flag = {0};
    urma_token_t token;

    urma_ctx = ascend_urma_ctx_get(devid);
    if (urma_ctx == NULL) {
        svm_err("Get urma ctx failed. (devid=%u)\n", devid);
        return NULL;
    }

    flag.bs.cacheable = URMA_NON_CACHEABLE;
    flag.bs.access = URMA_ACCESS_READ | URMA_ACCESS_WRITE | URMA_ACCESS_ATOMIC;
    flag.bs.mapping = URMA_SEG_NOMAP;
    flag.bs.reserved = 0;

    token.token = token_val;
    tseg = urma_import_seg(ascend_to_urma_ctx(urma_ctx), seg, &token, 0, flag);
    ascend_urma_ctx_put(urma_ctx);
    if (tseg == NULL) {
        svm_err("Urma import seg failed.\n");
    }

    return tseg;
}

static void svm_urma_seg_unimport(urma_target_seg_t *tseg)
{
    (void)urma_unimport_seg(tseg);
}
#else
static int svm_urma_register_seg_client_local(u32 user_devid, struct svm_dst_va *dst_va,
                                              struct svm_urma_client_seg *client_seg, u32 seg_flag)
{
    SVM_UNUSED(user_devid);
    SVM_UNUSED(dst_va);
    SVM_UNUSED(client_seg);
    SVM_UNUSED(seg_flag);

    return DRV_ERROR_NOT_SUPPORT;
}

static int svm_urma_unregister_seg_client_local(u32 user_devid, u64 va, u64 size, u32 seg_flag)
{
    SVM_UNUSED(user_devid);
    SVM_UNUSED(va);
    SVM_UNUSED(size);
    SVM_UNUSED(seg_flag);

    return DRV_ERROR_NOT_SUPPORT;
}

static urma_target_seg_t *svm_urma_seg_import(u32 devid, u32 token_val, urma_seg_t *seg)
{
    SVM_UNUSED(devid);
    SVM_UNUSED(token_val);
    SVM_UNUSED(seg);

    return (urma_target_seg_t *)seg;
}

static void svm_urma_seg_unimport(urma_target_seg_t *tseg)
{
    SVM_UNUSED(tseg);
}
#endif

static int svm_urma_register_seg_client_remote(u32 user_devid, struct svm_dst_va *dst_va,
                                               struct svm_urma_client_seg *client_seg, u32 seg_flag)
{
    urma_seg_t seg;
    u32 token_id, token_val;
    int ret;
    SVM_UNUSED(user_devid);

    ret = svm_urma_remote_register_seg(dst_va, seg_flag, &seg, &token_id, &token_val);
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            svm_debug("Remote register urma seg check. (ret=%d; devid=%u; va=0x%llx; size=%llu; seg_flag=0x%x)\n", ret,
                      dst_va->devid, dst_va->va, dst_va->size, seg_flag);
        }
        return ret;
    }

    if (!svm_urma_seg_flag_is_self_user(seg_flag)) {
        urma_target_seg_t *tseg = svm_urma_seg_import(dst_va->devid, token_val, &seg);
        if (tseg == NULL) {
            (void)svm_urma_remote_unregister_seg(dst_va->devid, dst_va->va, dst_va->size, seg_flag);
            return DRV_ERROR_INNER_ERR;
        }

        client_seg->tseg = tseg;
        client_seg->seg = tseg->seg;
        client_seg->token_id = token_id;
        client_seg->token_val = token_val;
        client_seg->seg_flag = seg_flag;
    }

    return DRV_ERROR_NONE;
}

static int svm_urma_unregister_seg_client_remote(u32 user_devid, struct svm_dst_va *dst_va,
                                                 struct svm_urma_client_seg *client_seg, u32 seg_flag)
{
    SVM_UNUSED(user_devid);

    if (!svm_urma_seg_flag_is_self_user(seg_flag)) {
        svm_urma_seg_unimport(client_seg->tseg);
    }

    return svm_urma_remote_unregister_seg(dst_va->devid, dst_va->va, dst_va->size, seg_flag);
}

int svm_urma_register_seg_client(u32 user_devid, struct svm_dst_va *dst_va, struct svm_urma_client_seg *client_seg,
                                 u32 seg_flag)
{
    if (dst_va->devid != svm_get_host_devid()) {
        return svm_urma_register_seg_client_remote(user_devid, dst_va, client_seg, seg_flag);
    } else {
        return svm_urma_register_seg_client_local(user_devid, dst_va, client_seg, seg_flag);
    }
}

int svm_urma_unregister_seg_client(u32 user_devid, struct svm_dst_va *dst_va, struct svm_urma_client_seg *client_seg,
                                   u32 seg_flag)
{
    if (dst_va->devid != svm_get_host_devid()) {
        return svm_urma_unregister_seg_client_remote(user_devid, dst_va, client_seg, seg_flag);
    } else {
        return svm_urma_unregister_seg_client_local(user_devid, dst_va->va, dst_va->size, client_seg->seg_flag);
    }
}
