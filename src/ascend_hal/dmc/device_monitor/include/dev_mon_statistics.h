/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DEV_MON_DFX_H
#define DEV_MON_DFX_H

#include "device_monitor_type.h"

#define MAX_DMP_THREADS 1024
#define MAX_DMP_THREADS_PRINT 768
#define MAX_OPCODE 256
#define MONITOR_SCAN_PERIOD 5
#define MONITOR_TIME_OUT_PERIOD 10

void dm_update_dmp_invoked_count(void);
void dm_update_cmd_invoked_count(DEV_MON_CMD_TAG_VALUE_ST *value);
void dm_printf_dmp_historical_invoke_data(SYSTEM_CB_T *cb);
void *monitor_worker(void *arg);
int dmp_monitor_start(unsigned int opcode, unsigned int opfun);
void dmp_monitor_end(int index);
void dmp_print_timeout_msg(hash_table *cmd_hash_table);

#endif