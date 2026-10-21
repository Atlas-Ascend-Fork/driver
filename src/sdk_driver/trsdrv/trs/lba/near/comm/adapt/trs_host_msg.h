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
#ifndef TRS_HOST_MSG_H
#define TRS_HOST_MSG_H

#include "ka_type.h"

#include "trs_pub_def.h"
#include "trs_msg.h"
#include "comm_kernel_interface.h"

struct devdrv_non_trans_msg_chan_info *trs_get_msg_chan_info(void);
int trs_host_msg_chan_recv_check(u32 devid, struct trs_msg_data *data, u32 in_data_len, u32 out_data_len,
                                 u32 *real_out_len);
int trs_host_msg_send(u32 devid, void *msg, size_t size);
int trs_host_msg_send_pure_ret(u32 devid, void *msg, size_t size);
int trs_host_msg_init(u32 ts_inst_id);
void trs_host_msg_uninit(u32 ts_inst_id);

#endif /* TRS_HOST_MSG_H */