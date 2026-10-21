/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_MON_DMP_MULIT_H
#define DEV_MON_DMP_MULIT_H
#include "device_monitor_type.h"
#include "dm_common.h"
#include "dev_mon_protocol_def.h"
#include "dev_mon_protocol_macro.h"
#include "dev_mon_api.h"
#include "dev_mon_management.h"
#include "dev_mon_ddmp_construction.h"

#ifdef DEV_MON_UT
#define STATIC
#else
#define STATIC static
#endif

#define MS_PER_SEC 1000
#define NS_TO_MS_DIV 1000000
#define MULT_MSG_TIMEOUT 30000 // 30s

int request_mult_proc(BD_MSG_ST *bd_msg);
int rsp_mult_proc(BD_MSG_ST *bd_msg);
int rsp_mult_send(DM_INTF_S *intf, DM_RECV_ST *recv_data, DM_REP_MSG *resp);

#endif
