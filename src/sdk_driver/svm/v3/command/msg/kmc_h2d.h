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

#ifndef KMC_H2D_H
#define KMC_H2D_H

#include "svm_pub.h"
#include "kmc_msg.h"

struct svm_kmc_d2h_recv_handle {
    int (*func)(u32 udevid, void *msg, u32 *reply_len);
    u32 raw_msg_size;
    u32 extend_gran_size;
};

void svm_kmc_d2h_recv_handle_register(enum svm_kmc_msg_id msg_id, struct svm_kmc_d2h_recv_handle *handle);
int svm_kmc_h2d_send(u32 udevid, void *msg, u32 in_len, u32 out_len, u32 *real_out_len);

int kmc_h2d_init_dev(u32 udevid);
void kmc_h2d_uninit_dev(u32 udevid);
int kmc_h2d_init(void);
void kmc_h2d_uninit(void);

u32 svm_kmc_h2d_get_max_msg_len(void);

#endif