/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCEND_URMA_ADAPT_H
#define ASCEND_URMA_ADAPT_H

#include "urma_types.h"
#include "urma_api.h"

#include "drv_user_common.h"
#include "ascend_urma_pub.h"

struct ascend_urma_cxt_info {
    struct list_head node;
    urma_device_t *urma_dev;
    u32 eid_index;
    urma_context_t *urma_ctx;
    u32 cnt;
};

struct ascend_tp_info {
    struct list_head node;
    urma_eid_t local_eid;
    urma_eid_t remote_eid;
    urma_tp_info_t tpid_info;
    bool ctp_used_flag;
    unsigned int cnt;
};

enum ascend_remote_res_type {
    ASCEND_RES_JFR = 0,
    ASCEND_RES_JETTY,
    ASCEND_RES_MAX
};

struct ascend_remote_res_info {
    u32 remote_res_type;
    void *remote_res;
};

void ascend_urma_adapt_release(void);

#endif
