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
#include "svm_addr_desc.h"
#include "smm_ioctl.h"
#include "smm.h"

int svm_smm_mmap(u32 devid, u64 *va, u64 size, u32 flag, struct svm_global_va *src_info)
{
    struct svm_smm_map_para para;
    int ret;

    if (src_info == NULL) {
        return DRV_ERROR_PARA_ERROR;
    }

    para.dst_va = *va;
    para.dst_size = size;
    para.flag = flag;
    para.src_info = *src_info;
    para.src_va = src_info->va;

    ret = svm_cmd_ioctl(devid, SVM_SMM_MAP, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        ret = (ret == DRV_ERROR_OPER_NOT_PERMITTED) ? DRV_ERROR_NO_PROCESS : ret;
        svm_err_if((ret != DRV_ERROR_NO_PROCESS),
                   "Svm smm mmap ioctl failed. (ret=%d; va=0x%llx; flag=%u; size=%llu)\n", ret, *va, flag,
                   para.dst_size);
        return ret;
    }

    *va = para.dst_va;

    return 0;
}

int svm_smm_munmap(u32 devid, u64 va, u64 size, u32 flag, struct svm_global_va *src_info)
{
    struct svm_smm_unmap_para para;
    int ret;

    if (src_info == NULL) {
        return DRV_ERROR_PARA_ERROR;
    }

    para.dst_va = va;
    para.dst_size = size;
    para.flag = flag;
    para.src_info = *src_info;

    ret = svm_cmd_ioctl(devid, SVM_SMM_UNMAP, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        ret = (ret == DRV_ERROR_OPER_NOT_PERMITTED) ? DRV_ERROR_NO_PROCESS : ret;
        svm_err_if((ret != DRV_ERROR_NO_PROCESS), "Svm smm munmap ioctl failed. (ret=%d; va=0x%llx; size=%llu)\n", ret,
                   va, para.dst_size);
    }

    return ret;
}
