/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "securec.h"
#include "devmng_common.h"
#include "devdrv_ioctl.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devmng_user_common.h"
#include "dms/dms_misc_interface.h"
#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "dms_device_info.h"
#include "davinci_interface.h"
#include "dsmi_common_interface.h"
#include "drv_devmng_adapt.h"
#include "hbm_ctrl.h"
#include "udis_user.h"
#include "dms_feature_user.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

drvError_t drv_get_memory_info(uint32_t devId, int32_t info_type, void *buf, unsigned int *size)
{
    int ret = DRV_ERROR_NOT_SUPPORT;

    switch (info_type) {
#ifdef CFG_FEATURE_QUERY_VA_INFO
        case INFO_TYPE_UCE_VA:
            ret = dev_ecc_config_get_va_info(devId, buf, size);
            break;
#endif
        case INFO_TYPE_SYS_COUNT:
            ret = dms_memory_get_hbm_ecc_syscnt(devId, buf, size);
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "drv_get_memory_info unsuccessful. (dev_id=%u; ret=%d; info_type=%d).\n",
                                     devId, ret, info_type);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC bool drv_get_memory_info_support(uint32_t devId)
{
#ifdef DRV_HOST
    return dms_has_feature_by_dms_feature_id(devId, DMS_MEMORY_INFO);
#else
    (void)devId;
#ifdef CFG_FEATURE_MEMORY_INFO
    return true;
#else
    return false;
#endif
#endif
}

drvError_t drv_get_memory_info_ex(uint32_t devId, int32_t info_type, int64_t *value)
{
    int ret = 0;
    struct devdrv_device_info dev_info = {0};

    if (!drv_get_memory_info_support(devId)) {
        DEVDRV_DRV_INFO("This version does not support get memory info.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (info_type) {
        case INFO_TYPE_CORE_NUM:
            ret = drvGetDevInfo(devId, &dev_info);
            if (ret != 0) {
                DEVDRV_DRV_ERR("Get device info failed. (devid=%u; ret=%d)\n", devId, ret);
                return ret;
            }
            *value = dev_info.hbm_num;
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

STATIC bool drv_is_ts_resource(u32 resource_type)
{
#ifdef CFG_FEATURE_TS_RESOURCE_FROM_US
    /* Query TS Information from User Space */
    return (resource_type <= DEVDRV_MAX_TS_INFO_TYPE);
#else
    (void)resource_type;
    return false;
#endif
}

STATIC int drv_resource_info_para_check(u32 devid, struct dsmi_resource_para *para, struct dsmi_resource_info *info)
{
    if (devid > VDAVINCI_MAX_VDEV_ID || info == NULL || para == NULL) {
        DEVDRV_DRV_ERR("Invalid parameter. (device_id=%u; info_is_null=%d; para_is_null=%d)\n", devid, (info == NULL),
                       (para == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    if (info->buf == NULL || info->buf_len > DEVDRV_MAX_PAYLOAD_LEN) {
        DEVDRV_DRV_ERR("Invalid input info. (device_id=%u; buf_is_null=%d; buf_len=%u; max_len=%u)\n", devid,
                       (info->buf == NULL), info->buf_len, DEVDRV_MAX_PAYLOAD_LEN);
        return DRV_ERROR_PARA_ERROR;
    }

    if (para->owner_type >= DEVDRV_MAX_OWNER_TYPE) {
        DEVDRV_DRV_ERR("Invalid input resource parameter. (device_id=%u; owner_type=%u)\n", devid, para->owner_type);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

STATIC int drv_ts_resource_info_query(u32 devid, struct dsmi_resource_para *para, struct dsmi_resource_info *info)
{
    drvResourceType_t resource_type[DRV_RESOURCE_NOTIFY_ID + 1] = {DRV_RESOURCE_STREAM_ID, DRV_RESOURCE_EVENT_ID,
                                                                   DRV_RESOURCE_NOTIFY_ID, DRV_RESOURCE_MODEL_ID};
    struct halResourceInfo resinfo = {0};
    u32 logic_devid = devid;
    int ret;

    if (((para->owner_type == DSMI_DEV_RESOURCE) && (devid >= VDAVINCI_VDEV_OFFSET)) ||
        ((para->owner_type == DSMI_VDEV_RESOURCE) && (devid < VDAVINCI_VDEV_OFFSET))) {
        DEVDRV_DRV_ERR("Invalid input resource parameter. (device_id=%u; owner_type=%u)\n", devid, para->owner_type);
        return DRV_ERROR_PARA_ERROR;
    }

    if (devid >= VDAVINCI_VDEV_OFFSET) {
        ret = drvDeviceGetIndexByPhyId(devid, &logic_devid);
        if (ret != 0) {
            DEVDRV_DRV_ERR("Get logic devid failed. (phyDevid=%u)\n", devid);
            return ret;
        }
    }
    ret = halResourceInfoQuery(logic_devid, para->tsid, resource_type[para->resource_type], &resinfo);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Resource info query failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    ret = memcpy_s(info->buf, info->buf_len, (void *)&(resinfo.capacity), sizeof(resinfo.capacity));
    if (ret != 0) {
        DEVDRV_DRV_ERR("Memcpy to buffer failed. (device_id=%u; ret=%d)\n", devid, ret);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return 0;
}

STATIC int drv_get_resource_info_ioctl(u32 devid, struct dsmi_resource_para *para, struct dsmi_resource_info *info)
{
    struct devdrv_resource_info d_info = {0};
    int ret;

    d_info.devid = devid;
    d_info.owner_type = para->owner_type;
    d_info.owner_id = para->owner_id;
    d_info.resource_type = para->resource_type;
    d_info.tsid = para->tsid;
    d_info.buf_len = info->buf_len;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_DEV_RESOURCE_INFO, &d_info);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (device_id=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    ret = memcpy_s(info->buf, info->buf_len, d_info.buf, d_info.buf_len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Memcpy to buffer failed. (device_id=%u; ret=%d)\n", devid, ret);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return 0;
}

STATIC int udis_get_resource_info(u32 devid, struct dsmi_resource_para *para, struct dsmi_resource_info *info)
{
    int ret = 0;
    struct udis_dev_info udis_info = {0};

    if (para->owner_type == DSMI_PROCESS_RESOURCE && para->resource_type == DSMI_DEV_PROCESS_MEM) {
#ifdef CFG_FEATURE_APM_SUPP_PID
        udis_info.module_type = UDIS_MODULE_APM;
#else
        udis_info.module_type = UDIS_MODULE_SVM;
#endif
        ret = sprintf_s(udis_info.name, UDIS_MAX_NAME_LEN, "%08xmem", para->owner_id);
        if (ret < 0) {
            DEVDRV_DRV_ERR("Failed to invoke sprintf_s to copy info name. (device_id=%u; ret=%d)\n", devid, ret);
            return DRV_ERROR_PARA_ERROR;
        }
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = halUdisGetDeviceInfo(devid, &udis_info);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get udis info. (dev_id=%u; module_type=%u; name=%s; ret=%d)\n",
                                     devid, udis_info.module_type, udis_info.name, ret);
        return ret;
    }

    if (udis_info.data_len != info->buf_len) {
        DEVDRV_DRV_ERR(
            "Data len is not equal to bufsize(dev_id=%u; module_type=%u; name=%s; data_len=%u, buf_size=%u)\n", devid,
            udis_info.module_type, udis_info.name, udis_info.data_len, info->buf_len);
        return DRV_ERROR_PARA_ERROR;
    }

    if (*(unsigned long *)(udis_info.data) == 0) {
        DEVDRV_DRV_WARN("udis get process memory is zero. (device_id=%u; ret=%d)\n", devid, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = memcpy_s(info->buf, info->buf_len, udis_info.data, udis_info.data_len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke memcpy_s to copy data. (dev_id=%u; module_type=%u; name=%s; ret=%d)\n", devid,
                       udis_info.module_type, udis_info.name, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    DEVDRV_DRV_DEBUG("success udis get resource info. (dev_id=%u; name=%s)\n", devid, udis_info.name);
    return 0;
}

drvError_t drvGetDeviceResourceInfo(u32 devid, struct dsmi_resource_para *para, struct dsmi_resource_info *info)
{
    int ret = 0;

    ret = drv_resource_info_para_check(devid, para, info);
    if (ret != 0) {
        return ret;
    }

    ret = udis_get_resource_info(devid, para, info);
    if (ret == 0) {
        return ret;
    }

    if (drv_is_ts_resource(para->resource_type) && (para->owner_type < DSMI_PROCESS_RESOURCE)) {
        ret = drv_ts_resource_info_query(devid, para, info);
#ifdef CFG_FEATURE_APM_SUPP_PID
    } else if (para->owner_type == DSMI_PROCESS_RESOURCE) {
        ret = dms_get_process_resource(devid, para, info->buf, info->buf_len);
#endif
    } else {
        ret = drv_get_resource_info_ioctl(devid, para, info);
    }
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret,
                                     "Get resource info failed. (dev_id=%u; ret=%d; owner_type=%u; resource_type=%u)\n",
                                     devid, ret, para->owner_type, para->resource_type);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t halDrvGetDeviceResourceInfo(unsigned int devid, void *para, void *info)
{
    return drvGetDeviceResourceInfo((u32)devid, (struct dsmi_resource_para *)para, (struct dsmi_resource_info *)info);
}

drvError_t drv_get_svm_vdev_info_cmd(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                                     unsigned int *size)
{
    (void)main_cmd;
    return drvGetSvmVdevInfo(dev_id, sub_cmd, buf, size);
}

drvError_t drv_get_ts_drv_dev_com_info(struct tsdrv_dev_com_info *dev_com_info)
{
    mmIoctlBuf dev_info_buf = {0};
    int ret;

    drv_ioctl_param_init(&dev_info_buf, (void *)dev_com_info, sizeof(struct tsdrv_dev_com_info));
    ret = drv_common_ioctl(&dev_info_buf, DEVDRV_MANAGER_GET_TSDRV_DEV_COM_INFO);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed ret = %d.\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
