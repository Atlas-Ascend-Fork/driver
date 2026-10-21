/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <unistd.h>
#include <stdbool.h>

#include "svm_pub.h"
#include "svm_dbi.h"
#include "svm_addr_desc.h"
#include "svm_urma_seg_mng.h"
#include "svm_register_to_master.h"

static u32 svm_register_to_master_flag_to_seg_flag(u32 user_devid, u32 devid, u32 flag)
{
    u32 seg_flag = 0;

    seg_flag |= register_to_master_flag_is_access_write(flag) ? SVM_URMA_SEG_FLAG_ACCESS_WRITE : 0;
    seg_flag |= register_to_master_flag_is_pin(flag) ? SVM_URMA_SEG_FLAG_PIN : 0;
    seg_flag |= (user_devid == devid) ? SVM_URMA_SEG_FLAG_SELF_USER : 0;

    return seg_flag;
}

static int svm_urma_register_seg_try(u32 user_devid, struct svm_dst_va *register_va, u32 seg_flag)
{
    int ret = svm_urma_register_seg(user_devid, register_va, seg_flag);
    return (ret == DRV_ERROR_NOT_SUPPORT) ? DRV_ERROR_NONE : ret;
}

int svm_ub_register_to_master(u32 user_devid, struct svm_dst_va *register_va, u32 flag)
{
    u32 seg_flag = svm_register_to_master_flag_to_seg_flag(user_devid, register_va->devid, flag);
    return svm_urma_register_seg_try(user_devid, register_va, seg_flag);
}

int svm_ub_unregister_to_master(u32 user_devid, struct svm_dst_va *register_va, u32 flag)
{
    u32 seg_flag = svm_register_to_master_flag_to_seg_flag(user_devid, register_va->devid, flag);
    return svm_urma_unregister_seg(user_devid, register_va, seg_flag);
}

static struct svm_register_to_master_ops g_ub_register_to_mster_ops = {
    .register_to_master = svm_ub_register_to_master, .unregister_to_master = svm_ub_unregister_to_master};

void svm_ub_register_to_master_ops_register(u32 user_devid, u32 devid)
{
    svm_register_to_master_set_ops(user_devid, devid, &g_ub_register_to_mster_ops);
}

void svm_ub_register_to_master_ops_unregister(u32 user_devid, u32 devid)
{
    svm_register_to_master_set_ops(user_devid, devid, NULL);
}
