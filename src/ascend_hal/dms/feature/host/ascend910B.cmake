# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

target_compile_definitions(dms_obj PRIVATE
    # ==== dms ====
    CFG_FEATURE_SRIOV
    CFG_FEATURE_SOC_VERSION
    CFG_FEATURE_VF_USE_DEVID
    CFG_FEATURE_FREQ_ADJUSTABLE
    CFG_FEATURE_PASS_THROUGH_MCU_BY_IMU
    CFG_FEATURE_HBM_FLASH
    CFG_FEATURE_SET_PAGE_RATIO
    CFG_FEATURE_SET_HPAGE_CLEAR
    CFG_SOC_PLATFORM_CLOUD
    CFG_FEATURE_MEMORY
    CORE_NUM_PER_CHIP=8
    CFG_FEATURE_RECONSITUTION_TEMPORARY
    CFG_FEATURE_SUPPORT_GET_SPOD_PING_INFO
    CFG_FEATURE_AIC_AIV_UTIL_FROM_TS
    CFG_FEATURE_NO_DEVICE_NO_ERR
    CFG_FEATURE_ERR_CODE_NOT_OPTIMIZATION
    CFG_FEATURE_TRS_MODE

    # bbox

    # board

    # chip

    # chip->can

    # chip->dvpp
    CFG_FEATURE_DVPP_INFO

    # chip->flash

    # chip->hccs

    # chip->host_aicpu

    # chip->imu

    # chip->isp

    # chip->memory
    CFG_DDR_REG_ADDR_910B

    # chip->sensorhub

    # chip->sio

    # chip->soc

    # chip->ts
    CFG_FEATURE_IPC_FORMAT_V2

    # common

    # communication

    # dc

    # emmc

    # fault
    CFG_FEATURE_GET_CURRENT_EVENTINFO

    # fault_inject

    # flash

    # hbm

    # log

    # lpm

    # p2p

    # pcie
    CFG_FEATURE_DMS_ARCH_V1
    CFG_FEATURE_PCIE_LINK_ERROR_INFO
    CFG_FEATURE_ERRORCODE_ON_NEW_CHIPS
    # power

    # qos

    # sils

    # time

    # ub

    # udis

    # vdev
)
