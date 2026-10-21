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

#ifndef K2UMC_H
#define K2UMC_H

#include "smm_uk_msg.h"
#include "svm_pub.h"

#define K2UMC_FLAG_TO_LOCAL (1U << 0)
#define K2UMC_FLAG_TO_HOST (1U << 1)
#define K2UMC_FLAG_TO_DEVICE (1U << 2)
#define K2UMC_FLAG_SYNC (1U << 3)

struct svm_k2umc_desc {
    u32 request_udevid;
    int request_pid;
    int response_pid;
    u32 subevent_id;
};

static inline bool svm_is_k2u_msg(const struct svm_uk_msg_head *msg_head)
{
    return (msg_head != NULL) && (msg_head->k2u_sync_handle != 0);
}

int svm_k2umc_send(struct svm_k2umc_desc *dst_desc, u32 flag, char *msg, u64 msg_len);
void svm_k2umc_finish(u64 k2u_sync_handle);
int svm_k2umc_init(void);
void svm_k2umc_uninit(void);

#endif
