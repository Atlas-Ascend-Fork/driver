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

#include "svm_init_pri.h"
#include "svm_criu.h"
#include "svm_sys_cmd.h"
#include "svm_ioctl_ex.h"
#include "svm_pub.h"
#include "svm_log.h"
#include "svm_dbi.h"
#include "va_dev_default_allocator.h"
#include "va_non_dev_default_allocator.h"
#include "va_pcie_th_allocator.h"
#include "va_dev_cp_only_allocator.h"
#include "va_dev_dcache_allocator.h"
#include "va_dev_linear_mem_allocator.h"
#include "va_dev_without_populate_allocator.h"
#include "va_gap.h"
#include "va_reserve.h"
#include "va_allocator.h"

#define VA_ALLOCATOR_TYPE_DEV_DEFAULT 0
#define VA_ALLOCATOR_TYPE_NON_DEV_DEFAULT 1
#define VA_ALLOCATOR_TYPE_DEV_CP_ONLY 2
#define VA_ALLOCATOR_TYPE_DEV_DCACHE 3
#define VA_ALLOCATOR_TYPE_PCIE_TH 4
#define VA_ALLOCATOR_TYPE_DEFAULT 5
#define VA_ALLOCATOR_TYPE_UNKNOWN 6
#define VA_ALLOCATOR_TYPE_VA_ONLY_WITHOUT_POPULATE 7

bool svm_va_is_in_range(u64 va, u64 size)
{
    bool is_reserved;

    if (svm_check_reserve_range(va, size, &is_reserved) != 0) {
        return false;
    }

    return is_reserved;
}

bool svm_is_valid_range(u64 va, u64 size)
{
    bool is_reserved;
    return (svm_check_reserve_range(va, size, &is_reserved) == 0);
}

int svm_get_va_type(u64 va, u64 size, int *va_type)
{
    bool is_reserved;
    int ret;

    ret = svm_check_reserve_range(va, size, &is_reserved);
    if (ret != 0) {
        return ret;
    }

    *va_type = is_reserved ? VA_TYPE_SVM : VA_TYPE_NON_SVM;
    return 0;
}

u32 svm_get_spacified_va_allocator_type(u64 va, u64 size, u32 devid, u32 flag)
{
    SVM_UNUSED(devid);

    /* dev cp only can not be specified va */
    if ((flag & SVM_VA_ALLOCATOR_FLAG_DEV_CP_ONLY) != 0) {
        svm_err("Specified flag dev cp only. (flag=%x)\n", flag);
        return VA_ALLOCATOR_TYPE_UNKNOWN;
    }

    if (svm_is_in_dcache_va_range(va, size)) {
        /* dcache can not with master */
        if ((flag & SVM_VA_ALLOCATOR_FLAG_WITH_MASTER) != 0) {
            svm_err("Dev dcache with master. (flag=%x)\n", flag);
            return VA_ALLOCATOR_TYPE_UNKNOWN;
        }

        return VA_ALLOCATOR_TYPE_DEV_DCACHE;
    }

    return (devid == SVM_INVALID_DEVID) ? VA_ALLOCATOR_TYPE_DEFAULT : VA_ALLOCATOR_TYPE_DEV_DEFAULT;
}

u32 svm_get_non_spacified_va_allocator_type(u64 va, u64 size, u32 devid, u32 flag)
{
    SVM_UNUSED(va);

    if ((flag & SVM_VA_ALLOCATOR_FLAG_DEV_CP_ONLY) != 0) {
        /* dev cp only can not with master */
        if ((flag & SVM_VA_ALLOCATOR_FLAG_WITH_MASTER) != 0) {
            svm_err("Dev cp only with master. (flag=%x)\n", flag);
            return VA_ALLOCATOR_TYPE_UNKNOWN;
        }

        return VA_ALLOCATOR_TYPE_DEV_CP_ONLY;
    }

    if (((flag & SVM_VA_ALLOCATOR_FLAG_MASTER_UVA) != 0) && (svm_is_support_pcie_th())) {
        return VA_ALLOCATOR_TYPE_PCIE_TH;
    }

    /* use default when not specified va but has no master or size is too bigger */
    if ((flag & SVM_VA_ALLOCATOR_FLAG_WITH_MASTER) == 0) {
        return VA_ALLOCATOR_TYPE_DEFAULT;
    }

    if (devid == SVM_INVALID_DEVID) {
        return (size > NON_DEV_DEFAULT_ALLOC_MAX_SIZE) ? VA_ALLOCATOR_TYPE_DEFAULT : VA_ALLOCATOR_TYPE_NON_DEV_DEFAULT;
    }

    return VA_ALLOCATOR_TYPE_DEV_DEFAULT;
}

u32 svm_get_va_allocator_type(u64 va, u64 size, u32 devid, u32 flag, int op)
{
    if ((flag & SVM_VA_ALLOCATOR_FLAG_MASTER_UVA) != 0) {
        if ((flag & SVM_VA_ALLOCATOR_FLAG_WITH_MASTER) == 0) {
            svm_err("Master uva must with master. (va=0x%llx; flag=%x)\n", va, flag);
            return VA_ALLOCATOR_TYPE_UNKNOWN;
        }
    }

    if ((flag & SVM_VA_ALLOCATOR_FLAG_WITHOUT_POPULATE) != 0) {
        return VA_ALLOCATOR_TYPE_VA_ONLY_WITHOUT_POPULATE;
    }

    if ((flag & SVM_VA_ALLOCATOR_FLAG_SPACIFIED_ADDR) != 0) {
        if ((op == 1) && (va == 0)) {
            svm_err("Specified flag but va is null. (va=0x%llx; flag=%x)\n", va, flag);
            return VA_ALLOCATOR_TYPE_UNKNOWN;
        }

        return svm_get_spacified_va_allocator_type(va, size, devid, flag);
    } else {
        if ((op == 1) && (va != 0)) {
            svm_err("No specified flag but va is not null. (va=0x%llx; flag=%x)\n", va, flag);
            return VA_ALLOCATOR_TYPE_UNKNOWN;
        }

        return svm_get_non_spacified_va_allocator_type(va, size, devid, flag);
    }
}

int svm_alloc_va(u64 va, u64 size, u64 align, u32 devid, u32 flag, u64 *start)
{
    u32 allocator_type = svm_get_va_allocator_type(va, size, devid, flag, 1);
    u32 reserve_flag = 0;
    int ret;

    if (size == 0) {
        svm_debug("Invalid size. (size=0x%llx)\n", size);
        /* It's not appropriate, we have to inherit the existing error codes. */
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    switch (allocator_type) {
        case VA_ALLOCATOR_TYPE_DEV_DEFAULT:
            return va_dev_default_alloc(devid, align, size, start);
        case VA_ALLOCATOR_TYPE_NON_DEV_DEFAULT:
            return va_non_dev_default_alloc(align, size, start);
        case VA_ALLOCATOR_TYPE_DEV_CP_ONLY:
            return va_dev_cp_only_alloc(devid, align, size, start);
        case VA_ALLOCATOR_TYPE_DEV_DCACHE:
            ret = va_dev_dcache_alloc(va, size);
            if (ret == 0) {
                *start = va;
            }
            return ret;
        case VA_ALLOCATOR_TYPE_PCIE_TH:
            return va_pcie_th_alloc(align, size, start);
        case VA_ALLOCATOR_TYPE_DEFAULT:
            reserve_flag |= SVM_VA_RESERVE_FLAG_WITH_CUSTOM_CP;
            reserve_flag |= SVM_VA_RESERVE_FLAG_WITH_HCCP;

            if ((flag & SVM_VA_ALLOCATOR_FLAG_WITH_MASTER) != 0) {
                reserve_flag |= SVM_VA_RESERVE_FLAG_WITH_MASTER;
            }

            return svm_reserve_va(va, size, reserve_flag, start);
        case VA_ALLOCATOR_TYPE_VA_ONLY_WITHOUT_POPULATE:
            return va_only_without_populate_alloc(devid, align, size, start);
        default:
            svm_err("Invalid para. (va=0x%llx size=0x%llx; devid=%u; flag=%x)\n", va, size, devid, flag);
            return DRV_ERROR_INVALID_VALUE;
    }
}

int svm_free_va(u64 va, u64 size, u64 align, u32 devid, u32 flag)
{
    u32 allocator_type = svm_get_va_allocator_type(va, size, devid, flag, 0);

    if (size == 0) {
        svm_err("Invalid size. (size=0x%llx)\n", size);
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (allocator_type) {
        case VA_ALLOCATOR_TYPE_DEV_DEFAULT:
            return va_dev_default_free(devid, va, size, align);
        case VA_ALLOCATOR_TYPE_NON_DEV_DEFAULT:
            return va_non_dev_default_free(va, size, align);
        case VA_ALLOCATOR_TYPE_DEV_CP_ONLY:
            return va_dev_cp_only_free(devid, va, size);
        case VA_ALLOCATOR_TYPE_DEV_DCACHE:
            return va_dev_dcache_free(va, size);
        case VA_ALLOCATOR_TYPE_PCIE_TH:
            return va_pcie_th_free(va, size, align);
        case VA_ALLOCATOR_TYPE_DEFAULT:
            return svm_release_va(va);
        case VA_ALLOCATOR_TYPE_VA_ONLY_WITHOUT_POPULATE:
            return va_only_without_populate_free(devid, va, size);
        default:
            svm_err("Invalid para. (va=0x%llx size=0x%llx; devid=%u; flag=%x)\n", va, size, devid, flag);
            return DRV_ERROR_INVALID_VALUE;
    }
}

static int svm_va_allocator_dev_init(u32 devid)
{
    int ret;

    ret = va_dev_default_allocator_dev_init(devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = va_reserve_add_dev(devid);
    if (ret != DRV_ERROR_NONE) {
        va_dev_default_allocator_dev_uninit(devid);
        return ret;
    }

    if (devid == svm_get_host_devid()) {
        ret = va_pcie_th_allocator_init();
        if (ret != DRV_ERROR_NONE) {
            va_reserve_del_dev(devid);
            va_dev_default_allocator_dev_uninit(devid);
            return ret;
        }

        return DRV_ERROR_NONE;
    }

    ret = va_dev_dcache_allocator_init();
    if (ret != DRV_ERROR_NONE) {
        va_reserve_del_dev(devid);
        va_dev_default_allocator_dev_uninit(devid);
        return ret;
    }

    ret = va_dev_linear_mem_enable();
    if (ret != DRV_ERROR_NONE) {
        /* not rollback dcache */
        va_reserve_del_dev(devid);
        va_dev_default_allocator_dev_uninit(devid);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int svm_va_allocator_dev_uninit(u32 devid)
{
    va_dev_default_allocator_dev_uninit(devid);

    if (devid == svm_get_host_devid()) {
        return DRV_ERROR_NONE;
    }

    va_reserve_del_dev(devid);
    return DRV_ERROR_NONE;
}

static int svm_va_allocator_criu_reset_dev(u32 devid, void *data)
{
    SVM_UNUSED(data);
    va_reserve_del_dev(devid);
    return 0;
}

static const struct svm_criu_ops g_va_allocator_criu_ops = {
    .name = "va_allocator",
    .reset = svm_va_allocator_criu_reset_dev,
};

void __attribute__((constructor(SVM_INIT_PRI_HIGH))) svm_va_allocator_init(void)
{
    int ret;

    ret = svm_register_ioctl_dev_init_post_handle(svm_va_allocator_dev_init);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev init post handle failed.\n");
    }

    ret = svm_register_ioctl_dev_uninit_pre_handle(svm_va_allocator_dev_uninit);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev uninit post handle failed.\n");
    }

    ret = svm_criu_register_ops(&g_va_allocator_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed.\n");
    }
}

void svm_va_allocator_uninit(void)
{
    va_non_dev_default_allocator_uninit();
    va_pcie_th_allocator_uninit();
    va_dev_cp_only_allocator_uninit();
    va_dev_linear_mem_disable();
    va_dev_dcache_allocator_uninit();
    va_only_without_populate_allocator_uninit();
}
