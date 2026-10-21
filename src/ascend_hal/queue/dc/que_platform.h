/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef QUE_PLATFORM_H
#define QUE_PLATFORM_H

#include "pbl_uda_user.h"
#include "que_platform_common.h"

extern bool g_enable_local_queue;
extern bool g_default_client_queue;

/* ============================================================================
 * Platform-specific queue interfaces - from que_host_adapt.c / que_device_adapt.c
 * Non-UBE related functions
 * ============================================================================ */

/* From queue_interface.c */
unsigned int que_get_unified_devid_platform(unsigned int dev_id);
void que_check_convert_to_inter_dev_queue_platform(unsigned int dev_id, int *type);
int queue_query_support_inter_dev_que_platform(QueueQueryOutputPara *out_put);
void que_get_inter_dev_local_devid_platform(struct shareQueInfo *que_info, unsigned int dev_id,
                                            unsigned int *local_dev_id);
bool queue_inter_devid_invalid_platform(unsigned int dev_id);

/*
 * Check whether the device uses client-queue deployment mode.
 * Host: queries the device feature bitmap.
 * Device: determined by compile-time configuration.
 */
bool queue_is_client_queue_deploy(void);

/* From queue.c */
void queue_send_wake_up_rsp_platform(struct queue_manages *que_manage, int rsp_ret);
unsigned int queue_get_submit_devid_platform(unsigned int submit_devid, unsigned int dst_engine,
                                             unsigned int dst_devid);
drvError_t queue_get_event_src_udevid_platform(unsigned int dev_id, unsigned int dst_engine, unsigned int *src_udevid,
                                               int *src_location);
unsigned int queue_get_grp_dst_engine_platform(unsigned int dev_id, unsigned int group_id);

#endif /* QUE_PLATFORM_H */
