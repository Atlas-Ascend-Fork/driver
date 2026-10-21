/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/ioctl.h>

#include "securec.h"
#include "dms_user_common.h"
#include "devdrv_user_common.h"
#include "dms/dms_lpm_interface.h"
#include "dms/dms_soc_interface.h"
#include "dms/dms_devdrv_info_comm.h"
#include "dms/dms_misc_interface.h"
#include "cpu_utilization.h"
#include "ascend_dev_num.h"

STATIC int dms_get_cpu_utilization(unsigned int *utilization, unsigned int num, unsigned int index)
{
    struct dms_ioctl_arg ioarg = {0};
    struct dms_get_cpu_utilization_in in_struct = {.index = index, .num = num};
    int ret;
#ifdef CFG_FEATURE_VFIO_SOC
    unsigned int physical_id = 0;

    if (drvDeviceGetPhyIdByIndex(0, &physical_id) != DRV_ERROR_NONE) {
        DMS_ERR("Failed to get physical_id. (dev_id=0)\n");
        return DRV_ERROR_INVALID_DEVICE;
    }

    /* If in compute-group container, it will return 'utilization = 0'. */
    if (physical_id >= ASCEND_VDEV_ID_START) {
        *utilization = 0;
        return DRV_ERROR_NONE;
    }
#endif

    ioarg.main_cmd = DMS_MAIN_CMD_SOC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_CPU_UTILIZATION;
    ioarg.filter_len = 0;
    ioarg.input = &in_struct;
    ioarg.input_len = sizeof(struct dms_get_cpu_utilization_in);
    ioarg.output = utilization;
    ioarg.output_len = (unsigned int)sizeof(unsigned int) * num;
    ret = DmsIoctl(DMS_GET_CPU_UTILIZATION, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms get cpu utilization failed. (index=%u; num=%u)\n", index, num);
    }
    return ret;
}

int dms_get_aicpu_info(unsigned int dev_id, struct dmanage_aicpu_info_stru *aicpu_info)
{
    unsigned int utilRate[TAISHAN_CORE_NUM] = {0};
    size_t rate_size = sizeof(unsigned int) * TAISHAN_CORE_NUM;
    drvCpuInfo_t cpu_info = {0};
    unsigned int freq = 0;
    int ret;
    struct dms_lpm_info_in in;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (aicpu_info == NULL)) {
        DMS_ERR("invalid input.(devid=%u;aicpu_info=%d)\n", dev_id, aicpu_info != NULL);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = DmsGetCpuInfo(dev_id, &cpu_info);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms get cpu info fail.(devid=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (cpu_info.aicpu_num == 0) {
        aicpu_info->aicpuNum = 0;
        aicpu_info->utilRate[0] = 0;
        aicpu_info->curFreq = 0;
        aicpu_info->maxFreq = 0;
        return DRV_ERROR_NONE;
    }

    if (cpu_info.aicpu_num > TAISHAN_CORE_NUM) {
        DMS_ERR("aicpu_num %u exceeds max value %u.\n", cpu_info.aicpu_num, TAISHAN_CORE_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = dms_get_cpu_utilization(utilRate, cpu_info.aicpu_num,
                                  CORE_NUM_PER_CHIP * dev_id + cpu_info.ccpu_num + cpu_info.dcpu_num);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "dms_get_cpu_utilization fail.(devid=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }
    aicpu_info->aicpuNum = cpu_info.aicpu_num;
    ret = memcpy_s(aicpu_info->utilRate, rate_size, utilRate, rate_size);
    if (ret != 0) {
        DMS_ERR("memcpy failed.(devid=%u;ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    in.dev_id = dev_id;
    in.vfid = DMS_VF_ID_PF;
    in.core_id = CLUSTER_ID;
    in.sub_cmd = DMS_LPM_GET_FREQUENCY;
    ret = DmsGetLpmInfo(&in, &freq, sizeof(unsigned int));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "DmsGetLpmInfo fail.(devid=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }
    aicpu_info->curFreq = freq;

#ifndef CFG_FEATURE_FREQ_ADJUSTABLE
    aicpu_info->maxFreq = freq;
#else
    in.sub_cmd = DMS_LPM_GET_MAX_FREQUENCY;
    ret = DmsGetLpmInfo(&in, &freq, sizeof(unsigned int));
    if (ret) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dmanage get aicpu max frequency failed.(devid=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }
    aicpu_info->maxFreq = freq;
#endif

    return DRV_ERROR_NONE;
}

int dms_get_aicpu_utilization(unsigned int dev_id, unsigned int *utilization)
{
    unsigned int utilRate[TAISHAN_CORE_NUM] = {0};
    drvCpuInfo_t cpu_info = {0};
    unsigned int total_rate = 0;
    unsigned int i;
    int ret;

    if ((dev_id >= DEVDRV_PF_DEV_MAX_NUM) || (utilization == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = DmsGetCpuInfo(dev_id, &cpu_info);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms get cpu info fail.(devid=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (cpu_info.aicpu_num == 0) {
        *utilization = 0;
        return DRV_ERROR_NONE;
    }

    if (cpu_info.aicpu_num > TAISHAN_CORE_NUM) {
        DMS_ERR("invalid aicpu num. (dev_id=%u;aicpu_num=%u)\n", dev_id, cpu_info.aicpu_num);
        return DRV_ERROR_INVALID_VALUE;
    }
    ret = dms_get_cpu_utilization(utilRate, cpu_info.aicpu_num,
                                  CORE_NUM_PER_CHIP * dev_id + cpu_info.ccpu_num + cpu_info.dcpu_num);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "dms_get_cpu_utilization fail.(devid=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    /* Calculate average utilization rate */
    for (i = 0; i < cpu_info.aicpu_num; i++) {
        total_rate += utilRate[i];
    }
    *utilization = total_rate / cpu_info.aicpu_num;

    return DRV_ERROR_NONE;
}

#define TAISHAN_CORE_NUM_MAX 40
int dms_get_ctlcpu_utilization(unsigned int dev_id, unsigned int *utilization)
{
    unsigned int utilRate[TAISHAN_CORE_NUM_MAX] = {0};
    drvCpuInfo_t cpu_info = {0};
    unsigned int total_rate = 0;
    unsigned int i;
    int ret;

    if ((utilization == NULL) || (dev_id >= ASCEND_PDEV_MAX_NUM)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = DmsGetCpuInfo(dev_id, &cpu_info);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms get cpu info fail.(devid=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    if ((cpu_info.ccpu_num == 0) || (cpu_info.ccpu_num > TAISHAN_CORE_NUM_MAX)) {
        DMS_ERR("invalid ctrl cpu num. (dev_id=%u;ccpu_num=%u)\n", dev_id, cpu_info.ccpu_num);
        return DRV_ERROR_INVALID_VALUE;
    }
    ret = dms_get_cpu_utilization(utilRate, cpu_info.ccpu_num, CORE_NUM_PER_CHIP * dev_id);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "dms_get_cpu_utilization fail.(devid=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    /* Calculate average utilization rate */
    for (i = 0; i < cpu_info.ccpu_num; i++) {
        total_rate += utilRate[i];
    }
    *utilization = total_rate / cpu_info.ccpu_num;

    return DRV_ERROR_NONE;
}
