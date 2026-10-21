/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/ioctl.h>
#include <stdbool.h>
#include <stdio.h>

#include "ascend_hal_error.h"
#include "svm_sys_cmd.h"
#include "svm_ioctl_ex.h"
#include "svm_pub.h"
#include "svm_log.h"
#include "casm_ioctl.h"
#include "casm.h"

int svm_dma_map(u32 user_devid, struct svm_dst_va *dst_va, u32 flag)
{
    struct svm_dma_map_para para;
    int ret;

    para.dst_va = *dst_va;
    para.flag = flag;

    ret = svm_cmd_ioctl(user_devid, SVM_DMA_MAP, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err_if((ret != DRV_ERROR_BUSY),
                   "Map failed. (user_devid=%u; task_type=%u; va=0x%llx; size=0x%llx; ret=%d)\n", user_devid,
                   dst_va->task_type, dst_va->va, dst_va->size, ret);
        return ret;
    }

    return 0;
}

int svm_dma_unmap(u32 user_devid, struct svm_dst_va *dst_va)
{
    struct svm_dma_unmap_para para;
    int ret;

    para.dst_va = *dst_va;

    ret = svm_cmd_ioctl(user_devid, SVM_DMA_UNMAP, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err_if((ret != DRV_ERROR_BUSY) && (ret != DRV_ERROR_PARA_ERROR),
                   "Unmap failed. (user_devid=%u; task_type=%u; va=0x%llx; size=0x%llx; ret=%d)\n", user_devid,
                   dst_va->task_type, dst_va->va, dst_va->size, ret);
        return ret;
    }

    return 0;
}
