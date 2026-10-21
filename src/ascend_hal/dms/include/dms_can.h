/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _DMS_CAN_H
#define _DMS_CAN_H

#include "dsmi_common_interface.h"

typedef enum {
    CAN_BUS_STATE_ACTIVER,
    CAN_BUS_STATE_ERR_WARNING,
    CAN_BUS_STATE_ERR_PASSIVE,
    CAN_BUS_STATE_ERR_BUSOFF,
    CAN_BUS_STATE_DOWN,
} CAN_BUS_STATE;

struct can_status_stru {
    CAN_BUS_STATE bus_state;
    unsigned int rx_err_counter;
    unsigned int tx_err_counter;
    unsigned int err_passive;
};

int DmsGetCanStatus(unsigned int dev_id, const char *name, unsigned int name_len,
                    struct dsmi_can_status_stru *can_status_data);
int DmsSetCanCfg(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size);
int DmsGetCanCfg(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size);

#endif