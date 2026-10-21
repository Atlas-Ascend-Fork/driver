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
#include "dms_user_common.h"
#include "ascend_dev_num.h"
#include "dms_device_info.h"
#include "dms/dms_devdrv_info_comm.h"

#ifdef CFG_FEATURE_MULTI_FRAME_IPC
#define IMU_DMP_MSG_RECV 220
#else
#define IMU_DMP_MSG_RECV 32
#endif

int dms_imu_dmp_msg_recv(int fd, unsigned int dev_id, unsigned char *buf, unsigned char buf_len,
                         unsigned char *recv_len)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct urd_imu_dmp_info info = {0};
    int ret;
    (void)fd;

    if (buf == NULL || dev_id >= ASCEND_DEV_MAX_NUM || recv_len == NULL) {
        DMS_ERR("Parameter is invalid. (dev_id=%u; buf_is_null=%d; recv_len_is_null=%d)\n", dev_id, (buf == NULL),
                (recv_len == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_IMU, DMS_IMU_SUBCMD_DMP_MSG_RECV, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&dev_id, sizeof(unsigned int), (void *)&info,
                          sizeof(struct urd_imu_dmp_info));
    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret != 0) {
        /* will rerun fail when imu not inited,not add log */
        return ret;
    }

    if ((buf_len < info.len) || (info.len > IMU_DMP_MSG_RECV)) {
        DMS_ERR("Invalid parameter. (dev_id=%u; buf_len=%u; recv_len=%u)\n", dev_id, buf_len, info.len);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = memcpy_s((void *)buf, buf_len, (void *)info.data, info.len);
    if (ret != 0) {
        DMS_ERR("Failed to copy the memory. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    *recv_len = info.len;

    return DRV_ERROR_NONE;
}

int dms_imu_dmp_msg_send(int fd, unsigned int dev_id, unsigned char *buf, unsigned char len)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct urd_imu_dmp_info info = {0};
    int ret;
    (void)fd;

    if (buf == NULL || dev_id >= ASCEND_DEV_MAX_NUM || len > IMU_DMP_MSG_RECV) {
        DMS_ERR("Parameter is invalid. (dev_id=%u; len=%u; buf_is_null=%d)\n", dev_id, len, (buf == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    info.dev_id = dev_id;
    info.len = len;
    ret = memcpy_s((void *)info.data, IMU_DMP_MSG_RECV, (void *)buf, len);
    if (ret != 0) {
        DMS_ERR("Failed to copy the memory. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_IMU, DMS_IMU_SUBCMD_DMP_MSG_SEND, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&info, sizeof(struct urd_imu_dmp_info), NULL, 0);
    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Failed to send the message. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int DmsGetImuInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    (void)vfid;

    return halDmsGetDeviceInfoEx(dev_id, DSMI_MAIN_CMD_CHIP_INF, sub_cmd, buf, size);
}