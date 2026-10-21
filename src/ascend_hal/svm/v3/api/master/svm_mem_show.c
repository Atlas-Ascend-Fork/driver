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

#include "ascend_hal.h"
#include "securec.h"
#include "esched_user_interface.h"

#include "svm_log.h"
#include "svm_user_adapt.h"
#include "svm_sys_cmd.h"
#include "svm_ioctl_ex.h"
#include "svm_sub_event_type_uk_msg.h"
#include "mem_show_uk_msg.h"
#include "malloc_mng.h"
#include "cache_malloc.h"
#include "svm_prefetch.h"
#include "svm_register_pcie_th.h"
#include "svm_pipeline.h"
#include "svm_mem_show_cfg.h"

struct svm_mem_show_feature {
    const char *feature_name;
    void (*show_func)(u32 devid, char *buf, u32 buf_len);
};

static void svm_show_mem_info(u32 devid, char *buf, u32 buf_len)
{
    struct MemInfo info = {0};
    DVresult ret;

    if ((devid == SVM_INVALID_DEVID) || (buf_len == 0)) {
        return;
    }

    ret = halMemGetInfo(devid, MEM_INFO_TYPE_DDR_SIZE, &info);
    if (ret != 0) {
        (void)snprintf_s(buf, buf_len, buf_len - 1, "get info failed devid %d\n", devid);
        return;
    }

    (void)snprintf_s(buf, buf_len, buf_len - 1, "ddr info: total %lu free %lu huge_total %lu huge_free %lu\n",
                     info.phy_info.total, info.phy_info.free, info.phy_info.huge_total, info.phy_info.huge_free);
}

void svm_show_register(u32 devid, char *buf, u32 buf_len)
{
    (void)svm_show_register_pcie_th(devid, buf, buf_len);
}

static struct svm_mem_show_feature show_features[] = {
    {USER_FEATURE_MALLOC_MNG, svm_show_dev_mem}, {USER_FEATURE_CACHE_MALLOC, svm_show_cache},
    {USER_FEATURE_PREFETCH, svm_show_prefetch},  {USER_FEATURE_REGISTER, svm_show_register},
    {USER_FEATURE_MEM_STAT, svm_show_mem_info},
};

static u32 show_feature_num = USER_FEATURE_SHOW_NUM;

static char show_buf[SVM_MEM_SHOW_BUF_LEN];
static struct svm_mem_show_buf_head *show_buf_head = (struct svm_mem_show_buf_head *)show_buf;

static void svm_mem_show(u32 devid, struct svm_mem_show_feature *feature)
{
    char *data = show_buf_head->data;

    data[0] = '\0';
    svm_use_pipeline();
    feature->show_func(devid, show_buf_head->data, SVM_MEM_SHOW_BUF_DATA_LEN);
    svm_unuse_pipeline();
    data[SVM_MEM_SHOW_BUF_DATA_LEN - 1] = '\0';
    show_buf_head->valid = 1;
}

static struct svm_mem_show_feature *svm_show_get_feature(char *feature_name)
{
    u32 i;

    for (i = 0; i < show_feature_num; i++) {
        if (strcmp(feature_name, show_features[i].feature_name) == 0) {
            return &show_features[i];
        }
    }

    return NULL;
}

static drvError_t svm_mem_show_event_proc_func(unsigned int devid, const void *msg, int msg_len,
                                               struct drv_event_proc_rsp *rsp)
{
    const struct svm_mem_show_msg *show_msg = (const struct svm_mem_show_msg *)msg;
    struct svm_mem_show_feature *feature = NULL;
    char feature_name[FEARURE_NAME_LEN];
    int ret;
    SVM_UNUSED(msg_len);

    (void)strcpy_s(feature_name, FEARURE_NAME_LEN, show_msg->feature_name);
    feature_name[FEARURE_NAME_LEN - 1U] = '\0';

    feature = svm_show_get_feature(feature_name);
    if (feature == NULL) {
        svm_err("Invalid feature name. (devid=%u; feature_name=%s)\n", devid, feature_name);
    } else {
        u32 show_devid = (show_msg->type == SVM_SHOW_SCOPE_DEV) ? devid : SVM_INVALID_DEVID;
        svm_mem_show(show_devid, feature);
    }

    ret = svm_show_mem_ack(devid, show_buf);
    rsp->need_rsp = false;
    return ret;
}

static struct drv_event_proc svm_mem_show_event_proc = {svm_mem_show_event_proc_func, sizeof(struct svm_mem_show_msg),
                                                        "svm_mem_show_event"};

static int __attribute__((constructor)) svm_mem_show_init(void)
{
    drv_registert_event_proc(SVM_MEM_SHOW_EVENT, &svm_mem_show_event_proc);
    return 0;
}
