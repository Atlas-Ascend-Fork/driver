/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef DMS_MSG_H
#define DMS_MSG_H

#include "devdrv_user_common_msg.h"
#include "devdrv_manager_common_msg.h"

struct dms_h2d_msg_head {
    u32 dev_id;
    u32 msg_id;
    u16 valid;  /* validity judgement, 0x5A5A is valid */
    u16 result; /* process result from rp, zero for succ, non zero for fail */
};

#define DMS_INFO_PAYLOAD_LEN 512UL
#define DMS_H2D_MSG_PAYLOAD_LEN (DMS_INFO_PAYLOAD_LEN - sizeof(struct dms_h2d_msg_head))
struct dms_h2d_msg {
    struct dms_h2d_msg_head header;
    u8 payload[DMS_H2D_MSG_PAYLOAD_LEN];
};

typedef struct devdrv_core_utilization {
    u32 dev_id;
    u32 vfid;
    u32 core_type; /* 0: aicore  1: aivector 2:aicpu */
    u32 utilization;
} devdrv_core_utilization_t;

#define FILTER_LEN_MAX 128
#define PAYLOAD_LEN_MAX 340
struct urd_forward_msg {
    u32 main_cmd;
    u32 sub_cmd;
    char filter[FILTER_LEN_MAX];
    unsigned int filter_len;
    u32 output_len;
    u32 payload_len;
    char payload[PAYLOAD_LEN_MAX];
};

struct dms_timespec_info {
    u64 tv_sec;
    u32 tv_nsec;
};

struct dms_walltime_info {
    u32 dev_id;
    u32 time_update; /* if time_update = 1, you need send time to device to update */
    struct dms_timespec_info wall_time;
};

#endif