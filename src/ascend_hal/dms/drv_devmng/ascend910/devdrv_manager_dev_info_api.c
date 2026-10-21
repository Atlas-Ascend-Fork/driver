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
#include "drv_user_common.h"
#include "dms_device_info.h"
#include "dms_user_common.h"
#include "ascend_dev_num.h"
#include "drv_devmng_adapt.h"
#include "hbm_ctrl.h"
#include "devdrv_user_common.h"
#include "dms/dms_drv_internal.h"
#include "dms/dms_misc_interface.h"
#include "dsmi_common_interface.h"
#ifdef CFG_FEATURE_SUPPORT_UB
#include "comm_user_interface.h"
#endif
#include "devdrv_pcie.h"
#include "devdrv_manager_ai_core.h"
#include "devdrv_manager_chip.h"
#include "devdrv_manager_resource.h"
#include "devdrv_manager_system_info.h"
#include "devdrv_manager_run_info.h"
#include "devdrv_manager_lp.h"
#include "devdrv_manager_bbox.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

typedef struct tag_drv_query_device_info_cmd_st {
    DSMI_MAIN_CMD main_cmd_type;
    unsigned int sub_cmd_type;
    drvError_t (*callback)(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                           unsigned int *size);
} drv_query_device_info_cmd;

typedef struct tag_drv_set_device_info_cmd_st {
    DSMI_MAIN_CMD main_cmd_type;
    unsigned int sub_cmd_type;
    drvError_t (*callback)(unsigned int dev_id, unsigned int sub_cmd, const void *buf, unsigned int size);
} drv_set_device_info_cmd;

STATIC void hal_report_invalid_buf(const char *func_name, const char *para_name)
{
    const char *keys[] = {"func_name", "para_name"};
    const char *values[] = {func_name, para_name};

    REPORT_PREDEFINED_ERR_MSG("EL0017", keys, values, 2UL);
}

drvError_t drvGetDevInfo(uint32_t devId, struct devdrv_device_info *info)
{
    struct devdrv_manager_hccl_devinfo dev_info = {0};
    mmIoctlBuf dev_info_buf = {0};
    int ret;
    if (devId >= ASCEND_DEV_MAX_NUM || info == NULL) {
        DEVDRV_DRV_ERR("Parameter is invalid. (devId=%u; info_is_null=%d)\n", devId, (info == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    dev_info.dev_id = devId;
    drv_ioctl_param_init(&dev_info_buf, (void *)&dev_info, sizeof(struct devdrv_manager_hccl_devinfo));
    ret = drv_common_ioctl(&dev_info_buf, DEVDRV_MANAGER_GET_DEVINFO);
    if (ret != 0) {
        if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED || ret == (int)DRV_ERROR_BUSY) {
            DEVDRV_DRV_ERR_EXTEND(ret, DRV_ERROR_BUSY, "The device is busy. (ret=%d)\n", ret);
            return DRV_ERROR_RESOURCE_OCCUPIED;
        } else {
            DEVDRV_DRV_ERR("Ioctl failed. (ret=%d)\n", ret);
            return ret;
        }
    }

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
    info->chip_id = dev_info.chip_id;
    info->die_id = dev_info.die_id;
    info->vector_core_num = dev_info.vector_core_num;
    info->vector_core_freq = (unsigned int)dev_info.vector_core_freq;
    info->l2_num = dev_info.l2_num;
    info->hbm_num = dev_info.hbm_num;
    info->addr_mode = dev_info.addr_mode;
    info->mainboard_id = dev_info.mainboard_id;
    info->product_type = dev_info.product_type;
    info->host_device_connect_type = dev_info.host_device_connect_type;
    info->aicore_bitmap[0] = dev_info.aicore_bitmap[0];
    info->aicore_bitmap[1] = dev_info.aicore_bitmap[1];
    info->vector_core_bitmap[0] = dev_info.vector_core_bitmap[0];
    info->vector_core_bitmap[1] = dev_info.vector_core_bitmap[1];
    ret = strcpy_s(info->soc_version, SOC_VERSION_LEN, dev_info.soc_version);
    if (ret != 0) {
        return ret;
    }
    return DRV_ERROR_NONE;
}

drvError_t halGetDeviceInfoByBuff(uint32_t devId, int32_t moduleType, int32_t infoType, void *buf, int32_t *size)
{
    int ret;
    DEV_MODULE_TYPE mtype = moduleType;
    DEV_INFO_TYPE itype = infoType;

    if (devId >= ASCEND_DEV_MAX_NUM || buf == NULL || size == NULL) {
        DEVDRV_DRV_ERR("invalid parameter. (dev_id=%u, buf%s, size%s)\n", devId, buf == NULL ? "=NULL" : "!=NULL",
                       size == NULL ? "=NULL" : "!=NULL");
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (mtype) {
        case MODULE_TYPE_QOS:
            ret = drv_get_qos_info(devId, itype, buf, (unsigned int *)size);
            break;

        case MODULE_TYPE_AICORE:
            ret = drv_get_ai_core_info_by_buff(devId, itype, buf, (unsigned int *)size);
            break;

        case MODULE_TYPE_MEMORY:
            ret = drv_get_memory_info(devId, itype, buf, (unsigned int *)size);
            break;

        case MODULE_TYPE_LOG:
            ret = drv_get_klog_info(devId, itype, buf, (unsigned int *)size);
            break;

        case MODULE_TYPE_LP:
            ret = drv_get_lp_info(devId, itype, buf, (unsigned int *)size);
            break;

        case MODULE_TYPE_CC:
            ret = drv_get_cc_info(devId, itype, buf, (unsigned int *)size);
            break;

        case MODULE_TYPE_L2BUFF:
            ret = drv_get_l_2_buff_info(devId, itype, buf, (unsigned int *)size);
            break;

        case MODULE_TYPE_SYSTEM:
            ret = drv_get_system_info_ex(devId, itype, buf, (unsigned int *)size);
            break;

        case MODULE_TYPE_UB:
            ret = dms_get_ub_info(devId, itype, buf, size);
            break;

        case MODULE_TYPE_PCIE:
            ret = dms_get_pcie_info(devId, itype, buf, size);
            break;

        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(
            ret, "halGetDeviceInfoByBuff unsuccessful. (dev_id=%u; ret=%d; moduleType=%d; infoType=%d).\n", devId, ret,
            mtype, itype);
    }

    return ret;
}

drvError_t halSetDeviceInfoByBuff(uint32_t devId, int32_t moduleType, int32_t infoType, void *buf, int32_t size)
{
    int ret;
    DEV_MODULE_TYPE mtype = moduleType;
    DEV_INFO_TYPE itype = infoType;

    if (devId >= ASCEND_DEV_MAX_NUM || buf == NULL || size <= 0) {
        if (devId >= ASCEND_DEV_MAX_NUM) {
            hal_report_invalid_dev_id("halSetDeviceInfoByBuff", devId, ASCEND_DEV_MAX_NUM - 1);
        } else if (buf == NULL) {
            hal_report_invalid_buf("halSetDeviceInfoByBuff", "buf");
        } else {
        }

        DEVDRV_DRV_ERR("invalid parameter. (dev_id=%u;buf=%d;size=%d)\n", devId, buf != NULL, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (mtype) {
        case MODULE_TYPE_LP:
            ret = drv_set_lp_info(devId, itype, buf, (unsigned int)size);
            break;

        case MODULE_TYPE_L2BUFF:
            ret = drv_set_l_2_buff_info(devId, itype, buf, (unsigned int)size);
            break;

        case MODULE_TYPE_SYSTEM:
            ret = drv_set_system_info_ex(devId, itype, buf, (unsigned int)size);
            break;

        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(
            ret, "halSetDeviceInfoByBuff unsuccessful. (dev_id=%u; ret=%d; moduleType=%d; infoType=%d).\n", devId, ret,
            mtype, itype);
        return ret;
    }

    return DRV_ERROR_NONE;
}

#ifdef CFG_FEATURE_FFTS
STATIC drvError_t hal_get_device_info_cmd(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                                          unsigned int *size)
{
    int ret;
    int64_t val;
    (void)main_cmd;
    (void)sub_cmd;

    if (*size < sizeof(unsigned int)) {
        DEVDRV_DRV_ERR("Parameter is invalid. (dev_id=%u; size=%u)\n", dev_id, *size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = halGetDeviceInfo(dev_id, MODULE_TYPE_TSCPU, INFO_TYPE_FFTS_TYPE, &val);
    if (ret != 0) {
        DEVDRV_DRV_ERR("halGetDeviceInfo failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    *(unsigned int *)buf = (unsigned int)val;
    *size = sizeof(unsigned int);
    return DRV_ERROR_NONE;
}
#endif

STATIC drvError_t hal_get_urma_dev_name(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                                        unsigned int *size)
{
    (void)main_cmd;
    (void)sub_cmd;
#if defined(CFG_FEATURE_SUPPORT_UB) && defined(CFG_EDGE_HOST)

    return ascend_urma_get_urma_dev_name(dev_id, (char *)buf, size);
#else
    (void)dev_id;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t drvQueryDeviceInfo(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                              unsigned int *size)
{
    drv_query_device_info_cmd query_info_cmd_func[] = {
        {DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_CHIP_ID, hal_get_chip_from_device_cmd},
        {DSMI_MAIN_CMD_UB, DSMI_UB_INFO_SUB_CMD_PORT_STATUS, hal_get_ub_status_cmd},
        {DSMI_MAIN_CMD_UB, DSMI_UB_INFO_SUB_CMD_ID, dms_get_ub_id_info},
        {DSMI_MAIN_CMD_UB, DSMI_UB_INFO_SUB_CMD_URMA_DEV_NAME, hal_get_urma_dev_name},
#ifdef CFG_FEATURE_FFTS
        {DSMI_MAIN_CMD_TS, DSMI_TS_SUB_CMD_FFTS_TYPE, hal_get_device_info_cmd},
#endif
        {DSMI_MAIN_CMD_SVM, DSMI_GET_VDEV_CONVERT_LEN, drv_get_svm_vdev_info_cmd},
        {DSMI_MAIN_CMD_VDEV_MNG, DSMI_VMNG_SUB_CMD_GET_VDEV_RESOURCE, drvGetSingleVdevInfo},
        {DSMI_MAIN_CMD_VDEV_MNG, DSMI_VMNG_SUB_CMD_GET_TOTAL_RESOURCE, drvGetVdevTotalInfo},
        {DSMI_MAIN_CMD_VDEV_MNG, DSMI_VMNG_SUB_CMD_GET_FREE_RESOURCE, drvGetVdevFreeInfo},
        {DSMI_MAIN_CMD_VDEV_MNG, DSMI_VMNG_SUB_CMD_GET_VDEV_ACTIVITY, drvGetVdevActivityInfo},
#ifdef CFG_FEATURE_VFIO_SOC
        {DSMI_MAIN_CMD_VDEV_MNG, DSMI_VMNG_SUB_CMD_GET_TOPS_PERCENTAGE, drvGetVdevTopsPercentage},
#endif
#if (defined DRV_HOST) && (defined CFG_FEATURE_HOST_AICPU)
        {DSMI_MAIN_CMD_HOST_AICPU, DSMI_SUB_CMD_HOST_AICPU_INFO, DmsGetHostAicpuInfo},
        {DSMI_MAIN_CMD_HCCS, DSMI_HCCS_CMD_GET_CREDIT_INFO, DmsGetDeviceInfoEx},
#endif
#if (defined DRV_HOST) && (defined CFG_FEATURE_HOST_TS)
        {DSMI_MAIN_CMD_TS, DSMI_TS_SUB_CMD_COMMON_MSG, DmsGetTsInfoEx},
#endif
        {DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_SPOD_INFO, dms_get_spod_info},
#ifdef CFG_FEATURE_GET_DEV_INDEX_IN_GROUP
        {DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_INDEX_IN_GROUP, DmsGetIndexInGroup},
#endif
        {DSMI_MAIN_CMD_HCCS, DSMI_HCCS_CMD_GET_PING_INFO, dms_get_spod_ping_info},
#if (defined DRV_HOST)
        {DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_SPOD_NODE_STATUS, dms_get_spod_node_status},
#endif
        {DSMI_MAIN_CMD_TRS, DSMI_TRS_SUB_CMD_KERNEL_LAUNCH_MODE, DmsGetTrsMode},
        {DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_UUID, hal_get_uuid_cmd},
        {DSMI_MAIN_CMD_UPGRADE, DSMI_UPGRADE_SUB_TYPE_SWPLUGIN_POLICY, hal_get_swplugin_upgrade},
        {DSMI_MAIN_CMD_PCIE, DSMI_PCIE_SUB_CMD_PCIE_INFO, DmsGetDeviceInfoEx},
    };

    unsigned int i;
    unsigned int query_cmd_max;

    if ((buf == NULL) || (size == NULL) || (dev_id >= ASCEND_DEV_MAX_NUM)) {
        DEVDRV_DRV_ERR("Parameter is invalid. (dev_id=%u; buf_is_null=%d; size_is_null=%d)\n", dev_id, (buf == NULL),
                       (size == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    query_cmd_max = sizeof(query_info_cmd_func) / sizeof(drv_query_device_info_cmd);

    for (i = 0; i < query_cmd_max; i++) {
        if ((query_info_cmd_func[i].main_cmd_type == main_cmd) && (query_info_cmd_func[i].sub_cmd_type == sub_cmd)) {
            if (query_info_cmd_func[i].callback != NULL) {
                return query_info_cmd_func[i].callback(dev_id, main_cmd, sub_cmd, buf, size);
            }
            break;
        }
    }
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t drvSetDeviceInfo(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, const void *buf,
                            unsigned int size)
{
    unsigned int i;
    unsigned int query_cmd_max;
    drv_set_device_info_cmd set_info_cmd_func[] = {
        {DSMI_MAIN_CMD_SVM, DSMI_SET_VDEV_CONVERT_LEN, drvSetSvmVdevInfo},
#ifdef CFG_FEATURE_SRIOV
        {DSMI_MAIN_CMD_VDEV_MNG, DSMI_VMNG_SUB_CMD_SET_SRIOV_SWITCH, dms_set_sriov_switch},
#endif
#if (defined DRV_HOST) && (defined CFG_FEATURE_HOST_AICPU)
        {DSMI_MAIN_CMD_HOST_AICPU, DSMI_SUB_CMD_HOST_AICPU_INFO, DmsSetHostAicpuInfo},
#endif
#if (defined DRV_HOST) && (defined CFG_FEATURE_HOST_TS)
        {DSMI_MAIN_CMD_TS, DSMI_TS_SUB_CMD_COMMON_MSG, DmsSetTsInfo},
#endif
#if (defined DRV_HOST)
        {DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_SPOD_NODE_STATUS, dms_set_spod_node_status},
#endif
        {DSMI_MAIN_CMD_UPGRADE, DSMI_UPGRADE_SUB_TYPE_SWPLUGIN_POLICY, DmsSetSWPluginUpgrade},
    };

    query_cmd_max = sizeof(set_info_cmd_func) / sizeof(drv_set_device_info_cmd);
    for (i = 0; i < query_cmd_max; i++) {
        if ((set_info_cmd_func[i].main_cmd_type == main_cmd) && (set_info_cmd_func[i].sub_cmd_type == sub_cmd)) {
#ifndef DEVDRV_UT
            return set_info_cmd_func[i].callback(dev_id, sub_cmd, buf, size);
#endif
        }
    }

    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t drvSetDeviceInfoToDmsHal(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, const void *buf,
                                    unsigned int size)
{
    int module_type = -1;
    int info_type = -1;

    if (buf == NULL || size == 0) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev_id=%u; buf_is_null=%d; size=%u)\n", dev_id, (buf == NULL), size);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((main_cmd == DSMI_MAIN_CMD_SOC_INFO) && (sub_cmd == DSMI_SOC_INFO_SUB_CMD_CUST_OP_ENHANCE)) {
        module_type = MODULE_TYPE_SYSTEM;
        info_type = INFO_TYPE_CUST_OP_ENHANCE;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DmsHalSetDeviceInfoEx(dev_id, module_type, info_type, buf, size);
}

static int hal_get_device_info_para_check(uint32_t devId, int32_t module_type, int32_t info_type, int64_t *value)
{
    if ((module_type == MODULE_TYPE_SYSTEM) && (info_type == INFO_TYPE_SPOD_VNIC_IP)) {
        /* input devId is sdid, no need check */
    } else if ((module_type == MODULE_TYPE_SYSTEM) && (info_type == INFO_TYPE_VNIC_IP)) {
        if (devId >= ASCEND_HOST_PDEV_MAX_NUM) {
            DEVDRV_DRV_ERR("Invalid parameter. (devId=%u; max_host_dev_id=%u)\n", devId, ASCEND_HOST_PDEV_MAX_NUM);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else {
        if (devId >= ASCEND_DEV_MAX_NUM) {
            hal_report_invalid_dev_id("halGetDeviceInfo", devId, ASCEND_DEV_MAX_NUM - 1);
            DEVDRV_DRV_ERR("Invalid parameter. (devId=%u; max_dev_id=%u)\n", devId, ASCEND_DEV_MAX_NUM);
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    if (value == NULL) {
        hal_report_invalid_buf("halGetDeviceInfo", "value");
        DEVDRV_DRV_ERR("Invalid parameter. (value_is_null=%d)\n", (value == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    return 0;
}

drvError_t halGetDeviceInfo(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
    int ret;
    DEV_INFO_TYPE itype = infoType;
    DEV_MODULE_TYPE mtype = moduleType;

    if (hal_get_device_info_para_check(devId, moduleType, infoType, value) != 0) {
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (mtype) {
        case MODULE_TYPE_SYSTEM:
            ret = drv_get_system_info(devId, itype, value);
            break;

        case MODULE_TYPE_AICPU:
            ret = drv_get_ai_cpu_info(devId, itype, value);
            break;

        case MODULE_TYPE_DCPU:
            ret = drv_get_data_cpu_info(devId, itype, value);
            break;

        case MODULE_TYPE_CCPU:
            ret = drv_get_ctrl_cpu_info(devId, itype, value);
            break;

        case MODULE_TYPE_AICORE:
            ret = drvGetAiCoreInfo(devId, itype, value);
            break;

        case MODULE_TYPE_TSCPU:
            ret = drv_get_ts_cpu_info(devId, itype, value);
            break;

        case MODULE_TYPE_PCIE:
            ret = drv_get_pcieinfo(devId, itype, value);
            break;

        case MODULE_TYPE_VECTOR_CORE:
            ret = drv_get_vector_core_info(devId, itype, value);
            break;

        case MODULE_TYPE_COMPUTING:
            ret = drv_get_device_computing_power(devId, infoType, value);
            break;

        case MODULE_TYPE_HOST_AICPU:
#if (defined DRV_HOST) && (defined CFG_FEATURE_HOST_AICPU)
            ret = drv_get_host_aicpu_info(devId, infoType, value);
            break;
#else
            return DRV_ERROR_NOT_SUPPORT;
#endif
        case MODULE_TYPE_HCOM_CPU:
            ret = drv_get_hcom_cpu_info(devId, itype, value);
            break;

        case MODULE_TYPE_L2BUFF:
            ret = drv_get_l2_info(devId, itype, value);
            break;

        case MODULE_TYPE_MEMORY:
            ret = drv_get_memory_info_ex(devId, itype, value);
            break;

        default:
            DEVDRV_DRV_INFO("This version does not support this moduleType. (moduleType=%d)\n", mtype);
            return DRV_ERROR_INVALID_VALUE;
    }

    /*  to retain scalability, no error log here */
    if (ret != 0) {
        DEVDRV_DRV_INFO("Result of call halgetdeviceinfo. (ret=%d; module_type=%d; info_type=%d).\n", ret, mtype,
                        itype);
        return ret;
    }

    return DRV_ERROR_NONE;
}
