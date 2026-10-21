/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "securec.h"
#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "drv_type.h"
#include "dms_user_interface.h"
#include "dms_device_info.h"
#include "dms_user_common.h"
#include "ascend_dev_num.h"

#define DMS_HOST_AICPU_MAX_NUM 512

drvError_t DmsSetHostAicpuInfo(unsigned int dev_id, unsigned int sub_cmd, const void *buf, unsigned int size)
{
    const struct dsmi_host_aicpu_info *info = (const struct dsmi_host_aicpu_info *)buf;
    unsigned int i, j;
    unsigned int bitmap_num = 0;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (buf == NULL) || (size != sizeof(struct dsmi_host_aicpu_info))) {
        DMS_ERR("The input parameter is incorrect. (dev_id=%u; buf=%d; size=%u;)\n", dev_id, (buf != NULL), size);
        return DRV_ERROR_PARA_ERROR;
    }

    if (info->work_mode > DSMI_HOST_AICPU_PROCESS_MODE) {
        DMS_ERR("The input parameter is incorrect. (dev_id=%u; work_mode=%u)\n", dev_id, info->work_mode);
        return DRV_ERROR_PARA_ERROR;
    }

    for (i = 0; i < DSMI_HOST_AICPU_BITMAP_LEN; i++) {
        for (j = 0; j < (DMS_HOST_AICPU_MAX_NUM / sizeof(unsigned long long)); j++) {
            if ((info->bitmap[i] & (0x1ULL << j)) != 0) {
                bitmap_num++;
            }
        }
    }

    /* Allow the bitmap not to be set. */
    if ((info->num > DMS_HOST_AICPU_MAX_NUM) || ((bitmap_num != 0) && (bitmap_num != info->num))) {
        DMS_ERR("The input parameter is incorrect. (dev_id=%u; aicpu_num=%u; bitmap_num=%u)\n", dev_id, info->num,
                bitmap_num);
        return DRV_ERROR_PARA_ERROR;
    }

    if (getuid() != 0) {
        DMS_ERR("No permission. (dev_id=%u; uid=%d)\n", dev_id, getuid());
        return DRV_ERROR_OPER_NOT_PERMITTED;
    }

    return DmsSetDeviceInfo(dev_id, DSMI_MAIN_CMD_HOST_AICPU, sub_cmd, buf, size);
}

drvError_t DmsGetHostAicpuInfo(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                               unsigned int *size)
{
    struct dsmi_host_aicpu_info *info = buf;
    int ret;

    if (size == NULL) {
        DMS_ERR("Size is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }
    if (buf == NULL) {
        DMS_ERR("Buffer is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }
    if (*size < sizeof(struct dsmi_host_aicpu_info)) {
        DMS_ERR("Wrong buffer length. (dev_id=%u, len=%u)\n", dev_id, *size);
        return DRV_ERROR_PARA_ERROR;
    }
    ret = DmsGetDeviceInfo(dev_id, main_cmd, sub_cmd, buf, size);
    if (ret != 0) {
        DMS_ERR("Failed to obtain the host aicpu information. (dev_id=%u; ret=%d)", dev_id, ret);
        return ret;
    }

    ret = memset_s((void *)info->reserved, DSMI_HOST_AICPU_RESERVED_LEN, 0, DSMI_HOST_AICPU_RESERVED_LEN);
    if (ret != 0) {
        DMS_ERR("memset_s failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    return DRV_ERROR_NONE;
}
