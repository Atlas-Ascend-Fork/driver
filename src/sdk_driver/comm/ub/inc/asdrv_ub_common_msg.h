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

#ifndef _ASDRV_UB_COMMON_MSG_H_
#define _ASDRV_UB_COMMON_MSG_H_

#include "asdrv_ub_main_adapt.h"

void ubdrv_init_common_msg_ctrl_rwsem(void);
int ubdrv_init_common_msg_chan(void *msg_chan);
void ubdrv_uninit_common_msg_chan(void *msg_chan);
int ubdrv_rx_msg_common_msg_process(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
int ubdrv_alloc_common_msg_queue(struct ascend_ub_msg_dev *msg_dev);
int ubdrv_free_common_msg_queue(u32 dev_id);
struct ubdrv_common_msg_stat *ubdrv_get_common_stat_dfx(u32 dev_id, u32 type);
int ubdrv_rx_msg_common_msg_process_proc(struct ascend_ub_msg_desc *desc, struct ubdrv_common_msg_stat *stat,
                                         u32 dev_id, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
void ubdrv_common_msg_send_ret_process(int ret, u32 dev_id, u32 type, struct ubdrv_common_msg_stat *stat);
int ubdrv_comm_msg_send_check(u32 dev_id, void *data, u32 *real_out_len);
#endif