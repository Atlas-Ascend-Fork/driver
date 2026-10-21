/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/file.h>
#include "bbox_dump_process.h"
#include "bbox_tool_fs.h"
#include "bbox_log_common.h"
#include "bbox_dump_exception.h"
#include "bbox_print.h"
#include "adx_api.h"
#include "adump_device_pub.h"
#include "ascend_hal_base.h"

bbox_status bbox_create_excep_dir_adapt(u32 phy_id, int logic_id, enum EXCEPTION_EVENT_TYPE event, const char *path,
                                        char *dev_path)
{
    int sn_ret;

    if (event == EVENT_LOAD_TIMEOUT) {
        sn_ret = bbox_format_device_offline_path(dev_path, DIR_MAXLEN, path, phy_id);
    } else {
        sn_ret = bbox_format_device_path(dev_path, DIR_MAXLEN, path, (u32)logic_id);
    }
    if (sn_ret == -1) {
        return BBOX_FAILURE;
    }

    return BBOX_SUCCESS;
}

static int remove_directory(const char *path)
{
    DIR *d;
    struct stat st;
    struct dirent *entry;
    char subpath[DIR_MAXLEN];

    if (lstat(path, &st) != 0) {
        return BBOX_FAILURE;
    }
    if (!S_ISDIR(st.st_mode)) {
        return unlink(path);
    }

    d = opendir(path);
    if (!d) {
        return BBOX_FAILURE;
    }

    while ((entry = readdir(d)) != NULL) {
        if ((strncmp(entry->d_name, ".", 1) == 0 && entry->d_name[1] == '\0') ||
            (strncmp(entry->d_name, "..", 2) == 0 && entry->d_name[2] == '\0')) {
            continue;
        }

        if (snprintf_s(subpath, sizeof(subpath), sizeof(subpath) - 1, "%s/%s", path, entry->d_name) == -1) {
            closedir(d);
            return BBOX_FAILURE;
        }

        if (lstat(subpath, &st) != 0) {
            closedir(d);
            return BBOX_FAILURE;
        }

        if (S_ISDIR(st.st_mode)) {
            if (remove_directory(subpath) != 0) {
                closedir(d);
                return BBOX_FAILURE;
            }
        } else if (unlink(subpath) != 0) {
            closedir(d);
            return BBOX_FAILURE;
        }
    }

    closedir(d);
    return rmdir(path);
}

static int bbox_create_temp_dir(const char *base_path, char *tmp_base, size_t size)
{
    char tmp_template[DIR_MAXLEN];

    if (snprintf_s(tmp_template, sizeof(tmp_template), sizeof(tmp_template) - 1, "%s/.bbox_XXXXXX", base_path) == -1) {
        BBOX_ERR("Failed to snprintf_s.");
        return BBOX_FAILURE;
    }

    if (mkdtemp(tmp_template) == NULL) {
        BBOX_ERR("Failed to create temp dir.");
        return BBOX_FAILURE;
    }

    if (strcpy_s(tmp_base, size, tmp_template) != 0) {
        BBOX_ERR("Failed to strcpy_s.");
        rmdir(tmp_template);
        return BBOX_FAILURE;
    }

    return BBOX_SUCCESS;
}

static int bbox_fetch_device_dir(u32 phy_id, int logic_id, const char *tmp_base, char *old_path, size_t old_size)
{
    s32 ret;
    char dev_buf[DEV_BUF_MAX_LEN];

    ret = AdxGetDeviceFile((unsigned short)phy_id, tmp_base, "bbox");
    if (ret == IDE_DAEMON_HDC_TIMEOUT) {
        BBOX_WAR("[%s] Export bbox timeout. Retrying later may help. (ret=%d)",
                 bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)), ret);
        return BBOX_FAILURE;
    }
    if (ret != 0) {
        BBOX_ERR("[%s] get bbox runtime data failed by hdc channel. (ret=%d)",
                 bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)), ret);
        return BBOX_FAILURE;
    }

    if (snprintf_s(old_path, old_size, old_size - 1, "%s/device-%u", tmp_base, phy_id) == -1) {
        BBOX_ERR("Failed to construct path.");
        return BBOX_FAILURE;
    }

    return BBOX_SUCCESS;
}

bbox_status bbox_dump_dev_files(u32 phy_id, int logic_id, const char *path)
{
    int errno_tmp;
    struct stat st;
    char dev_buf[DEV_BUF_MAX_LEN], tmp_base[DIR_MAXLEN], old_path[DIR_MAXLEN], new_path[DIR_MAXLEN];

    BBOX_INF("[%s] bbox runtime data, dump start.", bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)));

    if (bbox_create_temp_dir(path, tmp_base, sizeof(tmp_base)) != 0) {
        return BBOX_FAILURE;
    }

    if (bbox_fetch_device_dir(phy_id, logic_id, tmp_base, old_path, sizeof(old_path)) != 0) {
        remove_directory(tmp_base);
        return BBOX_FAILURE;
    }

    if ((lstat(old_path, &st) != 0) || (!S_ISDIR(st.st_mode))) {
        BBOX_INF("[%s] bbox runtime data, dump end.", bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)));
        remove_directory(tmp_base);
        return BBOX_SUCCESS;
    }

    if (snprintf_s(new_path, sizeof(new_path), sizeof(new_path) - 1, "%s/device-%d", path, logic_id) == -1) {
        BBOX_ERR("Failed to construct path.");
        remove_directory(old_path);
        remove_directory(tmp_base);
        return BBOX_FAILURE;
    }

    if (rename(old_path, new_path) != 0) {
        errno_tmp = errno;
        BBOX_INF("Skip renaming. (status=%d)", errno_tmp);
        remove_directory(old_path);
    }

    if (rmdir(tmp_base) != 0) {
        BBOX_WAR("Temp dir not empty or could not be removed.");
    }

    BBOX_INF("[%s] bbox runtime data, dump end.", bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)));
    return BBOX_SUCCESS;
}

s32 bbox_format_devices_info(char *buf, const char *status, u32 dev_status, int logic_id, u32 phy_id)
{
    s32 ret;

    if (dev_status == SOC_STATUS_BOOT_FAILED) {
        ret = sprintf_s(buf, DEV_INFO_MAXLEN, "device-offline-%-2u  %-2u        %-2d          %s\n", phy_id, phy_id,
                        logic_id, status);
    } else {
        ret = sprintf_s(buf, DEV_INFO_MAXLEN, "device-%-2d          %-2u        %-2d          %s\n", logic_id, phy_id,
                        logic_id, status);
    }

    return ret;
}

const char *bbox_get_dev_str(u32 phy_id, int logic_id, char *buf, u32 buf_size)
{
    int ret;

    if ((buf == NULL) || (buf_size == 0)) {
        return "device";
    }

    if (logic_id == BBOX_INVALID_DEVICE_ID) {
        ret = snprintf_s(buf, buf_size, buf_size - 1, "phy-device-%u", phy_id);
    } else {
        ret = snprintf_s(buf, buf_size, buf_size - 1, "device-%d", logic_id);
    }
    if (ret < 0) {
        return "device";
    }

    return buf;
}

bbox_status bbox_kdump_flag_set_data(u32 phy_id, u32 offset, u32 size, u8 *buf, bbox_pcie_set_data_func set_data)
{
    return set_data(phy_id, offset, size, buf, bbox_drv_kdump_write);
}
