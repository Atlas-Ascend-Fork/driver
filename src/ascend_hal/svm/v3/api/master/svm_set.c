/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <securec.h>

#include "ascend_hal.h"

#include "svm_user_adapt.h"
#include "svm_log.h"
#include "svm_flag.h"
#include "malloc_mng.h"
#include "svm_dbi.h"
#include "va_allocator.h"
#include "svm_memset_client.h"
#include "svm_memset.h"

/* todo: same with memcpy, later adjust memcpy */
static int svm_memset_check_addr_prop_consistency(struct svm_prop *tar_prop, u64 start, u64 end)
{
    struct svm_prop prop;
    u64 va;
    int ret;

    for (va = start; va < end; va += prop.size) {
        ret = svm_get_prop(va, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Invalid addr. (ret=%d; va=0x%llx)\n", ret, va);
            return ret;
        }

        if ((prop.flag != tar_prop->flag) || (prop.devid != tar_prop->devid)) {
            svm_err("Addr's prop is not consistent.\n");
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    return DRV_ERROR_NONE;
}

static int svm_memset_check_addr_prop_cap(u64 va, u64 size, u64 cap_mask)
{
    struct svm_prop prop;
    int ret;

    /* todo: verify whether some addresses are in the svm range. */
    if (svm_va_is_in_range(va, size)) {
        ret = svm_get_prop(va, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Invalid addr. (ret=%d; va=0x%llx)\n", ret, va);
            return ret;
        }

        if ((prop.flag & cap_mask) == 0) {
            svm_err("Va not support cur cap. (va=0x%llx; prop.flag=0x%llx)\n", va, prop.flag);
            return DRV_ERROR_PARA_ERROR;
        }

        if ((va + size) > (prop.start + prop.size)) {
            ret = svm_memset_check_addr_prop_consistency(&prop, prop.start + prop.size, va + size);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
        }
    }

    return DRV_ERROR_NONE;
}

static int svm_memset_para_check(u64 dst, u64 dest_max, u8 value, u64 num)
{
    int ret;
    SVM_UNUSED(value);

    if (dst == 0) {
        svm_err("Addr is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dest_max < num) {
        svm_err("Num bigger than dest_max. (dst=0x%llx; num=%lu; dest_max=%lu)\n", dst, num, dest_max);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (num == 0) {
        svm_err("Num is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_memset_check_addr_prop_cap(dst, num, SVM_FLAG_CAP_MEMSET);
    if (ret != 0) {
        return ret;
    }

    return DRV_ERROR_NONE;
}

#define SVM_MEMSET_FAST_LIMIT_SIZE (512ULL * 1024ULL)
static u64 svm_get_host_zero_mem(u32 host_devid)
{
    static pthread_rwlock_t memset_fast_rwlock = PTHREAD_RWLOCK_INITIALIZER;
    static u64 host_zero_mem = 0;
    int ret;
    if (host_zero_mem == 0) {
        (void)pthread_rwlock_wrlock(&memset_fast_rwlock);
        if (host_zero_mem == 0) {
#ifndef EMU_ST /* Simulation ST is required and cannot be deleted. */
            ret = halMemAlloc((void **)&host_zero_mem, SVM_MEMSET_FAST_LIMIT_SIZE,
                              (MEM_HOST | ((u64)DEVMM_MODULE_ID << MEM_MODULE_ID_BIT)));
            if (ret != 0) {
                (void)pthread_rwlock_unlock(&memset_fast_rwlock);
                return 0;
            }
#endif
            (void)svm_memset_client(host_devid, host_zero_mem, SVM_MEMSET_FAST_LIMIT_SIZE, 0,
                                    SVM_MEMSET_FAST_LIMIT_SIZE);
        }
        (void)pthread_rwlock_unlock(&memset_fast_rwlock);
    }

    return host_zero_mem;
}

static int svm_mem_clear_fast(u32 host_devid, DVdeviceptr dst, size_t destMax, size_t num)
{
    u64 va = svm_get_host_zero_mem(host_devid);
    return (va == 0) ? (int)DRV_ERROR_INNER_ERR : (int)drvMemcpy(dst, destMax, va, num);
}

static bool svm_can_go_fast_mem_clear(u32 devid, u32 host_devid, u8 value, size_t num)
{
    return !svm_dbi_is_cc_vm() && (value == 0) && (devid != host_devid) && (num < SVM_MEMSET_FAST_LIMIT_SIZE);
}

drvError_t drvMemsetD8Inner(DVdeviceptr dst, size_t destMax, UINT8 value, size_t num);
drvError_t drvMemsetD8Inner(DVdeviceptr dst, size_t destMax, UINT8 value, size_t num)
{
    struct svm_prop prop;
    u32 host_devid, devid;
    int ret;
    svm_debug("Memset info. (va=0x%llx; value=%u; num=%lu)\n", dst, value, num);

    ret = svm_memset_para_check(dst, destMax, value, num);
    if (ret != DRV_ERROR_NONE) {
        svm_err("check failed (ret=%d; va=0x%llx)\n", ret, dst);
        return ret;
    }

    host_devid = svm_get_host_devid();
    if (svm_va_is_in_range(dst, num)) {
        ret = svm_get_prop(dst, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Invalid addr. (ret=%d; va=0x%llx)\n", ret, dst);
            return ret;
        }
        devid = prop.devid;
    } else {
        devid = host_devid;
    }

    if (svm_can_go_fast_mem_clear(devid, host_devid, value, num)) {
        ret = svm_mem_clear_fast(host_devid, dst, destMax, num);
        if (ret == 0) {
            return DRV_ERROR_NONE;
        }
    }

    return svm_memset_client(devid, dst, destMax, value, num);
}

drvError_t drvMemsetD8(DVdeviceptr dst, size_t destMax, UINT8 value, size_t num)
{
    return drvMemsetD8Inner(dst, destMax, value, num);
}
