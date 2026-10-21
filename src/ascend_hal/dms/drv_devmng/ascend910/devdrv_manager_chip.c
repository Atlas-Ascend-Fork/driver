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
#include "mmpa_api.h"
#include "devdrv_ioctl.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devmng_user_common.h"
#include "ascend_hal.h"
#include "dms_device_info.h"
#include "dms/dms_misc_interface.h"
#include "drv_devmng_adapt.h"
#include "ascend_dev_num.h"
#include "dsmi_common_interface.h"
#include "dms_user_common.h"
#include "dms_feature_user.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

struct udis_core_util_info_adapter {
    unsigned int core_type;
    const char *name;
    bool enable_dmp;
    int (*callback)(unsigned int device_id, const char *name, unsigned int *result_data);
};

static struct udis_core_util_info_adapter g_udis_get_core_util_info_table[] = {
#ifdef CFG_FEATURE_UDIS_UTIL_FROM_TS
    {DEV_DRV_TYPE_AICORE, "aic_util", false, dms_get_util_from_udis},
    {DEV_DRV_TYPE_AIVECTOR, "aiv_util", false, dms_get_util_from_udis},
#endif
#if defined(CFG_FEATURE_SUPPORT_UDIS) && defined(DRV_HOST)
    {DEV_DRV_TYPE_AICPU, "aicpu_util", true, dms_get_cpu_rate_from_udis},
#endif
};

drvError_t drv_get_core_utilization(uint32_t dev_id, struct devdrv_core_utilization *utilization_info)
{
    struct devdrv_core_utilization util_info = {0};
    unsigned int i;
    int ret;
    unsigned int table_size = sizeof(g_udis_get_core_util_info_table) / sizeof(struct udis_core_util_info_adapter);

#ifdef DRV_HOST
    mmIoctlBuf util_info_buf = {0};
#endif

    if (utilization_info == NULL || utilization_info->core_type >= DEV_DRV_TYPE_MAX) {
        DEVDRV_DRV_ERR("Parameter is invalid. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < table_size; i++) {
        if (g_udis_get_core_util_info_table[i].core_type == utilization_info->core_type) {
            ret = g_udis_get_core_util_info_table[i].callback(dev_id, g_udis_get_core_util_info_table[i].name,
                                                              &utilization_info->utilization);
            if (ret == 0) {
                return ret;
            }
            if (!g_udis_get_core_util_info_table[i].enable_dmp) {
                DEVDRV_DRV_ERR("Failed to get davinci info from udis. (dev_id=%d; core_type=%d; ret=%d)\n", dev_id,
                               utilization_info->core_type, ret);
                return ret;
            }
        }
    }

#ifdef DRV_HOST
    util_info.dev_id = dev_id;
    util_info.core_type = utilization_info->core_type;
    drv_ioctl_param_init(&util_info_buf, (void *)&util_info, sizeof(struct devdrv_core_utilization));
#ifndef CFG_FEATURE_AIC_AIV_UTIL_FROM_TS
    if (utilization_info->core_type == DEV_DRV_TYPE_AIVECTOR) {
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif
    ret = drv_common_ioctl(&util_info_buf, DEVDRV_MANAGER_GET_CORE_UTILIZATION);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
#else
    if (utilization_info->core_type == DEV_DRV_TYPE_AICPU) {
        ret = dms_get_aicpu_utilization(dev_id, &(util_info.utilization));
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get aicpu utilization. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }
    } else if (utilization_info->core_type == DEV_DRV_TYPE_AICORE ||
               utilization_info->core_type == DEV_DRV_TYPE_AIVECTOR) {
#ifdef CFG_FEATURE_AIC_AIV_UTIL_FROM_TS
        ret = dms_get_average_util_from_ts(dev_id, 0, utilization_info->core_type, &(util_info.utilization));
#else
        if (utilization_info->core_type == DEV_DRV_TYPE_AICORE) {
            ret = DmsGetTsInfo(dev_id, 0, AICORE0_ID, &util_info.utilization, sizeof(unsigned int));
        } else {
            return DRV_ERROR_NOT_SUPPORT;
        }
#endif
        if (ret != 0) {
            DEVDRV_DRV_ERR("Failed to get utilization. (dev_id=%u; core_type=%u; ret=%d)\n", dev_id,
                           utilization_info->core_type, ret);
            return ret;
        }
    }

#endif
    utilization_info->utilization = util_info.utilization;

    return DRV_ERROR_NONE;
}

drvError_t drv_get_osc_freq(uint32_t devId, int32_t info_type, uint64_t *value)
{
#if defined CFG_FEATURE_OSC_FREQ && (defined(DRV_HOST) || defined(CFG_FEATURE_RC_MODE))
    int ret;
    unsigned int sub_cmd;
    int ioctl_cmd;
    struct dms_ioctl_arg ioarg = {0};
#ifndef CFG_FEATURE_SRIOV
    struct dms_osc_freq freq = {0};

    freq.dev_id = devId;
    freq.sub_cmd = (info_type == INFO_TYPE_HOST_OSC_FREQUE) ? DMS_SUBCMD_GET_HOST_OSC_FREQ :
                                                              DMS_SUBCMD_GET_DEV_OSC_FREQ;
    if (DmsGetVirtFlag() != 0) {
        ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_OSC_FREQ, &freq);
        if (ret != 0) {
            if (ret != DRV_ERROR_NOT_SUPPORT) {
                DEVDRV_DRV_ERR("Ioctl failed. (dev_id=%u; ret=%d; errno=%d)\n", devId, ret, errno);
            }
        } else {
            *value = freq.value;
        }
        return ret;
    }
#endif

    sub_cmd = (info_type == INFO_TYPE_HOST_OSC_FREQUE) ? DMS_SUBCMD_GET_HOST_OSC_FREQ : DMS_SUBCMD_GET_DEV_OSC_FREQ;
    ioctl_cmd = (info_type == INFO_TYPE_HOST_OSC_FREQUE) ? DMS_GET_OSC_FREQ_INFO_HOST : DMS_GET_OSC_FREQ_INFO_DEVICE;
    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = sub_cmd;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&devId;
    ioarg.input_len = sizeof(int);
    ioarg.output = (void *)value;
    ioarg.output_len = sizeof(uint64_t);
    ret = DmsIoctl(ioctl_cmd, &ioarg);
    if (ret != 0) {
        if ((ret != EOPNOTSUPP) && (ret != EAGAIN)) {
            DEVDRV_DRV_ERR("DmsIoctl failed. (devId=%u; ret=%d)\n", devId, ret);
        }
        return errno_to_user_errno(ret);
    }
    return DRV_ERROR_NONE;
#else
    (void)devId;
    (void)info_type;
    (void)value;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t drv_get_cc_info(uint32_t devId, int32_t info_type, void *buf, unsigned int *size)
{
    int ret;

    switch (info_type) {
        case INFO_TYPE_CC:
            ret = dms_get_cc_info(devId, buf, size);
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Get cc info unsuccessful. (dev_id=%u; ret=%d; info_type=%d).\n", devId, ret,
                                     info_type);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC bool drv_get_l2_info_support(uint32_t devId)
{
#ifdef DRV_HOST
    return dms_has_feature_by_dms_feature_id(devId, DMS_L2_BUFF_INFO);
#else
    (void)devId;
#ifdef CFG_FEATURE_L2_INFO
    return true;
#else
    return false;
#endif
#endif
}

drvError_t drv_get_l2_info(uint32_t devId, int32_t info_type, int64_t *value)
{
    int ret = 0;
    struct devdrv_device_info dev_info = {0};

    if (!drv_get_l2_info_support(devId)) {
        DEVDRV_DRV_INFO("This version does not support get l2 buf info.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (info_type) {
        case INFO_TYPE_CORE_NUM:
            ret = drvGetDevInfo(devId, &dev_info);
            if (ret != 0) {
                DEVDRV_DRV_ERR("Get device info failed. (devid=%u; ret=%d)\n", devId, ret);
                return ret;
            }
            *value = dev_info.l2_num;
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

drvError_t drv_get_l_2_buff_info(uint32_t devId, int32_t info_type, void *buf, unsigned int *size)
{
    int ret = 0;

    switch (info_type) {
        case INFO_TYPE_L2BUFF_RESUME_CNT:
            ret = DmsHalGetDeviceInfoEx(devId, MODULE_TYPE_L2BUFF, info_type, buf, size);
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Get l2buff info unsuccessful. (dev_id=%u; ret=%d; info_type=%d).\n", devId,
                                     ret, info_type);
    }
    return ret;
}

drvError_t drv_set_l_2_buff_info(uint32_t devId, int32_t info_type, void *buf, unsigned int size)
{
    int ret = 0;

    switch (info_type) {
        case INFO_TYPE_L2BUFF_RESUME:
            ret = DmsHalSetDeviceInfoEx(devId, MODULE_TYPE_L2BUFF, INFO_TYPE_L2BUFF_RESUME, buf, size);
            break;

        case INFO_TYPE_L2BUFF_INVALID_CACHE:
            ret = DmsHalSetDeviceInfoEx(devId, MODULE_TYPE_L2BUFF, INFO_TYPE_L2BUFF_INVALID_CACHE, buf, size);
            break;

        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to set l2buff info. (devId=%u, infoType=%d, ret=%d)\n", devId,
                                     info_type, ret);
    }
    return ret;
}

drvError_t hal_get_uuid_cmd(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                            unsigned int *size)
{
    (void)main_cmd;
    (void)sub_cmd;
    return halGetDeviceInfoByBuff(dev_id, MODULE_TYPE_SYSTEM, INFO_TYPE_UUID, buf, (int32_t *)size);
}

drvError_t hal_get_swplugin_upgrade(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                                    unsigned int *size)
{
    (void)main_cmd;
    (void)sub_cmd;
    int ret = 0;
    ret = halGetDeviceInfo(dev_id, MODULE_TYPE_SYSTEM, INFO_TYPE_SWPLUGIN_UPGRADE_POLICY, buf);
    if (ret != 0) {
        return ret;
    }

    *size = sizeof(unsigned int);
    return 0;
}

drvError_t halGetChipInfo(unsigned int devId, halChipInfo *chipInfo)
{
    int ret;
    struct dms_query_chip_info query_info = {0};

    if (chipInfo == NULL) {
        DEVDRV_DRV_ERR("Chip info is null. (dev_id=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = (int)drvCheckDevid(devId);
    if (ret != DRV_ERROR_NONE) {
        DEVDRV_DRV_ERR("Check device id failed. (devid=%u; ret=%d)\n", devId, ret);
        if (ret == DRV_ERROR_RESOURCE_OCCUPIED) {
            return DRV_ERROR_RESOURCE_OCCUPIED;
        }
        return DRV_ERROR_PARA_ERROR;
    }

    query_info.dev_id = devId;
    ret = DmsGetChipInfo(&query_info);
    if (ret != DRV_ERROR_NONE) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Get chip info failed. (dev_id=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    *chipInfo = *(halChipInfo *)(&query_info.info);
    return DRV_ERROR_NONE;
}

drvError_t halParseSDID(uint32_t sdid, struct halSDIDParseInfo *sdid_parse)
{
    if (sdid_parse == NULL) {
        DEVDRV_DRV_ERR("SDID parse pointer is null. (sdid=%u)\n", sdid);
        return DRV_ERROR_PARA_ERROR;
    }
    return dms_parse_sdid(sdid, sdid_parse);
}

drvError_t drvDeviceResetInform(uint32_t devid)
{
    int ret;
    mmIoctlBuf buf = {0};
    if (devid >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    drv_ioctl_param_init(&buf, (void *)&devid, sizeof(uint32_t));
    ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_DEVICE_RESET_INFORM);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "ioctl failed ret = %d.\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t halGetChipCount(int *chip_count)
{
#ifdef CFG_FEATURE_CHIP_DIE
    int ret;
    int count = 0;
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};

    if (chip_count == NULL) {
        DEVDRV_DRV_ERR("Chip count is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_COUNT, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, NULL, 0, (void *)&count, sizeof(int));
    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret == DRV_ERROR_RESOURCE_OCCUPIED) {
        DEVDRV_DRV_ERR("Ioctl failed, device is busy. (ret=%d)\n", ret);
        return ret;
    }
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (ret=%d)\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    if ((count < 0) || (count > DEVDRV_MAX_CHIP_NUM)) {
        DEVDRV_DRV_ERR("Chip count is invalid. (count=%u; max_count=%u)\n", count, DEVDRV_MAX_CHIP_NUM);
        return DRV_ERROR_INNER_ERR;
    }

    *chip_count = count;
    return DRV_ERROR_NONE;
#else
    (void)chip_count;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t halGetChipList(int chip_list[], int count)
{
#ifdef CFG_FEATURE_CHIP_DIE
    int ret, i;
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct devdrv_chip_list chip_list_info = {0};

    if ((count <= 0) || (count > DEVDRV_MAX_CHIP_NUM) || (chip_list == NULL)) {
        DEVDRV_DRV_ERR("Input chip para error. (count=%d; chip_list_is_null=%d)\n", count, chip_list == NULL);
        return DRV_ERROR_PARA_ERROR;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_LIST, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, NULL, 0, (void *)&chip_list_info, sizeof(struct devdrv_chip_list));
    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret == DRV_ERROR_RESOURCE_OCCUPIED) {
        DEVDRV_DRV_ERR("Ioctl failed, device is busy. (ret=%d)\n", ret);
        return ret;
    }
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (ret=%d)\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    if ((chip_list_info.count <= 0) || (chip_list_info.count > count)) {
        DEVDRV_DRV_ERR("ChipCount is invalid. (count=%u)\n", chip_list_info.count);
        return DRV_ERROR_INNER_ERR;
    }

    ret = memcpy_s(chip_list, (unsigned int)(count * sizeof(int)), chip_list_info.chip_list,
                   (unsigned int)(chip_list_info.count * sizeof(int)));
    if (ret != EOK) {
        DEVDRV_DRV_ERR("Memcpy_s fail. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    for (i = 0; i < count; i++) {
        DEVDRV_DRV_DEBUG("Chip list. (list[%d]=%d)\n", i, chip_list[i]);
    }

    return DRV_ERROR_NONE;
#else
    (void)chip_list;
    (void)count;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t halGetDeviceCountFromChip(int chip_id, int *device_count)
{
#ifdef CFG_FEATURE_CHIP_DIE
    int ret;
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct devdrv_chip_dev_list chip_dev_list = {0};

    if ((chip_id < 0) || (chip_id >= DEVDRV_MAX_CHIP_NUM) || (device_count == NULL)) {
        DEVDRV_DRV_ERR("Invalid Parameter. (chip_id=%d; device_count_is_null=%d)\n", chip_id, device_count == NULL);
        return DRV_ERROR_PARA_ERROR;
    }

    chip_dev_list.chip_id = chip_id;
    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEVICE_FROM_CHIP, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&chip_dev_list.chip_id, sizeof(int), (void *)&chip_dev_list,
                          sizeof(struct devdrv_chip_dev_list));
    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret == DRV_ERROR_RESOURCE_OCCUPIED) {
        DEVDRV_DRV_ERR("Ioctl failed, device is busy. (ret=%d)\n", ret);
        return ret;
    }
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (ret=%d)\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    if ((chip_dev_list.count <= 0) || (chip_dev_list.count > ASCEND_DEV_MAX_NUM)) {
        DEVDRV_DRV_ERR("Device count is invalid. (Device count=%u)\n", chip_dev_list.count);
        return DRV_ERROR_INNER_ERR;
    }

    *device_count = chip_dev_list.count;
    DEVDRV_DRV_DEBUG("Device count. (count=%d)\n", *device_count);
    return DRV_ERROR_NONE;
#else
    (void)chip_id;
    (void)device_count;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t halGetDeviceFromChip(int chip_id, int device_list[], int count)
{
#ifdef CFG_FEATURE_CHIP_DIE
    int ret, i;
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct devdrv_chip_dev_list chip_dev_list = {0};

    if ((device_list == NULL) || (chip_id < 0) || (chip_id >= DEVDRV_MAX_CHIP_NUM) || (count <= 0)) {
        DEVDRV_DRV_ERR("Input chip id or count or device list err. (chip id=%d; count=%d; device_list_is_null=%d)\n",
                       chip_id, count, device_list == NULL);
        return DRV_ERROR_PARA_ERROR;
    }

    chip_dev_list.chip_id = chip_id;
    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEVICE_FROM_CHIP, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&chip_dev_list.chip_id, sizeof(int), (void *)&chip_dev_list,
                          sizeof(struct devdrv_chip_dev_list));
    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret == DRV_ERROR_RESOURCE_OCCUPIED) {
        DEVDRV_DRV_ERR("Ioctl failed, device is busy.(ret=%d)\n", ret);
        return ret;
    }

    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (ret=%d)\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    if ((chip_dev_list.count <= 0) || (chip_dev_list.count > ASCEND_DEV_MAX_NUM) || (chip_dev_list.count > count)) {
        DEVDRV_DRV_ERR("Device Count is invalid. (DeviceCount=%u)\n", chip_dev_list.count);
        return DRV_ERROR_INNER_ERR;
    }

    ret = memcpy_s(device_list, (unsigned int)(count * sizeof(int)), chip_dev_list.dev_list,
                   (unsigned int)(chip_dev_list.count * sizeof(int)));
    if (ret != EOK) {
        DEVDRV_DRV_ERR("Memcpy_s fail. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    for (i = 0; i < chip_dev_list.count; i++) {
        DEVDRV_DRV_DEBUG("Device list[%d] is %d.\n", i, device_list[i]);
    }

    return DRV_ERROR_NONE;
#else
    (void)chip_id;
    (void)device_list;
    (void)count;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t halGetChipFromDevice(int device_id, int *chip_id)
{
#ifdef CFG_FEATURE_CHIP_DIE
    int ret;
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct devdrv_get_dev_chip_id chip_id_from_dev = {0};

    if ((chip_id == NULL) || (device_id < 0) || (device_id >= ASCEND_DEV_MAX_NUM)) {
        DEVDRV_DRV_ERR("Input Dev_id or chip_id invalid. (device_id=%d; chip_id_is_null=%d)\n", device_id,
                       (chip_id == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    chip_id_from_dev.dev_id = device_id;
    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_FROM_DEVICE, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&chip_id_from_dev, sizeof(struct devdrv_get_dev_chip_id),
                          (void *)&chip_id_from_dev, sizeof(struct devdrv_get_dev_chip_id));
    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret == DRV_ERROR_RESOURCE_OCCUPIED) {
        DEVDRV_DRV_ERR("Ioctl failed, device is busy. (ret=%d)\n", ret);
        return ret;
    }
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (ret=%d)\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    if ((chip_id_from_dev.chip_id < 0) || (chip_id_from_dev.chip_id >= DEVDRV_MAX_CHIP_NUM)) {
        DEVDRV_DRV_ERR("Output invalid. (device_id=%d; chip_id=%d)\n", device_id, chip_id_from_dev.chip_id);
        return ret;
    }

    *chip_id = chip_id_from_dev.chip_id;
    DEVDRV_DRV_DEBUG("Get chip_id. (device_id=%d; chip_id=%d)\n", device_id, *chip_id);
    return DRV_ERROR_NONE;
#else
    (void)device_id;
    (void)chip_id;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t hal_get_chip_from_device_cmd(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                                        unsigned int *size)
{
    int ret;
    (void)main_cmd;
    (void)sub_cmd;

    if (*size < sizeof(int)) {
        DEVDRV_DRV_ERR("Parameter is invalid. (dev_id=%u; size=%u)\n", dev_id, *size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = (int)halGetChipFromDevice((int)dev_id, (int *)buf);
    if (ret != 0) {
        return ret;
    }

    *size = sizeof(int);
    return DRV_ERROR_NONE;
}
