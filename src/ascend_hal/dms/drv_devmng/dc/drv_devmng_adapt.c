/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "drv_devmng_adapt.h"
#include "devmng_common.h"
#include "ascend_hal_error.h"
#include "devmng_cmd_def.h"

drvError_t drv_get_info_type_version_adapt(uint32_t devId, int32_t info_type, int64_t *value)
{
    if (value == NULL) {
        DEVDRV_DRV_ERR("Input value is null. (dev_id=%u)\n", devId);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return drv_get_info_from_dev_info(devId, info_type, value);
}

drvError_t drvGetPlatformInfo(uint32_t *info)
{
    mmIoctlBuf para_buf = {0};
    uint32_t para;
    int ret;

    if (info == NULL) {
        DEVDRV_DRV_ERR("info is NULL.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    drv_ioctl_param_init(&para_buf, (void *)&para, sizeof(uint32_t));
    ret = drv_common_ioctl(&para_buf, DEVDRV_MANAGER_GET_PLATINFO);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, ret = %d.\n", ret);
        return ret;
    }

    if (para > DEVDRV_MANAGER_HOST_ENV) {
        DEVDRV_DRV_ERR("wrong platform type = %u, max = %u.\n", para, DEVDRV_MANAGER_HOST_ENV);
        return DRV_ERROR_INVALID_DEVICE;
    }

    *info = para;

    return DRV_ERROR_NONE;
}