/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <errno.h>
#include "ascend_hal_error.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

int halGetUserConfig(unsigned int devId, const char *name, unsigned char *buf, unsigned int *bufSize)
{
    (void)devId;
    (void)name;
    (void)buf;
    (void)bufSize;
    return DRV_ERROR_NOT_SUPPORT;
}

int halSetUserConfig(unsigned int devId, const char *name, unsigned char *buf, unsigned int bufSize)
{
    (void)devId;
    (void)name;
    (void)buf;
    (void)bufSize;
    return DRV_ERROR_NOT_SUPPORT;
}

int halClearUserConfig(unsigned int devId, const char *name)
{
    (void)devId;
    (void)name;
    return DRV_ERROR_NOT_SUPPORT;
}

int devdrv_get_user_config(unsigned int devid, const char *name, unsigned char *buf, unsigned int *buf_size)
{
    return halGetUserConfig(devid, name, buf, buf_size);
}

int devdrv_set_user_config(unsigned int devid, const char *name, unsigned char *buf, unsigned int buf_size)
{
    return halSetUserConfig(devid, name, buf, buf_size);
}

int devdrv_clear_user_config(unsigned int devid, const char *name)
{
    return halClearUserConfig(devid, name);
}
