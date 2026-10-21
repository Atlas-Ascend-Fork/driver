/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DMS_PCIE_H
#define __DMS_PCIE_H

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#ifndef __linux
extern int stop_device(void);
extern int start_device(void);
#endif

struct devdrv_pcie_info_para {
    unsigned int dev_id;
    unsigned int sub_cmd;
};

int dms_get_pcie_link_info(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                           unsigned int *size);
int DmsGetPcieInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size);
#endif