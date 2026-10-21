/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/ioctl.h>
#include <stdio.h>
#include <pthread.h>

#include "svm_init_pri.h"
#include "svm_sys_cmd.h"
#include "svm_register_to_master.h"
#include "svm_pub.h"
#include "svm_log.h"
#include "svm_dbi.h"
#include "svm_apbi.h"
#include "va_allocator.h"
#include "normal_malloc.h"
#include "svm_sub_event_type_uk_msg.h"
#include "svm_umc_client.h"
#include "notice_gap_uk_msg.h"
#include "svm_criu.h"

#define DEFAULT_ASSIGN_GAP_SIZE (2ULL * SVM_BYTES_PER_MB)

static u64 g_svm_host_gap_va = 0ULL;
static u64 g_svm_host_gap_size = 0ULL;
static int g_svm_host_gap_init_flag[SVM_MAX_DEV_NUM] = {
    0,
};

static int assign_alloc_host_gap_va(u64 size, u64 *host_gap_va)
{
    u64 start = 0;
    u32 flag = 0;
    u64 align = size;
    int ret;

    flag |= SVM_NORMAL_MALLOC_FLAG_VA_WITH_MASTER;

    ret = svm_normal_malloc(svm_get_host_devid(), flag, align, &start, size);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Alloc host gap va failed. (ret=%d; devid=%u)\n", ret, svm_get_host_devid());
        return ret;
    }

    *host_gap_va = start;
    return 0;
}

static void assign_free_host_gap_va(u64 host_gap_va, u64 size)
{
    u32 flag = 0;
    u64 align = size;

    flag |= SVM_NORMAL_MALLOC_FLAG_VA_WITH_MASTER;
    (void)svm_normal_free(svm_get_host_devid(), flag, align, host_gap_va, size);
}

static int assign_get_host_gap_va(u32 devid, u64 *host_gap_va, u64 *host_gap_size)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    u64 npage_size;
    int ret = 0;

    if (g_svm_host_gap_va != 0ULL) {
        *host_gap_va = g_svm_host_gap_va;
        *host_gap_size = g_svm_host_gap_size;
        return 0;
    }

    ret = svm_dbi_query_host_align_page_size(devid, &npage_size);
    if (ret != 0) {
        svm_err("Get page size failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    (void)pthread_mutex_lock(&mutex);
    if (g_svm_host_gap_va == 0ULL) {
        ret = assign_alloc_host_gap_va(npage_size, &g_svm_host_gap_va);
        g_svm_host_gap_size = npage_size;
    }

    if (g_svm_host_gap_va != 0ULL) {
        *host_gap_va = g_svm_host_gap_va;
        *host_gap_size = g_svm_host_gap_size;
    }

    (void)pthread_mutex_unlock(&mutex);

    return ret;
}

static int assign_notice_host_gap_va(u32 devid, u64 host_gap_va, u64 size, u32 op)
{
    struct svm_umc_msg_head head;
    struct svm_notice_gap_msg notice_gap_msg_msg = {.host_gap_va = host_gap_va, .size = size, .op = op};
    struct svm_umc_msg msg = {.msg_in = (char *)(uintptr_t)&notice_gap_msg_msg,
                              .msg_in_len = sizeof(struct svm_notice_gap_msg),
                              .msg_out = NULL,
                              .msg_out_len = 0};

    struct svm_apbi apbi;
    int ret;

    ret = svm_apbi_query(devid, DEVDRV_PROCESS_CP1, &apbi);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_umc_msg_head_pack(devid, apbi.tgid, apbi.grp_id, SVM_NOTICE_GAP_VA_EVENT, &head);
    ret = svm_umc_h2d_send(&head, &msg);
    if (ret == DRV_ERROR_NO_PROCESS) {
        (void)svm_apbi_clear(devid, DEVDRV_PROCESS_CP1);
    }

    return ret;
}

static int assign_setup_host_gap_va(u32 devid)
{
    struct svm_dst_va register_va;
    u64 host_gap_va, host_gap_size;
    int ret;

    ret = assign_get_host_gap_va(devid, &host_gap_va, &host_gap_size);
    if (ret != 0) {
        return ret;
    }

    svm_dst_va_pack(svm_get_host_devid(), 0, host_gap_va, host_gap_size, &register_va);
    ret = svm_register_to_master(devid, &register_va, 0);
    if (ret != 0) {
        svm_err("Register to master failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    ret = assign_notice_host_gap_va(devid, host_gap_va, host_gap_size, 1);
    if (ret != 0) {
        (void)svm_unregister_to_master(devid, &register_va, 0);
        svm_err("Notice failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    return 0;
}

static void assign_clear_host_gap_va(u32 devid)
{
    struct svm_dst_va register_va;
    int ret;

    ret = assign_notice_host_gap_va(devid, g_svm_host_gap_va, g_svm_host_gap_size, 0);
    if (ret != 0) {
        svm_err("Notice failed. (ret=%d; devid=%u)\n", ret, devid);
    }

    svm_dst_va_pack(svm_get_host_devid(), 0, g_svm_host_gap_va, g_svm_host_gap_size, &register_va);
    ret = svm_unregister_to_master(devid, &register_va, 0);
    if (ret != 0) {
        svm_err("Unregister to master failed. (ret=%d; devid=%u)\n", ret, devid);
    }
}

static int assign_init_host_gap_va(u32 devid)
{
    int ret;

    if (svm_get_device_connect_type(devid) != HOST_DEVICE_CONNECT_TYPE_PCIE) {
        return 0;
    }

    if (g_svm_host_gap_init_flag[devid] == 1) {
        return 0;
    }

    ret = assign_setup_host_gap_va(devid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Setup host gap va failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    g_svm_host_gap_init_flag[devid] = 1;
    return 0;
}

static void assign_uninit_host_gap_va(u32 devid)
{
    if (g_svm_host_gap_init_flag[devid] == 1) {
        assign_clear_host_gap_va(devid);
        g_svm_host_gap_init_flag[devid] = 0;
    }

    if (devid == svm_get_host_devid()) {
        if (g_svm_host_gap_va != 0ULL) {
            assign_free_host_gap_va(g_svm_host_gap_va, g_svm_host_gap_size);
            g_svm_host_gap_va = 0ULL;
        }
    }
}

static int assign_gap_dev_init(u32 devid)
{
    int ret;

    if (!svm_dbi_is_support_assign_gap(devid)) {
        return 0;
    }

    svm_va_set_gap(DEFAULT_ASSIGN_GAP_SIZE);

    ret = assign_init_host_gap_va(devid);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

static int assign_gap_dev_uninit(u32 devid)
{
    assign_uninit_host_gap_va(devid);
    return 0;
}

static int svm_assign_gap_criu_reset(u32 devid, void *data)
{
    SVM_UNUSED(data);
    assign_uninit_host_gap_va(devid);
    return 0;
}

static const struct svm_criu_ops g_assign_gap_criu_ops = {
    .name = "assign_gap",
    .reset = svm_assign_gap_criu_reset,
};

void __attribute__((constructor(SVM_INIT_PRI_PRE_HIGH))) assign_gap_init(void)
{
    int ret;

    ret = svm_register_ioctl_dev_init_post_handle(assign_gap_dev_init);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev init post handle failed.\n");
    }

    ret = svm_register_ioctl_dev_uninit_pre_handle(assign_gap_dev_uninit);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register ioctl dev uninit post handle failed.\n");
    }

    ret = svm_criu_register_ops(&g_assign_gap_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed. (ret=%d)\n", ret);
    }
}
