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

#include "svm_ioctl_ex.h"
#include "svm_pub.h"
#include "svm_log.h"
#include "svm_sys_cmd.h"
#include "mpl_ioctl.h"
#include "mpl.h"

int svm_mpl_populate(u32 devid, u64 va, u64 size, u32 flag)
{
    struct svm_mpl_populate_para para = {0};
    int ret;

    para.flag = flag;
    para.va = va;
    para.size = size;

    ret = svm_cmd_ioctl(devid, SVM_MPL_POPULATE, (void *)&para);
    if (ret != 0) {
        svm_err_if((ret != DRV_ERROR_OUT_OF_MEMORY),
                   "Svm ioctl mem populate failed. (ret=%d; va=0x%llx; size=%llu; flag=%u)\n", ret, va, size, flag);
    }

    return ret;
}

int svm_mpl_depopulate(u32 devid, u64 va, u64 size, u32 flag)
{
    struct svm_mpl_depopulate_para para = {0};
    int ret;

    para.va = va;
    para.size = size;
    para.flag = flag;

    ret = svm_cmd_ioctl(devid, SVM_MPL_DEPOPULATE, (void *)&para);
    if ((ret != 0) && (ret != DRV_ERROR_BUSY)) {
        svm_err("Svm ioctl mem depopulate failed. (ret=%d; va=0x%llx; size=%llu; flag=%u)\n", ret, va, size, flag);
    }

    return ret;
}
