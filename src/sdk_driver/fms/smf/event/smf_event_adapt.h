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

#ifndef __SMF_EVENT_ADAPT_H__
#define __SMF_EVENT_ADAPT_H__
#include "kernel_version_adapt.h"
#include "dms/dms_shm_info.h"
#include "dms/dms_cmd_def.h"
#include "comm_kernel_interface.h"
#include "fms/fms_smf.h"

int smf_event_subscribe_from_device(u32 phyid);
int smf_event_clean_to_device(u32 phyid);
int smf_event_mask_event_code(u32 phyid, u32 event_code, u8 mask);
int smf_get_event_code_from_bar(u32 devid, u32 *health_code, u32 health_len, struct shm_event_code *event_code,
                                u32 event_len);
int smf_get_event_code_from_local(u32 devid, u32 *health_code, struct shm_event_code *event_code, u32 event_len);
int smf_get_health_code_from_bar(u32 devid, u32 *health_code, u32 health_len);
int smf_get_health_code_from_local(u32 devid, u32 *health_code);
int smf_event_distribute_to_bar(u32 phyid);
int smf_distribute_all_devices_event_to_bar(void);
int smf_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
int smf_get_remote_event_para(int phyid, struct dms_event_para *dms_event, u32 in_cnt, u32 *event_num);
int smf_get_remote_notify_event_para(int phyid, struct dms_event_para *dms_event, u32 in_cnt, u32 *event_num);
int smf_get_connect_protocol(u32 dev_id);
int smf_get_container_ns_id(u32 *ns_id);
#endif
