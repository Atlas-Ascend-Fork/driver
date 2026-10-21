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
#include <linux/limits.h>
#include "ascend_inpackage_hal.h"
#include "dm_hdc.h"
#include "dm_udp.h"
#include "dsmi_common_interface.h"
#include "dsmi_common.h"
#include "dev_mon_api.h"
#include "dms_user_interface.h"
#include "dsmi_dmp_command.h"
#include "hdc_user_interface.h"
#include "dsmi_upgrade_proc.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

int dsmi_update_no_response_data(int device_id, unsigned char control_cmd)
{
    int ret;
    ret = dsmi_cmd_upgrade_ctl_cmd(device_id, control_cmd);

    return ret;
}

// find component name from config file which type is component_type
STATIC int find_index_by_type(unsigned char component_type, CFG_FILE_DES *component_name, unsigned int *idx)
{
    unsigned int array_num;
    unsigned int i = 0;

    if (component_name == NULL || idx == NULL || component_type >= (unsigned int)DSMI_COMPONENT_TYPE_MAX) {
        dev_upgrade_err("find_index_by_type parameter error!\n");
        return DRV_ERROR_PARA_ERROR;
    }

    array_num = MAX_COMPONENT_NUM;
    for (i = 0; i < array_num; i++) {
        if (component_type == component_name[i].component_type) {
            *idx = i;
            return 0;
        }
    }

    return DRV_ERROR_NOT_EXIST;
}

int dsmi_update_send_file_name(int device_id, DSMI_COMPONENT_TYPE component_type, const char *file_name)
{
    unsigned int file_name_len;

    if (file_name == NULL) {
        dev_upgrade_err("devid %d dsmi_update_send_file_name param error\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    file_name_len = (unsigned int)strnlen(file_name, PATH_MAX);
    if (file_name_len >= PATH_MAX) {
        dev_upgrade_err("devid %d dsmi_update_send_file_name filename len error:%d, max=%d\n", device_id, file_name_len,
                        PATH_MAX);
        return DRV_ERROR_PARA_ERROR;
    }

    return dsmi_cmd_update_send_file_name(device_id, (unsigned char)component_type, file_name, (file_name_len + 1U));
}

int transmit_file_to_device(int device_id, const char *src_file, char *dst_file, DSMI_COMPONENT_TYPE component_type)
{
    int ret;

    DRV_CHECK_RETV((src_file != NULL), DRV_ERROR_PARA_ERROR);
    DRV_CHECK_RETV((dst_file != NULL), DRV_ERROR_PARA_ERROR);
    ret = get_pcie_mode();
    if (ret == PCIE_RC_MODE) {
        ret = local_copy_file(device_id, src_file, dst_file);
        if (ret != 0) {
            dev_upgrade_err("devid %d local copy file fail, transmit file  %s to %s ret = %d!\n", device_id, src_file,
                            dst_file, ret);
            return ret;
        }
    } else if (ret == PCIE_EP_MODE) {
        ret = drvHdcSendFileEx(0xFF, 0, device_id, src_file, dst_file, NULL);
        if (ret != 0) {
            dev_upgrade_err("copy file %s from host to device(0x%x) %s fail, ret = %x\n", src_file, device_id, dst_file,
                            ret);
            return ret;
        }
    } else {
        dev_upgrade_err("devid %d get pcie mode fail ,ret = 0x%x\n", device_id, ret);
        return ret;
    }
    dev_upgrade_info("transmit file %s to device %s success, device id = 0x%x\n", src_file, dst_file, device_id);

    // send file name to device
    ret = dsmi_update_send_file_name(device_id, component_type, (const char *)dst_file);
    if (ret != 0) {
        dev_upgrade_ex_notsupport_err(
            ret, "transmit_file_to_device  file name =%s, component_type = 0x%x to device 0x%x error %x!\n", dst_file,
            component_type, device_id, ret);
        return ret;
    }
    dev_upgrade_info("transmit file name %s  to device success, device id = 0x%x\n", dst_file, device_id);

    return 0;
}

// check component_type is support or not by device
int check_component_type_validity(DSMI_COMPONENT_TYPE *component_list, DSMI_COMPONENT_TYPE component_type,
                                  unsigned int component_num)
{
    unsigned int i = 0;

    DRV_CHECK_RETV((component_list != NULL), DRV_ERROR_PARA_ERROR);
    if (component_type >= DSMI_COMPONENT_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }
    for (i = 0; i < component_num; i++) {
        if (component_list[i] == component_type) {
            return 0;
        }
    }

    dev_upgrade_debug("Invalid Component type, all validity component type is as follow\n");

    for (i = 0; i < component_num; i++) {
        dev_upgrade_debug("validity component type = 0x%x\n", component_list[i]);
    }

    return DRV_ERROR_NOT_SUPPORT;
}

static int upgrade_flash_component_check(DSMI_COMPONENT_TYPE component_type)
{
    unsigned int i;
    /* The component is not written to the flash. */
    DSMI_COMPONENT_TYPE component_list[] = {
        DSMI_COMPONENT_TYPE_AICPU,      DSMI_COMPONENT_TYPE_RAWDATA,     DSMI_COMPONENT_TYPE_SYSDRV,
        DSMI_COMPONENT_TYPE_ADSAPP,     DSMI_COMPONENT_TYPE_COMISOLATOR, DSMI_COMPONENT_TYPE_CLUSTER,
        DSMI_COMPONENT_TYPE_CUSTOMIZED, DSMI_COMPONENT_TYPE_RECOVERY,
    };

    if (component_type >= DSMI_COMPONENT_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    for (i = 0; i < sizeof(component_list) / sizeof(DSMI_COMPONENT_TYPE); i++) {
        if (component_type == component_list[i]) {
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    return DRV_ERROR_NONE;
}

int upgrade_all_component(int device_id, const char *file_name, DSMI_COMPONENT_TYPE *component_list,
                          unsigned int component_num, DSMI_COMPONENT_TYPE component_type)
{
    int ret;
    unsigned int upgrade_success_num = 0;
    unsigned int i = 0;
    unsigned int idx = 0;
    CFG_FILE_DES *component_path_des = NULL;

    DRV_CHECK_RETV((file_name != NULL), DRV_ERROR_PARA_ERROR)
    DRV_CHECK_RETV((component_list != NULL), DRV_ERROR_PARA_ERROR)
    component_path_des = malloc((sizeof(CFG_FILE_DES) * MAX_COMPONENT_NUM));
    DRV_CHECK_RETV((component_path_des != NULL), DRV_ERROR_MALLOC_FAIL)
    ret = memset_s((unsigned char *)component_path_des, (sizeof(CFG_FILE_DES) * MAX_COMPONENT_NUM), 0,
                   (sizeof(CFG_FILE_DES) * MAX_COMPONENT_NUM));
    if (ret != 0) {
        dev_upgrade_err("devid %d memset fail, ret = %d\n", device_id, ret);
        goto out;
    }

    for (i = 0; i < MAX_COMPONENT_NUM; i++) {
        component_path_des[i].component_type = DSMI_COMPONENT_TYPE_MAX;
    }

    ret = parse_cfg_file(device_id, component_path_des, file_name, component_type);
    if (ret != 0) {
        dev_upgrade_ex_notsupport_err(ret, "device 0x%x  parse cfg file %s fail\n", device_id, file_name);
        goto out;
    }

    // first send all file to device then send cmd START_UPDATE(0x1)
    for (i = 0; i < component_num; i++) {
        if (upgrade_flash_component_check(component_list[i]) == (int)DRV_ERROR_NOT_SUPPORT) {
            continue;
        }

        ret = find_index_by_type(component_list[i], component_path_des, &idx);
        if (ret != 0) {
#ifndef CFG_FEATURE_FIRMWARE_TYPE_STRONG_MATCH
            if (ret == (int)DRV_ERROR_NOT_EXIST) {
                ret = (int)DRV_ERROR_NONE;
                continue;
            }
#endif
            dev_upgrade_err("Can not find supported type in cfg file. (type=0x%x; device_id=%d; cfg_file=%s; ret=%d)\n",
                            component_list[i], device_id, file_name, ret);
            goto out;
        }

        ret = transmit_file_to_device(device_id, component_path_des[idx].src_component_path,
                                      component_path_des[idx].dst_compoent_path, component_list[i]);
        if (ret != 0) {
            dev_upgrade_err("update device %d when transmit file %s to device %s fail, ret = %x\n", device_id,
                            component_path_des[idx].src_component_path, component_path_des[idx].dst_compoent_path, ret);
            goto out;
        }

        upgrade_success_num++;
    }

    if (upgrade_success_num == 0) {
        dev_upgrade_err("Upgrade component number is 0. (device_id=%d)\n", device_id);
        ret = DRV_ERROR_PARA_ERROR;
    }

out:
    DSMI_FREE(component_path_des);
    return ret;
}

int upgrade_single_component(int device_id, const char *file_name, DSMI_COMPONENT_TYPE *component_list,
                             unsigned int component_num, DSMI_COMPONENT_TYPE component_type)
{
    int ret;
    char *dst_compoent_name = NULL;

    // check component_type is support or not
    ret = check_component_type_validity(component_list, component_type, component_num);
    if (ret != 0) {
        dev_upgrade_debug(
            "device does not support upgrade component type. (device=0x%x; component_type=0x%x; ret=0x%x)\n", device_id,
            component_type, ret);
        return ret;
    }

    dst_compoent_name = (char *)calloc(PATH_MAX, sizeof(char));
    if (dst_compoent_name == NULL) {
        dev_upgrade_err("dst_component_name calloc fail\n");
        return DRV_ERROR_MALLOC_FAIL;
    }

    // path unity change to UPGRADE_DST_PATH
    ret = find_file_name(device_id, file_name, dst_compoent_name);
    if (ret != 0) {
        dev_upgrade_err("devid %d find file fail, file_name: %s fail, ret = 0x%x\n", device_id, file_name, ret);
        goto exit;
    }

    ret = transmit_file_to_device(device_id, file_name, dst_compoent_name, component_type);
    if (ret != 0) {
        dev_upgrade_err("update device %d when transmit file %s to device %s fail, ret = 0x%x\n", device_id, file_name,
                        dst_compoent_name, ret);
    }

exit:
    DSMI_FREE(dst_compoent_name);
    return ret;
}

int check_upgrade_component_type_and_state(int device_id, unsigned char *upgrade_schedule,
                                           unsigned char *upgrade_status, DSMI_COMPONENT_TYPE component_type)
{
    int ret;
    if (upgrade_schedule == NULL || upgrade_status == NULL) {
        dev_upgrade_err("devid %d dsmi_upgrade_get_state parameter error!\n", device_id);
        return (int)DRV_ERROR_PARA_ERROR;
    }
    ret = dsmi_upgrade_get_state(device_id, upgrade_schedule, upgrade_status);
    if (ret != 0) {
        dev_upgrade_err("devid %d dsmi_upgrade_get_state return fail ret = %d \n", device_id, ret);
        return ret;
    }
    if ((*upgrade_status) == (unsigned char)IS_UPGRADING) {
        return (int)DRV_ERROR_NOT_SUPPORT;
    }

    if ((*upgrade_status) == (unsigned char)UPGRADE_NOT_SUPPORT) {
        return (int)DRV_ERROR_NOT_SUPPORT;
    }

    if ((*upgrade_status) == (unsigned char)UPGRADE_SYNCHRONIZING) {
        dev_upgrade_err("devid %d device return upgrade status (%d):is synchronizing\n", device_id, (*upgrade_status));
        return (int)DEVICE_RETURN_UPGRADE_STATUS_IS_SYNCHRONIZING;
    }

    if (component_type == DSMI_COMPONENT_TYPE_AICPU) {
        return (int)DRV_ERROR_NOT_SUPPORT;
    }

#if (((!defined CFG_SOC_PLATFORM_MINI) && (!defined CFG_SOC_PLATFORM_DC_V51)) || (defined CFG_SOC_PLATFORM_RC))
    if (component_type == UPGRADE_AND_RESET_ALL_COMPONENT) {
        dev_upgrade_err("devid %d component_type[%d] parameter error!\n", device_id, component_type);
        return (int)DRV_ERROR_PARA_ERROR;
    }
#endif

    return 0;
}

#define DSMI_SEND_PATCH_RETRY_TIMES 30
static int send_patch_to_device(int device_id, const char *file_name, char *dst_compoent_name)
{
    unsigned int retry = 0;
    int ret;

    // path unity change to UPGRADE_DST_PATH
    ret = find_file_name(device_id, file_name, dst_compoent_name);
    if (ret != 0) {
        dev_upgrade_err("Find file name fail. (dev_id=%d; ret=%x; file_name=%s)\n", device_id, ret, file_name);
        return ret;
    }

    while (retry < DSMI_SEND_PATCH_RETRY_TIMES) {
        ret = drvHdcSendFileEx(0xFF, 0, device_id, file_name, dst_compoent_name, NULL);
        if ((ret != DRV_ERROR_REMOTE_NOT_LISTEN)) {
            break;
        }
        retry++;
        (void)sleep(1); /* 1s */
    }

    if (ret != 0) {
        dev_upgrade_err("Copy file to device failed. (devid=%d; ret=%x; src=%s; dst=%s)\n", device_id, ret, file_name,
                        dst_compoent_name);
        return ret;
    }
    return 0;
}

int upgrade_trans_patch(int device_id, const char *file_name)
{
    int ret;
    char *dst_compoent_name = NULL;
    dst_compoent_name = (char *)calloc(PATH_MAX, sizeof(char));
    if (dst_compoent_name == NULL) {
        dev_upgrade_err("dst_component_name calloc fail\n");
        return DRV_ERROR_MALLOC_FAIL;
    }

    // path unity change to UPGRADE_DST_PATH
    ret = send_patch_to_device(device_id, file_name, dst_compoent_name);
    if (ret != 0) {
        goto exit;
    }

    ret = dsmi_cmd_load_patch(device_id, dst_compoent_name, (unsigned int)(strnlen(dst_compoent_name, PATH_MAX) + 1UL));
    if (ret != 0) {
        dev_upgrade_ex_notsupport_err(ret, "Send start update cmd failed. (devid=%d; ret=0x%x)\n", device_id, ret);
    }

exit:
    DSMI_FREE(dst_compoent_name);
    return ret;
}

int upgrade_trans_mami_patch(int device_id, const char *file_name)
{
    int ret;
    char *dst_compoent_name = NULL;

    if (!file_name) {
        dev_upgrade_err("file name is null\n");
        return DRV_ERROR_PARA_ERROR;
    }

    dst_compoent_name = (char *)calloc(PATH_MAX, sizeof(char));
    if (!dst_compoent_name) {
        dev_upgrade_err("dst_component_name calloc fail\n");
        return DRV_ERROR_MALLOC_FAIL;
    }

    if (strcmp(file_name, "NULL") != 0) {
        dev_upgrade_info("Send alone mami patch. (device_id=%d)\n", device_id);
        ret = send_patch_to_device(device_id, file_name, dst_compoent_name);
        if (ret != 0) {
            goto exit;
        }

        ret = dsmi_cmd_load_mami_patch(device_id, 1, dst_compoent_name,
                                       (unsigned int)(strnlen(dst_compoent_name, PATH_MAX) + 1UL));
        if (ret != 0) {
            dev_upgrade_ex_notsupport_err(ret, "Load mami patch failed. (devid=%d; ret=0x%x)\n", device_id, ret);
            goto exit;
        }
    } else {
        dev_upgrade_info("Load default mami patch. (device_id=%d)\n", device_id);
        ret = dsmi_cmd_load_mami_patch(device_id, 0, dst_compoent_name,
                                       (unsigned int)(strnlen(dst_compoent_name, PATH_MAX) + 1UL));
        if (ret != 0) {
            dev_upgrade_ex_notsupport_err(ret, "Load mami patch failed. (devid=%d; ret=0x%x)\n", device_id, ret);
            goto exit;
        }
    }

exit:
    DSMI_FREE(dst_compoent_name);
    return ret;
}