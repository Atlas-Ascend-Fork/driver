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
#include "ka_compiler_pub.h"
#include "ubcore_opcode.h"
#include "asdrv_ub_admin_msg.h"
#include "asdrv_ub_link_chan.h"
#include "asdrv_ub_non_trans_chan.h"
#include "asdrv_ub_proc_fs.h"
#include "asdrv_ub_main.h"

void ubdrv_wait_chan_jfce_user_cnt(ka_atomic_t *user_cnt, u32 dev_id, u32 chan_id)
{
    int i;

    for (i = 0; i < MSG_RX_USER_CNT_WAIT_CNT; i++) {
        if (ka_base_atomic_read(user_cnt) == 0) {
            return;
        }
        ka_system_usleep_range(1, 2);
    }
    ubdrv_warn("Wait msg chan jfce_recv user cnt timeout. (chan_id=%u;dev_id=%u)\n", chan_id, dev_id);
}

int ubdrv_copy_user_send(struct ascend_ub_msg_desc *desc, char *data, u32 len, u32 max_len)
{
    if (len > max_len) {
        return -EDOM;
    }
    return memcpy_s(&(desc->user_data), len, data, len);
}

int ubdrv_prepare_recv_data(struct ascend_ub_msg_desc *desc, struct ascend_ub_user_data *data, u32 msg_num, u32 max_len)
{
    u32 real_reply_size = desc->real_data_len;

    if (msg_num != desc->msg_num) {
        ubdrv_err("Msg num check failed. (seg_id=%u;msg_num=%u;real_msg_num=%u)\n", desc->seg_id, msg_num,
                  desc->msg_num);
        return -EAGAIN;
    }
    if ((data->reply == NULL) || (real_reply_size == 0)) {
        return 0;
    } else {
        if ((real_reply_size > data->reply_size) || (real_reply_size > max_len)) {
            ubdrv_err("Reply data len invalid. (reply_size=%u;real_reply_size=%u)\n", data->reply_size,
                      real_reply_size);
            return -EINVAL;
        }
        return memcpy_s(data->reply, data->reply_size, &(desc->user_data), real_reply_size);
    }
}

int ubdrv_copy_rqe_data_to_user(struct ascend_ub_msg_desc *rqe_desc, struct ascend_ub_user_data *data,
                                struct ubdrv_msg_chan_stat *stat, u32 rqe_len)
{
    u32 real_reply_size = rqe_desc->real_data_len;
    u32 max_len;

    if ((data->reply == NULL) || (real_reply_size == 0)) {
        return 0;
    }
    if (rqe_len <= ASCEND_UB_MSG_DESC_LEN) {
        ubdrv_err("Rqe len invalid. (rqe_len=%u)\n", rqe_len);
        stat->tx_recv_data_err++;
        return -EINVAL;
    }
    max_len = (rqe_len - ASCEND_UB_MSG_DESC_LEN);
    if ((real_reply_size > data->reply_size) || (real_reply_size > max_len)) {
        ubdrv_err("Reply data len invalid. (expect_reply_size=%u;real_reply_size=%u;max_len=%u)\n", data->reply_size,
                  real_reply_size, max_len);
        stat->tx_recv_data_err++;
        return -EINVAL;
    }
    return memcpy_s(data->reply, data->reply_size, rqe_desc->user_data, real_reply_size);
}

struct ascend_ub_msg_desc *ubdrv_alloc_sync_send_seg(struct ascend_ub_sync_jetty *sync_jetty, u32 dev_id, u32 msg_num,
                                                     u32 try_cnt, u32 wait_per_us)
{
    u32 i;
    struct ascend_ub_jetty_ctrl *send_jetty;
    struct ascend_ub_msg_desc *desc = NULL;
    struct ascend_ub_msg_desc *tmp_desc = NULL;
    u32 seg_num;

retry:
    if (ka_unlikely(ubdrv_get_device_status(dev_id) == UBDRV_DEVICE_DEAD)) {
        ubdrv_warn("Device status is dead. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    try_cnt--;
    send_jetty = &(sync_jetty->send_jetty);
    seg_num = send_jetty->jfs_msg_depth;
    ka_task_mutex_lock(&sync_jetty->mutex_lock);
    for (i = 0; i < seg_num; ++i) {
        desc = ubdrv_get_send_desc(send_jetty, i);
        if (desc->time_out == UB_MSG_TIME_OUT) {
            tmp_desc = desc;
            continue;
        }
        if ((desc->status == UB_MSG_INIT) || (desc->status == UB_MSG_IDLE)) {
            desc->status = UB_MSG_SENDING;
            desc->msg_num = msg_num;
            ka_task_mutex_unlock(&sync_jetty->mutex_lock);
            return desc;
        }
    }
    if (tmp_desc == NULL) {
        ka_task_mutex_unlock(&sync_jetty->mutex_lock);
        if (try_cnt > 0) {
            ka_system_usleep_range(wait_per_us, wait_per_us);
            goto retry;
        }
        return NULL;
    }
    tmp_desc->time_out = UB_MSG_TIME_NORMAL;
    tmp_desc->status = UB_MSG_SENDING;
    tmp_desc->msg_num = msg_num;
    ka_task_mutex_unlock(&sync_jetty->mutex_lock);
    ubdrv_warn("Will use a timeout(expired) segment. (msg_num=%u;dev_id=%u)", msg_num, dev_id);
    return tmp_desc;
}

void ubdrv_free_sync_send_seg(struct ascend_ub_sync_jetty *sync_jetty, struct ascend_ub_msg_desc *desc)
{
    ka_task_mutex_lock(&sync_jetty->mutex_lock);
    desc->status = UB_MSG_IDLE;
    ka_task_mutex_unlock(&sync_jetty->mutex_lock);
    return;
}

int ubdrv_msg_send_data_prepare(struct ascend_ub_msg_desc *desc, u32 chan_len, struct ascend_ub_user_data *user_data)
{
    u32 user_max_len = (chan_len > ASCEND_UB_MAX_SEND_LIMIT ? ASCEND_UB_MAX_SEND_LIMIT : chan_len) -
                       ASCEND_UB_MSG_DESC_LEN;
    int ret = 0;

    if (user_data->size > user_max_len) {
        ubdrv_err("Send msg over len. (real_size=%u;max_size=%u)\n", user_data->size, user_max_len);
        return -EINVAL;
    }
    desc->opcode = user_data->opcode;        // Used only in admin msg
    desc->client_type = user_data->msg_type; // Used only in common msg
    desc->in_data_len = user_data->size;
    desc->out_data_len = user_data->reply_size;
    if ((user_data->size != 0) && (user_data->cmd != NULL)) {
        ret = ubdrv_copy_user_send(desc, user_data->cmd, user_data->size, user_max_len);
        if (ret != 0) {
            ubdrv_err("ubdrv_copy_user_send failed. (ret=%d)\n", ret);
            return ret;
        }
    }
    return 0;
}

void ubdrv_msg_send_wqe_prepare(struct send_wr_cfg *wr_cfg, struct ascend_ub_jetty_ctrl *send_jetty,
                                struct ascend_ub_msg_desc *desc, struct ubcore_tjetty *tjetty, u32 size)
{
    wr_cfg->user_ctx = desc->seg_id;
    wr_cfg->tjetty = tjetty;
    wr_cfg->sva = (u64)(uintptr_t)desc;
    wr_cfg->sseg = send_jetty->send_seg;
    wr_cfg->tseg = NULL; // Not used in send opcode
    wr_cfg->jfs = send_jetty->jfs;
    wr_cfg->len = size;
    return;
}

void ubdrv_record_chan_jetty_info(struct ubdrv_msg_chan_stat *stat, struct ascend_ub_sync_jetty *sync_jetty)
{
    struct ascend_ub_jetty_ctrl *send_jetty = &sync_jetty->send_jetty;
    struct ascend_ub_jetty_ctrl *recv_jetty = &sync_jetty->recv_jetty;

    stat->tx_jfr_jfc_id = send_jetty->jfr_jfc->id;
    stat->tx_jfs_jfc_id = send_jetty->jfs_jfc->id;
    stat->tx_jfr_id = send_jetty->jfr->jfr_id.id;
    stat->tx_jfs_id = send_jetty->jfs->jfs_id.id;
    stat->rx_jfs_jfc_id = recv_jetty->jfs_jfc->id;
    stat->rx_jfr_jfc_id = recv_jetty->jfr_jfc->id;
    stat->rx_jfr_id = recv_jetty->jfr->jfr_id.id;
    stat->rx_jfs_id = recv_jetty->jfs->jfs_id.id;
    return;
}

void ubdrv_clear_chan_dfx(struct ubdrv_msg_chan_stat *stat)
{
    size_t len = sizeof(struct ubdrv_msg_chan_stat);

    (void)memset_s(stat, len, 0, len);
    return;
}

void ubdrv_recv_msg_call_process(struct ascend_ub_jetty_ctrl *cfg, struct ascend_ub_msg_desc *desc, u32 seg_id)
{
    if (cfg->chan_mode == UBDRV_MSG_CHAN_FOR_ADMIN) {
        ubdrv_admin_recv_prepare_process(cfg, desc, seg_id);
    } else if (cfg->chan_mode == UBDRV_MSG_CHAN_FOR_LINK) {
        ubdrv_link_chan_recv_prepare_process(cfg, desc, seg_id);
    } else if (cfg->chan_mode == UBDRV_MSG_CHAN_FOR_NON_TRANS) {
        ubdrv_non_trans_recv_prepare_process(cfg, desc, seg_id);
    } else {
        ubdrv_err("Invalid chan mode. (chan_mode=%d)\n", cfg->chan_mode);
    }
}

struct ascend_ub_msg_desc *ubdrv_copy_msg_from_rqe_to_chan(struct ascend_ub_jetty_ctrl *cfg,
                                                           struct ascend_ub_msg_desc *src_desc, u32 seg_id)
{
    struct ubdrv_non_trans_chan *chan;
    struct ascend_ub_admin_chan *admin_chan;
    struct ascend_ub_msg_desc *dst_desc;
    u32 dev_id = 0, chan_id = 0;
    int ret;

    if ((cfg->chan_mode == UBDRV_MSG_CHAN_FOR_NON_TRANS) || ((cfg->chan_mode == UBDRV_MSG_CHAN_FOR_RAO))) {
        chan = cfg->msg_chan;
        dst_desc = chan->msg_desc;
        dev_id = chan->dev_id;
        chan_id = chan->chan_id;
    } else {
        admin_chan = cfg->msg_chan;
        dst_desc = admin_chan->msg_desc;
        dev_id = admin_chan->dev_id;
        chan_id = (u32)-1;
    }
    if (dst_desc->msg_num == src_desc->msg_num) {
        ubdrv_err("Recv repeat msg. (seg_id=%u;dev_id=%u;chan_id=%u;msg_num=%u)\n", seg_id, dev_id, chan_id,
                  dst_desc->msg_num);
        return NULL;
    }
    ret = memcpy_s(dst_desc, cfg->recv_msg_len, src_desc, cfg->recv_msg_len);
    if (ret != 0) {
        ubdrv_err("Copy msg from rqe to chan fail. (seg_id=%u;dev_id=%u;chan_id=%u;ret=%d)\n", seg_id, dev_id, chan_id,
                  ret);
        return NULL;
    }
    return dst_desc;
}

int ubdrv_rebuild_chan_send_jetty(u32 dev_id, u32 chan_id, struct ubdrv_msg_chan_stat *stat,
                                  struct ascend_ub_jetty_ctrl *jetty_ctrl, struct send_wr_cfg *wr_cfg)
{
    int ret;

    stat->tx_rebuild++;
    ret = ubdrv_rebuild_jfs_jfc(dev_id, jetty_ctrl);
    ubdrv_warn("Enter send jfs rebuild. (chan_id=%u;dev_id=%u;chan_mode=%u;dev_name=%s;ret=%d)\n", chan_id, dev_id,
               jetty_ctrl->chan_mode, jetty_ctrl->ubc_dev->dev_name, ret);
    if (ret != 0) {
        wr_cfg->jfs = NULL;
        return ret;
    }
    stat->tx_jfs_id = jetty_ctrl->jfs->jfs_id.id;
    stat->tx_jfs_jfc_id = jetty_ctrl->jfs_jfc->id;
    wr_cfg->jfs = jetty_ctrl->jfs;
    return 0;
}

int ubdrv_repair_chan_send_jetty(u32 dev_id, u32 chan_id, struct ubdrv_msg_chan_stat *stat,
                                 struct ascend_ub_jetty_ctrl *jetty_ctrl, struct send_wr_cfg *wr_cfg)
{
    if ((jetty_ctrl->jfs != NULL) && (jetty_ctrl->jfs_jfc != NULL)) {
        return 0;
    }
    return ubdrv_rebuild_chan_send_jetty(dev_id, chan_id, stat, jetty_ctrl, wr_cfg);
}

STATIC int ubdrv_rebuild_chan_recv_jetty(u32 dev_id, u32 chan_id, struct ubdrv_msg_chan_stat *stat,
                                         struct ascend_ub_jetty_ctrl *jetty_ctrl, struct send_wr_cfg *wr_cfg)
{
    int ret;

    stat->rx_rebuild++;
    ret = ubdrv_rebuild_jfs_jfc(dev_id, jetty_ctrl);
    ubdrv_warn("Enter recv jfs rebuild(chan_id=%u;dev_id=%u;chan_mode=%u;dev_name=%s;ret=%d)\n", chan_id, dev_id,
               jetty_ctrl->chan_mode, jetty_ctrl->ubc_dev->dev_name, ret);
    if (ret != 0) {
        wr_cfg->jfs = NULL;
        return ret;
    }
    stat->rx_jfs_id = jetty_ctrl->jfs->jfs_id.id;
    stat->rx_jfs_jfc_id = jetty_ctrl->jfs_jfc->id;
    wr_cfg->jfs = jetty_ctrl->jfs;
    return 0;
}

int ubdrv_msg_ret_cal(int ret, int cr_status)
{
    if (ret != 0) {
        return ret;
    } else if (cr_status != UBCORE_CR_SUCCESS) {
        return -EUBOPFAIL;
    } else {
        return 0;
    }
}

int ubdrv_send_reply_msg(u32 dev_id, u32 chan_id, struct send_wr_cfg *wr_cfg, struct ascend_ub_jetty_ctrl *jetty_ctrl,
                         struct ubdrv_msg_chan_stat *stat)
{
    struct ubcore_cr cr = {0};
    u32 retry_cnt = 0;
    int ret;

    if ((jetty_ctrl->jfs == NULL) || (jetty_ctrl->jfs_jfc == NULL)) {
        ret = ubdrv_rebuild_chan_recv_jetty(dev_id, chan_id, stat, jetty_ctrl, wr_cfg);
        if (ret != 0) {
            return ret;
        }
    }
retry_recv_jfs_send:
    ret = ubdrv_post_send_wr(wr_cfg, dev_id);
    if (ret != 0) {
        ubdrv_err("Send reply result data failed. (ret=%d;dev_id=%u)", ret, dev_id);
        stat->rx_tx_post_err++;
        return ret;
    }

    ret = ubdrv_interval_poll_recv_jfs_jfc(jetty_ctrl, (u64)wr_cfg->user_ctx, MSG_MAX_WAIT_CNT, &cr, stat);
    if ((ret == 0) && (retry_cnt < ubdrv_get_msg_retry_cnt(dev_id)) && (cr.status == UBCORE_CR_ACK_TIMEOUT_ERR)) {
        retry_cnt++;
        (void)ubdrv_rebuild_chan_recv_jetty(dev_id, chan_id, stat, jetty_ctrl, wr_cfg);
        goto retry_recv_jfs_send;
    }
    return ubdrv_msg_ret_cal(ret, cr.status);
}

int ubdrv_msg_result_process(int ret, int peer_status, u32 msg_type)
{
    if (ret != 0) {
        return ret;
    }
    if (peer_status == UB_MSG_PROCESS_SUCCESS) {
        return 0;
    } else if (peer_status == UB_MSG_PROCESS_FAILED) {
        ubdrv_warn("Msg send finish, process unsuccessful, Please check peer log. (process_ret=%d;msg_type=%u)\n",
                   peer_status, msg_type);
        return peer_status;
    } else if (peer_status == UB_MSG_NULL_PROCESS) {
        ubdrv_warn("Msg send finish, no process cb. (process_ret=%d;msg_type=%u)\n", peer_status, msg_type);
        return -EUNATCH;
    } else if (peer_status == UB_MSG_CHECK_VERSION_FAILED) {
        ubdrv_warn("Msg send finish, peer check version not match. (process_ret=%d;msg_type=%u)\n", peer_status,
                   msg_type);
        return peer_status;
    } else {
        ubdrv_warn("Msg send finish, invalid para. (process_ret=%d;msg_type=%u)\n", peer_status, msg_type);
        return -ETIMEDOUT;
    }
    return 0;
}

void ubdrv_msg_tx_dfx_info(struct ubdrv_msg_chan_stat *stat)
{
    ubdrv_err("Msg chan id info. (dev_id=%u;chan_id=%u;tx_jfr=%u,tx_jfr_jfc=%u;tx_jfs=%u;tx_jfs_jfc=%u;rx_jfr=%u;"
              "rx_jfr_jfc=%u;rx_jfs=%u;rx_jfs_jfc=%u)\n",
              stat->dev_id, stat->chan_id, stat->tx_jfr_id, stat->tx_jfr_jfc_id, stat->tx_jfs_id, stat->tx_jfs_jfc_id,
              stat->rx_jfr_id, stat->rx_jfr_jfc_id, stat->rx_jfs_id, stat->rx_jfs_jfc_id);
    ubdrv_err("Msg chan tx dfx info. (tx_total=%llu,post_err=%llu;cqe_cnt=%llu;cqe_timeout=%llu)\n", stat->tx_total,
              stat->tx_post_send_err, stat->tx_cqe, stat->tx_cqe_timeout);
    ubdrv_err("Msg chan tx dfx info. (poll_cqe_err=%llu,cqe_status_err=%llu;tx_rebuild=%llu;recv_cqe_timeout=%llu)\n",
              stat->tx_poll_cqe_err, stat->tx_cqe_status_err, stat->tx_rebuild, stat->tx_recv_cqe_timeout);
    ubdrv_err("Msg chan tx dfx info. (recv_cqe_cnt=%llu,recv_poll_cqe_err=%llu;recv_cqe_status_err=%llu;"
              "recv_data_err=%llu)\n",
              stat->tx_recv_cqe, stat->tx_recv_poll_cqe_err, stat->tx_recv_cqe_status_err, stat->tx_recv_data_err);
    return;
}

void ubdrv_record_common_prof_time(u64 *time)
{
    if (ka_unlikely(ubdrv_procfs_get_prof_state()) == true) {
        *time = ka_system_ktime_get_raw_ns();
    }
}

STATIC u32 ubdrv_cal_ktime_to_us(u64 start_jiffies, u64 finish_jiffies)
{
    return (finish_jiffies > start_jiffies) ? (finish_jiffies - start_jiffies) / KA_NSEC_PER_USEC : 0;
}

void ubdrv_msg_chan_tx_prof_info(u32 dev_id, u32 chan_id, u32 msg_num, struct ubdrv_msg_chan_stat *stat)
{
    u32 cost1, cost2, cost3, cost4;

    if (ka_likely(ubdrv_procfs_get_prof_state() == false)) {
        return;
    }
    cost4 = ubdrv_cal_ktime_to_us(stat->tx_recv_stamp, ka_system_ktime_get_raw_ns());
    cost3 = ubdrv_cal_ktime_to_us(stat->tx_wait_reply_stamp, stat->tx_recv_stamp);
    cost2 = ubdrv_cal_ktime_to_us(stat->tx_post_stamp, stat->tx_wait_reply_stamp);
    cost1 = ubdrv_cal_ktime_to_us(stat->tx_start_stamp, stat->tx_post_stamp);
    ubdrv_info("Chan tx prof time info. "
               "(dev_id=%u;chan_id=%u;msg_num=%u;tx_prepare=%uus;post_wqe=%uus;wait_reply=%uus;post_rqe=%uus)\n",
               dev_id, chan_id, msg_num, cost1, cost2, cost3, cost4);
}

void ubdrv_msg_chan_rx_prof_info(u32 dev_id, u32 chan_id, u32 msg_num, struct ubdrv_msg_chan_stat *stat)
{
    u32 cost1, cost2, cost3, cost4;

    if (ka_likely(ubdrv_procfs_get_prof_state() == false)) {
        return;
    }
    cost1 = stat->rx_work_max_time;                                     // rx_start to work run time
    cost2 = ubdrv_cal_ktime_to_us(stat->rx_work_stamp, stat->rx_stamp); // rx_start to call time
    cost3 = cost2 + stat->rx_max_time;                                  // rx_work to call finish time
    cost4 = ubdrv_cal_ktime_to_us(stat->rx_work_stamp,
                                  ka_system_ktime_get_raw_ns()); // rx_work to rx process finish time
    ubdrv_info("Chan rx prof time info. "
               "(dev_id=%u;chan_id=%u;msg_num=%u;work_sched=%uus;call_before=%uus;call_finish=%uus;call_cost=%uus;rx_"
               "finish=%uus)\n",
               dev_id, chan_id, msg_num, cost1, cost2, cost3, stat->rx_max_time, cost4);
}

void ubdrv_urma_chan_tx_prof_info(u32 dev_id, enum devdrv_urma_chan_type type, enum ubcore_opcode opcode, u32 len,
                                  struct ubdrv_msg_chan_stat *stat)
{
    u32 cost1, cost2;

    if (ka_likely(ubdrv_procfs_get_prof_state() == false)) {
        return;
    }
    cost2 = ubdrv_cal_ktime_to_us(stat->tx_post_stamp, stat->tx_recv_stamp);
    cost1 = ubdrv_cal_ktime_to_us(stat->tx_start_stamp, stat->tx_post_stamp);
    ubdrv_info("Urma copy chan tx prof time info. (dev_id=%u;type=%u;len=%u;opcode=%u;tx_prepare=%uus;post_wqe=%uus)\n",
               dev_id, (u32)type, len, (u32)opcode, cost1, cost2);
}

int ubdrv_get_non_trans_chan_capacity(void *msg_chan, u32 *len)
{
    struct ubdrv_non_trans_chan *chan;
    u32 chan_len, capacity_len;

    if ((msg_chan == NULL) || (len == NULL)) {
        ubdrv_err("Input len or msg_chan is NULL.\n");
        return -EINVAL;
    }

    chan = ubdrv_find_msg_chan_by_handle(msg_chan);
    if (chan == NULL) {
        ubdrv_err("Can not find chan.\n");
        return -EINVAL;
    }

    chan_len = chan->msg_jetty.send_jetty.send_msg_len;
    capacity_len = (chan_len > ASCEND_UB_MAX_SEND_LIMIT ? ASCEND_UB_MAX_SEND_LIMIT : chan_len) - ASCEND_UB_MSG_DESC_LEN;
    if (capacity_len > ASCEND_UB_MAX_SEND_LIMIT) {
        ubdrv_err("Chan len has problem.(chan_len=%u; desc_len=%u; send_limit_len=%u)\n", chan_len,
                  (u32)ASCEND_UB_MSG_DESC_LEN, (u32)ASCEND_UB_MAX_SEND_LIMIT);
        return -EINVAL;
    }
    *len = capacity_len;

    return 0;
}