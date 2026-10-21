/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

#include "ascend_hal.h"

#include "drv_user_common.h" /* user list */

#include "svm_addr_desc.h"
#include "svm_user_interface.h"
#include "svm_user_adapt.h"
#include "svm_log.h"
#include "svm_pagesize.h"
#include "svm_dbi.h"
#include "malloc_mng.h"
#include "svm_vmm.h"
#include "svmm.h"
#include "va_allocator.h"
#include "svm_register.h"
#include "svm_register_to_master.h"
#include "svm_share_type.h"
#include "svm_pipeline.h"
#include "svm_memcpy_local.h"
#include "svm_memcpy.h"

#define ACCESS_MAX_LOCAL_MEM_NUM 16
#define SVM_MAX_ACCESS_SIZE (128ULL * SVM_BYTES_PER_MB)

struct svm_access_local_mem {
    int status; /* 0: idle, 1: in use */
    u32 size;
    u64 va;
};

static pthread_mutex_t access_local_mem_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct svm_access_local_mem access_local_mem[ACCESS_MAX_LOCAL_MEM_NUM];
static u64 register_num = 0;

static struct svm_access_local_mem *access_get_idle_local_mem(void)
{
    int i;

    for (i = 0; i < ACCESS_MAX_LOCAL_MEM_NUM; i++) {
        struct svm_access_local_mem *local_mem = &access_local_mem[i];

        if ((local_mem->va != 0) && (local_mem->status == 0)) {
            return local_mem;
        }
    }

    return NULL;
}

static struct svm_access_local_mem *access_local_mem_get(void)
{
    struct svm_access_local_mem *local_mem = NULL;

    (void)pthread_mutex_lock(&access_local_mem_mutex);
    local_mem = access_get_idle_local_mem();
    if (local_mem != NULL) {
        local_mem->status = 1;
    }
    (void)pthread_mutex_unlock(&access_local_mem_mutex);

    return local_mem;
}

static void access_local_mem_put(struct svm_access_local_mem *local_mem)
{
    local_mem->status = 0;
}

static void access_local_mem_init_locked(void)
{
    int i, ret;
    u64 page_size;

    ret = svm_dbi_query_npage_size(svm_get_host_devid(), &page_size);
    if (ret != 0) {
        svm_warn("Get page size failed.\n");
        return;
    }

    for (i = 0; i < ACCESS_MAX_LOCAL_MEM_NUM; i++) {
        void *host_va;
        struct svm_access_local_mem *local_mem = &access_local_mem[i];

        ret = halMemAlloc(&host_va, page_size, MEM_HOST | ((u64)DEVMM_MODULE_ID << MEM_MODULE_ID_BIT));
        if (ret != 0) {
            svm_warn("Alloc access local mem failed. (i=%d)\n", i);
            break;
        }

        local_mem->va = (u64)(uintptr_t)host_va;
        local_mem->size = (u32)page_size;
    }
}

static void access_local_mem_uninit_locked(void)
{
    int i;

    for (i = 0; i < ACCESS_MAX_LOCAL_MEM_NUM; i++) {
        struct svm_access_local_mem *local_mem = &access_local_mem[i];
        if (local_mem->va != 0) {
            (void)halMemFree((void *)(uintptr_t)local_mem->va);
            local_mem->va = 0;
        }
    }
}

static void access_local_mem_init(void)
{
    (void)pthread_mutex_lock(&access_local_mem_mutex);
    if (register_num == 0) {
        access_local_mem_init_locked();
    }
    register_num++;
    (void)pthread_mutex_unlock(&access_local_mem_mutex);
}

static void access_local_mem_uninit(void)
{
    (void)pthread_mutex_lock(&access_local_mem_mutex);
    if (register_num == 0) {
        svm_warn("Access local mem is not initialized.\n");
        (void)pthread_mutex_unlock(&access_local_mem_mutex);
        return;
    }

    register_num--;
    if (register_num == 0) {
        access_local_mem_uninit_locked();
    }
    (void)pthread_mutex_unlock(&access_local_mem_mutex);
}

static bool svm_access_is_write(u32 flag)
{
    return ((flag & SVM_MEM_ACCESS_WRITE) != 0);
}

static bool svm_access_is_read(u32 flag)
{
    return ((flag & SVM_MEM_ACCESS_READ) != 0);
}

static bool svm_register_flag_is_access_dma(u64 flag)
{
    return ((flag & SVM_REGISTER_FLAG_ACCESS_BY_DMA) != 0);
}

static bool svm_register_flag_is_support(u64 flag)
{
    static u32 g_support_flag[] = {
        SVM_REGISTER_FLAG_ACCESS_BY_DMA,
    };
    u64 i;

    for (i = 0; i < (sizeof(g_support_flag) / sizeof(g_support_flag[0])); i++) {
        if (flag == g_support_flag[i]) {
            return true;
        }
    }

    return false;
}

static int register_check_svm_va_prop_consistency(struct svm_prop *tar_prop, u64 start, u64 end)
{
    struct svm_prop prop;
    u64 va;
    int ret;

    for (va = start; va < end; va += prop.size) {
        ret = svm_get_prop(va, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Invalid addr. (ret=%d; va=0x%llx)\n", ret, va);
            return DRV_ERROR_INVALID_VALUE;
        }

        if ((prop.flag != tar_prop->flag) || (prop.devid != tar_prop->devid)) {
            svm_err("Addr's prop is not consistent.\n");
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    return DRV_ERROR_NONE;
}

static int register_check_svm_va_prop_cap(u32 devid, u64 va, u64 size)
{
    struct svm_prop prop;
    int ret;

    ret = svm_get_prop(va, &prop);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Invalid addr. (ret=%d; va=0x%llx)\n", ret, va);
        return ret;
    }

    if (!svm_flag_cap_is_support_register_access(prop.flag)) {
        svm_run_info("Addr cap does not support register peer. (va=0x%llx)\n", va);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((devid != prop.devid)) {
        svm_run_info("Only support register to self device. (devid=%u; prop.devid=%u)\n", devid, prop.devid);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((va + size) > (prop.start + prop.size)) {
        ret = register_check_svm_va_prop_consistency(&prop, prop.start + prop.size, va + size);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Check addr prop failed. (ret=%d; va=0x%llx; size=%llu)\n", ret, va, size);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

static int svm_register_unregister_common_para_check(u32 devid, u64 va, u64 flag, bool is_svm_va)
{
    int ret = DRV_ERROR_NONE;

    if ((devid >= SVM_MAX_DEV_AGENT_NUM) || (va == 0)) {
        svm_err("Invalid para. (devid=%u; va=0x%llx)\n", devid, va);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!svm_is_npage_align(devid, va)) {
        svm_err("Devid is invalid, or va is not npage aligned. (devid=%u; va=0x%llx)\n", devid, va);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!svm_register_flag_is_support(flag)) {
        svm_run_info("Flag is not supported. (flag=0x%llx)\n", flag);
        return DRV_ERROR_NOT_SUPPORT;
    }

#ifndef EMU_ST /* emu_st support svm addr */
    if (is_svm_va) {
        svm_run_info("Svm addr is not supported. (flag=0x%llx)\n", flag);
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif
    return ret;
}

static int svm_register_with_access_va(u32 devid, u64 va, u64 size, u64 flag, u64 *access_va)
{
    struct svm_dst_va register_va;
    u32 host_devid, register_flag = 0;
    u64 dst_va;
    int ret;

    host_devid = svm_get_host_devid();

    if (svm_register_flag_is_access_dma(flag)) {
        register_flag |= REGISTER_TO_MASTER_FLAG_ACCESS_WRITE;
        register_flag |= REGISTER_TO_MASTER_FLAG_APM_MAPED_REG;

        svm_dst_va_pack(devid, DEVDRV_PROCESS_CP1, va, size, &register_va);
        ret = svm_register_to_master(host_devid, &register_va, register_flag);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Register to master failed. (ret=%d; devid=%u; va=0x%llx; size=%llu)\n", ret, devid, va, size);
            return ret;
        }

        access_local_mem_init();
        *access_va = va;
    } else {
        bool is_svm_va = svm_va_is_in_range(va, size);
        if (is_svm_va) {
            ret = svm_register_to_peer(va, size, host_devid, 0, &dst_va);
            if (ret == DRV_ERROR_NONE) { /* if register to peer failed, try register_to_master and use access api to
                                            read write */
                *access_va = va;
            }
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

static int svm_unregister_with_access_va(u32 devid, u64 va, u64 flag)
{
    struct svm_dst_va register_va;
    u32 host_devid, register_flag = 0;
    int ret = DRV_ERROR_INVALID_VALUE;

    host_devid = svm_get_host_devid();

    if (svm_register_flag_is_access_dma(flag)) {
        register_flag |= REGISTER_TO_MASTER_FLAG_APM_MAPED_REG;

        /* size: 0, will use the size saved before, no need size input */
        svm_dst_va_pack(devid, DEVDRV_PROCESS_CP1, va, 0, &register_va);
        ret = svm_unregister_to_master(host_devid, &register_va, register_flag);
        if (ret == DRV_ERROR_NONE) {
            access_local_mem_uninit();
        }
    } else {
        bool is_svm_va = svm_va_is_in_range(va, 1);
        if (is_svm_va) {
            struct svm_prop prop;

            ret = svm_get_prop(va, &prop);
            if (ret != DRV_ERROR_NONE) {
                svm_err("Get prop failed. (va=0x%llx)\n", va);
                return ret;
            }

            if (svm_flag_cap_is_support_ldst(prop.flag)) {
                return svm_unregister_to_peer(va, host_devid);
            }
        }
    }
    return ret;
}

static int svm_register_raw(u32 devid, u64 va, u64 size)
{
    struct svm_dst_va register_va;
    u64 aligned_size, page_size;
    u32 flag = 0;
    int ret;

    flag |= REGISTER_TO_MASTER_FLAG_ACCESS_WRITE;

    ret = svm_get_npage_aligned_size(devid, size, &aligned_size);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get aligned size failed. (devid=%u; va=0x%llx; size=0x%llx)\n", devid, va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_dbi_query_npage_size(devid, &page_size);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Query npage size failed. (devid=%u; va=0x%llx; size=0x%llx)\n", devid, va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_dst_va_pack(devid, DEVDRV_PROCESS_CP1, svm_align_down(va, page_size), aligned_size, &register_va);
    ret = svm_register_to_master(devid, &register_va, flag);
    if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_NOT_SUPPORT)) {
        svm_err("Register to master failed. (ret=%d; devid=%u; va=0x%llx; size=%llu)\n", ret, devid, va, aligned_size);
    }

    return ret;
}

static int svm_unregister_raw(u32 devid, u64 va)
{
    struct svm_dst_va register_va;
    u64 page_size;
    u32 flag = 0;
    int ret;

    flag |= REGISTER_TO_MASTER_FLAG_ACCESS_WRITE;

    ret = svm_dbi_query_npage_size(devid, &page_size);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Query npage size failed. (devid=%u; va=0x%llx)\n", devid, va);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_dst_va_pack(devid, DEVDRV_PROCESS_CP1, svm_align_down(va, page_size), 0, &register_va);
    ret = svm_unregister_to_master(devid, &register_va, flag);
    if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_NOT_SUPPORT)) {
        svm_err("Unregister to master failed. (ret=%d; devid=%u; va=0x%llx)\n", ret, devid, va);
    }

    return ret;
}

drvError_t halSvmAccessRegister(uint32_t devid, uint64_t va, uint64_t size, uint64_t flag, uint64_t *access_va)
{
    bool is_svm_va = svm_va_is_in_range(va, size);
    int ret = DRV_ERROR_INVALID_VALUE;

    DRV_CURRENT_API_SCOPE();

    ret = svm_register_unregister_common_para_check(devid, va, flag, is_svm_va);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (access_va == NULL) {
        svm_err("Access_va para is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((size == 0) || !svm_is_npage_align(devid, size)) {
        svm_err("Invalid size. (devid=%u; va=0x%llx; size=%llu)\n", devid, va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((va + size) <= va) {
        svm_err("Va and size overflow. (devid=%u; va=0x%llx; size=%llu)\n", devid, va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_register_with_access_va(devid, va, size, flag, (u64 *)access_va);
    svm_debug("Register. (ret=%d; devid=%u; va=0x%llx; size=%llu; flag=0x%llx)\n", ret, devid, va, size, flag);
    return ret;
}

drvError_t halSvmAccessUnRegister(uint32_t devid, uint64_t va, uint64_t flag)
{
    bool is_svm_va = svm_va_is_in_range(va, 1);
    int ret = DRV_ERROR_INVALID_VALUE;

    ret = svm_register_unregister_common_para_check(devid, va, flag, is_svm_va);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_unregister_with_access_va(devid, va, flag);
    svm_debug("Unregister. (ret=%d; devid=%u; va=0x%llx; flag=0x%llx)\n", ret, devid, va, flag);
    return ret;
}

static int svm_access_direct(u64 access_va, u64 local_va, u64 size, u32 flag)
{
    void *src, *dst;
    int ret;

    src = svm_access_is_write(flag) ? (void *)(uintptr_t)local_va : (void *)(uintptr_t)access_va;
    dst = svm_access_is_write(flag) ? (void *)(uintptr_t)access_va : (void *)(uintptr_t)local_va;

    ret = svm_memcpy_local((u64)(uintptr_t)dst, size, (u64)(uintptr_t)src, size);
    if (ret != 0) {
        svm_err("Memcpy error. (access_va=0x%llx; local_va=0x%llx; size=0x%x; flag=0x%x; ret=%d)\n", access_va,
                local_va, size, flag, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    return 0;
}

static int _svm_access_by_dma(u32 devid, u64 access_va, u64 register_local_va, u64 size, u32 flag)
{
    struct svm_copy_va_info src_info, dst_info;
    u32 host_devid;

    host_devid = svm_get_host_devid();

    if (svm_access_is_write(flag)) {
        svm_copy_va_info_pack(register_local_va, size, host_devid, &src_info);
        svm_copy_va_info_pack(access_va, size, devid, &dst_info);
    } else {
        svm_copy_va_info_pack(register_local_va, size, host_devid, &dst_info);
        svm_copy_va_info_pack(access_va, size, devid, &src_info);
    }

    return svm_sync_copy(&src_info, &dst_info);
}

static int svm_access_check_local_svm_va(u64 local_va, u64 size)
{
    u32 host_devid = svm_get_host_devid();
    u64 end = local_va + size;
    u64 va;
    int ret;

    for (va = local_va; va < end;) {
        struct svm_prop prop;

        ret = svm_get_prop(va, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Get prop failed. (va=0x%llx; ret=%d)\n", va, ret);
            return ret;
        }

        if ((prop.flag & SVM_FLAG_CAP_SYNC_COPY) == 0) {
            svm_err("Va not support cur cap. (va=0x%llx; prop.flag=0x%llx)\n", va, prop.flag);
            return DRV_ERROR_PARA_ERROR;
        }
        if (prop.devid != host_devid) {
            svm_err("Local va is not host svm addr. (local_va=0x%llx; prop.devid=%u; host_devid=%u)\n", local_va,
                    prop.devid, host_devid);
            return DRV_ERROR_INVALID_VALUE;
        }
        va = prop.start + prop.size;
    }

    return DRV_ERROR_NONE;
}

static int svm_access_check_local_va(u64 local_va, u64 size)
{
    int va_type;
    int ret;

    if ((local_va + size) <= local_va) {
        svm_err("Va and size overflow. (local_va=0x%llx; size=%llu)\n", local_va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_get_va_type(local_va, size, &va_type);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Invalid local va range. (local_va=0x%llx; size=%llu; ret=%d)\n", local_va, size, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (va_type == VA_TYPE_NON_SVM) {
        return DRV_ERROR_NONE;
    }

    return svm_access_check_local_svm_va(local_va, size);
}

static int svm_access_by_dma(u32 devid, u64 access_va, u64 local_va, u64 size, u32 flag)
{
    struct svm_dst_va register_va;
    u32 register_to_master_flag = 0;
    u32 host_devid;
    u64 local_mem_size;
    int ret;

    host_devid = svm_get_host_devid();

    if (svm_va_is_in_range(local_va, size)) {
        struct svm_prop prop;

        ret = svm_get_prop(local_va, &prop);
        if (ret != 0) {
            svm_err("Get prop failed. (va=0x%llx)\n", local_va);
            return DRV_ERROR_INVALID_VALUE;
        }

        if (prop.devid != host_devid) {
            svm_err("Va does not belong to host. (va=0x%llx; prop.devid=%u)\n", local_va, prop.devid);
            return DRV_ERROR_INVALID_VALUE;
        }

        return _svm_access_by_dma(devid, access_va, local_va, size, flag);
    }

    ret = svm_dbi_query_npage_size(host_devid, &local_mem_size);
    if (ret != 0) {
        svm_err("Get page size failed.\n");
        return ret;
    }

    if (size <= local_mem_size) {
        struct svm_access_local_mem *local_mem = access_local_mem_get();
        if (local_mem != NULL) {
            ret = svm_memcpy_local(local_mem->va, size, local_va, size);
            if (ret != 0) {
                svm_err("Memcpy error. (access_va=0x%llx; local_va=0x%llx; size=%llu; flag=0x%x; ret=%d)\n", access_va,
                        local_va, size, flag, ret);
                ret = DRV_ERROR_INVALID_VALUE;
            } else {
                ret = _svm_access_by_dma(devid, access_va, local_mem->va, size, flag);
                if ((ret == 0) && svm_access_is_read(flag)) {
                    /* If read access, we need to update the local memory */
                    ret = svm_memcpy_local(local_va, size, local_mem->va, size);
                    if (ret != 0) {
                        svm_err("Memcpy err. (access_va=0x%llx; local_va=0x%llx; size=%llu; flag=0x%x; ret=%d)\n",
                                access_va, local_va, size, flag, ret);
                        ret = DRV_ERROR_INVALID_VALUE;
                    }
                }
            }

            access_local_mem_put(local_mem);
            return ret;
        }
    }

    if (svm_get_device_connect_type(devid) == HOST_DEVICE_CONNECT_TYPE_PCIE) {
        return _svm_access_by_dma(devid, access_va, local_va, size, flag);
    }

    svm_dst_va_pack(host_devid, 0, local_va, size, &register_va);
    /* Write access va means read local va */
    register_to_master_flag |= svm_access_is_write(flag) ? 0 : REGISTER_TO_MASTER_FLAG_ACCESS_WRITE;
    register_to_master_flag |= REGISTER_TO_MASTER_FLAG_PIN;

    /* register local va */
    ret = svm_register_to_master(devid, &register_va, register_to_master_flag);
    if (ret != 0) {
        svm_err("Local mem register failed. (access_va=0x%llx; local_va=0x%llx; size=0x%x; flag=0x%x; ret=%d)\n",
                access_va, local_va, size, flag, ret);
        return ret;
    }
    ret = _svm_access_by_dma(devid, access_va, local_va, size, flag);
    svm_unregister_to_master(devid, &register_va, 0);

    return ret;
}

drvError_t halSvmAccess(uint32_t devid, uint64_t access_va, uint64_t local_va, uint64_t size, uint32_t flag)
{
    int ret;

    svm_debug("Access. (devid=%u; access_va=0x%llx; local_va=0x%llx; size=0x%llx; flag=0x%x)\n", devid, access_va,
              local_va, size, flag);

    if ((devid >= SVM_MAX_DEV_AGENT_NUM) || (size == 0) || (size > SVM_MAX_ACCESS_SIZE) || (access_va == 0) ||
        (local_va == 0)) {
        svm_err("Invalid para. (devid=%u; access_va=0x%llx; local_va=0x%llx; size=0x%llx)\n", devid, access_va,
                local_va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((svm_access_is_write(flag) && svm_access_is_read(flag)) ||
        (!svm_access_is_write(flag) && !svm_access_is_read(flag))) {
        svm_err("Invalid flag. (flag=0x%x)\n", flag);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_access_check_local_va(local_va, size);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (svm_va_is_in_range(access_va, size)) {
        struct svm_prop prop;

        ret = svm_get_prop(access_va, &prop);
        if (ret != 0) {
            svm_err("Get prop failed. (va=0x%llx)\n", access_va);
            return ret;
        }

        if (prop.devid != devid) {
            svm_err("Va does not belong to device. (va=0x%llx; devid=%u; prop.devid=%u)\n", access_va, devid,
                    prop.devid);
            return DRV_ERROR_INVALID_VALUE;
        }

        if (svm_flag_cap_is_support_ldst(prop.flag)) {
            return svm_access_direct(access_va, local_va, size, flag);
        }
    }

    return svm_access_by_dma(devid, access_va, local_va, size, flag);
}

static int svm_ub_seg_register_unregister_para_check(uint32_t devid, uint64_t va, uint64_t size)
{
    bool is_svm_va = svm_va_is_in_range(va, size);

    if ((devid >= SVM_MAX_DEV_AGENT_NUM) || (size == 0) || (va == 0)) {
        svm_err("Invalid para. (devid=%u; va=0x%llx; size=0x%x)\n", devid, va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!is_svm_va) {
        svm_run_info("Not svm addr. (devid=%u; va=0x%llx; size=%llu)\n", devid, va, size);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return register_check_svm_va_prop_cap(devid, va, size);
}

drvError_t halMemRegUbSegment(uint32_t devid, uint64_t va, uint64_t size)
{
    int ret;

    ret = svm_ub_seg_register_unregister_para_check(devid, va, size);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_register_raw(devid, va, size);
    svm_debug("RegUbSegment. (ret=%d; devid=%u; va=0x%llx; size=%llu)\n", ret, devid, va, size);
    return ret;
}

drvError_t halMemUnRegUbSegment(uint32_t devid, uint64_t va)
{
    int ret;

    /* ignore size */
    ret = svm_ub_seg_register_unregister_para_check(devid, va, 1);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_unregister_raw(devid, va);
    svm_debug("UnRegUbSegment. (ret=%d; devid=%u; va=0x%llx)\n", ret, devid, va);
    return ret;
}
