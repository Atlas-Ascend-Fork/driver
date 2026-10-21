/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include "devmng_common.h"
#include "devdrv_user_common.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devdrv_ioctl.h"
#include "securec.h"
#include "devdrv_shm_comm.h"
#include "tsdrv_user_interface.h"
#include "devmng_user_common.h"
#include "devdrv_hccl.h"
#ifndef CFG_FEATURE_TRS_REFACTOR
#include "ascend_dev_num.h"
#endif

#define DEVDRV_THREAD_STACK_SIZE (128 * 1024)

drvError_t drvGetLocalDevIDs(uint32_t *devices, uint32_t len)
{
    drvError_t err;
    uint32_t info;

    if (devices == NULL) {
        DEVDRV_DRV_ERR("devices is NULL\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    err = drvGetPlatformInfo(&info);
    if (err != DRV_ERROR_NONE) {
        DEVDRV_DRV_ERR("get platform info failed\n");
        return DRV_ERROR_NO_DEVICE;
    }

    if (info == 0) {
        return drvGetDeviceLocalIDs(devices, len);
    }

    return drvGetDevIDs(devices, len);
}

#ifndef CFG_FEATURE_APM_SUPP_PID
drvError_t drvGetProcessSign(struct process_sign *sign)
{
    struct process_sign dev_sign;
    int fd = -1;
    int ret;
    int i;

    if (sign == NULL) {
        DEVDRV_DRV_ERR("sign is null.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }
    fd = devdrv_open_device_manager();
    if (fd < 0) {
        DEVDRV_DRV_ERR("open devdrv_device manager failed, fd(%d).\n", fd);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = ioctl(fd, DEVDRV_MANAGER_GET_PROCESS_SIGN, &dev_sign);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, (ret=%d; errno=%d).\n", ret, errno);
        return DRV_ERROR_IOCRL_FAIL;
    }
    DEVDRV_DRV_INFO("Process sign list add success.(tgid=%d)\n", dev_sign.tgid);

    sign->tgid = dev_sign.tgid;
    for (i = 0; i < PROCESS_SIGN_LENGTH; i++) {
        sign->sign[i] = dev_sign.sign[i];
        dev_sign.sign[i] = 0;
    }
    return DRV_ERROR_NONE;
}
#endif

#ifndef CFG_FEATURE_TRS_REFACTOR
static struct tsdrv_shrid_remote_ops g_shrid_remote_ops = {NULL};
void tsDrvRegisterShridRemoteOps(struct tsdrv_shrid_remote_ops *ops)
{
    g_shrid_remote_ops = *ops;
}

static struct tsdrv_shrid_remote_ops *tsDrvGetShridRemoteOps(void)
{
    return &g_shrid_remote_ops;
}

static drvError_t tsDrvShridRemoteConfig(uint32_t local_devid, uint32_t remote_devId, uint32_t tsId, uint32_t IdType,
                                         uint32_t Id)
{
    struct drvShrIdInfo info = {0};
    info.devid = remote_devId;
    info.tsid = tsId;
    info.id_type = IdType;
    info.shrid = Id;

    if (tsDrvGetShridRemoteOps()->shrid_config != NULL) {
        return tsDrvGetShridRemoteOps()->shrid_config(local_devid, &info);
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }
    return DRV_ERROR_NONE;
}

static drvError_t tsDrvShridRemoteDeconfig(uint32_t local_devid, uint32_t remote_devId, uint32_t tsId, uint32_t IdType,
                                           uint32_t Id)
{
    struct drvShrIdInfo info = {0};
    info.devid = remote_devId;
    info.tsid = tsId;
    info.id_type = IdType;
    info.shrid = Id;

    if (tsDrvGetShridRemoteOps()->shrid_deconfig != NULL) {
        return tsDrvGetShridRemoteOps()->shrid_deconfig(local_devid, &info);
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }
    return DRV_ERROR_NONE;
}

static int tsDrvShridRemoteOps(uint32_t local_devid, struct devdrv_notify_ioctl_info *info)
{
    u32 res_id = info->notify_id;
    int ret;

    if (NOTIFY_ID_TYPE == IPC_EVENT_TYPE) {
        res_id |= IPC_EVENT_MASK;
    }

    if ((info->flag & DEVDRV_SHRID_OP_CONFIG) != 0) {
        return tsDrvShridRemoteConfig(local_devid, info->dev_id, info->tsid, SHR_ID_NOTIFY_TYPE, res_id);
    }

    if ((info->flag & DEVDRV_SHRID_OP_DECONFIG) != 0) {
        ret = tsDrvShridRemoteDeconfig(local_devid, info->dev_id, info->tsid, SHR_ID_NOTIFY_TYPE, res_id);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(
                ret,
                "Failed to tsDrvShridRemoteDeconfig. (local_devid=%u; remote_devid=%u; tsid=%u; id_type=%u; id=%u)\n",
                local_devid, info->dev_id, info->tsid, info->id_type, info->notify_id);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t __drvIpcNotifyProc(int32_t cmd, const char *name, uint32_t devId, uint32_t notifyId,
                                     struct devdrv_notify_ioctl_info *notify_ioc)
{
    struct devdrv_notify_ioctl_info tmp_notify_ioc = {0};
    struct drvIpcNotifyInfo info = {0};
    uint32_t local_devid = devId;
    int fd = -1;
    int len;
    int ret;
    int err;

    if (name == NULL || devId >= ASCEND_DEV_MAX_NUM || notifyId >= DEVDRV_MAX_NOTIFY_ID) {
        DEVDRV_DRV_ERR("invalid parameter. (name_is_null=%d; devId=%u; notifyId=%u)\n", (name == NULL), devId,
                       notifyId);
        return DRV_ERROR_INVALID_VALUE;
    }
    fd = devdrv_open_device_manager();
    if (fd < 0) {
        DEVDRV_DRV_ERR("open drvdev_device manager failed, fd(%d). devid(%u)\n", fd, devId);
        return DRV_ERROR_INVALID_VALUE;
    }

    len = strnlen(name, DEVDRV_IPC_NAME_SIZE);
    if (len == 0 || len == DEVDRV_IPC_NAME_SIZE) {
        DEVDRV_DRV_ERR("invalid name, len(%d), devid(%u).\n", len, devId);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = memcpy_s(tmp_notify_ioc.name, DEVDRV_IPC_NAME_SIZE, name, len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("memcpy failed, len(%d), ret(%d), devid(%u).\n", len, ret, devId);
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    tmp_notify_ioc.dev_id = devId;
    tmp_notify_ioc.notify_id = notifyId;
    tmp_notify_ioc.id_type = NOTIFY_ID_TYPE;
    if (notify_ioc != NULL) {
        tmp_notify_ioc.open_devid = notify_ioc->open_devid;
    }
    ret = ioctl(fd, cmd, &tmp_notify_ioc);
    if (ret != 0) {
        err = errno;
        (void)memset_s(tmp_notify_ioc.name, DEVDRV_IPC_NAME_SIZE, 0, DEVDRV_IPC_NAME_SIZE);
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), notifyid(%u), ret(%d)\n", devId, notifyId, ret);
        if (err == EPERM) {
            return DRV_ERROR_OPER_NOT_PERMITTED;
        }
        return DRV_ERROR_IOCRL_FAIL;
    }

    if (notify_ioc != NULL) {
        if (cmd == DEVDRV_MANAGER_IPC_NOTIFY_GET_ATTR) {
            notify_ioc->enableFlag = tmp_notify_ioc.enableFlag;
        } else {
            notify_ioc->dev_id = tmp_notify_ioc.dev_id;
            notify_ioc->tsid = tmp_notify_ioc.tsid;
            notify_ioc->notify_id = tmp_notify_ioc.notify_id;
        }
    }
    (void)memset_s(tmp_notify_ioc.name, DEVDRV_IPC_NAME_SIZE, 0, DEVDRV_IPC_NAME_SIZE);
    DEVDRV_DRV_DEBUG("devid=%u; remote_devid=%u; open_devid=%u; tsid=%u; notify_id=%u; id_type=%u; flag=%u\n", devId,
                     tmp_notify_ioc.dev_id, tmp_notify_ioc.open_devid, tmp_notify_ioc.tsid, tmp_notify_ioc.notify_id,
                     tmp_notify_ioc.id_type, tmp_notify_ioc.flag);

    if ((tmp_notify_ioc.flag & DEVDRV_SHRID_OP_DECONFIG) != 0) {
        local_devid = tmp_notify_ioc.open_devid;
    }
    ret = tsDrvShridRemoteOps(local_devid, &tmp_notify_ioc);
    if (ret != 0) {
        if ((tmp_notify_ioc.flag & DEVDRV_SHRID_OP_CONFIG) != 0) {
            info.notifyId = tmp_notify_ioc.notify_id;
            (void)drvCloseIpcNotify(name, &info);
        }
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to tsDrvShridRemoteOps. (local_devid=%u; cmd=%d; flag=%u; ret=%d).\n",
                                     local_devid, cmd, tmp_notify_ioc.flag, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t __drvIpcNotifyCreat(char *name, struct drvShrIdInfo *info)
{
    struct devdrv_notify_ioctl_info notify_ioc = {0};
    int fd = -1;
    int ret;

    fd = devdrv_open_device_manager();
    if (fd < 0) {
        DEVDRV_DRV_ERR("open devdrv_device manager failed, fd(%d). devid(%u)\n", fd, info->devid);
        return DRV_ERROR_INVALID_VALUE;
    }
    notify_ioc.dev_id = info->devid;
    notify_ioc.notify_id = info->shrid;
    notify_ioc.tsid = info->tsid;
    notify_ioc.id_type = info->id_type;
    notify_ioc.flag = info->flag;
    DEVDRV_DRV_DEBUG("devid=%u; tsid=%u; notify_id=%u; id_type=%u; flag=%u\n", notify_ioc.dev_id, notify_ioc.tsid,
                     notify_ioc.notify_id, notify_ioc.id_type, notify_ioc.flag);
    if (ioctl(fd, DEVDRV_MANAGER_IPC_NOTIFY_CREATE, &notify_ioc) != 0) {
        (void)memset_s(notify_ioc.name, DEVDRV_IPC_NAME_SIZE, 0, DEVDRV_IPC_NAME_SIZE);
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), notifyid(%u), errno(%d).\n", info->devid, info->shrid, errno);
        return DRV_ERROR_IOCRL_FAIL;
    }
    ret = strcpy_s(name, DEVDRV_IPC_NAME_SIZE, notify_ioc.name);
    if (ret != 0) {
        (void)memset_s(notify_ioc.name, DEVDRV_IPC_NAME_SIZE, 0, DEVDRV_IPC_NAME_SIZE);
        DEVDRV_DRV_ERR("strcpy failed, ret(%d).\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }
    (void)memset_s(notify_ioc.name, DEVDRV_IPC_NAME_SIZE, 0, DEVDRV_IPC_NAME_SIZE);

    return DRV_ERROR_NONE;
}

drvError_t drvCreateIpcNotify(struct drvIpcNotifyInfo *info, char *name, unsigned int len)
{
    struct halResourceIdInputInfo in = {0};
    struct drvShrIdInfo shr_info = {0};
    drvError_t ret;
    u64 value;

    if (info == NULL) {
        DEVDRV_DRV_ERR("info is null pointer\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (name == NULL) {
        DEVDRV_DRV_ERR("name is null pointer\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (len < DEVDRV_IPC_NAME_SIZE) {
        DEVDRV_DRV_ERR("invalid len, len(%u)\n", len);
        return DRV_ERROR_INVALID_VALUE;
    }

    shr_info.devid = info->devId;
    shr_info.shrid = info->notifyId;
    shr_info.tsid = info->tsId;
    shr_info.id_type = NOTIFY_ID_TYPE;
    if ((shr_info.tsid >= DEVDRV_MAX_TS_NUM) || (shr_info.devid >= ASCEND_DEV_MAX_NUM) ||
        (shr_info.shrid >= DEVDRV_MAX_NOTIFY_ID)) {
        DEVDRV_DRV_ERR("invalid parameter, devId(%u) tsId(%u) notifyId(%u).\n", shr_info.devid, shr_info.tsid,
                       shr_info.shrid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (NOTIFY_ID_TYPE == IPC_EVENT_TYPE) {
        ret = tsDrvShrIdAllocIpcEventId(shr_info.devid, shr_info.tsid, &shr_info.shrid, shr_info.flag, &value);
        if (ret != 0) {
            return ret;
        }

        info->notifyId = shr_info.shrid | IPC_EVENT_MASK;
    }

    ret = __drvIpcNotifyCreat(name, &shr_info);
    if (ret != 0) {
        if (NOTIFY_ID_TYPE == IPC_EVENT_TYPE) {
            in.type = DRV_NOTIFY_ID;
            in.resourceId = shr_info.shrid | IPC_EVENT_MASK;
            in.tsId = shr_info.tsid;
            in.res[RESOURCEID_RESV_FLAG] = shr_info.flag;
            (void)halResourceIdFree(shr_info.devid, &in);
        }
        DEVDRV_DRV_ERR("Create ipcnotify failed. (devId=%u; ret=%d).\n", shr_info.devid, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvDestroyIpcNotify(const char *name, struct drvIpcNotifyInfo *info)
{
    uint32_t notifyId;
    uint32_t devId;

    if (info == NULL) {
        DEVDRV_DRV_ERR("info is null pointer\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    devId = info->devId;
    notifyId = info->notifyId;
    if (NOTIFY_ID_TYPE == IPC_EVENT_TYPE) {
        notifyId &= (~IPC_EVENT_MASK);
    }

    return __drvIpcNotifyProc(DEVDRV_MANAGER_IPC_NOTIFY_DESTROY, name, devId, notifyId, NULL);
}

static drvError_t _drvOpenIpcNotify(const char *name, struct drvIpcNotifyInfo *info, uint32_t flag)
{
    struct devdrv_notify_ioctl_info notify_ioc = {0};
    drvError_t err;

    if (info == NULL) {
        DEVDRV_DRV_ERR("info is null pointer\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (name == NULL) {
        DEVDRV_DRV_ERR("Invalid parameter, name is NULL.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    if ((flag & TSDRV_FLAG_REMOTE_ID) != 0) {
        notify_ioc.flag |= TSDRV_FLAG_REMOTE_ID;
        notify_ioc.open_devid = info->devId;
    }

    err = __drvIpcNotifyProc(DEVDRV_MANAGER_IPC_NOTIFY_OPEN, name, info->devId, 0, &notify_ioc);
    if (err == DRV_ERROR_NONE) {
        info->devId = notify_ioc.dev_id;
        info->tsId = notify_ioc.tsid;
        info->notifyId = notify_ioc.notify_id;
        if (NOTIFY_ID_TYPE == IPC_EVENT_TYPE) {
            info->notifyId |= IPC_EVENT_MASK;
        }
    }
    return err;
}

drvError_t drvOpenIpcNotify(const char *name, struct drvIpcNotifyInfo *info)
{
    if (info == NULL) {
        DEVDRV_DRV_ERR("The argument info is a null pointer.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    info->devId = 0;

    return _drvOpenIpcNotify(name, info, 0);
}

drvError_t drvCloseIpcNotify(const char *name, struct drvIpcNotifyInfo *info)
{
    uint32_t notifyId;

    if (info == NULL) {
        DEVDRV_DRV_ERR("The argument info is a null pointer.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    notifyId = info->notifyId;
    if (NOTIFY_ID_TYPE == IPC_EVENT_TYPE) {
        notifyId &= (~IPC_EVENT_MASK);
    }

    return __drvIpcNotifyProc(DEVDRV_MANAGER_IPC_NOTIFY_CLOSE, name, info->devId, notifyId, NULL);
}

drvError_t drvSetIpcNotifyPid(const char *name, pid_t pid[], int num)
{
    struct devdrv_notify_ioctl_info tmp_notify_ioc = {0};
    int fd = -1;
    int ret;
    int len;
    int err;
    int i;

    /* pass in only one parameter at a time in the current scene */
    if (name == NULL || pid == NULL || num != 1) {
        DEVDRV_DRV_ERR("invalid parameter. (name_is_null=%d, pid_is_null=%d, num=%d)\n", (name == NULL), (pid == NULL),
                       num);
        return DRV_ERROR_INVALID_VALUE;
    }
    fd = devdrv_open_device_manager();
    if (fd < 0) {
        DEVDRV_DRV_ERR("dev manager open failed\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    len = strnlen(name, DEVDRV_IPC_NAME_SIZE);
    if (len == 0 || len == DEVDRV_IPC_NAME_SIZE) {
        DEVDRV_DRV_ERR("invalid name, len = %d.\n", len);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = memcpy_s(tmp_notify_ioc.name, DEVDRV_IPC_NAME_SIZE, name, len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("memcpy failed, len(%d), ret(%d).\n", len, ret);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    for (i = 0; i < num; i++) {
        tmp_notify_ioc.pid[i] = pid[i];
        DEVDRV_DRV_DEBUG("PID[%d] = %d\n", i, pid[i]);
    }

    ret = ioctl(fd, DEVDRV_MANAGER_IPC_NOTIFY_SET_PID, &tmp_notify_ioc);
    err = errno;
    if (ret != 0) {
        (void)memset_s(tmp_notify_ioc.name, DEVDRV_IPC_NAME_SIZE, 0, DEVDRV_IPC_NAME_SIZE);
        DEVDRV_DRV_ERR("ioctl failed\n");
        if (err == EPERM) {
            return DRV_ERROR_OPER_NOT_PERMITTED;
        }
        return DRV_ERROR_IOCRL_FAIL;
    }
    (void)memset_s(tmp_notify_ioc.name, DEVDRV_IPC_NAME_SIZE, 0, DEVDRV_IPC_NAME_SIZE);

    return DRV_ERROR_NONE;
}

drvError_t halShrIdSetAttribute(const char *name, enum shrIdAttrType type, struct shrIdAttr attr)
{
    drvError_t err;

    if (name == NULL) {
        DEVDRV_DRV_ERR("Invalid name when setting ipc notify attribute.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((type < 0) || (type >= SHR_ID_ATTR_TYPE_MAX) || (attr.enableFlag != SHRID_NO_WLIST_ENABLE)) {
        DEVDRV_DRV_DEBUG("Access denied when setting ipc notify attribute.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }
    err = __drvIpcNotifyProc(DEVDRV_MANAGER_IPC_NOTIFY_SET_ATTR, name, 0, 0, NULL);
    if (err != DRV_ERROR_NONE) {
        return err;
    }
    return DRV_ERROR_NONE;
}

drvError_t halShrIdGetAttribute(const char *name, enum shrIdAttrType type, struct shrIdAttr *attr)
{
    struct devdrv_notify_ioctl_info notify_ioc = {0};
    drvError_t err;

    if ((name == NULL) || (attr == NULL)) {
        DEVDRV_DRV_ERR("Invalid arguments when getting ipc notify info.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((type < 0) || (type >= SHR_ID_ATTR_TYPE_MAX)) {
        DEVDRV_DRV_DEBUG("Shr node type not support.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    err = __drvIpcNotifyProc(DEVDRV_MANAGER_IPC_NOTIFY_GET_ATTR, name, 0, 0, &notify_ioc);
    if (err == DRV_ERROR_NONE) {
        attr->enableFlag = notify_ioc.enableFlag;
    }
    return err;
}

drvError_t halShrIdInfoGet(const char *name, struct shrIdGetInfo *info)
{
    struct devdrv_notify_ioctl_info notify_ioc = {0};
    drvError_t err;

    if ((name == NULL) || (info == NULL)) {
        DEVDRV_DRV_ERR("Invalid arguments when getting ipc notify info.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    err = __drvIpcNotifyProc(DEVDRV_MANAGER_IPC_NOTIFY_GET_INFO, name, 0, 0, &notify_ioc);
    if (err == DRV_ERROR_NONE) {
        info->phyDevid = notify_ioc.dev_id;
    }
    return err;
}

drvError_t drvRecordIpcNotify(const char *name)
{
    return __drvIpcNotifyProc(DEVDRV_MANAGER_IPC_NOTIFY_RECORD, name, 0, 0, NULL);
}
#ifndef DEVDRV_USER_UT_TEST
drvError_t halShrIdCreate(struct drvShrIdInfo *info, char *name, uint32_t name_len)
{
    struct halResourceIdInputInfo in = {0};
    uint32_t notifyId, devId, tsId;
    u64 value;
    drvError_t ret;

    if (info == NULL) {
        DEVDRV_DRV_ERR("Info is NULL\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (name == NULL) {
        DEVDRV_DRV_ERR("name is null pointer\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (name_len < DEVDRV_IPC_NAME_SIZE) {
        DEVDRV_DRV_ERR("invalid name_len, name_len(%u)\n", name_len);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (info->id_type != SHR_ID_NOTIFY_TYPE) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    devId = info->devid;
    notifyId = info->shrid;
    tsId = info->tsid;
    info->id_type = NOTIFY_ID_TYPE;
    if ((tsId >= DEVDRV_MAX_TS_NUM) || (devId >= ASCEND_DEV_MAX_NUM) || (notifyId >= DEVDRV_MAX_NOTIFY_ID)) {
        DEVDRV_DRV_ERR("invalid parameter, devId(%u) tsId(%u) notifyId(%u).\n", devId, tsId, notifyId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (NOTIFY_ID_TYPE == IPC_EVENT_TYPE) {
        ret = tsDrvShrIdAllocIpcEventId(devId, tsId, &info->shrid, info->flag, &value);
        if (ret != 0) {
            return ret;
        }
    }

    ret = __drvIpcNotifyCreat(name, info);
    if (ret != 0) {
        if (NOTIFY_ID_TYPE == IPC_EVENT_TYPE) {
            in.type = DRV_NOTIFY_ID;
            in.resourceId = info->shrid | IPC_EVENT_MASK;
            in.tsId = tsId;
            in.res[RESOURCEID_RESV_FLAG] = info->flag;
            (void)halResourceIdFree(devId, &in);
        }
        DEVDRV_DRV_ERR("Create ipcnotify failed. (devId=%u; ret=%d).\n", devId, ret);
        return ret;
    }

    info->shrid |= (NOTIFY_ID_TYPE == IPC_EVENT_TYPE) ? IPC_EVENT_MASK : 0;
    return DRV_ERROR_NONE;
}

drvError_t halShrIdDestroy(const char *name)
{
    struct drvIpcNotifyInfo notifyInfo = {0};
    return drvDestroyIpcNotify(name, &notifyInfo);
}

drvError_t halShrIdOpen(const char *name, struct drvShrIdInfo *info)
{
    struct drvIpcNotifyInfo notifyInfo = {0};
    drvError_t ret;

    if (info == NULL) {
        DEVDRV_DRV_ERR("Info is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    notifyInfo.devId = info->devid;
    notifyInfo.tsId = info->tsid;
    ret = _drvOpenIpcNotify(name, &notifyInfo, info->flag);
    if (ret == DRV_ERROR_NONE) {
        info->devid = notifyInfo.devId;
        info->tsid = notifyInfo.tsId;
        info->shrid = notifyInfo.notifyId;
    }
    return ret;
}

drvError_t halShrIdClose(const char *name)
{
    struct drvIpcNotifyInfo notify_info = {0};
    return drvCloseIpcNotify(name, &notify_info);
}

drvError_t halShrIdSetPid(const char *name, pid_t pid[], uint32_t pid_num)
{
    return drvSetIpcNotifyPid(name, pid, pid_num);
}

drvError_t halShrIdRecord(const char *name)
{
    return drvRecordIpcNotify(name);
}
#endif
#endif
