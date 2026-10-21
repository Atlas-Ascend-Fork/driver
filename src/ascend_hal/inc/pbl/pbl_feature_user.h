/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef PBL_FEATURE_USER_H
#define PBL_FEATURE_USER_H

#include <stdbool.h>
#include "ascend_hal_external.h"

DLLEXPORT int drv_query_module_features(unsigned int dev_id, const char *module_name, unsigned long long *bitmap,
                                        unsigned int *bitmap_len);

bool pbl_support_feature(unsigned int dev_id, unsigned int feature_id);

#endif /* PBL_FEATURE_USER_H */
