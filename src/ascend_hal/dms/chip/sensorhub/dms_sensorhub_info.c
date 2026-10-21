/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/**
 * brief description about this document.
 * points to focus on.
 */

#ifndef AOSCORE_TEST_UT
#ifdef CFG_FEATURE_SENSORHUB_INFO
#include <sys/ioctl.h>
#include "securec.h"
#include "dmc_user_interface.h"
#include "dsmi_common_interface.h"
#endif
#include "dms_user_common.h"
#include "dms_sensorhub_info.h"

int dms_get_sensor_hub_status(int device_id, struct dsmi_sensorhub_status_stru *sensorhub_status_data)
{
#ifdef CFG_FEATURE_SENSORHUB_INFO
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    struct dsmi_sensorhub_status_stru local_sensorhub_status = {0};

    if (sensorhub_status_data == NULL) {
        DMS_ERR("sensorhub_status_data is null pointer.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_SENSORHUB;
    ioarg.sub_cmd = DMS_SUBCMD_GET_SENSORHUB_STATUS;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&device_id;
    ioarg.input_len = sizeof(int);
    ioarg.output = (void *)&local_sensorhub_status;
    ioarg.output_len = sizeof(struct dsmi_sensorhub_status_stru);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_ERR("get sensorhub status ioctl failed. (ret=%d)\n", ret);
        return errno_to_user_errno(ret);
    }

    ret = memcpy_s(sensorhub_status_data, sizeof(struct dsmi_sensorhub_status_stru), &local_sensorhub_status,
                   sizeof(struct dsmi_sensorhub_status_stru));
    if (ret != 0) {
        DMS_ERR("memcpy_s failed, ret(%d).\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
#else

    (void)device_id;
    (void)sensorhub_status_data;

    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dms_get_sensor_hub_config(int device_id, struct dsmi_sensorhub_config_stru *sensorhub_config_data)
{
#ifdef CFG_FEATURE_SENSORHUB_INFO
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    struct dsmi_sensorhub_config_stru sensorhub_config = {0};

    if (sensorhub_config_data == NULL) {
        DMS_ERR("sensorhub_config_data is null pointer.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_SENSORHUB;
    ioarg.sub_cmd = DMS_SUBCMD_GET_SENSORHUB_CONFIG;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&device_id;
    ioarg.input_len = sizeof(int);
    ioarg.output = (void *)&sensorhub_config;
    ioarg.output_len = sizeof(struct dsmi_sensorhub_config_stru);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_ERR("get sensorhub config ioctl failed, ret(%d).\n", ret);
        return errno_to_user_errno(ret);
    }

    ret = memcpy_s(sensorhub_config_data, sizeof(struct dsmi_sensorhub_config_stru), &sensorhub_config,
                   sizeof(struct dsmi_sensorhub_config_stru));
    if (ret != 0) {
        DMS_ERR("memcpy_s failed, ret(%d).\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
#else

    (void)device_id;
    (void)sensorhub_config_data;

    return DRV_ERROR_NOT_SUPPORT;
#endif
}
#else
void dms_sensorhub_ut_stub(void)
{
    return;
}
#endif
