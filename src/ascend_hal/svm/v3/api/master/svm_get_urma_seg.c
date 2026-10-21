/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ascend_hal.h"
#include "securec.h"

#include "svm_pub.h"
#include "svm_log.h"
#include "svm_addr_desc.h"
#include "svm_dbi.h"
#include "va_allocator.h"
#include "malloc_mng.h"
#ifdef CFG_FEATURE_SUPPORT_UB
#include "svm_urma_seg_mng.h"

#define SVM_ADDR_RANGE_MAX_COPY_SIZE 0x10000000ULL /* 256MB */

static int svm_get_non_svm_seg(u32 devid, u64 va, u64 size, urma_seg_t *seg, urma_token_t *token)
{
    struct svm_dst_va dst_va;
    urma_seg_t tmp_seg;
    u32 token_id, token_val;
    int ret;

    svm_dst_va_pack(svm_get_host_devid(), 0, va, size, &dst_va);
    ret = svm_urma_get_seg_with_token_info(devid, &dst_va, &tmp_seg, &token_id, &token_val);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get svm_urma_seg failed. (host pinned; devid=%u; va=0x%llx; size=%llu)\n", devid, va, size);
        return ret;
    }
    *seg = tmp_seg;
    token->token = token_val;
    return DRV_ERROR_NONE;
}

static int svm_get_svm_seg(u32 devid, u64 va, u64 size, urma_seg_t *seg, urma_token_t *token)
{
    struct svm_dst_va dst_va;
    struct svm_prop prop, pre_prop;
    urma_seg_t tmp_seg, first_seg;
    u32 token_id, token_val, pre_token_id, pre_token_val;
    u32 user_devid;
    u64 tmp_va;
    int ret;

    for (tmp_va = va; tmp_va < (va + size); tmp_va = (prop.start + prop.size)) {
        ret = svm_get_prop(tmp_va, &prop);
        if (ret != 0) {
            svm_err("Get prop failed. (va=0x%llx)\n", tmp_va);
            return ret;
        }

        if (prop.devid == SVM_INVALID_DEVID) {
            svm_err("Invalid va. (va=0x%llx)\n", tmp_va);
            return DRV_ERROR_INVALID_VALUE;
        }

        user_devid = (prop.devid == svm_get_host_devid()) ? devid : svm_get_host_devid();
        svm_dst_va_pack(prop.devid, PROCESS_CP1, prop.start, prop.size, &dst_va);
        ret = svm_urma_get_seg_with_token_info(user_devid, &dst_va, &tmp_seg, &token_id, &token_val);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Get svm_urma_seg failed. (user_devid=%u; devid=%u; va=0x%llx; size=%llu)\n", user_devid,
                    dst_va.devid, dst_va.va, dst_va.size);
            return ret;
        }

        /* Only check token_id and token_val consistency; segment struct may differ for sub-ranges. */
        if ((tmp_va != va) &&
            ((pre_token_id != token_id) || (pre_token_val != token_val) || (pre_prop.devid != prop.devid))) {
            svm_err("Token does not match. (va=0x%llx; tmp_va=0x%llx; size=0x%llx)\n", va, tmp_va, size);
            return DRV_ERROR_INVALID_VALUE;
        }

        if (tmp_va == va) {
            first_seg = tmp_seg;
        }
        pre_token_id = token_id;
        pre_token_val = token_val;
        pre_prop = prop;
    }

    *seg = first_seg;
    token->token = token_val;
    return DRV_ERROR_NONE;
}

/* to-do: tsdrv call, delete later, let tsdrv call get token info */
int halMemGetSeg(uint32_t devid, uint64_t va, uint64_t size, urma_seg_t *seg, urma_token_t *token)
{
    int ret;

    if ((devid >= SVM_MAX_AGENT_NUM) || (seg == NULL) || (token == NULL)) {
        svm_err("Invalid para. (devid=%u; seg_is_null=%d; token_is_null=%d)\n", devid, (seg == NULL), (token == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_va_is_in_range(va, size) == false) {
        ret = svm_get_non_svm_seg(devid, va, size, seg, token);
    } else {
        ret = svm_get_svm_seg(devid, va, size, seg, token);
    }

    return ret;
}

/* Split a range into segments no larger than max_copy_size, appended to out array. */
static int svm_split_range_by_max(u64 start, u64 size, u32 max_cnt, u32 *cur_cnt, struct addr_range *out)
{
    u64 remain = size;
    u64 offset = 0;

    while (remain > 0) {
        u64 chunk = (remain > SVM_ADDR_RANGE_MAX_COPY_SIZE) ? SVM_ADDR_RANGE_MAX_COPY_SIZE : remain;
        if (*cur_cnt >= max_cnt) {
            svm_err("Addr range out count exceeds max. (cnt=%u; max=%u)\n", *cur_cnt, max_cnt);
            return DRV_ERROR_NO_RESOURCES;
        }
        out[*cur_cnt].addr = start + offset;
        out[*cur_cnt].size = chunk;
        (*cur_cnt)++;
        offset += chunk;
        remain -= chunk;
    }

    return DRV_ERROR_NONE;
}

/* Split an SVM VA range into segments by token_id boundary, then by 256MB max.
 * Segments with continuous VA and same token_id are merged before splitting. */

struct svm_seg_token_info {
    u64 next_va;
    u64 seg_start;
    u64 seg_end;
    u32 token_id;
    u32 token_val;
};

static int svm_get_seg_token_info(u32 dev_id, u64 va, u64 range_end, struct svm_seg_token_info *info)
{
    struct svm_prop prop;
    struct svm_dst_va dst_va;
    urma_seg_t tmp_seg;
    u32 user_devid;
    int ret;

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (va=0x%llx)\n", va);
        return ret;
    }

    if (prop.devid == SVM_INVALID_DEVID) {
        svm_err("Invalid va. (va=0x%llx)\n", va);
        return DRV_ERROR_INVALID_VALUE;
    }

    user_devid = (prop.devid == svm_get_host_devid()) ? dev_id : svm_get_host_devid();
    svm_dst_va_pack(prop.devid, PROCESS_CP1, prop.start, prop.size, &dst_va);
    ret = svm_urma_get_seg_with_token_info(user_devid, &dst_va, &tmp_seg, &info->token_id, &info->token_val);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get svm_urma_seg failed. (user_devid=%u; devid=%u; va=0x%llx; size=%llu)\n", user_devid, dst_va.devid,
                dst_va.va, dst_va.size);
        return ret;
    }

    /* Clip prop range to the requested [va, range_end) range. */
    info->seg_start = (prop.start > va) ? prop.start : va;
    info->seg_end = (prop.start + prop.size < range_end) ? (prop.start + prop.size) : range_end;
    info->next_va = prop.start + prop.size;

    return DRV_ERROR_NONE;
}

static int svm_get_svm_addr_seg(u32 dev_id, u64 va, u64 size, struct addr_range *out, u32 max_cnt, u32 *out_cnt)
{
    struct svm_seg_token_info info;
    u32 pre_token_id = 0, pre_token_val = 0;
    u64 tmp_va, merged_start = 0, merged_size = 0;
    u64 range_end = va + size;
    int ret;

    for (tmp_va = va; tmp_va < range_end; tmp_va = info.next_va) {
        ret = svm_get_seg_token_info(dev_id, tmp_va, range_end, &info);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }

        if (merged_size == 0) {
            pre_token_id = info.token_id;
            pre_token_val = info.token_val;
            merged_start = info.seg_start;
            merged_size = info.seg_end - info.seg_start;
        } else if ((info.token_id == pre_token_id) && (info.token_val == pre_token_val) &&
                   (merged_start + merged_size == info.seg_start)) {
            /* Continuous VA with same token: merge. */
            merged_size += info.seg_end - info.seg_start;
        } else {
            /* Token changed or VA discontinuous: flush merged range. */
            ret = svm_split_range_by_max(merged_start, merged_size, max_cnt, out_cnt, out);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
            pre_token_id = info.token_id;
            pre_token_val = info.token_val;
            merged_start = info.seg_start;
            merged_size = info.seg_end - info.seg_start;
        }
    }

    /* Flush last merged range. */
    return svm_split_range_by_max(merged_start, merged_size, max_cnt, out_cnt, out);
}

static int svm_mem_get_addr_seg_check(uint32_t dev_id, const struct addr_range *addr_out, const uint32_t *cnt,
                                      uint64_t flag)
{
    u32 hd_connect_type;

    if ((dev_id >= SVM_MAX_AGENT_NUM) || (addr_out == NULL) || (cnt == NULL)) {
        svm_err("Invalid para. (dev_id=%u; out_is_null=%d; cnt_is_null=%d)\n", dev_id, (addr_out == NULL),
                (cnt == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (*cnt == 0) {
        svm_err("Invalid para. (cnt=%u)\n", *cnt);
        return DRV_ERROR_INVALID_VALUE;
    }

    hd_connect_type = svm_get_device_connect_type(dev_id);
    if (hd_connect_type != HOST_DEVICE_CONNECT_TYPE_UB) {
        svm_debug("Not support for non-UB connection type.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (flag != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

drvError_t halMemGetAddrSeg(uint32_t dev_id, struct addr_range addr_in, struct addr_range *addr_out, uint32_t *cnt,
                            uint64_t flag)
{
    u64 va = addr_in.addr;
    u64 size = addr_in.size;
    u32 out_cnt = 0;
    int ret;

    ret = svm_mem_get_addr_seg_check(dev_id, addr_out, cnt, flag);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    /* Non-SVM VA: simply split by 256MB. */
    if (svm_va_is_in_range(va, size) == false) {
        ret = svm_split_range_by_max(va, size, *cnt, &out_cnt, addr_out);
    } else {
        ret = svm_get_svm_addr_seg(dev_id, va, size, addr_out, *cnt, &out_cnt);
    }
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    *cnt = out_cnt;
    return DRV_ERROR_NONE;
}
#endif
