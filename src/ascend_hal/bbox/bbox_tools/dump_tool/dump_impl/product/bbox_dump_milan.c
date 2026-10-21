/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "bbox_dump_process.h"
#include "bbox_tool_fs.h"
#include "bbox_log_common.h"
#include "bbox_dump_exception.h"
#include "bbox_print.h"
#include "adx_api.h"
#include "adump_device_pub.h"

bbox_status bbox_create_excep_dir_adapt(u32 phy_id, int logic_id, enum EXCEPTION_EVENT_TYPE event, const char *path,
                                        char *dev_path)
{
    return (bbox_format_device_path(dev_path, DIR_MAXLEN, path, phy_id) == -1) ? BBOX_FAILURE : BBOX_SUCCESS;
}

bbox_status bbox_dump_dev_files(u32 phy_id, int logic_id, const char *path)
{
    BBOX_INF("[device-%u] bbox runtime data, dump start.", phy_id);
    s32 ret = AdxGetDeviceFile((unsigned short)phy_id, path, "bbox");
    if (ret == IDE_DAEMON_HDC_TIMEOUT) {
        BBOX_WAR("[device-%u] Export bbox timeout. Retrying later may help. (ret=%d)", phy_id, ret);
        return BBOX_FAILURE;
    }
    if (ret != 0) {
        BBOX_ERR("[device-%u] get bbox runtime data failed by hdc channel. (ret=%d)", phy_id, ret);
        return BBOX_FAILURE;
    }

    BBOX_INF("[device-%u] bbox runtime data, dump end.", phy_id);
    return BBOX_SUCCESS;
}

s32 bbox_format_devices_info(char *buf, const char *status, u32 dev_status, int logic_id, u32 phy_id)
{
    return sprintf_s(buf, DEV_INFO_MAXLEN, "device-%-2u          %-2u        %-2d          %s\n", phy_id, phy_id,
                     logic_id, status);
}

const char *bbox_get_dev_str(u32 phy_id, int logic_id, char *buf, u32 buf_size)
{
    int ret;

    if ((buf == NULL) || (buf_size == 0)) {
        return "device";
    }

    ret = snprintf_s(buf, buf_size, buf_size - 1, "device-%u", phy_id);
    if (ret < 0) {
        return "device";
    }

    return buf;
}

bbox_status bbox_kdump_flag_set_data(u32 phy_id, u32 offset, u32 size, u8 *buf, bbox_pcie_set_data_func set_data)
{
    return set_data(phy_id, offset, size, buf, bbox_drv_pcie_kdump_write);
}
