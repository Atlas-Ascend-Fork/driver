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
#ifndef _ASDRV_UB_MSG_H_
#define _ASDRV_UB_MSG_H_

#include "asdrv_ub_jetty.h"
#include "asdrv_ub_common.h"
#include "asdrv_ub_msg_def.h"

#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA)
#define UBDRV_WAIT_JFC_MIN 1000
#define UBDRV_WAIT_JFC_MAX 1001
#else
#define UBDRV_WAIT_JFC_MIN 1
#define UBDRV_WAIT_JFC_MAX 2
#endif

#define ASCEND_MSG_NORMAL_RETRY_CNT 1
#define ASCEND_MSG_MAX_RETRY_CNT 35
#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA)
#define MSG_MAX_WAIT_CNT 100000U // 100000 * 1000us = 100s
#else
#define MSG_MAX_WAIT_CNT 15000000U // 15000000 * 1us = 15s
#endif
#define MSG_MAX_WAIT_CNT_BEFORE_ONLINE 290000000U // 290000000 * 1us = 290s
#define MSG_RX_USER_CNT_WAIT_CNT 5000000U         // 5s

#define UBDRV_DEFAULT_JETTY_ID 0
#define UBDRV_NON_TRANS_CHAN_CNT 32
#define UBDRV_MSG_TYPE_DEFAULT 0
#define UBDRV_ALLOC_SEG_TRY_CNT 3000
#define UBDRV_ALLOC_SEG_WAIT_PER_US 1000

// =============================================================================
// @struct ascend_ub_msg_status
// @compatibility: link msg version 1.0, UB_MSG_CHECK_VERSION_FAILED must fix 207
//                 add/del must be made after this
// @review_required: module owner and se
// =============================================================================
enum ascend_ub_msg_status {
    UB_MSG_INIT = 200,
    UB_MSG_IDLE,
    UB_MSG_SENDING,
    UB_MSG_INVALID_VALUE,
    UB_MSG_NULL_PROCESS,
    UB_MSG_PROCESS_FAILED,
    UB_MSG_PROCESS_SUCCESS,
    UB_MSG_CHECK_VERSION_FAILED,
    UB_MSG_REPLY_FAILED,
    UB_MSG_RECV_ABORT,
    UB_MSG_STATUS_MAX
};

#pragma pack(4)
struct ascend_ub_user_data {
    u32 opcode;
    u32 size;
    u32 reply_size;
    u32 msg_type;
    void *cmd;
    void *reply;
};
#pragma pack()

int ubdrv_msg_ret_cal(int ret, int cr_status);
void ubdrv_wait_chan_jfce_user_cnt(ka_atomic_t *user_cnt, u32 dev_id, u32 chan_id);
int ubdrv_copy_user_send(struct ascend_ub_msg_desc *desc, char *data, u32 len, u32 max_len);
int ubdrv_prepare_recv_data(struct ascend_ub_msg_desc *desc, struct ascend_ub_user_data *data, u32 msg_num,
                            u32 max_len);
int ubdrv_copy_rqe_data_to_user(struct ascend_ub_msg_desc *rqe_desc, struct ascend_ub_user_data *data,
                                struct ubdrv_msg_chan_stat *stat, u32 rqe_len);
struct ascend_ub_msg_desc *ubdrv_alloc_sync_send_seg(struct ascend_ub_sync_jetty *sync_jetty, u32 dev_id, u32 msg_num,
                                                     u32 try_cnt, u32 wait_per_us);
void ubdrv_free_sync_send_seg(struct ascend_ub_sync_jetty *sync_jetty, struct ascend_ub_msg_desc *desc);
int ubdrv_msg_send_data_prepare(struct ascend_ub_msg_desc *desc, u32 chan_len, struct ascend_ub_user_data *user_data);
void ubdrv_msg_send_wqe_prepare(struct send_wr_cfg *wr_cfg, struct ascend_ub_jetty_ctrl *send_jetty,
                                struct ascend_ub_msg_desc *desc, struct ubcore_tjetty *tjetty, u32 size);
void ubdrv_record_chan_jetty_info(struct ubdrv_msg_chan_stat *stat, struct ascend_ub_sync_jetty *sync_jetty);
void ubdrv_clear_chan_dfx(struct ubdrv_msg_chan_stat *stat);
void ubdrv_recv_msg_call_process(struct ascend_ub_jetty_ctrl *cfg, struct ascend_ub_msg_desc *desc, u32 seg_id);
struct ascend_ub_msg_desc *ubdrv_copy_msg_from_rqe_to_chan(struct ascend_ub_jetty_ctrl *cfg,
                                                           struct ascend_ub_msg_desc *src_desc, u32 seg_id);
int ubdrv_send_reply_msg(u32 dev_id, u32 chan_id, struct send_wr_cfg *wr_cfg, struct ascend_ub_jetty_ctrl *jetty_ctrl,
                         struct ubdrv_msg_chan_stat *stat);
int ubdrv_rebuild_chan_send_jetty(u32 dev_id, u32 chan_id, struct ubdrv_msg_chan_stat *stat,
                                  struct ascend_ub_jetty_ctrl *jetty_ctrl, struct send_wr_cfg *wr_cfg);
int ubdrv_repair_chan_send_jetty(u32 dev_id, u32 chan_id, struct ubdrv_msg_chan_stat *stat,
                                 struct ascend_ub_jetty_ctrl *jetty_ctrl, struct send_wr_cfg *wr_cfg);
int ubdrv_msg_result_process(int ret, int peer_status, u32 msg_type);
void ubdrv_msg_tx_dfx_info(struct ubdrv_msg_chan_stat *stat);
void ubdrv_record_common_prof_time(u64 *time);
void ubdrv_msg_chan_tx_prof_info(u32 dev_id, u32 chan_id, u32 msg_num, struct ubdrv_msg_chan_stat *stat);
void ubdrv_msg_chan_rx_prof_info(u32 dev_id, u32 chan_id, u32 msg_num, struct ubdrv_msg_chan_stat *stat);
void ubdrv_urma_chan_tx_prof_info(u32 dev_id, enum devdrv_urma_chan_type type, enum ubcore_opcode, u32 len,
                                  struct ubdrv_msg_chan_stat *stat);
int ubdrv_get_non_trans_chan_capacity(void *msg_chan, u32 *len);
#endif