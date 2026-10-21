/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "pbl_feature_loader.h"
#include "pbl_soc_res.h"
#include "casm_kernel.h"
#include "svm_pub.h"
#include "svm_ub_mem.h"
#include "svm_addr_desc.h"
#include "svm_kern_log.h"
#include "svm_sub_event_type_uk_msg.h"
#include "va_reserve_uk_msg.h"
#include "linear_mem.h"

#define AIC_ATU_CFG_NAME "AIC_ATU_CFG_BASE"
#define LINEAR_MEM_MAX_SHARE_MEM_SIZE (48 * SVM_BYTES_PER_GB)

static u64 _linear_mem_get_access_va(u32 udevid, u32 mem_id, u64 offset)
{
    struct soc_reg_base_info io_base;
    int ret;

    ret = soc_resmng_dev_get_reg_base(udevid, AIC_ATU_CFG_NAME, &io_base);
    if (ret != 0) {
        return 0ULL;
    }

    return io_base.io_base + (mem_id * LINEAR_MEM_MAX_SHARE_MEM_SIZE) + offset;
}
static int linear_mem_set_access_va(u32 udevid, u64 va, u64 size)
{
    struct soc_reg_base_info io_base;
    int ret;

    /* not support repeat set */
    ret = soc_resmng_dev_get_reg_base(udevid, AIC_ATU_CFG_NAME, &io_base);
    if (ret == 0) {
        if ((io_base.io_base == va) && (io_base.io_base_size == size)) {
            return 0;
        }
        svm_warn("Repeat set different base. (udevid=%u)\n", udevid);
    }

    io_base.io_base = va;
    io_base.io_base_size = size;
    ret = soc_resmng_dev_set_reg_base(udevid, AIC_ATU_CFG_NAME, &io_base);
    if (ret != 0) {
        svm_err("Set linear_mem_base failed. (ret=%d; udevid=%u)\n", ret, udevid);
        return ret;
    }

    return 0;
}

static int um_va_reserve_host_post_handle(u32 udevid, int master_tgid, int slave_tgid, void *msg, u32 msg_len)
{
    struct svm_va_reserve_msg *reserve_msg = (struct svm_va_reserve_msg *)msg;
    int ret = 0;

    if (msg_len != sizeof(*reserve_msg)) {
        svm_err("Invalid para. (udevid=%u; master_tgid=%d; slave_tgid=%d; msg_len=%u)\n", udevid, master_tgid,
                slave_tgid, msg_len);
        return -EINVAL;
    }

    if (reserve_msg->status != VA_RESERVE_STATUS_OK) {
        return 0;
    }

    /* Use size for approximate identification of linear mem */
    if (((reserve_msg->flag & SVM_MMAP_FLAG_PRIVATE) != 0) && (reserve_msg->size > SVM_BYTES_PER_TB)) {
        ret = linear_mem_set_access_va(udevid, reserve_msg->va, reserve_msg->size);
    }

    return ret;
}

/* for ut test, cannot add static */
int linear_mem_get_access_va(u32 udevid, struct svm_global_va *src_va, struct casm_src_ex *src_ex, u64 *ex_info)
{
    u32 mem_id = 0;
    u64 offset;

    svm_ub_va_to_mem_id_and_offset(src_ex->updated_va, &mem_id, &offset);
    *ex_info = _linear_mem_get_access_va(udevid, mem_id, offset);
    return 0;
}

int linear_mem_register_ops(void)
{
    svm_um_register_handle(SVM_VA_RESERVE_EVENT, NULL, NULL, um_va_reserve_host_post_handle);
    svm_casm_register_get_src_va_ex_info_handle(linear_mem_get_access_va);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(linear_mem_register_ops, FEATURE_LOADER_STAGE_2);