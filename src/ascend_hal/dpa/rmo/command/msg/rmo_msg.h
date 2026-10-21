/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __RMO_MSG_H__
#define __RMO_MSG_H__

struct rmo_map_msg {
    struct res_map_info res_info;
    unsigned int len;
    unsigned long va;
};

#define RMO_EVENT_PROC_READ_LEN EVENT_PROC_RSP_LEN
struct rmo_read_msg {
    struct res_map_info res_info;
    unsigned int len;
};

/* Hardware event scheduler limits msg_len to 40 bytes (TOPIC_SCHED_USER_DATA_PAYLOAD_LEN).
 * RMO msg_len = sizeof(event_sync_msg)(8) + sizeof(res_map_info)(20) + data[] + sizeof(len)(4).
 * So max data payload = 40 - 8 - 20 - 4 = 8.
 */
#define RMO_EVENT_PROC_WRITE_LEN 8
struct rmo_write_msg {
    struct res_map_info res_info;
    char data[RMO_EVENT_PROC_WRITE_LEN];
    unsigned int len;
};

#endif // __RMO_MSG_H__
