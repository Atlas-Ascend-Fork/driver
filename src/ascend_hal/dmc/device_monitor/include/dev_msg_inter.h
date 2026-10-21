/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DEV_MSG_INTER
#define DEV_MSG_INTER

signed int ipmi_init(IPMI_CB_ST **ipmi_cb)
{}

INT32 openipmi_init(IPMI_INTF_ST **my_intf, IPMI_CB_ST *cb, const IPMI_ADDR_ST *my_addr, const char *my_name,
                    int name_len)
{}

#endif
