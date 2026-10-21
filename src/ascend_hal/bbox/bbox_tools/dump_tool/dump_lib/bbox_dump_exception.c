/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "bbox_dump_exception.h"
#include "ascend_hal.h"
#include "dsmi_common_interface.h"
#include "securec.h"
#include "bbox_int.h"
#include "bbox_log_common.h"
#include "bbox_print.h"
#include "bbox_tool_fs.h"

#define KDUMP_MAGIC_NUM 0xf0f0f0f0
#define KDUMP_FLAG_NUM 3

/**
 * @brief       get status of the devices which is running
 * @param [out] status_list:     devices status list
 * @param [in]  list_size:       devices status list size
 * @return      NA
 */
STATIC void bbox_online_device_state(struct bbox_devices_info *info)
{
    drvStatus_t device_status = DRV_STATUS_INITING;
    u32 boot_status = DSMI_BOOT_STATUS_UNINIT, phy_id = 0, i;
    char dev_buf[DEV_BUF_MAX_LEN];

    drvError_t err = drvGetDevNum(&info->dev_num);
    if (err != DRV_ERROR_NONE) {
        BBOX_ERR_CTRL(BBOX_WAR, return, "get dev number unsuccessfully(%d).", (int)err);
    }

    for (i = 0; i < info->dev_num && i < MAX_PHY_DEV_NUM; i++) {
        err = drvDeviceGetPhyIdByIndex(i, &phy_id);
        if (err != DRV_ERROR_NONE) {
            BBOX_ERR("get phy_id by index[%u] failed(%d).", i, (int)err);
            continue;
        }

        if (phy_id >= MAX_PHY_DEV_NUM) {
            continue;
        }

        info->dev_id[phy_id] = i;
        if (info->status[phy_id] != SOC_STATUS_UNKNOWN) {
            continue;
        }

        err = drvGetDeviceBootStatus((s32)phy_id, &boot_status);
        if (err != DRV_ERROR_NONE) {
            BBOX_ERR("[%s] get boot status failed with %d.", bbox_get_dev_str(phy_id, i, dev_buf, sizeof(dev_buf)),
                     (int)err);
            continue;
        }

        err = drvDeviceStatus(i, &device_status);
        if (err != DRV_ERROR_NONE) {
            BBOX_ERR("[%s] get run status failed with %d.", bbox_get_dev_str(phy_id, i, dev_buf, sizeof(dev_buf)),
                     (int)err);
            continue;
        }

        if (device_status == DRV_STATUS_COMMUNICATION_LOST) {
            if (boot_status == DSMI_BOOT_STATUS_FINISH) {
                info->status[phy_id] = SOC_STATUS_HEARTBEAT_LOST;
                info->online_num++;
            } else {
                info->status[phy_id] = SOC_STATUS_BOOT_FAILED;
                info->offline_num++;
            }
        } else if (device_status == DRV_STATUS_INITING) {
            info->status[phy_id] = SOC_STATUS_INITING;
            info->online_num++;
        } else {
            info->status[phy_id] = SOC_STATUS_OS_RUNNING;
            info->online_num++;
        }
    }
}

STATIC void bbox_offline_device_state(struct bbox_devices_info *info)
{
    u32 devices[MAX_PHY_DEV_NUM] = {0};
    u32 i;

    drvError_t ret = halGetDevProbeList(devices, MAX_PHY_DEV_NUM);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != DRV_ERROR_NONE, return, "get physical id list failed(%d).", (int)ret);

    for (i = 0; i < MAX_PHY_DEV_NUM; i++) {
        u32 phy_id = devices[i];
        if (phy_id >= MAX_PHY_DEV_NUM) {
            continue;
        }

        if (info->status[phy_id] != SOC_STATUS_UNKNOWN) {
            continue;
        }

        info->offline_num++;
        info->status[phy_id] = SOC_STATUS_BOOT_FAILED;
    }
}

/**
 * @brief       get status of the devices
 * @param [in]  info:       devices info
 * @return      NA
 */
STATIC void bbox_get_devices_status(struct bbox_devices_info *info)
{
    // 1.host reboot, example:
    //   a. first can find 5 physical device, and 5 logical device
    //   b. host reboot, find 5 physical device, and 4 logical device
    // 2.device hot reset, example:
    //   a. first can find 5 physical device, and 5 logical device
    //   b. host reset, find 4 physical device, and 5 logical device
    bbox_online_device_state(info);
    bbox_offline_device_state(info);
}

STATIC const char *bbox_get_devices_status_des(u32 status)
{
    switch (status) {
        case SOC_STATUS_NO_DEVICE:
            return STRING_NO_DEVICE;
        case SOC_STATUS_BOOT_FAILED:
            return STRING_BOOT_FAILED;
        case SOC_STATUS_HEARTBEAT_LOST:
            return STRING_HEARTBEAT_LOST;
        case SOC_STATUS_OS_RUNNING:
            return STRING_OS_RUNNING;
        case SOC_STATUS_INITING:
            return STRING_INITING;
        default:
            return STRING_UNKNOWN;
    }
}

STATIC void bbox_save_devices_info(const struct bbox_devices_info *info, const char *path)
{
    BBOX_CHK_NULL_PTR(path, return);
    BBOX_CHK_NULL_PTR(info, return);

    char buf[DEV_INFO_MAXLEN] = {0};
    s32 ret = sprintf_s(buf, DEV_INFO_MAXLEN,
                        "base info:\n"
                        "==================================================\n"
                        "device num:\t\t0x%x\n"
                        "probe num:\t\t0x%x\n"
                        "online num:\t\t0x%x\n"
                        "offline num:\t\t0x%x\n\n"
                        "devices info:\n"
                        "==================================================\n"
                        "dir                phy-id    logic-id    status\n",
                        info->dev_num, info->prob_num, info->online_num, info->offline_num);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret == -1, return, "format devices information failed.");
    ret = bbox_save_buf_to_fs(path, DEV_INFO_FILE, buf, (u32)strlen(buf), BBOX_FALSE);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret < 0, return, "save devices information failed");

    s32 i;
    for (i = 0; i < MAX_PHY_DEV_NUM; i++) {
        if (info->status[i] == SOC_STATUS_NO_DEVICE) {
            continue;
        }
        s32 dev_id = ((info->dev_id[i] == UINT_INVALID) ? (-1) : (s32)info->dev_id[i]);
        ret = bbox_format_devices_info(buf, bbox_get_devices_status_des(info->status[i]), info->status[i], dev_id, i);
        BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret == -1, return, "format device information failed.");
        ret = bbox_save_buf_to_fs(path, DEV_INFO_FILE, buf, (u32)strlen(buf), BBOX_TRUE);
        BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret < 0, return, "save device information failed.");
    }
}

STATIC void bbox_init_devices_status(struct bbox_devices_info *info)
{
    BBOX_CHK_NULL_PTR(info, return);
    s32 i;

    info->prob_num = 0;
    info->dev_num = 0;
    info->online_num = 0;
    info->offline_num = 0;

    for (i = 0; i < MAX_PHY_DEV_NUM; i++) {
        info->dev_id[i] = UINT_INVALID;
        info->status[i] = SOC_STATUS_NO_DEVICE;
    }
    u32 devices[MAX_PHY_DEV_NUM] = {0};
    drvError_t ret = halGetDevProbeList(devices, MAX_PHY_DEV_NUM);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != DRV_ERROR_NONE, return, "get physical id list failed(%d).", (int)ret);

    for (i = 0; i < MAX_PHY_DEV_NUM; i++) {
        u32 phy_id = devices[i];
        if (phy_id >= MAX_PHY_DEV_NUM) {
            continue;
        }
        info->status[phy_id] = SOC_STATUS_UNKNOWN;
        info->prob_num++;
    }
}

STATIC int bbox_get_logic_id(const struct bbox_devices_info *info, u32 phy_id)
{
    if (phy_id >= MAX_PHY_DEV_NUM) {
        return BBOX_INVALID_DEVICE_ID;
    }

    if ((info->status[phy_id] == SOC_STATUS_NO_DEVICE) || (info->dev_id[phy_id] == UINT_INVALID)) {
        return BBOX_INVALID_DEVICE_ID;
    }

    return (int)info->dev_id[phy_id];
}

/**
 * @brief       dump exception data of device id
 * @param [in]  phy_id:      logic id of device
 * @param [in]  path:       path to save
 * @param [in]  mode:       process mode
 * @param [in]  info:       device info
 * @return      == -1 failed, == 0 success
 */
STATIC bbox_status bbox_dump_excep_event_by_id(u32 phy_id, const char *path, enum BBOX_DUMP_MODE mode,
                                               const struct bbox_devices_info *info)
{
    bool err = false;
    bbox_status ret = BBOX_SUCCESS;
    int logic_id = bbox_get_logic_id(info, phy_id);

    // dump if device heart beat lost when msnpureport permanent
    if (mode == BBOX_DUMP_MODE_HBL) {
        return (info->status[phy_id] == SOC_STATUS_HEARTBEAT_LOST) ?
                   (ret = bbox_dump_excep_event(phy_id, path, EVENT_HEARTBEAT_LOST, NULL, logic_id)) :
                   ret;
    }
    switch (info->status[phy_id]) {
        case SOC_STATUS_BOOT_FAILED:
            ret = bbox_dump_excep_event(phy_id, path, EVENT_LOAD_TIMEOUT, NULL, logic_id);
            break;
        case SOC_STATUS_HEARTBEAT_LOST:
            ret = bbox_dump_excep_event(phy_id, path, EVENT_HEARTBEAT_LOST, NULL, logic_id);
            break;
        case SOC_STATUS_OS_RUNNING:
        case SOC_STATUS_INITING:
            ret = bbox_dump_runtime(phy_id, path, mode, logic_id);
            break;
        default:
            ret = BBOX_SUCCESS;
            BBOX_DBG("status unknown or not found.");
            break;
    }
    err = ((ret == BBOX_SUCCESS) ? err : true);
    // dump device all info
    if (mode == BBOX_DUMP_MODE_FORCE) {
        if (info->status[phy_id] == SOC_STATUS_OS_RUNNING || info->status[phy_id] == SOC_STATUS_HEARTBEAT_LOST ||
            info->status[phy_id] == SOC_STATUS_INITING) {
            ret = bbox_dump_excep_event(phy_id, path, EVENT_DUMP_FORCE, NULL, logic_id);
            err = ((ret == BBOX_SUCCESS) ? err : true);
        }
    }
    // dump device vmcore data
    if (mode == BBOX_DUMP_MODE_VMCORE) {
        u8 setbuffer[KDUMP_FLAG_NUM * sizeof(unsigned int)] = {0};
        *((unsigned int *)setbuffer) = KDUMP_MAGIC_NUM;
        if (info->status[phy_id] == SOC_STATUS_HEARTBEAT_LOST) {
            ret = bbox_pcie_set_kdump_flag(phy_id, 0, KDUMP_FLAG_NUM * sizeof(unsigned int), setbuffer);
            if (ret == BBOX_FAILURE) {
                BBOX_DBG("kdump flag set not succeed.");
            } else {
                ret = bbox_dump_excep_event(phy_id, path, EVENT_DUMP_VMCORE, NULL, logic_id);
                err = ((ret == BBOX_SUCCESS) ? err : true);
            }
        } else {
            BBOX_DBG("status is normal, no need to dump vmcore.");
        }
    }
    return (err == false) ? BBOX_SUCCESS : BBOX_FAILURE;
}

/**
 * @brief       dump exception data by status of the devices
 * @param [in]  dev_id:      physical id of device
 * @param [in]  path:       path to save
 * @param [in]  p_size:      path size
 * @param [in]  mode:       process mode
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dump_exception(s32 dev_id, const char *path, u32 p_size, enum BBOX_DUMP_MODE mode)
{
    BBOX_CHK_NULL_PTR(path, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(p_size == 0, return BBOX_FAILURE, "%u", p_size);

    struct bbox_devices_info info;
    bbox_init_devices_status(&info);
    bbox_get_devices_status(&info);
    bbox_save_devices_info(&info, path);

    bool err = false;
    bbox_status ret = BBOX_SUCCESS;

    if (dev_id == BBOX_INVALID_DEVICE_ID) {
        for (u32 i = 0; i < MAX_PHY_DEV_NUM; i++) {
            ret = bbox_dump_excep_event_by_id(i, path, mode, &info);
            err = ((ret == BBOX_SUCCESS) ? err : true);
        }
    } else {
        u32 phy_id = 0;
        drvError_t err_code = drvDeviceGetPhyIdByIndex((u32)dev_id, &phy_id);
        if (err_code != DRV_ERROR_NONE) {
            BBOX_ERR("get phy_id by index failed. (dev_id=%u)", (u32)dev_id);
            err = true;
        } else if (phy_id >= MAX_PHY_DEV_NUM) {
            BBOX_ERR("Invalid id. (logic_id=%d; phy_id=%u)", dev_id, phy_id);
            err = true;
        } else {
            ret = bbox_dump_excep_event_by_id(phy_id, path, mode, &info);
            err = ((ret == BBOX_SUCCESS) ? err : true);
        }
    }

    bbox_dir_chmod(path, FILE_RO_MODE, DIR_RECURSION_DEPTH);
    return (err == false) ? BBOX_SUCCESS : BBOX_FAILURE;
}
