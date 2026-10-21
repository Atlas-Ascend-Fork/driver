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

#include "malloc_mng.h"
#include "svm_sub_event_type_uk_msg.h"
#include "pagefault_uk_msg.h"
#include "svm_umc_server.h"
#include "svm_dbi.h"
#include "svm_log.h"
#include "svm_pagefault_info.h"

#define SVM_PAGEFAULT_MAX_MAP_PAGE_NUM 8
static int svm_pagefault_handle(u32 devid, u64 va)
{
    struct svm_prop prop;
    u64 size, remain_size;
    int ret;

    if (devid == svm_get_host_devid()) {
        svm_run_info_ratelimited("Not support host pagefault. (va=0x%llx; host_devid=%u)\n", va, devid);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        svm_run_info_ratelimited("Get prop not success. (va=0x%llx)\n", va);
        return ret;
    }

    if (prop.devid == devid) {
        svm_run_info_ratelimited("Pagefault va is belong to device. (va=0x%llx; devid=%u)\n", va, devid);
        return DRV_ERROR_PARA_ERROR;
    } else if (prop.devid == svm_get_host_devid()) {
        svm_run_info_ratelimited("Not support device pagefault access host mem. (va=0x%llx; host_devid=%u)\n", va,
                                 devid);
        return DRV_ERROR_PARA_ERROR;
    } else if (prop.devid >= SVM_MAX_DEV_AGENT_NUM) {
        svm_run_info_ratelimited("Only support device va. (va=0x%llx; invalid_devid=%u)\n", va, prop.devid);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = svm_dbi_query_npage_size(prop.devid, &size);
    if (ret != 0) {
        svm_err("Get page size failed. (va=0x%llx; prop.devid=%u; devid=%u)\n", va, prop.devid, devid);
        return ret;
    }

    size *= SVM_PAGEFAULT_MAX_MAP_PAGE_NUM;
    remain_size = prop.size - (va - prop.start);
    size = (size < remain_size) ? size : remain_size;

    ret = drvMemPrefetchToDevice((DVdeviceptr)va, (size_t)size, (DVdevice)devid);
    if (ret != 0) {
        svm_err("Prefetch to device failed. (va=0x%llx; size=0x%llx; prop.devid=%u; devid=%u; ret=%d)\n", va, size,
                prop.devid, devid, ret);
    }

    return ret;
}

static int svm_pagefault_proc_func(u32 devid, const void *msg_in, void *msg_out)
{
    const struct svm_pagefault_msg *pagefault_msg = (const struct svm_pagefault_msg *)msg_in;
    struct svm_pagefault_msg *pagefault_msg_out = (struct svm_pagefault_msg *)msg_out;

    *pagefault_msg_out = *pagefault_msg;
    pagefault_msg_out->result = svm_pagefault_handle(devid, pagefault_msg->va);

    if (pagefault_msg_out->result != 0) {
        svm_pagefault_info_record_access(devid, pagefault_msg->va);
    }

    return 0;
}

SVM_EVENT_PROC_REGISTER(SVM_PAGEFAULT_EVENT, svm_pagefault_proc_func, (u64)sizeof(struct svm_pagefault_msg),
                        (u64)sizeof(struct svm_pagefault_msg));
