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
#include "svm_dbi.h"
#include "malloc_mng.h"
#include "svm_register_only.h"
#include "svm_master_init.h"
#include "va_allocator.h"

static int svm_addr_prop_check(struct svm_prop *prop, u64 va)
{
    if (svm_flag_cap_is_support_get_attr(prop->flag) == false) {
        svm_err("Addr cap does not support get attr. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    if (prop->devid >= SVM_INVALID_DEVID) {
        svm_err("Svm check addr prop failed. (va=0x%llx; prop_devid=%u)\n", va, prop->devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static void svm_addr_attr_pack(u32 devid, u32 memtype, u32 page_size, struct DVattribute *attr)
{
    attr->devId = devid;
    attr->memType = memtype;
    attr->pageSize = page_size;
}

static bool svm_addr_is_in_managed_range(u64 va)
{
    u64 start, size;

    svm_get_va_range(&start, &size);
    return (svm_check_va_range(va, 1ULL, start, start + size) == 0) || va_is_in_pcie_th_range(va, 1ULL);
}

static int svm_addr_attr_get(u64 va, struct DVattribute *attr)
{
    struct svm_prop prop;
    u64 page_size;
    int ret;

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        return DRV_ERROR_NOT_EXIST;
    }

    ret = svm_addr_prop_check(&prop, va);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (svm_flag_attr_is_npage(prop.flag)) {
        ret = svm_dbi_query_npage_size(prop.devid, &page_size);
    } else {
        /* gpage return hpage_size */
        ret = svm_dbi_query_hpage_size(prop.devid, &page_size);
    }
    if (ret != 0) {
        return ret;
    }

    if (svm_get_host_devid() == prop.devid) {
        svm_addr_attr_pack(0, DV_MEM_LOCK_HOST, (u32)page_size, attr);
    } else {
        u32 mem_type = svm_flag_cap_is_support_ipc_close(prop.flag) ? DV_MEM_SVM_DEVICE : DV_MEM_LOCK_DEV;
        svm_addr_attr_pack(prop.devid, mem_type, (u32)page_size, attr);
    }

    return DRV_ERROR_NONE;
}

static drvError_t svm_get_dev_info(u32 *dev_num, u32 *ids)
{
    int ret;

    ret = drvGetDevNum(dev_num);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Drv get dev num error. (ret=%d)\n", ret);
        return ret;
    }

    ret = drvGetDevIDs(ids, SVM_MAX_DEV_NUM);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Drv get dev ids error. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static bool addr_is_registered(u64 va)
{
    u32 ids[SVM_MAX_DEV_NUM] = {0};
    u32 dev_num, i;
    int ret;

    ret = svm_get_dev_info(&dev_num, ids);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get dev info failed. (ret=%d)\n", ret);
        return false;
    }

    for (i = 0; (i < dev_num) && (i < SVM_MAX_DEV_NUM); i++) {
        if (svm_is_dev_open(ids[i]) == false) {
            continue;
        }

        if (svm_va_is_registered(va, 1, ids[i])) {
            return true;
        }
    }

    return false;
}

DVresult drvMemGetAttribute(DVdeviceptr vptr, struct DVattribute *attr)
{
    u32 memtype;
    int ret;

    if ((vptr == 0) || (attr == NULL)) {
        svm_err("Get attr input para failed. (vptr=0x%llx; attr_is_null=%u)\n", vptr, (attr == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    /*
     * First use the fixed SVM and PCIeTH VA ranges as a coarse filter. If malloc_mng has no property for an address
     * in these ranges, query va_reserve to distinguish a reserved SVM VA from a user address.
     */
    if (svm_addr_is_in_managed_range(vptr)) {
        ret = svm_addr_attr_get(vptr, attr);
        if (ret != DRV_ERROR_NOT_EXIST) {
            return (DVresult)ret;
        }

        if (svm_va_is_in_range(vptr, 1ULL)) {
            return DRV_ERROR_NOT_EXIST;
        }
    }

    /* todo queue need adapt. */
    memtype = addr_is_registered(vptr) ? DV_MEM_USER_REGISTER : DV_MEM_USER_MALLOC;
    svm_addr_attr_pack(0, memtype, (u32)getpagesize(), attr);
    return DRV_ERROR_NONE;
}

bool svm_support_get_user_malloc_attr(uint32_t dev_id)
{
    SVM_UNUSED(dev_id);

    return true;
}

bool svm_support_vmm_normal_granularity(uint32_t dev_id)
{
    return (dev_id < SVM_MAX_DEV_AGENT_NUM);
}

bool svm_support_mem_register_query_and_get_attr(uint32_t dev_id)
{
    return (dev_id < SVM_MAX_DEV_AGENT_NUM);
}

bool svm_support_mem_register_host_pinned(uint32_t dev_id)
{
    return (dev_id < SVM_MAX_DEV_AGENT_NUM);
}

bool svm_support_mem_host_uva(uint32_t dev_id)
{
    return (dev_id < SVM_MAX_DEV_AGENT_NUM) && ((svm_get_device_connect_type(dev_id) == HOST_DEVICE_CONNECT_TYPE_UB) ||
                                                (svm_is_support_pcie_th() && svm_is_pcie_th_allocator_enable()));
}
