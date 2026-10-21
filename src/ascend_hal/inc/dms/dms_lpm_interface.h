/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DMS_LPM_INTERFACE_H
#define __DMS_LPM_INTERFACE_H

#include "ascend_hal_error.h"
#include "dsmi_common_interface.h"
#include "dms_urd_forward_uk_msg.h"

#define DMS_LPM_SOC_ID 8
#define DMS_VF_ID_PF 0
#define DMS_IPC_TIMEOUT 110

#define DMS_LPM_GET_TEMPERATURE 0
#define DMS_LPM_GET_POWER 1
#define DMS_LPM_GET_VOLTAGE 2
#define DMS_LPM_GET_FREQUENCY 3
#define DMS_LPM_GET_TSENSOR 4
#define DMS_LPM_GET_MAX_FREQUENCY 5
#define DMS_SUBCMD_GET_LP_STATUS 7
#define DMS_LPM_SUB_CMD_MAX 8

#define DMS_SUBCMD_LP_SUSPEND 0

#define POWER_INFO_RESERVE_LEN 8
struct dms_power_state_info {
    unsigned int type;
    unsigned int mode;
    unsigned int value;
    unsigned int reserve[POWER_INFO_RESERVE_LEN];
};

struct dms_lp_state_in {
    unsigned int dev_id;
    struct dms_power_state_info power_info;
};

drvError_t DmsGetLpmInfo(struct dms_lpm_info_in *in, void *result, unsigned int result_size);
drvError_t dms_get_lpm_freq_v2(struct dms_lpm_info_in_v2 *in, unsigned int *frequency);
int DmsLpmPassThroughMcu(unsigned char rw_flag, unsigned char *buf, unsigned char buf_len, unsigned char *resp_buff,
                         unsigned char *recv_len);

/* get info from LP: new\old\adapter */
int DmsGetLowPowerInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size);
int DmsGetInfoFromLp(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                     unsigned int *buf_size);
int DmsGetInfoFromLpAdapter(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                            unsigned int *buf_size);
int dms_set_temperature_threshold(unsigned int dev_id, unsigned int sub_cmd, void *in_buf, unsigned int buf_size);

/* set info to LP: new */
int DmsSetLowPowerInfo(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size);

/* get temperature and threshold from LP: new\old\adapter */
int DmsGetTempFromLp(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size);
int DmsGetTemperature(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                      unsigned int *buf_size);
int DmsGetTemperatureAdapter(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                             unsigned int *buf_size);

/* set temperature and threshold from LP: old */
int DmsSetTemperature(unsigned int dev_id, unsigned int sub_cmd, void *in_buf, unsigned int buf_size);
int dms_lpm_get_ai_core_curr_freq(unsigned int devId, void *buf, unsigned int *size);
drvError_t DmsSetPowerState(unsigned int dev_id, struct dsmi_power_state_info_stru *power_info);
#endif
