/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BBOX_DUMP_PROCESS_H
#define BBOX_DUMP_PROCESS_H

#include "bbox_int.h"
#include "bbox_data_parser.h"
#include "bbox_ddr_int.h"
#include "bbox_drv_adapter.h"

#define MAX_PHY_DEV_NUM 64
#define BBOX_INVALID_DEVICE_ID (-1)
#define DEV_BUF_MAX_LEN 32U

enum BBOX_DUMP_MODE {
    BBOX_DUMP_MODE_NONE = 0,
    BBOX_DUMP_MODE_ALL = 1,
    BBOX_DUMP_MODE_FORCE = 2,
    BBOX_DUMP_MODE_VMCORE = 3,
    BBOX_DUMP_MODE_HBL = 4,
};

enum DEVICE_EVENT_BBIT {
    DEVICE_EVENT_NONE = 0,
    DEVICE_EVENT_OOM_TRIGGER = 1 << 0,
    DEVICE_EVENT_CHANNEL_ERROR = 1 << 1,
    DEVICE_EVENT_MAX = 0xFFFF,
};

enum EXCEPTION_EVENT_TYPE {
    EVENT_NONE = 0,
    EVENT_DEVICE, // device event list
    EVENT_DUMP_FILE = EVENT_DEVICE,
    EVENT_OOM,
    EVENT_HOST, // host event list
    EVENT_LOAD_TIMEOUT = EVENT_HOST,
    EVENT_HEARTBEAT_LOST,
    EVENT_HDC_EXCEPTION,
    EVENT_DUMP_FORCE,
    EVENT_DUMP_VMCORE,
};

// report from host
#define EXCEPID_OSLOAD_TIMEOUT 0x68020001 // device os load driver failed
#define EXCEPID_HEARTBEAT_LOST 0x68020002 // device heart beat abnormal
#define EXCEPID_HDC_EXCEPTION 0x64021001
#define EXCEPID_AP_OOM 0xA4040001
#define EXCEPID_DUMP_FORCE 0x721E0000
#define EXCEPID_DUMP_VMCORE 0x721E0001

struct host_exception_list {
    enum EXCEPTION_EVENT_TYPE event;
    u32 e_type;
    u32 except_id;
    const char *module;
    const char *reason;
    const char *event_name;
};

typedef struct dump_data_config {
    const char *name;
    u32 phy_offset; // dma offset
    u32 data_size;  // data size
    s32 type;
    enum plain_text_table_type parse_type;
    bbox_status (*bbox_data_check_ptr)(u32 phy_id);
    bbox_status (*bbox_data_dump_ptr)(u32 phy_id, u32 offset, u32 size, u8 *buf);
    bbox_status (*bbox_data_parse_ptr)(enum plain_text_table_type type, u8 *data, u32 len, const char *lpath);
} dump_data_config_st;

typedef bbox_status (*bbox_pcie_set_data_func)(u32 phy_id, u32 offset, u32 size, u8 *buffer,
                                               drvError_t (*write_func)(u32 phy_id, u32 offset, u8 *buffer, u32 len));

const dump_data_config_st *bbox_get_data_config(enum EXCEPTION_EVENT_TYPE event);
u32 bbox_get_data_config_size(enum EXCEPTION_EVENT_TYPE event);

bbox_status bbox_dump_excep_event(u32 phy_id, const char *path, enum EXCEPTION_EVENT_TYPE event, const char *tms,
                                  int logic_id);
bbox_status bbox_dump_runtime(u32 phy_id, const char *path, enum BBOX_DUMP_MODE mode, int logic_id);
bbox_status bbox_get_device_log_tms(const struct rdr_log_info *info, char *tms, s32 len);
bbox_status bbox_check_dev_event(u32 phy_id, int logic_id, u16 *event, char *tms, s32 len);
bbox_status bbox_create_excep_dir_adapt(u32 phy_id, int logic_id, enum EXCEPTION_EVENT_TYPE event, const char *path,
                                        char *dev_path);
bbox_status bbox_dump_dev_files(u32 phy_id, int logic_id, const char *path);
const char *bbox_get_dev_str(u32 phy_id, int logic_id, char *buf, u32 buf_size);
bbox_status bbox_kdump_flag_set_data(u32 phy_id, u32 offset, u32 size, u8 *buf, bbox_pcie_set_data_func set_data);
#endif
