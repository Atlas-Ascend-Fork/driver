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
#include "mmpa_api.h"
#include "devmng_common.h"
#include "devdrv_ioctl.h"
#include "dms/dms_devdrv_info_comm.h"
#include "davinci_interface.h"
#include "dms_user_interface.h"
#include "drv_user_common.h"
#include "devdrv_manager_system_info.h"
#include "dms_device_info.h"
#include "dms_user_common.h"
#include "devmng_user_common.h"
#include "ascend_dev_num.h"
#include "drv_devmng_adapt.h"
#include "pbl/pbl_urd_user.h"
#include "dms/dms_misc_interface.h"

#include "devdrv_manager_chip.h"

#ifndef __linux
#pragma comment(lib, "libc_sec.lib")
#define PTHREAD_MUTEX_INITIALIZER NULL
#define DEVDRV_BB_DEVICE_ID_INFORM 0x66020004
#define DEVDRV_BB_DEVICE_STATE_INFORM 0x66020008
#define fd_is_invalid(fd) (fd == (mmProcess)DEVDRV_INVALID_FD_OR_INDEX)
#else
#include <sys/prctl.h>
#include <errno.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/ioctl.h>
#include "devdrv_user_common.h"
#include "dms/dms_drv_internal.h"
#define DAVINCI_COMMON_DRV_NAME "asdrv_pbl"
#define DAVINCI_COMMON_VDRV_NAME "asdrv_vpbl"
#define fd_is_invalid(fd) ((fd) < 0)
#endif

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define DAVINCI_INTF_INVALID_STATUS (-1)
#define INVALID_VF_DEVICE_ID 0xFFFFFFFF

drvError_t drvDeviceStatus(uint32_t devId, drvStatus_t *status)
{
#ifdef CFG_FEATURE_TRS_HB_REFACTOR
    unsigned int dev_status = DRV_STATUS_RESERVED;
    int ret;

    if (devId >= ASCEND_DEV_MAX_NUM || status == NULL) {
        DEVDRV_DRV_ERR("Parameter is invalid. (devId=%u; status_is_null=%d)\n", devId, (status == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = DmsDeviceInitStatus(devId, &dev_status);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Get device init status failed. (devId=%u, ret=%d)\n", devId, ret);
        return ret;
    }

    if (dev_status == DRV_STATUS_WORK) {
        /* DC has one Ts module, tsid = 0. */
        ret = DmsTsHeartbeatStatus(devId, 0, 0, &dev_status);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Get ts hb status failed. (devId=%u, ret=%d)\n", devId, ret);
            return ret;
        }
    }

    if (dev_status >= DRV_STATUS_RESERVED) {
        DEVDRV_DRV_ERR("Invalid status. (devId=%u, dev_status=%d)\n", devId, dev_status);
        return DRV_ERROR_INVALID_VALUE;
    }

    *status = (drvStatus_t)dev_status;
    return DRV_ERROR_NONE;

#else
    int para;
    int ret;

    mmIoctlBuf buf = {0};
    if (devId >= ASCEND_DEV_MAX_NUM || status == NULL) {
        DEVDRV_DRV_ERR("Parameter is invalid. (devId=%u; status_is_null=%d)\n", devId, (status == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    *status = DRV_STATUS_RESERVED;
    para = (int)devId;
    drv_ioctl_param_init(&buf, (void *)&para, sizeof(int));

    ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_DEVICE_STATUS);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed devId = %u, ret = %d.\n", devId, ret);
        return ret;
    }

    if (para < 0 || para >= DRV_STATUS_RESERVED) {
        DEVDRV_DRV_ERR("invalid status(%d).\n", para);
        return DRV_ERROR_INVALID_VALUE;
    }
    *status = (drvStatus_t)para;

    return DRV_ERROR_NONE;
#endif
}

drvError_t drvGetDeviceSpec(uint32_t devId, drvSpec_t *drvspec)
{
    struct devdrv_hardware_inuse inuse;
    struct devdrv_hardware_spec spec;
    int ret;
    mmIoctlBuf spec_buf = {0};
    mmIoctlBuf inuse_buf = {0};

    if (devId >= ASCEND_DEV_MAX_NUM || drvspec == NULL) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev_id=%u; drvspec_is_null=%d)\n", devId, (drvspec == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = memset_s((void *)drvspec, sizeof(drvSpec_t), 0, sizeof(drvSpec_t));
    if (ret != EOK) {
        DEVDRV_DRV_ERR("memset_s failed. (ret=%d; dev_id=%u)\n", ret, devId);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    spec.devid = (u32)devId;
    drv_ioctl_param_init(&spec_buf, (void *)&spec, sizeof(struct devdrv_hardware_spec));

    ret = drv_common_ioctl(&spec_buf, DEVDRV_MANAGER_GET_CORE_SPEC);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (dev_id=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    inuse.devid = (u32)devId;
    drv_ioctl_param_init(&inuse_buf, (void *)&inuse, sizeof(struct devdrv_hardware_inuse));

    ret = drv_common_ioctl(&inuse_buf, DEVDRV_MANAGER_GET_CORE_INUSE);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (dev_id=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    drvspec->aiCoreNum = spec.ai_core_num;
    drvspec->aiCpuNum = spec.ai_cpu_num;
    drvspec->aiCoreNumLevel = spec.ai_core_num_level;
    drvspec->aiCoreFreqLevel = spec.ai_core_freq_level;
    drvspec->aiCoreInuse = inuse.ai_core_num;
    drvspec->aiCoreErrorMap = inuse.ai_core_error_bitmap;
    drvspec->aiCpuInuse = inuse.ai_cpu_num;
    drvspec->aiCpuErrorMap = inuse.ai_cpu_error_bitmap;

    return DRV_ERROR_NONE;
}

drvError_t vdevmng_get_dev_probe_num(uint32_t *num)
{
    uint32_t dev_num = 0;
    int ret;

    if (num == NULL) {
        DEVDRV_DRV_ERR("Num is null.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_PROBE_NUM, &dev_num);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "dmanage_common_ioctl failed. (ret=%d)\n", ret);
        return ret;
    }

    *num = dev_num;
    return DRV_ERROR_NONE;
}

drvError_t drvGetDevProbeNum(uint32_t *num)
{
    if (DmsGetVirtFlag()) {
        return vdevmng_get_dev_probe_num(num);
    } else {
        return DmsGetDevProbeNum(num);
    }
}

drvError_t halGetDevProbeList(uint32_t *devices, uint32_t len)
{
    return DmsGetDevProbeList(devices, len);
}

STATIC int devdrv_check_vdevid(unsigned int devId)
{
#ifdef CFG_FEATURE_SRIOV
    int ret;
    uint32_t i;
    uint32_t vdev_num = 0;
    uint32_t *vdevids = NULL;

    ret = halGetVdevNum(&vdev_num);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Get vf device num failed. (vdev_num=%u; ret=%d)\n", vdev_num, ret);
        return ret;
    }
    if (vdev_num == 0) {
        DEVDRV_DRV_ERR("Invalid device id. (devId=%d; vdev_num=%u)\n", devId, vdev_num);
        return DRV_ERROR_INVALID_DEVICE;
    }

    vdevids = (uint32_t *)malloc(sizeof(uint32_t) * vdev_num);
    if (vdevids == NULL) {
        DEVDRV_DRV_ERR("malloc failed. (devId=%d)\n", devId);
        return DRV_ERROR_MALLOC_FAIL;
    }
    ret = memset_s(vdevids, vdev_num * sizeof(uint32_t), INVALID_VF_DEVICE_ID, vdev_num * sizeof(uint32_t));
    if (ret != 0) {
        free(vdevids);
        vdevids = NULL;
        DEVDRV_DRV_ERR("vdevids memset fail. (dev_id=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    ret = halGetVdevIDs(vdevids, vdev_num);
    if (ret != 0) {
        free(vdevids);
        vdevids = NULL;
        DEVDRV_DRV_ERR("Get vf device id failed. (vdev_num=%u; ret=%d)\n", vdev_num, ret);
        return ret;
    }
    for (i = 0; i < vdev_num; i++) {
        if (devId == vdevids[i]) {
            free(vdevids);
            vdevids = NULL;
            return DRV_ERROR_NONE;
        }
    }

    free(vdevids);
    vdevids = NULL;
#endif
    DEVDRV_DRV_ERR("Invalid device id. (devId=%d)\n", devId);
    return DRV_ERROR_INVALID_DEVICE;
}

drvError_t drvCheckDevid(unsigned int devId)
{
    int ret;
    unsigned int num_dev = 0, i;
    unsigned int devids[DEVDRV_PF_DEV_MAX_NUM];

    if (devId >= DEVDRV_PF_DEV_MAX_NUM) {
        return devdrv_check_vdevid(devId);
    }

    ret = drvGetDevNum(&num_dev);
    if (ret != 0 || num_dev > DEVDRV_PF_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("Get device num failed. (num_dev=%u; ret=%d)\n", num_dev, ret);
        return ret;
    }
#ifndef DEVDRV_UT
    ret = drvGetDevIDs(devids, num_dev);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Get device list failed. (num_dev=%u; ret=%d)\n", num_dev, ret);
        return ret;
    }
    for (i = 0; i < num_dev; i++) {
        if (devids[i] == devId) {
            return DRV_ERROR_NONE;
        }
    }
    DEVDRV_DRV_ERR("Invalid device id. (devId=%d; num_dev=%u)\n", devId, num_dev);
#endif
    return DRV_ERROR_INVALID_DEVICE;
}

drvError_t drv_get_container_dev_ids(uint32_t *devices, uint32_t len, uint32_t *num)
{
    struct devdrv_manager_devids dev_info = {0};
    mmIoctlBuf dev_info_buf = {0};
    uint32_t min_len;
    uint32_t i;
    int ret;

    if (devices == NULL || len > ASCEND_DEV_MAX_NUM || num == NULL) {
        DEVDRV_DRV_ERR("Parameter is invalid. (len=%u; devices_is_null=%d; num_is_null=%d)\n", len, (devices == NULL),
                       (num == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    drv_ioctl_param_init(&dev_info_buf, (void *)&dev_info, sizeof(struct devdrv_manager_devids));
    ret = drv_common_ioctl(&dev_info_buf, DEVDRV_MANAGER_GET_CONTAINER_DEVIDS);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed ret = %d.\n", ret);
        return ret;
    }

    if (dev_info.num_dev > ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("wrong num_dev(%u) exceeds max(%u)\n", dev_info.num_dev, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_INVALID_DEVICE;
    }

    min_len = dev_info.num_dev > len ? len : dev_info.num_dev;
    for (i = 0; i < min_len; i++) {
        devices[i] = dev_info.devids[i];
    }

    *num = min_len;

    return DRV_ERROR_NONE;
}

drvError_t drv_get_h2d_dev_info(uint32_t devId, struct devdrv_device_info *info)
{
#ifdef CFG_FEATURE_DEVMNG_IOCTL
    mmIoctlBuf dev_info_buf = {0};
#else
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
#endif
    struct devdrv_manager_hccl_devinfo dev_info = {0};
    int ret, i;

    if (devId >= ASCEND_DEV_MAX_NUM || info == NULL) {
        DEVDRV_DRV_ERR("Parameter is invalid. (devId=%u; info_is_null=%d)\n", devId, (info == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

#ifdef CFG_FEATURE_DEVMNG_IOCTL
    dev_info.dev_id = devId;
    drv_ioctl_param_init(&dev_info_buf, (void *)&dev_info, sizeof(struct devdrv_manager_hccl_devinfo));
    ret = drv_common_ioctl(&dev_info_buf, DEVDRV_MANAGER_GET_H2D_DEVINFO);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed ret = %d.\n", ret);
        return ret;
    }
#else
    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_H2D_DEV_INFO, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&devId, sizeof(uint32_t), (void *)&dev_info,
                          sizeof(struct devdrv_manager_hccl_devinfo));
    ret = urd_dev_usr_cmd_ex(devId, DMS_GET_H2D_DEV_INFO, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (ret=%d; devId=%u)\n", ret, devId);
        return ret;
    }
#endif

    info->ai_core_num = dev_info.ai_core_num;
    info->ai_core_freq = (unsigned int)dev_info.aicore_freq;
    info->ai_cpu_core_num = dev_info.ai_cpu_core_num;
    info->ctrl_cpu_ip = dev_info.ctrl_cpu_ip;
    info->ctrl_cpu_id = dev_info.ctrl_cpu_id;
    info->ctrl_cpu_core_num = dev_info.ctrl_cpu_core_num;
    info->ctrl_cpu_occupy_bitmap = dev_info.ctrl_cpu_occupy_bitmap;
    info->ctrl_cpu_endian_little = dev_info.ctrl_cpu_endian_little;
    info->ts_cpu_core_num = dev_info.ts_cpu_core_num;
    info->env_type = dev_info.env_type;
    info->aicpu_occupy_bitmap = dev_info.ai_cpu_bitmap;
    info->ai_core_id = dev_info.ai_core_id;
    info->ai_cpu_core_id = dev_info.ai_cpu_core_id;
    info->hardware_version = dev_info.hardware_version;
    info->ts_num = dev_info.ts_num;
    info->cpu_system_count = dev_info.cpu_system_count;
    info->monotonic_raw_time_ns = dev_info.monotonic_raw_time_ns;
    info->real_time_ms = dev_info.real_time_ms;
    info->ffts_type = dev_info.ffts_type;
    for (i = 0; i < DEVDRV_MAX_COMPUTING_POWER_TYPE; i++) {
        info->computing_power[i] = dev_info.computing_power[i];
    }

    return DRV_ERROR_NONE;
}

drvError_t drv_get_info_from_dev_info(uint32_t devId, int32_t info_type, int64_t *value)
{
    int ret;
    struct devdrv_device_info info = {0};

    ret = drvGetDevInfo(devId, &info);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Get device info failed. (dev_id=%u; infotype=%d; ret=%d)\n", devId, info_type, ret);
        return ret;
    }

    switch (info_type) {
        case INFO_TYPE_ENV:
            *value = info.env_type;
            break;

        case INFO_TYPE_VERSION:
            *value = info.hardware_version;
            break;

        case INFO_TYPE_CORE_NUM:
            *value = info.ts_num;
            break;

        case INFO_TYPE_PHY_CHIP_ID:
            *value = info.chip_id;
            break;

        case INFO_TYPE_PHY_DIE_ID:
            *value = info.die_id;
            break;

        case INFO_TYPE_ADDR_MODE:
            *value = info.addr_mode;
            break;

        case INFO_TYPE_MAINBOARD_ID:
            *value = info.mainboard_id;
            break;
        case INFO_TYPE_PRODUCT_TYPE:
            *value = info.product_type;
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

drvError_t drv_get_system_info(uint32_t devId, int32_t info_type, int64_t *value)
{
    int ret;
    uint64_t osc_freq = 0;
    unsigned int tmp = 0;
    unsigned int tmp_len = sizeof(tmp);
    struct devdrv_device_info info = {0};

    switch (info_type) {
        case INFO_TYPE_ENV:
        case INFO_TYPE_CORE_NUM:
        case INFO_TYPE_ADDR_MODE:
            return drv_get_info_from_dev_info(devId, info_type, value);
        case INFO_TYPE_PHY_CHIP_ID:
        case INFO_TYPE_PHY_DIE_ID:
#ifdef CFG_FEATURE_SDID_INFO_FROM_SYSTOPO
            return dms_get_spod_item(devId, info_type, value);
#else
            return drv_get_info_from_dev_info(devId, info_type, value);
#endif
        case INFO_TYPE_MAINBOARD_ID:
#ifdef CFG_FEATURE_HW_INFO_FROM_BIOS
            return drv_get_info_from_dev_info(devId, info_type, value);
#else
            return DRV_ERROR_NOT_SUPPORT;
#endif
        case INFO_TYPE_PRODUCT_TYPE:
#ifdef CFG_FEATURE_PRODUCT_TYPE
            return drv_get_info_from_dev_info(devId, info_type, value);
#else
            return DRV_ERROR_NOT_SUPPORT;
#endif
        case INFO_TYPE_VERSION:
            return drv_get_info_type_version_adapt(devId, info_type, value);

        case INFO_TYPE_MASTERID:
            ret = DmsGetMasterDevInTheSameOs(devId, &tmp);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "drvGetMasterDeviceInTheSameOS failed. (dev_id=%u; ret=%d)\n", devId,
                                             ret);
                return ret;
            }

            *value = tmp;
            break;

        case INFO_TYPE_SYS_COUNT:
        case INFO_TYPE_MONOTONIC_RAW:
        case INFO_TYPE_REAL_TIME:
            ret = drv_get_h2d_dev_info(devId, &info);
            if (ret != 0) {
                DEVDRV_DRV_ERR("drvGetDevInfo failed. (dev_id=%u; ret=%d)\n", devId, ret);
                return ret;
            }
            *value = (int64_t)(info_type == INFO_TYPE_SYS_COUNT     ? info.cpu_system_count :
                               info_type == INFO_TYPE_MONOTONIC_RAW ? info.monotonic_raw_time_ns :
                                                                      info.real_time_ms);
            break;

        case INFO_TYPE_HOST_OSC_FREQUE:
        case INFO_TYPE_DEV_OSC_FREQUE:
            ret = drv_get_osc_freq(devId, info_type, &osc_freq);
            if (ret != 0) {
                return ret;
            }

            *value = (int64_t)osc_freq;
            break;
        case INFO_TYPE_SDID:
        case INFO_TYPE_SERVER_ID:
        case INFO_TYPE_SCALE_TYPE:
        case INFO_TYPE_SUPER_POD_ID:
            return dms_get_spod_item(devId, info_type, value);
        case INFO_TYPE_CHASSIS_ID:
#ifdef CFG_FEATURE_SUPPORT_CHASSIS_ID
            return dms_get_spod_item(devId, info_type, value);
#else
            return DRV_ERROR_NOT_SUPPORT;
#endif
        case INFO_TYPE_SUPER_POD_TYPE:
#ifdef CFG_FEATURE_SUPPORT_SPOD_TYPE
            return dms_get_spod_item(devId, info_type, value);
#else
            return DRV_ERROR_NOT_SUPPORT;
#endif
        case INFO_TYPE_SUPER_POD_INTERCON_TYPE:
#ifdef CFG_FEATURE_SUPPORT_SPOD_INTERCON_TYPE
            return dms_get_spod_item(devId, info_type, value);
#else
            return DRV_ERROR_NOT_SUPPORT;
#endif
        case INFO_TYPE_RUN_MACH:
            ret = drvCheckDevid(devId);
            if (ret != 0) {
                DEVDRV_DRV_ERR("Check device id failed. (dev_id=%u; ret=%d)\n", devId, ret);
                return ret;
            }
#ifdef DRV_HOST
            ret = drvGetHostPhyMachFlag(devId, &tmp);
            if (ret != 0) {
                DEVDRV_DRV_ERR("Get host physical machine flag failed. (dev_id=%u; ret=%d).\n", devId, ret);
                return ret;
            }

            *value = (tmp == DEVDRV_HOST_PHY_MACH_FLAG) ? RUN_MACHINE_PHYCICAL : RUN_MACHINE_VIRTUAL;
#else
            *value = RUN_MACHINE_PHYCICAL;
#endif
            break;

        case INFO_TYPE_CUST_OP_ENHANCE:
            ret = DmsHalGetDeviceInfoEx(devId, MODULE_TYPE_SYSTEM, info_type, &tmp, &tmp_len);
            if ((ret != 0) || (tmp_len != sizeof(tmp))) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get cust op enhance. (dev_id=%u; ret=%d; tmp_len=%u)\n",
                                             devId, ret, tmp_len);
                return ret;
            }
            *value = tmp;

            break;

        case INFO_TYPE_HD_CONNECT_TYPE:
#ifdef CFG_FEATURE_HD_COMNNECT_TYPE
            ret = drvGetDevInfo(devId, &info);
            if (ret != 0) {
                DEVDRV_DRV_ERR("Get device info failed. (dev_id=%u; ret=%d)\n", devId, ret);
                return ret;
            }
            *value = info.host_device_connect_type;
            break;
#else
            return DRV_ERROR_NOT_SUPPORT;
#endif
        case INFO_TYPE_BOARD_ID:
            ret = DmsGetBoardId(devId, &tmp);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get board id. (dev_id=%u; ret=%d)\n", devId, ret);
                return ret;
            }
            *value = tmp;
            break;

        case INFO_TYPE_VNIC_IP:
            ret = devdrv_get_vnic_ip(devId, &tmp);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get vnic ip. (dev_id=%u; ret=%d)\n", devId, ret);
                return ret;
            }
            *value = tmp;
            break;

        case INFO_TYPE_SPOD_VNIC_IP:
            ret = devdrv_get_vnic_ip_by_sdid(devId, &tmp);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get vnic ip by sdid. (dev_id=%u; ret=%d)\n", devId, ret);
                return ret;
            }
            *value = tmp;
            break;

        case INFO_TYPE_SLOT_ID:
#ifdef DRV_HOST
            ret = dms_get_basic_info_host(devId, &tmp, DMS_SUBCMD_GET_SLOT_ID_HOST, sizeof(unsigned int));
#else
            ret = (int)DmsGetSlotId(devId, &tmp);
#endif
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get slot id. (dev_id=%u; ret=%d)\n", devId, ret);
                return ret;
            }
            *value = tmp;
            break;
        case INFO_TYPE_SWPLUGIN_UPGRADE_POLICY:
            ret = (int)DmsGetSWPluginUpgrade(devId, &tmp);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get swplugin upgrade policy. (dev_id=%u; ret=%d)\n", devId,
                                             ret);
                return ret;
            }
            *value = tmp;
            break;
        default:
            DEVDRV_DRV_INFO("This version does not support this type. (dev_id=%u; Type=%d)\n", devId, info_type);
            return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

#ifdef CFG_SOC_PLATFORM_CLOUD
drvError_t drvGetDeviceModuleStatus(uint32_t devId, drvModuleStatus_t *moduleStatus)
{
    struct devdrv_module_status status = {0};
    int ret;
    mmIoctlBuf buf = {0};

    if (devId >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", devId);
        return DRV_ERROR_INVALID_VALUE;
    }

    status.devid = devId;
#else
drvError_t drvGetDeviceModuleStatus(drvModuleStatus_t *moduleStatus)
{
    struct devdrv_module_status status = {0};
    int ret;
    mmIoctlBuf buf = {0};

    status.devid = 0;
#endif

    if (moduleStatus == NULL) {
        DEVDRV_DRV_ERR("moduleStatus is NULL.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = memset_s((void *)moduleStatus, sizeof(drvModuleStatus_t), 0, sizeof(drvModuleStatus_t));
    if (ret != EOK) {
        DEVDRV_DRV_ERR("memset_s failed, ret(%d).\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    drv_ioctl_param_init(&buf, (void *)&status, sizeof(struct devdrv_module_status));
    ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_GET_MODULE_STATUS);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed ret = %d.\n", ret);
        return ret;
    }

    /* cloud hasn't lpm3, value about lpm3 always be 0 */
    moduleStatus->ai_core_error_bitmap = status.ai_core_error_bitmap;
    moduleStatus->lpm3_start_fail = status.lpm3_start_fail;
    moduleStatus->lpm3_lost_heart_beat = status.lpm3_lost_heart_beat;
    moduleStatus->ts_start_fail = status.ts_start_fail;
    moduleStatus->ts_lost_heart_beat = status.ts_lost_heart_beat;
    moduleStatus->ts_sram_broken = status.ts_sram_broken;
    moduleStatus->ts_sdma_broken = status.ts_sdma_broken;
    moduleStatus->ts_bs_broken = status.ts_bs_broken;
    moduleStatus->ts_l2_buf0_broken = status.ts_l2_buf0_broken;
    moduleStatus->ts_l2_buf1_broken = status.ts_l2_buf1_broken;
    moduleStatus->ts_spcie_broken = status.ts_spcie_broken;
    moduleStatus->ts_ai_core_broken = status.ts_ai_core_broken;
    moduleStatus->ts_hwts_broken = status.ts_hwts_broken;
    moduleStatus->ts_doorbell_broken = status.ts_doorbell_broken;

    return DRV_ERROR_NONE;
}

drvError_t halGetOnlineDevList(unsigned int *devBuf, unsigned int bufCnt, unsigned int *devCnt)
{
    int ret;
    int i = 0;
    mmIoctlBuf buf = {0};
    unsigned int dev_buf_tmp[ASCEND_DEV_MAX_NUM] = {0};
    unsigned int valid_dev_num = 0;

    if ((devBuf == NULL) || (devCnt == NULL)) {
        DEVDRV_DRV_ERR("input para is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    /* set buf to invalid value before ioctl */
    ret = memset_s(dev_buf_tmp, sizeof(dev_buf_tmp), ASCEND_DEV_MAX_NUM, sizeof(dev_buf_tmp));
    if (ret != EOK) {
        DEVDRV_DRV_ERR("memset_s failed, ret=%d.\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    drv_ioctl_param_init(&buf, (void *)dev_buf_tmp, sizeof(unsigned int) * ASCEND_DEV_MAX_NUM);
    ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_GET_BOOT_DEV_ID);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed ret = %d.\n", ret);
        return ret;
    }

    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        if (dev_buf_tmp[i] < ASCEND_DEV_MAX_NUM) {
            DEVDRV_DRV_INFO("online dev id is %u.\n", dev_buf_tmp[i]);
            valid_dev_num++;
        } else {
            break;
        }
    }

    if (bufCnt < valid_dev_num) {
        DEVDRV_DRV_ERR("buf_cnt[%u] is shorter than actual dev_num[%u].\n", bufCnt, valid_dev_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    *devCnt = valid_dev_num;
    ret = memcpy_s(devBuf, bufCnt * sizeof(unsigned int), dev_buf_tmp, *devCnt * sizeof(unsigned int));
    if (ret != EOK) {
        DEVDRV_DRV_ERR("buf_cnt[%u] is shorter than actual dev_num[%u].\n", bufCnt, valid_dev_num);
        return DRV_ERROR_NO_DEVICE;
    }

    return DRV_ERROR_NONE;
}

drvError_t halDevOnlinePollWait(void)
{
    int ret;
    mmProcess fd = -1;
    struct pollfd pollfds;

    fd = devdrv_open_device_manager();
    if (fd_is_invalid(fd)) {
        DEVDRV_DRV_ERR("open device manager failed, fd(%d).\n", fd);
        return DRV_ERROR_INVALID_HANDLE;
    }

    pollfds.fd = fd;
    pollfds.events = POLLIN;

    do {
        ret = poll(&pollfds, 1, -1);
        if (ret < 0) {
            if (errno != EINTR) {
                DEVDRV_DRV_ERR("polling interrupted! ret = %d, errno:%d.\n", ret, errno);
                return DRV_ERROR_INVALID_HANDLE;
            }
        } else if (ret == 0) {
            DEVDRV_DRV_ERR("polling time out! ret = %d\n", ret);
            return DRV_ERROR_WAIT_TIMEOUT;
        }
    } while ((ret < 0) && (errno == EINTR));

    return DRV_ERROR_NONE;
}

drvError_t drvMngGetConsoleLogLevel(unsigned int *logLevel)
{
    int error = 0;
    mmProcess fd;
    int ret;

    fd = devdrv_open_device_manager();
    if (fd_is_invalid(fd)) {
        DEVDRV_DRV_ERR("open device manager failed, fd = %d.\n", fd);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = ioctl(fd, DEVDRV_MANAGER_GET_CONSOLE_LOG_LEVEL, logLevel);
    if (ret != 0) {
        if (__errno_location() != NULL) {
            error = errno;
            DEVDRV_DRV_ERR("ioctl error, errno = %d.\n", error);
        }

        if (error == ERESTARTSYS) {
            return ERESTARTSYS;
        }

        return DRV_ERROR_IOCRL_FAIL;
    }

    return ret;
}

drvError_t drvGetDeviceStatus(unsigned int devId, unsigned int *device_status)
{
    int ret;
    mmIoctlBuf para_buf = {0};
    struct devdrv_device_work_status device_work_status = {0};

    if (devId >= ASCEND_DEV_MAX_NUM || device_status == NULL) {
        DEVDRV_DRV_ERR("Parameter is invalid. (devId=%u; status_is_null=%d)\n", devId, (device_status == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    device_work_status.device_id = devId;
    drv_ioctl_param_init(&para_buf, (void *)&device_work_status, sizeof(struct devdrv_device_work_status));
    ret = drv_common_ioctl(&para_buf, DEVDRV_MANAGER_GET_STARTUP_STATUS);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (dev_id=%u; ret=%d)\n", devId, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    *device_status = device_work_status.device_process_status;

    return DRV_ERROR_NONE;
}

drvError_t drvUpdateDeviceStatus(unsigned int dev_id, unsigned int device_status)
{
    int ret;
    mmIoctlBuf para_buf = {0};
    struct devdrv_device_work_status device_work_status = {0};
    device_work_status.device_id = dev_id;
    device_work_status.device_process_status = device_status;
    drv_ioctl_param_init(&para_buf, (void *)&device_work_status, sizeof(struct devdrv_device_work_status));
    ret = drv_common_ioctl(&para_buf, DEVDRV_MANAGER_UPDATE_STARTUP_STATUS);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl error, ret =%d\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvDeviceHealthStatus(uint32_t devId, unsigned int *healthStatus)
{
    int ret;
    mmIoctlBuf para_buf = {0};
    struct devdrv_device_health_status para = {0};

    if (devId >= ASCEND_DEV_MAX_NUM || healthStatus == NULL) {
        DEVDRV_DRV_ERR("Parameter is invalid. (devId=%u; status_is_null=%d)\n", devId, (healthStatus == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    para.device_id = devId;
    drv_ioctl_param_init(&para_buf, (void *)&para, sizeof(struct devdrv_device_health_status));
    ret = drv_common_ioctl(&para_buf, DEVDRV_MANAGER_GET_DEVICE_HEALTH_STATUS);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl error, ret =%d\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    *healthStatus = para.device_health_status;

    return DRV_ERROR_NONE;
}

drvError_t hal_get_ub_status_cmd(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                                 unsigned int *size)
{
    (void)main_cmd;
    (void)sub_cmd;
    return halGetDeviceInfoByBuff(dev_id, MODULE_TYPE_UB, INFO_TYPE_UB_STATUS, buf, (int32_t *)size);
}

drvError_t dms_get_ub_id_info(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                              unsigned int *size)
{
    (void)main_cmd;
    (void)sub_cmd;
    int ret = 0;
    struct dms_ubdev_id_info info = {0};

    if (*size != sizeof(struct dms_ubdev_id_info)) {
        DEVDRV_DRV_ERR("size is invalid. (size=%u, correct_size=%u)\n", *size, sizeof(struct dms_ubdev_id_info));
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dms_get_ub_dev_id_info(dev_id, &info);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Get ubdev_id fail. (dev_id=%u; ret=%d).\n", dev_id, ret);
        return ret;
    }

    ret = memcpy_s(buf, *size, &info, sizeof(struct dms_ubdev_id_info));
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke memcpy_s to copy ubdev_id data. (dev_id=%d; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvGetCommonDriverInitStatus(int *status)
{
    static int davinci_intf_status = DAVINCI_INTF_INVALID_STATUS;
    if (status == NULL) {
        DEVDRV_DRV_ERR("invalid input argument.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (davinci_intf_status == true) {
        *status = davinci_intf_status;
        return DRV_ERROR_NONE;
    }

    if ((dmanage_check_module_init(DAVINCI_COMMON_DRV_NAME) == 0) ||
        (dmanage_check_module_init(DAVINCI_COMMON_VDRV_NAME) == 0)) {
        davinci_intf_status = true;
    } else {
        davinci_intf_status = false;
    }

    *status = davinci_intf_status;
    return DRV_ERROR_NONE;
}

int halRegisterVmngClient(void)
{
#if (defined CFG_SOC_PLATFORM_MINIV2) || (defined CFG_SOC_PLATFORM_CLOUD)
    mmIoctlBuf buf = {0};
    int ret;
    int para;

    drv_ioctl_param_init(&buf, (void *)&para, sizeof(int));
    ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_REG_VMNG_CLIENT);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed ret = %d.\n", ret);
        return ret;
    }
#endif

    return DRV_ERROR_NONE;
}

int halGetDeviceVfMax(unsigned int devId, unsigned int *vf_max_num)
{
    mmIoctlBuf buf = {0};
    unsigned int para;
    int ret;

    if ((vf_max_num == NULL) || drvCheckDevid(devId) != 0) {
        DEVDRV_DRV_ERR("Parameter is invalid. (dev_id=%u; vf_max_num_is_null=%d)\n", devId, (vf_max_num == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    para = devId;
    drv_ioctl_param_init(&buf, (void *)&para, sizeof(unsigned int));
    ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_GET_DEVICE_VF_MAX);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (dev_id=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    *vf_max_num = para;

    return DRV_ERROR_NONE;
}

int halGetDeviceVfList(unsigned int devId, unsigned int *vf_list, unsigned int list_len, unsigned int *vf_num)
{
    struct virtmng_vf_list get_vf_list = {0};
    mmIoctlBuf buf = {0};
    int ret;
    u32 i;

    if ((vf_list == NULL) || (vf_num == NULL) || drvCheckDevid(devId) != 0) {
        DEVDRV_DRV_ERR("Parameter is invalid. (dev_id=%u; vf_list=%s; vf_num=%s)\n", devId,
                       vf_list == NULL ? "NULL" : "OK", vf_num == NULL ? "NULL" : "OK");
        return DRV_ERROR_PARA_ERROR;
    }

    get_vf_list.dev_id = devId;
    drv_ioctl_param_init(&buf, (void *)&get_vf_list, sizeof(struct virtmng_vf_list));
    ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_GET_DEVICE_VF_LIST);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (dev_id=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    if ((get_vf_list.vf_num > list_len) || (get_vf_list.vf_num > VDAVINCI_MAX_VFID_NUM)) {
        DEVDRV_DRV_ERR("Parameter is invalid. (dev_id=%u; list_len=%u; vf_num=%u)\n", devId, list_len,
                       get_vf_list.vf_num);
        return DRV_ERROR_PARA_ERROR;
    }

    for (i = 0; i < get_vf_list.vf_num; i++) {
        vf_list[i] = get_vf_list.vf_list[i];
    }
    *vf_num = get_vf_list.vf_num;

    return DRV_ERROR_NONE;
}

drvError_t drv_get_qos_info(uint32_t devId, int32_t info_type, void *buf, unsigned int *size)
{
#ifdef CFG_FEATURE_QUERY_QOS_CFG_INFO
    int ret;
    switch (info_type) {
        case INFO_TYPE_CONFIG:
            ret = drv_get_qos_config(devId, buf, size);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get qos config. (dev_id=%u; ret=%d)\n", devId, ret);
                return ret;
            }
            break;

#ifdef CFG_FEATURE_GET_QOS_MASTER_CFG
        case INFO_TYPE_QOS_MASTER_CONFIG:
            ret = (int)DmsHalGetDeviceInfoEx(devId, MODULE_TYPE_QOS, info_type, buf, size);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get master qos config. (dev_id=%u; ret=%d)\n", devId, ret);
                return ret;
            }
            break;
#endif
        default:
            DEVDRV_DRV_INFO("This version does not support this type. (dev_id=%u, Type=%d)\n", devId, info_type);
            return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
#else
    (void)devId;
    (void)info_type;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t drvGetDmpStarted(uint32_t devId, uint32_t *dmp_started)
{
    int ret;
    mmIoctlBuf para_buf = {0};
    struct devdrv_device_work_status device_work_status = {0};

    if (devId >= ASCEND_DEV_MAX_NUM || dmp_started == NULL) {
        DEVDRV_DRV_ERR("Parameter is invalid. (devId=%u; status_is_null=%d)\n", devId, (dmp_started == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    device_work_status.device_id = devId;
    drv_ioctl_param_init(&para_buf, (void *)&device_work_status, sizeof(struct devdrv_device_work_status));
    ret = drv_common_ioctl(&para_buf, DEVDRV_MANAGER_GET_STARTUP_STATUS);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (devid=%u; ret=%d)\n", devId, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    *dmp_started = device_work_status.dmp_started;

    return DRV_ERROR_NONE;
}
