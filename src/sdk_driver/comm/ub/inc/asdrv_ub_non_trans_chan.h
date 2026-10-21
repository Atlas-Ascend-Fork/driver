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

#ifndef _ASDRV_UB_NON_TRANS_CHAN_H_
#define _ASDRV_UB_NON_TRANS_CHAN_H_

#include "asdrv_ub_jetty.h"
#include "asdrv_ub_dev.h"
#include "asdrv_ub_main_adapt.h"

#define UBDRV_MSG_MAGIC 0x5a6b
#define UBDRV_NON_TRANS_CLIENT_ENABLE 1
#define UBDRV_NON_TRANS_CLIENT_DISABLE 0
#define UBDRV_NON_TRANS_MSG_DEFAULT_DESC_SIZE 0x1000

typedef union {
    /* Define the struct bits */
    struct {
        u64 dev_id : 16;   /* [15..0] */
        u64 chan_id : 16;  /* [31..16] */
        u64 magic : 16;    /* [47..32] */
        u64 reserved : 16; /* [63..48] */
    } bits;

    /* Define an u64 member */
    u64 value;
} UBDRV_MSG_HANDLE;

struct ubdrv_non_trans_msg_client_ctrl {
    u32 status;
    ka_mutex_t mutex_lock;
    struct agentdrv_non_trans_msg_client non_trans_msg_client;
};

bool ubdrv_lock_non_trans_client_by_type(u32 type);
void ubdrv_unlock_non_trans_client_by_type(u32 type);
void ubdrv_non_trans_msg_chan_init(struct ascend_ub_msg_dev *msg_dev);
void ubdrv_non_trans_msg_chan_uninit(struct ascend_ub_msg_dev *msg_dev);
void ubdrv_rao_msg_chan_init(struct ascend_ub_msg_dev *msg_dev);
void ubdrv_rao_msg_chan_uninit(struct ascend_ub_msg_dev *msg_dev);
void ubdrv_non_trans_jfce_recv_handle(struct ubcore_jfc *jfc);
void ubdrv_non_trans_recv_prepare_process(struct ascend_ub_jetty_ctrl *cfg, struct ascend_ub_msg_desc *desc,
                                          u32 seg_id);
void *ubdrv_get_non_trans_chan_handle(const struct ubdrv_non_trans_chan *chan);
int devdrv_sync_msg_send_inner(u32 dev_id, void *msg_chan, struct ascend_ub_user_data *user_data, u32 *real_out_len);
void ubdrv_sync_send_prepare_user_data(struct ascend_ub_user_data *user_data, void *data, u32 in_data_len,
                                       u32 out_data_len, u32 msg_type);

u32 ubdrv_get_devid_by_non_trans_handle(const void *chan);
void ubdrv_non_trans_client_ctrl_init(void);
void ubdrv_non_trans_client_ctrl_uninit(void);
struct ubdrv_non_trans_chan *ubdrv_find_msg_chan_by_handle(void *chan_handle);
int ubdrv_get_devid_by_chan_pointer(const struct ubdrv_non_trans_chan *chan, u32 *dev_id);
int ubdrv_get_msg_chan_devid(void *msg_chan);
void *ubdrv_get_msg_chan_priv(void *msg_chan);
int ubdrv_set_msg_chan_priv(void *msg_chan, void *priv);
void ubdrv_free_all_non_trans_chan(u32 dev_id);
int ubdrv_sync_send_check_param(void *msg_chan, void *data, u32 *real_out_len);
int ubdrv_prepare_non_trans_jetty_info(struct ubdrv_non_trans_chan *chan, struct ubdrv_create_non_trans_cmd *cmd);
int ubdrv_create_non_trans_jetty(struct ubcore_device *ubc_dev, struct ubdrv_non_trans_chan *chan,
                                 struct ubdrv_create_non_trans_cmd *cmd);
void ubdrv_delete_non_trans_jetty(struct ubdrv_non_trans_chan *chan);
void ubdrv_uninit_non_trans_chan(struct ubdrv_non_trans_chan *chan, u32 dev_id);
int ubdrv_init_non_trans_chan(struct ubdrv_non_trans_chan *chan, struct ubcore_device *ubc_dev,
                              struct ubdrv_create_non_trans_cmd *cmd, u32 dev_id, u32 msg_type);
#endif