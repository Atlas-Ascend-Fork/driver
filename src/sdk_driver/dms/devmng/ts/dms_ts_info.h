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
#ifndef DMS_TS_STATUS_H
#define DMS_TS_STATUS_H
#include "dms_ts_info_ioctl.h"

#define DMS_MODULE_TS_INFO "dms_ts_info"

#define DMS_AI_PROFILING_VALUE 0xEF
#define DMS_AI_PROFILING_CONFLIC_RET 2
#define DMS_AI_TIMEOUT_RET 7
#define START_MSG 0
#define END_MSG 1
#define NPU_CORE_NUM 5
#define COPY_TO_USER 1
#define NOT_COPY_TO_USER 0

struct ts_info_data {
    u32 core_num;
    u8 core_data[128]; // max support 128 core
};

int dms_ts_init(void);
void dms_ts_uninit(void);
int dms_get_ai_info_from_ts(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int dms_get_ai_info_from_lp(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int ts_get_ai_utilization(unsigned int dev_id, unsigned int vfid, unsigned int core_id, unsigned int *utilization);
int dms_calc_aicore_aivector_utilization(unsigned int dev_id, unsigned int cmd_type, struct dms_ts_info_in *ts_info,
                                         unsigned int *utilization);

#ifdef CFG_FEATURE_AI_UTIL_BY_CALCULATE
int ts_get_ai_utilization_by_calculate(unsigned int dev_id, unsigned int vfid, unsigned int core_id,
                                       unsigned int *utilization);
#define ts_get_ai_utilization ts_get_ai_utilization_by_calculate
#endif
#ifdef CFG_FEATURE_AI_UTIL_FROM_IPC
int ts_get_ai_utilization_from_ipc(unsigned int dev_id, unsigned int vfid, unsigned int core_id,
                                   unsigned int *utilization);
#define ts_get_ai_utilization ts_get_ai_utilization_from_ipc
#endif

#ifdef CFG_FEATURE_TS_FAULT_MASK
int dms_get_ts_fault_mask(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_set_ts_fault_mask(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#endif

u32 dms_get_tsv_tx_chan_id(void);
int dms_check_ts_cmd_type(u32 dev_id, u32 cmd_type);
#endif