/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ascend_hal_error.h"
#include "udis_user.h"

int __attribute__((weak)) udis_get_device_info(unsigned int dev_id, struct udis_dev_info *info)
{
    (void)dev_id;
    (void)info;
    return DRV_ERROR_NOT_SUPPORT;
}

int __attribute__((weak)) udis_set_device_info(unsigned int dev_id, const struct udis_dev_info *info)
{
    (void)dev_id;
    (void)info;
    return DRV_ERROR_NOT_SUPPORT;
}

int __attribute__((weak)) halUdisGetDeviceInfo(unsigned int dev_id, void *info)
{
    (void)dev_id;
    (void)info;
    return DRV_ERROR_NOT_SUPPORT;
}