/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __ABL_DMS_MEM_H
#define __ABL_DMS_MEM_H

#include "dsmi_common_interface.h"

int dms_get_ai_ddr_info(unsigned int dev_id, unsigned int vf_id, unsigned long *total, unsigned long *free);
int dms_get_ddr_capacity(unsigned int dev_id, unsigned int vfid, unsigned long *total);
int dms_get_ddr_utilization(unsigned int dev_id, unsigned int vfid, unsigned int *ddr_util);
int dms_get_hbm_capacity(unsigned int dev_id, unsigned int vfid, unsigned int *total);
int dms_get_hbm_utilization(unsigned int dev_id, unsigned int vfid, unsigned int *hbm_util);
int dms_get_device_cgroup_info(unsigned int dev_id, unsigned int vfid, struct tag_cgroup_info *cg_info);
int DmsGetMemoryInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size);
int DmsSetMemoryInfo(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size);
int dms_set_memory_detect_info(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size);
int dms_get_memory_detect_info(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int *size);

#endif
