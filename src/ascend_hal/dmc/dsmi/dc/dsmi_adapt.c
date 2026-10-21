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
#include <unistd.h>
#include <sys/types.h>

#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "drv_type.h"
#include "dsmi_common.h"
#include "dms_cmd_def.h"
#include "dms_fault.h"
#include "dsmi_common_interface.h"
#include "dsmi_dmp_command.h"
#include "dsmi_upgrade_proc.h"
#include "dms_user_interface.h"
#include "hdc_user_interface.h"
#include "dsmi_product.h"
#include "dsmi_adapt.h"

#if defined(CFG_SOC_PLATFORM_MINI)
#define UPGRADE_DST_PATH "/mnt/"
#elif defined(CFG_FEATURE_HOST_DEVICE_UPGRADE)
#define UPGRADE_DST_PATH "/home/HwDmUser/hdcd/device"
#else
/* Actually path is /home/drv/upgrade/firmware/device[0-9]/ */
#define UPGRADE_DST_PATH "/home/drv/upgrade/firmware/device"
#endif

int _dsmi_get_fault_inject_info(unsigned int device_id, unsigned int max_info_cnt, DSMI_FAULT_INJECT_INFO *info_buf,
                                unsigned int *real_info_cnt)
{
    (void)device_id;
    (void)max_info_cnt;
    (void)info_buf;
    (void)real_info_cnt;
    return DRV_ERROR_NOT_SUPPORT;
}

int find_file_name(int device_id, const char *path_name, char *dst_name)
{
    const char *name_start = NULL;
    int sep = '/';
    int ret;
    char *base_path = NULL;

    if ((path_name == NULL) || (dst_name == NULL)) {
        DEV_MON_ERR("find_file_name param fail \n");
        return DRV_ERROR_PARA_ERROR;
    }

    base_path = (char *)calloc(PATH_MAX, sizeof(char));
    if (base_path == NULL) {
        dev_upgrade_err("base_path calloc fail\n");
        return DRV_ERROR_MALLOC_FAIL;
    }

    name_start = strrchr(path_name, sep);
    name_start = (name_start == NULL) ? path_name : (name_start + 1);

    ret = memset_s(dst_name, PATH_MAX, '\0', PATH_MAX);
    DRV_CHECK_RETV_DO_SOMETHING(ret == 0, DSMI_SAFE_FUN_FAIL, DEV_MON_ERR("DSMI call safe fun fail, ret = %d\n", ret);
                                free(base_path); base_path = NULL);

    ret = get_pcie_mode();
    if (ret == PCIE_RC_MODE) {
#if defined(CFG_SOC_PLATFORM_MINI)
        ret = sprintf_s(dst_name, PATH_MAX, UPGRADE_DST_PATH "%s", name_start);
#elif defined(CFG_FEATURE_HOST_DEVICE_UPGRADE)
        ret = sprintf_s(dst_name, PATH_MAX, UPGRADE_DST_PATH "%d/upgrade/%s", device_id, name_start);
#else
        ret = sprintf_s(dst_name, PATH_MAX, UPGRADE_DST_PATH "%d/%s", device_id, name_start);
#endif
        DRV_CHECK_RETV_DO_SOMETHING(
            ret >= 0, DSMI_SAFE_FUN_FAIL, DEV_MON_ERR("DSMI call safe fun fail, ret = %d\n", ret); free(base_path);
            base_path = NULL);
    } else if (ret == PCIE_EP_MODE) {
        ret = drvHdcGetTrustedBasePathEx(0xFF, 0, device_id, base_path, PATH_MAX);
        if (ret != DRV_ERROR_NONE) {
            DEV_MON_ERR("drvHdcGetTrustedBasePathEx error, hdcError_t: %d.", ret);
            DSMI_FREE(base_path);
            return ret;
        }
        ret = sprintf_s(dst_name, PATH_MAX, "%s%s%s", base_path, UPGRADE_DIR_NAME, name_start);
        if (ret < 0) {
            DEV_MON_ERR("sprintf_s failed.\n");
            DSMI_FREE(base_path);
            return DSMI_SAFE_FUN_FAIL;
        }
    } else {
        DEV_MON_ERR("pcie mode invalid, ret: %d\n", ret);
        DSMI_FREE(base_path);
        return DRV_ERROR_INNER_ERR;
    }
    if ((strlen(dst_name) + 1) > PATH_MAX) {
        DEV_MON_ERR("strlen(dst_name): %u is invalid\n", (strlen(dst_name) + 1));
        DSMI_FREE(base_path);
        return DRV_ERROR_PARA_ERROR;
    }

    DSMI_FREE(base_path);
    return 0;
}

int parse_cfg_file(int devices_id, CFG_FILE_DES *component_des, const char *file_name,
                   DSMI_COMPONENT_TYPE component_type)
{
    unsigned int i;
    FILE *fd = NULL;
    char *str = NULL;
    char buf[MAX_LINE_LEN + 1] = {0};
    char *tmp_path = NULL;
    char *value = NULL;
    long int tmp_type = 0;
    int ret = 0;
    unsigned int file_name_len;

    DRV_CHECK_RETV_DO_SOMETHING((file_name != NULL), DRV_ERROR_PARA_ERROR,
                                dev_upgrade_err("get component des param is null\n"));
    DRV_CHECK_RETV_DO_SOMETHING((component_des != NULL), DRV_ERROR_PARA_ERROR,
                                dev_upgrade_err("get component des param is null\n"));

    file_name_len = (unsigned int)strlen(file_name);
    DRV_CHECK_RETV_DO_SOMETHING((file_name_len < PATH_MAX), DRV_ERROR_PARA_ERROR,
                                dev_upgrade_err("get component des param is null\n"));

    tmp_path = (char *)calloc(PATH_MAX, sizeof(char));
    DRV_CHECK_RETV_DO_SOMETHING((tmp_path != NULL), DRV_ERROR_MALLOC_FAIL, dev_upgrade_err("tmp_path calloc fail\n"));

    value = (char *)calloc(PATH_MAX, sizeof(char));
    DRV_CHECK_RETV_DO_SOMETHING((value != NULL), DRV_ERROR_MALLOC_FAIL, dev_upgrade_err("value calloc fail\n");
                                DSMI_FREE(tmp_path));

    dev_mon_fopen(&fd, file_name, "r");
    if (fd == NULL) {
        dev_upgrade_err("open config file fail %s\n", file_name);
        DSMI_FREE(tmp_path);
        DSMI_FREE(value);
        return DRV_ERROR_FILE_OPS;
    }

    str = buf;
    // i have become MAX_COMPONENT_NUM because in
    // to initialize component_des[i].component_type
    i = 0;
    while ((str = fgets(str, MAX_LINE_LEN, fd)) != NULL) {
        if (!split_by_char(buf, (char *)tmp_path, PATH_MAX, (char *)value, PATH_MAX, ':')) {
            DRV_CHECK_DO_SOMETHING(i < MAX_COMPONENT_NUM, ret = DRV_ERROR_NOT_SUPPORT;
                                   dev_upgrade_ex_notsupport_err(ret, "i:%d >= MAX_COMPONENT_NUM\n", i); goto out);

            (void)component_type;
            ret = strncpy_s(component_des[i].src_component_path, PATH_MAX, tmp_path, strnlen(tmp_path, PATH_MAX - 1));
            if (ret != 0) {
                dev_upgrade_err("strncpy_s error ret = %d\n", ret);
                goto out;
            }

            component_des[i].src_component_path[strnlen(tmp_path, PATH_MAX - 1)] = '\0';
            tmp_type = strtol(value, NULL, 0);
            if (tmp_type < 0 || tmp_type >= (long)DSMI_COMPONENT_TYPE_MAX) {
                ret = DRV_ERROR_NOT_SUPPORT;
                dev_upgrade_ex_notsupport_err(ret, "unexpected component type in upgrade.cfg, type= %d\n", tmp_type);
                goto out;
            }

            component_des[i].component_type = (unsigned char)tmp_type;
            ret = find_file_name(devices_id, component_des[i].src_component_path,
                                 (char *)component_des[i].dst_compoent_path);
            if (ret != 0) {
                dev_upgrade_err("dst path len is too long, cfg path is %s, dst path is %s ret %d\n",
                                component_des[i].src_component_path, component_des[i].dst_compoent_path, ret);
                goto out;
            }
            i++;
        }
    }

out:
    (void)fclose(fd);
    fd = NULL;
    DSMI_FREE(tmp_path);
    DSMI_FREE(value);

    return ret;
}

int drv_get_phy_mach_flag(int device_id)
{
    unsigned int host_flag;
    int ret;

#ifdef CFG_EDGE_HOST
    ret = drvGetHostPhyMachFlag((unsigned int)device_id, &host_flag);
    if (ret != 0) {
        dev_upgrade_err("devid %d drvGetHostPhyMachFlag return value is error 0x%x\n", device_id, ret);
        return ret;
    }
#else
    ret = devdrv_get_host_phy_mach_flag(device_id, &host_flag);
    if (ret != 0) {
        dev_upgrade_err("devid %d devdrv_get_host_phy_mach_flag return value is error 0x%x\n", device_id, ret);
        return ret;
    }
#endif

    if (host_flag != DEVDRV_HOST_PHY_MACH_FLAG) {
        dev_upgrade_err("devid %d not a phy mach! host_flag = 0x%x\n", device_id, host_flag);
        return DRV_ERROR_OPER_NOT_PERMITTED;
    }

    return 0;
}

int check_component_type(DSMI_COMPONENT_TYPE component_type, int device_id)
{
    DSMI_COMPONENT_TYPE component_list[MAX_COMPONENT_NUM] = {DSMI_COMPONENT_TYPE_MAX};
    unsigned int component_num = 0;
    int ret;

    ret = dsmi_get_component_count(device_id, &component_num);
    if (ret || (component_num > MAX_COMPONENT_NUM) || (component_num == 0)) {
        dev_upgrade_err("get device %d component count fail ret = %d, component_num = %u!\n", device_id, ret,
                        component_num);
        return ret;
    }

    ret = dsmi_get_component_list(device_id, component_list, component_num);
    if (ret != 0) {
        dev_upgrade_err("devid %d get component list error, ret  = 0x%x\n", device_id, ret);
        return ret;
    }
    ret = check_component_type_validity(component_list, component_type, component_num);
    if (ret != 0) {
        dev_upgrade_debug("Component type is not valid. (dev_id=%d, ret=0x%x)\n", device_id, ret);
        return ret;
    }
    return ret;
}

int check_dst_file_path(int device_id, const char *src_path)
{
    int ret;
    char *dst_path = NULL;

    dst_path = (char *)calloc(PATH_MAX, sizeof(char));
    if (dst_path == NULL) {
        dev_upgrade_err("Call calloc fail. (dev_id=%d)\n", device_id);
        return DRV_ERROR_MALLOC_FAIL;
    }
#if defined(CFG_SOC_PLATFORM_MINI)
    ret = sprintf_s(dst_path, PATH_MAX, UPGRADE_DST_PATH);
#elif defined(CFG_FEATURE_HOST_DEVICE_UPGRADE)
    ret = sprintf_s(dst_path, PATH_MAX, UPGRADE_DST_PATH "%d/upgrade/", device_id);
#else
    ret = sprintf_s(dst_path, PATH_MAX, UPGRADE_DST_PATH "%d/", device_id);
#endif
    DRV_CHECK_RETV_DO_SOMETHING(ret >= 0, DSMI_SAFE_FUN_FAIL, DEV_MON_ERR("DSMI call safe fun fail. (ret=%d)\n", ret);
                                DSMI_FREE(dst_path));
    if (access(dst_path, F_OK) != 0) {
        ret = mkdir(dst_path, S_IRWXU | S_IRGRP | S_IXGRP);
        DRV_CHECK_RETV_DO_SOMETHING(
            ret >= 0, DRV_ERROR_FILE_OPS,
            DEV_MON_ERR("Call mkdir failed. (dev_id=%d; dst_path=%s; errno=%d)\n", device_id, dst_path, errno);
            DSMI_FREE(dst_path));
    }

    ret = strncmp(src_path, dst_path, strlen(dst_path));
    if (ret != 0) {
        DEV_MON_ERR("The file path is not in the expected path. (dev_id=%d; src_path=%s; dst_path=%s)\n", device_id,
                    src_path, dst_path);
        DSMI_FREE(dst_path);
        return DRV_ERROR_PARA_ERROR;
    }
    DSMI_FREE(dst_path);

    return 0;
}

int _dsmi_check_partitions(const char *config_xml_path)
{
    (void)config_xml_path;
    return DRV_ERROR_NOT_SUPPORT;
}

int _dsmi_get_device_utilization_rate(int device_id, int device_type, unsigned int *putilization_rate)
{
    return dsmi_get_davinchi_info(device_id, device_type, REQ_D_INFO_INFO_TYPE_RATE, putilization_rate);
}

int _dsmi_get_ecc_enable(int device_id, DSMI_DEVICE_TYPE device_type, int *enable_flag)
{
    return dsmi_get_enable(device_id, ECC_CONFIG_ITEM, device_type, enable_flag);
}

int _dsmi_get_flash_content(int device_id, DSMI_FLASH_CONTENT content_info)
{
    (void)device_id;
    (void)content_info;
    return DRV_ERROR_NOT_SUPPORT;
}

int _dsmi_set_flash_content(int device_id, DSMI_FLASH_CONTENT content_info)
{
    (void)device_id;
    (void)content_info;
    return DRV_ERROR_NOT_SUPPORT;
}

int dsmi_upgrade_cmd_send(int device_id, DSMI_COMPONENT_TYPE component_type, const char *file_name)
{
    int ret;
    DSMI_COMPONENT_TYPE component_list[MAX_COMPONENT_NUM] = {DSMI_COMPONENT_TYPE_MAX};
    unsigned int component_num = 0;
    unsigned char upgrade_status = 0;
    unsigned char upgrade_schedule = 0;
    unsigned char control_cmd;

    ret = check_upgrade_component_type_and_state(device_id, &upgrade_schedule, &upgrade_status, component_type);
    if (ret != 0) {
        dev_upgrade_ex_notsupport_err(ret,
                                      "check upgrade state fail, current status does not support upgrade. "
                                      "(device=0x%x; ret=%d; upgrade_status=%u)\n",
                                      device_id, ret, upgrade_status);
        return ret;
    }

    // 1 prepare upgrade, device clear component list saved by previous upgrade process
    ret = dsmi_update_no_response_data(device_id, UPGRADE_PREPARE);
    if (ret != 0) {
        dev_upgrade_ex_notsupport_err(ret, "send prepare upgrade info failed. (dev_id=%d; ret=%d)!\n", device_id, ret);
        return ret;
    }

    // 2 get type  from device
    ret = dsmi_get_component_count(device_id, &component_num);
    if (ret || (component_num > MAX_COMPONENT_NUM) || (component_num == 0)) {
        dev_upgrade_ex_notsupport_err(ret, "get device %d component list fail ret = %d, component_num = %u!\n",
                                      device_id, ret, component_num);
        return ret;
    }

    ret = dsmi_get_component_list(device_id, component_list, component_num);
    if (ret != 0) {
        dev_upgrade_ex_notsupport_err(ret, "get device %d component list fail ret = %d!\n", device_id, ret);
        return ret;
    }

    // 3 transmit file to device
    if ((component_type != UPGRADE_ALL_COMPONENT) && (component_type != UPGRADE_AND_RESET_ALL_COMPONENT)) {
        // check component_type is support or not
        ret = upgrade_single_component(device_id, file_name, component_list, component_num, component_type);
        if (ret) {
            dev_upgrade_ex_notsupport_err(ret, "update device %d of  component %s fail ret = %d\n", device_id,
                                          file_name, ret);
            return ret;
        }
    } else {
        ret = upgrade_all_component(device_id, file_name, component_list, component_num, component_type);
        if (ret) {
            dev_upgrade_ex_notsupport_err(ret, "update device %d of all component fail ret = %d\n", device_id, ret);
            return DRV_ERROR_INNER_ERR;
        }
    }

    // send start update cmd (0x4) to device
    control_cmd = (component_type != UPGRADE_AND_RESET_ALL_COMPONENT) ? START_UPDATE : START_UPDATE_AND_RESET;
    ret = dsmi_update_no_response_data(device_id, control_cmd);
    if (ret != 0) {
        dev_upgrade_ex_notsupport_err(ret, "update device %d when send start update cmd (0x4) fail ret = %d!\n",
                                      device_id, ret);
        return ret;
    }

    return ret;
}

int _dsmi_get_last_bootstate(int device_id, BOOT_TYPE boot_type, unsigned int *state)
{
    (void)device_id;
    (void)boot_type;
    (void)state;
    return DRV_ERROR_NOT_SUPPORT;
}

int _dsmi_get_centre_notify_info(int device_id, int index, int *value)
{
    (void)device_id;
    (void)index;
    (void)value;
    return DRV_ERROR_NOT_SUPPORT;
}

int _dsmi_set_centre_notify_info(int device_id, int index, int value)
{
    (void)device_id;
    (void)index;
    (void)value;
    return DRV_ERROR_NOT_SUPPORT;
}

int _dsmi_ctrl_device_node(int device_id, struct dsmi_dtm_node_s dtm_node, DSMI_DTM_OPCODE opcode, IN_OUT_BUF buf)
{
    (void)device_id;
    (void)dtm_node;
    (void)opcode;
    (void)buf;
    return DRV_ERROR_NOT_SUPPORT;
}

int _dsmi_get_all_device_node(int device_id, DEV_DTM_CAP capability, struct dsmi_dtm_node_s node_info[],
                              unsigned int *size)
{
    (void)device_id;
    (void)capability;
    (void)node_info;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
}

int _dsmi_fault_inject(DSMI_FAULT_INJECT_INFO fault_inject_info)
{
    (void)fault_inject_info;
    return DRV_ERROR_NOT_SUPPORT;
}

int _dsmi_get_device_state(int device_id, DSMI_DEV_NODE_STATE *node_state, unsigned int max_num, unsigned int *num)
{
    (void)device_id;
    (void)node_state;
    (void)max_num;
    (void)num;
    return DRV_ERROR_NOT_SUPPORT;
}

int _dsmi_set_bist_info(int device_id, DSMI_BIST_CMD cmd, const void *buf, unsigned int buf_size)
{
    DEV_MON_EVENT("set bist info, (user id=%u; device_id=%d).\n", getuid(), device_id);
#ifdef CFG_FEATURE_BIST
    if ((buf == NULL) || (cmd >= DSMI_BIST_CMD_MAX)) {
        DEV_MON_ERR("device_id %d parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    return dsmi_cmd_set_bist_info(device_id, cmd, buf, buf_size);
#else
    (void)device_id;
    (void)cmd;
    (void)buf;
    (void)buf_size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int _dsmi_get_bist_info(int device_id, DSMI_BIST_CMD cmd, void *buf, unsigned int *size)
{
#ifdef CFG_FEATURE_BIST
    if ((buf == NULL) || (size == NULL) || (cmd >= DSMI_BIST_CMD_MAX)) {
        DEV_MON_ERR("device_id %d parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    return dsmi_cmd_get_bist_info(device_id, cmd, buf, size);
#else
    (void)device_id;
    (void)cmd;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int _dsmi_set_upgrade_attr(int device_id, DSMI_COMPONENT_TYPE component_type, DSMI_UPGRADE_ATTR attr)
{
    (void)device_id;
    (void)component_type;
    (void)attr;
    return DRV_ERROR_NOT_SUPPORT;
}

int _dsmi_set_power_state(int device_id, DSMI_POWER_STATE type)
{
    DEV_MON_CRIT_EVENT("dsmi set power state exec, (user=%u; devid=%d; type=%u).\n", getuid(), device_id, type);
    (void)device_id;
    (void)type;
    return DRV_ERROR_NOT_SUPPORT;
}

int _dsmi_get_ufs_status(int device_id, struct dsmi_ufs_status_stru *ufs_status_data)
{
    if (ufs_status_data == NULL) {
        DEV_MON_ERR("devid %d dsmi_get_ufsstatus parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    return dsmi_cmd_get_ufs_status(device_id, ufs_status_data);
}

int _dsmi_set_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                          unsigned int buf_size)
{
    int ret;
    DEV_MON_EVENT("set device info, (user id=%u; device_id=0x%x; main cmd=%u; sub cmd=%u\n", getuid(), device_id,
                  (unsigned int)main_cmd, sub_cmd);

    ret = dsmi_check_device_id((int)device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (device_id != DSMI_SET_ALL_DEVICE && (ret != 0)) {
        DEV_MON_ERR("Dsmi check device id fail. (device_id=%d)\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = dsmi_cmd_set_device_info_method(device_id, main_cmd, sub_cmd, buf, buf_size);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Failed to set dev info. (dev_id=%u; main_cmd=%u; sub_main=%u; ret=%d)\n",
                                  device_id, main_cmd, sub_cmd, ret);
    }

    return ret;
}

int _dsmi_get_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                          unsigned int *size)
{
    int ret = 0;
    DEV_INFO_MAIN_CMD_TYPE cmd_type;

#ifndef CFG_FEATURE_PCIE_LINK_ERROR_INFO
    if ((main_cmd == DSMI_MAIN_CMD_PCIE) && (sub_cmd == DSMI_PCIE_SUB_CMD_PCIE_LINK_ERROR_INFO)) {
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif

#ifndef CFG_FEATURE_UDIS_UTIL_FROM_TS
    if ((main_cmd == DSMI_MAIN_CMD_TS) && (sub_cmd == DSMI_TS_SUB_CMD_NPU_MULTI_UTILIZATION_RATE_V2)) {
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif

    cmd_type = dsmi_get_dev_info_main_cmd_type(main_cmd, sub_cmd);
    switch (cmd_type) {
        case MAIN_CMD_TYPE_PRODUCT:
            ret = dsmi_product_get_device_info(device_id, main_cmd, sub_cmd, buf, size);
            break;
        case MAIN_CMD_TYPE_HOST_DMP:
            if (main_cmd == DSMI_MAIN_CMD_SEC) {
                ret = dsmi_cmd_get_sec_info(device_id, main_cmd, sub_cmd, buf, size);
            } else {
                if (dsmi_get_device_info_is_critical(main_cmd, sub_cmd) == true) {
                    ret = dsmi_cmd_get_device_info_critical(device_id, main_cmd, sub_cmd, buf, size);
                } else if (main_cmd == DSMI_MAIN_CMD_FLASH) {
                    ret = dsmi_cmd_get_flash_info(device_id, main_cmd, sub_cmd, buf, size);
                } else {
                    ret = dsmi_cmd_get_device_info(device_id, main_cmd, sub_cmd, buf, size);
                }
            }
            break;
        case MAIN_CMD_TYPE_HOST_DEVMNG:
            ret = drvQueryDeviceInfo(device_id, main_cmd, sub_cmd, buf, size);
            break;
        case MAIN_CMD_TYPE_NONE:
        default:
            ret = DRV_ERROR_PARA_ERROR;
            break;
    }

    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d cmd_type %d get device info fail, main_cmd=%d, sub_cmd=%d, ret=%d.\n",
                                  device_id, cmd_type, main_cmd, sub_cmd, ret);
    }

    return ret;
}

int dsmi_get_device_die_v2(struct dsmi_device_info device_info, struct dsmi_soc_die_stru *pdevice_die)
{
    (void)device_info;
    (void)pdevice_die;
    return DRV_ERROR_NOT_SUPPORT;
}

int dsmi_get_ecc_info_v2(struct dsmi_device_info device_info, int device_type,
                         struct dsmi_ecc_info_stru *pdevice_ecc_info)
{
    (void)device_info;
    (void)device_type;
    (void)pdevice_ecc_info;
    return DRV_ERROR_NOT_SUPPORT;
}

int dsmi_get_device_frequency_v2(struct dsmi_device_info device_info, int device_type, unsigned int *pfrequency)
{
    (void)device_info;
    (void)device_type;
    (void)pfrequency;
    return DRV_ERROR_NOT_SUPPORT;
}