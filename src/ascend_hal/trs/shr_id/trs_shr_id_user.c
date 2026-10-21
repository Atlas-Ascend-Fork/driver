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
#include "ascend_hal.h"
#include "pbl_uda_user.h"

#include "trs_shr_id_fd.h"
#include "trs_shr_id_ioctl.h"
#include "trs_user_pub_def.h"
#include "trs_shr_id_user.h"

int trs_res_reg_offset_query(uint32_t dev_id, uint32_t ts_id, drvIdType_t type, uint32_t id, uint32_t *value);
int trs_res_reg_total_size_query(uint32_t dev_id, uint32_t ts_id, drvIdType_t type, uint32_t *value);

struct trs_shrid_remote_ops g_shrid_remote_ops;
void trs_register_shrid_remote_ops(struct trs_shrid_remote_ops *ops)
{
    g_shrid_remote_ops = *ops;
}

static struct trs_shrid_remote_ops *trs_get_shrid_remote_ops(void)
{
    return &g_shrid_remote_ops;
}

static drvError_t shrid_proc(uint32_t cmd, const char *name, struct shr_id_ioctl_info *ioctl_info)
{
    struct shr_id_ioctl_info tmp_info = {0};
    size_t len;
    int ret;

    if (name == NULL) {
        trs_err("Param invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    len = strnlen(name, SHR_ID_NSM_NAME_SIZE);
    if ((len == 0) || (len == SHR_ID_NSM_NAME_SIZE)) {
        trs_err("Invalid name. (len=%zu)\n", len);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = memcpy_s(tmp_info.name, SHR_ID_NSM_NAME_SIZE, name, len + 1);
    if (ret != 0) {
        trs_err("Memcpy failed. (len=%zu; ret=%d)\n", len, ret);
        return DRV_ERROR_INNER_ERR;
    }

    if ((ioctl_info->flag & TSDRV_FLAG_REMOTE_ID) != 0) {
        tmp_info.flag |= TSDRV_FLAG_REMOTE_ID;
        tmp_info.opened_devid = ioctl_info->opened_devid;
    }

    if (shrid_ioctl(cmd, &tmp_info) != DRV_ERROR_NONE) {
        return DRV_ERROR_IOCRL_FAIL;
    }

    ioctl_info->opened_devid = tmp_info.opened_devid;
    ioctl_info->devid = tmp_info.devid;
    ioctl_info->tsid = tmp_info.tsid;
    ioctl_info->id_type = tmp_info.id_type;
    ioctl_info->shr_id = tmp_info.shr_id;
    ioctl_info->flag = tmp_info.flag;
    ioctl_info->enable_flag = tmp_info.enable_flag;
    ioctl_info->rsv[0] = tmp_info.rsv[0];

    return DRV_ERROR_NONE;
}

static drvError_t shrid_create(char *name, uint32_t dev_id, struct drvShrIdInfo *info)
{
    struct shr_id_ioctl_info ioctl_info = {0};
    int ret;

    ioctl_info.devid = dev_id;
    ioctl_info.shr_id = info->shrid;
    ioctl_info.tsid = info->tsid;
    ioctl_info.id_type = info->id_type;
    ioctl_info.flag = info->flag;
    if (shrid_ioctl(SHR_ID_CREATE, &ioctl_info) != DRV_ERROR_NONE) {
        return DRV_ERROR_IOCRL_FAIL;
    }
    ret = strcpy_s(name, SHR_ID_NSM_NAME_SIZE, ioctl_info.name);
    if (ret != 0) {
        trs_err("strcpy failed, ret(%d).\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static drvError_t info_param_check(struct drvShrIdInfo *info)
{
    if (info == NULL) {
        trs_err("Para invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((info->devid >= TRS_DEV_NUM) || (info->tsid >= TRS_TS_NUM)) {
        trs_err("Param invalid. (devid=%u; tsid=%u)\n", info->devid, info->tsid);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

drvError_t halShrIdCreate(struct drvShrIdInfo *info, char *name, uint32_t name_len)
{
    uint32_t udev_id;
    int ret;

    if ((info_param_check(info) != DRV_ERROR_NONE) || (name == NULL) || (name_len < SHR_ID_NSM_NAME_SIZE)) {
        trs_err("Param invalid. (len=%u)\n", name_len);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = uda_get_udevid_by_devid(info->devid, &udev_id);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Get vdevid failed. (devid=%u)\n", info->devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    return shrid_create(name, udev_id, info);
}

drvError_t halShrIdDestroy(const char *name)
{
    struct shr_id_ioctl_info tmp_info = {0};
    return shrid_proc(SHR_ID_DESTROY, name, &tmp_info);
}

static drvError_t trs_shrid_remote_config(uint32_t dev_id, struct drvShrIdInfo *info)
{
    if (trs_get_shrid_remote_ops()->shrid_config != NULL) {
        return trs_get_shrid_remote_ops()->shrid_config(dev_id, info);
    } else {
        trs_warn("Not support. (dev_id=%u; res_type=%u; flag=%u)\n", dev_id, info->id_type, info->shrid);
        return DRV_ERROR_NOT_SUPPORT;
    }
    return DRV_ERROR_NONE;
}

static drvError_t trs_shrid_remote_deconfig(uint32_t dev_id, struct drvShrIdInfo *info)
{
    if (trs_get_shrid_remote_ops()->shrid_deconfig != NULL) {
        return trs_get_shrid_remote_ops()->shrid_deconfig(dev_id, info);
    } else {
        trs_warn("Not support. (dev_id=%u; res_type=%u; flag=%u)\n", dev_id, info->id_type, info->shrid);
        return DRV_ERROR_NOT_SUPPORT;
    }
    return DRV_ERROR_NONE;
}

drvError_t halShrIdSetAttribute(const char *name, enum shrIdAttrType type, struct shrIdAttr attr)
{
    struct shr_id_ioctl_info ioctl_info = {0};
    drvError_t ret;
    size_t len;

    if (name == NULL) {
        trs_err("Invalid name when setting ipc notify attribute.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if ((type < 0) || (type >= SHR_ID_ATTR_TYPE_MAX) || (attr.enableFlag != SHRID_NO_WLIST_ENABLE)) {
        trs_debug("Access denied when setting ipc notify attribute.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    len = strnlen(name, SHR_ID_NSM_NAME_SIZE);
    if ((len == 0) || (len == SHR_ID_NSM_NAME_SIZE)) {
        trs_err("Invalid name when setting ipc notify attribute.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = memcpy_s(ioctl_info.name, SHR_ID_NSM_NAME_SIZE, name, len + 1);
    if (ret != EOK) {
        trs_err("An exception occurred when memcpy the name of ipc notify.\n");
        return DRV_ERROR_INNER_ERR;
    }

    return shrid_ioctl(SHR_ID_SET_ATTR, &ioctl_info);
}

drvError_t halShrIdGetAttribute(const char *name, enum shrIdAttrType type, struct shrIdAttr *attr)
{
    struct shr_id_ioctl_info ioctl_info = {0};
    drvError_t ret;

    if ((name == NULL) || (attr == NULL)) {
        trs_err("Invalid arguments when getting ipc notify info.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((type < 0) || (type >= SHR_ID_ATTR_TYPE_MAX)) {
        trs_debug("Access denied when setting ipc notify attribute.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = shrid_proc((uint32_t)SHR_ID_GET_ATTR, name, &ioctl_info);
    if (ret == DRV_ERROR_NONE) {
        attr->enableFlag = ioctl_info.enable_flag;
    }

    return ret;
}

drvError_t halShrIdInfoGet(const char *name, struct shrIdGetInfo *info)
{
    struct shr_id_ioctl_info ioctl_info = {0};
    drvError_t ret;

    if ((name == NULL) || (info == NULL)) {
        trs_err("Invalid arguments when getting ipc notify info.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = shrid_proc((uint32_t)SHR_ID_GET_INFO, name, &ioctl_info);
    if (ret == DRV_ERROR_NONE) {
        info->phyDevid = ioctl_info.devid;
    }

    return ret;
}

drvError_t halShrIdOpen(const char *name, struct drvShrIdInfo *info)
{
    struct shr_id_ioctl_info ioctl_info = {0};
    uint32_t dev_id;
    drvError_t err;

    DRV_CURRENT_API_SCOPE();

    if (info == NULL) {
        trs_err("Para invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((info->flag & TSDRV_FLAG_REMOTE_ID) != 0) {
        ioctl_info.flag |= TSDRV_FLAG_REMOTE_ID;
        ioctl_info.opened_devid = info->devid;
    }

    dev_id = info->devid; /* logic devid */
    err = shrid_proc((uint32_t)SHR_ID_OPEN, name, &ioctl_info);
    if (err == DRV_ERROR_NONE) {
        info->devid = ioctl_info.devid;
        info->tsid = ioctl_info.tsid;
        info->shrid = ioctl_info.shr_id;
        info->id_type = ioctl_info.id_type;
        info->flag = ioctl_info.flag; // for spod
        info->rsv[0] = ioctl_info.rsv[0];
        if ((ioctl_info.flag & TSDRV_FLAG_REMOTE_ID) != 0) {
            if (dev_id >= TRS_DEV_NUM) {
                (void)halShrIdClose(name);
                trs_err("Param invalid. (devid=%u)\n", dev_id);
                return DRV_ERROR_INVALID_VALUE;
            }
            trs_debug("Shr Info. (devid=%u; tsid=%u; shrid=%u; type=%d; flag=0x%x)\n", info->devid, info->tsid,
                      info->shrid, info->id_type, info->flag);
            err = trs_shrid_remote_config(dev_id, info);
            if (err != DRV_ERROR_NONE) {
                (void)halShrIdClose(name);
            }
        }
    }

    return err;
}

drvError_t halShrIdClose(const char *name)
{
    struct shr_id_ioctl_info ioctl_info = {0};
    drvError_t err;

    err = shrid_proc((uint32_t)SHR_ID_CLOSE, name, &ioctl_info);
    if (err == DRV_ERROR_NONE) {
        if ((ioctl_info.flag & TSDRV_FLAG_REMOTE_ID) != 0) {
            struct drvShrIdInfo info;

            if (ioctl_info.opened_devid >= TRS_DEV_NUM) {
                trs_err("Param invalid. (devid=%u)\n", ioctl_info.opened_devid);
                return DRV_ERROR_INVALID_VALUE;
            }

            info.devid = ioctl_info.devid;
            info.tsid = ioctl_info.tsid;
            info.shrid = ioctl_info.shr_id;
            info.id_type = ioctl_info.id_type;
            info.flag = ioctl_info.flag; /* for spod */
            trs_debug("Shr Info. (devid=%u; tsid=%u; shrid=%u; type=%d; flag=0x%x)\n", info.devid, info.tsid,
                      info.shrid, info.id_type, info.flag);
            return trs_shrid_remote_deconfig(ioctl_info.opened_devid, &info);
        }
    }

    return err;
}

drvError_t halShrIdSetPid(const char *name, pid_t pid[], uint32_t pid_num)
{
    struct shr_id_ioctl_info ioctl_info = {0};
    uint32_t i;
    int ret;
    size_t len;

    /* pass in only one parameter at a time in the current scene */
    if ((name == NULL) || (pid == NULL) || (pid_num != 1)) {
        trs_err("invalid parameter. (name_is_null=%d, pid_is_null=%d, num=%d)\n", (name == NULL), (pid == NULL),
                pid_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    len = strnlen(name, SHR_ID_NSM_NAME_SIZE);
    if ((len == 0) || (len == SHR_ID_NSM_NAME_SIZE)) {
        trs_err("Invalid name. (len=%zu).\n", len);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = memcpy_s(ioctl_info.name, SHR_ID_NSM_NAME_SIZE, name, len + 1);
    if (ret != EOK) {
        trs_err("Memcpy failed, (len=%zu; ret=%d).\n", len, ret);
        return DRV_ERROR_INNER_ERR;
    }

    for (i = 0; i < pid_num; i++) {
        ioctl_info.pid[i] = pid[i];
        trs_debug("PID[%u] = %d\n", i, pid[i]);
    }

    return shrid_ioctl(SHR_ID_SET_PID, &ioctl_info);
}

drvError_t halShrIdRecord(const char *name)
{
    struct shr_id_ioctl_info ioctl_info = {0};
    int ret;
    size_t len;

    if (name == NULL) {
        trs_err("invalid parameter. (name_is_null=%d)\n", (name == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    len = strnlen(name, SHR_ID_NSM_NAME_SIZE);
    if ((len == 0) || (len == SHR_ID_NSM_NAME_SIZE)) {
        trs_err("invalid name. (len=%zu).\n", len);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = memcpy_s(ioctl_info.name, SHR_ID_NSM_NAME_SIZE, name, len + 1);
    if (ret != EOK) {
        trs_err("memcpy failed, (len=%zu; ret=%d).\n", len, ret);
        return DRV_ERROR_INNER_ERR;
    }

    return shrid_ioctl(SHR_ID_RECORD, &ioctl_info);
}

#define SHR_ID_EVENT_MASK 0x8000
/* ipc notify old API */
/* Sunset interface: external statement has been removed from ascend_hal.h. */
drvError_t drvCreateIpcNotify(struct drvIpcNotifyInfo *info, char *name, unsigned int len)
{
    struct drvShrIdInfo tmp_info = {0};
    drvError_t err;

    if (info == NULL) {
        trs_err("Info is null pointer.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    tmp_info.devid = info->devId;
    tmp_info.tsid = info->tsId;
    tmp_info.id_type = SHR_ID_NOTIFY_TYPE;
    tmp_info.shrid = info->notifyId;
    err = halShrIdCreate(&tmp_info, name, len);
    if (err != 0) {
        return err;
    }

    if (SHR_ID_NOTIFY_TYPE == SHR_ID_EVENT_TYPE) {
        tmp_info.shrid |= SHR_ID_EVENT_MASK;
    }

    return DRV_ERROR_NONE;
}

/* Sunset interface: external statement has been removed from ascend_hal.h. */
drvError_t drvDestroyIpcNotify(const char *name, struct drvIpcNotifyInfo *info)
{
    (void)info;
    return halShrIdDestroy(name);
}

/* Sunset interface: external statement has been removed from ascend_hal.h. */
drvError_t drvOpenIpcNotify(const char *name, struct drvIpcNotifyInfo *info)
{
    struct drvShrIdInfo tmp_info = {0};
    drvError_t err;

    if (info == NULL) {
        trs_err("info is null pointer\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    tmp_info.devid = info->devId;
    tmp_info.tsid = info->tsId;
    err = halShrIdOpen(name, &tmp_info);
    if (err == DRV_ERROR_NONE) {
        info->devId = tmp_info.devid;
        info->tsId = tmp_info.tsid;
        info->notifyId = tmp_info.shrid;
        if (SHR_ID_NOTIFY_TYPE == SHR_ID_EVENT_TYPE) {
            info->notifyId |= SHR_ID_EVENT_MASK;
        }
    }

    return err;
}

drvError_t drvCloseIpcNotify(const char *name, struct drvIpcNotifyInfo *info)
{
    (void)info;
    return halShrIdClose(name);
}

/* Sunset interface: external statement has been removed from ascend_hal.h. */
drvError_t drvSetIpcNotifyPid(const char *name, pid_t pid[], int num)
{
    return halShrIdSetPid(name, pid, (uint32_t)num);
}

/* Sunset interface: external statement has been removed from ascend_hal.h. */
drvError_t drvRecordIpcNotify(const char *name)
{
    return halShrIdRecord(name);
}

/* Sunset interface: external statement has been removed from ascend_hal.h. */
drvError_t drvNotifyIdAddrOffset(uint32_t devId, struct drvNotifyInfo *info)
{
    uint32_t offset;
    drvError_t ret;

    if (info == NULL) {
        trs_err("Invalid info. (devid=%u)\n", devId);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = trs_res_reg_offset_query(devId, 0, DRV_NOTIFY_ID, info->notifyId, &offset);
    if (ret == DRV_ERROR_NONE) {
        info->devAddrOffset = offset;
    }

    return ret;
}

drvError_t halNotifyGetInfo(uint32_t devId, uint32_t tsId, uint32_t type, uint32_t *val)
{
    struct halResourceInfo info;
    drvError_t ret;

    if (val == NULL) {
        trs_err("Para val is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (type) {
        case DRV_NOTIFY_TYPE_TOTAL_SIZE:
            return trs_res_reg_total_size_query(devId, tsId, DRV_NOTIFY_ID, val);
        case DRV_NOTIFY_TYPE_NUM:
            ret = halResourceInfoQuery(devId, tsId, DRV_RESOURCE_NOTIFY_ID, &info);
            if (ret == DRV_ERROR_NONE) {
                *val = info.capacity;
            }
            return ret;
        default:
            trs_err("Invalid para. (type=%d; devid=%u; tsid=%u)\n", type, devId, tsId);
            return DRV_ERROR_INVALID_VALUE;
    }
}