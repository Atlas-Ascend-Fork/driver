/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "dms_cmd_def.h"
#include "dms_user_common.h"
#include "dms/dms_misc_interface.h"
#include "dms_device_info.h"

int DmsGetSioInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    int ret;
    unsigned int sio_sub_cmd;
    struct dsmi_sio_crc_err_statistics_info *out_info = NULL;
    struct dms_sio_crc_err_info info = {0};
    unsigned int out_len = sizeof(struct dms_sio_crc_err_info);

    (void)vfid;

    if ((buf == NULL) || (size == NULL)) {
        DMS_ERR("Size or buf is NULL. (dev_id=%u; buf_is_null=%d; size_is_null=%d)\n", dev_id, (buf == NULL),
                (size == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    if (*size < sizeof(struct dsmi_sio_crc_err_statistics_info)) {
        DMS_ERR("Wrong buffer length. (dev_id=%u; len=%u)\n", dev_id, *size);
        return DRV_ERROR_PARA_ERROR;
    }

    sio_sub_cmd = sub_cmd & DSMI_SIO_SUB_CMD_BIT;
    info.sllc_index = (unsigned char)((sub_cmd & DSMI_SIO_SLLC_INDEX_BIT) >> DSMI_SIO_SLLC_INDEX_OFFSET);

    ret = halDmsGetDeviceInfoEx(dev_id, DSMI_MAIN_CMD_SIO, sio_sub_cmd, &info, &out_len);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get device info failed. (dev_id=%u; sub_cmd=%u; index=%u; ret=%d)\n", dev_id,
                              sio_sub_cmd, info.sllc_index, ret);
        return ret;
    }

    if (out_len != sizeof(struct dms_sio_crc_err_info)) {
        DMS_ERR("Wrong buffer length. (dev_id=%u; out_len=%u; ret=%d)\n", dev_id, out_len, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    out_info = (struct dsmi_sio_crc_err_statistics_info *)buf;
    out_info->tx_error_count = info.tx_error_count;
    out_info->rx_error_count = info.rx_error_count;

    *size = sizeof(struct dsmi_sio_crc_err_statistics_info);

    return ret;
}
