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

#include "svm_urma_def.h"
#include "svm_pub.h"
#include "svm_addr_desc.h"
#include "svm_log.h"
#include "va_allocator.h"
#include "svm_criu.h"
#include "svm_sys_cmd.h"
#include "svm_user_adapt.h"
#include "svm_urma_seg_mng.h"
#include "svm_urma_chan.h"
#include "svm_dbi.h"
#include "svm_timestamp.h"

static int svm_urma_dev_valid[SVM_MAX_DEV_NUM];

bool svm_urma_id_dev_valid(u32 devid)
{
    return (svm_urma_dev_valid[devid] != 0);
}

static int svm_urma_seg_sp_init(u32 devid)
{
    struct svm_dst_va dst_va;
    int ret;

    svm_dst_va_pack(devid, PROCESS_CP1, SP_VA_START, SP_VA_SIZE, &dst_va);

    ret = svm_urma_register_seg(svm_get_host_devid(), &dst_va, SVM_URMA_SEG_FLAG_ACCESS_WRITE);
    if (ret == DRV_ERROR_NONE) {
        svm_debug("Urma seg init. (devid=%u; va=0x%llx; size=0x%llx; user_devid=%u)\n", devid, SP_VA_START, SP_VA_SIZE,
                  svm_get_host_devid());
    }

    /* may be not support, return ok */
    return 0;
}

static void svm_urma_seg_sp_uninit(u32 devid)
{
    struct svm_dst_va dst_va;
    svm_dst_va_pack(devid, PROCESS_CP1, SP_VA_START, SP_VA_SIZE, &dst_va);

    svm_debug("Urma seg uninit. (devid=%u; va=0x%llx; size=0x%llx; user_devid=%u)\n", devid, SP_VA_START, SP_VA_SIZE,
              svm_get_host_devid());
    (void)svm_urma_unregister_seg(svm_get_host_devid(), &dst_va, 0);
}

static int svm_urma_seg_init(u32 devid, u64 va, u64 size, u32 user_devid)
{
    struct svm_dst_va dst_va;
    int ret;

    svm_dst_va_pack(devid, PROCESS_CP1, va, size, &dst_va);

    ret = svm_urma_register_seg(user_devid, &dst_va, SVM_URMA_SEG_FLAG_ACCESS_WRITE);
    if (ret == DRV_ERROR_NONE) {
        svm_debug("Urma seg init. (devid=%u; va=0x%llx; size=0x%llx; user_devid=%u)\n", devid, va, size, user_devid);
    }

    /* may be not support, return ok */
    return 0;
}

static void svm_urma_seg_uninit(u32 devid, u64 va, u64 size, u32 user_devid)
{
    struct svm_dst_va dst_va;

    svm_dst_va_pack(devid, PROCESS_CP1, va, size, &dst_va);

    svm_debug("Urma seg uninit. (devid=%u; va=0x%llx; size=0x%llx; user_devid=%u)\n", devid, va, size, user_devid);
    (void)svm_urma_unregister_seg(user_devid, &dst_va, 0);
}

static int svm_urma_seg_init_single_device(u32 devid, u64 va, u64 size)
{
    u32 host_devid = svm_get_host_devid();
    int ret;

    ret = svm_urma_seg_init(devid, va, size, host_devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return 0;
}

static void svm_urma_seg_uninit_single_device(u32 devid, u64 va, u64 size)
{
    u32 host_devid = svm_get_host_devid();

    svm_urma_seg_uninit(devid, va, size, host_devid);
}

static void svm_urma_seg_uninit_devices(u64 va, u64 size, u32 max_devid)
{
    u32 devid;

    for (devid = 0; devid < max_devid; devid++) {
        if (!svm_urma_id_dev_valid(devid)) {
            continue;
        }

        svm_urma_seg_uninit_single_device(devid, va, size);
    }
}

static int svm_urma_seg_init_devices(u64 va, u64 size, u32 max_devid)
{
    u32 devid;
    int ret;

    for (devid = 0; devid < max_devid; devid++) {
        if (!svm_urma_id_dev_valid(devid)) {
            continue;
        }

        ret = svm_urma_seg_init_single_device(devid, va, size);
        if (ret != DRV_ERROR_NONE) {
            svm_urma_seg_uninit_devices(va, size, devid);
            return ret;
        }
    }

    return 0;
}

static int svm_urma_va_reserve(u64 va, u64 size)
{
    return svm_urma_seg_init_devices(va, size, SVM_MAX_AGENT_NUM);
}

static int svm_urma_va_release(u64 va, u64 size)
{
    svm_urma_seg_uninit_devices(va, size, SVM_MAX_AGENT_NUM);
    return 0;
}

static int svm_urma_dev_init_va_reserve(u64 va, u64 size, void *priv)
{
    u32 devid = (u32)(uintptr_t)priv;
    return svm_urma_seg_init_single_device(devid, va, size);
}

static int svm_urma_dev_uninit_va_release(u64 va, u64 size, void *priv)
{
    u32 devid = (u32)(uintptr_t)priv;
    svm_urma_seg_uninit_single_device(devid, va, size);
    return 0;
}

static int _svm_urma_master_dev_init(u32 devid)
{
    int ret;
    u64 start, end_ub, end_va;
    u32 hd_connect_type = svm_get_device_connect_type(devid);

    start = svm_get_timestamp_ns();

    if (hd_connect_type == HOST_DEVICE_CONNECT_TYPE_UB) {
#ifdef CFG_FEATURE_SUPPORT_UB
        ret = svm_urma_chan_init(devid);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
#endif

        ret = svm_urma_seg_sp_init(devid);
        if (ret != DRV_ERROR_NONE) {
#ifdef CFG_FEATURE_SUPPORT_UB
            svm_urma_chan_uninit(devid);
#endif
            return ret;
        }
    }

    end_ub = svm_get_timestamp_ns();

    ret = va_reserve_for_each_node(svm_urma_dev_init_va_reserve, (void *)(uintptr_t)devid);
    if (ret != 0) {
        if (hd_connect_type == HOST_DEVICE_CONNECT_TYPE_UB) {
            svm_urma_seg_sp_uninit(devid);
#ifdef CFG_FEATURE_SUPPORT_UB
            svm_urma_chan_uninit(devid);
#endif
        }
        return ret;
    }

    svm_urma_dev_valid[devid] = 1;

    end_va = svm_get_timestamp_ns();

    svm_run_info("Urma master dev init ok. (devid=%u; ub init=%llu ns; va reserve=%llu ns)\n", devid, end_ub - start,
                 end_va - end_ub);

    return 0;
}

static int _svm_urma_master_dev_uninit(u32 devid)
{
    u32 hd_connect_type = svm_get_device_connect_type(devid);
    if (hd_connect_type == HOST_DEVICE_CONNECT_TYPE_UB) {
        svm_urma_seg_sp_uninit(devid);
#ifdef CFG_FEATURE_SUPPORT_UB
        svm_urma_chan_uninit(devid);
#endif
    }

    svm_urma_dev_valid[devid] = 0;

    (void)va_reserve_for_each_node(svm_urma_dev_uninit_va_release, (void *)(uintptr_t)devid);

    return 0;
}

int svm_urma_master_dev_init(u32 devid)
{
    int ret = 0;

    if ((devid < SVM_MAX_DEV_AGENT_NUM) && !svm_urma_id_dev_valid(devid)) {
        ret = _svm_urma_master_dev_init(devid);
    }

    return ret;
}

int svm_urma_master_dev_uninit(u32 devid)
{
    int ret = 0;

    if ((devid < SVM_MAX_DEV_AGENT_NUM) && svm_urma_id_dev_valid(devid)) {
        ret = _svm_urma_master_dev_uninit(devid);
    }

    return ret;
}

static int svm_urma_criu_reset(u32 devid, void *data)
{
    SVM_UNUSED(data);
    return svm_urma_master_dev_uninit(devid);
}

static const struct svm_criu_ops g_urma_adapt_criu_ops = {
    .name = "urma_adapt",
    .reset = svm_urma_criu_reset,
};

void __attribute__((constructor)) svm_urma_adapt_init(void)
{
    int ret;

    ret = svm_register_ioctl_dev_init_post_handle(svm_urma_master_dev_init);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev init post handle failed.\n");
    }

    ret = svm_register_ioctl_dev_uninit_pre_handle(svm_urma_master_dev_uninit);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev uninit pre handle failed.\n");
    }

    ret = svm_criu_register_ops(&g_urma_adapt_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed.\n");
    }

    ret = svm_register_va_reserve_post_handle(svm_urma_va_reserve);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register va reserve post handle failed.\n");
    }

    ret = svm_register_va_release_pre_handle(svm_urma_va_release);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register va release pre handle failed.\n");
    }
}
