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

#include "ascend_hal.h"
#include "ascend_hal_define.h"
#include "ascend_hal_error.h"

#include "svm_pub.h"
#include "svm_dbi.h"
#include "svm_log.h"
#include "svm_addr_desc.h"
#include "svm_mem_repair.h"
#include "svm_dbi.h"
#include "svm_pagefault_info.h"

int mem_ctrl_mem_repair(void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret)
{
    if ((param_value == NULL) || (param_value_size != sizeof(struct MemRepairInPara))) {
        svm_err("Param_value is invalid. (param_value=0x%llx; param_value_size=%llu)\n", (u64)(uintptr_t)param_value,
                param_value_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((out_value != NULL) || (out_size_ret != NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return svm_mem_repair(param_value);
}

#if !defined(CFG_SOC_PLATFORM_CLOUD_V5_ESL) && !defined(CFG_SOC_PLATFORM_CLOUD_V4_ESL)
static bool mem_ctrl_is_host_access_dev_mem_support(u32 devid) /* todo: p2p support query. */
{
    if (svm_dbi_is_cc_vm()) {
        return false;
    }

    if (svm_dbi_is_support_host_access_dev(devid)) {
        u64 page_size, host_page_size;
        int ret = svm_dbi_query_npage_size(devid, &page_size);
        if (ret != 0) {
            return false;
        }

        ret = svm_dbi_query_npage_size(svm_get_host_devid(), &host_page_size);
        if (ret != 0) {
            return false;
        }

        return (host_page_size == page_size);
    }

    return false;
}

static bool mem_ctrl_is_host_access_dev_huge_mem_support(u32 devid)
{
    return !svm_dbi_is_cc_vm() && svm_dbi_is_support_host_access_dev(devid);
}

static bool mem_ctrl_is_giant_page_support(u32 devid)
{
    SVM_UNUSED(devid);

    return true;
}
#endif

#define SUPPORT_FEATURE_MAX_NUM 6
int mem_ctrl_support_feature(void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret)
{
#if !defined(CFG_SOC_PLATFORM_CLOUD_V5_ESL) && !defined(CFG_SOC_PLATFORM_CLOUD_V4_ESL)
    static bool (*const mem_ctl_feature_is_support[SUPPORT_FEATURE_MAX_NUM])(u32 devid) = {
        [CTRL_SUPPORT_PCIE_BAR_MEM_BIT] = mem_ctrl_is_host_access_dev_mem_support,
        [CTRL_SUPPORT_PCIE_BAR_HUGE_MEM_BIT] = mem_ctrl_is_host_access_dev_huge_mem_support,
        [CTRL_SUPPORT_GIANT_PAGE_BIT] = mem_ctrl_is_giant_page_support,
    };
    struct supportFeaturePara *in_para = (struct supportFeaturePara *)param_value;
    struct supportFeaturePara *out_para = (struct supportFeaturePara *)out_value;
    u32 i;
    SVM_UNUSED(param_value_size);
    SVM_UNUSED(out_size_ret);

    if ((in_para == NULL) || (out_para == NULL)) {
        svm_err("Para is invalid. (in_para_is_null=%d)\n", (in_para == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    out_para->support_feature = 0;
    for (i = CTRL_SUPPORT_NUMA_TS_BIT; i < SUPPORT_FEATURE_MAX_NUM; i++) {
        uint64_t feature_mask = (1ULL << i);
        if ((in_para->support_feature & feature_mask) != 0) {
            if ((mem_ctl_feature_is_support[i] != NULL) && mem_ctl_feature_is_support[i](in_para->devid)) {
                out_para->support_feature |= feature_mask;
            }
        }
    }

    svm_info("RTS note drv support feature. (in_feature=0x%llx; out_feature=0x%llx)\n", in_para->support_feature,
             out_para->support_feature);
    return DRV_ERROR_NONE;
#else
    struct supportFeaturePara *out_para = (struct supportFeaturePara *)out_value;
    SVM_UNUSED(param_value);
    SVM_UNUSED(param_value_size);
    SVM_UNUSED(out_size_ret);
    out_para->support_feature = 0;
    return DRV_ERROR_NONE;
#endif
}

static int mem_ctrl_get_addr_module_id(void *param_value, size_t param_value_size, void *out_value,
                                       size_t *out_size_ret)
{
    u32 *module_id = (u32 *)out_value;
    u64 *va = (u64 *)param_value;
    struct svm_prop prop;
    u32 tmp_module_id;
    int ret;

    if ((param_value == NULL) || (out_value == NULL) || (out_size_ret == NULL)) {
        svm_err("Input is invalid. (param_value=%u; out_value=%u; out_size_ret=%u)\n", (param_value != NULL),
                (out_value != NULL), (out_size_ret != NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (param_value_size != sizeof(u64)) {
        svm_err("Param_value_size is invalid. (param_value_size=%lu; size=%lu)\n", param_value_size, sizeof(u64));
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_get_prop(*va, &prop);
    tmp_module_id = (svm_flag_get_module_id(prop.flag) == SVM_FLAG_INVALID_MODULE_ID) ?
                        SVM_INVALID_MODULE_ID :
                        svm_flag_get_module_id(prop.flag);
    *module_id = ((ret == DRV_ERROR_NONE) ? tmp_module_id : SVM_INVALID_MODULE_ID);
    *out_size_ret = sizeof(u32);
    return DRV_ERROR_NONE;
}

static int (*svm_mem_ctrl_handlers[CTRL_TYPE_MAX])(void *param, size_t param_size, void *out_param,
                                                   size_t *out_size) = {
    [CTRL_TYPE_SUPPORT_FEATURE] = mem_ctrl_support_feature,
    [CTRL_TYPE_MEM_REPAIR] = mem_ctrl_mem_repair,
    [CTRL_TYPE_GET_ADDR_MODULE_ID] = mem_ctrl_get_addr_module_id,
    [CTRL_TYPE_PAGEFAULT_INFO_QUERY] = mem_ctrl_pagefault_info_query,
    [CTRL_TYPE_PAGEFAULT_INFO_CLEAR] = mem_ctrl_pagefault_info_clear,
};

drvError_t halMemCtl(int type, void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret)
{
    int ret;

    if ((type < 0) || (type >= (int)CTRL_TYPE_MAX) || (svm_mem_ctrl_handlers[type] == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = svm_mem_ctrl_handlers[type](param_value, param_value_size, out_value, out_size_ret);
    if (ret != DRV_ERROR_NONE) {
        return (drvError_t)ret;
    }

    return DRV_ERROR_NONE;
}
