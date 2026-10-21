/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <linux/limits.h>
#include <pthread.h>
#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "dm_hdc.h"
#include "dm_udp.h"
#include "dsmi_common.h"
#include "dev_mon_api.h"
#include "dsmi_product.h"
#include "dms_user_interface.h"
#include "dms_device_info.h"
#include "dms_cmd_def.h"
#include "config.h"
#include "dsmi_dmp_command.h"
#include "dsmi_upgrade_proc.h"
#include "dms_pcie.h"
#ifdef CFG_DSMI_DEVICE_ENV
#include "drvfault_fault.h"
#include "drvfault_msg.h"
#endif
#include "drv_type.h"
#include "devdrv_user_common.h"
#include "dms_fault.h"
#include "udis_user.h"
#include "ascend_hal.h"
#include "ascend_hal_external.h"
#include "ascend_dev_num.h"
#include "dsmi_adapt.h"
#include "pbl_urd_common.h"
#include "udis_user.h"
#include "dsmi_common_interface.h"
#ifdef DRV_HOST
#include "dmc_feature_user.h"
#endif

#define DSMI_GET_FLASH_COUNT 0xff

#ifdef CFG_FEATURE_VFIO_SOC
#define DSMI_HOT_RESET_SLEEP_TIME 50000
#endif

/* add for power and reset */
#ifdef CFG_FEATURE_POWER_COMMAND
#define DSMI_DMP_POWER_SET_SCRIPT "/var/dmp_power_set.sh"
#define DSMI_DMP_POWER_CMD_POWEROFF "poweroff"
#define DSMI_DMP_POWER_CMD_RESET "reboot"
#endif

#define DSMI_SEC_SIGN_CFG "/etc/pss.cfg"
#define DSMI_SEC_SIGN_CFG_MODE 0640

#define PACKAGE_TYPE_ABL_PATCH 1
#define PACKAGE_TYPE_ABL_UBE_MGMT_PATCH 2

static const char *g_user_config_no_support_list[] = {
#ifndef CFG_FEATURE_SUPPORT_SSH_CONFIG
    SSH_CONFIG_NAME,
#endif
#ifndef CFG_FEATURE_SUPPORT_P2P_CONFIG
    P2P_MEM_CONFIG_NAME,
#endif
#ifndef CFG_FEATURE_SUPPORT_CCPU_USR_CERT_CONFIG
    CCPU_USR_CERT_CONFIG_NAME,
#endif
#ifndef CFG_FEATURE_SUPPORT_AUTH_ENABLE_SIGN
    AUTH_CONFIG_ENABLE_NAME,
#endif
#ifndef CFG_FEATURE_SUPPORT_UBMEM_RETRY_FLAG
    UBMEM_RETRY_FLAG_NAME,
#endif
};

#define HIGH_16_BITS 16
#ifndef CHECK_DEVICE_BUSY
#define CHECK_DEVICE_BUSY((devid), (ret))                                            \
    {                                                                                \
        if ((ret) == (int)DRV_ERROR_RESOURCE_OCCUPIED) {                             \
            DEV_MON_ERR("Device is busy. (device_id=%d, ret=%d)\n", (devid), (ret)); \
            return (ret);                                                            \
        }                                                                            \
    }
#endif

typedef struct {
    DSMI_MAIN_CMD main_cmd;
    unsigned int sub_cmd_start;
    unsigned int sub_cmd_end;
    drvError_t (*callback)(unsigned int, DSMI_MAIN_CMD, unsigned int, const void *, unsigned int);
    int (*cmd_check)(void); /* check not support docker or others. */
} drv_set_dev_info_cmd_t;

struct udis_davinci_info_adapter {
    int device_type;
    int info_type;
    const char *name;
    bool enable_dmp;
    int (*callback)(unsigned int device_id, const char *name, unsigned int *result_data);
};

struct udis_device_info_adapter {
    DSMI_MAIN_CMD main_cmd;
    unsigned int sub_cmd;
    const char *name;
    bool enable_dmp;
    int (*callback)(unsigned int device_id, const char *name, void *buf, unsigned int *size);
};

/* Max means not limit */
#define DSMI_DEV_INFO_SUB_CMD_MAX (0xffffffff)

int dsmi_dft_get_elable(int device_id, int item_type, char *elabel_data, int *len)
{
#ifdef CFG_SOC_PLATFORM_RC
    int dev_index;
    int epprom_index;

    dev_index = (int)(((unsigned int)device_id) & (0xFFFFU));
    epprom_index = (int)((unsigned int)device_id >> 16); // 16 high bit is soc_type
    DRV_CHECK_RETV(((item_type >= 0) && (item_type <= UCHAR_MAX)), DRV_ERROR_PARA_ERROR);
    return dsmi_cmd_dft_get_elabel(dev_index, (unsigned char)item_type, epprom_index, elabel_data, len);
#else
    (void)device_id;
    (void)item_type;
    (void)elabel_data;
    (void)len;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_get_device_count(int *device_count)
{
    int ret;

    if (device_count == NULL) {
        DEV_MON_ERR("dsmi_get_device_count parameter error! \n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = drvGetDevNum((uint32_t *)device_count);
    if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED) {
        DEV_MON_ERR("device is busy. (ret=%d)\n", ret);
        return ret;
    }

    if ((*device_count) == 0) {
        DEV_MON_ERR("drvGetDevNum call error. (ret=0x%x, device_count=%d)\n", ret, *device_count);
        return DRV_ERROR_INNER_ERR;
    }

    if (ret != 0) {
        DEV_MON_ERR("drvGetDevNum call error! (ret=0x%x)\n", ret);
        return ret;
    }

    DEV_MON_DEBUG(" device count = 0x%x, ret = 0x%x\n", *device_count, ret);

    return ret;
}

int dsmi_get_all_device_count(int *all_device_count)
{
    drvError_t ret;

    if (all_device_count == NULL) {
        dev_upgrade_err("dsmi_get_all_device_count parameter error!\n");
        return DRV_ERROR_PARA_ERROR;
    }

    *all_device_count = 0; // only stub at present, wait for implementation
    ret = drvGetDevProbeNum((uint32_t *)all_device_count);
    if (ret != DRV_ERROR_NONE) {
        dev_upgrade_ex_notsupport_err(ret, "drvGetDevProbeNum call error!, ret = 0x%x, all_device_count = 0x%x\n", ret,
                                      *all_device_count);
        return ret;
    }

    dev_upgrade_debug(" all device count = 0x%x\n", *all_device_count);
    return 0;
}

int dsmi_list_device(int device_id_list[], int count)
{
    int ret;
    int i = 0;

    if ((count <= 0) || (device_id_list == NULL) || (count > MAX_DEVICE_COUNT)) {
        dev_upgrade_err("dsmi_list_device parameter error! count %d\n", count);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = drvGetLocalDevIDs((uint32_t *)device_id_list, (uint32_t)count);
    if (ret != 0) {
        dev_upgrade_err(" get device id fail ret  = 0x%x, count = %d\n", ret, count);
        return ret;
    }

    for (i = 0; i < count; i++) {
        dev_upgrade_debug(" device id = 0x%x!\n", device_id_list[i]);
    }

    dev_upgrade_debug("dsmi_list_device ret = 0x%x\n", ret);
    return ret;
}

int dsmi_list_all_device(int device_ids[], int count)
{
    int ret;

    if ((device_ids == NULL) || (count <= 0)) {
        dev_upgrade_err("Parameter error, (device_ids_is_null=%d; count_is_null=%d)\n", device_ids == NULL, count == 0);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = DmsGetAllDeviceList(device_ids, count);
    if (ret != 0) {
        dev_upgrade_ex_notsupport_err(ret, "Get all device listfail. (ret = %d)\n", ret);
        return ret;
    }
    return 0;
}

int dsmi_enable_container_service(void)
{
    int ret;

    DEV_MON_EVENT("enable container service, (user id=%u)\n", getuid());

    ret = drvStartContainerServe();
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "dsmi_enable_container_service fail ret = %d!\n", ret);
        return ret;
    }
    return 0;
}

int dsmi_get_phyid_from_logicid(unsigned int logicid, unsigned int *phyid)
{
    return drvDeviceGetPhyIdByIndex(logicid, phyid);
}

int dsmi_get_logicid_from_phyid(unsigned int phyid, unsigned int *logicid)
{
    return drvDeviceGetIndexByPhyId(phyid, logicid);
}

int dsmi_create_vdevice(unsigned int devid, unsigned int vdev_id, struct dsmi_create_vdev_res_stru *vdev_res,
                        struct dsmi_create_vdev_result *vdev_result)
{
    DEV_MON_EVENT("The create-virtual-device details. (uid=%u; devid=%u; vdev_id=%u)\n", getuid(), devid, vdev_id);
    return halDrvCreateVdevice(devid, vdev_id, vdev_res, vdev_result);
}

int dsmi_destroy_vdevice(unsigned int devid, unsigned int vdevid)
{
    DEV_MON_EVENT("The destroy-virtual-device details. (uid=%u; devid=%u; vdev_id=%u)\n", getuid(), devid, vdevid);
    return drvDestroyVdevice(devid, vdevid);
}

int dsmi_get_resource_info(unsigned int devid, struct dsmi_resource_para *para, struct dsmi_resource_info *info)
{
    return halDrvGetDeviceResourceInfo(devid, para, info);
}

#ifdef CFG_FEATURE_HEALTH_BBOX
int dsmi_get_device_health(int device_id, unsigned int *phealth)
{
    unsigned char device_health = 0;
    unsigned int dev_num = 0;
    int ret;

    if (phealth == NULL) {
        DEV_MON_ERR("device_id:%d dsmi_get_device_health parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_check_device_id(device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret == DRV_ERROR_INVALID_DEVICE) {
        DEV_MON_ERR("dsmi_get_device_health: device_id:%d , is not in the alive device list\n", device_id);
        *phealth = 0xFFFFFFFF;
        return 0;
    } else if (ret != 0) {
        DEV_MON_ERR("dsmi_get_device_health: device_id:%d, call check_device_id fail, please retry\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = drvGetDevNum(&dev_num);
    if (ret != 0) {
        DEV_MON_ERR("Get device number failed. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
#if ((defined CFG_SOC_PLATFORM_MINI) && (defined UPGRADE_DEVICE))
    /* when the DSMI module operates on the device side, there is no need to verify the validity of the device ID */
    if ((device_id < 0) || ((unsigned int)device_id >= dev_num)) {
        device_id = 0;
    }
#endif

    ret = dsmi_cmd_get_device_health(device_id, &device_health);
    if (ret == (int)DRV_ERROR_NOT_SUPPORT) {
        DEV_MON_DEBUG("The module is initializing. (dev_id=%d, ret=%d)\n", device_id, ret);
        return DRV_ERROR_TRY_AGAIN;
    } else if (ret != OK) {
        DEV_MON_ERR("dsmi_cmd_get_device_health device_id:%d, error:%d!\n", device_id, ret);
        return ret;
    }

    *phealth = device_health;
    return ret;
}
#else
int dsmi_get_device_health(int device_id, unsigned int *phealth)
{
    unsigned int dms_status = 0;
#ifdef DRV_HOST
    unsigned int boot_status, device_status;
#endif
    int ret;

    if (phealth == NULL) {
        DEV_MON_ERR("device_id:%d dsmi_get_device_health parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_check_device_id(device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret == DRV_ERROR_INVALID_DEVICE) {
        DEV_MON_ERR("dsmi_get_device_health: device_id:%d , is not in the alive device list\n", device_id);
        *phealth = 0xFFFFFFFFU;
        return 0;
    } else if (ret != 0) {
        DEV_MON_ERR("dsmi_get_device_health: device_id:%d, call check_device_id fail, please retry\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

#ifdef DRV_HOST
    ret = drvGetDeviceStatus((unsigned int)device_id, &boot_status);
    if (ret) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "drvGetDeviceStatus failed device_id:%d, ret = %d\n", device_id, ret);
        return ret;
    }

    if (boot_status != DSMI_SYSTEM_START_FINISH) {
        DEV_MON_WARNING("dsmi_cmd_get_device_health initing:%d, device_id:%d!\n", ret, device_id);
        *phealth = 0xFFFFFFFF;
        return DRV_ERROR_TRY_AGAIN;
    }

    ret = drvDeviceStatus((uint32_t)device_id, &device_status);
    if (ret) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "drvDeviceStatusl failed device_id:%d, ret = %d\n", device_id, ret);
        return ret;
    }

    if (device_status == DRV_STATUS_COMMUNICATION_LOST) {
        DEV_MON_ERR("drvGetDeviceStatus failed device_id:%d, ret = %d\n", device_id, ret);
        *phealth = STATE_FATAL;
        return ret;
    }
#endif
#ifndef CFG_FEATURE_NEW_EVENT_CODE
    ret = drvDeviceHealthStatus(device_id, phealth);
    if (ret) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Failed to invoke drvDeviceHealthStatus. (device_id=%d; ret=%d)\n", device_id,
                                  ret);
        return ret;
    }
#else
    *phealth = STATE_NORMAL;
#endif

    ret = DmsGetHealthCode((u32)device_id, &dms_status);
    if (ret == (int)DRV_ERROR_NOT_SUPPORT) {
    } else if (ret != 0) {
        return DRV_ERROR_IOCRL_FAIL;
    }

    *phealth = dms_status > *phealth ? dms_status : *phealth;

    return (*phealth) > (unsigned int)STATE_FATAL ? DRV_ERROR_INVALID_VALUE : DRV_ERROR_NONE;
}
#endif

int dsmi_get_device_errorcode(int device_id, int *errorcount, unsigned int *perrorcode)
{
    int ret;
#ifndef CFG_FEATURE_DMS_EVENT_DISTRIBUTE
    int i, j, event_cnt = 0;
    unsigned int event_code[DMANAGE_ERROR_ARRAY_NUM] = {0};
#endif
    if (errorcount == NULL || perrorcode == NULL) {
        DEV_MON_ERR("The errorcount or perrorcode is NULL. (errorcount_is_null=%d; perrorcode_is_null=%d)\n",
                    errorcount == NULL, perrorcode == NULL);
        return DRV_ERROR_INVALID_HANDLE;
    }
    *errorcount = 0;

    ret = dsmi_check_device_id(device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0) {
        DEV_MON_ERR("dsmi check device id fail, device id = %d.\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

#ifndef CFG_FEATURE_NEW_EVENT_CODE
    ret = dmanage_get_device_errorcode((unsigned int)device_id, errorcount, perrorcode, DMANAGE_ERROR_ARRAY_NUM);
    if (ret != 0) {
        return ret;
    }
#endif

#ifndef CFG_FEATURE_DMS_EVENT_DISTRIBUTE
    ret = DmsGetEventCode((u32)device_id, &event_cnt, event_code, DMANAGE_ERROR_ARRAY_NUM);
    if (ret == (int)DRV_ERROR_NOT_SUPPORT) {
    } else if (ret != 0) {
        DEV_MON_ERR("Get event code failed. (dev_id=%d; ret=%d).\n", device_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    for (i = *errorcount, j = 0; (i < DMANAGE_ERROR_ARRAY_NUM) && (j < event_cnt); i++, j++) {
        perrorcode[i] = event_code[j];
    }
    *errorcount = (*errorcount + event_cnt) > DMANAGE_ERROR_ARRAY_NUM ? DMANAGE_ERROR_ARRAY_NUM :
                                                                        (*errorcount + event_cnt);
#endif
#ifdef ENABLE_BUILD_PRODUCT
    ret = error_code_filter(perrorcode, errorcount);
    if (ret != OK) {
        DEV_MON_ERR("error code filter failed:%d\n", ret);
        return DRV_ERROR_INVALID_HANDLE;
    }
#endif

    return DRV_ERROR_NONE;
}

int dsmi_query_errorstring(int device_id, unsigned int errorcode, unsigned char *perrorinfo, int buffsize)
{
    int ret;
    int tmp_size;

    if (perrorinfo == NULL) {
        DEV_MON_ERR("Invalid handle.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (buffsize <= 0) {
        DEV_MON_ERR("Invalid parameter. (buffsize=%d)\n", buffsize);
        return DRV_ERROR_INVALID_VALUE;
    }

    tmp_size = buffsize > DSMI_BB_EVENTSTR_LENGTH ? DSMI_BB_EVENTSTR_LENGTH : buffsize;
    ret = dsmi_cmd_get_errorstring(device_id, errorcode, perrorinfo, tmp_size);
    return ret;
}

STATIC int udis_get_dev_info(int dev_id, struct udis_dev_info *info, void *buf, unsigned int buf_size)
{
    int ret;

    ret = halUdisGetDeviceInfo((unsigned int)dev_id, info);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Failed to get udis info. (dev_id=%d; module_type=%u; name=%s; ret=%d)\n",
                                  dev_id, info->module_type, info->name, ret);
        return ret;
    }

    if (info->data_len != buf_size) {
        DEV_MON_ERR("Data len is not equal to bufsize(dev_id=%d; module_type=%u; name=%s; data_len=%u; buf_size=%u)\n",
                    dev_id, info->module_type, info->name, info->data_len, buf_size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = memcpy_s(buf, buf_size, info->data, info->data_len);
    if (ret != 0) {
        DEV_MON_ERR("Failed to invoke memcpy_s to copy data. (dev_id=%d; module_type=%u; name=%s; ret=%d)\n", dev_id,
                    info->module_type, info->name, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

STATIC int udis_get_lp_info(int device_id, const char *name, void *buf, unsigned int buf_size)
{
    int ret;
    struct udis_dev_info info = {0};

    if ((name == NULL) || (buf == NULL)) {
        DEV_MON_ERR("name or buf is NULL. (device_id=%d; name_is_null=%d; buf_is_null=%d)\n", device_id, name == NULL,
                    buf == NULL);
        return DRV_ERROR_PARA_ERROR;
    }

    info.module_type = UDIS_MODULE_LP;
    ret = strcpy_s(info.name, UDIS_MAX_NAME_LEN, name);
    if (ret != 0) {
        DEV_MON_ERR("Failed to invoke strcpy_s to copy info name. (dev_id=%d; name=%s; ret=%d)\n", device_id, name,
                    ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = udis_get_dev_info(device_id, &info, buf, buf_size);
    if (ret != 0) {
        return ret;
    }

    DEV_MON_DEBUG("success udis get lp info. (dev_id=%d; name=%s)\n", device_id, name);
    return 0;
}

STATIC int udis_get_mem_info(int device_id, const char *name, void *buf, unsigned int buf_size)
{
    int ret;
    struct udis_dev_info info = {0};

    if ((name == NULL) || (buf == NULL)) {
        DEV_MON_ERR("name or buf is NULL. (device_id=%d; name_is_null=%d; buf_is_null=%d)\n", device_id, name == NULL,
                    buf == NULL);
        return DRV_ERROR_PARA_ERROR;
    }

    info.module_type = UDIS_MODULE_MEMORY;
    ret = strcpy_s(info.name, UDIS_MAX_NAME_LEN, name);
    if (ret != 0) {
        DEV_MON_ERR("Failed to invoke strcpy_s to copy info name. (dev_id=%d; name=%s; ret=%d)\n", device_id, name,
                    ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = udis_get_dev_info(device_id, &info, buf, buf_size);
    if (ret != 0) {
        return ret;
    }

    DEV_MON_DEBUG("success udis get memory info. (dev_id=%d; name=%s)\n", device_id, name);
    return 0;
}

int dsmi_get_device_temperature(int device_id, int *ptemperature)
{
#ifdef CFG_FEATURE_DMS_ARCH_V1
    signed short chip_temp_data = 0;
    int ret;

    if (ptemperature == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_device_temperature parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = udis_get_lp_info(device_id, "soc_max_temp", &chip_temp_data, sizeof(chip_temp_data));
    if (ret == 0) {
        *ptemperature = (signed int)chip_temp_data;
        return 0;
    }

    ret = dsmi_cmd_get_device_temperature(device_id, &chip_temp_data);
    if (ret != OK) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_get_device_temperature  error:%d!\n", device_id, ret);
        return ret;
    }
    *ptemperature = (signed int)chip_temp_data;
    return ret;
#else
    (void)device_id;
    (void)ptemperature;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_get_device_power_info(int device_id, struct dsmi_power_info_stru *pdevice_power_info)
{
#ifdef CFG_FEATURE_POWER
    int ret;
    unsigned int power = 0;

    if (pdevice_power_info == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_device_power_info parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = udis_get_lp_info(device_id, "power_limit", &power, sizeof(power));
    if (ret == 0) {
        if (power == 0) {
            DEV_MON_WARNING("udis get power is zero. (device_id=%d; ret=%d)\n", device_id, ret);
        } else {
            pdevice_power_info->power = (unsigned short)power;
            return 0;
        }
    }

    return dsmi_cmd_get_device_power_info(device_id, &(pdevice_power_info->power));
#else
    (void)device_id;
    (void)pdevice_power_info;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_get_pcie_info(int device_id, struct tag_pcie_idinfo *pcie_idinfo)
{
#ifndef CFG_SOC_PLATFORM_RC
    int ret;
    struct devdrv_device_info dev_info = {0};

    ret = drvGetDevInfo((uint32_t)(device_id), &dev_info);
    if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED) {
        return DRV_ERROR_RESOURCE_OCCUPIED;
    }

    /* get bus, deviceid and func */
    ret = halDrvDeviceGetPcieIdInfo((uint32_t)device_id, pcie_idinfo);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d drvDeviceGetPcieIdInfo call error ret = %d!\n", device_id, ret);
        return ret;
    }

    return 0;
#else
    (void)device_id;
    (void)pcie_idinfo;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_get_device_voltage(int device_id, unsigned int *pvoltage)
{
    unsigned short device_voltage = 0;
    int ret;

    if (pvoltage == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_device_voltage parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = udis_get_lp_info(device_id, "aic_volt", pvoltage, sizeof(unsigned int));
    if (ret == 0) {
        if (*pvoltage == 0) {
            DEV_MON_WARNING("udis get voltage is zero. (device_id=%d; ret=%d)\n", device_id, ret);
        } else {
            *pvoltage = *pvoltage / 10; /* The voltage needs to be divided by 10 for calculation. */
            return 0;
        }
    }

    ret = dsmi_cmd_get_device_voltage(device_id, &device_voltage);
    if (ret != OK) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_get_device_voltage  error:%d!\n", device_id, ret);
        return ret;
    }
    *pvoltage = device_voltage;

    return ret;
}

STATIC int udis_get_davinci_lp_info(unsigned int device_id, const char *name, unsigned int *result_data)
{
    return udis_get_lp_info((int)device_id, name, result_data, sizeof(unsigned int));
}

STATIC int dsmi_get_hbm_size_from_udis(int device_id, struct dsmi_hbm_info_stru *pdevice_hbm_info)
{
    int ret = 0;
    struct udis_dev_info udis_hbm_info = {0};
    unsigned long long total_size, free_size;

    udis_hbm_info.module_type = 0;
    ret = strcpy_s(udis_hbm_info.name, UDIS_MAX_NAME_LEN, "hbm_mem_info");
    if (ret != 0) {
        DEV_MON_ERR("strcpy hbm mem info failed. (device_id=%d; ret=%d)\n", device_id, ret);
        return ret;
    }
    ret = halUdisGetDeviceInfo((unsigned int)device_id, &udis_hbm_info);
    if (ret != 0) {
        return ret;
    }

    total_size = ((struct udis_mem_info *)(udis_hbm_info.data))->medium_mem_info.total_size;
    free_size = ((struct udis_mem_info *)(udis_hbm_info.data))->medium_mem_info.free_size;

    if (total_size == 0) {
        DEV_MON_WARNING("udis get hbm total size is zero. (device_id=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    pdevice_hbm_info->memory_size = total_size;
    pdevice_hbm_info->memory_usage = total_size - free_size;

    return ret;
}

#define PERCENT_VALUE 100
STATIC int udis_get_hbm_rate(unsigned int device_id, const char *name, unsigned int *result_data)
{
    int ret = 0;
    struct dsmi_hbm_info_stru hbm_info = {0};
    (void)name;

    ret = dsmi_get_hbm_size_from_udis((int)device_id, &hbm_info);
    if (ret != 0) {
        DEV_MON_DEBUG("failed udis get hbm info. (device_id=%d; ret=%d)\n", device_id, ret);
        return ret;
    }

    if (hbm_info.memory_size == 0) {
        DEV_MON_WARNING("udis get hbm total size is zero. (device_id=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    *result_data = (unsigned int)(hbm_info.memory_usage * PERCENT_VALUE / hbm_info.memory_size);
    return ret;
}

STATIC int dsmi_udis_get_hbm_size(unsigned int device_id, const char *name, void *buf, unsigned int *size)
{
    int ret = 0;
    struct dsmi_hbm_info_stru hbm_info = {0};
    struct dsmi_memory_info *memory_info_out;
    (void)name;

    if (buf == NULL || *size < sizeof(struct dsmi_memory_info)) {
        DEV_MON_ERR("The input parameters are incorrect. (dev_id=%u; buf_is_null=%d; size=%u)\n", device_id,
                    buf == NULL, *size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_get_hbm_size_from_udis((int)device_id, &hbm_info);
    if (ret != 0) {
        return ret;
    }

    memory_info_out = (struct dsmi_memory_info *)buf;
    memory_info_out->total_size = (unsigned int)hbm_info.memory_size;
    memory_info_out->used_size = (unsigned int)hbm_info.memory_usage;
    *size = sizeof(struct dsmi_memory_info);

    return 0;
}

STATIC int udis_get_hbm_bandwidth_util(unsigned int device_id, const char *name, unsigned int *result_data)
{
    int ret;
    struct udis_dev_info info = {0};

    if (result_data == NULL) {
        DEV_MON_ERR("result_data is NULL. (dev_id=%u)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    info.module_type = UDIS_MODULE_MEMORY;
    ret = strcpy_s(info.name, UDIS_MAX_NAME_LEN, name);
    if (ret != 0) {
        DEV_MON_ERR("Failed to invoke strcpy_s to copy info name. (dev_id=%u; ret=%d)\n", device_id, ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = halUdisGetDeviceInfo(device_id, &info);
    if (ret != 0) {
        return ret;
    }

    *result_data = *(unsigned int *)info.data;
    return 0;
}

static struct udis_davinci_info_adapter g_udis_get_davinci_info_table[] = {
    {REQ_D_INFO_DEV_TYPE_AICORE0, REQ_D_INFO_INFO_TYPE_FREQ, "aic0_freq", true, udis_get_davinci_lp_info},
    {REQ_D_INFO_DEV_TYPE_AICORE1, REQ_D_INFO_INFO_TYPE_FREQ, "aic1_freq", true, udis_get_davinci_lp_info},
    {REQ_D_INFO_DEV_TYPE_HBM, REQ_D_INFO_INFO_TYPE_FREQ, "hbm_freq", true, udis_get_davinci_lp_info},
    {REQ_D_INFO_DEV_TYPE_HBM, REQ_D_INFO_INFO_TYPE_RATE, "hbm_rate", true, udis_get_hbm_rate},
    {REQ_D_INFO_DEV_TYPE_HBM_BW, REQ_D_INFO_INFO_TYPE_RATE, "bandwidth_util", true, udis_get_hbm_bandwidth_util},
#ifdef CFG_FEATURE_UDIS_UTIL_FROM_TS
    {REQ_D_INFO_DEV_TYPE_CPU, REQ_D_INFO_INFO_TYPE_RATE, "aicore_util", false, dms_get_util_from_udis},
    {REQ_D_INFO_DEV_TYPE_VECTOR, REQ_D_INFO_INFO_TYPE_RATE, "aiv_util", false, dms_get_util_from_udis},
    {REQ_D_INFO_DEV_TYPE_ACC, REQ_D_INFO_INFO_TYPE_RATE, "stars_util", false, dms_get_util_from_udis},
    {REQ_D_INFO_DEV_TYPE_AI_CUBE, REQ_D_INFO_INFO_TYPE_RATE, "aic_util", false, dms_get_util_from_udis},
#endif
#ifdef CFG_FEATURE_SUPPORT_UDIS
    {REQ_D_INFO_DEV_TYPE_AI_CPU, REQ_D_INFO_INFO_TYPE_RATE, "aicpu_util", true, dms_get_cpu_rate_from_udis},
    {REQ_D_INFO_DEV_TYPE_CTRL_CPU, REQ_D_INFO_INFO_TYPE_RATE, "ccpu_util", true, dms_get_cpu_rate_from_udis},
#endif
};

int dsmi_get_davinchi_info(int device_id, int device_type, int info_type, unsigned int *result_data)
{
    DSMI_DAVINCHI_INFO davinchi_data = {{0}};
    int ret;
    unsigned int i;
    unsigned int table_size = sizeof(g_udis_get_davinci_info_table) / sizeof(g_udis_get_davinci_info_table[0]);

    if (device_type < (int)REQ_D_INFO_DEV_TYPE_MEM || device_type >= (int)REQ_D_INFO_DEV_TYPE_MAX_INVALID_VALUE ||
        result_data == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_davinci_info parameter error, device_type = %d!\n", device_id, device_type);
        return DRV_ERROR_PARA_ERROR;
    }

    for (i = 0; i < table_size; i++) {
        if ((g_udis_get_davinci_info_table[i].device_type == device_type) &&
            (g_udis_get_davinci_info_table[i].info_type == info_type)) {
            ret = g_udis_get_davinci_info_table[i].callback((unsigned int)device_id,
                                                            g_udis_get_davinci_info_table[i].name, result_data);
            if (ret == 0) {
                if ((info_type == REQ_D_INFO_INFO_TYPE_FREQ) && (*result_data == 0)) {
                    DEV_MON_WARNING("udis get frequency is zero. (device_id=%d; ret=%d)\n", device_id, ret);
                } else {
                    return ret;
                }
            }
            if (!g_udis_get_davinci_info_table[i].enable_dmp) {
                DEV_MON_ERR("Failed to get davinci info from udis. (dev_id=%d; device_type=%d; info_type=%d; ret=%d)\n",
                            device_id, device_type, info_type, ret);
                return ret;
            }
        }
    }

    davinchi_data.info.device_type = (unsigned char)device_type & 0xFu; /* bit field: 4 */
    davinchi_data.info.info_type = (unsigned char)info_type & 0xFu;     /* bit field: 4 */

    return dsmi_cmd_get_davinchi_info(device_id, davinchi_data.data, result_data);
}

int dsmi_get_device_utilization_rate(int device_id, int device_type, unsigned int *putilization_rate)
{
    return _dsmi_get_device_utilization_rate(device_id, device_type, putilization_rate);
}

int dsmi_get_device_frequency(int device_id, int device_type, unsigned int *pfrequency)
{
    return dsmi_get_davinchi_info(device_id, device_type, REQ_D_INFO_INFO_TYPE_FREQ, pfrequency);
}

int dsmi_get_device_flash_count(int device_id, unsigned int *pflash_count)
{
    if (pflash_count == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_device_flash_count parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    return dsmi_cmd_get_device_flash_count(device_id, pflash_count);
}

int dsmi_get_device_flash_info(int device_id, unsigned int flash_index, dm_flash_info_stru *pflash_info)
{
    if (pflash_info == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_device_flash_info parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (flash_index >= DSMI_GET_FLASH_COUNT) {
        DEV_MON_ERR("devid %d flash_index parameter error! (flash_index=%d, valid range=[0, %d))\n", device_id,
                    flash_index, DSMI_GET_FLASH_COUNT);
        return DRV_ERROR_PARA_ERROR;
    }

    return dsmi_cmd_get_device_flash_info(device_id, (unsigned char)flash_index, (DSMI_DEVICE_FLASH_INFO *)pflash_info);
}

int dsmi_get_memory_info(int device_id, struct dsmi_memory_info_stru *pdevice_memory_info)
{
    int ret;
    unsigned int memory_size = 0;
    unsigned int utiliza = 0;
    unsigned int freq = 0;

    if (pdevice_memory_info == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_memory_info parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_get_davinchi_info(device_id, REQ_D_INFO_DEV_TYPE_MEM, REQ_D_INFO_INFO_TYPE_MEM_SIZE, &memory_size);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_get_davinci_info call error, ret=%d.\n", device_id, ret);
        return ret;
    }

    pdevice_memory_info->memory_size = memory_size;

    ret = dsmi_get_davinchi_info(device_id, REQ_D_INFO_DEV_TYPE_MEM, REQ_D_INFO_INFO_TYPE_FREQ, &freq);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d get memory frequency call error, ret=%d.\n", device_id, ret);
        return ret;
    }
    pdevice_memory_info->freq = freq;

    ret = dsmi_get_davinchi_info(device_id, REQ_D_INFO_DEV_TYPE_MEM, REQ_D_INFO_INFO_TYPE_RATE, &utiliza);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d get memory utilization call error, ret=%d.\n", device_id, ret);
        return ret;
    }

    pdevice_memory_info->utiliza = utiliza;

    return ret;
}

#ifdef CFG_FEATURE_DMS_ARCH_V1
STATIC int dsmi_get_hbm_size_info(unsigned int device_id, unsigned int info_type, unsigned int *ret_value)
{
    struct dsmi_resource_para resource_para = {0};
    struct dsmi_resource_info resource_info = {0};
    u64 value = 0;
    int ret;

    resource_para.owner_type = DSMI_DEV_RESOURCE;
    resource_para.resource_type = info_type;
    resource_info.buf = (void *)&value;
    resource_info.buf_len = sizeof(value);

    ret = dsmi_get_resource_info(device_id, &resource_para, &resource_info);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d get hbm total size fail, ret=%d.\n", device_id, ret);
        return ret;
    }

    *ret_value = (unsigned int)value;
    return ret;
}
#endif

#ifdef CFG_FEATURE_DMS_ARCH_V1
STATIC int dsmi_get_hbm_temp(int device_id, int *temp)
{
    int ret;
    short hbm_temp = 0;
    TAG_SENSOR_INFO sensor_info = {0};

    ret = udis_get_lp_info(device_id, "hbm_temp", &hbm_temp, sizeof(hbm_temp));
    if (ret == 0) {
        *temp = hbm_temp;
        return 0;
    }

    ret = dsmi_get_soc_sensor_info(device_id, HBM_TEMP_ID, &sensor_info);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_get_soc_sensor_info call error, ret=%d.\n", device_id, ret);
        return ret;
    }
    *temp = sensor_info.uchar;

    return 0;
}

STATIC int dsmi_get_hbm_size(int device_id, struct dsmi_hbm_info_stru *pdevice_hbm_info)
{
    int ret = 0;
    unsigned int mem_size = 0;
    unsigned int mem_free = 0;

    ret = dsmi_get_hbm_size_from_udis(device_id, pdevice_hbm_info);
    if (ret == 0) {
        DEV_MON_DEBUG("get hbm info from udis success. (device_id=%d)\n", device_id);
        return 0;
    }

    ret = dsmi_get_davinchi_info(device_id, REQ_D_INFO_DEV_TYPE_HBM, REQ_D_INFO_INFO_TYPE_MEM_SIZE, &mem_size);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Get hbm total size failed,(devid=%u,ret=%d).\n", device_id, ret);
        return ret;
    }
    pdevice_hbm_info->memory_size = mem_size;

    ret = dsmi_get_hbm_size_info((unsigned int)device_id, DSMI_DEV_HBM_FREE, &mem_free);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Get hbm free size failed,(devid=%u,ret=%d).\n", device_id, ret);
        return ret;
    }
    pdevice_hbm_info->memory_usage = mem_size - mem_free;

    return 0;
}
#endif

int dsmi_get_hbm_info(int device_id, struct dsmi_hbm_info_stru *pdevice_hbm_info)
{
#ifdef CFG_FEATURE_DMS_ARCH_V1
    unsigned int utiliza = 0;
    unsigned int freq = 0;
    int ret;

    if (pdevice_hbm_info == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_hbm_info parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_get_hbm_size(device_id, pdevice_hbm_info);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Get hbm total and usage size failed. (devid=%u; ret=%d).\n", device_id, ret);
        return ret;
    }

    ret = dsmi_get_davinchi_info(device_id, REQ_D_INFO_DEV_TYPE_HBM, REQ_D_INFO_INFO_TYPE_FREQ, &freq);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d get HBM frequency call error, ret=%d.\n", device_id, ret);
        return ret;
    }
    pdevice_hbm_info->freq = freq;

    ret = dsmi_get_hbm_temp(device_id, &pdevice_hbm_info->temp);
    if (ret != 0) {
        return ret;
    }

    // hbm band rate
    ret = dsmi_get_davinchi_info(device_id, REQ_D_INFO_DEV_TYPE_HBM_BW, REQ_D_INFO_INFO_TYPE_RATE, &utiliza);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_get_davinci_info error, ret=%d.\n", device_id, ret);
        return ret;
    }
    pdevice_hbm_info->bandwith_util_rate = utiliza;

    return ret;
#else
    (void)device_id;
    (void)pdevice_hbm_info;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_get_aicore_info(int device_id, struct dsmi_aicore_info_stru *pdevice_aicore_info)
{
    unsigned int curfeq = 0;
    unsigned int freq = 0;
    int ret;

    if (pdevice_aicore_info == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_ai_core_info parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    /* current freq */
    ret = dsmi_get_davinchi_info(device_id, REQ_D_INFO_DEV_TYPE_AICORE0, REQ_D_INFO_INFO_TYPE_FREQ, &curfeq);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d get AICORE0 curfrequency call error, ret = %d.\n", device_id, ret);
        return ret;
    }
    pdevice_aicore_info->curfreq = curfeq;

    /* normal freq */
    ret = dsmi_get_davinchi_info(device_id, REQ_D_INFO_DEV_TYPE_AICORE1, REQ_D_INFO_INFO_TYPE_FREQ, &freq);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d get AICORE1 frequency call error, ret = %d.\n", device_id, ret);
        return ret;
    }
    pdevice_aicore_info->freq = freq;

    return ret;
}

int dsmi_get_aicpu_info(int device_id, struct dsmi_aicpu_info_stru *pdevice_aicpu_info)
{
    int ret;

    if (pdevice_aicpu_info == NULL) {
        DEV_MON_ERR("dsmi_get_aicpu_info parameter error!\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_aicpu_info(device_id, pdevice_aicpu_info);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "get device aicpu info call error, ret = %d.\n", ret);
    }
    return ret;
}

int dsmi_get_enable(int device_id, CONFIG_ITEM config_item, DSMI_DEVICE_TYPE device_type, int *enable_flag)
{
    unsigned char enable_value = 0;
    int ret;

    if (((device_type != DSMI_DEVICE_TYPE_NONE) && (device_type != DSMI_DEVICE_TYPE_SRAM) &&
         (device_type != DSMI_DEVICE_TYPE_HBM) && (device_type != DSMI_DEVICE_TYPE_NPU) &&
         (device_type != DSMI_DEVICE_TYPE_DDR)) ||
        (enable_flag == NULL)) {
        DEV_MON_ERR("devid %d dsmi_get_enable parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (config_item == VDEV_MODE_CONFIG_ITEM) {
        return drvGetVdeviceMode(enable_flag);
    }

    ret = dsmi_cmd_get_enable(device_id, (unsigned char)device_type, (unsigned char)config_item, &enable_value);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_get_enable call error ret = %d!\n", device_id, ret);
        return ret;
    }
    *enable_flag = enable_value;
    return ret;
}

int dsmi_get_ecc_enable(int device_id, DSMI_DEVICE_TYPE device_type, int *enable_flag)
{
    return _dsmi_get_ecc_enable(device_id, device_type, enable_flag);
}

STATIC int udis_get_ecc_cont_info(int device_id, int device_type, DSMI_ECC_STATICS_RESULT *ecc_result)
{
    int ret;
    struct udis_dev_info info = {0};
    struct dsmi_udis_ecc_count_info buf = {0};

    if (device_type != DSMI_DEVICE_TYPE_HBM) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (ecc_result == NULL) {
        DEV_MON_ERR("ecc_result is NULL. (device_id=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    info.module_type = UDIS_MODULE_MEMORY;
    ret = strcpy_s(info.name, UDIS_MAX_NAME_LEN, "ecc_rlt_cnt");
    if (ret != 0) {
        DEV_MON_ERR("Failed to invoke strcpy_s to copy info name. (dev_id=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = udis_get_dev_info(device_id, &info, &buf, sizeof(struct dsmi_udis_ecc_count_info));
    if (ret != 0) {
        return ret;
    } else if (buf.is_vaild == 0) {
        return DRV_ERROR_INNER_ERR;
    }
    *ecc_result = buf.ecc_result;

    return 0;
}

STATIC int dsmi_get_ecc_bit_cnt(int device_id, int device_type, DSMI_ECC_STATICS_RESULT *ecc_result)
{
    int ret;
    DSMI_ECC_STATICS ecc_static = {{0}};

    ret = udis_get_ecc_cont_info(device_id, device_type, ecc_result);
    if (ret == 0) {
        return 0;
    }

    ecc_static.info.device_type = (unsigned char)device_type & 0x3Fu; /* bit field: 6 */
    ecc_static.info.error_type = 0x0;

    ret = dsmi_cmd_get_ecc_info(device_id, ecc_static.udata, ecc_result);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_get_ecc_info call error ret = %d!\n", device_id, ret);
        return ret;
    }
    return 0;
}

int dsmi_get_ecc_info(int device_id, int device_type, struct dsmi_ecc_info_stru *pdevice_ecc_info)
{
    DSMI_ECC_STATICS_RESULT ecc_result = {0};
    int ecc_enable_flag = 1;
    int ret;

    if (pdevice_ecc_info == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_ecc_info parameter error!\n", device_id);
        return -EINVAL;
    }

    ret = dsmi_get_ecc_bit_cnt(device_id, device_type, &ecc_result);
    if (ret != 0) {
        return ret;
    }

#if defined(CFG_SOC_PLATFORM_MINI) || defined(CFG_SOC_PLATFORM_MINIV2)
    ret = dsmi_get_enable(device_id, ECC_CONFIG_ITEM, device_type, &ecc_enable_flag);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d call dsmi_get_ecc_enable fail ret = %d!\n", device_id, ret);
        return ret;
    }
#endif
    pdevice_ecc_info->enable_flag = ecc_enable_flag;

    pdevice_ecc_info->single_bit_error_count = ecc_result.single_bit_error_count;
    pdevice_ecc_info->double_bit_error_count = ecc_result.double_bit_error_count;

    return OK;
}

int dsmi_get_system_time(int device_id, unsigned int *ntime_stamp)
{
    return dsmi_cmd_get_system_time(device_id, ntime_stamp);
}

int dsmi_config_enable(int device_id, CONFIG_ITEM config_item, DSMI_DEVICE_TYPE device_type, int enable_flag)
{
    DSMI_CONFIG_PARA config_enable = {0};

    DEV_MON_EVENT("config enable, (user id=%u; device_id=0x%x; item=0x%x; device_type=0x%x, enable_flag=0x%x)\n",
                  getuid(), device_id, config_item, device_type, enable_flag);

    if (((device_type != DSMI_DEVICE_TYPE_NONE) && (device_type != DSMI_DEVICE_TYPE_SRAM) &&
         (device_type != DSMI_DEVICE_TYPE_HBM) && (device_type != DSMI_DEVICE_TYPE_NPU) &&
         (device_type != DSMI_DEVICE_TYPE_DDR)) ||
        ((enable_flag != 0) && (enable_flag != 1))) {
        DEV_MON_ERR("devid %d parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (config_item == VDEV_MODE_CONFIG_ITEM) {
        return drvSetVdeviceMode(enable_flag);
    }

    config_enable.device_type = (unsigned char)device_type;
    config_enable.config_item = (unsigned char)config_item;
    config_enable.value = (unsigned char)enable_flag;

    return dsmi_cmd_config_enable(device_id, config_enable);
}

int dsmi_config_ecc_enable(int device_id, DSMI_DEVICE_TYPE device_type, int enable_flag)
{
    DEV_MON_EVENT("config ecc enable, (user id=%u; device_id=%d; device_type=%u; enable_flag=%d)\n", getuid(),
                  device_id, device_type, enable_flag);

    return dsmi_config_enable(device_id, ECC_CONFIG_ITEM, device_type, enable_flag);
}

int dsmi_set_mac_addr(int device_id, int mac_id, const char *pmac_addr, unsigned int mac_addr_len)
{
#if defined CFG_SOC_PLATFORM_MINI && !defined CFG_SOC_PLATFORM_RC
    /* only support in rc mode */
    (void)device_id;
    (void)mac_id;
    (void)pmac_addr;
    (void)mac_addr_len;
    return DRV_ERROR_NOT_SUPPORT;
#else
    DSMI_MAC_PARA mac_para = {0};

    DRV_CHECK_RETV((pmac_addr != NULL), DRV_ERROR_PARA_ERROR);
    DRV_CHECK_RETV(((mac_id >= 0) && (mac_id <= UCHAR_MAX)), DRV_ERROR_PARA_ERROR);

    if (mac_addr_len < MAC_ADDR_LEN) {
        DEV_MON_ERR("devid %d mac_addr_len %d not valid\n", device_id, mac_addr_len);
        return DRV_ERROR_PARA_ERROR;
    }

    DEV_MON_EVENT("set mac addr, (user id=%u; device_id=%d; mac_id=%d)\n", getuid(), device_id, mac_id);

    mac_para.mac_id = mac_id;
    mac_para.mac_type = MAC_INFO_TYPE;
    return dsmi_cmd_set_mac_addr(device_id, mac_para, pmac_addr);
#endif
}

int dsmi_get_mac_count(int device_id, int *count)
{
#if defined CFG_SOC_PLATFORM_MINI && !defined CFG_SOC_PLATFORM_RC
    /* only support in rc mode */
    (void)device_id;
    (void)count;
    return DRV_ERROR_NOT_SUPPORT;
#else
    unsigned char dsmi_mac_count = 0;
    int ret;
    if (count == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_mac_count parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_mac_count(device_id, &dsmi_mac_count);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_get_mac_count call error ret = %d!\n", device_id, ret);
        return ret;
    }

    *count = dsmi_mac_count;
    return ret;
#endif
}

int dsmi_get_mac_addr(int device_id, int mac_id, char *pmac_addr, unsigned int mac_addr_len)
{
#if defined CFG_SOC_PLATFORM_MINI && !defined CFG_SOC_PLATFORM_RC
    /* only support in rc mode */
    (void)device_id;
    (void)mac_id;
    (void)pmac_addr;
    (void)mac_addr_len;
    return DRV_ERROR_NOT_SUPPORT;
#else
    if (mac_addr_len < MAC_ADDR_LEN || pmac_addr == NULL) {
        DEV_MON_ERR("devid %d mac_addr_len %d not valid or pmac_addr == NULL\n", device_id, mac_addr_len);
        return DRV_ERROR_PARA_ERROR;
    }

    DRV_CHECK_RETV(((mac_id >= 0) && (mac_id <= UCHAR_MAX)), DRV_ERROR_PARA_ERROR);

    return dsmi_cmd_get_mac_addr(device_id, mac_id, pmac_addr);
#endif
}

int dsmi_set_device_ip_address(int device_id, int port_type, int port_id, ip_addr_t ip_address, ip_addr_t mask_address)
{
    DSMI_PORT_PARA port_para = {0};
    IPADDR_ST mask_addr = {0};
    IPADDR_ST ip_addr = {0};
    int ret;

    DEV_MON_EVENT("set device ip addr, (user id=%u, device_id=%d; port_type=%d; port_id=%d; ip_type=%u)\n", getuid(),
                  device_id, port_type, port_id, ip_address.ip_type);

    DRV_CHECK_RETV(((port_type >= 0) && (port_type <= UCHAR_MAX)), DRV_ERROR_PARA_ERROR);
    DRV_CHECK_RETV(((port_id >= 0) && (port_id <= UCHAR_MAX)), DRV_ERROR_PARA_ERROR);

#ifdef CFG_FEATURE_NETWORK_ROCE
    DRV_CHECK_RETV_DO_SOMETHING(
        (port_type == DEVDRV_ROCE || port_type == DEVDRV_BOND), DRV_ERROR_PARA_ERROR,
        DEV_MON_ERR("Can not set non-roce ip. (devid=%d; port_type=%d)\n", device_id, port_type));
#endif
#ifdef CFG_SOC_PLATFORM_MINI
    DRV_CHECK_RETV_DO_SOMETHING(
        (port_type == DEVDRV_VNIC), DRV_ERROR_PARA_ERROR,
        DEV_MON_ERR("Can not set non-vnic ip. (devid=%d; port_type=%d)\n", device_id, port_type));
#endif

    if ((ip_address.ip_type != IPADDR_TYPE_V4) && (ip_address.ip_type != IPADDR_TYPE_V6)) {
        DEV_MON_ERR("Ip_type only support ipv4 or ipv6. (devid=%d; type=%d)", device_id, ip_address.ip_type);
        return DRV_ERROR_PARA_ERROR;
    }

    port_para.card_type = (unsigned char)port_type;
    port_para.card_info.fields.card_id = (unsigned char)port_id & 0x3Fu;           /* bit field: 6 */
    port_para.card_info.fields.ip_type = (unsigned char)ip_address.ip_type & 0x3u; /* bit field: 2 */

    ret = memcpy_s(&ip_addr, sizeof(IPADDR_ST), &ip_address.u_addr, sizeof(IPADDR_ST));
    if (ret != 0) {
        DEV_MON_ERR("Copy ip_address.u_addr to ip_addr failed. (devid=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    ret = memcpy_s(&mask_addr, sizeof(IPADDR_ST), &mask_address.u_addr, sizeof(IPADDR_ST));
    if (ret != 0) {
        DEV_MON_ERR("Copy mask_address.u_addr to mask_addr failed. (devid=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    return dsmi_cmd_set_device_ip_address(device_id, port_para, ip_addr, mask_addr);
}

int dsmi_get_device_ip_address_from_udis(int device_id, DSMI_PORT_PARA *port_para, IPADDR_ST *ip_addr,
                                         IPADDR_ST *netmask)
{
    int ret = 0;
    struct udis_dev_info get_info = {0};
    DSMI_IP_INFO ip_info = {0};
    const char *ip_type = NULL;

    get_info.module_type = UDIS_MODULE_DEVMNG;
    if (port_para->card_info.fields.ip_type == IPADDR_TYPE_V4) {
        ip_type = "ipv4";
    } else if (port_para->card_info.fields.ip_type == IPADDR_TYPE_V6) {
        ip_type = "ipv6";
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = sprintf_s(get_info.name, sizeof(get_info.name), "%s_%u_%u", ip_type, port_para->card_type,
                    port_para->card_info.fields.card_id);
    if (ret < 0) {
        DEV_MON_WARNING("sprintf_s failed. (ret=%d)\n", ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    get_info.name[UDIS_IP_NAME_LEN] = '\0';
    ret = halUdisGetDeviceInfo((unsigned int)device_id, &get_info);
    if (ret != 0) {
        return ret;
    }

    ret = memcpy_s(&ip_info, sizeof(ip_info), get_info.data, get_info.data_len);
    if (ret != 0) {
        DEV_MON_WARNING("Copy udis ip data failed. (devid=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    *ip_addr = (IPADDR_ST)ip_info.ip_addr;
    *netmask = (IPADDR_ST)ip_info.mask_addr;
    return 0;
}

int dsmi_get_device_ip_address(int device_id, int port_type, int port_id, ip_addr_t *ip_address,
                               ip_addr_t *mask_address)
{
    DSMI_PORT_PARA port_para = {0};
    IPADDR_ST mask_addr = {0};
    IPADDR_ST ip_addr = {0};
    int ret, i;

    if (ip_address == NULL || mask_address == NULL) {
        DEV_MON_ERR("Parameter is invalid. (devid=%d; ip_is_null=%d; mask_is_null=%d)\n", device_id,
                    (ip_address == NULL), (mask_address == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    DRV_CHECK_RETV(((port_type >= 0) && (port_type <= UCHAR_MAX)), DRV_ERROR_PARA_ERROR);
    DRV_CHECK_RETV(((port_id >= 0) && (port_id <= UCHAR_MAX)), DRV_ERROR_PARA_ERROR);

#ifdef CFG_SOC_PLATFORM_MINI
    DRV_CHECK_RETV_DO_SOMETHING(
        (port_type == DEVDRV_VNIC), DRV_ERROR_PARA_ERROR,
        DEV_MON_ERR("Can not get non-vnic ip. (devid=%d; port_type=%d).\n", device_id, port_type));
#endif

    if (ip_address->ip_type != IPADDR_TYPE_V4 && ip_address->ip_type != IPADDR_TYPE_V6) {
        DEV_MON_ERR("Ip_type only support ipv4 or ipv6. (devid=%d; ip_type=%d)\n", device_id, ip_address->ip_type);
        return DRV_ERROR_PARA_ERROR;
    }

    port_para.card_type = (unsigned char)port_type;
    port_para.card_info.fields.card_id = (unsigned char)port_id & 0x3Fu;            /* bit field: 6 */
    port_para.card_info.fields.ip_type = (unsigned char)ip_address->ip_type & 0x3u; /* bit field: 2 */

    ret = dsmi_cmd_get_device_ip_address(device_id, port_para, &ip_addr, &mask_addr);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Dsmi cmd get device ip address error! (devid=%d; ret=%d)\n", device_id, ret);
        return ret;
    }

    ret = memcpy_s(&ip_address->u_addr, sizeof(IPADDR_ST), &ip_addr, sizeof(IPADDR_ST));
    if (ret != 0) {
        DEV_MON_ERR("Copy ip_addr to ip_address->u_addr failed. (devid=%d; ret=%d)\n", device_id, ret);
        return ret;
    }

    ret = memcpy_s(&mask_address->u_addr, sizeof(IPADDR_ST), &mask_addr, sizeof(IPADDR_ST));
    if (ret != 0) {
        DEV_MON_ERR("Copy mask_addr to mask_address->u_addr failed (devid=%d; ret=%d).\n", device_id, ret);
        return ret;
    }

    if (ip_address->ip_type == IPADDR_TYPE_V4) {
        for (i = DSMI_ARRAY_IPV4_NUM; i < DSMI_ARRAY_IPV6_NUM; i++) {
            ip_address->u_addr.ip6[i] = 0;
            mask_address->u_addr.ip6[i] = 0;
        }
    }

    return 0;
}

int dsmi_set_gateway_addr(int device_id, int port_type, int port_id, ip_addr_t gtw_address)
{
    DSMI_PORT_PARA port_para = {0};
    IPADDR_ST gtw_addr = {0};
    int ret;

    DEV_MON_EVENT("set gateway addr, (user id=%u; device_id=%d; port_type=%d; port_id=%d; ip_type=%u)\n", getuid(),
                  device_id, port_type, port_id, gtw_address.ip_type);

#ifdef CFG_FEATURE_NETWORK_ROCE
    DRV_CHECK_RETV_DO_SOMETHING(
        (port_type == DEVDRV_ROCE), DRV_ERROR_PARA_ERROR,
        DEV_MON_ERR("devid %d Cloud can not set non-roce ip, port_type %d.\n", device_id, port_type));
#endif
#ifdef CFG_SOC_PLATFORM_MINI
    DRV_CHECK_RETV_DO_SOMETHING(
        (port_type == DEVDRV_VNIC), DRV_ERROR_PARA_ERROR,
        DEV_MON_ERR("devid %d Mini can not set non-vnic ip, port_type %d.\n", device_id, port_type));
#endif

    if ((gtw_address.ip_type != IPADDR_TYPE_V4) && gtw_address.ip_type != IPADDR_TYPE_V6) {
        DEV_MON_ERR("devid %d ip_type only support ipv4 or ipv6 = %d", device_id, gtw_address.ip_type);
        return DRV_ERROR_PARA_ERROR;
    }

    port_para.card_type = (unsigned char)port_type;
    port_para.card_info.fields.card_id = (unsigned char)port_id & 0x3Fu;            /* bit field: 6 */
    port_para.card_info.fields.ip_type = (unsigned char)gtw_address.ip_type & 0x3u; /* bit field: 2 */

    ret = memcpy_s(&gtw_addr, sizeof(IPADDR_ST), &gtw_address.u_addr, sizeof(IPADDR_ST));
    if (ret != 0) {
        DEV_MON_ERR("devid %d copy gtw_address.u_addr to gtw_addr failed ret = %d.\n", device_id, ret);
        return ret;
    }

    return dsmi_cmd_set_device_gtw_address(device_id, port_para, gtw_addr);
}

int dsmi_get_gateway_addr(int device_id, int port_type, int port_id, ip_addr_t *gtw_address)
{
    DSMI_PORT_PARA port_para = {0};
    IPADDR_ST gtw_addr = {0};
    int ret;
    int i;

    if (gtw_address == NULL) {
        DEV_MON_ERR("devid %d invalid input.\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    DRV_CHECK_RETV(((port_type >= 0) && (port_type <= UCHAR_MAX)), DRV_ERROR_PARA_ERROR);
    DRV_CHECK_RETV(((port_id >= 0) && (port_id <= UCHAR_MAX)), DRV_ERROR_PARA_ERROR);
/* 910_A5/910/910B/910_A3: ROCE */
#if defined(CFG_FEATURE_NETWORK_ROCE)
    DRV_CHECK_RETV_DO_SOMETHING(
        (port_type == DEVDRV_ROCE), DRV_ERROR_PARA_ERROR,
        DEV_MON_ERR("devid %d Cloud can not set non-roce ip, port_type %d.\n", device_id, port_type));
#endif
#ifdef CFG_SOC_PLATFORM_MINI
    DRV_CHECK_RETV_DO_SOMETHING(
        (port_type == DEVDRV_VNIC), DRV_ERROR_PARA_ERROR,
        DEV_MON_ERR("devid %d Mini can not set non-vnic ip, port_type %d.\n", device_id, port_type));
#endif

    if ((gtw_address->ip_type != IPADDR_TYPE_V4) && (gtw_address->ip_type != IPADDR_TYPE_V6)) {
        DEV_MON_ERR("ip_type only supports ipv4 or ipv6. (devid=%d; ip_type=%d)", device_id, gtw_address->ip_type);
        return DRV_ERROR_PARA_ERROR;
    }

    port_para.card_type = (unsigned char)port_type;
    port_para.card_info.fields.card_id = (unsigned char)port_id & 0x3Fu;             /* bit field: 6 */
    port_para.card_info.fields.ip_type = (unsigned char)gtw_address->ip_type & 0x3u; /* bit field: 2 */

    ret = dsmi_cmd_get_device_gtw_address(device_id, port_para, &gtw_addr);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_get_device_gtw_address return %d error!\n", device_id, ret);
        return ret;
    }

    ret = memcpy_s(&gtw_address->u_addr, sizeof(IPADDR_ST), &gtw_addr, sizeof(IPADDR_ST));
    if (ret != 0) {
        DEV_MON_ERR("devid %d copy gtw_addr to gtw_address->u_addr failed ret = %d.\n", device_id, ret);
        return DRV_ERROR_INNER_ERR;
    }

    if (gtw_address->ip_type == IPADDR_TYPE_V4) {
        for (i = DSMI_ARRAY_IPV4_NUM; i < DSMI_ARRAY_IPV6_NUM; i++) {
            gtw_address->u_addr.ip6[i] = 0;
        }
    }

    return 0;
}

STATIC int dsmi_get_device_fan_info(int device_id, int message_type, int fan_id, int *count_speed)
{
    DSMI_FAN_INFO chip_fan_info = {0};
    unsigned int fan_speed_sum = 0;
    unsigned int i = 0;
    int ret;

    if (count_speed == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_device_fan_info parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_fan_info(device_id, &chip_fan_info);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_get_fan_info call error ret = %d!\n", device_id, ret);
        return ret;
    }

    if (message_type == GET_FAN_COUNT) {
        *count_speed = chip_fan_info.fan_num;
    } else {
        if (fan_id > 0 && fan_id <= MAX_FAN_ID) {
            *count_speed = chip_fan_info.fan_speed[fan_id - 1];
        } else {
            for (i = 0; i < chip_fan_info.fan_num && i < MAX_FAN_ID; i++) {
                fan_speed_sum = fan_speed_sum + chip_fan_info.fan_speed[i];
            }
            if (i > 0) {
                *count_speed = (int)(fan_speed_sum / i);
            } else {
                *count_speed = 0;
            }
        }
    }

    return ret;
}

int dsmi_get_fan_count(int device_id, int *count)
{
    return dsmi_get_device_fan_info(device_id, GET_FAN_COUNT, GET_FAN_COUNT, count);
}

int dsmi_get_fan_speed(int device_id, int fan_id, int *speed)
{
    if ((fan_id > MAX_FAN_ID) || (fan_id < MIN_FAN_ID)) {
        DEV_MON_ERR("devid %d dsmi_get_fan_speed error : fan_id = %d, valid range [%d, %d]!\n", device_id, fan_id,
                    MIN_FAN_ID, MAX_FAN_ID);
        return DRV_ERROR_PARA_ERROR;
    }

    return dsmi_get_device_fan_info(device_id, GET_FAN_SPEED, fan_id, speed);
}

int dsmi_pre_reset_soc(int device_id)
{
    DEV_MON_EVENT("pre reset soc, (user id=%u; device_id=%d)\n", getuid(), device_id);

    if ((device_id >= ASCEND_DEV_MAX_NUM) || (device_id < DEVDRV_MIN_DAVINCI_NUM)) {
        DEV_MON_ERR("invalid device id %d, valid range [%d, %d).\n", device_id, DEVDRV_MIN_DAVINCI_NUM,
                    ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    return drvPciePreReset((uint32_t)device_id);
}

int dsmi_rescan_soc(int device_id)
{
    DEV_MON_EVENT("rescan soc, (user id=%u; device_id=%d)\n", getuid(), device_id);

    if ((device_id >= ASCEND_DEV_MAX_NUM) || (device_id < DEVDRV_MIN_DAVINCI_NUM)) {
        DEV_MON_ERR("invalid device id %d, valid range [%d, %d).\n", device_id, DEVDRV_MIN_DAVINCI_NUM,
                    ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    return drvPcieRescan((uint32_t)device_id);
}

int dsmi_hot_reset_soc(int device_id)
{
    DEV_MON_EVENT("hot reset soc, (user id=%u; device_id=%d)\n", getuid(), device_id);
#ifdef CFG_FEATURE_VFIO_SOC
    struct dsmi_create_vdev_res_stru vdev_creat_info = {0};
    struct dsmi_create_vdev_result vdev_result = {0};
    struct dsmi_vdev_query_stru vdev_info = {0};
    unsigned int buf_size = sizeof(struct dsmi_vdev_query_stru);
    unsigned int pf_id;
    int ret;

    if (device_id >= ASCEND_DEV_MAX_NUM || device_id < DEVDRV_MIN_DAVINCI_NUM) {
        DEV_MON_ERR("invalid device id %d, valid range [%d, %d).\n", device_id, DEVDRV_MIN_DAVINCI_NUM,
                    ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    pf_id = 0;
    vdev_info.vdev_id = device_id;

    ret = drvGetSingleVdevInfo(pf_id, DSMI_MAIN_CMD_VDEV_MNG, DSMI_VMNG_SUB_CMD_GET_VDEV_RESOURCE, &vdev_info,
                               &buf_size);
    if (ret != 0) {
        /* No vdev device is available, and reset is not supported. */
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = drvDestroyVdevice(pf_id, vdev_info.vdev_id);
    if (ret != 0) {
        DEV_MON_ERR("Destroy Vdevice failed! (devid=%d; vdevid=%u; ret=%d)\n", pf_id, vdev_info.vdev_id, ret);
        return ret;
    }

    (void)usleep(DSMI_HOT_RESET_SLEEP_TIME);

    vdev_creat_info.computing.aic = vdev_info.query_info.computing.aic;
    vdev_creat_info.base.token = vdev_info.query_info.base.token;
    vdev_creat_info.base.token_max = vdev_info.query_info.base.token_max;
    vdev_creat_info.base.task_timeout = vdev_info.query_info.base.task_timeout;
    ret = strcpy_s(vdev_creat_info.name, DSMI_VDEV_RES_NAME_LEN, vdev_info.query_info.name);
    if (ret != 0) {
        DEV_MON_ERR("Failed to invoke the strcpy_s. (ret=%d).\n", ret);
    }
    ret = (int)halDrvCreateVdevice(pf_id, vdev_info.vdev_id, &vdev_creat_info, &vdev_result);
    if (ret != 0) {
        DEV_MON_ERR("Create Vdevice info failed! (devid=%d; vdevid=%u; ret=%d)\n", pf_id, vdev_info.vdev_id, ret);
        return ret;
    }

    return ret;
#else

    return dsmi_hot_reset_atomic(device_id, DSMI_SUBCMD_HOTRESET_ASSEMBLE);
#endif
}

STATIC int dsmi_pcie_inform_bbox(int device_id)
{
    int ret;
    int i;
    int device_count = INVALID_DEVICE_ID;

    if ((unsigned int)device_id == ALL_DEVICE_RESET_FLAG) {
        /* all chip together hot reset report bbox */
        ret = dsmi_get_device_count(&device_count);
        if (ret != 0) {
            DEV_MON_ERR("dsmi_get_device_count fail.(devid=%d; ret=%d)\n", device_id, ret);
            return ret;
        }
        for (i = 0; i < device_count; i++) {
            ret = drvDeviceResetInform((uint32_t)i);
            if (ret != 0) {
                DEV_MON_ERR("drvDeviceResetInform fail.(devid=%d; ret=%d)\n", i, ret);
                return ret;
            }
        }
    } else {
        /* device id chip alone hot reset report bbox */
        ret = drvDeviceResetInform((uint32_t)device_id);
        if (ret != 0) {
#ifndef DEV_MON_UT
            DEV_MON_EX_NOTSUPPORT_ERR(ret, "device id %d report bbox fail, uid=%d\n", device_id, ret);
#endif
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}
int g_dsmi_hotreset_cmd_convert[DSMI_SUBCMD_HOTRESET_BUTT] = {
    DMS_SUBCMD_HOTRESET_ASSEMBLE, DMS_SUBCMD_HOTRESET_SETFLAG,  DMS_SUBCMD_HOTRESET_CLEARFLAG,
    DMS_SUBCMD_HOTRESET_UNBIND,   DMS_SUBCMD_HOTRESET_RESET,    DMS_SUBCMD_HOTRESET_REMOVE,
    DMS_SUBCMD_HOTRESET_RESCAN,   DMS_SUBCMD_PRERESET_ASSEMBLE, DMS_SUBCMD_PRERESET_ASSEMBLE1};

int dsmi_hot_reset_atomic(int device_id, int dsmi_hotreset_subcmd)
{
    DEV_MON_EVENT("dsmi_hot_reset_atomic, (user id=%u; device_id=%d, dsmi_hotreset_subcmd=%d)\n", getuid(), device_id,
                  dsmi_hotreset_subcmd);

    if (((device_id >= ASCEND_DEV_MAX_NUM) || (device_id < DEVDRV_MIN_DAVINCI_NUM)) &&
        (device_id != (int)ALL_DEVICE_RESET_FLAG)) {
        DEV_MON_ERR("Invalid device id, (dev_id=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    if ((dsmi_hotreset_subcmd < 0) || (dsmi_hotreset_subcmd >= DSMI_SUBCMD_HOTRESET_BUTT)) {
        DEV_MON_ERR("Invalid dsmi_hotreset_subcmd, (dsmi_hotreset_subcmd=%d)\n", dsmi_hotreset_subcmd);
        return DRV_ERROR_PARA_ERROR;
    }
    int ioctl_sub_cmd = g_dsmi_hotreset_cmd_convert[dsmi_hotreset_subcmd];
    int ret;
    if ((ioctl_sub_cmd == DMS_SUBCMD_HOTRESET_ASSEMBLE) || (ioctl_sub_cmd == DMS_SUBCMD_HOTRESET_RESET)) {
        ret = dsmi_pcie_inform_bbox(device_id);
        if (ret != DRV_ERROR_NONE) {
            DEV_MON_ERR("Dsmi_pcie_inform_bbox failed. (ret=%d)\n", ret);
            return ret;
        }
    }
    ret = dms_power_hotreset_common((unsigned int)device_id, ioctl_sub_cmd);
    return ret;
}

int dsmi_get_device_boot_status(int device_id, enum dsmi_boot_status *boot_status)
{
    unsigned int dev_boot_status = 0;
    uint32_t phy_id;
    int ret;

    if (boot_status == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_device_boot_status parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    *boot_status = DSMI_BOOT_STATUS_UNINIT;

    ret = dsmi_check_device_id(device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0) {
        DEV_MON_ERR("device_id didn't exist. (device_id=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = (int)drvDeviceGetPhyIdByIndex((uint32_t)device_id, &phy_id);
    if (ret != 0) {
        DEV_MON_ERR("devid %d transfer to phyid fail, ret=%d.\n", device_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = (int)drvGetDeviceBootStatus((int)phy_id, &dev_boot_status);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d phy_id %u device_boot_status failed ret = %d!\n", device_id, phy_id,
                                  ret);
        return ret;
    }
    *boot_status = dev_boot_status;

    if (*boot_status < DSMI_BOOT_STATUS_FINISH) {
        *boot_status = DSMI_BOOT_STATUS_UNINIT;
    } else if (*boot_status == DSMI_BOOT_STATUS_FINISH) {
        ret = (int)drvGetDeviceStatus((unsigned int)device_id, &dev_boot_status);
        if (ret != 0) {
            DEV_MON_EX_NOTSUPPORT_ERR(ret, "drvGetDeviceStatus failed. (device_id=%d; ret=%d)\n", device_id, ret);
            return ret;
        }
        if (dev_boot_status == (unsigned int)DSMI_SYSTEM_START_FINISH) {
            *boot_status = (unsigned int)DSMI_SYSTEM_START_FINISH;
        }
    } else {
        DEV_MON_ERR("The status obtained by the DSMI is incorrect. (dev_id=%d; boot_status=%d)\n", device_id,
                    dev_boot_status);
        return DRV_ERROR_PARA_ERROR;
    }

    return ret;
}

int dsmi_get_soc_sensor_info(int device_id, int sensor_id, TAG_SENSOR_INFO *tsensor_info)
{
    unsigned char sensorid;
    int ret;
    signed short chip_temp_data;
    unsigned int hbm_array_temp_data = 0;
    DRV_CHECK_RETV(((sensor_id >= 0) && (sensor_id < (int)INVALID_TSENSOR_ID) && (tsensor_info != NULL)),
                   DRV_ERROR_PARA_ERROR);
    sensorid = (unsigned char)sensor_id;
    if (sensorid == SOC_TEMP_ID) {
        ret = udis_get_lp_info(device_id, "soc_max_temp", &chip_temp_data, sizeof(chip_temp_data));
        if (ret == 0) {
            tsensor_info->iint = (signed int)chip_temp_data;
            return 0;
        }
    } else if (sensorid == HBM_ARRAY_TEMP_ID) {
        ret = udis_get_mem_info(device_id, "hbm_array_temp", &hbm_array_temp_data, sizeof(hbm_array_temp_data));
        if (ret == 0) {
            tsensor_info->uint = hbm_array_temp_data;
            return 0;
        }
    }
    return dsmi_cmd_get_soc_sensor_info(device_id, sensorid, tsensor_info);
}

STATIC int dsmi_parse_version_str(int device_id, unsigned char *version_str, unsigned int *len,
                                  struct udis_dev_info *get_info)
{
    int ret;
    ret = halUdisGetDeviceInfo((unsigned int)device_id, get_info);
    if (ret != 0) {
        return ret;
    }
    if (get_info->data_len == 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }
    *len = get_info->data_len;
    ret = memcpy_s(version_str, get_info->data_len, get_info->data, get_info->data_len);
    if (ret != 0) {
        DEV_MON_WARNING("Copy version from udis failed. (devid=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    if (strcmp((char *)version_str, "") == 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

STATIC int dsmi_get_version_from_udis(int device_id, unsigned char component_type, unsigned char *version_str,
                                      unsigned int *len)
{
    int ret;
    struct udis_dev_info get_info = {0};
    unsigned int patch_len = 0;
    unsigned char hot_patch_version[MAX_LINE_LEN + 1] = {0};
    get_info.module_type = UDIS_MODULE_DEVMNG;

    ret = strncpy_s(get_info.name, sizeof(get_info.name), "system_ver", sizeof(get_info.name));
    if (ret != 0) {
        DEV_MON_WARNING("strncpy failed. (devid=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    ret = dsmi_parse_version_str(device_id, version_str, len, &get_info);
    if (ret != 0) {
        return ret;
    }

    ret = strncpy_s(get_info.name, sizeof(get_info.name), "hot_patch_ver", sizeof(get_info.name));
    if (ret != 0) {
        DEV_MON_WARNING("strncpy failed. (devid=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    ret = dsmi_parse_version_str(device_id, hot_patch_version, &patch_len, &get_info);
    if (ret != 0) {
        goto OUT;
    }

    (void)strcat_s((char *)version_str, *len + patch_len, ".");
    (void)strcat_s((char *)version_str, *len + patch_len, (char *)hot_patch_version);

    (void)component_type;
OUT:
    *len = (unsigned int)strlen((char *)version_str);
    return 0;
}

int dsmi_get_version(int device_id, char *version_str, unsigned int version_len, unsigned int *ret_len)
{
    int ret;

    if (version_str == NULL || ret_len == NULL) {
        DEV_MON_ERR("dsmi_get_version param error. (devid=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_check_device_id(device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0) {
        DEV_MON_ERR("dsmi_check_device_id error. (device_id=0x%x, ret=%d)\n", device_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    if (version_len < FW_VERSION_MAX_LENGTH) {
        DEV_MON_ERR("devid %d version_len(%u) error, expect >= FW_VERSION_MAX_LENGTH(%d)!\n", device_id, version_len,
                    FW_VERSION_MAX_LENGTH);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_get_version_from_udis(device_id, (unsigned char)DAVINCHI_SYS_VERSION, (unsigned char *)version_str,
                                     ret_len);
    if (ret != OK) {
        ret = dsmi_cmd_upgrade_get_version(device_id, (unsigned char)DAVINCHI_SYS_VERSION, (unsigned char *)version_str,
                                           ret_len);
        if (ret != OK) {
            DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_upgrade_get_version failed %d\n", device_id, ret);
            return ret;
        }
    }
    // if end of version_str contain '\n', change to '\0'
    if (*ret_len != 0 && *ret_len <= FW_VERSION_MAX_LENGTH) {
        if (version_str[(*ret_len) - 1U] == '\n') {
            version_str[(*ret_len) - 1U] = '\0';
            *ret_len = (*ret_len) - 1U;
        }
    }

    return ret;
}

int dsmi_get_chip_info(int device_id, struct dsmi_chip_info_stru *chip_info)
{
    return halGetChipInfo((unsigned int)device_id, (halChipInfo *)chip_info);
}

int dsmi_get_board_id(int device_id, unsigned int *board_id)
{
    unsigned int sub_boardid = 0;
    int ret;

    if (board_id == NULL) {
        DEV_MON_ERR("device_id %d parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dms_get_basic_info_host((unsigned int)device_id, board_id, DMS_SUBCMD_GET_BOARD_ID_HOST,
                                  sizeof(unsigned int));
    if (ret == 0) {
        return 0;
    }

    ret = dsmi_cmd_get_board_id(device_id, &sub_boardid);
    if (ret != OK) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_get_board_id failed %d\n", device_id, ret);
        return ret;
    }

    *board_id = sub_boardid;
    return ret;
}

STATIC int dsmi_get_pcb_id(int device_id, unsigned int *pcb_id)
{
    unsigned char pcb_id_data = 0;
    int ret;

    if (pcb_id == NULL) {
        DEV_MON_ERR("\rdevid %d dsmi_get_pcb_id parameter error!\r\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dms_get_basic_info_host((unsigned int)device_id, pcb_id, DMS_SUBCMD_GET_PCB_ID_HOST, sizeof(unsigned int));
    if (ret == 0) {
        return 0;
    }

    ret = dsmi_cmd_get_pcb_id(device_id, &pcb_id_data);
    if (ret != OK) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_get_pcb_id failed %d\n", device_id, ret);
        return ret;
    }

    *pcb_id = pcb_id_data;
    return ret;
}

STATIC int dsmi_get_board_information(int device_id, unsigned char info_type, unsigned int *id_value)
{
    int ret = 0;

    if (id_value == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_board_information parameter error\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((info_type != DEV_MON_GET_BOARD_INFO_BOM_ID) && (info_type != DEV_MON_GET_BOARD_INFO_SLOT_ID)) {
        DEV_MON_ERR("devid %d dsmi_get_board_information parameter info type error:%d, "
                    "expect DEV_MON_GET_BOARD_INFO_BOM_ID:%d or DEV_MON_GET_BOARD_INFO_SLOT_ID:%d!\n",
                    device_id, info_type, DEV_MON_GET_BOARD_INFO_BOM_ID, DEV_MON_GET_BOARD_INFO_SLOT_ID);
        return DRV_ERROR_PARA_ERROR;
    }

    if (info_type == DEV_MON_GET_BOARD_INFO_BOM_ID) {
        ret = dms_get_basic_info_host((unsigned int)device_id, id_value, DMS_SUBCMD_GET_BOM_ID_HOST,
                                      sizeof(unsigned int));
    } else if (info_type == DEV_MON_GET_BOARD_INFO_SLOT_ID) {
        ret = dms_get_basic_info_host((unsigned int)device_id, id_value, DMS_SUBCMD_GET_SLOT_ID_HOST,
                                      sizeof(unsigned int));
    }
    if (ret == 0) {
        return 0;
    }

    return dsmi_cmd_get_board_information(device_id, info_type, id_value);
}

int dsmi_get_board_info(int device_id, struct dsmi_board_info_stru *pboard_info)
{
    int ret;
#ifdef CFG_SOC_PLATFORM_MINI
    int ret_get_mem = 0;
    unsigned int board_id = 0;
    unsigned int pcb_id = 0;
    unsigned int bom_id = 0;
    unsigned int slot_id = 0;
#endif

    if (pboard_info == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_board_info parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    /* get board id */
    ret = dsmi_get_board_id(device_id, &(pboard_info->board_id));
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_board_id call error ret = %d!\n", device_id, ret);
        goto FROM_MEM;
    }

    /* get pcb id */
    ret = dsmi_get_pcb_id(device_id, &(pboard_info->pcb_id));
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_get_pcb_id call error ret = %d!\n", device_id, ret);
        goto FROM_MEM;
    }

    /* get bom id */
    ret = dsmi_get_board_information(device_id, DEV_MON_GET_BOARD_INFO_BOM_ID, &(pboard_info->bom_id));
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_get_bom_id call error ret = %d!\n", device_id, ret);
        goto FROM_MEM;
    }

    /* get slot id */
    ret = dsmi_get_board_information(device_id, DEV_MON_GET_BOARD_INFO_SLOT_ID, &(pboard_info->slot_id));
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_get_slot_id call error ret = %d!\n", device_id, ret);
        goto FROM_MEM;
    }

    return 0;

FROM_MEM:
#ifdef CFG_SOC_PLATFORM_MINI
    /* used for reset mini */
    ret_get_mem = drv_device_get_board_from_host_mem(device_id, &board_id, &pcb_id, &bom_id, &slot_id);
    if (ret_get_mem) {
        DEV_MON_ERR("devid %d drv_device_get_board_from_host_mem call error ret = %d!\n", device_id, ret_get_mem);
        return ret;
    }

    pboard_info->board_id = board_id;
    pboard_info->pcb_id = pcb_id;
    pboard_info->bom_id = bom_id;
    pboard_info->slot_id = slot_id;

    return ret_get_mem;
#else
    return ret;
#endif
}

#if defined(CFG_FEATURE_UPGRADE_PATCH_CONFIG) || defined(CFG_FEATURE_UPGRADE_MAMI_PATCH_CONFIG)
STATIC int dsmi_load_patch_cmd_send(int device_id, int pach_type, const char *file_name)
{
    int ret = -EINVAL;

    if (pach_type == PACKAGE_TYPE_ABL_PATCH) {
        dev_upgrade_info("Type is PACKAGE_TYPE_ABL_PATCH. (device_id=%d; file_name=%s)", device_id, file_name);
        ret = upgrade_trans_patch(device_id, file_name);
    } else if (pach_type == PACKAGE_TYPE_ABL_UBE_MGMT_PATCH) {
        dev_upgrade_info("Type is PACKAGE_TYPE_ABL_UBE_MGMT_PATCH. (device_id=%d; file_name=%s)", device_id, file_name);
        ret = upgrade_trans_mami_patch(device_id, file_name);
    }
    if (ret) {
        dev_upgrade_ex_notsupport_err(ret, "transmit file to device patch failed. (device_id=%d; ret=%d; file_name=%s)",
                                      device_id, ret, file_name);
    }

    return ret;
}

STATIC int hotpatch_mode_check(unsigned int dev_id)
{
    int ret;
    unsigned int container_flag;
    unsigned int hostflag;

    ret = dmanage_get_container_flag(&container_flag);
    if (ret != 0) {
        DEV_MON_ERR("Get container flag failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    /* 1: container scenario */
    if (container_flag == 1) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = drvGetHostPhyMachFlag(dev_id, &hostflag);
    if (ret != 0) {
        DEV_MON_ERR("Get host flag failed. (dev_id=%u; ret=%d).\n", dev_id, ret);
        return ret;
    }

    if (hostflag == DEVDRV_HOST_VM_MACH_FLAG) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

STATIC int dsmi_load_one_device_patch(int device_id, int pach_type, const char *file_name)
{
    int ret;
    int sem_id = 0;
    char *file_name_local = NULL;

    ret = hotpatch_mode_check((unsigned int)device_id);
    if (ret != 0) {
        return ret;
    }

    file_name_local = calloc(PATH_MAX, sizeof(char));
    if (file_name_local == NULL) {
        dev_upgrade_err("calloc file name failed for patch, errno=%d.\n", errno);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = strcpy_s(file_name_local, PATH_MAX, file_name);
    if (ret != 0) {
        dev_upgrade_err("strcpy filename to local failed for patch.\n");
        DSMI_FREE(file_name_local);
        return DRV_ERROR_INNER_ERR;
    }

    dev_upgrade_event("It is to upgrade patch. (device_id=%d; uid=%u)\n", device_id, getuid());

    ret = dsmi_check_device_id(device_id);
    if (ret == DRV_ERROR_RESOURCE_OCCUPIED) {
        dev_upgrade_err("device is busy, (device_id=0x%x; ret=%d)\n", device_id, ret);
        DSMI_FREE(file_name_local);
        return ret;
    }
    if (ret != 0) {
        dev_upgrade_err("device_id didn't exist. (device_id=0x%x; ret=%d)\n", device_id, ret);
        DSMI_FREE(file_name_local);
        return DRV_ERROR_PARA_ERROR;
    }

    // get lock
    ret = dsmi_mutex_p((key_t)(device_id + DSMI_UPGRADE_LOCK_TAG), &sem_id, DSMI_MUTEX_WAIT_FOR_EVER);
    if (ret != 0) {
        dev_upgrade_err("get lock failed. (devid=%d; ret=0x%x)\n", device_id, ret);
        DSMI_FREE(file_name_local);
        return DRV_ERROR_INNER_ERR;
    }

    ret = dsmi_load_patch_cmd_send(device_id, pach_type, file_name_local);
    if (ret != 0) {
        dev_upgrade_ex_notsupport_err(ret, "dsmi_load_patch_cmd_send failed. (devid=%d; ret=0x%x)\n", device_id, ret);
        (void)dsmi_mutex_v(sem_id);
        DSMI_FREE(file_name_local);
        return ret;
    }

    (void)dsmi_mutex_v(sem_id);
    DSMI_FREE(file_name_local);

    return ret;
}

STATIC int dsmi_unload_one_device_patch(int device_id)
{
    int ret;
    int sem_id = 0;

    ret = hotpatch_mode_check((unsigned int)device_id);
    if (ret != 0) {
        return ret;
    }

    dev_upgrade_event("It is to upgrade patch. (device_id=%d; uid=%u)\n", device_id, getuid());

    // get lock
    ret = dsmi_mutex_p((key_t)(device_id + DSMI_UPGRADE_LOCK_TAG), &sem_id, DSMI_MUTEX_WAIT_FOR_EVER);
    if (ret != 0) {
        dev_upgrade_err("Get lock failed. (devid=%d; ret=0x%x)\n", device_id, ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = dsmi_cmd_unload_patch(device_id, UNLOAD_PATCH);
    if (ret) {
        dev_upgrade_ex_notsupport_err(ret, "Send start update cmd failed. (devid=%d; ret = %d)\n", device_id, ret);
        (void)dsmi_mutex_v(sem_id);
        return ret;
    }
    (void)dsmi_mutex_v(sem_id);
    return 0;
}

STATIC bool dsmi_package_type_is_valid(int pack_type)
{
#ifdef CFG_FEATURE_UPGRADE_PATCH_CONFIG
    if (pack_type == PACKAGE_TYPE_ABL_PATCH) {
        return true;
    }
#endif

#ifdef CFG_FEATURE_UPGRADE_MAMI_PATCH_CONFIG
    if (pack_type == PACKAGE_TYPE_ABL_UBE_MGMT_PATCH) {
        return true;
    }
#endif
    return false;
}
#endif

#ifdef CFG_FEATURE_UPGRADE_MAMI_PATCH_CONFIG
#define PCIE_CARD_MAIN_BOARD_ID_1P 0x68
#define PCIE_CARD_MAIN_BOARD_ID_1P_EQUIP 0x70
#define UBOE_CARD_MAIN_BOARD_ID_1P_EQUIP 0x72

STATIC bool dsmi_check_board_1p(int64_t main_board_id)
{
    if ((main_board_id == PCIE_CARD_MAIN_BOARD_ID_1P) || (main_board_id == PCIE_CARD_MAIN_BOARD_ID_1P_EQUIP) ||
        (main_board_id == UBOE_CARD_MAIN_BOARD_ID_1P_EQUIP)) {
        return true;
    } else {
        return false;
    }
}

STATIC bool dsmi_is_need_upgrade_mami_package(unsigned int device_id)
{
    int ret;
    int64_t value = 0;
    unsigned int size = sizeof(struct dsmi_ub_status);
    struct dsmi_ub_status buff = {0};

    /* is pcie card */
    ret = halGetDeviceInfo(device_id, MODULE_TYPE_SYSTEM, INFO_TYPE_PRODUCT_TYPE, &value);
    if (ret != 0 || value != HAL_PRODUCT_TYPE_PCIE_CARD) {
        DEV_MON_WARNING(
            "Get product type failed, or is not pcie card, no need to upgrade ube mgmt. (ret=%d; dev_id=%u)\n", ret,
            device_id);
        return false;
    }

    /* is not pure pcie card */
    ret = halGetDeviceInfo(device_id, MODULE_TYPE_SYSTEM, INFO_TYPE_MAINBOARD_ID, &value);
    if (ret != 0) {
        DEV_MON_WARNING("Get board id failed, no need to upgrade ube mgmt. (ret=%d; dev_id=%u)\n", ret, device_id);
        return false;
    }

    if (dsmi_check_board_1p(value)) {
        DEV_MON_WARNING("1P card does not need to upgrade ube mgmt. (dev_id=%u; main_board_id=0x%llx)\n", device_id,
                        value);
        return false;
    }

    /* is need link */
    ret = dsmi_get_device_info(device_id, DSMI_MAIN_CMD_UB, DSMI_UB_INFO_SUB_CMD_PORT_STATUS, (void *)&buff, &size);
    if (ret != 0 || buff.ub_link_status == DSMI_UB_NO_NEED_LINK) {
        DEV_MON_WARNING(
            "Get port status failed, or is no need link, no need to upgrade ube mgmt. (ret=%d; dev_id=%u)\n", ret,
            device_id);
        return false;
    }

    DEV_MON_INFO("Need to upgrade ube mgmt. (device_id=%u)\n", device_id);
    return true;
}
#endif

#ifdef CFG_FEATURE_UPGRADE_MAMI_PATCH_CONFIG
typedef struct {
    int device_id;
    const char *file_name;
    int ret;
} dsmi_load_patch_arg_t;

static void *dsmi_load_patch_thread_fn(void *arg)
{
    dsmi_load_patch_arg_t *p = (dsmi_load_patch_arg_t *)arg;
    p->ret = dsmi_load_one_device_patch(p->device_id, PACKAGE_TYPE_ABL_UBE_MGMT_PATCH, p->file_name);
    return NULL;
}

static int dsmi_load_mami_patch(int *device_list, int device_count, const char *file_name)
{
    dsmi_load_patch_arg_t *args = NULL;
    pthread_t *tids = NULL;
    int ret = 0;
    int i;

    args = (dsmi_load_patch_arg_t *)calloc((unsigned long)device_count, sizeof(dsmi_load_patch_arg_t));
    if (args == NULL) {
        DEV_MON_ERR("Failed to alloc memory for thread args. \n");
        return DRV_ERROR_MALLOC_FAIL;
    }
    tids = (pthread_t *)calloc((unsigned long)device_count, sizeof(pthread_t));
    if (tids == NULL) {
        DEV_MON_ERR("Failed to alloc memory for parallel load patch.\n");
        free(args);
        return DRV_ERROR_MALLOC_FAIL;
    }

    for (i = 0; i < device_count; i++) {
        args[i].ret = 0;
        if (!dsmi_is_need_upgrade_mami_package((unsigned int)i)) {
            continue;
        }
        args[i].device_id = device_list[i];
        args[i].file_name = file_name;
        ret = pthread_create(&tids[i], NULL, dsmi_load_patch_thread_fn, &args[i]);
        if (ret != 0) {
            DEV_MON_EX_NOTSUPPORT_ERR(ret, "Create thread for load patch failed. (device_id=%d; ret=%d)",
                                      device_list[i], ret);
            args[i].ret = ret;
        }
    }

    for (i = 0; i < device_count; i++) {
        if (tids[i] != 0) {
            (void)pthread_join(tids[i], NULL);
        }
        if (args[i].ret != 0) {
            ret = args[i].ret;
            DEV_MON_EX_NOTSUPPORT_ERR(ret, "Load patch failed. (device_id=%d; ret=%d)", args[i].device_id, ret);
        }
    }

    free(args);
    free(tids);
    args = NULL;
    tids = NULL;
    return ret;
}
#endif

int dsmi_load_package(int device_id, int pack_type, const char *file_name)
{
#if defined(CFG_FEATURE_UPGRADE_PATCH_CONFIG) || defined(CFG_FEATURE_UPGRADE_MAMI_PATCH_CONFIG)
    int ret, i, device_count;
    int *device_list = NULL;
    if (((unsigned int)device_id != DSMI_SET_ALL_DEVICE) || (file_name == NULL)) {
        DEV_MON_ERR("Invalid parameter. (device_id=%d;pack_type=%d)\n", device_id, pack_type);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (dsmi_package_type_is_valid(pack_type) == false) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (strnlen(file_name, PATH_MAX) >= PATH_MAX) {
        dev_upgrade_err("Invalid file_name. (devid=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    ret = dsmi_get_device_count(&device_count);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0 || (device_count == 0)) {
        DEV_MON_ERR("Failed to get device count. (ret=%d; device_count=%d)\n", ret, device_count);
        return DRV_ERROR_INVALID_DEVICE;
    }
    device_list = (int *)malloc((unsigned long)device_count * sizeof(int));
    if (device_list == NULL) {
        DEV_MON_ERR("Failed to invoke malloc function.\n");
        return DRV_ERROR_MALLOC_FAIL;
    }
    ret = memset_s(device_list, (unsigned long)device_count * sizeof(int), INVALID_DEVICE_ID,
                   (unsigned long)device_count * sizeof(int));
    if (ret != 0) {
        DEV_MON_ERR("memset_s failed. (ret=%d)\n", ret);
        goto device_id_resource_free;
    }
    ret = dsmi_list_device(device_list, device_count);
    if (ret != 0) {
        DEV_MON_ERR("Failed to get device list. (ret=%d)\n", ret);
        goto device_id_resource_free;
    }

    if (pack_type == PACKAGE_TYPE_ABL_UBE_MGMT_PATCH) {
#ifdef CFG_FEATURE_UPGRADE_MAMI_PATCH_CONFIG
        ret = dsmi_load_mami_patch(device_list, device_count, file_name);
#else
        ret = DRV_ERROR_NOT_SUPPORT;
#endif
        goto device_id_resource_free;
    }

    for (i = 0; i < device_count; i++) {
        ret = dsmi_load_one_device_patch(device_list[i], pack_type, file_name);
        if (ret != 0) {
            DEV_MON_EX_NOTSUPPORT_ERR(ret, "Load patch failed. (device_id=%d; ret=%d)", device_list[i], ret);
            goto device_id_resource_free;
        }
    }
device_id_resource_free:
    free(device_list);
    device_list = NULL;
    return ret;
#else
    (void)device_id;
    (void)pack_type;
    (void)file_name;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_unload_package(int device_id, int pack_type)
{
#if defined(CFG_FEATURE_UPGRADE_PATCH_CONFIG)
    int ret;
    int device_count = 0;
    int *device_list = NULL;
    int i = 0;

    if (((unsigned int)device_id != DSMI_SET_ALL_DEVICE) || pack_type != PACKAGE_TYPE_ABL_PATCH) {
        DEV_MON_ERR("Invalid parameter. (device_id=%d;pack_type=%d)\n", device_id, pack_type);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = dsmi_get_device_count(&device_count);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0 || (device_count == 0)) {
        DEV_MON_ERR("Failed to get device count. (ret=%d; device_count=%d)\n", ret, device_count);
        return DRV_ERROR_INVALID_DEVICE;
    }

    device_list = (int *)malloc((unsigned long)device_count * sizeof(int));
    if (device_list == NULL) {
        DEV_MON_ERR("Failed to invoke malloc function.\n");
        return DRV_ERROR_MALLOC_FAIL;
    }

    ret = memset_s(device_list, (unsigned long)device_count * sizeof(int), INVALID_DEVICE_ID,
                   (unsigned long)device_count * sizeof(int));
    if (ret != 0) {
        DEV_MON_ERR("memset_s failed. (ret=%d)\n", ret);
        goto device_id_resource_free;
    }

    ret = dsmi_list_device(device_list, device_count);
    if (ret != 0) {
        DEV_MON_ERR("Failed to get device list. (ret=%d)\n", ret);
        goto device_id_resource_free;
    }

    for (i = 0; i < device_count; i++) {
        ret = dsmi_unload_one_device_patch(device_list[i]);
        if (ret != 0) {
            DEV_MON_EX_NOTSUPPORT_ERR(ret, "Failed to invoke dsmi_unload_one_device_patch. (device_id=%d; ret=%d)",
                                      device_list[i], ret);
            goto device_id_resource_free;
        }
    }

device_id_resource_free:
    free(device_list);
    device_list = NULL;
    return ret;
#else
    (void)device_id;
    (void)pack_type;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_upgrade_start(int device_id, DSMI_COMPONENT_TYPE component_type, const char *file_name)
{
    int ret;
    int sem_id = 0;
    char *file_name_local = NULL;

    if (file_name == NULL) {
        dev_upgrade_err("devid %d dsmi_upgrade_start parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (strnlen(file_name, PATH_MAX) >= PATH_MAX) {
        dev_upgrade_err("devid %d invalid file_name.\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    file_name_local = calloc(PATH_MAX, sizeof(char));
    if (file_name_local == NULL) {
        dev_upgrade_err("calloc file name failed for upgrade, errno=%d.\n", errno);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = strcpy_s(file_name_local, PATH_MAX, file_name);
    if (ret != 0) {
        dev_upgrade_err("strcpy filename to local failed for upgrade.\n");
        DSMI_FREE(file_name_local);
        return DRV_ERROR_INNER_ERR;
    }

    dev_upgrade_event("It is to upgrade firmware. (device_id=%d; uid=%u; component_type=%d)\n", device_id, getuid(),
                      component_type);

    ret = dsmi_check_device_id(device_id);
    if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED) {
        dev_upgrade_err("device is busy, (device_id=0x%x; ret=%d)\n", device_id, ret);
        DSMI_FREE(file_name_local);
        return ret;
    }
    if (ret != 0) {
        dev_upgrade_err("device_id didn't exist. (device_id=0x%x; ret=%d)\n", device_id, ret);
        DSMI_FREE(file_name_local);
        return DRV_ERROR_PARA_ERROR;
    }

    // get lock
    ret = dsmi_mutex_p((key_t)(device_id + DSMI_UPGRADE_LOCK_TAG), &sem_id, DSMI_MUTEX_NO_WAIT);
    if (ret != 0) {
        dev_upgrade_err("devid %d get lock fail 0x%x\n", device_id, ret);
        DSMI_FREE(file_name_local);
        return DRV_ERROR_INNER_ERR;
    }

    ret = dsmi_upgrade_cmd_send(device_id, component_type, file_name_local);
    if (ret != 0) {
        dev_upgrade_ex_notsupport_err(ret, "device_id(%u) dsmi_upgrade_cmd_send fail ret = %d!\n", device_id, ret);
        (void)dsmi_mutex_v(sem_id);
        DSMI_FREE(file_name_local);
        return ret;
    }

    (void)dsmi_mutex_v(sem_id);
    DSMI_FREE(file_name_local);

    return ret;
}

int dsmi_get_last_bootstate(int device_id, BOOT_TYPE boot_type, unsigned int *state)
{
    return _dsmi_get_last_bootstate(device_id, boot_type, state);
}

int dsmi_get_centre_notify_info(int device_id, int index, int *value)
{
    return _dsmi_get_centre_notify_info(device_id, index, value);
}

int dsmi_set_centre_notify_info(int device_id, int index, int value)
{
    return _dsmi_set_centre_notify_info(device_id, index, value);
}

int dsmi_ctrl_device_node(int device_id, struct dsmi_dtm_node_s dtm_node, DSMI_DTM_OPCODE opcode, IN_OUT_BUF buf)
{
    return _dsmi_ctrl_device_node(device_id, dtm_node, opcode, buf);
}

int dsmi_get_all_device_node(int device_id, DEV_DTM_CAP capability, struct dsmi_dtm_node_s node_info[],
                             unsigned int *size)
{
    return _dsmi_get_all_device_node(device_id, capability, node_info, size);
}

int dsmi_get_reboot_reason(int device_id, struct dsmi_reboot_reason *reboot_reason)
{
#ifdef CFG_FEATURE_REBOOT_REASON
    if (reboot_reason == NULL) {
        DEV_MON_ERR("device_id %d parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    return dsmi_cmd_get_reboot_reason(device_id, reboot_reason);
#else
    (void)device_id;
    (void)reboot_reason;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_set_bist_info(int device_id, DSMI_BIST_CMD cmd, const void *buf, unsigned int buf_size)
{
    return _dsmi_set_bist_info(device_id, cmd, buf, buf_size);
}

int dsmi_get_bist_info(int device_id, DSMI_BIST_CMD cmd, void *buf, unsigned int *size)
{
    return _dsmi_get_bist_info(device_id, cmd, buf, size);
}

int dsmi_set_upgrade_attr(int device_id, DSMI_COMPONENT_TYPE component_type, DSMI_UPGRADE_ATTR attr)
{
    return _dsmi_set_upgrade_attr(device_id, component_type, attr);
}

#define BIT_NUM_OF_BYTE 8UL
int dsmi_get_component_count(int device_id, unsigned int *component_count)
{
    int ret;
    unsigned int i = 0;
    unsigned long long bitmap = 0;
    unsigned int count = 0;

    if (component_count == NULL) {
        dev_upgrade_err("devid %d dsmi_get_component_count parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_check_device_id(device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0) {
        dev_upgrade_err("device_id didn't exist. (device_id=0x%x; ret=%d)\n", device_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_component_list(device_id, &bitmap);
    if (ret != OK) {
        dev_upgrade_ex_notsupport_err(ret, "dsmi_get_component_count fail, device id 0x%x ret = %d\n", device_id, ret);
        return ret;
    }

    for (i = 0; i < (unsigned int)(sizeof(bitmap) * BIT_NUM_OF_BYTE); i++) {
        if (BIT_IF_ONE(bitmap, i) == 0x1) {
            count++;
        }
    }

    *component_count = count;

    return ret;
}

int dsmi_get_component_list(int device_id, DSMI_COMPONENT_TYPE *component_table, unsigned int component_count)
{
    unsigned int i = 0;
    unsigned int j = 0;
    unsigned long long bitmap = 0;
    int ret;

    if (component_table == NULL) {
        dev_upgrade_err("devid %d dsmi_get_component_list parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_check_device_id(device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0) {
        dev_upgrade_err("device_id didn't exist. (device_id=0x%x; ret=%d)\n", device_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_component_list(device_id, &bitmap);
    if (ret != OK) {
        dev_upgrade_ex_notsupport_err(ret, "dsmi_cmd_get_component_list fail, device id %d ret = %d\n", device_id, ret);
        return ret;
    }

    for (i = 0; i < (unsigned int)(sizeof(bitmap) * BIT_NUM_OF_BYTE); i++) {
        if (BIT_IF_ONE(bitmap, i) == 0x1) {
            if (j >= component_count) {
                dev_upgrade_warn("input param component_count exceeds support count. (devid=%d; component_count=%d; "
                                 "support_count=%d)\n",
                                 device_id, component_count, j);
                return DRV_ERROR_NONE;
            }
            component_table[j] = i;
            dev_upgrade_debug("devid %d component type = 0x%x\n", device_id, component_table[j]);
            j++;
        }
    }

    return ret;
}

int dsmi_upgrade_get_state(int device_id, unsigned char *schedule, unsigned char *upgrade_status)
{
    int ret;

    if (schedule == NULL || upgrade_status == NULL) {
        dev_upgrade_err("devid %d dsmi_upgrade_get_state parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_check_device_id(device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0) {
        dev_upgrade_err("device_id didn't exist. (device_id=0x%x; ret=%d)\n", device_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_upgrade_get_state(device_id, 0, schedule, upgrade_status);
    dev_upgrade_event("upgrade get state, (device_id=0x%x; status=0x%x; schedule=%u; ret=%d)\n", device_id,
                      *upgrade_status, *schedule, ret);

    return ret;
}

int dsmi_upgrade_get_component_static_version(int device_id, DSMI_COMPONENT_TYPE component_type,
                                              unsigned char *version_str, unsigned int version_len,
                                              unsigned int *ret_len)
{
    int ret;

    if (version_str == NULL || ret_len == NULL) {
        dev_upgrade_err("devid %d dsmi_upgrade_get_component_static_version error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (version_len < FW_VERSION_MAX_LENGTH) {
        dev_upgrade_err("devid %d version_len(%u) error, expect >= FW_VERSION_MAX_LENGTH(%d)!\n", device_id,
                        version_len, FW_VERSION_MAX_LENGTH);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = check_component_type(component_type, device_id);
    if (ret != 0) {
        dev_upgrade_ex_notsupport_err(ret, "The component type is error. (dev_id=%d; type=0x%x; ret=%d)\n", device_id,
                                      component_type, ret);
        return ret;
    }

    ret = dsmi_cmd_upgrade_get_version(device_id, (unsigned char)component_type, version_str, ret_len);

    return ret;
}

STATIC int dsmi_user_config_item_check(int device_id, const char *config_name)
{
    int i = 0;
    int len = sizeof(g_user_config_no_support_list) / sizeof(g_user_config_no_support_list[0]);

    for (i = 0; i < len; i++) {
        if (strcmp(config_name, g_user_config_no_support_list[i]) == 0) {
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    if (strcmp(config_name, UBMEM_RETRY_FLAG_NAME) != 0) {
        return DRV_ERROR_NONE;
    }
#ifdef CFG_FEATURE_SUPPORT_UBMEM_RETRY_FLAG
#ifdef DRV_HOST
    if (!dmc_has_feature_id((unsigned int)device_id, DMC_CFG_FEATURE_SUPPORT_UBMEM_RETRY_FLAG)) {
        return DRV_ERROR_NOT_SUPPORT;
    } else {
        return DRV_ERROR_NONE;
    }
#else
    (void)device_id;
    return DRV_ERROR_NONE;
#endif
#else
    (void)device_id;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_get_user_config(int device_id, const char *config_name, unsigned int buf_size, unsigned char *buf)
{
    int ret;
    unsigned int buf_size_tmp = buf_size;
    size_t config_name_len;

    if ((config_name == NULL) || (buf == NULL) || (buf_size > UC_ITEM_DATA_MAX_LEN)) {
        DEV_MON_ERR("devid %d input para of get_user_config is error\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    config_name_len = strlen(config_name);
    if (config_name_len > (size_t)(DSMI_USER_CONFIG_NAME_MAX - 1)) {
        DEV_MON_ERR("Invalid config_name. (device_id=%u; config_name_len=%zu; config_name_len_max=%u)\n", device_id,
                    config_name_len, DSMI_USER_CONFIG_NAME_MAX - 1);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_user_config_item_check(device_id, config_name);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "User config not support. (device_id=%d; ret=%d)\n", device_id, ret);
        return ret;
    }

    if (is_product_user_config_item_by_name(config_name, (unsigned char)config_name_len) == true) {
        return dsmi_product_get_user_config(device_id, config_name, buf_size, buf);
    }

    ret = dsmi_cmd_get_user_config(device_id, (unsigned char)config_name_len, config_name, &buf_size_tmp, buf);
    if (buf_size < buf_size_tmp) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_get_user_config size failed, set size=%d, return size=%d\n",
                                  device_id, buf_size, buf_size_tmp);
        return DRV_ERROR_PARA_ERROR;
    }

    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_get_user_config failed ret = %d\n", device_id, ret);
        return ret;
    }

    return ret;
}

int dsmi_set_user_config(int device_id, const char *config_name, unsigned int buf_size, unsigned char *buf)
{
    int ret;
    int i = 0;
    size_t config_name_len;

    if ((config_name == NULL) || (buf == NULL) || (buf_size > BUF_MAX_LEN)) {
        DEV_MON_ERR("devid %d input para of set_user_config is error\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_user_config_item_check(device_id, config_name);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "User config not support. (device_id=%d; ret=%d)\n", device_id, ret);
        return ret;
    }

    DEV_MON_EVENT("It is to set user configuration. (device_id=%d; uid=%u)\n", device_id, getuid());

    for (i = 0; i < UC_ITEM_MAX_NUM; i++) {
        if ((strcmp(config_name, user_cfg_version_1[i].name) == 0) && (user_cfg_version_1[i].check_para != NULL)) {
            if (user_cfg_version_1[i].check_para(buf_size, buf) != 0) {
                DEV_MON_ERR("input para of set_user_config buf size(%d) error, config_name=%s\n", buf_size,
                            config_name);
                return DRV_ERROR_PARA_ERROR;
            }
        }
    }
    config_name_len = strlen(config_name);
    if (config_name_len > (size_t)(DSMI_USER_CONFIG_NAME_MAX - 1)) {
        DEV_MON_ERR("Invalid config_name. (device_id=%u; config_name_len=%zu; config_name_len_max=%u)\n", device_id,
                    config_name_len, DSMI_USER_CONFIG_NAME_MAX - 1);
        return DRV_ERROR_PARA_ERROR;
    }

    if (is_product_user_config_item_by_name(config_name, (unsigned char)config_name_len) == true) {
        return dsmi_product_set_user_config(device_id, config_name, buf_size, buf);
    }

    ret = dsmi_cmd_set_user_config(device_id, (unsigned char)config_name_len, config_name, buf_size, buf);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d set user config failed ret = %d\n", device_id, ret);
        return ret;
    }

    return ret;
}

int dsmi_clear_user_config(int device_id, const char *config_name)
{
    int ret;
    unsigned char config_name_len;

    if (config_name == NULL) {
        DEV_MON_ERR("config_name is NULL. (device_id=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_user_config_item_check(device_id, config_name);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "User config not support. (device_id=%d; ret=%d)\n", device_id, ret);
        return ret;
    }

    DEV_MON_EVENT("It is to clear user configuration. (device_id=%d; uid=%d)\n", device_id, getuid());

    config_name_len = (unsigned char)(strlen(config_name) + 1U);
    if (is_product_user_config_item_by_name(config_name, config_name_len) == true) {
        return dsmi_product_clear_user_config(device_id, config_name);
    }

    ret = dsmi_cmd_clear_user_config(device_id, config_name_len, config_name);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d clear user config failed\n", device_id);
        return ret;
    }

    return ret;
}

/* ****************************************************************************
 Prototype    : dsmi_get_network_health
 Description  :get network health
 Input        : int device_id
 Output       : unsigned int *presult
 Return Value :0 success
                other failed
1.Date         : 2019/5/7
Modification : Created function

**************************************************************************** */
int dsmi_get_network_health(int device_id, DSMI_NET_HEALTH_STATUS *presult)
{
    if (presult == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_network_health parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    return dsmi_cmd_get_network_health(device_id, presult);
}

int dsmi_get_llc_perf_para(int device_id, DSMI_LLC_PERF_INFO *perf_para)
{
    int ret;
    DSMI_LLC_RX_RESULT rx_result = {0};

    if (perf_para == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_llc_perf_para parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_llc_perf_para(device_id, &rx_result);
    if (ret < 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_get_llc_perf_para failed ret = %d!\n", device_id, ret);
        return ret;
    }

    perf_para->rd_hit_rate = rx_result.rd_hit_rate;
    perf_para->wr_hit_rate = rx_result.wr_hit_rate;
    perf_para->throughput = rx_result.throughput;

    return ret;
}

int dsmi_get_device_die(int device_id, struct dsmi_soc_die_stru *pdevice_die)
{
    int dev_index;
    int soc_type;

    if (pdevice_die == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_device_die parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    dev_index = (int)(((unsigned int)device_id) & (0xFFFFU));
    soc_type = (int)((unsigned int)device_id >> 16); // 16 high bit is soc_type
    if (soc_type > UCHAR_MAX) {
        DEV_MON_ERR("input soc_type(%d) exceeds UCHAR_MAX(%d).\n", soc_type, UCHAR_MAX);
        return DRV_ERROR_PARA_ERROR;
    }
    return dsmi_cmd_get_device_die(dev_index, soc_type, pdevice_die);
}

int dsmi_set_sec_revocation(int device_id, DSMI_REVOCATION_TYPE revo_type, const unsigned char *file_data,
                            unsigned int file_size)
{
    int ret;

    if (revo_type >= DSMI_REVOCATION_TYPE_MAX) {
        DEV_MON_ERR("device_id %d revocation type[%d] error, expect < DSMI_REVOCATION_TYPE_MAX(%d)!\n", device_id,
                    revo_type, DSMI_REVOCATION_TYPE_MAX);
        return DRV_ERROR_PARA_ERROR;
    }

    if (file_data == NULL) {
        DEV_MON_ERR("device_id %d file data is NULL.\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((file_size == 0) || (file_size > REVOCATION_FILE_LEN_MAX)) {
        DEV_MON_ERR("device_id %d file size[%u] is out of range, expect in (0, REVOCATION_FILE_LEN_MAX(%u)].\n",
                    device_id, file_size, REVOCATION_FILE_LEN_MAX);
        return DRV_ERROR_PARA_ERROR;
    }

    DEV_MON_EVENT("set sec revocation, (user id=%u; device_id=%d; type=%d\n", getuid(), device_id, revo_type);

    ret = dsmi_cmd_set_sec_revocation(device_id, revo_type, file_data, file_size);
    if (ret != OK) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_cmd_set_sec_revocation failed %d\n", device_id, ret);
        return ret;
    }

    return ret;
}

#ifdef CFG_FEATURE_POWER_COMMAND
#define REBOOT_WAIT_TIME 5
#endif
STATIC int dsmi_set_poweroff_reset(int devid, struct dsmi_power_state_info_stru *power_info)
{
#ifdef CFG_FEATURE_POWER_COMMAND
    int ret;
    unsigned int type = power_info->type;
    char *argv_poweroff[] = {DSMI_DMP_POWER_SET_SCRIPT, DSMI_DMP_POWER_CMD_POWEROFF, NULL};
    char *argv_reset[] = {DSMI_DMP_POWER_SET_SCRIPT, DSMI_DMP_POWER_CMD_RESET, NULL};

    if (access(DSMI_DMP_POWER_SET_SCRIPT, X_OK) != 0) {
        DEV_MON_CRIT_ERR("dsmi set power no exec permission, (devid=%d; type=%u; error=%s).\n", devid, type,
                         strerror(errno));
        return DRV_ERROR_OPER_NOT_PERMITTED;
    }

    ret = dsmi_cmd_set_power_state(devid, power_info);
    if (ret == (int)DRV_ERROR_NOT_SUPPORT) {
        DEV_MON_CRIT_EVENT("dsmi set power state not support, (devid=%d; type=%u; mode=%u).\n", devid, type,
                           (u32)power_info->mode);
        return DRV_ERROR_NOT_SUPPORT;
    } else if (ret != 0) {
        DEV_MON_CRIT_EVENT("set power state not ok, (devid=%d; type=%u; mode=%u, ret=%d).\n", devid, type,
                           (u32)power_info->mode, ret);
    } else {
    }

    if (type == (unsigned int)POWER_STATE_POWEROFF) {
        ret = dsmi_raise_script(DSMI_DMP_POWER_SET_SCRIPT, argv_poweroff);
    } else {
        ret = dsmi_raise_script(DSMI_DMP_POWER_SET_SCRIPT, argv_reset);
    }
    if (ret != 0) {
        DEV_MON_CRIT_EVENT("start call shutdown or reboot, (devid=%d; type=%u; ret=%d).\n", devid, type, ret);
        sync();
        DlogFlush();
        (void)sleep(REBOOT_WAIT_TIME);
        ret = dsmi_invoke_os_cmd(type);
        if (ret != DRV_ERROR_NONE) {
            DEV_MON_CRIT_ERR("dsmi_invoke_os_cmd failed, (type=%u; ret=%d).\n", type, ret);
            return DRV_ERROR_INNER_ERR;
        }
    }
    DEV_MON_CRIT_EVENT("poweroff or reset success, (devid=%d; type=%u).\n", devid, type);

    return ret;
#else
    (void)devid;
    (void)power_info;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_set_power_state_base(int device_id, struct dsmi_power_state_info_stru *power_info)
{
    int ret = dsmi_check_device_id(device_id);
    if (ret != 0) {
        DEV_MON_ERR("Device is invalid. (device_id=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (power_info->type >= POWER_STATE_MAX) {
        DEV_MON_ERR("Type is invalid. (device_id=%d; type=%u)\n", device_id, power_info->type);
        return DRV_ERROR_PARA_ERROR;
    }
#ifndef CFG_FEATURE_POWER_COMMAND
    if ((power_info->type != POWER_STATE_SUSPEND)) {
        DEV_MON_ERR("Type is invalid. (device_id=%d; type=%u)\n", device_id, power_info->type);
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif
    if ((power_info->type == POWER_STATE_POWEROFF) || (power_info->type == POWER_STATE_RESET)) {
        return dsmi_set_poweroff_reset(device_id, power_info);
    } else {
        return dsmi_cmd_set_power_state(device_id, power_info);
    }
}

int dsmi_set_power_state(int device_id, DSMI_POWER_STATE type)
{
    return _dsmi_set_power_state(device_id, type);
}

int dsmi_set_power_state_v2(int device_id, struct dsmi_power_state_info_stru power_info)
{
    DEV_MON_CRIT_EVENT("dsmi set power state exec v2, (user=%u; devid=%d; type=%u).\n", getuid(), device_id,
                       power_info.type);
    return dsmi_set_power_state_base(device_id, &power_info);
}

int dsmi_get_hiss_status(int device_id, struct dsmi_hiss_status_stru *hiss_status_data)
{
    if (hiss_status_data == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_hiss_status parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    return dsmi_cmd_get_hiss_status(device_id, hiss_status_data);
}

int dsmi_get_lp_status(int device_id, struct dsmi_lp_status_stru *lp_status_data)
{
    if (lp_status_data == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_lp_status parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    return dsmi_cmd_get_lp_status(device_id, lp_status_data);
}

int dsmi_get_can_status(int device_id, const char *name, unsigned int name_len,
                        struct dsmi_can_status_stru *can_status_data)
{
    if (can_status_data == NULL || name == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_canstatus parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    return dsmi_cmd_get_can_status(device_id, name, name_len, can_status_data);
}
int dsmi_get_ufs_status(int device_id, struct dsmi_ufs_status_stru *ufs_status_data)
{
    return _dsmi_get_ufs_status(device_id, ufs_status_data);
}

int dsmi_get_sensorhub_status(int device_id, struct dsmi_sensorhub_status_stru *sensorhub_status_data)
{
    if (sensorhub_status_data == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_sensorhubstatus parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    return dsmi_cmd_get_sensorhub_status(device_id, sensorhub_status_data);
}

int dsmi_get_sensorhub_config(int device_id, struct dsmi_sensorhub_config_stru *sensorhub_config_data)
{
    if (sensorhub_config_data == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_sensorhubconfig parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    return dsmi_cmd_get_sensorhub_config(device_id, sensorhub_config_data);
}

int dsmi_get_gpio_status(int device_id, unsigned int gpio_num, unsigned int *status)
{
    if (status == NULL) {
        DEV_MON_ERR("para is null \n");
        return DRV_ERROR_PARA_ERROR;
    }
    return dsmi_cmd_get_gpio_status(device_id, gpio_num, status);
}

int dsmi_get_sochwfault(int device_id, struct dsmi_emu_subsys_state_stru *emu_subsys_state_data)
{
    if ((device_id >= ASCEND_DEV_MAX_NUM) || (device_id < DEVDRV_MIN_DAVINCI_NUM)) {
        DEV_MON_ERR("devid %d is invalid.\n", device_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (emu_subsys_state_data == NULL) {
        DEV_MON_ERR("devid %d para is null.\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    return dsmi_cmd_get_soc_hw_fault(device_id, emu_subsys_state_data);
}

int dsmi_get_safetyisland_status(int device_id, struct dsmi_safetyisland_status_stru *safetyisland_status_data)
{
    if ((device_id >= ASCEND_DEV_MAX_NUM) || (device_id < DEVDRV_MIN_DAVINCI_NUM)) {
        DEV_MON_ERR("devid %d is invalid.\n", device_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (safetyisland_status_data == NULL) {
        DEV_MON_ERR("devid %d para is null.\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    return dsmi_cmd_get_safetyisland_status(device_id, safetyisland_status_data);
}

int dsmi_get_device_cgroup_info(int device_id, struct tag_cgroup_info *cg_info)
{
    int ret;
    if (cg_info == NULL) {
        DEV_MON_ERR("dsmi_get_device_cgroup_info parameter error!\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_check_device_id(device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0) {
        DEV_MON_ERR("device_id didn't exist. (device_id=0x%x; ret=%d)\n", device_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_device_cgroup_info(device_id, cg_info);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "dsmi_cmd_get_device_cgroup_info error:%d!\n", ret);
        return ret;
    }

    return ret;
}

#define PKCS_SIGN_ON "pkcs=on"
#define PKCS_SIGN_OFF "pkcs=off"
#define PKCS_ON_LEN 7
#define PKCS_OFF_LEN 8
#define MAX_LINE_SIZE 1024
#define SIGN_KEY_WORDS "pkcs"
#define INVALID_SIGN_VALUE 2
STATIC int dsmi_main_cmd_sec_update_sign(const void *buf, unsigned int buf_size)
{
    size_t w_len, rw_len;
    FILE *fp = NULL;
    unsigned char buf_info = *(const unsigned char *)buf;
    int errno_tmp;

    fp = fopen(DSMI_SEC_SIGN_CFG, "w");
    if (fp == NULL) {
        errno_tmp = errno;
        DEV_MON_ERR("Failed to open pss sign file, (errno=%d).\n", errno_tmp);
        return DRV_ERROR_PARA_ERROR;
    }

    (void)chmod(DSMI_SEC_SIGN_CFG, DSMI_SEC_SIGN_CFG_MODE);
    switch (buf_info) {
        case PKCS_SIGN_TYPE_ON:
            w_len = PKCS_ON_LEN;
            rw_len = fwrite(PKCS_SIGN_ON, 1, w_len, fp);
            break;
        case PKCS_SIGN_TYPE_OFF:
            w_len = PKCS_OFF_LEN;
            rw_len = fwrite(PKCS_SIGN_OFF, 1, w_len, fp);
            break;
        default:
            (void)fclose(fp);
            fp = NULL;
            DEV_MON_ERR("buf_info is invalid. (buf_info=%u)\n", buf_info);
            return DRV_ERROR_INVALID_VALUE;
    }
    (void)fclose(fp);
    fp = NULL;

    if (rw_len != w_len) {
        DEV_MON_ERR("Write file failed. (w_len=%u; rw_len=%u)\n", w_len, rw_len);
        return DRV_ERROR_INNER_ERR;
    }

    (void)buf_size;
    return 0;
}

STATIC int search_key_and_get_value(FILE *fp, const char *split, const char *key, char *value, int len)
{
    char tmp[MAX_LINE_SIZE];
    char *left = NULL;
    char *right = NULL;
    char *p_save = NULL;
    size_t tmp_len;

    if (len <= 0 || len > MAX_LINE_SIZE) {
        DEV_MON_ERR("The variable len is invalid. (len=%d)\n", len);
        return DRV_ERROR_PARA_ERROR;
    }

    while ((memset_s(tmp, MAX_LINE_SIZE, 0, MAX_LINE_SIZE) == 0) && (fgets(tmp, MAX_LINE_SIZE, fp) != NULL)) {
        tmp_len = strlen(tmp);
        if (tmp_len == 0UL) {
            continue;
        }
        /* Check if the words is end of '\n' or not. */
        if (tmp[tmp_len - 1UL] == '\n') {
            tmp[tmp_len - 1UL] = '\0';
        }
        /* Check if it owns the key words or not. */
        if (strstr(tmp, key) == NULL) {
            continue;
        }
        /* Take the first part of the separator. */
        left = strtok_r(tmp, split, &p_save);
        if (left == NULL) {
            continue;
        }
        /* Compare key-words. */
        if (strcmp(key, left) != 0) {
            continue;
        }
        /* To get wanted-string. */
        if ((right = strtok_r(NULL, split, &p_save)) != NULL) {
            if (strcpy_s(value, (size_t)len, right) != EOK) {
                DEV_MON_ERR("Failed to invoke strcpy_s to copy the variable right.\n");
                return DRV_ERROR_INNER_ERR;
            }
            return 0;
        }
    }

    DEV_MON_ERR("The local file pss.cfg is invalid. \n");
    return DRV_ERROR_PARA_ERROR;
}

STATIC int dsmi_get_local_sign(unsigned char *sign)
{
    int ret;
    FILE *fp = NULL;
    char val[MAX_LINE_SIZE] = {0};
    int errno_tmp;

    if (getuid() != 0) {
        DEV_MON_ERR("Permission denied. \n");
        return DRV_ERROR_OPER_NOT_PERMITTED;
    }

    if (access(DSMI_SEC_SIGN_CFG, F_OK) != 0) {
        *sign = PKCS_SIGN_TYPE_OFF;
        return 0;
    }

    fp = fopen(DSMI_SEC_SIGN_CFG, "r");
    if (fp == NULL) {
        errno_tmp = errno;
        DEV_MON_ERR("Failed to open pss sign file, (errno=%d).\n", errno_tmp);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = search_key_and_get_value(fp, "=", SIGN_KEY_WORDS, val, MAX_LINE_SIZE);
    (void)fclose(fp);
    fp = NULL;

    if (ret != 0) {
        DEV_MON_ERR("Failed to invoke search_key_and_get_value.\n");
        return ret;
    }

    if (strcmp(val, "on") == 0) {
        *sign = PKCS_SIGN_TYPE_ON;
    } else if (strcmp(val, "off") == 0) {
        *sign = PKCS_SIGN_TYPE_OFF;
    } else {
        DEV_MON_ERR("The local pss.cfg configuration file is invalid.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

STATIC drvError_t dsmi_set_muti_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd,
                                            const void *buf, unsigned int buf_size)
{
    int ret, i = 0, device_count = 0, sem_id = 0;
    int *device_list = NULL;

    if (device_id != 0) {
        DEV_MON_ERR("Failed to check device_id. (device_id=%u)\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = dsmi_get_device_count(&device_count);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0 || (device_count == 0)) {
        DEV_MON_ERR("Failed to get device count. (ret=%d; device_count=%d)\n", ret, device_count);
        return DRV_ERROR_INVALID_DEVICE;
    }

    device_list = (int *)malloc((size_t)device_count * sizeof(int));
    if (device_list == NULL) {
        DEV_MON_ERR("Failed to invoke malloc function.\n");
        return DRV_ERROR_MALLOC_FAIL;
    }

    ret = memset_s(device_list, (unsigned long)device_count * sizeof(int), INVALID_DEVICE_ID,
                   (unsigned long)device_count * sizeof(int));
    if (ret != 0) {
        DEV_MON_ERR("memset_s failed. (ret=%d)\n", ret);
        goto device_id_resource_free;
    }

    ret = dsmi_list_device(device_list, device_count);
    if (ret != 0) {
        DEV_MON_ERR("Failed to get device list. (ret=%d)\n", ret);
        goto device_id_resource_free;
    }

    ret = dsmi_mutex_p((key_t)(ASCEND_DEV_MAX_NUM + DSMI_UPGRADE_LOCK_TAG), &sem_id, DSMI_MUTEX_WAIT_FOR_EVER);
    if (ret != 0) {
        dev_upgrade_err("Failed to get lock for updating pss conf.\n");
        goto device_id_resource_free;
    }

    for (i = 0; i < device_count; i++) {
        ret = dsmi_cmd_set_device_info((unsigned int)device_list[i], main_cmd, sub_cmd, buf, buf_size);
        if (ret != 0) {
            DEV_MON_EX_NOTSUPPORT_ERR(ret, "Failed to invoke dsmi_cmd_set_device_info. (device_id=%d; ret=%d)\n",
                                      device_list[i], ret);
            goto sem_free;
        }
    }

    ret = dsmi_main_cmd_sec_update_sign(buf, buf_size);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Failed to invoke dsmi_main_cmd_sec_update_sign. \n");
    }

sem_free:
    (void)dsmi_mutex_v(sem_id);
device_id_resource_free:
    free(device_list);
    device_list = NULL;
    return (drvError_t)ret;
}

int dsmi_get_muti_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                              unsigned int *size)
{
    int ret;
    int device_count = 0;
    int *device_list = NULL;
    int i = 0;
    unsigned char sign;

    if (device_id != 0) {
        DEV_MON_ERR("Failed to check device_id. (device_id=%u)\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = dsmi_get_local_sign(&sign);
    if (ret != 0) {
        DEV_MON_ERR("Failed to invoke dsmi_get_local_sign. (ret=%d)\n", ret);
        return ret;
    }

    ret = dsmi_get_device_count(&device_count);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0 || (device_count == 0)) {
        DEV_MON_ERR("Failed to get device count. (ret=%d; device_count=%d)\n", ret, device_count);
        return DRV_ERROR_INVALID_DEVICE;
    }

    device_list = (int *)malloc(((size_t)device_count) * sizeof(int));
    if (device_list == NULL) {
        DEV_MON_ERR("Failed to invoke malloc function.\n");
        return DRV_ERROR_MALLOC_FAIL;
    }

    ret = memset_s(device_list, (size_t)device_count * sizeof(int), INVALID_DEVICE_ID,
                   (unsigned long)device_count * sizeof(int));
    if (ret != 0) {
        DEV_MON_ERR("memset_s failed. (ret=%d)\n", ret);
        goto device_id_resource_free;
    }

    ret = dsmi_list_device(device_list, device_count);
    if (ret != 0) {
        DEV_MON_ERR("Failed to get device list. (ret=%d)\n", ret);
        goto device_id_resource_free;
    }

    for (i = 0; i < device_count; i++) {
        ret = dsmi_cmd_get_device_info((unsigned int)device_list[i], main_cmd, sub_cmd, buf, size);
        if (ret != 0) {
            DEV_MON_EX_NOTSUPPORT_ERR(ret, "Failed to invoke dsmi_cmd_get_device_info. (device_id=%d; ret=%d)",
                                      device_list[i], ret);
            goto device_id_resource_free;
        }

        if (*(unsigned char *)buf != sign) {
            ret = 0;
            *(unsigned char *)buf = INVALID_SIGN_VALUE;
            goto device_id_resource_free;
        }
    }

device_id_resource_free:
    free(device_list);
    device_list = NULL;
    return ret;
}

/* The sub_cmd range is set as follows：[ sub_cmd_start, sub_cmd_end ]  */
drv_set_dev_info_cmd_t g_set_dev_info_cmd[] = {
    {DSMI_MAIN_CMD_SVM, 0, DSMI_DEV_INFO_SUB_CMD_MAX, drvSetDeviceInfo, NULL},
    {DSMI_MAIN_CMD_VDEV_MNG, 0, DSMI_DEV_INFO_SUB_CMD_MAX, drvSetDeviceInfo, NULL},
    {DSMI_MAIN_CMD_HOST_AICPU, 0, DSMI_DEV_INFO_SUB_CMD_MAX, drvSetDeviceInfo, NULL},
    {DSMI_MAIN_CMD_SEC, DSMI_SEC_SUB_CMD_PSS, DSMI_SEC_SUB_CMD_PSS, dsmi_set_muti_device_info, NULL},
    {DSMI_MAIN_CMD_SEC, DSMI_SEC_SUB_CMD_CC, DSMI_SEC_SUB_CMD_CC, dsmi_cmd_set_device_info_ex, NULL},
    {DSMI_MAIN_CMD_SEC, DSMI_SEC_SUB_CMD_CUST_SIGN_FLAG, DSMI_SEC_SUB_CMD_CUST_SIGN_FLAG, dsmi_cmd_set_custom_sign_flag,
     NULL},
    {DSMI_MAIN_CMD_SEC, DSMI_SEC_SUB_CMD_CUST_SIGN_USER_CERT, DSMI_SEC_SUB_CMD_CUST_SIGN_USER_CERT,
     dsmi_cmd_set_custom_sign_cert, NULL},
    {DSMI_MAIN_CMD_UPGRADE, DSMI_UPGRADE_SUB_TYPE_SWPLUGIN_POLICY, DSMI_UPGRADE_SUB_TYPE_SWPLUGIN_POLICY,
     drvSetDeviceInfo, NULL},
    {DSMI_MAIN_CMD_UPGRADE, 0, DSMI_DEV_INFO_SUB_CMD_MAX, dsmi_cmd_set_device_info_critical, NULL},
    {DSMI_MAIN_CMD_RECOVERY, 0, DSMI_DEV_INFO_SUB_CMD_MAX, dsmi_cmd_set_device_info_critical, NULL},
    {DSMI_MAIN_CMD_MEMORY, DSMI_SUB_CMD_MEMORY_CLEAR_HUGE_PAGE, DSMI_SUB_CMD_MEMORY_CLEAR_HUGE_PAGE,
     dsmi_cmd_set_device_info_ex, NULL},
    {DSMI_MAIN_CMD_MEMORY, DSMI_SUB_CMD_MEMORY_SET_HPAGE_RATIO, DSMI_SUB_CMD_MEMORY_SET_HPAGE_RATIO,
     dsmi_cmd_set_device_info_ex, NULL},
    {DSMI_MAIN_CMD_SOC_INFO, DSMI_SOC_INFO_SUB_CMD_CUST_OP_ENHANCE, DSMI_SOC_INFO_SUB_CMD_CUST_OP_ENHANCE,
     drvSetDeviceInfoToDmsHal, NULL},
    {DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_SPOD_NODE_STATUS, DSMI_CHIP_INF_SUB_CMD_SPOD_NODE_STATUS,
     drvSetDeviceInfo, NULL},
    {DSMI_MAIN_CMD_TRS, DSMI_TRS_SUB_CMD_KERNEL_LAUNCH_MODE, DSMI_TRS_SUB_CMD_KERNEL_LAUNCH_MODE, DmsSetTrsMode, NULL},
    {DSMI_MAIN_CMD_TS, DSMI_TS_SUB_CMD_COMMON_MSG, DSMI_TS_SUB_CMD_COMMON_MSG, drvSetDeviceInfo, NULL},
    /* default used dsmi_cmd_set_device_info */
};

int dsmi_cmd_set_device_info_method(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                                    unsigned int buf_size)
{
    int ret;
    unsigned int i, size;

    if ((main_cmd >= DSMI_PRODUCT_MAIN_CMD_START) && (main_cmd <= DSMI_PRODUCT_MAIN_CMD_END)) {
        return dsmi_product_set_device_info(dev_id, main_cmd, sub_cmd, buf, buf_size);
    }

    size = sizeof(g_set_dev_info_cmd) / sizeof(g_set_dev_info_cmd[0]);
    for (i = 0; i < size; i++) {
        if (main_cmd == g_set_dev_info_cmd[i].main_cmd) {
            if ((g_set_dev_info_cmd[i].sub_cmd_start <= sub_cmd) && (sub_cmd <= g_set_dev_info_cmd[i].sub_cmd_end)) {
                break;
            }
        }
    }

    if (i < size) {
        if (g_set_dev_info_cmd[i].cmd_check != NULL) {
            ret = g_set_dev_info_cmd[i].cmd_check();
            if (ret != 0) {
                DEV_MON_EX_NOTSUPPORT_ERR(ret,
                                          "Set device info check failed. "
                                          "(dev_id=%u; main_cmd=%u; sub_cmd=%u; ret=%d).\n",
                                          dev_id, main_cmd, sub_cmd, ret);
                return ret;
            }
        }

        return g_set_dev_info_cmd[i].callback(dev_id, main_cmd, sub_cmd, buf, buf_size);
    } else {
        return dsmi_cmd_set_device_info(dev_id, main_cmd, sub_cmd, buf, buf_size);
    }
}

int dsmi_set_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                         unsigned int buf_size)
{
    return _dsmi_set_device_info(device_id, main_cmd, sub_cmd, buf, buf_size);
}

bool dsmi_get_device_info_is_critical(DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd)
{
    if (((sub_cmd >> HIGH_16_BITS) == (unsigned int)DSMI_UPGRADE_SUB_TYPE_FW_VERIFY) &&
        (main_cmd == DSMI_MAIN_CMD_UPGRADE)) {
        return true;
    }

    return false;
}

int dsmi_cmd_get_flash_erase_count(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                                   unsigned int *size)
{
#define DSMI_READ_TIMES 4
#define DSMI_OUT_LEN_EACH_READ (2 * 1024)
    int ret;
    int sem_id = 0;
    unsigned int i;
    unsigned int temp_size;

    if (sub_cmd != DSMI_FLASH_SUB_CMD_GET_ERASE_COUNT) {
        DEV_MON_ERR("sub_cmd error. (sub_cmd=%d)\n", sub_cmd);
        return DRV_ERROR_PARA_ERROR;
    }
    if (buf == NULL) {
        DEV_MON_ERR("buf is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if (size == NULL) {
        DEV_MON_ERR("size is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if (*size != sizeof(DSMI_FLASH_ERASE_COUNT)) {
        DEV_MON_ERR("size is invalid. (size=%u, correct_size=%u)\n", *size, sizeof(DSMI_FLASH_ERASE_COUNT));
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_mutex_p((key_t)(device_id + DSMI_FLASH_LOCK_TAG), &sem_id, DSMI_MUTEX_WAIT_FOR_EVER);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "get lock fail. (devid=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_INNER_ERR;
    }

    for (i = 0; i < DSMI_READ_TIMES; i++) {
        temp_size = DSMI_OUT_LEN_EACH_READ;
        ret = dsmi_cmd_get_device_info(device_id, main_cmd, i, ((unsigned char *)buf) + (i * DSMI_OUT_LEN_EACH_READ),
                                       &temp_size);
        if (ret != 0) {
            DEV_MON_EX_NOTSUPPORT_ERR(ret, "get flash info fail. (i=%d; ret=%d)\n", i, ret);
            (void)dsmi_mutex_v(sem_id);
            return ret;
        }
    }
    (void)dsmi_mutex_v(sem_id);
    *size = sizeof(DSMI_FLASH_ERASE_COUNT);

    return DRV_ERROR_NONE;
}

int dsmi_cmd_get_flash_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                            unsigned int *size)
{
    int ret;
    switch (sub_cmd) {
        case DSMI_FLASH_SUB_CMD_GET_ERASE_COUNT:
            ret = dsmi_cmd_get_flash_erase_count(device_id, main_cmd, sub_cmd, buf, size);
            break;
        case DSMI_FLASH_SUB_CMD_FW_WRITE_PROTECTION:
            ret = dsmi_cmd_get_device_info(device_id, main_cmd, sub_cmd, buf, size);
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return ret;
}

STATIC int dsmi_udis_get_cpu_topo(unsigned int device_id, const char *name, void *buf, unsigned int *size)
{
    int ret = 0;
    (void)name;

    ret = drv_get_cpu_topo(device_id, buf, size);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "drv_get_cpu_topo failed. (dev_id=%u; ret=%d)\n", device_id, ret);
        return ret;
    }
    return 0;
}

STATIC int dsmi_get_cust_board_info(unsigned int device_id, const char *name, void *buf, unsigned int *size)
{
    int ret = 0;
    (void)name;

    ret = halDmsGetDeviceInfoEx(device_id, DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_CUST_BOARD_INF, buf, size);
    if (ret == 0) {
        DEV_MON_DEBUG("Get device info for cust board. (dev_id=%u; ret=%d)\n", device_id, ret);
        return ret;
    }

    ret = _dsmi_get_device_info(device_id, DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_CUST_BOARD_INF, buf, size);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Failed to get cust board info by dmp. (dev_id=%u; ret=%d)\n", device_id, ret);
        return ret;
    }

    ret = halDmsSetDeviceInfoEx(device_id, DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_CUST_BOARD_INF, buf, *size);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Failed to set cust board info. (dev_id=%u; ret=%d)\n", device_id, ret);
    }

    return ret;
}

static struct udis_device_info_adapter g_get_device_info_table[] = {
    /* host ioctl */
    {DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_CUST_BOARD_INF, "cust_board_info", false, dsmi_get_cust_board_info},
    /* udis */
    {DSMI_MAIN_CMD_MEMORY, DSMI_SUB_CMD_HBM_MEMORY, "hbm_mem_info", true, dsmi_udis_get_hbm_size},
    {DSMI_MAIN_CMD_SOC_INFO, DSMI_SOC_INFO_SUB_CMD_CPU_TOPO, "cpu_topo_num", false, dsmi_udis_get_cpu_topo},
#ifdef CFG_FEATURE_UDIS_UTIL_FROM_TS
    {DSMI_MAIN_CMD_TS, DSMI_TS_SUB_CMD_AICORE_UTILIZATION_RATE, "aic_util", false, dms_get_ts_info_from_udis},
    {DSMI_MAIN_CMD_TS, DSMI_TS_SUB_CMD_VECTORCORE_UTILIZATION_RATE, "aiv_util", false, dms_get_ts_info_from_udis},
    {DSMI_MAIN_CMD_TS, DSMI_TS_SUB_CMD_NPU_MULTI_UTILIZATION_RATE, "npu_multi_util", true,
     dms_get_npu_multi_util_from_udis},
    {DSMI_MAIN_CMD_TS, DSMI_TS_SUB_CMD_NPU_MULTI_UTILIZATION_RATE_V2, "npu_multi_util_v2", true,
     dms_get_npu_multi_util_from_udis},
#endif
};

int dsmi_get_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                         unsigned int *size)
{
    int ret = 0;
    unsigned int i;
    unsigned int table_size = sizeof(g_get_device_info_table) / sizeof(g_get_device_info_table[0]);

    if ((buf == NULL) || (size == NULL)) {
        DEV_MON_ERR("Invalid parameter. (dev_id=%u; main_cmd=%u; sub_cmd=%u; buf=%d; size=%d)\n", device_id, main_cmd,
                    sub_cmd, (buf != NULL), (size != NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    for (i = 0; i < table_size; i++) {
        if ((g_get_device_info_table[i].main_cmd == main_cmd) && (g_get_device_info_table[i].sub_cmd == sub_cmd)) {
            ret = g_get_device_info_table[i].callback(device_id, g_get_device_info_table[i].name, buf, size);
            if (ret == 0) {
                return ret;
            }
            if (!g_get_device_info_table[i].enable_dmp) {
                DEV_MON_EX_NOTSUPPORT_ERR(ret,
                                          "Failed to get device info. "
                                          "(dev_id=%d; main_cmd=%d; sub_cmd=%d; ret=%d)\n",
                                          device_id, main_cmd, sub_cmd, ret);
                return ret;
            }
        }
    }
    return _dsmi_get_device_info(device_id, main_cmd, sub_cmd, buf, size);
}

int dsmi_create_capability_group(int device_id, int ts_id, struct dsmi_capability_group_info *group_info)
{
    int ret;

    if (group_info == NULL) {
        DEV_MON_ERR("group info is null\n");
        return DRV_ERROR_PARA_ERROR;
    }
    DEV_MON_EVENT("device id %d is create ts group, ts_id=%d,group_id=%d,state=%d,extend_attribute=%d,"
                  "aicore_number=%d,aivector_number=%d,sdma_number=%d,aicpu_number=%d,use_l2cache=%u\n",
                  device_id, ts_id, group_info->group_id, group_info->state, group_info->extend_attribute,
                  group_info->aicore_number, group_info->aivector_number, group_info->sdma_number,
                  group_info->aicpu_number, group_info->use_l2cache);
    ret = dsmi_cmd_create_capability_group(device_id, ts_id, group_info);
    if (ret != 0) {
        DEV_MON_ERR("Dsmi create capability group fail. (device_id=%d, ts_id=%d, group_id=%u, ret=%d)\n", device_id,
                    ts_id, group_info->group_id, ret);
        return ret;
    }

    return ret;
}

int dsmi_delete_capability_group(int device_id, int ts_id, int group_id)
{
    int ret;

    DEV_MON_EVENT("device id %d is delete ts group, ts_id=%d,group_id=%d", device_id, ts_id, group_id);
    ret = dsmi_cmd_delete_capability_group(device_id, ts_id, group_id);
    if (ret != 0) {
        DEV_MON_ERR("Dsmi delete capability group fail. (device_id=%d, ts_id=%d, group_id=%d, ret=%d)\n", device_id,
                    ts_id, group_id, ret);
        return ret;
    }

    return 0;
}

int dsmi_get_capability_group_info(int device_id, int ts_id, int group_id,
                                   struct dsmi_capability_group_info *group_info, int group_count)
{
    if (group_info == NULL) {
        DEV_MON_ERR("group info is null\n");
        return DRV_ERROR_PARA_ERROR;
    }
    return dsmi_cmd_get_capability_group_info(device_id, ts_id, group_id, group_info, group_count);
}

int dsmi_create_capability_multi_group(int device_id, struct dsmi_capability_multi_group_info *group_info)
{
    int ret;

    if (group_info == NULL) {
        DEV_MON_ERR("group info is null\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if (group_info->group_num == 0 || group_info->group_num > GROUP_NUM_MAX) {
        DEV_MON_ERR("group_num %u is invalid, should be in (0, %d]\n", group_info->group_num, GROUP_NUM_MAX);
        return DRV_ERROR_PARA_ERROR;
    }
    DEV_MON_EVENT("device id %d is create multi group, ts_id=%u, group_num=%u\n", device_id, group_info->ts_id,
                  group_info->group_num);
    ret = dsmi_cmd_create_capability_multi_group(device_id, group_info);
    if (ret != 0) {
        DEV_MON_ERR("Dsmi create capability multi group fail. (device_id=%d, ts_id=%u, group_num=%u, ret=%d)\n",
                    device_id, group_info->ts_id, group_info->group_num, ret);
        return ret;
    }

    return ret;
}

int dsmi_delete_capability_multi_group(int device_id, struct dsmi_capability_multi_group_info *group_info)
{
    int ret;

    if (group_info == NULL) {
        DEV_MON_ERR("group info is null\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if (group_info->group_num == 0 || group_info->group_num > GROUP_NUM_MAX) {
        DEV_MON_ERR("group_num %u is invalid, should be in (0, %d]\n", group_info->group_num, GROUP_NUM_MAX);
        return DRV_ERROR_PARA_ERROR;
    }
    DEV_MON_EVENT("device id %d is delete multi group, ts_id=%u, group_num=%u\n", device_id, group_info->ts_id,
                  group_info->group_num);
    ret = dsmi_cmd_delete_capability_multi_group(device_id, group_info);
    if (ret != 0) {
        DEV_MON_ERR("Dsmi delete capability multi group fail. (device_id=%d, ts_id=%u, group_num=%u, ret=%d)\n",
                    device_id, group_info->ts_id, group_info->group_num, ret);
        return ret;
    }

    return 0;
}

int dsmi_get_capability_multi_group(int device_id, struct dsmi_capability_multi_group_info *group_info)
{
    if (group_info == NULL) {
        DEV_MON_ERR("group info is null\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if (group_info->group_num == 0 || group_info->group_num > GROUP_NUM_MAX) {
        DEV_MON_ERR("group_num %u is invalid, should be in (0, %d]\n", group_info->group_num, GROUP_NUM_MAX);
        return DRV_ERROR_PARA_ERROR;
    }
    return dsmi_cmd_get_capability_multi_group(device_id, group_info);
}

int dsmi_get_total_ecc_isolated_pages_info(int device_id, int module_type,
                                           struct dsmi_ecc_pages_stru *pdevice_ecc_pages_statistics)
{
#if defined CFG_FEATURE_ECC_HBM_INFO || defined CFG_FEATURE_ECC_DDR_INFO
    int ret;

    if (module_type != DSMI_DEVICE_TYPE_DDR && module_type != DSMI_DEVICE_TYPE_HBM) {
        DEV_MON_ERR("moduletype %d is not supported\n", module_type);
        return DRV_ERROR_PARA_ERROR;
    }
#ifdef CFG_FEATURE_ECC_DDR_INFO
    if (module_type == DSMI_DEVICE_TYPE_HBM) {
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif
    if (pdevice_ecc_pages_statistics == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_total_ecc_isolated_pages_info parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (module_type == DSMI_DEVICE_TYPE_HBM) {
        ret = dsmi_udis_get_hbm_isolated_pages_info(device_id, (unsigned char)module_type,
                                                    pdevice_ecc_pages_statistics);
        if (ret == 0) {
            return 0;
        }
    }

    ret = dsmi_cmd_get_total_ecc_isolated_pages_info(device_id, (unsigned char)module_type,
                                                     pdevice_ecc_pages_statistics);
    if (ret) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_get_total_ecc_isolated_pages_info call error ret = %d!\n",
                                  device_id, ret);
        return ret;
    }

    return OK;
#else
    (void)device_id;
    (void)module_type;
    (void)pdevice_ecc_pages_statistics;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_clear_ecc_isolated_statistics_info(int device_id)
{
#if defined CFG_FEATURE_ECC_HBM_INFO || defined CFG_FEATURE_ECC_DDR_INFO
    int ret;

    DEV_MON_EVENT("clear ecc isolated statistics info. (device_id=%d; uid=%u)\n", device_id, getuid());

    ret = dsmi_cmd_clear_ecc_isolated_info(device_id);
    if (ret) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d dsmi_clear_ecc_isolated_statistics_info call error ret = %d!\n",
                                  device_id, ret);
        return ret;
    }
    return OK;
#else
    (void)device_id;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_check_partitions(const char *config_xml_path)
{
    return _dsmi_check_partitions(config_xml_path);
}

int dsmi_get_chip_count(int *chip_count)
{
    return halGetChipCount(chip_count);
}

int dsmi_list_chip(int chip_list[], int count)
{
    return halGetChipList(chip_list, count);
}

int dsmi_get_device_count_from_chip(int chip_id, int *device_count)
{
    return halGetDeviceCountFromChip(chip_id, device_count);
}

int dsmi_get_device_from_chip(int chip_id, int device_list[], int count)
{
    return halGetDeviceFromChip(chip_id, device_list, count);
}

int dsmi_parse_sdid(unsigned int sdid, struct dsmi_sdid_parse_info *sdid_parse)
{
    return halParseSDID(sdid, (struct halSDIDParseInfo *)sdid_parse);
}

int dsmi_fault_inject(DSMI_FAULT_INJECT_INFO fault_inject_info)
{
    return _dsmi_fault_inject(fault_inject_info);
}

int dsmi_get_flash_content(int device_id, DSMI_FLASH_CONTENT content_info)
{
    return _dsmi_get_flash_content(device_id, content_info);
}

int dsmi_set_flash_content(int device_id, DSMI_FLASH_CONTENT content_info)
{
    return _dsmi_set_flash_content(device_id, content_info);
}

int dsmi_get_device_state(int device_id, DSMI_DEV_NODE_STATE *node_state, unsigned int max_num, unsigned int *num)
{
    return _dsmi_get_device_state(device_id, node_state, max_num, num);
}

int dsmi_set_detect_info(unsigned int device_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                         unsigned int buf_size)
{
    int ret;
    DEV_MON_EVENT("Set detect info, (user id=%u; device_id=%u; main cmd=%u; sub cmd=%u; buf_size=%u\n", getuid(),
                  device_id, (unsigned int)main_cmd, sub_cmd, buf_size);

    ret = dsmi_check_device_id((int)device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if ((device_id != DSMI_SET_ALL_DEVICE) && (ret != 0)) {
        DEV_MON_ERR("Dsmi check device id fail. (device_id=%u)\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (main_cmd >= DSMI_DETECT_MAIN_CMD_MAX) {
        DEV_MON_ERR("Dsmi check main_cmd fail. (main_cmd=%u)\n", main_cmd);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_set_detect_info(device_id, main_cmd, sub_cmd, buf, buf_size);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Set detect info fail, (devid=%u, main_cmd=%u, sub_cmd=%u, ret=%d).\n",
                                  device_id, main_cmd, sub_cmd, ret);
    }

    return ret;
}

int dsmi_get_detect_info(unsigned int device_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                         unsigned int *buf_size)
{
    int ret = 0;

    if (main_cmd >= DSMI_DETECT_MAIN_CMD_MAX) {
        DEV_MON_ERR("Dsmi check main_cmd fail. (main_cmd=%u)\n", main_cmd);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_detect_info(device_id, main_cmd, sub_cmd, buf, buf_size);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Get detect info failed. (devid=%u; main_cmd=%u; sub_cmd=%u; ret=%d)\n",
                                  device_id, main_cmd, sub_cmd, ret);
    }

    return ret;
}

int dsmi_get_platform_info(DSMI_PLATFORM_INFO *info)
{
    int ret = 0;

    ret = drvGetPlatformInfo((unsigned int *)info);
    if (ret != 0) {
        DEV_MON_ERR("Failed to obtain platform information. (ret=%d)\n", ret);
    }

    return ret;
}

int dsmi_cmd_get_sec_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                          unsigned int *size)
{
    int ret;
    switch (sub_cmd) {
        case DSMI_SEC_SUB_CMD_PSS:
            ret = dsmi_get_muti_device_info(device_id, main_cmd, sub_cmd, buf, size);
            break;
        case DSMI_SEC_SUB_CMD_CC:
            ret = dsmi_cmd_get_device_info(device_id, main_cmd, sub_cmd, buf, size);
            break;
        case DSMI_SEC_SUB_CMD_CUST_SIGN_FLAG:
            ret = dsmi_cmd_get_custom_sign_flag(device_id, main_cmd, sub_cmd, buf, size);
            break;
        case DSMI_SEC_SUB_CMD_CUST_SIGN_USER_CERT:
            ret = dsmi_cmd_get_sign_cert(device_id, main_cmd, sub_cmd, buf, size);
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }
    return ret;
}

int dsmi_cmd_get_custom_sign_flag(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                                  unsigned int *size)
{
    int ret = 0;

    if ((buf == NULL) || (size == NULL) || (device_id > USHORT_MAX)) {
        DEV_MON_ERR("Para error, (devid=%u; buf=%d; size=%d).\n", device_id, (buf != NULL), (size != NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_check_device_id((int)device_id);
    if (ret != 0) {
        DEV_MON_ERR("Device does not exist. (device_id=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = halDmsGetSignFlag(device_id, main_cmd, sub_cmd, buf, size);
    if (ret != 0) {
        DEV_MON_ERR("Failed to invoke dms_get_sign_flag_ioctl. (devid=%u; ret=%d)\n", device_id, ret);
        return ret;
    }

    return 0;
}

drvError_t dsmi_cmd_set_custom_sign_flag(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd,
                                         const void *buf, unsigned int size)
{
    int ret = 0;
    drvError_t err = DRV_ERROR_NONE;
    unsigned int sign_flag = 0;

    if (buf == NULL || size > USHORT_MAX) {
        DEV_MON_ERR("Parameter error. (devid=%u; buf_size=%u; buf_size_max=%u)\n", device_id, size, USHORT_MAX);
        return DRV_ERROR_PARA_ERROR;
    }

    if (getuid() != 0) {
        DEV_MON_ERR("Permission denied. (devid=%u)\n", device_id);
        return DRV_ERROR_OPER_NOT_PERMITTED;
    }

    ret = memcpy_s(&sign_flag, sizeof(unsigned int), buf, size);
    if (ret != 0) {
        DEV_MON_ERR("Failed to invoke memcpy. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    if (sign_flag >= SIGN_FLAG_MAX) {
        DEV_MON_ERR("Parameter error. (devid=%u; sign_flag=%u)\n", device_id, sign_flag);
        return DRV_ERROR_PARA_ERROR;
    }

    err = halDmsSetSignFlag(device_id, main_cmd, sub_cmd, buf, size);
    if (err != DRV_ERROR_NONE) {
        DEV_MON_ERR("Failed to invoke dms_set_sign_flag_ioctl. (devid=%u; ret=%d)\n", device_id, (int)err);
        return err;
    }
    return DRV_ERROR_NONE;
}

drvError_t dsmi_cmd_set_custom_sign_cert(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd,
                                         const void *buf, unsigned int size)
{
    drvError_t err = DRV_ERROR_NONE;

    if (buf == NULL || size > USHORT_MAX) {
        DEV_MON_ERR("Parameter error. (devid=%u; buf_size=%u; buf_size_max=%u)\n", device_id, size, USHORT_MAX);
        return DRV_ERROR_PARA_ERROR;
    }

    if (getuid() != 0) {
        DEV_MON_ERR("Permission denied. (devid=%u)\n", device_id);
        return DRV_ERROR_OPER_NOT_PERMITTED;
    }

    err = halDmsSetDeviceInfoEx(device_id, main_cmd, sub_cmd, buf, size);
    if (err != DRV_ERROR_NONE) {
        DEV_MON_ERR("Failed to invoke DmsSetDeviceInfoEx. (devid=%u; ret=%d)\n", device_id, (int)err);
        return err;
    }
    return err;
}

int dsmi_cmd_get_sign_cert(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                           unsigned int *size)
{
    int ret = 0;

    if ((buf == NULL) || (size == NULL) || (device_id > USHORT_MAX)) {
        DEV_MON_ERR("Para error, (devid=%u; buf=%d; size=%d).\n", device_id, (buf != NULL), (size != NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_check_device_id((int)device_id);
    if (ret != 0) {
        DEV_MON_ERR("Device does not exist. (device_id=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = halDmsGetDeviceInfoEx(device_id, main_cmd, sub_cmd, buf, size);
    if (ret != 0) {
        DEV_MON_ERR("Failed to invoke DmsGetDeviceInfoEx. (devid=%u; ret=%d)\n", device_id, ret);
        return ret;
    }

    if (*size == 0) {
        DEV_MON_WARNING("Custom sign cert not config. (devid=%u; ret=%d)\n", device_id, *size);
        return DRV_ERROR_CONFIG_READ_FAIL;
    }

    return ret;
}