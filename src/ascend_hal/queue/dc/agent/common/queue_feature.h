/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef QUEUE_FEATURE_H
#define QUEUE_FEATURE_H

#include "ascend_hal_define.h"
#include "queue_ioctl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool queue_has_dev_feature(unsigned int dev_id, unsigned int feature_id);

/*
 * Version-package feature interface.
 * Query whether a drvFeature_t is supported by the current driver version package.
 * Mapping entries are defined in queue_feature.c (g_queue_feature_map[]).
 */
bool queue_has_ver_feature(unsigned int dev_id, drvFeature_t feature);

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_FEATURE_H */
