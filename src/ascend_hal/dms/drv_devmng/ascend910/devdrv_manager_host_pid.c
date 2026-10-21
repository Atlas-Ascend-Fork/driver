/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "devmng_common.h"
#include "ascend_hal.h"
#include "dms_user_common.h"
#ifndef CFG_FEATURE_APM_SUPP_PID
#include "securec.h"
#include "mmpa_api.h"
#include "devdrv_ioctl.h"
#include "devmng_user_common.h"
#include "drv_devmng_adapt.h"
#endif

#ifndef __linux
#define fd_is_invalid(fd) (fd == (mmProcess)DEVDRV_INVALID_FD_OR_INDEX)
#else
#define fd_is_invalid(fd) ((fd) < 0)
#endif

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#ifndef CFG_FEATURE_APM_SUPP_PID
drvError_t drvBindHostPid(struct drvBindHostpidInfo info)
{
    struct devdrv_ioctl_para_bind_host_pid ioctl_para = {0};
    mmIoctlBuf para_buf = {0};
    int ret;

    if ((info.len != PROCESS_SIGN_LENGTH) || (info.cp_type >= DEVDRV_PROCESS_CPTYPE_MAX)) {
        DEVDRV_DRV_ERR("sign value or len/cp_type invalid: len:%u cp_type:%d\n", info.len, info.cp_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    ioctl_para.host_pid = info.host_pid;
    ioctl_para.chip_id = info.chip_id;
    ioctl_para.mode = info.mode;
    ioctl_para.cp_type = info.cp_type;
    ioctl_para.len = info.len;
    ioctl_para.vfid = info.vfid;
    ret = memcpy_s(ioctl_para.sign, PROCESS_SIGN_LENGTH, info.sign, info.len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("memcpy_s error: %d, pid:%d, chip id:%u, cp_type:%d, mode:%d, vfid:%u.\n", ret, info.host_pid,
                       info.chip_id, info.cp_type, info.mode, info.vfid);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    para_buf.inbuf = (void *)&ioctl_para;
    para_buf.inbufLen = sizeof(struct devdrv_ioctl_para_bind_host_pid);
    para_buf.outbuf = para_buf.inbuf;
    para_buf.outbufLen = para_buf.inbufLen;
    para_buf.oa = NULL;

    ret = drv_common_ioctl(&para_buf, DEVDRV_MANAGER_BIND_PID_ID);
    if (ret != 0) {
        if (ret == DRV_ERROR_NOT_SUPPORT) {
            return ret;
        }
        DEVDRV_DRV_ERR("Ioctl error. (ret=%d, pid=%d, chip id=%u, cp_type=%d, mode=%d)\n", ret, info.host_pid,
                       info.chip_id, info.cp_type, info.mode);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvUnbindHostPid(struct drvBindHostpidInfo info)
{
#ifndef CFG_FEATURE_HOST_UNBIND
    (void)info;
    return DRV_ERROR_NOT_SUPPORT;
#else
    struct devdrv_ioctl_para_bind_host_pid ioctl_para = {0};
    mmIoctlBuf para_buf = {0};
    int ret;

    if ((info.len != PROCESS_SIGN_LENGTH) || (info.cp_type != DEVDRV_PROCESS_QS)) {
        DEVDRV_DRV_ERR("sign value or len/cp_type invalid: len:%u cp_type:%d\n", info.len, info.cp_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    ioctl_para.host_pid = info.host_pid;
    ioctl_para.chip_id = info.chip_id;
    ioctl_para.mode = info.mode;
    ioctl_para.cp_type = info.cp_type;
    ioctl_para.len = info.len;
    ioctl_para.vfid = info.vfid;
    ret = memcpy_s(ioctl_para.sign, PROCESS_SIGN_LENGTH, info.sign, info.len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("memcpy_s ret: %d, pid:%d, chip_id:%u, cp_type:%d, mode:%d, vfid:%u.\n", ret, info.host_pid,
                       info.chip_id, info.cp_type, info.mode, info.vfid);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    para_buf.inbuf = (void *)&ioctl_para;
    para_buf.inbufLen = sizeof(struct devdrv_ioctl_para_bind_host_pid);
    para_buf.outbuf = para_buf.inbuf;
    para_buf.outbufLen = para_buf.inbufLen;
    para_buf.oa = NULL;

    ret = drv_common_ioctl(&para_buf, DEVDRV_MANAGER_UNBIND_PID_ID);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, ret(%d), pid:%d, chip_id:%u, cp_type:%d, mode:%d.\n", ret, info.host_pid,
                       info.chip_id, info.cp_type, info.mode);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
#endif
}

drvError_t halQueryDevpid(struct halQueryDevpidInfo info, pid_t *dev_pid)
{
    int ret;
    struct devdrv_ioctl_para_query_pid ioctl_para = {0};
    mmIoctlBuf para_buf = {0};
    mmProcess fd = -1;

    if (info.proc_type == DEVDRV_PROCESS_USER) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (info.proc_type >= DEVDRV_PROCESS_CPTYPE_MAX || dev_pid == NULL) {
        DEVDRV_DRV_ERR("proc_type invalid: proc_type:%d or dev_pid is NULL\n", info.proc_type);
        return DRV_ERROR_INVALID_VALUE;
    }
    fd = devdrv_open_device_manager();
    if (fd_is_invalid(fd)) {
        DEVDRV_DRV_ERR("open device error, hostpid:%d, devid:%u, proc_type:%d\n", info.hostpid, info.devid,
                       info.proc_type);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ioctl_para.host_pid = info.hostpid;
    ioctl_para.vfid = info.vfid;
    ioctl_para.chip_id = info.devid;
    ioctl_para.cp_type = info.proc_type;

    para_buf.oa = NULL;
    para_buf.inbuf = (void *)&ioctl_para;
    para_buf.inbufLen = sizeof(struct devdrv_ioctl_para_query_pid);
    para_buf.outbuf = para_buf.inbuf;
    para_buf.outbufLen = para_buf.inbufLen;

    ret = dmanage_mmIoctl(fd, DEVDRV_MANAGER_QUERY_DEV_PID, &para_buf);
    if (ret != 0) {
        /* ES, HDC perform the next query based on this return code */
        if (ret == DRV_ERROR_NO_PROCESS) {
            DEVDRV_DRV_WARN("This host pid didn't bind this type device pid. (host_pid=%d)\n", info.hostpid);
            return ret;
        }
        DEVDRV_DRV_WARN("ioctl or query warn, errno(%d), hostpid:%d, devid:%u, proc_type:%d\n", errno, info.hostpid,
                        info.devid, info.proc_type);
        return DRV_ERROR_IOCRL_FAIL;
    }

    *dev_pid = ioctl_para.pid;

    return DRV_ERROR_NONE;
}

drvError_t drvQueryProcessHostPid(int pid, unsigned int *chip_id, unsigned int *vfid, unsigned int *host_pid,
                                  unsigned int *cp_type)
{
    int ret, err_buf;
    struct devdrv_ioctl_para_query_pid ioctl_para = {0};
    mmIoctlBuf para_buf = {0};
    mmProcess fd = -1;

    if (pid <= 0) {
        DEVDRV_DRV_ERR("invalid input parameter. (pid=%d)\n", pid);
        return DRV_ERROR_INVALID_VALUE;
    }

    fd = devdrv_open_device_manager();
    if ((int)fd == -DRV_ERROR_RESOURCE_OCCUPIED) {
        DEVDRV_DRV_ERR("open device manager failed, device is busy.\n");
        return DRV_ERROR_RESOURCE_OCCUPIED;
    }
    if (fd_is_invalid(fd)) {
        DEVDRV_DRV_ERR("Open device manager failed. (fd=%d)\n", fd);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ioctl_para.pid = pid;

    para_buf.inbuf = (void *)&ioctl_para;
    para_buf.outbuf = para_buf.inbuf;
    para_buf.inbufLen = sizeof(struct devdrv_ioctl_para_query_pid);
    para_buf.outbufLen = para_buf.inbufLen;
    para_buf.oa = NULL;

    ret = dmanage_mmIoctl(fd, DEVDRV_MANAGER_QUERY_HOST_PID, &para_buf);
    if (ret != 0) {
        err_buf = errno;
        if (err_buf == ESRCH) {
            DEVDRV_DRV_DEBUG("Can not find hostpid. (ret=%d; pid=%d)\n", ret, pid);
            return DRV_ERROR_NO_PROCESS;
        } else if (err_buf == EPERM) {
            return DRV_ERROR_IOCRL_FAIL;
        } else if (err_buf == EOPNOTSUPP) {
            hal_report_not_support("drvQueryProcessHostPid", "This API cannot be called in the split scenario");
            return DRV_ERROR_NOT_SUPPORT;
        }
        DEVDRV_DRV_ERR("Ioctl failed. (errno=%d; pid=%d)\n", errno, pid);
        return DRV_ERROR_IOCRL_FAIL;
    }

    if (chip_id != NULL) {
        *chip_id = ioctl_para.chip_id;
    }

    if (vfid != NULL) {
        *vfid = ioctl_para.vfid;
    }

    if (host_pid != NULL) {
        *host_pid = (unsigned int)ioctl_para.host_pid;
    }

    if (cp_type != NULL) {
        *cp_type = ioctl_para.cp_type;
    }

    return DRV_ERROR_NONE;
}
#endif
