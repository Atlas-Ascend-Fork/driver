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

#include "svm_log.h"
#include "svm_sys_cmd.h"
#include "normal_malloc.h"
#include "malloc_mng.h"
#include "svm_register_to_master.h"
#include "svm_dbi.h"
#include "svm_vmm.h"
#include "cache_malloc.h"
#include "svmm.h"

static bool g_support_register[SVM_MAX_DEV_AGENT_NUM] = {false};

static int svm_host_register(u32 user_devid, u64 va, u64 size)
{
    u32 register_flag = REGISTER_TO_MASTER_FLAG_ACCESS_WRITE;
    struct svm_dst_va register_va;
    int ret;

    if (g_support_register[user_devid] == false) {
        return 0;
    }

    svm_dst_va_pack(svm_get_host_devid(), 0, va, size, &register_va);
    ret = svm_register_to_master(user_devid, &register_va, register_flag);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register local mem to master failed. (ret=%d; devid=%u; host_va=0x%llx; size=%llu)\n", ret, user_devid,
                va, size);
    }
    return ret;
}

static int svm_host_unregister(u32 user_devid, u64 va, u64 size)
{
    struct svm_dst_va register_va;
    int ret;

    if (g_support_register[user_devid] == false) {
        return 0;
    }

    svm_dst_va_pack(svm_get_host_devid(), 0, va, size, &register_va);
    ret = svm_unregister_to_master(user_devid, &register_va, 0);
    if (ret != DRV_ERROR_NONE) {
        svm_err_if(((ret != DRV_ERROR_PARA_ERROR) && (ret != DRV_ERROR_BUSY)),
                   "Unregister local mem to master failed. (ret=%d; devid=%u; host_va=0x%llx; size=%llu)\n", ret,
                   user_devid, va, size);
    }
    return ret;
}

static int svm_try_host_register_vmm_existed_mem(void *seg_handle, u64 start, struct svm_global_va *src_info,
                                                 void *priv)
{
    u64 svm_flag = svm_svmm_get_seg_svm_flag(seg_handle);
    u32 devid = svm_svmm_get_seg_devid(seg_handle);
    u32 user_devid = *(u32 *)priv;

    return ((devid == svm_get_host_devid()) && ((svm_flag & SVM_FLAG_CAP_SYNC_COPY) != 0) &&
            (svm_flag_cap_is_support_vmm_ipc_unmap(svm_flag) == false)) ?
               svm_host_register(user_devid, start, src_info->size) :
               0;
}

static int svm_try_host_unregister_vmm_existed_mem(void *seg_handle, u64 start, struct svm_global_va *src_info,
                                                   void *priv)
{
    u64 svm_flag = svm_svmm_get_seg_svm_flag(seg_handle);
    u32 devid = svm_svmm_get_seg_devid(seg_handle);
    u32 user_devid = *(u32 *)priv;

    return ((devid == svm_get_host_devid()) && ((svm_flag & SVM_FLAG_CAP_SYNC_COPY) != 0) &&
            (svm_flag_cap_is_support_vmm_ipc_unmap(svm_flag) == false)) ?
               svm_host_unregister(user_devid, start, src_info->size) :
               0;
}

static int svm_try_host_register_existed_mem(void *va_handle, u64 start, struct svm_prop *prop, void *priv)
{
    u32 devid = *(u32 *)priv;
    SVM_UNUSED(va_handle);

    if (svm_flag_cap_is_support_normal_free(prop->flag)) {
        if ((svm_flag_attr_is_va_only(prop->flag) == false) && (prop->is_from_cache == false) &&
            (prop->devid == svm_get_host_devid()) && ((prop->flag & SVM_FLAG_CAP_SYNC_COPY) != 0)) {
            return svm_host_register(devid, start, prop->aligned_size);
        }
    } else {
        void *vmm_svmm = vmm_get_svmm(va_handle);
        return (vmm_svmm == NULL) ? 0 :
                                    svm_svmm_for_each_seg_handle(vmm_svmm, svm_try_host_register_vmm_existed_mem, priv);
    }

    return 0;
}

static int svm_try_host_unregister_existed_mem(void *va_handle, u64 start, struct svm_prop *prop, void *priv)
{
    u32 devid = *(u32 *)priv;
    SVM_UNUSED(va_handle);

    if (svm_flag_cap_is_support_normal_free(prop->flag)) {
        if ((svm_flag_attr_is_va_only(prop->flag) == false) && (prop->is_from_cache == false) &&
            (prop->devid == svm_get_host_devid()) && ((prop->flag & SVM_FLAG_CAP_SYNC_COPY) != 0)) {
            return svm_host_unregister(devid, start, prop->aligned_size);
        }
    } else {
        void *vmm_svmm = vmm_get_svmm(va_handle);
        return (vmm_svmm == NULL) ?
                   0 :
                   svm_svmm_for_each_seg_handle(vmm_svmm, svm_try_host_unregister_vmm_existed_mem, priv);
    }
    return 0;
}

static int svm_try_host_register_cache_mem(u32 devid, u64 start, u64 size, u32 flag, void *priv)
{
    u32 user_devid = *(u32 *)priv;

    SVM_UNUSED(devid);
    SVM_UNUSED(flag);
    return svm_host_register(user_devid, start, size);
}

static int svm_try_host_unregister_cache_mem(u32 devid, u64 start, u64 size, u32 flag, void *priv)
{
    u32 user_devid = *(u32 *)priv;

    SVM_UNUSED(devid);
    SVM_UNUSED(flag);
    return svm_host_unregister(user_devid, start, size);
}

static int svm_host_register_enable(u32 devid)
{
    int ret;

    if ((devid == svm_get_host_devid()) || g_support_register[devid]) {
        return 0;
    }

    /* Bounce-buffer mappings are limited and must be created explicitly through HOST_PINNED. */
    if (svm_dbi_is_cc_vm()) {
        return 0;
    }

    g_support_register[devid] = true; /* svm_try_host_register* will call svm_host_register, which will judge if
                                         support_register */
    ret = svm_for_each_valid_handle(svm_try_host_register_existed_mem, (void *)&devid);
    if (ret != 0) {
        svm_err("Try host register existed mem failed. (ret=%d; devid=%u)\n", ret, devid);
        g_support_register[devid] = false;
        return ret;
    }

    ret = svm_cache_for_each_range(svm_get_host_devid(), svm_try_host_register_cache_mem, (void *)&devid);
    if (ret != 0) {
        svm_err("Try host register cache mem failed. (ret=%d; devid=%u)\n", ret, devid);
        (void)svm_for_each_valid_handle(svm_try_host_unregister_existed_mem, (void *)&devid);
        g_support_register[devid] = false;
        return ret;
    }

    return 0;
}

static int svm_host_register_disable(u32 devid)
{
    u32 hd_connect_type = svm_get_device_connect_type(devid);

    if ((devid == svm_get_host_devid()) || !g_support_register[devid]) {
        return 0;
    }

    /* Pcie scene will error when addr is in use, so we rely on kernel recycling */
    if (hd_connect_type == HOST_DEVICE_CONNECT_TYPE_PCIE) {
        g_support_register[devid] = false;
        return 0;
    }

    (void)svm_cache_for_each_range(svm_get_host_devid(), svm_try_host_unregister_cache_mem, (void *)&devid);
    (void)svm_for_each_valid_handle(svm_try_host_unregister_existed_mem, (void *)&devid);
    g_support_register[devid] = false;
    return 0;
}

static void svm_host_register_post_malloc(u32 devid, u64 start, u64 size, u32 flag)
{
    u32 i;

    if ((devid == svm_get_host_devid()) && ((flag & SVM_NORMAL_MALLOC_FLAG_CAP_COPY) != 0)) {
        for (i = 0; i < SVM_MAX_DEV_AGENT_NUM; i++) {
            (void)svm_host_register(i, start, size);
        }
    }
}

static void svm_host_unregister_pre_free(u32 devid, u64 start, u64 size, u32 flag)
{
    u32 i;

    if ((devid == svm_get_host_devid()) && ((flag & SVM_NORMAL_MALLOC_FLAG_CAP_COPY) != 0)) {
        for (i = 0; i < SVM_MAX_DEV_AGENT_NUM; i++) {
            (void)svm_host_unregister(i, start, size);
        }
    }
}

struct svm_normal_ops host_register_ops = {.post_malloc = svm_host_register_post_malloc,
                                           .pre_free = svm_host_unregister_pre_free};

static int svm_host_register_post_map(u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info)
{
    u32 i, j;
    int ret;

    if ((devid == svm_get_host_devid()) && ((svm_flag & SVM_FLAG_CAP_SYNC_COPY) != 0)) {
        for (i = 0; i < SVM_MAX_DEV_AGENT_NUM; i++) {
            ret = svm_host_register(i, start, src_info->size);
            if (ret != 0) {
                for (j = 0; j < i; j++) {
                    (void)svm_host_unregister(j, start, src_info->size);
                }
                return ret;
            }
        }
    }
    return 0;
}

static int svm_host_register_pre_unmap(u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info)
{
    u32 i;

    if ((devid == svm_get_host_devid()) && ((svm_flag & SVM_FLAG_CAP_SYNC_COPY) != 0)) {
        for (i = 0; i < SVM_MAX_DEV_AGENT_NUM; i++) {
            int ret = svm_host_unregister(i, start, src_info->size);
            if (ret == DRV_ERROR_BUSY) {
                return ret;
            }
        }
    }
    return 0;
}

struct svm_vmm_ops vmm_host_register_ops = {.post_map = svm_host_register_post_map,
                                            .pre_unmap = svm_host_register_pre_unmap};

void __attribute__((constructor)) svm_host_register_init(void)
{
    int ret;

    svm_normal_set_ops(&host_register_ops);
    svm_vmm_set_ops(&vmm_host_register_ops);

    ret = svm_register_ioctl_dev_init_post_handle(svm_host_register_enable);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev init post handle failed.\n");
    }

    ret = svm_register_ioctl_dev_uninit_pre_handle(svm_host_register_disable);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev uninit pre handle failed.\n");
    }
}
