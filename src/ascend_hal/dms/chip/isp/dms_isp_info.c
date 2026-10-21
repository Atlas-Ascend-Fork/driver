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
#ifdef CFG_FEATURE_ISP_INFO
#include <sys/ioctl.h>
#include "securec.h"
#include "dms_user_common.h"
#include "dsmi_common_interface.h"
#include "hisp_config_status.h"
#include "dms_isp_info.h"

#ifndef STATIC
#define STATIC static
#endif

STATIC int dms_get_isp_status(unsigned int dev_id, unsigned int vfid, unsigned int isp_subcmd, void *buf,
                              unsigned int *size)
{
    (void)vfid;
    (void)isp_subcmd;
    int ret;
    unsigned int device_id = dev_id;
    isp_status_stru isp_status = {0};
    struct dms_ioctl_arg ioarg = {0};

    ioarg.main_cmd = DMS_MAIN_CMD_ISP;
    ioarg.sub_cmd = DMS_SUBCMD_GET_ISP_STATUS;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&device_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = (void *)&isp_status;
    ioarg.output_len = sizeof(isp_status_stru);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_ERR("DmsIoctl failed. (ret=%d)\n", ret);
        return errno_to_user_errno(ret);
    }

    ret = memcpy_s(buf, *size, &isp_status.status, sizeof(isp_status.status));
    if (ret != 0) {
        DMS_ERR("memcpy_s failed, (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    *size = sizeof(isp_status.status);

    return DRV_ERROR_NONE;
}

STATIC int dms_extract_camera_config(camera_config_stru *camera_config, unsigned int isp_subcmd, void *buf,
                                     unsigned int *size)
{
    int ret;

    switch (isp_subcmd) {
        case DSMI_SUB_CMD_ISP_CAMERA_NAME:
            ret = memcpy_s(buf, *size, &camera_config->cameraName, sizeof(camera_config->cameraName));
            *size = sizeof(camera_config->cameraName);
            break;
        case DSMI_SUB_CMD_ISP_CAMERA_TYPE:
            ret = memcpy_s(buf, *size, &camera_config->cameraType, sizeof(camera_config->cameraType));
            *size = sizeof(camera_config->cameraType);
            break;
        case DSMI_SUB_CMD_ISP_CAMERA_BINOCULAR_TYPE:
            ret = memcpy_s(buf, *size, &camera_config->binocularType, sizeof(camera_config->binocularType));
            *size = sizeof(camera_config->binocularType);
            break;
        case DSMI_SUB_CMD_ISP_CAMERA_FULLSIZE_WIDTH:
            ret = memcpy_s(buf, *size, &camera_config->fullSizeWidth, sizeof(camera_config->fullSizeWidth));
            *size = sizeof(camera_config->fullSizeWidth);
            break;
        case DSMI_SUB_CMD_ISP_CAMERA_FULLSIZE_HEIGHT:
            ret = memcpy_s(buf, *size, &camera_config->fullSizeHeight, sizeof(camera_config->fullSizeHeight));
            *size = sizeof(camera_config->fullSizeHeight);
            break;
        case DSMI_SUB_CMD_ISP_CAMERA_FOV:
            ret = memcpy_s(buf, *size, &camera_config->fov, sizeof(camera_config->fov));
            *size = sizeof(camera_config->fov);
            break;
        case DSMI_SUB_CMD_ISP_CAMERA_CFA:
            ret = memcpy_s(buf, *size, &camera_config->cfa, sizeof(camera_config->cfa));
            *size = sizeof(camera_config->cfa);
            break;
        case DSMI_SUB_CMD_ISP_CAMERA_EXPOSURE_MODE:
            ret = memcpy_s(buf, *size, &camera_config->exposureMode, sizeof(camera_config->exposureMode));
            *size = sizeof(camera_config->exposureMode);
            break;
        case DSMI_SUB_CMD_ISP_CAMERA_RAWFORMAT:
            ret = memcpy_s(buf, *size, &camera_config->rawFormat, sizeof(camera_config->rawFormat));
            *size = sizeof(camera_config->rawFormat);
            break;
        default:
            DMS_ERR("isp_subcmd is invalid, (isp_subcmd=%u).\n", isp_subcmd);
            return DRV_ERROR_INVALID_VALUE;
    }
    if (ret != 0) {
        DMS_ERR("memcpy_s failed, (ret=%d), (isp_subcmd=%u)\n", ret, isp_subcmd);
        return DRV_ERROR_INNER_ERR;
    }
    return DRV_ERROR_NONE;
}

STATIC int dms_get_isp_config(unsigned int dev_id, unsigned int vfid, unsigned int subcmd, void *buf,
                              unsigned int *size)
{
    (void)vfid;
    int ret;
    unsigned int camera_index;
    unsigned int isp_subcmd;
    unsigned int device_id = dev_id;
    struct dms_ioctl_arg ioarg = {0};
    isp_config_stru isp_config = {0};
    camera_config_stru *camera_config = NULL;

    ioarg.main_cmd = DMS_MAIN_CMD_ISP;
    ioarg.sub_cmd = DMS_SUBCMD_GET_ISP_CONFIG;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&device_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = (void *)&isp_config;
    ioarg.output_len = sizeof(isp_config_stru);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_ERR("DmsIoctl failed. (ret=%d)\n", ret);
        return errno_to_user_errno(ret);
    }

    camera_index = subcmd >> DSMI_ISP_CAMERA_INDEX_OFFSET;
    if (camera_index >= MAX_CAMERA_NUM) {
        DMS_ERR("invalid camera index, (camera_index=%d)\n", camera_index);
        return DRV_ERROR_INVALID_VALUE;
    }
    camera_config = &isp_config.cameras[camera_index];
    isp_subcmd = subcmd & ISP_SUB_CMD_MASK;

    ret = dms_extract_camera_config(camera_config, isp_subcmd, buf, size);
    if (ret != 0) {
        DMS_ERR("extract camera config info failed, (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    return DRV_ERROR_NONE;
}

int DmsGetIspInfo(unsigned int dev_id, unsigned int vfid, unsigned int subcmd, void *buf, unsigned int *size)
{
    int ret;
    unsigned int isp_subcmd;

    if ((buf == NULL) || (size == NULL)) {
        DMS_ERR("invalid para, buf or size is null pointer.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    // The most significant 8 bits are camera_index, and the least significant 24 bits are sub_cmd.
    isp_subcmd = subcmd & ISP_SUB_CMD_MASK;
    if (isp_subcmd == DSMI_SUB_CMD_ISP_STATUS) {
        ret = dms_get_isp_status(dev_id, vfid, isp_subcmd, buf, size);
        if (ret != 0) {
            DMS_ERR("get isp status failed, (ret=%d)\n", ret);
        }
    } else {
        ret = dms_get_isp_config(dev_id, vfid, subcmd, buf, size);
        if (ret != 0) {
            DMS_ERR("get isp config failed, (ret=%d)\n", ret);
        }
    }

    return ret;
}
#endif // CFG_FEATURE_ISP_INFO
#else
void dms_isp_info_ut_stub(void)
{
    return;
}
#endif
