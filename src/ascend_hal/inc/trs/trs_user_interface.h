/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef __TRS_USER_INTERFACE_H__
#define __TRS_USER_INTERFACE_H__

#include "drv_type.h"
#include "ascend_hal.h"

enum trs_sq_send_mode {
    TRS_MODE_TYPE_SQ_SEND_HIGH_SECURITY = 0x0, /* sq use uik mode */
    TRS_MODE_TYPE_SQ_SEND_HIGH_PERFORMANCE,    /* sq use uio mode */
    TRS_MODE_TYPE_SQ_SEND_MAX
};

typedef enum tag_trs_mode_type {
    TRS_MODE_TYPE_SQ_SEND = 0,
    TRS_MODE_TYPE_MAX
} trs_mode_type_t;

struct trs_mode_info {
    uint32_t dev_id;
    uint32_t ts_id;
    trs_mode_type_t mode_type;
    int mode;
};

drvError_t trs_mode_config(struct trs_mode_info *info);
drvError_t trs_mode_query(struct trs_mode_info *info);

#ifdef CFB_BUILD_DEBUG
DLLEXPORT bool trs_feature_is_support(uint32_t dev_id, drvFeature_t type);
#else
bool trs_feature_is_support(uint32_t dev_id, drvFeature_t type);
#endif

#endif /* __TRS_USER_INTERFACE_H__ */
