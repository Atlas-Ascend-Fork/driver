/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DMS_SOC_H
#define __DMS_SOC_H

#include "ascend_hal_error.h"
#include "dsmi_common_interface.h"

drvError_t DmsGetAllDeviceNode(unsigned int dev_id, DEV_DTM_CAP capability, struct dsmi_dtm_node_s node_info[],
                               unsigned int *size);
drvError_t DmsCtrlDeviceNode(unsigned int dev_id, struct dsmi_dtm_node_s dtm_node, DSMI_DTM_OPCODE opcode,
                             IN_OUT_BUF buf);
drvError_t DmsGetBistInfo(unsigned int dev_id, unsigned int cmd, unsigned char *buf, unsigned int *len);
#endif
