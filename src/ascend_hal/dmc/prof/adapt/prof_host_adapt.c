/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ascend_hal.h"
#include "prof_adapt.h"

drvError_t prof_adapt_check_cc_mode(unsigned int dev_id)
{
    (void)dev_id;
    return DRV_ERROR_NONE;
}

bool prof_is_v4_extra_channel(unsigned int chan_id)
{
    (void)chan_id;
    return false;
}

bool prof_is_support_host_sample(void)
{
    return false;
}
