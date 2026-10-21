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
#include "dms_device_info.h"
#include "ascend_dev_num.h"
#include "devdrv_user_common.h"
#include "dms/dms_drv_internal.h"
#include "dms/dms_misc_interface.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devdrv_manager_chip.h"
#include "devdrv_manager_ai_core.h"
#include "devdrv_manager_run_info.h"
#include "ascend_inpackage_hal.h"
#include "udis_user.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

STATIC drvError_t drv_get_ai_cpu_num_and_bitmap(uint32_t devId, int32_t info_type, int64_t *query_info)
{
    int ret;
    unsigned int query_dev_id;
    struct devdrv_device_info dev_info = {0};

    if ((info_type == INFO_TYPE_PF_CORE_NUM) || (info_type == INFO_TYPE_PF_OCCUPY)) {
#if defined(DRV_HOST) || !defined(CFG_FEATURE_SRIOV)
        return DRV_ERROR_NOT_SUPPORT;
#else
        if (devId >= ASCEND_VDEV_ID_START) {
            /*
             * VF device id is start from ASCEND_VDEV_ID_START, each PF has VDAVINCI_MAX_VFID_NUM of VFs
             */
            query_dev_id = (devId - ASCEND_VDEV_ID_START) / VDAVINCI_MAX_VFID_NUM;
        } else {
            query_dev_id = devId;
        }
#endif
    } else {
        query_dev_id = devId;
    }

    ret = drvGetDevInfo(query_dev_id, &dev_info);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Get device info failed. (ret=%d; dev_id=%u)\n", ret, devId);
        return ret;
    }

    if ((info_type == INFO_TYPE_PF_CORE_NUM) || (info_type == INFO_TYPE_CORE_NUM)) {
        *query_info = dev_info.ai_cpu_core_num;
    } else if ((info_type == INFO_TYPE_OCCUPY) || (info_type == INFO_TYPE_PF_OCCUPY)) {
        *query_info = (int64_t)(dev_info.aicpu_occupy_bitmap);
    } else {
#ifdef CFG_FEATURE_AICPU_NOT_EXIST
        if (dev_info.ai_cpu_core_num == 0) {
            return DRV_ERROR_NOT_EXIST;
        }
#endif
        *query_info = dev_info.ai_cpu_core_id;
    }

    return DRV_ERROR_NONE;
}

drvError_t drv_get_ai_cpu_info(uint32_t devId, int32_t info_type, int64_t *value)
{
    int ret;
    struct tagDrvSpec spc_info = {0};
    struct tagDrvCpuInfo cpu_info = {0};
    struct devdrv_core_utilization aicpu_util = {0};

    if ((info_type == INFO_TYPE_CORE_NUM) || (info_type == INFO_TYPE_PF_CORE_NUM) || (info_type == INFO_TYPE_OCCUPY) ||
        (info_type == INFO_TYPE_PF_OCCUPY) || (info_type == INFO_TYPE_ID)) {
        ret = drv_get_ai_cpu_num_and_bitmap(devId, info_type, value);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Get aicpu number or bitmap failed. (dev_id=%u; info_type=%d; ret=%d)\n",
                                         devId, info_type, ret);
            return ret;
        }
    } else if (info_type == INFO_TYPE_OS_SCHED) {
        ret = drvGetCpuInfo(devId, &cpu_info);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "drvGetCpuInfo failed ret = %d.\n", ret);
            return ret;
        }
        *value = cpu_info.aicpu_os_sched;
    } else if (info_type == INFO_TYPE_IN_USED) {
        ret = drvGetDeviceSpec(devId, &spc_info);
        if (ret != 0) {
            DEVDRV_DRV_ERR("drvGetDeviceSpec failed ret = %d.\n", ret);
            return ret;
        }
        *value = spc_info.aiCpuInuse;
    } else if (info_type == INFO_TYPE_ERROR_MAP) {
        ret = drvGetDeviceSpec(devId, &spc_info);
        if (ret != 0) {
            DEVDRV_DRV_ERR("drvGetDeviceSpec failed ret = %d.\n", ret);
            return ret;
        }
        *value = spc_info.aiCpuErrorMap;
    } else if (info_type == INFO_TYPE_UTILIZATION) {
        aicpu_util.core_type = DEV_DRV_TYPE_AICPU;
        ret = drv_get_core_utilization(devId, &aicpu_util);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get aicpu utilization. (dev_id=%u; ret=%d)\n", devId, ret);
            return ret;
        }
        *value = (int64_t)aicpu_util.utilization;
    } else if (info_type == INFO_TYPE_WORK_MODE) {
        return DmsGetCpuWorkMode(devId, (long long *)value);
    } else {
        DEVDRV_DRV_INFO("This version does not support this type. (Type=%d)\n", info_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t drv_get_ctrl_cpu_info(uint32_t devId, int32_t info_type, int64_t *value)
{
    int ret;
    struct devdrv_device_info dev_info = {0};
    struct tagDrvCpuInfo cpu_info = {0};

    if (info_type == INFO_TYPE_CORE_NUM) {
        ret = drvGetDevInfo(devId, &dev_info);
        if (ret != 0) {
            DEVDRV_DRV_ERR("drvGetDevInfo failed ret = %d.\n", ret);
            return ret;
        }
        *value = dev_info.ctrl_cpu_core_num;
    } else if (info_type == INFO_TYPE_ID) {
        ret = drvGetDevInfo(devId, &dev_info);
        if (ret != 0) {
            DEVDRV_DRV_ERR("drvGetDevInfo failed ret = %d.\n", ret);
            return ret;
        }
        *value = dev_info.ctrl_cpu_id;
    } else if (info_type == INFO_TYPE_OCCUPY) {
        ret = drvGetDevInfo(devId, &dev_info);
        if (ret != 0) {
            DEVDRV_DRV_ERR("drvGetDevInfo failed ret = %d.\n", ret);
            return ret;
        }
        *value = (int64_t)(dev_info.ctrl_cpu_occupy_bitmap);
    } else if (info_type == INFO_TYPE_IP) {
#ifdef CFG_FEATURE_DMS_ARCH_V1
        ret = drvGetDevInfo(devId, &dev_info);
        if (ret != 0) {
            DEVDRV_DRV_ERR("drvGetDevInfo failed ret = %d.\n", ret);
            return ret;
        }
        *value = dev_info.ctrl_cpu_ip;
#else
        return DRV_ERROR_NOT_SUPPORT;
#endif
    } else if (info_type == INFO_TYPE_ENDIAN) {
        ret = drvGetDevInfo(devId, &dev_info);
        if (ret != 0) {
            DEVDRV_DRV_ERR("drvGetDevInfo failed ret = %d.\n", ret);
            return ret;
        }
        *value = dev_info.ctrl_cpu_endian_little;
    } else if (info_type == INFO_TYPE_OS_SCHED) {
        ret = drvGetCpuInfo(devId, &cpu_info);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "drvGetCpuInfo failed ret = %d.\n", ret);
            return ret;
        }
        *value = cpu_info.ccpu_os_sched;
    } else {
        DEVDRV_DRV_INFO("This version does not support this type. (Type=%d)\n", info_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t drv_get_data_cpu_info(uint32_t devId, int32_t info_type, int64_t *value)
{
    int ret;
    struct tagDrvCpuInfo cpu_info = {0};

    ret = drvGetCpuInfo(devId, &cpu_info);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "drvGetCpuInfo failed ret = %d.\n", ret);
        return ret;
    }

    if (info_type == INFO_TYPE_CORE_NUM) {
        *value = cpu_info.dcpu_num;
    } else if (info_type == INFO_TYPE_OS_SCHED) {
        *value = cpu_info.dcpu_os_sched;
    } else {
        DEVDRV_DRV_INFO("This version does not support this type. (Type=%d)\n", info_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t drv_get_ai_core_level(uint32_t devId, int32_t info_type, int64_t *value)
{
    struct tagDrvSpec spc_info;
    int ret;
    unsigned char level = 0;

    ret = drvGetDeviceSpec(devId, &spc_info);
    if (ret != 0) {
        return ret;
    }

    if (info_type == INFO_TYPE_CORE_NUM_LEVEL) {
        level = spc_info.aiCoreNumLevel;
    } else if (info_type == INFO_TYPE_FREQUE_LEVEL) {
        level = spc_info.aiCoreFreqLevel;
    }

    if (level == 0) {
#if defined(CFG_SOC_PLATFORM_CLOUD) && !defined(CFG_FEATURE_ERRORCODE_ON_NEW_CHIPS)
        return DRV_ERROR_INVALID_VALUE;
#else
        return DRV_ERROR_NOT_SUPPORT;
#endif
    }
    *value = (int64_t)level;

    return DRV_ERROR_NONE;
}

drvError_t drvGetAiCoreInfo(uint32_t devId, int32_t info_type, int64_t *value)
{
    struct devdrv_device_info dev_info = {0};
    struct tagDrvSpec spc_info = {0};
    struct devdrv_core_utilization aicore_util = {0};
    int ret = DRV_ERROR_INVALID_VALUE;

    switch (info_type) {
        case INFO_TYPE_UTILIZATION:
            aicore_util.core_type = DEV_DRV_TYPE_AICORE;
            ret = drv_get_core_utilization(devId, &aicore_util);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get aicore utilization. (dev_id=%u; ret=%d)\n", devId,
                                             ret);
                return ret;
            }
            *value = (int64_t)aicore_util.utilization;
            break;
        case INFO_TYPE_CORE_NUM:
            ret = drvGetDevInfo(devId, &dev_info);
            if (ret != 0) {
                DEVDRV_DRV_ERR("drvGetDevInfo failed ret = %d.\n", ret);
                return ret;
            }
            *value = dev_info.ai_core_num;
            break;
        case INFO_TYPE_IN_USED:
            ret = drvGetDeviceSpec(devId, &spc_info);
            if (ret != 0) {
                DEVDRV_DRV_ERR("drvGetDeviceSpec failed ret = %d.\n", ret);
                return ret;
            }
            *value = spc_info.aiCoreInuse;
            break;
        case INFO_TYPE_ERROR_MAP:
            ret = drvGetDeviceSpec(devId, &spc_info);
            if (ret != 0) {
                DEVDRV_DRV_ERR("drvGetDeviceSpec failed ret = %d.\n", ret);
                return ret;
            }
            *value = spc_info.aiCoreErrorMap;
            break;
        case INFO_TYPE_ID:
            ret = drvGetDevInfo(devId, &dev_info);
            if (ret != 0) {
                DEVDRV_DRV_ERR("drvGetDevInfo failed ret = %d.\n", ret);
                return ret;
            }
            *value = dev_info.ai_core_id;
            break;
        case INFO_TYPE_FREQUE:
            ret = drvGetDevInfo(devId, &dev_info);
            if (ret != 0) {
                DEVDRV_DRV_ERR("drvGetDevInfo failed ret = %d.\n", ret);
                return ret;
            }
            *value = dev_info.ai_core_freq;
            break;
        case INFO_TYPE_CORE_NUM_LEVEL:
        case INFO_TYPE_FREQUE_LEVEL:
            ret = drv_get_ai_core_level(devId, info_type, value);
            if (ret != 0) {
                if (ret != DRV_ERROR_NOT_SUPPORT) {
                    DEVDRV_DRV_ERR("Failed to invoke drv_get_ai_core_level. (ret=%d)\n", ret);
                }
                return ret;
            }
            break;
        case INFO_TYPE_DIE_NUM:
            return DmsGetAiCoreDieNum(devId, (long long *)value);
        default:
            DEVDRV_DRV_INFO("This version does not support this type. (Type=%d)\n", info_type);
            return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t drv_get_vector_core_info(uint32_t devId, int32_t info_type, int64_t *value)
{
    int ret;
    struct devdrv_device_info dev_info = {0};
    struct devdrv_core_utilization aivector_util = {0};

    ret = drvGetDevInfo(devId, &dev_info);
    if (ret != 0) {
        DEVDRV_DRV_ERR("drvGetDevInfo failed ret = %d.\n", ret);
        return ret;
    }

    switch (info_type) {
        case INFO_TYPE_UTILIZATION:
            aivector_util.core_type = DEV_DRV_TYPE_AIVECTOR;
            ret = drv_get_core_utilization(devId, &aivector_util);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get aivector utilization. (dev_id=%u; ret=%d)\n", devId,
                                             ret);
                return ret;
            }
            *value = (int64_t)aivector_util.utilization;
            break;
        case INFO_TYPE_CORE_NUM:
            *value = dev_info.vector_core_num;
            break;
        case INFO_TYPE_FREQUE:
#ifdef CFG_FEATURE_VECTOR_CORE_FREQ_IS_ZERO
            if (dev_info.vector_core_freq == 0) {
                return DRV_ERROR_NOT_SUPPORT;
            }
#endif
            *value = dev_info.vector_core_freq;
            break;
        default:
            DEVDRV_DRV_INFO("This version does not support this type. (Type=%d)\n", info_type);
            ret = DRV_ERROR_INVALID_VALUE;
            return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t drv_get_ts_cpu_info(uint32_t devId, int32_t info_type, int64_t *value)
{
    int ret;
    struct tagDrvCpuInfo cpu_info = {0};
    struct devdrv_device_info dev_info = {0};

    if (info_type == INFO_TYPE_CORE_NUM) {
        ret = drvGetDevInfo(devId, &dev_info);
        if (ret != 0) {
            DEVDRV_DRV_ERR("drvGetDevInfo failed ret = %d.\n", ret);
            return ret;
        }
        *value = dev_info.ts_cpu_core_num;
    } else if (info_type == INFO_TYPE_OS_SCHED) {
        ret = drvGetCpuInfo(devId, &cpu_info);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "drvGetCpuInfo failed ret = %d.\n", ret);
            return ret;
        }
        *value = cpu_info.tscpu_os_sched;
    } else if (info_type == INFO_TYPE_FFTS_TYPE) {
        ret = drv_get_h2d_dev_info(devId, &dev_info);
        if (ret != 0) {
            DEVDRV_DRV_ERR("FFTS drv_get_h2d_dev_info failed ret = %d.\n", ret);
            return ret;
        }
        if ((PLAT_GET_CHIP(dev_info.hardware_version) == CHIP_CLOUD_V4) ||
            (PLAT_GET_CHIP(dev_info.hardware_version) == CHIP_CLOUD_V5) ||
            (PLAT_GET_CHIP(dev_info.hardware_version) == CHIP_MINI_V4)) {
            return DRV_ERROR_NOT_SUPPORT;
        }
        *value = dev_info.ffts_type;
    } else {
        DEVDRV_DRV_INFO("This version does not support this type. (Type=%d)\n", info_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t drv_get_device_computing_power(uint32_t devId, int32_t info_type, int64_t *value)
{
    struct devdrv_device_info dev_info = {0};
    int ret;

    if (info_type >= DEVDRV_MAX_COMPUTING_POWER_TYPE || info_type < 0) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev=%u; info_type=%d)\n", devId, info_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = drv_get_h2d_dev_info(devId, &dev_info);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Get device info failed. (ret=%d; dev=%u; info_type=%d)\n", ret, devId, info_type);
        return ret;
    }

    if (dev_info.computing_power[info_type] == (uint64_t)DEVDRV_COMPUTING_VALUE_ERROR) {
        DEVDRV_DRV_ERR("Get computing value failed. (dev_id=%u; info_type=%d)\n", devId, info_type);
        return DRV_ERROR_INNER_ERR;
    }

    *value = *(int64_t *)&dev_info.computing_power[info_type];
    return DRV_ERROR_NONE;
}

#if (defined DRV_HOST) && (defined CFG_FEATURE_HOST_AICPU)
drvError_t drv_get_host_aicpu_info(uint32_t devId, int32_t info_type, int64_t *value)
{
    int ret;
    drvHostAicpuInfo_t info = {0};
    unsigned int size = sizeof(drvHostAicpuInfo_t);

    ret = DmsGetDeviceInfo(devId, DSMI_MAIN_CMD_HOST_AICPU, DSMI_SUB_CMD_HOST_AICPU_INFO, &info, &size);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to obtain the host aicpu info. (dev_id=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    if (info_type == INFO_TYPE_CORE_NUM) {
        *value = (int64_t)info.num;
    } else if (info_type == INFO_TYPE_FREQUE) {
        *value = (int64_t)info.frequency;
    } else if (info_type == INFO_TYPE_OCCUPY) {
        ret = memcpy_s(value, sizeof(unsigned long long) * DSMI_HOST_AICPU_BITMAP_LEN, info.bitmap,
                       sizeof(unsigned long long) * DSMI_HOST_AICPU_BITMAP_LEN);
        if (ret != 0) {
            DEVDRV_DRV_ERR("Failed to copy the memory. (dev_id=%u; ret=%d)\n", devId, ret);
            return ret;
        }
    } else if (info_type == INFO_TYPE_WORK_MODE) {
        *value = (int64_t)info.work_mode;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}
#endif

drvError_t drv_get_hcom_cpu_info(uint32_t dev_id, int32_t info_type, int64_t *value)
{
    int ret = DRV_ERROR_NOT_SUPPORT;

    switch (info_type) {
        case INFO_TYPE_CORE_NUM:
            ret = dms_get_hcom_cpu_num(dev_id, (long long *)value);
            if (ret != 0) {
                DEVDRV_DRV_ERR("dms_get_hcom_cpu_num failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
                return ret;
            }
            break;
        default:
            return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t drv_get_ai_core_info_by_buff(uint32_t devId, int32_t info_type, void *buf, unsigned int *size)
{
    int ret = DRV_ERROR_NONE;
#ifdef CFG_FEATURE_QUERY_AICORE_BITMAP
    struct devdrv_device_info dev_info = {0};
#define AICORE_BITMAP_BUF_SIZE (sizeof(unsigned long long) * 2) /* 2 * 64 bit, max 128 bit */
#endif

    switch (info_type) {
#ifdef CFG_FEATURE_QUERY_FREQ_INFO
        case INFO_TYPE_CURRENT_FREQ:
            ret = dms_lpm_get_ai_core_curr_freq(devId, buf, size);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(
                    ret, "drv_get_ai_core_info_by_buff unsuccessful. (dev_id=%u; ret=%d; infoType=%d).\n", devId, ret,
                    info_type);
                return ret;
            }

            break;
#endif
#ifdef CFG_FEATURE_QUERY_AICORE_BITMAP
        case INFO_TYPE_OCCUPY:
            ret = drvGetDevInfo(devId, &dev_info);
            if (ret != 0) {
                DEVDRV_DRV_ERR("Get device info failed. (dev_id=%u; ret=%d)\n", devId, ret);
                return ret;
            }

            ret = memcpy_s(buf, *size, &dev_info.aicore_bitmap[0], AICORE_BITMAP_BUF_SIZE);
            if (ret != 0) {
                DEVDRV_DRV_ERR("Copy aicore bitmap failed. (dev_id=%u; ret=%d; src_size=%u; dst_size=%u)\n", devId, ret,
                               AICORE_BITMAP_BUF_SIZE, *size);
                return DRV_ERROR_INVALID_VALUE;
            }

            *size = AICORE_BITMAP_BUF_SIZE;
            break;
#endif
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return ret;
}

drvError_t drvGetCpuInfo(uint32_t devId, drvCpuInfo_t *cpuInfo)
{
    int ret;

    if (cpuInfo == NULL) {
        DEVDRV_DRV_ERR("cpuInfo is NULL. devid(%u)\n", devId);
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (devId >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", devId);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = DmsGetCpuInfo(devId, cpuInfo);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "dms get cpu info failed. (devid=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC int dms_cal_core_avg_util(unsigned char *core, unsigned int core_num, unsigned int *utilization)
{
    unsigned int i;
    unsigned int total_rate = 0;
    unsigned int valid_num = 0;
    for (i = 0; i < core_num; i++) {
        if (core[i] == 0xEF) { /* 0xEF: means conflict with profiling */
            *utilization = 0xEF;
            return 0;
        } else if (core[i] == 0xEE ||
                   core[i] > 100) { /* 0xEE: means the damaged core; 100: utilization can never over 100% */
            continue;
        } else {
            total_rate += (unsigned int)core[i];
            valid_num++;
        }
    }
    if (valid_num != 0) {
        *utilization = (unsigned int)(total_rate / valid_num);
    } else {
        *utilization = 0;
    }

    return 0;
}

STATIC int dms_cal_core_max_util(unsigned char *core, unsigned int core_num, unsigned int *utilization)
{
    unsigned int i;
    *utilization = 0;
    for (i = 0; i < core_num; i++) {
        if (core[i] == 0xEF) { /* 0xEF: means conflict with profiling */
            *utilization = 0xEF;
            return 0;
        } else if (core[i] == 0xEE ||
                   core[i] > 100) { /* 0xEE: means the damaged core; 100: utilization can never over 100% */
            continue;
        } else if ((unsigned int)core[i] > *utilization) {
            *utilization = (unsigned int)core[i];
        } else {
        }
    }

    return 0;
}

STATIC int dms_get_core_bitmap_by_name(unsigned int dev_id, const char *name, unsigned long long *bitmap)
{
    int ret = 0;
    struct devdrv_device_info dev_info = {0};

    ret = drvGetDevInfo(dev_id, &dev_info);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Get bitmap from device info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (strcmp(name, "aic_util") == 0) {
        ret = memcpy_s(bitmap, sizeof(dev_info.aicore_bitmap), dev_info.aicore_bitmap, sizeof(dev_info.aicore_bitmap));
    } else if (strcmp(name, "aiv_util") == 0) {
        ret = memcpy_s(bitmap, sizeof(dev_info.vector_core_bitmap), dev_info.vector_core_bitmap,
                       sizeof(dev_info.vector_core_bitmap));
    } else {
        return DRV_ERROR_PARA_ERROR;
    }

    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke memcpy_s to copy bitmap. (dev_id=%d; name=%s; ret=%d)\n", dev_id, name, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    return 0;
}

STATIC int dms_trans_core_util_2_buf(unsigned int dev_id, struct udis_core_util_info *core_util, void *buf,
                                     unsigned int *size)
{
    int ret = 0;
    if (*size < core_util->core_num) {
        DEVDRV_DRV_ERR("The input parameters are incorrect. (dev_id=%u; size=%u; data_len=%u)\n", dev_id, *size,
                       core_util->core_num);
        return DRV_ERROR_PARA_ERROR;
    }

    *size = core_util->core_num;
    if (core_util->core_num == 0) {
        return 0;
    }

    ret = memcpy_s(buf, *size, core_util->core, core_util->core_num);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke memcpy_s to copy core_util. (dev_id=%d; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return 0;
}

STATIC int dms_filter_core_by_bitmap(unsigned int dev_id, struct udis_dev_info *info, void *buf, unsigned int *size)
{
    int ret = 0;
    unsigned int i, phy_id, hostflag;
    unsigned long long core_bitmap[2];
    struct udis_core_util_info origin_core_util = {0};
    struct udis_core_util_info filter_core_util = {0};

    ret = memcpy_s(&origin_core_util, sizeof(origin_core_util), info->data, info->data_len);
    if (ret != 0) {
        DEVDRV_DRV_ERR(
            "Failed to invoke memcpy_s to copy origin_core_util. (dev_id=%d; module_type=%u; name=%s; ret=%d)\n",
            dev_id, info->module_type, info->name, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = drvDeviceGetPhyIdByIndex(dev_id, &phy_id);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Get phy_id failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = drvGetHostPhyMachFlag(dev_id, &hostflag);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Get hostflag failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if ((phy_id < ASCEND_VDEV_ID_START) &&
        (hostflag != DEVDRV_HOST_VM_MACH_FLAG)) { /* If not in VM split, it is pf, no need to filter */
        return dms_trans_core_util_2_buf(dev_id, &origin_core_util, buf, size);
    }

    ret = dms_get_core_bitmap_by_name(dev_id, info->name, core_bitmap);
    if (ret != 0) {
        return ret;
    }

    filter_core_util.core_num = 0;
    for (i = 0; i < origin_core_util.core_num; i++) {
        /* Support max 128 bits, needs to use core_bitmap[1] if the index exceeds 64. */
        if (((core_bitmap[i / 64] >> (i % 64)) & 1) == 0) {
            continue;
        }
        filter_core_util.core[filter_core_util.core_num] = origin_core_util.core[i];
        filter_core_util.core_num++;
    }

    return dms_trans_core_util_2_buf(dev_id, &filter_core_util, buf, size);
}

int dms_get_ts_info_from_udis(unsigned int dev_id, const char *name, void *buf, unsigned int *size)
{
    int ret = 0;
    struct udis_dev_info info = {0};

    if (buf == NULL || size == NULL) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev_id=%u; buf_is_null=%d; size_is_null=%d)\n", dev_id, (buf == NULL),
                       (size == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    info.module_type = UDIS_MODULE_TS;
    ret = strcpy_s(info.name, UDIS_MAX_NAME_LEN, name);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke strcpy_s to copy info name. (dev_id=%d; name=%s; ret=%d)\n", dev_id, name,
                       ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = halUdisGetDeviceInfo(dev_id, &info);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get udis info. (dev_id=%d; module_type=%u; name=%s; ret=%d)\n",
                                     dev_id, info.module_type, info.name, ret);
        return ret;
    }

    if (strcmp(name, "aic_util") == 0 || strcmp(name, "aiv_util") == 0) {
        return dms_filter_core_by_bitmap(dev_id, &info, buf, size);
    }

    if (*size < info.data_len) {
        DEVDRV_DRV_ERR("The input parameters are incorrect. (dev_id=%u; size=%u; data_len=%u)\n", dev_id, *size,
                       info.data_len);
        return DRV_ERROR_PARA_ERROR;
    }

    *size = info.data_len;
    ret = memcpy_s(buf, *size, info.data, info.data_len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke memcpy_s to copy data. (dev_id=%d; module_type=%u; name=%s; ret=%d)\n", dev_id,
                       info.module_type, info.name, ret);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return 0;
}

int dms_get_util_from_udis(unsigned int dev_id, const char *name, unsigned int *result_data)
{
    int ret = 0;
    /* Support max 128 core. */
    unsigned char core[128];
    unsigned int core_size = sizeof(core);
    *result_data = 0;
    ret = dms_get_ts_info_from_udis(dev_id, name, core, &core_size);
    if (ret != 0) {
        return ret;
    }
    if (strcmp(name, "aic_util") == 0 || strcmp(name, "aiv_util") == 0) {
        return dms_cal_core_avg_util(core, core_size, result_data);
    }

    if (core_size != 1) {
        DEVDRV_DRV_ERR(
            "Data len is not equal to bufsize. (dev_id=%d; module_type=%u; name=%s; data_len=%u; buf_size=1)\n", dev_id,
            UDIS_MODULE_TS, name, core_size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = memcpy_s(result_data, core_size, core, core_size);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke memcpy_s to copy result_data. (dev_id=%d; module_type=%u; name=%s; ret=%d)\n",
                       dev_id, UDIS_MODULE_TS, name, ret);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return 0;
}

#define NPU_MULTI_UTIL_INFO_NUM 4
static char *g_npu_multi_util_info_table[NPU_MULTI_UTIL_INFO_NUM] = {"aic_util", "aiv_util", "aicore_util",
                                                                     "stars_util"};
/* UDIS_QUERY_INTERVAL 105000us = 105ms. */
#define UDIS_QUERY_INTERVAL 105000
STATIC int dms_check_info_time_consistent_from_udis(unsigned int dev_id, struct udis_dev_info *info, unsigned int size)
{
    unsigned int i, j;
    int ret = 0;
    unsigned long long last_update_time = 0;
    for (i = 0; i < size; i++) {
        ret = halUdisGetDeviceInfo(dev_id, &info[i]);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get udis info. (dev_id=%d; module_type=%u; name=%s; ret=%d)\n",
                                         dev_id, info[i].module_type, info[i].name, ret);
            return ret;
        }
        if (i == 0) {
            last_update_time = info[i].last_update_time;
            continue;
        }
#ifdef DRV_HOST
        /* Check time consistency. */
        if (info[i].last_update_time < last_update_time) {
            usleep(UDIS_QUERY_INTERVAL);
            ret = halUdisGetDeviceInfo(dev_id, &info[i]);
            if (ret != 0) {
                return ret;
            }
            if (last_update_time != info[i].last_update_time) {
                DEVDRV_DRV_WARN("Failed to get_multi_util, the timestamps are inconsistent. (dev_id=%d; ret=%d)\n",
                                dev_id, ret);
                return DRV_ERROR_INNER_ERR;
            }
        } else if (info[i].last_update_time > last_update_time) {
            last_update_time = info[i].last_update_time;
            for (j = 0; j < i; j++) {
                ret = halUdisGetDeviceInfo(dev_id, &info[j]);
                if (ret != 0) {
                    return ret;
                }
                if (last_update_time != info[j].last_update_time) {
                    DEVDRV_DRV_WARN("Failed to get_multi_util, the timestamps are inconsistent. (dev_id=%d; ret=%d)\n",
                                    dev_id, ret);
                    return DRV_ERROR_INNER_ERR;
                }
            }
        }
#endif
    }

    return 0;
}

STATIC int dms_get_aicore_and_stars_util(unsigned int dev_id, struct udis_dev_info *info, unsigned char *aicore_util,
                                         unsigned char *npu_util)
{
    /* Check data_len from info[2] and info[3] */
    if (info[2].data_len != 1 || info[3].data_len != 1) {
        DEVDRV_DRV_ERR("Data len is not equal to bufsize. (dev_id=%d; data_len1=%u; data_len2=%u; buf_size=1)\n",
                       dev_id, info[2].data_len, info[3].data_len); /* info[2]: aicore_util; info[3]: stars_util. */
        return DRV_ERROR_PARA_ERROR;
    }

    /* info[2]: aicore_util. */
    *aicore_util = (unsigned char)info[2].data[0];
    /* info[3]: stars_util. */
    *npu_util = (unsigned char)info[3].data[0];
    return 0;
}

STATIC int dms_get_npu_multi_util_struct(unsigned int dev_id, struct udis_dev_info *info,
                                         NPU_MULTI_UTIL_INFO *result_data)
{
    unsigned int i;
    int ret = 0;
    /* There are 2 types need to be filtered (core_util[0]: aic_util; core_util[1]: aiv_util). */
    struct udis_core_util_info core_util[2] = {0};

    /* Filter 2 types by bitmap */
    for (i = 0; i < 2; i++) {
        core_util[i].core_num = sizeof(core_util[i].core);
        ret = dms_filter_core_by_bitmap(dev_id, &info[i], core_util[i].core, &core_util[i].core_num);
        if (ret != 0) {
            return ret;
        }
    }

    /* core_util[0]: aic_util; core_util[1]: aiv_util. */
    dms_cal_core_avg_util(core_util[0].core, core_util[0].core_num, (unsigned int *)&result_data->aic_avg_util);
    dms_cal_core_avg_util(core_util[1].core, core_util[1].core_num, (unsigned int *)&result_data->aiv_avg_util);
    dms_cal_core_max_util(core_util[0].core, core_util[0].core_num, (unsigned int *)&result_data->aic_max_util);
    dms_cal_core_max_util(core_util[1].core, core_util[1].core_num, (unsigned int *)&result_data->aiv_max_util);
    return dms_get_aicore_and_stars_util(dev_id, info, &result_data->aicore_util, &result_data->npu_util);
}

STATIC int dms_get_npu_multi_util_v2_struct(unsigned int dev_id, struct udis_dev_info *info,
                                            NPU_MULTI_UTIL_INFO_V2 *result_data)
{
    int ret;

    /* info[0]: aicube_util. */
    result_data->aicube_count = *((u32 *)(info[0].data));
    if (result_data->aicube_count > sizeof(result_data->aicube_util)) {
        DEVDRV_DRV_ERR("Data len is not equal to bufsize. (dev_id=%d; data_len=%u; buf_size=1)\n", dev_id,
                       result_data->aicube_count, sizeof(result_data->aicube_util));
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dms_filter_core_by_bitmap(dev_id, &info[0], result_data->aicube_util, &result_data->aicube_count);
    if (ret != 0) {
        return ret;
    }

    /* info[1]: vector_util. */
    result_data->vector_count = *((u32 *)(info[1].data));
    if (result_data->vector_count > sizeof(result_data->vector_util)) {
        DEVDRV_DRV_ERR("Data len is not equal to bufsize. (dev_id=%d; data_len=%u; buf_size=1)\n", dev_id,
                       result_data->vector_count, sizeof(result_data->vector_util));
        return DRV_ERROR_PARA_ERROR;
    }
    ret = dms_filter_core_by_bitmap(dev_id, &info[1], result_data->vector_util, &result_data->vector_count);
    if (ret != 0) {
        return ret;
    }

    return dms_get_aicore_and_stars_util(dev_id, info, &result_data->aicore_util, &result_data->npu_total_util);
}

static int dms_get_npu_multi_util_result(unsigned int dev_id, const char *name, struct udis_dev_info *info, void *buf,
                                         unsigned int *size)
{
    int ret = 0;
    NPU_MULTI_UTIL_INFO result_data = {0};
    NPU_MULTI_UTIL_INFO_V2 result_data_v2 = {0};

    if (strcmp(name, "npu_multi_util_v2") == 0) {
        ret = dms_get_npu_multi_util_v2_struct(dev_id, info, &result_data_v2);
        if (ret != 0) {
            return ret;
        }

        if (*size < sizeof(NPU_MULTI_UTIL_INFO_V2)) {
            DEVDRV_DRV_ERR("The input parameters are incorrect. (dev_id=%u; size=%u; data_len=%u)\n", dev_id, *size,
                           sizeof(NPU_MULTI_UTIL_INFO_V2));
            return DRV_ERROR_PARA_ERROR;
        }

        *size = (unsigned int)sizeof(NPU_MULTI_UTIL_INFO_V2);
        ret = memcpy_s(buf, *size, &result_data_v2, sizeof(NPU_MULTI_UTIL_INFO_V2));
    } else {
        ret = dms_get_npu_multi_util_struct(dev_id, info, &result_data);
        if (ret != 0) {
            return ret;
        }

        if (*size < sizeof(NPU_MULTI_UTIL_INFO)) {
            DEVDRV_DRV_ERR("The input parameters are incorrect. (dev_id=%u; size=%u; data_len=%u)\n", dev_id, *size,
                           sizeof(NPU_MULTI_UTIL_INFO));
            return DRV_ERROR_PARA_ERROR;
        }

        *size = (unsigned int)sizeof(NPU_MULTI_UTIL_INFO);
        ret = memcpy_s(buf, *size, &result_data, sizeof(NPU_MULTI_UTIL_INFO));
    }

    if (ret != EOK) {
        DEVDRV_DRV_ERR("Failed to invoke memcpy_s to copy data. (dev_id=%d; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return 0;
}

int dms_get_npu_multi_util_from_udis(unsigned int dev_id, const char *name, void *buf, unsigned int *size)
{
    unsigned int i;
    int ret = 0;
    struct udis_dev_info info[NPU_MULTI_UTIL_INFO_NUM] = {0};

    if (buf == NULL || size == NULL) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev_id=%u; buf_is_null=%d; size_is_null=%d)\n", dev_id, (buf == NULL),
                       (size == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    for (i = 0; i < NPU_MULTI_UTIL_INFO_NUM; i++) {
        info[i].module_type = UDIS_MODULE_TS;
        ret = strcpy_s(info[i].name, UDIS_MAX_NAME_LEN, g_npu_multi_util_info_table[i]);
        if (ret != 0) {
            DEVDRV_DRV_ERR("Failed to invoke strcpy_s to copy info name. (dev_id=%d; name=%s; ret=%d)\n", dev_id,
                           g_npu_multi_util_info_table[i], ret);
            return DRV_ERROR_PARA_ERROR;
        }
    }

    ret = dms_check_info_time_consistent_from_udis(dev_id, info, NPU_MULTI_UTIL_INFO_NUM);
    if (ret != 0) {
        return ret;
    }

    return dms_get_npu_multi_util_result(dev_id, name, info, buf, size);
}

STATIC int dms_udis_cal_cpu_utilization(unsigned int dev_id, int module_type, const struct udis_dev_info *get_info,
                                        unsigned int *utilization)
{
    int ret = 0, i;
    int64_t cpu_num = 0;
    unsigned int total_rate = 0;
    unsigned int util_rate[TAISHAN_CORE_NUM] = {0};

    ret = halGetDeviceInfo((unsigned int)dev_id, module_type, INFO_TYPE_CORE_NUM, &cpu_num);
    if (ret != 0) {
        DEVDRV_DRV_WARN("Dms get cpu info not success.(dev_id=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }
    if ((cpu_num == 0) || (cpu_num > TAISHAN_CORE_NUM)) {
        DEVDRV_DRV_WARN("Invalid cpu num. (dev_id=%u; module_type=%u; cpu_num=%u)\n", dev_id, module_type, cpu_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((unsigned int)cpu_num * sizeof(unsigned int) != get_info->data_len) {
        DEVDRV_DRV_WARN("Expected data_len not equal actual data_len for cpu-util. (dev_id=%u; module_type=%u; "
                        "expected_data_len=%u; actual_data_len=%u)\n",
                        dev_id, module_type, (unsigned int)((unsigned long)(cpu_num) * sizeof(unsigned int)),
                        get_info->data_len);
        return DRV_ERROR_INNER_ERR;
    }

    ret = memcpy_s(util_rate, (unsigned int)((unsigned long)(cpu_num) * sizeof(unsigned int)), get_info->data,
                   get_info->data_len);
    if (ret != 0) {
        DEVDRV_DRV_WARN("Memcpy to util_rate not success. (dev_id=%u; module_type=%u; cpu_num=%u)\n", dev_id,
                        module_type, cpu_num);
        return DRV_ERROR_INVALID_HANDLE;
    }

    for (i = 0; i < cpu_num; i++) {
        total_rate += util_rate[i];
    }
    *utilization = (unsigned int)(total_rate / (unsigned int)cpu_num);

    return 0;
}

int dms_get_cpu_rate_from_udis(unsigned int dev_id, const char *name, unsigned int *result_data)
{
    struct udis_dev_info get_info = {0};
    int module_type = 0;
    int ret = 0;

    ret = strcpy_s(get_info.name, UDIS_MAX_NAME_LEN, name);
    if (ret != 0) {
        DEVDRV_DRV_WARN("sprintf to udis_get_info_user name failed. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }
    if (strcmp(name, "ccpu_util") == 0) {
        module_type = MODULE_TYPE_CCPU;
    } else if (strcmp(name, "aicpu_util") == 0) {
        module_type = MODULE_TYPE_AICPU;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }

    get_info.module_type = UDIS_MODULE_DEVMNG;
    ret = halUdisGetDeviceInfo((unsigned int)dev_id, &get_info);
    if (ret != 0) {
        DEVDRV_DRV_WARN("call udis_get_device_info not success. (dev_id=%u; name=%s; ret=%d)\n", dev_id, get_info.name,
                        ret);
        return ret;
    }

    return dms_udis_cal_cpu_utilization(dev_id, module_type, &get_info, result_data);
}
