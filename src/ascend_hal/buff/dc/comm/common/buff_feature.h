/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef BUFF_FEATURE_H
#define BUFF_FEATURE_H

#include <stdbool.h>

#include "ascend_hal_define.h"
#include "buff_ioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

drvError_t buff_dev_feature_init_all(void);
bool buff_has_dev_feature(unsigned int dev_id, unsigned int feature_id);

/*
 * Version-package feature interface.
 * Query whether a drvFeature_t is supported by the current driver version package.
 * Mapping entries are defined in buff_feature.c (g_buff_feature_map[]).
 */
bool buff_has_ver_feature(unsigned int dev_id, drvFeature_t feature);

#ifdef __cplusplus
}
#endif
#endif
