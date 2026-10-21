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
#include <stdio.h>

#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "svm_sys_cmd.h"
#include "svm_ioctl_ex.h"
#include "svm_pub.h"
#include "svm_log.h"
#include "svm_dbi.h"
#include "casm_ioctl.h"

int svm_casm_cs_query_src_info(u64 key, struct svm_global_va *src_va, int *owner_pid)
{
    struct svm_casm_cs_query_src_para para;
    int ret;

    para.key = key;

    ret = svm_cmd_ioctl(svm_get_host_devid(), SVM_CASM_CS_QUERY_SRC, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    *src_va = para.src_va;
    *owner_pid = para.owner_pid;

    return 0;
}

int svm_casm_cs_set_src_info(u32 devid, u64 key, const struct svm_global_va *src_va, int owner_pid,
                             const struct casm_src_prop *prop)
{
    struct svm_casm_cs_set_src_para para = {0};
    int ret;

    para.key = key;
    para.src_va = *src_va;
    para.owner_pid = owner_pid;
    if (prop != NULL) {
        para.src_prop = *prop;
    }

    ret = svm_cmd_ioctl(devid, SVM_CASM_CS_SET_SRC, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Set src failed. (devid=%u; key=0x%llx; ret=%d)\n", devid, key, ret);
        return ret;
    }

    return 0;
}

int svm_casm_cs_clr_src_info(u32 devid, u64 key)
{
    struct svm_casm_cs_clr_src_para para;
    int ret;

    para.key = key;

    ret = svm_cmd_ioctl(devid, SVM_CASM_CS_CLR_SRC, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Clr src failed. (devid=%u; key=0x%llx; ret=%d)\n", devid, key, ret);
        return ret;
    }

    return 0;
}
