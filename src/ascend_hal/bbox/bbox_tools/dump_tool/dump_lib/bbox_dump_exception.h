/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BBOX_DUMP_EXCEPTION_H
#define BBOX_DUMP_EXCEPTION_H

#include "bbox_int.h"
#include "bbox_dump_process.h"
#include "bbox_dump_interface.h"

#define UINT_INVALID ((u32)(-1))
#define DEV_INFO_MAXLEN 256
#define DEV_INFO_FILE "device_info.txt"

#define STRING_UNKNOWN "unknown"
#define STRING_NO_DEVICE "no device"
#define STRING_BOOT_FAILED "boot failed"
#define STRING_HEARTBEAT_LOST "heartbeat lost"
#define STRING_OS_RUNNING "os running"
#define STRING_INITING "initing"

enum SOC_STATUS_TYPE {
    SOC_STATUS_UNKNOWN = 0,
    SOC_STATUS_NO_DEVICE,
    SOC_STATUS_BOOT_FAILED,
    SOC_STATUS_HEARTBEAT_LOST,
    SOC_STATUS_OS_RUNNING,
    SOC_STATUS_INITING,
};

struct bbox_devices_info {
    u32 online_num;
    u32 offline_num;
    u32 prob_num;
    u32 dev_num;
    u32 dev_id[MAX_PHY_DEV_NUM];
    u32 status[MAX_PHY_DEV_NUM];
};

bbox_status bbox_dump_exception(s32 dev_id, const char *path, u32 p_size, enum BBOX_DUMP_MODE mode);
s32 bbox_format_devices_info(char *buf, const char *status, u32 dev_status, int logic_id, u32 phy_id);
#endif // BBOX_DUMP_EXCEPTION_H
