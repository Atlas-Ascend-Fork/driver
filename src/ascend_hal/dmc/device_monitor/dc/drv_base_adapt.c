/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "dm_common.h"
#include <string.h>
#include "securec.h"

void __dm_udp_recv_set_irecv_authority(DM_INTF_S *intf, DM_RECV_ST *irecv)
{
    if (strcmp(intf->name, DM_UDP_MANAGEMENT_INTF) == 0) {
        irecv->session_prop = ADMIN_PROP;
        irecv->host_root = ROOT_PRIV;
    } else {
        irecv->session_prop = GUEST_PROP;
        irecv->host_root = 0;
    }
}