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
#include "dms/dms_devdrv_info_comm.h"
#include "dms/dms_misc_interface.h"
#include "ascend_hal.h"
#include "dms_device_info.h"
#include "ascend_dev_num.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

STATIC drvError_t drv_get_sys_info_by_phy_id(uint32_t phy_id, int32_t info_type, int64_t *value)
{
    int ret;
    unsigned int tmp_value;

    switch (info_type) {
        case PHY_INFO_TYPE_CHIPTYPE:
            ret = DmsGetChipType(phy_id, &tmp_value);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "drvGetDevInfo failed ret = %d.\n", ret);
                return ret;
            }
            *value = (int64_t)tmp_value;
            break;
        case PHY_INFO_TYPE_MASTER_ID:
            ret = DmsGetMasterDevInTheSameOs(phy_id, &tmp_value);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to obtain the master device. (phy_id=%u; ret=%d)\n", phy_id,
                                             ret);
                return ret;
            }
            *value = (int64_t)tmp_value;
            break;
        case PHY_INFO_TYPE_PHY_CHIP_ID:
        case PHY_INFO_TYPE_PHY_DIE_ID:
            ret = dms_get_phy_dev_info(phy_id, info_type, &tmp_value);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to obtain physical device info. (phy_id=%u; ret=%d)\n",
                                             phy_id, ret);
                return ret;
            }
            *value = (int64_t)tmp_value;
            break;
        default:
            DEVDRV_DRV_INFO("Invalid parameter. (info_type=%d; phy_id=%u)\n", info_type, phy_id);
            return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t halGetPhyDeviceInfo(uint32_t phyId, int32_t moduleType, int32_t infoType, int64_t *value)
{
    int ret;
    DEV_MODULE_TYPE mtype = moduleType;
    DEV_INFO_TYPE itype = infoType;

    if (phyId >= ASCEND_DEV_MAX_NUM || value == NULL) {
        DEVDRV_DRV_ERR("invalid parameter, value is NULL or phy_id(%u) is invalid\n", phyId);
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (mtype) {
        case MODULE_TYPE_SYSTEM:
            ret = drv_get_sys_info_by_phy_id(phyId, itype, value);
            break;

        default:
            DEVDRV_DRV_INFO("Invalid parameter. (moduleType=%d)\n", mtype);
            return DRV_ERROR_INVALID_VALUE;
    }
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Get device info failed. (ret=%d; module_type=%d; info_type=%d; phy_id=%u)\n",
                                     ret, mtype, itype, phyId);
        return ret;
    }
    return DRV_ERROR_NONE;
}

#ifdef DRV_HOST
#define DMS_DEVICE_NUM_PER_BOARD 8
STATIC drvError_t dms_get_device_board_location_is_supported(uint32_t devId)
{
    static const char *supported_chips[] = {"950"};
    uint32_t supported_chip_num = sizeof(supported_chips) / sizeof(supported_chips[0]);
    halChipInfo chipInfo;
    uint32_t mode, i;
    int ret;

    ret = halGetChipInfo(devId, &chipInfo);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to get chip info. (dev_id=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    for (i = 0; i < supported_chip_num; i++) {
        if (strstr((const char *)chipInfo.name, supported_chips[i]) != NULL) {
            break;
        }
    }
    if (i == supported_chip_num) {
        DEVDRV_DRV_DEBUG("(dev_id=%u; name=%s)\n", devId, chipInfo.name);
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = halGetDeviceSplitMode(devId, &mode);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to get device split mode. (dev_id=%u; ret=%d)\n", devId, ret);
        return ret;
    }
    if (mode != VMNG_NORMAL_NONE_SPLIT_MODE) {
        DEVDRV_DRV_DEBUG("(dev_id=%u; split_mode=%d)\n", devId, mode);
        return DRV_ERROR_NOT_SUPPORT;
    }

    DEVDRV_DRV_DEBUG("(dev_id=%u; name=%s; split_mode=%d)\n", devId, chipInfo.name, mode);
    return DRV_ERROR_NONE;
}

STATIC drvError_t dms_get_device_board_location(uint32_t devId, uint32_t otherDevId, int *board_location)
{
    unsigned int phy_devid = 0;
    unsigned int phy_other_devid = 0;
    drvError_t ret;

    ret = dms_get_device_board_location_is_supported(devId);
    if (ret != 0) {
        return ret;
    }
    ret = dms_get_device_board_location_is_supported(otherDevId);
    if (ret != 0) {
        return ret;
    }

    ret = halGetPhyDevIdByLogicDevId(devId, &phy_devid);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to get phy dev id. (dev_id=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    ret = halGetPhyDevIdByLogicDevId(otherDevId, &phy_other_devid);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to get phy dev id. (dev_id_other=%u; ret=%d)\n", otherDevId, ret);
        return ret;
    }

    *board_location = (phy_devid / DMS_DEVICE_NUM_PER_BOARD == phy_other_devid / DMS_DEVICE_NUM_PER_BOARD) ?
                          LOCATION_IN_BOARD :
                          LOCATION_CROSS_BOARD;
    return DRV_ERROR_NONE;
}
#endif

drvError_t halGetPairDevicesInfo(uint32_t devId, uint32_t otherDevId, int32_t infoType, int64_t *value)
{
#ifdef DRV_HOST
    int ret;
    int topology_type = 0;

    if (value == NULL) {
        DEVDRV_DRV_ERR("Input value is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((drvCheckDevid(devId) != 0) || (drvCheckDevid(otherDevId) != 0)) {
        DEVDRV_DRV_ERR("Device id check failed. (devId=%u; other_dev_id=%u)\n", devId, otherDevId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (infoType == DEVS_INFO_TYPE_TOPOLOGY) {
        ret = dms_get_device_topology(devId, otherDevId, &topology_type);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Get device topology failed. (dev_id=%u; dev_id_other=%u; ret=%d)\n",
                                         devId, otherDevId, ret);
            return ret;
        }

        *value = topology_type;
        DEVDRV_DRV_DEBUG("Get topology type success. (dev_id=%u; dev_id_other=%u)\n", devId, otherDevId);
    } else if (infoType == DEVS_INFO_TYPE_BOARD_LOCATION) {
        int board_location;
        ret = dms_get_device_board_location(devId, otherDevId, &board_location);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret,
                                         "Get device board location failed. (dev_id=%u; dev_id_other=%u; ret=%d)\n",
                                         devId, otherDevId, ret);
            return ret;
        }
        *value = (int64_t)board_location;
        DEVDRV_DRV_DEBUG("Get board location success. (dev_id=%u; dev_id_other=%u; location=%d)\n", devId, otherDevId,
                         board_location);
    } else {
        DEVDRV_DRV_ERR("Invalid info type. (info_type=%d)\n", infoType);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
#else
    (void)devId;
    (void)otherDevId;
    (void)infoType;
    (void)value;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t halGetPairPhyDevicesInfo(uint32_t devId, uint32_t otherDevId, int32_t infoType, int64_t *value)
{
#if (defined CFG_FEATURE_PHY_DEVICES_TOPO) && (defined DRV_HOST)
    int ret;
    int topology_type = 0;

    if (value == NULL) {
        DEVDRV_DRV_ERR("Input value is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (infoType == DEVS_INFO_TYPE_TOPOLOGY) {
        ret = dms_get_phy_devices_topology(devId, otherDevId, &topology_type);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret,
                                         "Failed to get devices topology info."
                                         "(dev_id=%u; dev_id_other=%u; ret=%d)\n",
                                         devId, otherDevId, ret);
            return ret;
        }

        *value = topology_type;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
#else
    (void)devId;
    (void)otherDevId;
    (void)infoType;
    (void)value;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}
