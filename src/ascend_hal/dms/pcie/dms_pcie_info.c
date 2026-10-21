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
#include "mmpa_api.h"
#include "dms_user_common.h"
#include "dms/dms_misc_interface.h"
#include "ascend_hal.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devmng_common.h"
#include "devmng_user_common.h"
#include "dsmi_common_interface.h"
#include "dms_device_info.h"
#include "dms_pcie.h"
#include "ascend_dev_num.h"

#ifdef CFG_FEATURE_DMS_ARCH_V1
#ifdef __linux
#include "devmng_cmd_def.h"
#define fd_is_invalid(fd) ((fd) < 0)
#else
#define fd_is_invalid(fd) ((fd) == (mmProcess)DEVDRV_INVALID_FD_OR_INDEX)
#endif
#endif

#ifndef CFG_DMS_HOT_RESET_USER_UNSUPPORT
#ifdef CFG_FEATURE_DMS_ARCH_V1
STATIC drvError_t drv_pcie_rescan_arch_v_1(uint32_t devId)
{
#ifdef __linux
    struct devdrv_pcie_rescan pcie_rescan = {0};
    int ret;
    int err_buf;
    int fd = -1;
    mmIoctlBuf buf = {0};
    if (devId >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("invalid devid(%u).\n", devId);
        return DRV_ERROR_INVALID_DEVICE;
    }
    fd = devdrv_open_device_manager();
    if (fd < 0) {
        DMS_ERR("open device manager failed, fd(%d). devid(%u)\n", fd, devId);
        return DRV_ERROR_INVALID_HANDLE;
    }
    pcie_rescan.dev_id = devId;
    buf.inbuf = (void *)&pcie_rescan;
    buf.inbufLen = sizeof(struct devdrv_pcie_rescan);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;
    ret = dmanage_mmIoctl(fd, DEVDRV_MANAGER_PCIE_RESCAN, &buf);
    if (ret != 0) {
        err_buf = errno;
        DMS_EX_NOTSUPPORT_ERR(err_buf, "ioctl failed, devid(%u), ret(%d), errno(%d).\n", devId, ret, err_buf);
#ifdef CFG_FEATURE_ERRORCODE_ON_NEW_CHIPS
        return ret == -1 ? errno_to_user_errno(err_buf) : ret;
#else
        return errno_to_user_errno(-err_buf);
#endif
    }
#else
    mmProcess fd = -1;
    int ret;

    fd = devdrv_open_device_manager();
    if (!fd_is_invalid(fd)) {
        fd = (mmProcess)DEVDRV_INVALID_FD_OR_INDEX;
    }
    ret = stop_device();
    if (ret != EN_OK) {
        DMS_ERR("stopDevice failed, ret(%d)\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }
    mmSleep(100); // 100ms
    ret = start_device();
    if (ret != EN_OK) {
        DMS_ERR("startDevice failed, ret(%d)\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }
#endif
    return DRV_ERROR_NONE;
}

STATIC drvError_t drv_pcie_pre_reset_arch_v_1(uint32_t devId)
{
    struct devdrv_pcie_pre_reset pcie_pre_reset = {0};
    int ret;
    int err_buf;
    mmProcess fd = -1;
    mmIoctlBuf buf = {0};

    if (devId >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("invalid devid(%u).\n", devId);
        return DRV_ERROR_INVALID_DEVICE;
    }
    fd = devdrv_open_device_manager();
    if (fd_is_invalid(fd)) {
        DMS_ERR("open device manager failed, fd(%d). devid(%u)\n", fd, devId);
        return DRV_ERROR_INVALID_HANDLE;
    }
    pcie_pre_reset.dev_id = devId;
    buf.inbuf = (void *)&pcie_pre_reset;
    buf.inbufLen = sizeof(struct devdrv_pcie_pre_reset);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;
    ret = dmanage_mmIoctl(fd, DEVDRV_MANAGER_PCIE_PRE_RESET, &buf);
    if (ret != 0) {
        err_buf = errno;
        DMS_EX_NOTSUPPORT_ERR(err_buf, "ioctl failed, devid(%u), ret(%d), errno(%d).\n", devId, ret, err_buf);
#ifdef CFG_FEATURE_ERRORCODE_ON_NEW_CHIPS
        return ret == -1 ? errno_to_user_errno(err_buf) : ret;
#else
        return errno_to_user_errno(-err_buf);
#endif
    }

    return DRV_ERROR_NONE;
}
#else
STATIC drvError_t dms_pcie_pre_reset(unsigned int dev_id)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    int ret;

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("Invalid device ID. (dev_id=%u; max_dev_num=%u)\n", dev_id, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_INVALID_DEVICE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_HOTRESET, DMS_SUBCMD_PRERESET_ASSEMBLE1, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&dev_id, sizeof(unsigned int), NULL, 0);
    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "PCIE pre-reset failure. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t dms_pcie_rescan(unsigned int dev_id)
{
    int ret;
#ifdef __linux
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("Invalid device ID. (dev_id=%u; max_dev_num=%u)\n", dev_id, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_INVALID_DEVICE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_HOTRESET, DMS_SUBCMD_HOTRESET_RESCAN, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&dev_id, sizeof(unsigned int), NULL, 0);
    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms set pre rescan failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
#else
    ret = stop_device();
    if (ret != 0) {
        DMS_ERR("Stop device failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }
    mmSleep(100); /* 100ms */
    ret = start_device();
    if (ret != 0) {
        DMS_ERR("Start device failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }
#endif
    return DRV_ERROR_NONE;
}
#endif

drvError_t drvPciePreReset(uint32_t devId)
{
#ifdef CFG_FEATURE_DMS_ARCH_V1
    return drv_pcie_pre_reset_arch_v_1(devId);
#else
    return dms_pcie_pre_reset(devId);
#endif
}

drvError_t drvPcieRescan(uint32_t devId)
{
#ifdef CFG_FEATURE_DMS_ARCH_V1
    return drv_pcie_rescan_arch_v_1(devId);
#else
    return dms_pcie_rescan(devId);
#endif
}
#endif
