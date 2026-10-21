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
#include "ascend_hal.h"
#include "dms_device_info.h"
#include "dms_user_common.h"
#include "ascend_inpackage_hal.h"
#include "udis_user.h"
#include "dms/dms_drv_internal.h"
#include "devdrv_manager_run_info.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define DMS_SDK_EX_VERSION_LEN_MAX 128

STATIC drvError_t drv_get_sdk_ex_version(unsigned int dev_id, void *buf, unsigned int *size)
{
    int ret;
    char sdk_ex_ver[DMS_SDK_EX_VERSION_LEN_MAX + 1] = {0};
    unsigned int ver_len = DMS_SDK_EX_VERSION_LEN_MAX;

    if (*size == 0) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev_id=%u; size=%u)\n", dev_id, *size);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = DmsHalGetDeviceInfoEx(dev_id, MODULE_TYPE_SYSTEM, INFO_TYPE_SDK_EX_VERSION, sdk_ex_ver, &ver_len);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get SDK Ex version. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (ver_len == 0) {
        *size = 0;
        return DRV_ERROR_NONE;
    }

    if (*size < ver_len || ver_len > DMS_SDK_EX_VERSION_LEN_MAX) {
        DEVDRV_DRV_ERR("The buff len is too small. (size=%d; ver_len=%d; buf_len_max=%u)\n", *size, ver_len,
                       DMS_SDK_EX_VERSION_LEN_MAX);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = memcpy_s(buf, *size, sdk_ex_ver, ver_len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to memcpy_s. (ret=%d; dev_id=%u; size=%u)\n", ret, dev_id, *size);
        return DRV_ERROR_INNER_ERR;
    }
    *size = ver_len;

    return DRV_ERROR_NONE;
}

STATIC drvError_t drv_get_udis_cpu_topo(unsigned int dev_id, char *name, void *buf, unsigned int *size)
{
    int ret;
    struct udis_dev_info udis_info = {0};
    if (*size == 0) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev_id=%u; size=%u)\n", dev_id, *size);
        return DRV_ERROR_INVALID_VALUE;
    }
    udis_info.module_type = UDIS_MODULE_DEVMNG;
    ret = strcpy_s(udis_info.name, UDIS_MAX_NAME_LEN, name);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to strcpy_s name. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INNER_ERR;
    }
    ret = halUdisGetDeviceInfo(dev_id, &udis_info);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get cpu topo. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    if (udis_info.data_len > *size) {
        DEVDRV_DRV_ERR("Size is insufficient. (dev_id=%u; data_len=%u; size=%u)\n", dev_id, udis_info.data_len, *size);
        return DRV_ERROR_INVALID_VALUE;
    }
    ret = memcpy_s(buf, *size, udis_info.data, udis_info.data_len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to memcpy_s data. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INNER_ERR;
    }
    *size = udis_info.data_len;
    return DRV_ERROR_NONE;
}

drvError_t drv_get_cpu_topo(unsigned int dev_id, void *buf, unsigned int *size)
{
    int ret;
    unsigned int cpu_topo_num = 0;
    unsigned int cpu_topo_num_size = sizeof(unsigned int);
    unsigned int i = 0;
    struct halCpuTopologyInfo cpu_topo = {0};
    struct halSingleCpuTopologyInfo single_cpu = {0};
    unsigned int single_cpu_size = sizeof(struct halSingleCpuTopologyInfo);
    char single_cpu_name[UDIS_MAX_NAME_LEN];

    if (*size < sizeof(struct halCpuTopologyInfo) || buf == NULL) {
        DEVDRV_DRV_ERR("Size is invalid or buf is NULL. (dev_id=%u; data_len=%u; size=%u, buf_is_null=%d)\n", dev_id,
                       sizeof(struct halCpuTopologyInfo), *size, buf == NULL);
        return DRV_ERROR_INVALID_VALUE;
    }
    ret = drv_get_udis_cpu_topo(dev_id, "cpu_topo_num", &cpu_topo_num, &cpu_topo_num_size);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get cpu topo num. (dev_id=%u; ret=%d; cpu_topo_num=%u)\n", dev_id,
                                     ret, cpu_topo_num);
        return ret;
    }
    if (cpu_topo_num == 0 || cpu_topo_num > HAL_MAX_CPU_TOPO_NUM) {
        DEVDRV_DRV_ERR("Invalid cpu topo num. (dev_id=%u; cpu_topo_num=%u)\n", dev_id, cpu_topo_num);
        return DRV_ERROR_INVALID_VALUE;
    }
    cpu_topo.total_nums = cpu_topo_num;
    for (; i < cpu_topo_num; i++) {
        ret = sprintf_s(single_cpu_name, UDIS_MAX_NAME_LEN, "cpu_topo_%u", i);
        if (ret < 0) {
            DEVDRV_DRV_ERR("Cpu topo %u sprintf_s failed. (dev_id=%u; ret=%d)\n", i, dev_id, ret);
            return DRV_ERROR_INNER_ERR;
        }
        ret = drv_get_udis_cpu_topo(dev_id, single_cpu_name, &single_cpu, &single_cpu_size);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get cpu topo %u. (dev_id=%u; ret=%d)\n", i, dev_id, ret);
            return ret;
        }
        cpu_topo.single_cpu_topo_info[i] = single_cpu;
    }
    ret = memcpy_s(buf, *size, &cpu_topo, sizeof(struct halCpuTopologyInfo));
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to memcpy_s data. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INNER_ERR;
    }
    *size = sizeof(struct halCpuTopologyInfo);
    return DRV_ERROR_NONE;
}

drvError_t drv_get_system_info_ex(uint32_t devId, int32_t info_type, void *buf, unsigned int *size)
{
    int ret = DRV_ERROR_NOT_SUPPORT;

    switch (info_type) {
        case INFO_TYPE_SDK_EX_VERSION:
            ret = drv_get_sdk_ex_version(devId, buf, size);
            break;
        case INFO_TYPE_UUID:
            ret = DmsHalGetDeviceInfoEx(devId, MODULE_TYPE_SYSTEM, info_type, buf, size);
            break;
        case INFO_TYPE_CPU_TOPO:
            ret = drv_get_cpu_topo(devId, buf, size);
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to set get version info. (devId=%u; infoType=%d; ret=%d)\n", devId,
                                     info_type, ret);
    }

    return ret;
}

STATIC drvError_t drv_set_sdk_ex_version(unsigned int dev_id, const void *buf, unsigned int size)
{
    int ret;
    char set_buf[DMS_SDK_EX_VERSION_LEN_MAX + 1] = {0};

    if ((size == 0) || (size > DMS_SDK_EX_VERSION_LEN_MAX)) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev_id=%u; size=%u; buf_len_max=%u)\n", dev_id, size,
                       DMS_SDK_EX_VERSION_LEN_MAX);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = memcpy_s(set_buf, DMS_SDK_EX_VERSION_LEN_MAX + 1, buf, size);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke memcpy_s. (dev_id=%u; size=%u)\n", dev_id, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = DmsHalSetDeviceInfoEx(dev_id, MODULE_TYPE_SYSTEM, INFO_TYPE_SDK_EX_VERSION, set_buf, size);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to set SDK Ex version. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    DEVDRV_DRV_EVENT("Set SDK Ex version [%s] success. (dev_id=%u)\n", set_buf, dev_id);

    return DRV_ERROR_NONE;
}

STATIC drvError_t drv_event_resume(unsigned int dev_id, const void *buf, unsigned int size)
{
#ifdef CFG_EDGE_HOST
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct dms_filter_st filter = {0};
    struct dms_hal_device_info_stru in = {0};
    struct dms_hal_device_info_stru out = {0};
    int ret;

    if ((buf == NULL) || (size != sizeof(struct hal_fault_event_resume))) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev_id=%u; buf=%s; size=%u; expect_size=%u)\n", dev_id,
                       (buf == NULL) ? "NULL" : "OK", size, sizeof(struct hal_fault_event_resume));
        return DRV_ERROR_INVALID_VALUE;
    }

    in.dev_id = dev_id;
    in.module_type = MODULE_TYPE_SYSTEM;
    in.info_type = INFO_TYPE_EVENT_RESUME;
    in.buff_size = size;
    ret = memcpy_s((void *)in.payload, sizeof(struct hal_fault_event_resume), buf, size);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Memcpy buf to payload failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INNER_ERR;
    }

    DMS_MAKE_UP_FILTER_HAL_DEV_INFO_EX(&filter, MODULE_TYPE_SYSTEM, INFO_TYPE_EVENT_RESUME);
    urd_usr_cmd_fill(&cmd, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, &filter.filter[0], filter.filter_len);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&in, sizeof(struct dms_hal_device_info_stru), (void *)&out,
                          sizeof(struct dms_hal_device_info_stru));
    ret = urd_dev_usr_cmd(dev_id, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Resume fault event failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    DEVDRV_DRV_EVENT("Resume fault event success. (dev_id=%u)\n", dev_id);

    return DRV_ERROR_NONE;
#else
    (void)dev_id;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t drv_set_system_info_ex(uint32_t devId, int32_t info_type, void *buf, unsigned int size)
{
    int ret = DRV_ERROR_NOT_SUPPORT;

    switch (info_type) {
        case INFO_TYPE_SDK_EX_VERSION:
            ret = drv_set_sdk_ex_version(devId, buf, size);
            break;

        case INFO_TYPE_EVENT_RESUME:
            ret = drv_event_resume(devId, buf, size);
            break;

        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to set system info. (dev_id=%u; infoType=%d; ret=%d; size=%u)\n",
                                     devId, info_type, ret, size);
    }

    return ret;
}

drvError_t DmsSetSWPluginUpgrade(unsigned int dev_id, unsigned int sub_cmd, const void *in_buff, unsigned int buf_len)
{
    (void)sub_cmd;
    return DmsHalSetDeviceInfoEx(dev_id, MODULE_TYPE_SYSTEM, INFO_TYPE_SWPLUGIN_UPGRADE_POLICY, in_buff, buf_len);
}

drvError_t DmsGetSWPluginUpgrade(unsigned int dev_id, void *in_buff)
{
    unsigned int size = sizeof(unsigned int);
    return DmsHalGetDeviceInfoEx(dev_id, MODULE_TYPE_SYSTEM, INFO_TYPE_SWPLUGIN_UPGRADE_POLICY, in_buff, &size);
}

drvError_t halGetAPIVersion(int *halAPIVersion)
{
    if (halAPIVersion == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }

    *halAPIVersion = __HAL_API_VERSION;

    return DRV_ERROR_NONE;
}

#ifndef DEVDRV_UT
int g_runtime_api_ver;
void halSetRuntimeApiVer(int Version)
{
    g_runtime_api_ver = Version;
}

int drvGetRuntimeApiVer(void)
{
    return g_runtime_api_ver;
}
#endif
