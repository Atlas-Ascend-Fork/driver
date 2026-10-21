/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ascend_hal.h"

#include "trs_user_interface.h"
#include "trs_user_pub_def.h"
#include "trs_sqcq.h"
#include "trs_dev_drv.h"
#include "trs_sq_mem_raw.h"

static bool trs_sq_send_mode_is_high_performance(uint32_t devid)
{
    return (trs_get_sq_send_mode(devid) == TRS_MODE_TYPE_SQ_SEND_HIGH_PERFORMANCE);
}

int trs_sq_mem_raw_alloc(uint32_t devid, uint64_t *va, uint64_t size)
{
    uint64_t svm_va, flag;
    int ret;

    ret = halMemAgentOpen(devid, SVM_AGENT_DEVICE);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Svm open device failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    flag = MEM_DEV | MEM_ADVISE_P2P | devid | ((uint64_t)TSDRV_MODULE_ID << MEM_MODULE_ID_BIT);
    flag |= trs_sq_send_mode_is_high_performance(devid) ? 0 : MEM_DEV_CP_ONLY;

    ret = halMemAlloc((void **)&svm_va, size, flag | MEM_CONTIGUOUS_PHY); /* Max support 4M contiguous. */
    if (ret != DRV_ERROR_NONE) {
        trs_warn_limit("Alloc contiguous memory failed. (ret=%d; devid=%u; size=%llu; flag=0x%llx)\n", ret, devid, size,
                       flag);
        ret = halMemAlloc((void **)&svm_va, size, flag);
        if (ret != DRV_ERROR_NONE) {
            trs_err("Alloc svm mem failed. (ret=%d; devid=%u; size=%llu; flag=0x%llx)\n", ret, devid, size, flag);
            return ret;
        }
    }

    if (trs_sq_send_mode_is_high_performance(devid)) { /* Only uio mode need host register va. */
        uint64_t dst_va;

        ret = halHostRegister((void *)svm_va, size, DEV_SVM_MAP_HOST, devid, (void **)&dst_va);
        if (ret != DRV_ERROR_NONE) {
            trs_err("Register svm mem failed. (ret=%d; devid=%u; size=%llu)\n", ret, devid, size);
            (void)halMemFree((void *)svm_va);
            return ret;
        }

        if (dst_va != svm_va) {
            trs_err("Svm register dst_va not equal to src_va. (devid=%u; size=%llu)\n", devid, size);
            (void)halHostUnregisterEx((void *)dst_va, devid, DEV_SVM_MAP_HOST);
            (void)halMemFree((void *)svm_va);
            return DRV_ERROR_INNER_ERR;
        }
    }

    *va = svm_va;
    return DRV_ERROR_NONE;
}

void trs_sq_mem_raw_free(uint32_t devid, uint64_t va)
{
    if (trs_sq_send_mode_is_high_performance(devid)) {
        (void)halHostUnregisterEx((void *)va, devid, DEV_SVM_MAP_HOST);
    }

    (void)halMemFree((void *)va);
}
