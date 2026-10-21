/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DMS_FAULT_H
#define __DMS_FAULT_H

#include <ascend_hal.h>

#define DMS_FAULT_DEVICE_ALL (-1)

drvError_t DmsGetFaultEvent(int dev_id, struct dms_event_filter *filter, int timeout, struct dms_fault_event *event);
drvError_t DmsGetHistoryFaultEvent(int device_id, struct dsmi_event *event_buf, int event_buf_size, int *event_cnt);
drvError_t dms_clear_fault_event(u32 devid);
drvError_t dms_disable_fault_event(u32 devid, u32 event_id);
drvError_t dms_enable_fault_event(u32 devid, u32 event_id);
drvError_t DmsQueryEventStr(u32 devid, u32 event_code, unsigned char *str, int str_size);
drvError_t dms_inject_fault(DSMI_FAULT_INJECT_INFO *info);
drvError_t DmsGetFaultInjectInfo(unsigned int device_id, const unsigned int max_info_cnt, void *info_buf,
                                 unsigned int *real_info_cnt);
drvError_t dms_get_device_state(u32 devid, char *state_out, unsigned int out_len);
drvError_t halDeviceBindHost(unsigned int dev_id, struct dsmi_eid_pair_info *eid_info);
drvError_t DmsDevReplace(struct dsmi_device_attr *src_dev_attr, struct dsmi_device_attr *dst_dev_attr,
                         unsigned int timeout, unsigned long long flag);
#endif
