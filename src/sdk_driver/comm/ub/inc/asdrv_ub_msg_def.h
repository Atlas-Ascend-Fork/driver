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

#ifndef _ASDRV_UB_MSG_DEF_H_
#define _ASDRV_UB_MSG_DEF_H_

#include "asdrv_ub_jetty.h"

enum ubdrv_chan_state {
    UBDRV_CHAN_IDLE,
    UBDRV_CHAN_OCCUPY,
    UBDRV_CHAN_ENABLE,
    UBDRV_CHAN_DISABLE,
    UBDRV_CHAN_STATE_MAX
};

struct ascend_ub_admin_chan {
    ka_mutex_t mutex_lock;
    ka_atomic_t msg_num;
    ka_atomic_t user_cnt;
    u32 valid;
    u32 dev_id;
    struct ascend_ub_msg_dev *msg_dev;
    struct ascend_ub_msg_desc *msg_desc;
    struct ascend_ub_sync_jetty *admin_jetty;
    struct ubcore_tjetty *tjetty; // peer
    struct ubdrv_msg_chan_stat chan_stat;
    ka_workqueue_struct_t *work_queue;
    struct ubdrv_sync_jfce_work recv_work;
};

struct ubdrv_non_trans_chan {
    ka_atomic_t user_cnt;
    ka_atomic_t msg_num;
    struct ascend_ub_msg_dev *msg_dev;
    struct ascend_ub_msg_desc *msg_desc;
    struct ascend_ub_sync_jetty msg_jetty; // local send & recv jetty
    struct ubcore_tjetty *s_tjetty;        // remote send target jetty
    struct ubcore_tjetty *r_tjetty;        // remote recv target jetty
    struct ubcore_target_seg *s_tseg;      // remote send_segment of send_jetty
    ka_workqueue_struct_t *work_queue;
    struct ubdrv_sync_jfce_work non_trans_r_work;
    u32 chan_id;
    u32 dev_id;
    enum ubdrv_chan_state status;
    u32 msg_type; /* enum devdrv_msg_client_type */
    int (*rx_msg_process)(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
    ka_mutex_t tx_mutex;
    void *priv;
    struct ubdrv_msg_chan_stat chan_stat; // non_trans dfx
    ubcore_comp_callback_t send_jfce_handler;
    ubcore_comp_callback_t recv_jfce_handler;
    enum ubdrv_msg_chan_mode chan_mode;
    struct ubdrv_rao_info rao_info;
};

struct ubdrv_urma_chan {
    u32 dev_id;
    u32 chan_id;
    struct ascend_ub_msg_dev *msg_dev;
    struct ascend_ub_jetty_ctrl send_jetty;
    struct ubcore_tjetty *tjetty;
    enum ubdrv_chan_state status;
    enum ubdrv_msg_chan_mode chan_mode;
    struct ubdrv_msg_chan_stat stat;
    ka_mutex_t tx_mutex;
};
#endif