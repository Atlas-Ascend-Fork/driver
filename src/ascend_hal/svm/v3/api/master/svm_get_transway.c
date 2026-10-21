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

#include "svm_pub.h"
#include "svm_log.h"
#include "malloc_mng.h"
#include "svm_dbi.h"

static int svm_get_transway(u32 src_devid, u32 dst_devid, u8 *trans_type)
{
    u32 hd_connect_type;

    if (src_devid == dst_devid) {
        *trans_type = MEM_TRANS_TYPE_SDMA;
        return 0;
    }

    hd_connect_type = svm_get_device_connect_type(src_devid);
    if (hd_connect_type == HOST_DEVICE_CONNECT_TYPE_UB) {
        *trans_type = MEM_TRANS_TYPE_UB_DMA;
    } else if (hd_connect_type == HOST_DEVICE_CONNECT_TYPE_PCIE) {
        *trans_type = MEM_TRANS_TYPE_PCIE_DMA;
    } else {
        svm_info("Connect type is not supported. (devid=%u; hd_connect_type=%u)\n", src_devid, hd_connect_type);
        return DRV_ERROR_NOT_SUPPORT;
    }
    return 0;
}

static int svm_get_transway_cap_check(struct svm_prop *src_prop, struct svm_prop *dst_prop)
{
    u32 host_devid = svm_get_host_devid();

    if ((src_prop->devid == host_devid) || (dst_prop->devid == host_devid)) {
        svm_info("Src or dst is host va. (src_devid=%u; dst_devid=%u)\n", src_prop->devid, dst_prop->devid);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (svm_flag_cap_is_support_get_d2d_transway(src_prop->flag) == false) {
        svm_info("Src va does not support get transway.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (svm_flag_cap_is_support_get_d2d_transway(dst_prop->flag) == false) {
        svm_info("Dst va does not support get transway.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

drvError_t drvDeviceGetTransWay(void *src, void *dest, uint8_t *trans_type)
{
    struct svm_prop src_prop, dst_prop;
    int ret;

    if ((src == NULL) || (dest == NULL) || (trans_type == NULL)) {
        svm_err("Src or dest or trans_type is NULL. (src_is_null=%d; dest_is_null=%d; trans_type_is_null=%d;)\n",
                (src == NULL), (dest == NULL), (trans_type == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_get_prop((u64)(uintptr_t)src, &src_prop);
    if (ret != 0) {
        svm_err("Src is invalid. (src_va=0x%llx)\n", (u64)(uintptr_t)src);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_get_prop((u64)(uintptr_t)dest, &dst_prop);
    if (ret != 0) {
        svm_err("Dst is invalid. (dst_va=0x%llx)\n", (u64)(uintptr_t)dest);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_get_transway_cap_check(&src_prop, &dst_prop);
    if (ret != 0) {
        return ret;
    }

    return svm_get_transway(src_prop.devid, dst_prop.devid, trans_type);
}
