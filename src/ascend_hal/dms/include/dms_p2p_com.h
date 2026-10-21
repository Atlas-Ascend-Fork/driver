/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DMS_P2P_COM_H
#define DMS_P2P_COM_H

#include "ascend_hal_error.h"
#include "pbl_urd_sub_cmd_common.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define DMS_RES_FLAG_ENABLE 0
#define DMS_RES_FLAG_DISABLE 1

typedef enum {
    DMS_P2P_TYPE_ALL = 0,
    DMS_P2P_TYPE_MEM,
    DMS_P2P_TYPE_NOTIFY,
    DMS_P2P_TYPE_MAX
} dms_p2p_type;

int dms_set_p2p_com_info(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size);
int dms_get_p2p_com_info(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size);
void dms_set_p2p_restore_info_flag(bool flag);
drvError_t dms_set_p2p_restore_info(u32 dev_id, u32 peer_phy_id, u32 p2p_type, enum urd_devdrv_p2p_attr_op opcode);
drvError_t dms_get_p2p_restore_info(u32 dev_id, u32 peer_phy_id, u32 p2p_type, u32 *cnt);
#endif