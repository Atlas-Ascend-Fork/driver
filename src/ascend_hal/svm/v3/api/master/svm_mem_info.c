/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>

#include "ascend_hal.h"
#include "svm_pub.h"
#include "svm_log.h"
#include "malloc_mng.h"
#include "svm_flag.h"
#include "svm_get_mem_size_info.h"
#include "svm_get_mem_token_info.h"
#include "va_mng.h"
#include "devdrv_hardware_version.h"
#include "svm_dbi.h"

static int get_mem_size_info(u32 devid, u32 type, struct MemInfo *info)
{
    struct MemPhyInfo *phy_info = &info->phy_info;

    return svm_get_mem_size_info(devid, type, phy_info);
}

static int get_mem_check_info_para_check(struct MemAddrInfo *info)
{
    if (info->addr == NULL) {
        svm_err("Info addr para is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((info->cnt == 0) || (info->cnt > SVM_ADDR_CHECK_MAX_NUM)) {
        svm_err("Cnt out of range. (cnt=%u; range=[1, %u])\n", info->cnt, (u32)SVM_ADDR_CHECK_MAX_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((info->mem_type & MEM_SVM_TYPE) || (info->mem_type & MEM_HOST_TYPE) || (info->mem_type & MEM_HOST_AGENT_TYPE)) {
        svm_info("Mem_type not support. (mem_type=%u)\n", info->mem_type);
        return DRV_ERROR_NOT_SUPPORT;
    }
    return DRV_ERROR_NONE;
}

static inline u32 svm_prop_to_mem_virt_mask(struct svm_prop *prop)
{
    if (svm_flag_cap_is_support_vmm_unmap(prop->flag)) {
        return MEM_RESERVE_TYPE;
    }

    return (MEM_DEV_TYPE | MEM_DVPP_TYPE);
}

int _get_mem_check_info(u32 devid, u32 type, struct MemAddrInfo *para)
{
    struct svm_prop prop;
    u32 i;
    int ret;
    SVM_UNUSED(type);

    for (i = 0; i < para->cnt; i++) {
        ret = svm_get_prop((u64)(uintptr_t)para->addr[i], &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Va is not alloced. (va=0x%llx; cnt=%u)\n", (u64)(uintptr_t)para->addr[i], para->cnt);
            para->flag = false;
            return DRV_ERROR_INVALID_VALUE;
        }

        if (svm_flag_cap_is_support_get_addr_check_info(prop.flag) == false) {
            para->flag = false;
            return DRV_ERROR_INVALID_VALUE;
        }

        if (prop.devid != devid) {
            svm_err("Svm check addr prop failed. (va=0x%llx; prop_devid=%u)\n", (u64)(uintptr_t)para->addr[i],
                    prop.devid);
            para->flag = false;
            return DRV_ERROR_INVALID_VALUE;
        }

        if ((para->mem_type & svm_prop_to_mem_virt_mask(&prop)) == 0) {
            svm_err("Mem type does not match. (va=0x%llx; cnt=%u; expected_mem_type=0x%x; real_mem_type=0x%x)\n",
                    (u64)(uintptr_t)para->addr[i], para->cnt, para->mem_type, svm_prop_to_mem_virt_mask(&prop));
            para->flag = false;
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    para->flag = true;
    return DRV_ERROR_NONE;
}

static int get_mem_check_info(u32 devid, u32 type, struct MemInfo *info)
{
    struct MemAddrInfo *mem_check_info = &info->addr_info;
    int ret;

    ret = get_mem_check_info_para_check(mem_check_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return _get_mem_check_info(devid, type, mem_check_info);
}

static int get_mem_token_info(u32 devid, u32 type, struct MemInfo *info)
{
    struct MemUbTokenInfo *ub_token_info = &info->ub_token_info;
    u32 pre_token_id = 0, pre_token_value = 0;
    u32 token_id, token_value;
    struct svm_prop prop;
    u64 va = ub_token_info->va;
    u64 size = ub_token_info->size;
    u64 tmp_va;
    int ret;
    SVM_UNUSED(type);

    if (size == 0) {
        svm_err("Size is zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((va + size) <= va) {
        svm_err("Address range is invalid. (va=0x%llx; size=0x%llx)\n", va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* For vmm segmented halMemMap scene, check each segment. */
    for (tmp_va = va; tmp_va < (va + size); tmp_va = (prop.start + prop.size)) {
        ret = svm_get_prop(tmp_va, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Va is not alloced. (ret=%d; va=0x%llx)\n", ret, tmp_va);
            return DRV_ERROR_INVALID_VALUE;
        }

        if (svm_flag_cap_is_support_get_mem_token_info(prop.flag) == false) {
            return DRV_ERROR_INVALID_VALUE;
        }

        if (devid != prop.devid) {
            svm_err("devid does not match. (devid=%u; prop.devid=%u)\n", devid, prop.devid);
            return DRV_ERROR_INVALID_VALUE;
        }

        ret = svm_get_mem_token_info(devid, prop.start, prop.size, &token_id, &token_value);
        if (ret != 0) {
            svm_err("svm_get_mem_token_info failed. (ret=%d; devid=%u; va=0x%llx; size=0x%llx)\n", ret, devid,
                    prop.start, prop.size);
            return ret;
        }

        if ((tmp_va != va) && ((token_id != pre_token_id) || (token_value != pre_token_value))) {
            svm_err("token info does not match in va range. (devid=%u; va=0x%llx; size=0x%llx)\n", devid, va, size);
            return DRV_ERROR_INVALID_VALUE;
        }
        pre_token_id = token_id;
        pre_token_value = token_value;
    }

    ub_token_info->token_id = token_id;
    ub_token_info->token_value = token_value;
    return 0;
}

static int (*g_svm_get_mem_info[MEM_INFO_TYPE_MAX])(u32 devid, u32 type, struct MemInfo *info) = {
    [MEM_INFO_TYPE_DDR_SIZE] = get_mem_size_info,
    [MEM_INFO_TYPE_HBM_SIZE] = get_mem_size_info,
    [MEM_INFO_TYPE_DDR_P2P_SIZE] = get_mem_size_info,
    [MEM_INFO_TYPE_HBM_P2P_SIZE] = get_mem_size_info,
    [MEM_INFO_TYPE_ADDR_CHECK] = get_mem_check_info,
    [MEM_INFO_TYPE_CTRL_NUMA_INFO] = NULL,
    [MEM_INFO_TYPE_AI_NUMA_INFO] = NULL,
    [MEM_INFO_TYPE_BAR_NUMA_INFO] = NULL,
    [MEM_INFO_TYPE_SVM_GRP_INFO] = NULL,
    [MEM_INFO_TYPE_UB_TOKEN_INFO] = get_mem_token_info};

static int get_mem_info_para_check(u32 devid, u32 type, struct MemInfo *info)
{
    if ((devid >= SVM_MAX_AGENT_NUM) || (info == NULL)) {
        svm_err("Invalid value. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (type >= MEM_INFO_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (g_svm_get_mem_info[type] == NULL) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

DVresult halMemGetInfo(DVdevice device, unsigned int type, struct MemInfo *info)
{
    int ret;

    ret = get_mem_info_para_check((u32)device, (u32)type, info);
    if (ret != 0) {
        return (DVresult)ret;
    }

    return (DVresult)g_svm_get_mem_info[(u32)type]((u32)device, (u32)type, info);
}

drvError_t halMemGetAddressRange(DVdeviceptr ptr, DVdeviceptr *pbase, size_t *psize)
{
    struct svm_prop prop;
    u64 va = ptr;
    int ret;

    if ((pbase == NULL) && (psize == NULL)) {
        svm_err("Invalid argument. (pbase=NULL; psize=NULL)\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        svm_err("Va is not allocated. (va=0x%llx)\n", va);
        return ret;
    }

    if (!svm_flag_cap_is_support_normal_free(prop.flag) && !svm_flag_cap_is_support_vmm_unmap(prop.flag) &&
        !svm_flag_cap_is_support_vmm_ipc_unmap(prop.flag) && !svm_flag_cap_is_support_ipc_close(prop.flag)) {
        svm_err("Va not support. (va=0x%llx)\n", va);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pbase != NULL) {
        *pbase = prop.start;
    }

    if (psize != NULL) {
        *psize = prop.size;
    }

    svm_debug("MemGetAddressRange. ( base=0x%llx; size=%llu)\n", (pbase != NULL) ? (u64)(*pbase) : 0ULL,
              (psize != NULL) ? (u64)(*psize) : 0ULL);
    return DRV_ERROR_NONE;
}

static drvError_t svm_check_pcie_th_support(uint32_t dev_id)
{
    int64_t hw_ver = 0;
    u32 chip_type, hd_connect_type;
    int ret;

    ret = halGetDeviceInfo(dev_id, MODULE_TYPE_SYSTEM, INFO_TYPE_VERSION, &hw_ver);
    if (ret != DRV_ERROR_NONE) {
        svm_err("halGetDeviceInfo failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    chip_type = PLAT_GET_CHIP((u64)hw_ver);
    if ((chip_type != CHIP_CLOUD_V4) && (chip_type != CHIP_CLOUD_V5)) {
        svm_debug("PCIE TH not support. (dev_id=%u; chip_type=%u)\n", dev_id, chip_type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    hd_connect_type = svm_get_device_connect_type(dev_id);
    if (hd_connect_type != HOST_DEVICE_CONNECT_TYPE_PCIE) {
        svm_debug("PCIE TH not support. (dev_id=%u; hd_connect_type=%u)\n", dev_id, hd_connect_type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

drvError_t halHostGetDevicePointerAddrCount(uint32_t dev_id, uint32_t *count)
{
    drvError_t ret;

    if (count == NULL) {
        svm_err("Count para is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_check_pcie_th_support(dev_id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    *count = PCIE_TH_VA_RANGE_COUNT;
    return DRV_ERROR_NONE;
}

drvError_t halHostGetDevicePointerAddrRange(uint32_t dev_id, struct drv_addr_range *addr_range, uint32_t *count)
{
    u64 va = 0;
    u64 size = 0;
    u32 i;
    drvError_t ret;

    if ((addr_range == NULL) || (count == NULL)) {
        svm_err("Invalid argument. (addr_range=%p; count=%p)\n", addr_range, (void *)count);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (*count < PCIE_TH_VA_RANGE_COUNT) {
        svm_err("Count is invalid. (count=%u)\n", *count);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_check_pcie_th_support(dev_id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_get_pcie_th_va_range(&va, &size);

    addr_range[0].start_addr = (void *)(uintptr_t)va;
    addr_range[0].end_addr = (void *)(uintptr_t)(va + size);

    for (i = PCIE_TH_VA_RANGE_COUNT; i < *count; i++) {
        addr_range[i].start_addr = NULL;
        addr_range[i].end_addr = NULL;
    }

    *count = PCIE_TH_VA_RANGE_COUNT;
    return DRV_ERROR_NONE;
}
