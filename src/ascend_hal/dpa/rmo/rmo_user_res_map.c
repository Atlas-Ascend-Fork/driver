/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/mman.h>

#include "ascend_hal_define.h"
#include "rmo.h"

int rmo_res_info_check(struct res_map_info *res_info)
{
    int i;

    if (res_info == NULL) {
        rmo_err("Null ptr\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (res_info->target_proc_type != PROCESS_CP1) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (res_info->flag != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    for (i = 0; i < RES_MAP_INFO_RSV_LEN; i++) {
        if (res_info->rsv[i] != 0) {
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    return DRV_ERROR_NONE;
}

drvError_t halResMap(unsigned int devId, struct res_map_info *res_info, unsigned long *va, unsigned int *len)
{
    int ret;

    if ((va == NULL) || (len == NULL)) {
        rmo_err("Null ptr\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = rmo_res_info_check(res_info);
    if (ret != 0) {
        return ret;
    }

    return dpa_res_map(devId, res_info, va, len);
}

drvError_t halResUnmap(unsigned int devId, struct res_map_info *res_info)
{
    int ret;

    ret = rmo_res_info_check(res_info);
    if (ret != 0) {
        return ret;
    }

    return dpa_res_unmap(devId, res_info);
}

drvError_t halResRead(unsigned int devId, struct res_map_info *res_info, void *data, unsigned int len)
{
    int ret;

    if ((data == NULL) || (len == 0)) {
        rmo_err("Null ptr or invalid len. (len=%u)\n", len);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = rmo_res_info_check(res_info);
    if (ret != 0) {
        return ret;
    }

    return dpa_res_read(devId, res_info, data, len);
}

drvError_t halResWrite(unsigned int devId, struct res_map_info *res_info, void *data, unsigned int len)
{
    int ret;

    if ((data == NULL) || (len == 0)) {
        rmo_err("Null ptr or invalid len. (len=%u)\n", len);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = rmo_res_info_check(res_info);
    if (ret != 0) {
        return ret;
    }

    return dpa_res_write(devId, res_info, data, len);
}
