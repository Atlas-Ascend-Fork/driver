/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef PMQ_MSG_H
#define PMQ_MSG_H

#include "svm_pub.h"
#include "svm_addr_desc.h"
#include "kmc_msg.h"

struct pmq_query_pa_msg {
    struct svm_kmc_msg_head head;
    int tgid;
    u64 va;
    u64 size;
    u64 rsv; /* reserve */
    struct svm_pa_seg seg[];
};

/* dst_udevid: host global view udevid */
int pmq_msg_send(u32 src_udevid, u32 dst_udevid, void *msg, u32 msg_len, u32 *reply_len);
u32 pmq_get_max_msg_len(void);
#endif
