/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

#include "ascend_hal.h"
#include "ascend_hal_error.h"
#include "utils.h"

#define MAX_DEV_NUM 64
#define SOC_VERSION_SIZE 32

int main(int argc, const char *argv[])
{
    int ret;
    int canAccessPeer = 0;
    unsigned int devId = 0;
    unsigned int devNum = 0;
    unsigned int phyId[2] = {0};
    char socVersion[SOC_VERSION_SIZE] = {0};
    unsigned int devArray[MAX_DEV_NUM] = {0};

    LOG_INFO("Start to run device manager sample.\n");

    ret = drvGetDevNum(&devNum);
    if (ret != 0) {
        LOG_ERR("Get device number failed. (ret=%d)\n", ret);
        return ret;
    }

    if (devNum > MAX_DEV_NUM) {
        LOG_ERR("Device number is invalid. (devNum=%u)\n", devNum);
        return DRV_ERROR_INVALID_DEVICE;
    }

    // Query logical device id array.
    ret = drvGetDevIDs(devArray, devNum);
    if (ret != 0) {
        LOG_ERR("Get device id array failed. (ret=%d)\n", ret);
        return ret;
    }

    // Query device soc version.
    ret = halGetSocVersion(devArray[0], socVersion, SOC_VERSION_SIZE);
    if (ret != 0) {
        LOG_ERR("Get soc version failed. (ret=%d)\n", ret);
        return ret;
    }

    LOG_INFO("Device soc version is %s.\n", socVersion);
    if (devNum <= 1) {
        LOG_INFO("Device number is %d, not support P2P.\n", devNum);
    } else {
        ret = drvDeviceGetPhyIdByIndex(devArray[0], &phyId[0]);
        if (ret != 0) {
            LOG_ERR("Get physical id from logical id failed. (ret=%d; dev_id=%u)\n", ret, devArray[0]);
            return ret;
        }

        ret = drvDeviceGetPhyIdByIndex(devArray[1], &phyId[1]);
        if (ret != 0) {
            LOG_ERR("Get physical id from logical id failed. (ret=%d; dev_id=%u)\n", ret, devArray[1]);
            return ret;
        }

        ret = halDeviceCanAccessPeer(&canAccessPeer, devArray[0], phyId[1]);
        if (ret != 0) {
            LOG_ERR("Check device data interaction failed. (ret=%d)\n", ret);
            return ret;
        }

        LOG_INFO("P2P access between device%u and device%u is %d.\n", devArray[0], devArray[1], canAccessPeer);
        if (canAccessPeer == 1) {
            ret = halDeviceEnableP2P(devArray[0], phyId[1], 0);
            if (ret != 0) {
                LOG_ERR("Enable P2P failed. (ret=%d)\n", ret);
                return ret;
            }

            ret = halDeviceEnableP2P(devArray[1], phyId[0], 0);
            if (ret != 0) {
                LOG_ERR("Enable P2P failed. (ret=%d)\n", ret);
                return ret;
            }

            ret = halDeviceDisableP2P(devArray[0], phyId[1], 0);
            if (ret != 0) {
                LOG_ERR("Disable P2P failed. (ret=%d)\n", ret);
                return ret;
            }

            ret = halDeviceDisableP2P(devArray[1], phyId[0], 0);
            if (ret != 0) {
                LOG_ERR("Disable P2P failed. (ret=%d)\n", ret);
                return ret;
            }

            LOG_INFO("Enable and disable P2P between device%u and device%u successfully.\n", devArray[0], devArray[1]);
        }
    }

    LOG_INFO("Run the device manager sample successfully.\n");
    return 0;
}
