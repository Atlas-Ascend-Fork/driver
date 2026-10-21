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

#include "comm_user_interface.h"

#include "svm_pub.h"
#include "svm_criu.h"
#include "svm_log.h"
#include "svm_init_pri.h"
#include "svm_dbi.h"
#include "svm_sys_cmd.h"
#include "svm_urma_to_ascend_flag.h"
#include "svm_urma_seg_local.h"
#include "svm_urma_chan.h"
#include "va_allocator.h"

#ifdef DRV_HOST
#define SVM_URMA_TOKEN_DEFAULT_NUM 64u
#define SVM_URMA_MAX_ACQUIRED_NUM_PER_TOKEN 512u
#else
#define SVM_URMA_TOKEN_DEFAULT_NUM 2u
#define SVM_URMA_MAX_ACQUIRED_NUM_PER_TOKEN 1024u
#endif
#define SVM_URMA_TOKEN_CACHE_UP_THRES_NUM 128u

static void *g_seg_mng[SVM_MAX_DEV_AGENT_NUM] = {NULL};
static void *g_self_user_seg_mng[SVM_MAX_DEV_AGENT_NUM] = {NULL};
static bool g_is_host_side_called = false;
static bool g_is_host_side = false;
static u32 g_host_user_devid = SVM_MAX_DEV_AGENT_NUM;
static bool g_host_user_devid_called = false;

static void svm_urma_seg_local_reset_state(void)
{
    g_is_host_side_called = false;
    g_is_host_side = false;
    g_host_user_devid = SVM_MAX_DEV_AGENT_NUM;
    g_host_user_devid_called = false;
}

static bool svm_is_in_host_side(void)
{
    u32 side = 0;

    if (!g_is_host_side_called) {
        int ret = drvGetPlatformInfo(&side);
        if (ret != 0) {
            svm_warn("Get side info failed. (ret=%d)\n", ret);
        }
        g_is_host_side = (side != 0);
        g_is_host_side_called = true;
    }

    return g_is_host_side;
}

static u32 svm_urma_seg_get_user_devid(u32 devid)
{
    u32 dev_num, i;

    if (svm_is_in_host_side() == false) {
        return devid;
    }

    (void)drvGetDevNum(&dev_num);
    if (!g_host_user_devid_called) {
        for (i = 0; i < dev_num; ++i) {
            if (ascend_urma_dev_is_exist(i)) {
                g_host_user_devid = i;
                break;
            }
        }
    }
    g_host_user_devid_called = true;
    return g_host_user_devid;
}

static void *svm_get_urma_seg_mng(u32 devid, u32 seg_flag)
{
    if (svm_urma_seg_flag_is_self_user(seg_flag)) {
        return g_self_user_seg_mng[devid];
    } else {
        return g_seg_mng[devid];
    }
}

static bool svm_urma_seg_local_is_dev_inited(u32 devid)
{
    return ((g_seg_mng[devid] != NULL) && (g_self_user_seg_mng[devid]));
}

static int _svm_urma_seg_local_dev_init(u32 devid)
{
    struct ascend_urma_seg_mng_attr attr = {.token_num_default = SVM_URMA_TOKEN_DEFAULT_NUM,
                                            .token_num_cache_up_thres = SVM_URMA_TOKEN_CACHE_UP_THRES_NUM,
                                            .max_seg_num_per_token = SVM_URMA_MAX_ACQUIRED_NUM_PER_TOKEN};
    void *seg_mng = NULL;
    void *slef_user_seg_mng = NULL;

#ifdef DRV_HOST
    attr.acquire_token = svm_acquire_token;
#endif
    seg_mng = ascend_urma_seg_mng_create(devid, &attr);
    if (seg_mng == NULL) {
        svm_err("Create urma seg mng failed. (devid=%d)\n", devid);
        return DRV_ERROR_INNER_ERR;
    }

    slef_user_seg_mng = ascend_urma_seg_mng_create(devid, &attr);
    if (slef_user_seg_mng == NULL) {
        svm_err("Create self user seg mng failed. (devid=%d)\n", devid);
        ascend_urma_seg_mng_destroy(seg_mng);
        return DRV_ERROR_INNER_ERR;
    }

    g_seg_mng[devid] = seg_mng;
    g_self_user_seg_mng[devid] = slef_user_seg_mng;
    return DRV_ERROR_NONE;
}

static void _svm_urma_seg_local_dev_uninit(u32 devid)
{
    ascend_urma_seg_mng_destroy(g_self_user_seg_mng[devid]);
    ascend_urma_seg_mng_destroy(g_seg_mng[devid]);
    g_self_user_seg_mng[devid] = NULL;
    g_seg_mng[devid] = NULL;
}

int svm_urma_seg_local_register(u32 devid, u64 start, u64 size, u32 seg_flag)
{
    u32 user_devid = svm_urma_seg_get_user_devid(devid);

    if (user_devid >= SVM_MAX_DEV_AGENT_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", user_devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!svm_urma_seg_local_is_dev_inited(user_devid)) {
        svm_err("Uninit. (devid=%u)\n", user_devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return ascend_urma_register_seg(svm_get_urma_seg_mng(user_devid, seg_flag), start, size,
                                    svm_urma_to_ascend_seg_flag(seg_flag));
}

int svm_urma_seg_local_unregister(u32 devid, u64 start, u64 size, u32 seg_flag)
{
    u32 user_devid = svm_urma_seg_get_user_devid(devid);

    if (user_devid >= SVM_MAX_DEV_AGENT_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", user_devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!svm_urma_seg_local_is_dev_inited(user_devid)) {
        svm_err("Uninit. (devid=%u)\n", user_devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return ascend_urma_unregister_seg(svm_get_urma_seg_mng(user_devid, seg_flag), start, size);
}

static inline u32 ascend_to_svm_urma_seg_flag(u32 ascend_flag)
{
    u32 svm_urma_seg_flag = 0;

    svm_urma_seg_flag |= ((ascend_flag & ASCEND_URMA_SEG_FLAG_ACCESS_WRITE) != 0) ? SVM_URMA_SEG_FLAG_ACCESS_WRITE : 0;
    svm_urma_seg_flag |= ((ascend_flag & ASCEND_URMA_SEG_FLAG_PIN) != 0) ? SVM_URMA_SEG_FLAG_PIN : 0;
    svm_urma_seg_flag |= ((ascend_flag & ASCEND_URMA_SEG_FLAG_WITHOUT_TOKEN_VAL) != 0) ? SVM_URMA_SEG_FLAG_SELF_USER :
                                                                                         0;

    return svm_urma_seg_flag;
}

static void ascend_to_svm_urma_seg_info(struct ascend_urma_seg_info *ascend_info, struct svm_urma_seg_info *svm_info)
{
    svm_info->start = ascend_info->start;
    svm_info->size = ascend_info->size;
    svm_info->token_id = ascend_info->token_id;
    svm_info->token_val = ascend_info->token_val;
    svm_info->seg_flag = ascend_to_svm_urma_seg_flag(ascend_info->seg_flag);
    svm_info->tseg = ascend_info->tseg;
}

int svm_urma_seg_local_get_info(u32 devid, u64 va, u32 seg_flag, struct svm_urma_seg_info *seg_info)
{
    u32 user_devid = svm_urma_seg_get_user_devid(devid);
    struct ascend_urma_seg_info info;
    int ret;

    if (user_devid >= SVM_MAX_DEV_AGENT_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", user_devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!svm_urma_seg_local_is_dev_inited(user_devid)) {
        svm_err("Uninit. (devid=%u)\n", user_devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = ascend_urma_get_seg_info(svm_get_urma_seg_mng(user_devid, seg_flag), va, &info);
    if (ret == DRV_ERROR_NONE) {
        ascend_to_svm_urma_seg_info(&info, seg_info);
    }

    return ret;
}

int svm_urma_seg_local_dev_init(u32 devid)
{
    u32 user_devid = svm_urma_seg_get_user_devid(devid);

    if ((devid == svm_get_host_devid()) || !ascend_urma_dev_is_exist(user_devid) ||
        svm_urma_seg_local_is_dev_inited(user_devid)) {
        return DRV_ERROR_NONE;
    }

    return _svm_urma_seg_local_dev_init(user_devid);
}

int svm_urma_seg_local_dev_uninit(u32 devid)
{
    u32 user_devid = svm_urma_seg_get_user_devid(devid);

    if ((devid == svm_get_host_devid()) || !ascend_urma_dev_is_exist(user_devid) ||
        (svm_is_in_host_side() && (devid != svm_get_host_devid()))) {
        return DRV_ERROR_NONE;
    }

    if (svm_urma_seg_local_is_dev_inited(user_devid)) {
        _svm_urma_seg_local_dev_uninit(user_devid);
    }

    return DRV_ERROR_NONE;
}

static int svm_urma_seg_local_criu_reset(u32 devid, void *data)
{
    SVM_UNUSED(data);
    svm_urma_seg_local_reset_state();
    if (devid < SVM_MAX_DEV_AGENT_NUM && svm_urma_seg_local_is_dev_inited(devid)) {
        _svm_urma_seg_local_dev_uninit(devid);
    }
    return DRV_ERROR_NONE;
}

static const struct svm_criu_ops g_urma_seg_local_criu_ops = {
    .name = "urma_seg_local",
    .reset = svm_urma_seg_local_criu_reset,
};

static void __attribute__((constructor(SVM_INIT_PRI_LOW))) svm_urma_seg_local_init(void)
{
    int ret;

    ret = svm_register_ioctl_dev_init_post_handle(svm_urma_seg_local_dev_init);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev init post handle failed.\n");
    }

    ret = svm_register_ioctl_dev_uninit_pre_handle(svm_urma_seg_local_dev_uninit);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev uninit pre handle failed.\n");
    }

    ret = svm_criu_register_ops(&g_urma_seg_local_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed.\n");
    }
}
