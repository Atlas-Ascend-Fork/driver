/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "comm_user_interface.h"

#include "svm_urma_def.h"
#include "svm_pub.h"
#include "svm_addr_desc.h"
#include "svm_log.h"
#include "svm_user_adapt.h"
#include "svm_memcpy.h"
#include "svm_urma_seg_mng.h"
#include "svm_urma_chan.h"
#include "svm_addr_desc.h"
#include "svm_dbi.h"
#include "va_allocator.h"
#include "svm_memcpy_ops_register.h"
#include "svm_register_to_master.h"
#include "malloc_mng.h"
#include "svm_memcpy.h"
#include "svm_timestamp.h"

#define SVM_URMA_CHAN_WAIT_TIMEOUT_MS 60000ULL
#define SVM_URMA_CHAN_ALLOC_TIMEOUT_S 5
#define SVM_URMA_CPY_TRACE_BOUNDARY_SIZE (2 * SVM_BYTES_PER_MB)
#define SVM_URMA_CPY_TRACE_TIMESTAMP_THRESHOLD 500000ULL

static bool svm_urma_cpy_time_consume_abnormal(u64 size, u64 time_consume_ns)
{
    u64 threshold = (size < SVM_URMA_CPY_TRACE_BOUNDARY_SIZE) ?
                        SVM_URMA_CPY_TRACE_TIMESTAMP_THRESHOLD :
                        (size / SVM_URMA_CPY_TRACE_BOUNDARY_SIZE * SVM_URMA_CPY_TRACE_TIMESTAMP_THRESHOLD);
    return (time_consume_ns > threshold);
}

static urma_opcode_t svm_cpy_dir_to_urma_opcode(enum svm_cpy_dir dir)
{
    return (dir == SVM_H2D_CPY) ? URMA_OPC_WRITE : URMA_OPC_READ;
}

static int svm_ub_copy_urma_tseg_get(u32 src_devid, u32 dst_devid, struct svm_urma_chan_submit_para *submit_para,
                                     void *local_tseg)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_devid, dst_devid);
    u32 user_devid = (dir == SVM_H2D_CPY) ? dst_devid : src_devid;
    struct svm_dst_va dst_va;
    int ret;

    svm_dst_va_pack(src_devid, PROCESS_CP1, submit_para->src, submit_para->size, &dst_va);
    ret = svm_urma_get_tseg(user_devid, &dst_va, &submit_para->src_tseg);
    if ((ret != DRV_ERROR_NONE) && (local_tseg != NULL)) {
        submit_para->src_tseg = (urma_target_seg_t *)local_tseg;
        ret = DRV_ERROR_NONE;
    }
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get src tseg failed. (ret=%d; src_devid=%u; src_va=0x%llx; size=%llu)\n", ret, src_devid,
                submit_para->src, submit_para->size);
        return ret;
    }

    svm_dst_va_pack(dst_devid, PROCESS_CP1, submit_para->dst, submit_para->size, &dst_va);
    ret = svm_urma_get_tseg(user_devid, &dst_va, &submit_para->dst_tseg);
    if ((ret != DRV_ERROR_NONE) && (local_tseg != NULL)) {
        submit_para->dst_tseg = (urma_target_seg_t *)local_tseg;
        ret = DRV_ERROR_NONE;
    }
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get dst tseg failed. (ret=%d; dst_devid=%u; dst_va=0x%llx; dst_size=%llu)\n", ret, dst_devid,
                submit_para->dst, submit_para->size);
        return ret;
    }

    return ret;
}

static int svm_ub_async_copy_once(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info,
                                  u32 chan_id, void *local_tseg)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_info->devid, dst_info->devid);
    struct svm_urma_chan_submit_para submit_para = {
        .src = src_info->va, .dst = dst_info->va, .size = src_info->size, .opcode = svm_cpy_dir_to_urma_opcode(dir)};
    u64 cpy_size = src_info->size;
    u32 out_wr_num;
    int ret;

    svm_debug("(src_va=0x%llx; src_devid=%u; dst_va=0x%llx; dst_devid=%u)\n", src_info->va, src_info->devid,
              dst_info->va, dst_info->devid);
    ret = svm_ub_copy_urma_tseg_get(src_info->devid, dst_info->devid, &submit_para, local_tseg);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    while (cpy_size > 0) {
        u64 cur_cpy_size = svm_min(cpy_size, svm_urma_chan_max_once_cpy_size());

        submit_para.size = cur_cpy_size;
        ret = svm_urma_chan_submit(devid, chan_id, &submit_para, &out_wr_num);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Urma chan submit failed. (ret=%d; chan_id=%u)\n", ret, chan_id);
            return ret;
        }
        submit_para.src += cur_cpy_size;
        submit_para.dst += cur_cpy_size;
        cpy_size -= cur_cpy_size;
    }

    return ret;
}

static int svm_ub_async_copy_slice(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info,
                                   u32 chan_id, void *local_tseg)
{
    u32 host_devid = svm_get_host_devid();
    struct svm_copy_va_info *host_info = (src_info->devid == host_devid) ? src_info : dst_info;
    struct svm_copy_va_info *dev_info = (src_info->devid == host_devid) ? dst_info : src_info;
    u64 host_va = host_info->va;
    u64 dev_va = dev_info->va;
    u64 size = host_info->size;
    struct svm_prop prop;
    u64 tmp_va;
    int ret;

    /* For host vmm segmented halMemMap scene. */
    for (tmp_va = host_va; tmp_va < (host_va + size); tmp_va = (prop.start + prop.size)) {
        ret = svm_get_prop(tmp_va, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Va is not alloced. (ret=%d; va=0x%llx)\n", ret, tmp_va);
            return DRV_ERROR_INVALID_VALUE;
        }

        host_info->va = tmp_va;
        host_info->size = ((host_va + size) <= (prop.start + prop.size)) ? ((host_va + size)) - tmp_va :
                                                                           (prop.start + prop.size) - tmp_va;
        dev_info->va = dev_va + (tmp_va - host_va);
        dev_info->size = host_info->size;
        ret = svm_ub_async_copy_once(devid, src_info, dst_info, chan_id, local_tseg);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Ub sync failed. (ret=%d; devid=%u; host_va=0x%llx; dst_va=0x%llx; size=%llu; is_h2d=%d)\n", ret,
                    devid, host_info->va, dev_info->size, size, (src_info->devid == host_devid));
            return ret;
        }
    }

    return 0;
}

static bool svm_ub_async_copy_need_slice(u64 va, u64 size)
{
    struct svm_prop prop;
    int ret;

    if (svm_va_is_in_range(va, size) == false) {
        return false;
    }

    ret = svm_get_prop(va, &prop);
    if (ret != DRV_ERROR_NONE) {
        return false;
    }

    return (va + size) > (prop.start + prop.size);
}

static int svm_ub_async_copy(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info,
                             u32 chan_id, void *local_tseg)
{
    struct svm_copy_va_info *host_info = (src_info->devid == svm_get_host_devid()) ? src_info : dst_info;

    return (svm_ub_async_copy_need_slice(host_info->va, host_info->size)) ?
               svm_ub_async_copy_slice(devid, src_info, dst_info, chan_id, local_tseg) :
               svm_ub_async_copy_once(devid, src_info, dst_info, chan_id, local_tseg);
}

static int svm_ub_copy_chan_get_token(u32 devid, u32 chan_id, struct ascend_urma_token *token)
{
    urma_token_id_t *token_id = NULL;
    urma_token_t token_val;
    int ret;

    ret = svm_urma_chan_get_token(devid, chan_id, &token_id, &token_val);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get chan token failed. (devid=%u; chan_id=%u; ret=%d)\n", devid, chan_id, ret);
        return ret;
    }

    token->token_id = token_id;
    token->token_val = token_val;
    return DRV_ERROR_NONE;
}

/* Unregister a seg or put ref in context for UB copy */
static void svm_ub_copy_unregister_seg_in_context(u32 devid, struct svm_copy_va_info *host_info, void *tseg)
{
    if (tseg != NULL) {
        ascend_urma_unregister_segment((urma_target_seg_t *)tseg);
    } else if (svm_va_is_in_range(host_info->va, host_info->size) == false) {
        struct svm_dst_va check_va;
        svm_dst_va_pack(svm_get_host_devid(), 0, host_info->va, host_info->size, &check_va);
        svm_urma_tseg_put(devid, &check_va);
    }
}

/* Try to get an existing seg or register a new one for the host va */
static int svm_ub_copy_register_seg_in_context(u32 devid, struct svm_copy_va_info *host_info, enum svm_cpy_dir dir,
                                               u32 chan_id, void **local_tseg)
{
    struct svm_dst_va check_va;
    urma_target_seg_t *existing_tseg = NULL;
    struct ascend_urma_token token = {0};
    u32 seg_flag;
    int ret;

    /* SVM addresses already have tseg; the copy path looks it up for each mapped range. */
    if (svm_va_is_in_range(host_info->va, host_info->size)) {
        return DRV_ERROR_NONE;
    }

    svm_dst_va_pack(svm_get_host_devid(), 0, host_info->va, host_info->size, &check_va);
    if (svm_urma_tseg_get(devid, &check_va, &existing_tseg) == DRV_ERROR_NONE) {
        return DRV_ERROR_NONE;
    }

    ret = svm_ub_copy_chan_get_token(devid, chan_id, &token);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    seg_flag = SVM_URMA_SEG_FLAG_PIN | ((dir == SVM_D2H_CPY) ? SVM_URMA_SEG_FLAG_ACCESS_WRITE : 0);
    *local_tseg = ascend_urma_register_segment(devid, host_info->va, host_info->size, seg_flag, &token);
    if (*local_tseg == NULL) {
        svm_err("Register seg failed. (va=0x%llx; size=%llu; devid=%u)\n", host_info->va, host_info->size, devid);
        return DRV_ERROR_INNER_ERR;
    }
    return DRV_ERROR_NONE;
}

/* Submit async copy and wait for completion */
static int svm_ub_do_sync_copy(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info,
                               u32 chan_id, void *local_tseg)
{
    int ret;

    ret = svm_ub_async_copy(devid, src_info, dst_info, chan_id, local_tseg);
    if (ret != DRV_ERROR_NONE) {
        (void)svm_urma_chan_wait(devid, chan_id, -1, SVM_URMA_CHAN_WAIT_TIMEOUT_MS);
        svm_err("Urma async copy failed. (ret=%d; chan_id=%u; src_va=0x%llx; src_size=%llu; src_devid=%u; "
                "dst_va=0x%llx; dst_size=%llu; dst_devid=%u)\n",
                ret, chan_id, src_info->va, src_info->size, src_info->devid, dst_info->va, dst_info->size,
                dst_info->devid);
        return ret;
    }

    ret = svm_urma_chan_wait(devid, chan_id, -1, SVM_URMA_CHAN_WAIT_TIMEOUT_MS);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Urma chan wait failed. (ret=%d; chan_id=%u; src_va=0x%llx; src_size=%llu; src_devid=%u; "
                "dst_va=0x%llx; dst_size=%llu; dst_devid=%u)\n",
                ret, chan_id, src_info->va, src_info->size, src_info->devid, dst_info->va, dst_info->size,
                dst_info->devid);
    }

    return ret;
}

/* Do the actual copy: chan_alloc �?get_tseg/register �?copy �?unregister �?chan_free */
static int _svm_ub_sync_copy(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_info->devid, dst_info->devid);
    struct svm_copy_va_info *host_info = (src_info->devid == svm_get_host_devid()) ? src_info : dst_info;
    void *local_tseg = NULL;
    u32 chan_id;
    int ret;

    ret = svm_urma_chan_alloc(devid, &chan_id);
    if (ret != DRV_ERROR_NONE) {
        svm_err("svm_urma_chan_alloc failed. (ret=%d; src_va=0x%llx; src_size=%llu; src_devid=%u; dst_va=0x%llx; "
                "dst_size=%llu; dst_devid=%u)\n",
                ret, src_info->va, src_info->size, src_info->devid, dst_info->va, dst_info->size, dst_info->devid);
        return ret;
    }

    ret = svm_ub_copy_register_seg_in_context(devid, host_info, dir, chan_id, &local_tseg);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register seg failed. (ret=%d; devid=%u; va=0x%llx; size=%llu)\n", ret, devid, host_info->va,
                host_info->size);
        svm_urma_chan_free(devid, chan_id);
        return ret;
    }

    ret = svm_ub_do_sync_copy(devid, src_info, dst_info, chan_id, local_tseg);

    svm_ub_copy_unregister_seg_in_context(devid, host_info, local_tseg);
    svm_urma_chan_free(devid, chan_id);

    return ret;
}

int svm_ub_sync_copy(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    u64 start, end;
    int ret;

    start = svm_get_timestamp_ns();
    ret = _svm_ub_sync_copy(devid, src_info, dst_info);
    end = svm_get_timestamp_ns();
    if (svm_urma_cpy_time_consume_abnormal(src_info->size, end - start)) {
        svm_debug("urma memcpy time consume. (size=%llu; time=%llu ns)\n", src_info->size, end - start);
    }
    return ret;
}

static void svm_unregister_to_master_2d(struct svm_copy_va_2d_info *info, u64 user_devid, u64 count,
                                        enum svm_cpy_dir dir, bool *is_register, bool reg_once)
{
    struct svm_dst_va register_va;
    u32 host_devid = svm_get_host_devid();
    u32 flag = 0;
    u64 i;

    flag |= (dir == SVM_H2D_CPY) ? 0 : REGISTER_TO_MASTER_FLAG_ACCESS_WRITE;
    flag |= REGISTER_TO_MASTER_FLAG_PIN;

    if (reg_once) {
        svm_dst_va_pack(host_devid, 0, info->va, info->pitch * (info->height - 1) + info->width, &register_va);
        (void)svm_unregister_to_master((u32)user_devid, &register_va, flag);
        return;
    }

    for (i = 0; i < count; i++) {
        if (is_register[i] == false) {
            continue;
        }

        svm_dst_va_pack(host_devid, 0, info->va + i * info->pitch, info->width, &register_va);
        (void)svm_unregister_to_master((u32)user_devid, &register_va, flag);
    }
}

static int svm_register_to_master_2d(struct svm_copy_va_2d_info *info, u64 user_devid, enum svm_cpy_dir dir,
                                     bool *is_register, bool *reg_once)
{
    struct svm_dst_va register_va;
    u32 host_devid = svm_get_host_devid();
    u32 flag = 0;
    int ret;
    u64 i;

    if (svm_va_is_in_range(info->va, info->pitch * info->height)) {
        return DRV_ERROR_NONE;
    }

    flag |= (dir == SVM_H2D_CPY) ? 0 : REGISTER_TO_MASTER_FLAG_ACCESS_WRITE;
    flag |= REGISTER_TO_MASTER_FLAG_PIN;

    svm_dst_va_pack(host_devid, 0, info->va, info->pitch * (info->height - 1) + info->width, &register_va);
    ret = svm_register_to_master((u32)user_devid, &register_va, flag);
    if (ret == DRV_ERROR_NONE) {
        *reg_once = true;
        return DRV_ERROR_NONE;
    }

    for (i = 0; i < info->height; i++) {
        u64 va = info->va + i * info->pitch;
        u64 size = info->width;
        if (svm_va_is_in_range(va, size)) {
            continue;
        }

        svm_dst_va_pack(host_devid, 0, va, size, &register_va);
        ret = svm_register_to_master((u32)user_devid, &register_va, flag);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Register local mem to master failed. (ret=%d; host_va=0x%llx; size=%llu)\n", ret, va, size);
            svm_unregister_to_master_2d(info, user_devid, i, dir, is_register, false);
            return ret;
        }

        is_register[i] = true;
    }

    return DRV_ERROR_NONE;
}

int _svm_ub_sync_copy_2d(u32 devid, struct svm_copy_va_2d_info *src_info, struct svm_copy_va_2d_info *dst_info)
{
    u32 chan_id;
    int ret;
    u64 i;

    ret = svm_urma_chan_alloc(devid, &chan_id);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Svm_urma_chan_alloc failed. (ret=%d)\n", ret);
        return ret;
    }

    for (i = 0; i < src_info->height; i++) {
        struct svm_copy_va_info tmp_src_info = {
            .va = src_info->va + src_info->pitch * i, .size = dst_info->width, .devid = src_info->devid};
        struct svm_copy_va_info tmp_dst_info = {
            .va = dst_info->va + dst_info->pitch * i, .size = dst_info->width, .devid = dst_info->devid};

        ret = svm_ub_async_copy(devid, &tmp_src_info, &tmp_dst_info, chan_id, NULL);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Svm_ub_sync_copy_slice failed. (ret=%d; src_start=0x%llx; dst_start=0x%llx; width=%llu; i=%llu)\n",
                    ret, src_info->va, dst_info->va, src_info->width, i);
            (void)svm_urma_chan_wait(devid, chan_id, -1, SVM_URMA_CHAN_WAIT_TIMEOUT_MS);
            goto chan_free;
        }
    }

    ret = svm_urma_chan_wait(devid, chan_id, -1, SVM_URMA_CHAN_WAIT_TIMEOUT_MS);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Urma chan wait failed. (ret=%d; chan_id=%u)\n", ret, chan_id);
    }

chan_free:
    svm_urma_chan_free(devid, chan_id);
    return ret;
}

int svm_ub_sync_copy_2d(u32 devid, struct svm_copy_va_2d_info *src_info, struct svm_copy_va_2d_info *dst_info)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_info->devid, dst_info->devid);
    struct svm_copy_va_2d_info *register_info = (dir == SVM_H2D_CPY) ? src_info : dst_info;
    bool *is_register = NULL;
    bool reg_once = false;
    int ret;

    is_register = calloc(src_info->height, sizeof(bool));
    if (is_register == NULL) {
        svm_err("Copy 2d calloc failed. (height=%llu)\n", src_info->height);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = svm_register_to_master_2d(register_info, devid, dir, is_register, &reg_once);
    if (ret != DRV_ERROR_NONE) {
        free(is_register);
        return ret;
    }

    ret = _svm_ub_sync_copy_2d(devid, src_info, dst_info);

    svm_unregister_to_master_2d(register_info, devid, register_info->height, dir, is_register, reg_once);
    free(is_register);

    return ret;
}

static void svm_unregister_to_master_batch(u64 batch_va[], u64 batch_size[], u32 user_devid, u64 count,
                                           enum svm_cpy_dir dir, bool *is_register)
{
    struct svm_dst_va register_va;
    u32 host_devid = svm_get_host_devid();
    u32 flag = 0;
    u64 i;

    flag |= (dir == SVM_H2D_CPY) ? 0 : REGISTER_TO_MASTER_FLAG_ACCESS_WRITE;
    flag |= REGISTER_TO_MASTER_FLAG_PIN;

    for (i = 0; i < count; i++) {
        if (is_register[i] == false) {
            continue;
        }

        svm_dst_va_pack(host_devid, 0, batch_va[i], batch_size[i], &register_va);
        (void)svm_unregister_to_master(user_devid, &register_va, flag);
    }
}

static int svm_register_to_master_batch(u64 batch_va[], u64 batch_size[], u32 user_devid, u64 count,
                                        enum svm_cpy_dir dir, bool *is_register)
{
    struct svm_dst_va register_va;
    u32 host_devid = svm_get_host_devid();
    u32 flag = 0;
    u64 i;
    int ret;

    flag |= (dir == SVM_H2D_CPY) ? 0 : REGISTER_TO_MASTER_FLAG_ACCESS_WRITE;
    flag |= REGISTER_TO_MASTER_FLAG_PIN;

    for (i = 0; i < count; i++) {
        u64 va = batch_va[i];
        u64 size = batch_size[i];
        if (svm_va_is_in_range(va, size)) {
            continue;
        }

        svm_dst_va_pack(host_devid, 0, va, size, &register_va);
        ret = svm_register_to_master(user_devid, &register_va, flag);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Register local mem to master failed. (ret=%d; host_va=0x%llx; size=%llu)\n", ret, va, size);
            svm_unregister_to_master_batch(batch_va, batch_size, user_devid, i, dir, is_register);
            return ret;
        }

        is_register[i] = true;
    }

    return DRV_ERROR_NONE;
}

int _svm_ub_sync_copy_batch(u64 src[], u64 dst[], u64 size[], u64 count, u32 src_devid, u32 dst_devid)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_devid, dst_devid);
    u32 user_devid = (dir == SVM_H2D_CPY) ? dst_devid : src_devid;
    u32 chan_id;
    int ret;
    u64 i;

    ret = svm_urma_chan_alloc(user_devid, &chan_id);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Svm alloc jetty chan failed. (ret=%d)\n", ret);
        return ret;
    }

    for (i = 0; i < count; i++) {
        struct svm_copy_va_info tmp_src_info = {.va = src[i], .size = size[i], .devid = src_devid};
        struct svm_copy_va_info tmp_dst_info = {.va = dst[i], .size = size[i], .devid = dst_devid};

        ret = svm_ub_async_copy(user_devid, &tmp_src_info, &tmp_dst_info, chan_id, NULL);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Svm submit jetty cpy failed. (ret=%d; src=0x%llx; dst=0x%llx; size=%llu; i=%llu)\n", ret, src[i],
                    dst[i], size[i], i);
            (void)svm_urma_chan_wait(user_devid, chan_id, -1, SVM_URMA_CHAN_WAIT_TIMEOUT_MS);
            goto chan_free;
        }
    }

    ret = svm_urma_chan_wait(user_devid, chan_id, -1, SVM_URMA_CHAN_WAIT_TIMEOUT_MS);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Svm chan jetty wait failed. (ret=%d; chan_id=%u)\n", ret, chan_id);
    }

chan_free:
    svm_urma_chan_free(user_devid, chan_id);
    return ret;
}

int svm_ub_sync_copy_batch(u64 src[], u64 dst[], u64 size[], u64 count, u32 src_devid, u32 dst_devid)
{
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_devid, dst_devid);
    u32 user_devid = (dir == SVM_H2D_CPY) ? dst_devid : src_devid;
    u64 *register_va = (dir == SVM_H2D_CPY) ? src : dst;
    bool *is_register = NULL;
    int ret;

    is_register = calloc(count, sizeof(bool));
    if (is_register == NULL) {
        svm_err("Batch cpy calloc failed. (addr_count=%llu)\n", count);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = svm_register_to_master_batch(register_va, size, user_devid, count, dir, is_register);
    if (ret != DRV_ERROR_NONE) {
        free(is_register);
        return ret;
    }

    ret = _svm_ub_sync_copy_batch(src, dst, size, count, src_devid, dst_devid);

    svm_unregister_to_master_batch(register_va, size, user_devid, count, dir, is_register);
    free(is_register);

    return ret;
}

/* Two-level async hooks: alloc channel once, reuse across chunks (pipeline) */
static int svm_ub_async_chan_alloc(u32 devid, int *chan_id)
{
    u32 cid;
    int ret;

    ret = svm_urma_chan_try_alloc(devid, &cid, SVM_URMA_CHAN_ALLOC_TIMEOUT_S);
    if (ret != DRV_ERROR_NONE) {
        svm_warn("Urma chan alloc not success. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    *chan_id = (int)cid;
    return DRV_ERROR_NONE;
}

static void svm_ub_async_chan_free(u32 devid, int chan_id)
{
    svm_urma_chan_free(devid, (u32)chan_id);
}

static int svm_ub_async_copy_submit_reuse(u32 devid, int chan_id, struct svm_copy_va_info *src_info,
                                          struct svm_copy_va_info *dst_info)
{
    u32 cid = (u32)chan_id;
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_info->devid, dst_info->devid);
    struct svm_copy_va_info *host_info = (src_info->devid == svm_get_host_devid()) ? src_info : dst_info;
    void *local_tseg = NULL;
    int ret;

    if (dir == SVM_D2D_CPY) {
        svm_info("Ub async copy reuse not support d2d. (src_devid=%u; dst_devid=%u)\n", src_info->devid,
                 dst_info->devid);
        return DRV_ERROR_NOT_SUPPORT;
    }

    /* For SVM-range host memory (pipeline bounce buffer), register is a no-op (local_tseg stays NULL).
     * For non-SVM host, register once per channel; the tseg is cached in rbtree so repeated calls are cheap. */
    ret = svm_ub_copy_register_seg_in_context(devid, host_info, dir, cid, &local_tseg);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register seg failed. (ret=%d; devid=%u; va=0x%llx; size=%llu)\n", ret, devid, host_info->va,
                host_info->size);
        return ret;
    }

    ret = svm_ub_async_copy(devid, src_info, dst_info, cid, local_tseg);
    if (ret != DRV_ERROR_NONE) {
        (void)svm_urma_chan_wait(devid, cid, -1, SVM_URMA_CHAN_WAIT_TIMEOUT_MS);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int svm_ub_async_copy_wait_one(u32 devid, int chan_id)
{
    return svm_urma_chan_wait_one(devid, (u32)chan_id, SVM_URMA_CHAN_WAIT_TIMEOUT_MS);
}

static struct svm_copy_ops g_ub_copy_ops = {.sync_copy = svm_ub_sync_copy,
                                            .async_copy_submit = NULL,
                                            .async_copy_wait = NULL,
                                            .dma_desc_convert = NULL,
                                            .dma_desc_submit = NULL,
                                            .dma_desc_wait = NULL,
                                            .dma_desc_destroy = NULL,
                                            .dma_desc_convert_2d = NULL,
                                            .sync_copy_2d = svm_ub_sync_copy_2d,
                                            .sync_copy_batch = svm_ub_sync_copy_batch,
                                            .async_chan_alloc = svm_ub_async_chan_alloc,
                                            .async_chan_free = svm_ub_async_chan_free,
                                            .async_copy_submit_reuse = svm_ub_async_copy_submit_reuse,
                                            .async_copy_wait_one = svm_ub_async_copy_wait_one};

void svm_ub_memcpy_ops_register(u32 devid)
{
    svm_copy_ops_register(devid, &g_ub_copy_ops);
}