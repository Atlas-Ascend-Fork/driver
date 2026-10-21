/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include "ascend_hal.h"
#include "dm_hdc.h"
#include "dm_udp.h"
#include "dsmi_common.h"
#include "dev_mon_dmp_client.h"
#include "dev_mon_api.h"
#include "devdrv_alarm.h"
#include "config.h"
#include "dsmi_dmp_command.h"
#include "drv_type.h"
#include "ascend_hal_error.h"
#include "dsmi_common_interface.h"
#include "devdrv_user_common.h"
#include "dsmi_product_ext.h"
#include "dsmi_product_user_config.h"
#include "pbl_uda_user.h"
#include "udis_user.h"
// clang-format off

#ifdef CFG_SOC_PLATFORM_CLOUD
#include "dms_user_interface.h"
#endif

#if defined(CFG_FEATURE_SERDES_INFO) || defined(CFG_FEATURE_SERDES_INFO_A5)
int dsmi_cmd_set_serdes_info(unsigned int dev_id, SERDES_MAIN_CMD main_cmd, unsigned int sub_cmd,
    const void *buf, unsigned int buf_size);
int dsmi_cmd_get_serdes_info(unsigned int dev_id, SERDES_MAIN_CMD main_cmd, unsigned int sub_cmd,
    void *buf, unsigned int *size);
#endif

int get_nve_level_check_para(unsigned int config_cmd, unsigned char *buf, unsigned int buf_size)
{
    if (config_cmd != DEVDRV_FLASH_CONFIG_READ_PRODUCT_CMD) {
        DEV_MON_ERR("Invalid config_cmd. (config_cmd=%u)\n", config_cmd);
        return DRV_ERROR_PARA_ERROR;
    }
    if (buf == NULL) {
        DEV_MON_ERR("Buff is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if (buf_size != NVE_CONFIG_SIZE) {
        DEV_MON_ERR("Invalid buf size. (buf_size=%u)\n", buf_size);
        return DRV_ERROR_PARA_ERROR;
    }
    return 0;
}

int set_nve_level_check_para(unsigned int config_cmd, unsigned char *buf, unsigned int buf_size)
{
    if (config_cmd != DEVDRV_FLASH_CONFIG_WRITE_PRODUCT_CMD) {
        DEV_MON_ERR("Invalid config_cmd. (config_cmd=%u)\n", config_cmd);
        return DRV_ERROR_PARA_ERROR;
    }
    if (buf == NULL) {
        DEV_MON_ERR("Buff is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if (buf_size != NVE_CONFIG_SIZE) {
        DEV_MON_ERR("Invalid buf size. (buf_size=%u)\n", buf_size);
        return DRV_ERROR_PARA_ERROR;
    }
    if (*buf > RATE_LEVEL_FULL) {
        DEV_MON_ERR("Invalid buf. (buf=%u)\n", *buf);
        return DRV_ERROR_PARA_ERROR;
    }
    return 0;
}

int set_cpu_freq_check_para(unsigned int config_cmd, unsigned char *buf, unsigned int buf_size)
{
    if (config_cmd != DEVDRV_FLASH_CONFIG_WRITE_PRODUCT_CMD) {
        DEV_MON_ERR("Invalid config_cmd. (config_cmd=%u)\n", config_cmd);
        return DRV_ERROR_PARA_ERROR;
    }
    if (buf == NULL) {
        DEV_MON_ERR("Buff is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if (buf_size != GET_CPU_FREQ_CONFIG_SIZE) {
        DEV_MON_ERR("Invalid buf size. (buf_size=%u)\n", buf_size);
        return DRV_ERROR_PARA_ERROR;
    }
    if ((*buf != CPU_FREQ_UP) && (*buf != CPU_FREQ_NORM)) {
        DEV_MON_ERR("Invalid buf. (buf=%u)\n", *buf);
        return DRV_ERROR_PARA_ERROR;
    }
    return 0;
}

int get_cpu_freq_check_para(unsigned int config_cmd, unsigned char *buf, unsigned int buf_size)
{
    if (config_cmd != DEVDRV_FLASH_CONFIG_READ_PRODUCT_CMD) {
        DEV_MON_ERR("Invalid config_cmd. (config_cmd=%u)\n", config_cmd);
        return DRV_ERROR_PARA_ERROR;
    }
    if (buf == NULL) {
        DEV_MON_ERR("Buff is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if (buf_size != GET_CPU_FREQ_CONFIG_SIZE) {
        DEV_MON_ERR("Invalid buf size. (buf_size=%u)\n", buf_size);
        return DRV_ERROR_PARA_ERROR;
    }
    return 0;
}

int dsmi_get_device_ndie(int device_id, struct dsmi_soc_die_stru *pdevice_die)
{
#ifdef CFG_SOC_PLATFORM_CLOUD
    int soc_type;

    if ((device_id >= DEVDRV_MAX_DAVINCI_NUM) || (device_id < 0)) {
        DEV_MON_ERR("Invalid device id. (device_id=%d)\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (pdevice_die == NULL) {
        DEV_MON_ERR("Device die is NULL. (device_id=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    soc_type = NDIE_TYPE;
    return dsmi_cmd_get_device_die(device_id, soc_type, pdevice_die);
#else
    (void)pdevice_die;
    DEV_MON_ERR("Dsmi not support. (device_id=%d)\n", device_id);
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_get_computing_power_info(int device_id, int computing_power_type,
                                  struct dsmi_computing_power_info *computing_power_info)
{
    drvSpec_t dev_aicore_info = {0};
    int ret_t;

    if ((device_id >= DEVDRV_MAX_DAVINCI_NUM) || (device_id < 0)) {
        DEV_MON_ERR("Invalid device id. (device_id=%d)\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (computing_power_info == NULL) {
        DEV_MON_ERR("Computing_power_info is NULL. (device_id=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    switch (computing_power_type) {
        case COMPUTING_POWER_AICORE_INDEX:
            ret_t = drvGetDeviceSpec((unsigned int)device_id, &dev_aicore_info);
            if (ret_t) {
                DEV_MON_ERR("Get device spec info error. (device_id=%d; ret=%d)\n", device_id, ret_t);
                return ret_t;
            }
            computing_power_info->data1 = dev_aicore_info.aiCoreNum;
            break;
        default:
            ret_t = DRV_ERROR_PARA_ERROR;
            DEV_MON_ERR("Computing_power_type parameter error. (device_id=%d)\n", device_id);
            break;
    }

    return ret_t;
}

#ifdef CFG_PCIE_ERROR_RATE
/*****************************************************************************
 Prototype    : dsmi_cmd_get_chip_pcie_err_rate
 Description  : get soc chip pcie err rate command proc
 Return Value : 0 success
*****************************************************************************/
static int dsmi_cmd_get_chip_pcie_err_rate(int device_id, PCIE_ERR_RATE_INFO_STU *pcie_err_code_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_CHIP_PCIE_ERR_RATE, device_id, 0, sizeof(PCIE_ERR_RATE_INFO_STU))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pcie_err_code_info, sizeof(PCIE_ERR_RATE_INFO_STU))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_clear_chip_pcie_err_rate
 Description  : clear soc chip pcie err rate command proc
 Return Value : 0 success
*****************************************************************************/
static int dsmi_cmd_clear_chip_pcie_err_rate(int device_id)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_CLEAR_CHIP_PCIE_ERR_RATE, device_id, 0, 0)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}
#endif

/*****************************************************************************
 Prototype    : dsmi_get_pcie_error_rate
 Description  : get device pcie err rate
 Input        : int device_id
 Output       : struct dsmi_chip_pcie_err_rate_stru *pcie_err_code_info
 Return Value : 0 success
                other failed
  1.Date         : 2021/01/28
  Modification : Created function
*****************************************************************************/
int dsmi_get_pcie_error_rate(int device_id, struct dsmi_chip_pcie_err_rate_stru *pcie_err_code_info)
{
#ifdef CFG_PCIE_ERROR_RATE
    int ret;

    if (pcie_err_code_info == NULL) {
        DEV_MON_ERR("Dsmi_get_pcie_info parameter error.\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if ((device_id >= DEVDRV_MAX_DAVINCI_NUM) || (device_id < 0)) {
        DEV_MON_ERR("Invalid device id. (device_id=%d)\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    ret = dsmi_cmd_get_chip_pcie_err_rate(device_id, pcie_err_code_info);
    if (ret) {
        DEV_MON_ERR("Dsmi_cmd_get_chip_pcie_err_rate call error. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
#else
    (void)pcie_err_code_info;
    DEV_MON_ERR("Dsmi not support. (device_id=%d)\n", device_id);
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

/*****************************************************************************
 Prototype    : dsmi_clear_pcie_error_rate
 Description  : clear device pcie err rate
 Input        : int device_id
 Return Value : 0 success
                other failed
  1.Date         : 2021/01/28
  Modification : Created function
*****************************************************************************/
int dsmi_clear_pcie_error_rate(int device_id)
{
#ifdef CFG_PCIE_ERROR_RATE
    int ret;

    if ((device_id >= DEVDRV_MAX_DAVINCI_NUM) || (device_id < 0)) {
        DEV_MON_ERR("Invalid device id. (device_id=%d)\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    ret = dsmi_cmd_clear_chip_pcie_err_rate(device_id);
    if (ret) {
        DEV_MON_ERR("Dsmi_cmd_clear_chip_pcie_err_rate call error. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
#else
    DEV_MON_ERR("Dsmi not support. (device_id=%d)\n", device_id);
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_get_device_alarminfo(int device_id, int *alarmcount, struct dsmi_alarm_info_stru *palarminfo)
{
#ifdef CFG_DEVICE_ALARMINFO
    int ret;
    int errorcount = 0;
    unsigned int errorcode[ERROR_CODE_MAX_NUM] = {0};

    if ((device_id >= DEVDRV_MAX_DAVINCI_NUM) || (device_id < 0)) {
        DEV_MON_ERR("Invalid device id. (device_id=%d)\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if ((!alarmcount) || (!palarminfo)) {
        DEV_MON_ERR("Dsmi_get_device_alarminfo paramer error.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dmanage_get_device_errorcode((unsigned int)device_id, &errorcount, errorcode, DMANAGE_ERROR_ARRAY_NUM);
    if (ret) {
        DEV_MON_ERR("Dsmi_cmd_get_device_errorcode call error. (ret=%d)\n", ret);
        return ret;
    }
    ret = devdrv_format_error_code(device_id, errorcode, errorcount, palarminfo);
    if (ret != errorcount) {
        DEV_MON_WARNING("Error code num and alarm info num not match. (err_count=%d; ret=%d)\n", errorcount, ret);
    }

    *alarmcount = errorcount;

    return 0;
#else
    (void)alarmcount;
    (void)palarminfo;
    DEV_MON_ERR("Dsmi not support. (device_id=%d)\n", device_id);
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

STATIC int get_davinci_device_health(void)
{
    int i;
    int ret;
    int dev_cnt;
    int dev_id[MAX_DEVICE_COUNT];
    unsigned int phyId;
    char davinci_dev_path[DEV_PATH_MAX] = {0};

    ret = dsmi_get_device_count(&dev_cnt);
    if (ret) {
        DEV_MON_ERR("Get dsmi device count failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = dsmi_list_device(dev_id, dev_cnt);
    if (ret) {
        DEV_MON_ERR("List dsmi device failed. (ret=%d)\n", ret);
        return ret;
    }

    for (i = 0; i < dev_cnt; i++) {
        ret = uda_get_udevid_by_devid((unsigned int)dev_id[i], &phyId);
        if (ret) {
            DEV_MON_ERR("Device id transfer to phyid fail. (device_id=%d; ret=%d)\n", dev_id[i], ret);
            return ret;
        }

        ret = sprintf_s(davinci_dev_path, DEV_PATH_MAX, "/dev/davinci%u", phyId);
        if (ret < 0) {
            DEV_MON_ERR("Device id sprintf_s davinci_dev_path failed. (device_id=%d)\n", dev_id[i]);
            return ret;
        }

        if (access(davinci_dev_path, F_OK) != 0) {
            DEV_MON_ERR("Davinci device path does not exist. (path=\"%s\")\n", davinci_dev_path);
            return COMMON_ERR;
        }
    }

    return DRV_ERROR_NONE;
}

bool dsmi_get_device_is_950(unsigned int main_board_id)
{
    switch (main_board_id) {
        case DEV_950_1P_MAINBOARD_ID:
        case DEV_950_2P_MAINBOARD_ID:
        case DEV_950_4P_MAINBOARD_ID:
        case DEV_UBOE_CARD_950_MAIN_BOARD_ID:
        case DEV_950_pod_2D_MAINBOARD_ID:
        case DEV_950_pod_2D_BackUp_MAINBOARD_ID:
        case DEV_950_pod_1D_MAINBOARD_ID:
        case DEV_950_pod_PCIE_MAINBOARD_ID:
        case DEV_950_pod_UBoE_MAINBOARD_ID:
        case DEV_950_Server_A_K_2_4_MAINBOARD_ID:
        case DEV_950_Server_A_K_2_4_UBoE_MAINBOARD_ID:
        case DEV_950_Server_A_K_2_6_MAINBOARD_ID:
        case DEV_950_Server_A_K_2_6_UBoE_MAINBOARD_ID:
        case DEV_950_Server_A_X_2_4_MAINBOARD_ID:
        case DEV_950_Server_A_X_2_4_UBoE_MAINBOARD_ID:
        case DEV_950_Server_A_X_PCIE_MAINBOARD_ID:
        case DEV_950_Server_A_X_UBoE_MAINBOARD_ID:
        case DEV_950_UBX_PCIE_MAINBOARD_ID:
        case DEV_950_UBX_UBoE_MAINBOARD_ID:
            return TRUE;
        default:
            return FALSE;
    }
}

int dsmi_get_mainboard_id_950(int chip_id, unsigned int *main_board_id)
{
    int ret;
    struct devdrv_device_info data = {0};

    ret = drvGetDevInfo((unsigned int)chip_id, &data);
    if (ret != DRV_ERROR_NONE) {
        DEV_MON_ERR("get dev mainboard_id info failed.(ret=%d)\n", ret);
        return ret;
    }
    *main_board_id = data.mainboard_id;
    return DRV_ERROR_NONE;
}

int dsmi_get_driver_health(unsigned int *phealth)
{
    int ret;
    int i;
    unsigned int board_type = BOARD_TYPE_EP;
    const char *host_dev_path_ep[] = {DEV_HISI_HDC_PATH, DEV_DAVINCI_MANAGER_PATH, DEV_DEVMMM_SVM_PATH};
    const char *host_dev_path_rc[] = {DEV_DAVINCI_MANAGER_PATH, DEV_SVM0_PATH};
    int dev_path_ep_num, dev_path_rc_num;
    unsigned int main_board_id = 0;

    if (phealth == NULL) {
        DEV_MON_ERR("Get driver health parameter error.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_get_mainboard_id_950(0, &main_board_id);
    if (ret != 0) {
        DEV_MON_ERR("dev_mon_get_mainboard_id_950 failed.(ret=%d)\n", ret);
        return DRV_ERROR_PARA_ERROR;
    }

    dev_path_ep_num = sizeof(host_dev_path_ep) / sizeof(char*);
    dev_path_rc_num = sizeof(host_dev_path_rc) / sizeof(char*);

#ifdef CFG_RC_DRIVER_HEALTH
    board_type = BOARD_TYPE_RC;
#endif

    if (board_type == BOARD_TYPE_RC) {
        for (i = 0; i < dev_path_rc_num; i++) {
            if (access(host_dev_path_rc[i], F_OK) != 0) {
                *phealth = DSMI_HEALTH_ERROR_LEVEL;
                return DRV_ERROR_NONE;
            }
        }
    } else {
        for (i = 0; i < dev_path_ep_num; i++) {
            if (access(host_dev_path_ep[i], F_OK) == 0) {
                continue;
            }
            if ((strcmp(host_dev_path_ep[i], DEV_DEVMMM_SVM_PATH) == 0) && dsmi_get_device_is_950(main_board_id)) {
                *phealth = 0;
            } else {
                *phealth = DSMI_HEALTH_ERROR_LEVEL;
                return DRV_ERROR_NONE;
            }
        }
    }

    ret = get_davinci_device_health();
    if (ret) {
        *phealth = DSMI_HEALTH_ERROR_LEVEL;
        return DRV_ERROR_NONE;
    }

    *phealth = 0;
    return DRV_ERROR_NONE;
}

int dsmi_get_driver_errorcode(int *errorcount, unsigned int *perrorcode)
{
    int ret;
    int i;
    unsigned int board_type = BOARD_TYPE_EP;
    int error_len = 0;
    const char *host_dev_path_ep[] = {DEV_HISI_HDC_PATH, DEV_DAVINCI_MANAGER_PATH, DEV_DEVMMM_SVM_PATH};
    const char *host_dev_path_rc[] = {DEV_DAVINCI_MANAGER_PATH, DEV_SVM0_PATH};
    unsigned int error_code_ep[] = {HOST_HDC_NOT_EXIST, HOST_MANAGER_NOT_EXIST, HOST_SVM_NOT_EXIST};
    unsigned int error_code_rc[] = {HOST_MANAGER_NOT_EXIST, HOST_SVM_NOT_EXIST};
    int dev_path_ep_num, dev_path_rc_num;

    if (errorcount == NULL || perrorcode == NULL) {
        DEV_MON_ERR("Get host errorcode parameter error.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    dev_path_ep_num = sizeof(host_dev_path_ep) / sizeof(char*);
    dev_path_rc_num = sizeof(host_dev_path_rc) / sizeof(char*);

#ifdef CFG_RC_DRIVER_HEALTH
    board_type = BOARD_TYPE_RC;
#endif

    if (board_type == BOARD_TYPE_RC) {
        for (i = 0; i < dev_path_rc_num; i++) {
            if (access(host_dev_path_rc[i], F_OK) != 0) {
                perrorcode[error_len] = error_code_rc[i];
                error_len++;
            }
        }
    } else {
        for (i = 0; i < dev_path_ep_num; i++) {
            if (access(host_dev_path_ep[i], F_OK) != 0) {
                perrorcode[error_len] = error_code_ep[i];
                error_len++;
            }
        }
    }

    ret = get_davinci_device_health();
    if (ret) {
        perrorcode[error_len] = DEV_DAVINCI_NOT_EXIST;
        error_len++;
    }

    *errorcount = error_len;
    return DRV_ERROR_NONE;
}

int dsmi_pcie_hot_reset(int device_id)
{
#ifdef CFG_PCIE_HOT_RESET
    if ((device_id >= DEVDRV_MAX_DAVINCI_NUM) || (device_id < 0)) {
        DEV_MON_ERR("Invalid device id. (device_id=%d)\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    return dsmi_hot_reset_soc(device_id);
#else
    DEV_MON_ERR("Dsmi not support. (device_id=%d)\n", device_id);
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_get_pcie_bdf(int device_id, struct tag_pcie_bdfinfo *pcie_bdfinfo)
{
#ifdef CFG_PCIE_BDF
    int ret = 0;
    int bus = 0;
    int dev = 0;
    int func = 0;

    if ((device_id >= DEVDRV_MAX_DAVINCI_NUM) || (device_id < 0)) {
        DEV_MON_ERR("Invalid device id. (device_id=%d)\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (pcie_bdfinfo == NULL) {
        DEV_MON_ERR("Dsmi_get_pcie_info parameter error. (device_id=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    /* get bus, deviceid and func */
    ret = drvDeviceGetPcieInfo(device_id, &bus, &dev, &func);
    if (ret) {
        DEV_MON_ERR("Get device pcie info failed. (device_id=%d; ret=%d)\n", device_id, ret);
        return ret;
    }

    pcie_bdfinfo->bdf_busid = (unsigned int)bus;
    pcie_bdfinfo->bdf_deviceid = (unsigned int)dev;
    pcie_bdfinfo->bdf_funcid = (unsigned int)func;

    return 0;
#else
    (void)pcie_bdfinfo;
    DEV_MON_ERR("Dsmi not support. (device_id=%d)\n", device_id);
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

STATIC int dsmi_cmd_set_user_config_product(int device_id, unsigned int config_name_len, const char *config_name,
    unsigned int buf_size, const unsigned char *buf)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_SET_USER_CONFIG_PRODUCT, device_id,
        (config_name_len + buf_size + sizeof(char) + sizeof(int)), 0)
    DM_COMMAND_ADD_REQ(&config_name_len, sizeof(char))
    DM_COMMAND_ADD_REQ(&buf_size, sizeof(int))
    DM_COMMAND_ADD_REQ(config_name, config_name_len)
    DM_COMMAND_ADD_REQ(buf, buf_size)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}
STATIC int dsmi_cmd_get_user_config_product(int device_id, unsigned int config_name_len, const char *config_name,
    unsigned int *p_buf_size, unsigned char *buf)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_USER_CONFIG_PRODUCT, device_id, (config_name_len + sizeof(char) + sizeof(int)),
        *p_buf_size)
    DM_COMMAND_ADD_REQ(p_buf_size, sizeof(int))
    DM_COMMAND_ADD_REQ(&config_name_len, sizeof(char))
    DM_COMMAND_ADD_REQ(config_name, config_name_len)
    DM_COMMAND_SEND()
    DM_COMMAND_GET_RSP_LEN(p_buf_size)
    DM_COMMAND_PUSH_OUT(buf, *p_buf_size)
    DM_COMMAND_END()
}
int dsmi_cmd_clear_user_config_product(int device_id, unsigned int config_name_len, const char *config_name)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_CLEAR_USER_CONFIG_PRODUCT, device_id, (config_name_len + sizeof(char)), 0)
    DM_COMMAND_ADD_REQ(&config_name_len, sizeof(char))
    DM_COMMAND_ADD_REQ(config_name, config_name_len)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

#if defined(CFG_SOC_PLATFORM_MINI) && defined(CFG_SOC_PLATFORM_RC)
static int read_shadow_password(struct password *passwd)
{
    int ret;
    int clr_ret;
    FILE *fp = NULL;
    char line_buf[MAX_LINE_SIZE] = {0};

    fp = fopen(PASSWD_FILE_PATH, "r");
    if (fp == NULL) {
        DEV_MON_ERR("Open file failed.\n");
        return DRV_ERROR_OPER_NOT_PERMITTED;
    }

    while (fgets(line_buf, MAX_LINE_SIZE, fp) != NULL) {
        if (strstr(line_buf, passwd->user_name)) {
            ret = sscanf_s(line_buf, "%[^:]:$%d$%[^:]\n",
                passwd->user_name, MAX_NAME_LEN, &(passwd->crypt_type), passwd->encrypted_passwd, MAX_PASSWD_LEN);
            if (ret != SSCANF_NUM || passwd->crypt_type != CRYPT_SHA512) {
                DEV_MON_ERR("Analysys passwd failed. (scanf_num=%d, crypt_type=%d)\n", ret, passwd->crypt_type);
                ret = COMMON_ERR;
                goto OUT;
            }
            ret = 0;
            goto OUT;
        }
    }

    ret = COMMON_ERR;
    DEV_MON_ERR("Can not find user password. (user=\"%s\")\n", passwd->user_name);

OUT:
    fclose(fp);
    fp = NULL;
    clr_ret = memset_s(line_buf, MAX_LINE_SIZE, 0, MAX_LINE_SIZE);
    if (clr_ret) {
        DEV_MON_ERR("Memset_s passwd line buf failed. (clr_ret=%d)", clr_ret);
    }
    return ret;
}

STATIC int check_para_recovery_mode_passwd(unsigned char *buff, unsigned int buff_size)
{
    int ret;
    int clr_ret;
    struct password user_passwd = {{USER_NAME}, 0, {0}};
    struct password root_passwd = {{ROOT_NAME}, 0, {0}};
    unsigned int passwd_size = sizeof(struct password);

    if (buff_size < (passwd_size + passwd_size)) {
        DEV_MON_ERR("Check para failed. (buf_size=%u)", buff_size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = memset_s(buff, buff_size, 0, buff_size);
    if (ret) {
        DEV_MON_ERR("Memset_s buf failed. (ret=%d; buf_size=%u)", ret, buff_size);
        return ret;
    }
    ret = read_shadow_password(&user_passwd);
    if (ret) {
        DEV_MON_ERR("Read user passwdord form shadow failed. (ret=%d)\n", ret);
        goto OUT;
    }

    ret = memcpy_s(buff, buff_size, &user_passwd, passwd_size);
    if (ret) {
        DEV_MON_ERR("Memcpy_s user passwd failed. (ret=%d)\n", ret);
        goto OUT;
    }

    ret = read_shadow_password(&root_passwd);
    if (ret) {
        (void)memset_s(buff, buff_size, 0, buff_size);
        DEV_MON_ERR("Read root passwdord form shadow failed. (ret=%d)\n", ret);
        goto OUT;
    }

    ret = memcpy_s(buff + passwd_size, buff_size - passwd_size, &root_passwd, passwd_size);
    if (ret) {
        (void)memset_s(buff, buff_size, 0, buff_size);
        DEV_MON_ERR("Memcpy_s root passwd failed. (ret=%d)\n", ret);
        goto OUT;
    }

OUT:
    clr_ret = memset_s(&user_passwd, passwd_size, 0, passwd_size);
    if (clr_ret) {
        DEV_MON_ERR("Memset_s user passwd failed. (clr_ret=%d)\n", clr_ret);
    }
    clr_ret = memset_s(&root_passwd, passwd_size, 0, passwd_size);
    if (clr_ret) {
        DEV_MON_ERR("Memset_s root passwd failed. (clr_ret=%d)\n", clr_ret);
    }

    return ret;
}
#endif

int cert_expired_config_para_check(unsigned int config_cmd, unsigned char *buf, unsigned int buf_size)
{
    if (buf_size != MB_CONFIG_EXPIRED_SIZE || buf == NULL) {
        DEV_MON_ERR("Input para of config is error. (buf_size=%u)\n", buf_size);
        return DRV_ERROR_PARA_ERROR;
    }
    if (config_cmd == DEVDRV_FLASH_CONFIG_WRITE_PRODUCT_CMD) {
        if (*buf > EXPIERD_THRESHOLD_MAX || *buf < EXPIERD_THRESHOLD_MIN) {
            DEV_MON_ERR("Input para of config is out of range. (value=%u)\n", *buf);
            return DRV_ERROR_PARA_ERROR;
        }
    }
    return 0;
}

STATIC int check_user_cfg_para(int device_id, const char *config_name, unsigned int config_cmd, unsigned int *cfg_index)
{
    unsigned int i;
    unsigned int config_name_len;

    if ((device_id < 0) || (device_id >= DEVDRV_MAX_DAVINCI_NUM) || (config_name == NULL)) {
        DEV_MON_ERR("Input para is error. (device_id=%d; config_name=%d)\n", device_id, (int)(config_name == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    config_name_len = (unsigned int)strlen(config_name) + 1;
    if (config_name_len > DSMI_USER_CONFIG_NAME_MAX) {
        DEV_MON_ERR("Config name length is error. (device_id=%d; len=%u)\n", device_id, config_name_len);
        return DRV_ERROR_PARA_ERROR;
    }
    for (i = 0; i < UC_PROD_ITEM_MAX_NUM; i++) {
        if (strncmp(g_user_config_info_product[i].name, config_name, config_name_len) == 0) {
            break;
        }
    }
    if (i >= UC_PROD_ITEM_MAX_NUM) {
        DEV_MON_ERR("Can not find user config name in user_config_info. (config_name=\"%s\")\n", config_name);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((g_user_config_info_product[i].support_flag & (1U << config_cmd)) == 0) {
        DEV_MON_ERR("Config not support the cmd. (config_name=\"%s\"; cfg_index=%d; cmd_index=%u; support_flag=%u)\n",
            config_name, i, config_cmd, g_user_config_info_product[i].support_flag);
        return DRV_ERROR_NOT_SUPPORT;
    }
    *cfg_index = i;

    return 0;
}

STATIC int check_user_config_buff_para(int device_id, const char *config_name, unsigned int config_cmd,
    unsigned int buf_size, unsigned char *buf)
{
    int ret;
    unsigned int cmd_index = 0;

    ret = check_user_cfg_para(device_id, config_name, config_cmd, &cmd_index);
    if (ret) {
        DEV_MON_ERR("Failed to check user config para. (dev_id=%d, ret=%d)\n", device_id, ret);
        return ret;
    }

    if ((buf == NULL) || (buf_size > DSMI_USER_CONFIG_BUF_MAX_LEN)) {
        DEV_MON_ERR("Failed to check buff. (dev_id=%d; buf=%d; buf_size=%u)\n",
            device_id, (int)(buf == NULL), buf_size);
        return DRV_ERROR_PARA_ERROR;
    }

    if (g_user_config_info_product[cmd_index].check_para != NULL) {
        ret = g_user_config_info_product[cmd_index].check_para(config_cmd, buf, buf_size);
        if (ret) {
            DEV_MON_ERR("Failed to check para by function. (ret=%d; cfg_cmd=%u)\n", ret, config_cmd);
            return ret;
        }
    }

    return 0;
}

bool is_product_user_config_item_by_name(const char *config_name, unsigned char name_len)
{
    int i;

    if (config_name == NULL || name_len > DSMI_USER_CONFIG_NAME_MAX) {
        DEV_MON_ERR("Check para failed. (config_name_len=%u)\n", name_len);
        return false;
    }

    for (i = 0; i < UC_PROD_ITEM_MAX_NUM; i++) {
        if (strncmp(g_user_config_info_product[i].name, config_name, name_len) == 0) {
            return true;
        }
    }

    DEV_MON_DEBUG("Product not support the user config item. (cfg_name=\"%s\"\n)", config_name);
    return false;
}

int dsmi_product_set_user_config(int device_id, const char *config_name, unsigned int buf_size, unsigned char *buf)
{
    int ret;
    unsigned int config_name_len;

    ret = check_user_config_buff_para(device_id, config_name, DEVDRV_FLASH_CONFIG_WRITE_PRODUCT_CMD, buf_size, buf);
    if (ret) {
        DEV_MON_ERR("Check user config para failed. (ret=%d)\n", ret);
        return ret;
    }

#if defined(CFG_SOC_PLATFORM_MINI) && defined(CFG_SOC_PLATFORM_RC)
    if (strcmp(config_name, RECOVERY_PASSWD_CONFIG) == 0) {
        ret = check_para_recovery_mode_passwd(buf, buf_size);
        if (ret) {
            DEV_MON_ERR("Set recovery passwd para failed. (device_id=%d; ret=%d)\n", device_id, ret);
            return ret;
        }
    }
#endif

    config_name_len = (unsigned int)strlen(config_name) + 1;
    ret = dsmi_cmd_set_user_config_product(device_id, config_name_len, config_name, buf_size, buf);
    if (ret != 0) {
        DEV_MON_ERR("Set user config failed. (ret=%d; device_id=%d)\n", ret, device_id);
    }
    return ret;
}

int dsmi_product_get_user_config(int device_id, const char *config_name, unsigned int buf_size, unsigned char *buf)
{
    int ret;
    unsigned int buf_size_tmp = buf_size;
    unsigned int config_name_len;

    ret = check_user_config_buff_para(device_id, config_name, DEVDRV_FLASH_CONFIG_READ_PRODUCT_CMD, buf_size, buf);
    if (ret) {
        DEV_MON_ERR("Check user config para failed. (ret=%d)\n", ret);
        return ret;
    }

    config_name_len = (unsigned int)strlen(config_name) + 1;
    ret = dsmi_cmd_get_user_config_product(device_id, config_name_len, config_name, &buf_size_tmp, buf);
    if (buf_size < buf_size_tmp) {
        DEV_MON_ERR("Return size error. (devid=%d; set_size=%u; return_size=%u)\n", device_id, buf_size, buf_size_tmp);
        return DRV_ERROR_PARA_ERROR;
    }

    if (ret) {
        DEV_MON_ERR("Get user config failed. (ret=%d; device_id=%d)\n", ret, device_id);
    }
    return ret;
}

int dsmi_product_clear_user_config(int device_id, const char *config_name)
{
    int ret;
    unsigned int cmd_index = 0;
    unsigned int config_name_len;

    ret = check_user_cfg_para(device_id, config_name, DEVDRV_FLASH_CONFIG_CLEAR_PRODUCT_CMD, &cmd_index);
    if (ret) {
        DEV_MON_ERR("Failed to check user config para. (dev_id=%d, ret=%d)\n", device_id, ret);
        return ret;
    }

    config_name_len = (unsigned int)strlen(config_name) + 1;
    ret = dsmi_cmd_clear_user_config_product(device_id, config_name_len, config_name);
    if (ret) {
        DEV_MON_ERR("Clear user config failed. (device_id=%d)\n", device_id);
    }
    return ret;
}

STATIC int dsmi_cmd_get_memory_info(int device_id, struct dsmi_get_memory_info_stru *pdevice_memory_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_MEM_INFO, device_id, 0, sizeof(struct dsmi_get_memory_info_stru))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pdevice_memory_info, sizeof(struct dsmi_get_memory_info_stru))
    DM_COMMAND_END()
}

DLLEXPORT int dsmi_get_memory_info_v2(int device_id, struct dsmi_get_memory_info_stru *pdevice_memory_info)
{
    int ret;

    if (pdevice_memory_info == NULL) {
        DEV_MON_ERR("Dsmi_get_memory_infoV2 parameter error.(dev_id=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_memory_info(device_id, pdevice_memory_info);
    if (ret) {
        DEV_MON_ERR("Dsmi_cmd_get_memory_info call error.(dev_id=%d; ret=%d)\n", device_id, ret);
        return ret;
    }

    return ret;
}

#ifdef CFG_SOC_PLATFORM_CLOUD_V4
STATIC int dsmi_cmd_get_hbm_manufacturer_id_from_udis(unsigned int device_id, unsigned int *manufacturer_id)
{
    int ret = 0;
    struct udis_dev_info udis_hbm_info = {0};
 
    udis_hbm_info.module_type = UDIS_MODULE_DEVMNG;
    ret = strcpy_s(udis_hbm_info.name, UDIS_MAX_NAME_LEN, "hbm_mfr_id");
    if (ret != 0) {
        DEV_MON_ERR("strcpy hbm mfr info failed. (devid=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_INNER_ERR;
    }
    ret = udis_get_device_info(device_id, &udis_hbm_info);
    if (ret != 0) {
        DEV_MON_ERR("udis_get_device_info failed. (devid=%d; ret=%d)\n", device_id, ret);
        return ret;
    }
    ret = memcpy_s(manufacturer_id, sizeof(unsigned int), udis_hbm_info.data, udis_hbm_info.data_len);
    if (ret != 0) {
        DEV_MON_ERR("Copy udis_hbm_info.data to manufacturer_id failed. (devid=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_INNER_ERR;
    }
    return ret;
}
#endif

#ifdef CFG_FEATURE_HBM_MANUFACTURER_ID
STATIC int dsmi_check_hbm_manufacturer_id(unsigned int manufacturer_id)
{
    uint8_t check = 0;
    uint32_t data = manufacturer_id;

    check ^= (uint8_t)((data >> CHECK_THREE_BYTE_BIT) & 0xFF);
    check ^= (uint8_t)((data >> CHECK_TWO_BYTE_BIT) & 0xFF);
    check ^= (uint8_t)((data >> CHECK_ONE_BYTE_BIT) & 0xFF);
    check ^= (uint8_t)(data & 0xFF);

    if (check == 0) {
        return DRV_ERROR_NONE;
    } else {
        DEV_MON_ERR("The manufacturer_id check error. (manufacturer_id=0x%x)\n", manufacturer_id);
        return DRV_ERROR_INNER_ERR;
    }
}
#endif

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
STATIC int dsmi_cmd_get_hbm_manufacturer_id(unsigned int device_id, unsigned int *manufacturer_id)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_HBM_MANUFACTURER_ID, device_id, 0, sizeof(unsigned int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(manufacturer_id, sizeof(unsigned int))
    DM_COMMAND_END()
}
#endif

DLLEXPORT int dsmi_get_hbm_manufacturer_id(unsigned int device_id, unsigned int *manufacturer_id)
{
#ifdef CFG_FEATURE_HBM_MANUFACTURER_ID
    int ret;

    if ((device_id >= DEVDRV_MAX_DAVINCI_NUM) || (manufacturer_id == NULL)) {
        DEV_MON_ERR("The parameter error.(device_id=%u)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
#ifndef CFG_SOC_PLATFORM_CLOUD_V4
    ret = dsmi_cmd_get_hbm_manufacturer_id(device_id, manufacturer_id);
    if (ret) {
        DEV_MON_ERR("Dsmi_cmd_get_hbm_manufacturer_id call error.(device_id=%u; ret=%d)\n", device_id, ret);
        return ret;
    }
#else
    ret = dsmi_cmd_get_hbm_manufacturer_id_from_udis(device_id, manufacturer_id);
    if (ret) {
        DEV_MON_ERR("dsmi_cmd_get_hbm_manufacturer_id_from_udis call error.(device_id=%u; ret=%d)\n", device_id, ret);
        return ret;
    }
#endif
    ret = dsmi_check_hbm_manufacturer_id(*manufacturer_id);
    if (ret) {
        DEV_MON_ERR("Check hbm manufacturer_id failed.(device_id=%u; ret=%d)\n", device_id, ret);
        return ret;
    }

    return ret;
#else
    DEV_MON_ERR("Dsmi not support. (device_id=%u)\n", device_id);
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

#ifdef CFG_ATTEST_EVIDENCE
STATIC int dsmi_cmd_get_attest_evidence(unsigned int device_id, ATTEST_OPERATE_REQUEST *req,
    ATTEST_OPERATE_RESPONSE *rep)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_ATTEST_EVIDENCE, device_id,
        sizeof(ATTEST_OPERATE_REQUEST), sizeof(ATTEST_OPERATE_RESPONSE))
    DM_COMMAND_ADD_REQ(req, sizeof(ATTEST_OPERATE_REQUEST))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(rep, sizeof(ATTEST_OPERATE_RESPONSE))
    DM_COMMAND_END()
}

STATIC int dsmi_cmd_get_akcert(unsigned int device_id, unsigned int *akcertLen, ATTEST_OPERATE_RESPONSE *rep)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_AKCERT, device_id, sizeof(unsigned int), sizeof(ATTEST_OPERATE_RESPONSE))
    DM_COMMAND_ADD_REQ(akcertLen, sizeof(unsigned int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(rep, sizeof(ATTEST_OPERATE_RESPONSE))
    DM_COMMAND_END()
}
#endif

DLLEXPORT int dsmi_attest_get_evidence(unsigned int device_id, ATTEST_OPERATE_REQUEST *req,
    ATTEST_OPERATE_RESPONSE *rep)
{
#ifdef CFG_ATTEST_EVIDENCE
    int ret;

    if ((device_id >= DEVDRV_MAX_DAVINCI_NUM) || (req == NULL) || (rep == NULL)) {
        DEV_MON_ERR("The parameter error.(device_id=%u)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_attest_evidence(device_id, req, rep);
    if (ret != 0) {
        DEV_MON_ERR("dsmi_cmd_get_attest_evidence call error. ret = %d", ret);
    }
    return ret;
#else
    (void)device_id;
    (void)req;
    (void)rep;
    DEV_MON_ERR("Dsmi do not support.");
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

DLLEXPORT int dsmi_attest_get_akcert(unsigned int device_id, unsigned int *akcertLen, ATTEST_OPERATE_RESPONSE *rep)
{
#ifdef CFG_ATTEST_EVIDENCE
    int ret;

    if (akcertLen == NULL || rep == NULL) {
        DEV_MON_ERR("The parameter error. device_id = %d", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (*akcertLen == 0) {
        DEV_MON_ERR("The parameter error.akcertLen=0 device_id = %d", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (device_id >= DEVDRV_MAX_DAVINCI_NUM) {
        DEV_MON_ERR("The parameter error.(device_id=%u)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_akcert(device_id, akcertLen, rep);
    if (ret != 0) {
        DEV_MON_ERR("dsmi_cmd_get_akcert call error.ret = %d", ret);
        return ret;
    }
    return ret;
#else
    (void)device_id;
    (void)akcertLen;
    (void)rep;
    DEV_MON_ERR("Dsmi do not support.");
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

#ifdef CFG_FEATURE_ROOTKEY_STATUS
STATIC int dsmi_cmd_get_rootkey_status(unsigned int device_id, unsigned int key_type, unsigned int *rootkey_status)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_ROOTKEY_STATUS, device_id, sizeof(unsigned int), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&key_type, sizeof(unsigned int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(rootkey_status, sizeof(unsigned int))
    DM_COMMAND_END()
}
#endif

DLLEXPORT int dsmi_get_rootkey_status(unsigned int device_id, unsigned int key_type, unsigned int *rootkey_status)
{
#ifdef CFG_FEATURE_ROOTKEY_STATUS
    int ret;

    if ((device_id >= DEVDRV_MAX_DAVINCI_NUM) || (rootkey_status == NULL)) {
        DEV_MON_ERR("The parameter error.(device_id=%u)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_rootkey_status(device_id, key_type, rootkey_status);
    if (ret) {
        DEV_MON_ERR("dsmi_cmd_get_rootkey_status call error.(device_id=%u; key_type=%u; ret=%d)\n",
            device_id, key_type, ret);
    }

    return ret;
#else
    (void)key_type;
    (void)rootkey_status;
    DEV_MON_ERR("Dsmi not support. (device_id=%u)\n", device_id);
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

DLLEXPORT int dsmi_get_hccs_status(unsigned int device_id1, unsigned int device_id2, int *hccs_status)
{
#ifdef CFG_SOC_PLATFORM_CLOUD
    int ret;
    int topology_type;
    int hccs_status_tmp;

    if ((device_id1 == device_id2) ||
        (device_id1 >= DEVDRV_MAX_DAVINCI_NUM) || (device_id2 >= DEVDRV_MAX_DAVINCI_NUM)) {
        DEV_MON_ERR("The device ID parameter error. (device_id1=%u; device_id2=%u)\n",
            device_id1, device_id2);
        return DRV_ERROR_PARA_ERROR;
    }

    if (hccs_status == NULL) {
        DEV_MON_ERR("Input type is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    // TOPOLOGY_HCCS(0) 表示hccs互联状态， 其他(1)表示hccs互联状态断开
    ret = dms_get_device_topology(device_id1, device_id2, &topology_type);
    if (ret != 0) {
        DEV_MON_ERR("Get device topology failed. (device_id1=%u; device_id2=%u; ret=%d)\n",
            device_id1, device_id2, ret);
        return ret;
    }

    hccs_status_tmp = (topology_type == TOPOLOGY_HCCS) ? HCCS_ON : HCCS_OFF;
    DEV_MON_DEBUG("Get hccs_status success. (device_id1=%u; device_id2=%u; topology_type=%d, hccs_status=%d)\n",
        device_id1, device_id2, topology_type, hccs_status_tmp);
    *hccs_status = hccs_status_tmp;
    return ret;
#else
    (void)hccs_status;
    DEV_MON_ERR("Dsmi not support. (device_id1=%u, device_id2=%u)\n", device_id1, device_id2);
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_get_vdevice_info(__attribute__((unused)) unsigned int devid, __attribute__((unused)) struct dsmi_vdev_info *info)
{
    return DRV_ERROR_NOT_SUPPORT;
}

#if defined CFG_SOC_PLATFORM_CLOUD || defined CFG_FEATURE_ECC_DDR || defined CFG_SOC_PLATFORM_CLOUD_V4
STATIC int dsmi_cmd_get_multi_ecc_time_info(int device_id, struct dsmi_multi_ecc_time_data *multi_ecc_time_data)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_MULTI_ECC_TIME_INFO, device_id, sizeof(unsigned char),
        sizeof(struct dsmi_multi_ecc_time_data))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(multi_ecc_time_data, sizeof(struct dsmi_multi_ecc_time_data))
    DM_COMMAND_END()
}

STATIC int dsmi_cmd_get_multi_ecc_record_info(int device_id, int data_index, unsigned char read_type,
    unsigned char module_type, struct dsmi_ecc_common_data *ecc_common_data_s)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_ECC_RECORD_INFO, device_id,
        sizeof(unsigned int) + sizeof(unsigned char) + sizeof(unsigned char), sizeof(struct dsmi_ecc_common_data))
    DM_COMMAND_ADD_REQ(&data_index, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&read_type, sizeof(unsigned char))
    DM_COMMAND_ADD_REQ(&module_type, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(ecc_common_data_s, sizeof(struct dsmi_ecc_common_data))
    DM_COMMAND_END()
}
#endif

int dsmi_get_multi_ecc_time_info(int device_id, struct dsmi_multi_ecc_time_data *multi_ecc_time_data)
{
#if defined CFG_SOC_PLATFORM_CLOUD || defined CFG_FEATURE_ECC_DDR || defined CFG_SOC_PLATFORM_CLOUD_V4
    int ret;

    if (multi_ecc_time_data == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_multi_ecc_time_info parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    ret = dsmi_cmd_get_multi_ecc_time_info(device_id, multi_ecc_time_data);
    if (ret) {
        DEV_MON_ERR("devid %d dsmi_get_multi_ecc_time_info call error ret = %d!\n", device_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
#else
    DEV_MON_ERR("Dsmi get multi ecc time info not support.\n");
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_get_multi_ecc_record_info(int device_id, unsigned int *ecc_count, unsigned char read_type,
    unsigned char module_type, struct dsmi_ecc_common_data *ecc_common_data_s)
{
#if defined CFG_SOC_PLATFORM_CLOUD || defined CFG_FEATURE_ECC_DDR || defined CFG_SOC_PLATFORM_CLOUD_V4
    int ret;
    unsigned int data_index = 0;
    unsigned int record_count = 0;
    struct dsmi_ecc_pages_stru pdevice_ecc_pages_statistics = {0};
    struct dsmi_multi_ecc_time_data multi_ecc_time_data = {0};

    if ((module_type != DSMI_DEVICE_TYPE_HBM) && (module_type != DSMI_DEVICE_TYPE_DDR)) {
        DEV_MON_ERR("moduletype %d is not supported\n", module_type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (ecc_common_data_s == NULL || ecc_count == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_multi_ecc_record_info parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (read_type == SINGLE_ECC_INFO_READ) {
        ret = dsmi_cmd_get_total_ecc_isolated_pages_info(device_id, DSMI_HBM_RECORDED_SINGLE_ADDR,
            &pdevice_ecc_pages_statistics);
        if (ret) {
            DEV_MON_ERR("devid %d dsmi_cmd_get_total_ecc_isolated_pages_info call error ret = %d!\n", device_id, ret);
            return ret;
        }
        *ecc_count = pdevice_ecc_pages_statistics.corrected_ecc_errors_aggregate_total;
    }

    if (read_type == MULTI_ECC_INFO_READ) {
        ret = dsmi_cmd_get_multi_ecc_time_info(device_id, &multi_ecc_time_data);
        if (ret) {
            DEV_MON_EVENT("devid %d dsmi_cmd_get_multi_ecc_time_info call error ret = %d!\n", device_id, ret);
            return ret;
        }
        *ecc_count = multi_ecc_time_data.multi_record_count;
    }

    record_count = (*ecc_count) > MAX_RECORD_ECC_ADDR_COUNT ? MAX_RECORD_ECC_ADDR_COUNT : (*ecc_count);
    for (data_index = 0; data_index < record_count; data_index++) {
        ret = dsmi_cmd_get_multi_ecc_record_info(device_id, (int)data_index, read_type, module_type,
            &ecc_common_data_s[data_index]);
        if (ret) {
            DEV_MON_ERR("devid %d dsmi_cmd_get_multi_ecc_record_info call error ret = %d!\n", device_id, ret);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
#else
    DEV_MON_ERR("Dsmi get multi ecc record info not support.\n");
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

DLLEXPORT int dsmi_get_serdes_info(unsigned int device_id, SERDES_MAIN_CMD main_cmd,
    unsigned int sub_cmd, void* buf, unsigned int* size)
{
#if defined(CFG_FEATURE_SERDES_INFO) || defined(CFG_FEATURE_SERDES_INFO_A5)
    int ret = 0;
    if ((main_cmd >= DSMI_SERDES_CMD_MAX) || (sub_cmd >= DSMI_SERDES_PRBS_SUB_CMD_MAX) ||
        (sub_cmd == DSMI_SERDES_PRBS_SUB_CMD_SET_TYPE)) {
        DEV_MON_ERR("get serdes para error. (devid=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (buf == NULL || size == NULL) {
        DEV_MON_ERR("get serdes para error. (devid=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    DEV_MON_EVENT("get prbs result in dsmi_get_serdes_info. devid=[%d], main_cmd=[%d], sub_cmd=[%d]",
                  device_id, main_cmd, sub_cmd);
    ret = dsmi_cmd_get_serdes_info(device_id, main_cmd, sub_cmd, buf, size);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret,
            "get serdes info fail. (devid=%d; main_cmd=%d; sub_cmd=%d; ret=%d)\n",
            device_id, main_cmd, sub_cmd, ret);
    }

    return ret;
#else
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

DLLEXPORT int dsmi_set_serdes_info(unsigned int device_id, SERDES_MAIN_CMD main_cmd,
    unsigned int sub_cmd, void* buf, unsigned int size)
{
#if defined(CFG_FEATURE_SERDES_INFO) || defined(CFG_FEATURE_SERDES_INFO_A5)
    int ret = 0;
    if ((main_cmd >= DSMI_SERDES_CMD_MAX) || (sub_cmd >= DSMI_SERDES_PRBS_SUB_CMD_MAX) ||
        (sub_cmd != DSMI_SERDES_PRBS_SUB_CMD_SET_TYPE)) {
        DEV_MON_ERR("set serdes para error. (devid=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (buf == NULL || size == 0) {
        DEV_MON_ERR("set serdes para error. (devid=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    DEV_MON_EVENT("send prbs in dsmi_set_serdes_info. devid=[%d], main_cmd=[%d], sub_cmd=[%d]",
                  device_id, main_cmd, sub_cmd);
    ret = dsmi_cmd_set_serdes_info(device_id, main_cmd, sub_cmd, buf, size);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "set serdes info fail. (devid=%d; main_cmd=%d; sub_cmd=%d; ret=%d)\n",
            device_id, main_cmd, sub_cmd, ret);
    }

    return ret;
#else
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_cmd_set_serdes_info(unsigned int dev_id, SERDES_MAIN_CMD main_cmd, unsigned int sub_cmd,
    const void *buf, unsigned int buf_size)
{
#if defined(CFG_FEATURE_SERDES_INFO) || defined(CFG_FEATURE_SERDES_INFO_A5)
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SET_SERDES_INFO, dev_id, (sizeof(SERDES_MAIN_CMD) + sizeof(unsigned int) +
                     sizeof(unsigned int) + buf_size), 0)
    DM_COMMAND_ADD_REQ(&main_cmd, sizeof(SERDES_MAIN_CMD))
    DM_COMMAND_ADD_REQ(&sub_cmd, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&buf_size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, buf_size)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
#else
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int dsmi_cmd_get_serdes_info(unsigned int dev_id, SERDES_MAIN_CMD main_cmd, unsigned int sub_cmd,
    void *buf, unsigned int *size)
{
#if defined(CFG_FEATURE_SERDES_INFO) || defined(CFG_FEATURE_SERDES_INFO_A5)
    unsigned int out_length = 0;

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_SERDES_INFO, dev_id, (sizeof(SERDES_MAIN_CMD) + sizeof(unsigned int) +
                        sizeof(unsigned int) + *size), (sizeof(unsigned int) + (*size)))
    DM_COMMAND_ADD_REQ(&main_cmd, sizeof(SERDES_MAIN_CMD))
    DM_COMMAND_ADD_REQ(&sub_cmd, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, *size)
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(&out_length, sizeof(unsigned int))

    if (out_length <= *size) {
        *size = out_length;
    } else {
        DEV_MON_WARNING("serdes_info outbuflen not enough. (devid=%d; size_out=%u; real_out=%u)\n",
            dev_id, *size, out_length);
    }
    DM_COMMAND_PUSH_OUT(buf, (*size))
    DM_COMMAND_END()
#else
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

#if defined(CFG_FEATURE_INIT_MCU_BOARD_ID)
int dsmi_get_mcu_board_id(unsigned int device_id, unsigned int *mcu_board_id)
{
    DM_COMMAND_BIGIN(DEV_MOV_CMD_GET_MCU_BOARD_ID, device_id, sizeof(unsigned char), sizeof(unsigned int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(mcu_board_id, sizeof(unsigned int))
    DM_COMMAND_END()
}
#endif

int dsmi_product_get_mcu_board_id(unsigned int device_id, unsigned int *mcu_board_id)
{
#if defined(CFG_FEATURE_INIT_MCU_BOARD_ID)
    int ret;
    if (mcu_board_id == NULL || device_id >= DEVDRV_MAX_DAVINCI_NUM) {
        DEV_MON_ERR("The mcu_board_id is null or the device_id is too large. (devid=%u)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_get_mcu_board_id(device_id, mcu_board_id);
    if (ret != 0) {
        DEV_MON_ERR("Get mcu board id fail. (devid=%u; ret=%d)\n", device_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
#else
    (void)device_id;
    (void)mcu_board_id;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

#ifdef CFG_FEATURE_AICPU_CUSTOM_CERT
int dsmi_cmd_get_custom_cert_show_info(unsigned int device_id, const char *buf, unsigned int buf_size,
                                       void *show_info, unsigned int show_info_size)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_CUSTOM_OP_SECVERIFY_CERT, device_id,
        (buf_size + sizeof(unsigned int) + sizeof(unsigned int)), show_info_size)
    DM_COMMAND_ADD_REQ(&show_info_size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&buf_size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, buf_size)
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(show_info, show_info_size)
    DM_COMMAND_END()
}
#endif

DLLEXPORT int dsmi_get_custom_op_secverify_cert_show_info(unsigned int device_id, const char *buf, unsigned int buf_size,
                                                void *show_info, unsigned int show_info_size)
{
#ifdef CFG_FEATURE_AICPU_CUSTOM_CERT
    int ret;
    if (show_info == NULL || device_id >= DEVDRV_MAX_DAVINCI_NUM || buf == NULL) {
        DEV_MON_ERR("The buf is null or the device_id is too large. (devid=%u)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((buf_size <= 0 || buf_size > DSMI_CUSTOM_OP_SECVERIFY_CERT_CHAIN_SIZE_MAX) ||
        (show_info_size <= 0 || show_info_size > DSMI_CUSTOM_OP_SECVERIFY_CERT_SHOW_INFO_SIZE_MAX)) {
        DEV_MON_ERR("Invalid buf_size or show_info_size. (buf_size=%u; show_info_size=%u)\n", buf_size, show_info_size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_cmd_get_custom_cert_show_info(device_id, buf, buf_size, show_info, show_info_size);
    if (ret != 0) {
        DEV_MON_ERR("Get custom cert show info fail. (devid=%u; ret=%d)\n", device_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
#else
    (void)device_id;
    (void)buf;
    (void)buf_size;
    (void)show_info;
    (void)show_info_size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

struct bandwidth_t {
    unsigned int time_interval;
    unsigned int tx_bandwidth;
    unsigned int rx_bandwidth;
};

DLLEXPORT int dsmi_get_bandwidth(int logic_id, int port, struct bandwidth_t *bandwidth_info)
{
    (void)logic_id;
    (void)port;
    (void)bandwidth_info;
    return DRV_ERROR_NOT_SUPPORT;
}

DLLEXPORT int dsmi_get_ub_bandwidth(int device_id, int udie_id, int port_id,
                        unsigned int time_interval, struct ub_bandwidth_t *bw)
{
    (void)device_id;
    (void)udie_id;
    (void)port_id;
    (void)time_interval;
    (void)bw;
    return DRV_ERROR_NOT_SUPPORT;
}

DLLEXPORT int dsmi_get_netdev_mac_addr(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                            char *mac_addr, unsigned int mac_addr_len)
{
    (void)dev_id;
    (void)netdev_name;
    (void)netdev_name_len;
    (void)mac_addr;
    (void)mac_addr_len;
    return DRV_ERROR_NOT_SUPPORT;
}

DLLEXPORT int dsmi_set_netdev_mac_addr(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                            const char *mac_addr, unsigned int mac_addr_len)
{
    (void)dev_id;
    (void)netdev_name;
    (void)netdev_name_len;
    (void)mac_addr;
    (void)mac_addr_len;
    return DRV_ERROR_NOT_SUPPORT;
}

DLLEXPORT int dsmi_get_device_bond_ip_address(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                                    struct dsmi_ip_addr *ip_address, struct dsmi_ip_addr *mask_address)
{
    (void)dev_id;
    (void)netdev_name;
    (void)netdev_name_len;
    (void)ip_address;
    (void)mask_address;
    return DRV_ERROR_NOT_SUPPORT;
}

DLLEXPORT int dsmi_set_device_bond_ip_address(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                                    struct dsmi_ip_addr *ip_address, struct dsmi_ip_addr *mask_address)
{
    (void)dev_id;
    (void)netdev_name;
    (void)netdev_name_len;
    (void)ip_address;
    (void)mask_address;
    return DRV_ERROR_NOT_SUPPORT;
}

DLLEXPORT int dsmi_set_netdev_gateway(int logic_id, const char *netdev_name, unsigned int netdev_name_len,
                            struct dsmi_ip_addr *gateway)
{
    (void)logic_id;
    (void)netdev_name;
    (void)netdev_name_len;
    (void)gateway;
    return DRV_ERROR_NOT_SUPPORT;
}

DLLEXPORT int dsmi_get_netdev_gateway(int logic_id, const char *netdev_name, unsigned int netdev_name_len,
                            struct dsmi_ip_addr *gateway)
{
    (void)logic_id;
    (void)netdev_name;
    (void)netdev_name_len;
    (void)gateway;
    return DRV_ERROR_NOT_SUPPORT;
}

DLLEXPORT int dsmi_set_device_offline_nic_down_flag(int logic_id, int enable_flag)
{
    (void)logic_id;
    (void)enable_flag;
    return DRV_ERROR_NOT_SUPPORT;
}
// clang-format on