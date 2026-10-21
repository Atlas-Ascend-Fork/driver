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
#ifndef _ASDRV_UB_ADMIN_MSG_H_
#define _ASDRV_UB_ADMIN_MSG_H_
#include "asdrv_ub_dev.h"

#define ADMIN_MSG_LEN 18
#define UB_ADMIN_MSG_ENABLE 1
#define UB_ADMIN_MSG_DISABLE 0

enum ubdrv_admin_msg_opcode {
    UBDRV_CREATE_MSG_QUEUE = 0,
    UBDRV_FREE_MSG_QUEUE,
    UBDRV_ENABLE_MSG_QUEUE,
    UBDRV_CREATE_URMA_CHAN,
    UBDRV_FREE_URMA_CHAN,
    UBDRV_VFE_ONLINE,
    UBDRV_VFE_OFFLINE,
    UBDRV_SYNC_RES_INFO,
    UBDRV_SYNC_DFX_INFO,
    UBDRV_HOST_HOT_RESET,
    UBDRV_GET_TOKEN_VAL,
    UBDRV_ADMIN_MSG_MAX
};

void ubdrv_print_exchange_data(struct ubdrv_jetty_exchange_data *data);
int ubdrv_create_admin_jetty(struct ub_idev *idev, u32 dev_id, u32 jfr_id);
void ubdrv_delete_admin_jetty(u32 dev_id);
int ubdrv_create_admin_msg_chan(u32 dev_id, struct ascend_ub_msg_dev *msg_dev);
void ubdrv_del_admin_msg_chan(struct ascend_ub_msg_dev *msg_dev, u32 dev_id);
int ubdrv_basic_chan_send(u32 dev_id, struct ascend_ub_user_data *data, struct ascend_ub_admin_chan *msg_chan);
int ubdrv_admin_send_msg(u32 dev_id, struct ascend_ub_user_data *data);
void ubdrv_admin_recv_prepare_process(struct ascend_ub_jetty_ctrl *cfg, struct ascend_ub_msg_desc *desc, u32 seg_id);
int ubdrv_admin_chan_import_jfr(struct ub_idev *idev, struct ascend_ub_admin_chan *msg_chan,
                                struct jetty_exchange_data *data, u32 dev_id, enum ubdrv_log_level log_level);
void ubdrv_admin_chan_unimport_jfr(struct ascend_ub_admin_chan *msg_chan, u32 dev_id);
int ubdrv_create_basic_jetty(struct ascend_ub_sync_jetty *jetty, u32 jfr_id);
void ubdrv_delete_basic_jetty(struct ascend_ub_sync_jetty *jetty);
int ubdrv_msg_get_token_val(struct ascend_ub_msg_dev *msg_dev, void *data);
#endif