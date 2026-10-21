/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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

#ifndef CHAN_TRACE_H
#define CHAN_TRACE_H
#include "drv_type.h"
#include "chan_init.h"
#include "ascend_kernel_hal.h"

struct trs_chan_recv_trace {
    u32 chan_id;
    struct trs_chan_type types;

    u32 cqid;
    u32 cq_head;

    u32 cqe_num;
    u32 recv_cqe_num; /* output */
    int timeout;      /* ms */
};

void trs_chan_trace_sqe(const char *str, struct trs_chan *chan, struct trs_chan_sq_ctx *sq, void *sqe);
void trs_chan_trace_cqe(const char *str, struct trs_chan *chan, struct trs_chan_cq_ctx *cq, void *cqe);
void trace_chan_trace_recv(const char *str, struct trs_chan *chan, struct trs_chan_recv_para *para);

#endif /* CHAN_TRACE_H */
