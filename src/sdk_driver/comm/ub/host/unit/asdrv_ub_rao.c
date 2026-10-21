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
#include "asdrv_ub_non_trans_chan.h"
#include "asdrv_ub_main.h"
#include "asdrv_ub_rao.h"

void ubdrv_set_rao_chan_cmd_by_chan_info(struct ubdrv_create_non_trans_cmd *cmd, enum devdrv_rao_client_type type,
                                         u64 len)
{
    cmd->chan_id = type;
    cmd->sq_depth = 1;
    cmd->cq_depth = 1;
    cmd->sq_size = len;
    cmd->cq_size = len;
    cmd->msg_type = devdrv_msg_client_max;
    cmd->chan_mode = UBDRV_MSG_CHAN_FOR_RAO;
}

int ubdrv_register_rao_client(u32 dev_id, enum devdrv_rao_client_type type, u64 va, u64 len,
                              enum devdrv_rao_permission_type perm)
{
    struct ubdrv_create_non_trans_cmd cmd = {0};
    rao_client_ctrl_arr_ptr client_ctrl;
    struct ubdrv_non_trans_chan *chan;
    struct ascend_ub_msg_dev *msg_dev;
    int ret = 0;

    ret = ubdrv_register_rao_para_check(dev_id, type, va, len, perm);
    if (ret != 0) {
        return ret;
    }

    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    client_ctrl = get_global_rao_client_ctrl();
    ka_task_mutex_lock(&client_ctrl[dev_id][type].mutex_lock);
    if (client_ctrl[dev_id][type].status == UBDRV_RAO_CLIENT_ENABLE) {
        ka_task_mutex_unlock(&client_ctrl[dev_id][type].mutex_lock);
        ubdrv_err("RAO client type is already registered. (dev_id=%u; type=%d)\n", dev_id, type);
        return -EINVAL;
    }
    chan = &msg_dev->rao.rao_msg_chan[type];
    chan->rao_info.va = va;
    chan->rao_info.len = len;
    chan->rao_info.type = type;
    chan->send_jfce_handler = NULL;
    chan->recv_jfce_handler = ubdrv_non_trans_jfce_recv_handle;
    ubdrv_set_rao_chan_cmd_by_chan_info(&cmd, type, len);
    ret = devdrv_ub_msg_alloc_non_trans_queue_process(chan, &cmd);
    if (ret != 0) {
        ka_task_mutex_unlock(&client_ctrl[dev_id][type].mutex_lock);
        ubdrv_err("Host alloc RAO msg chan failed. (ret=%d; dev_id=%u; type=%d)\n", ret, dev_id, type);
        return ret;
    }

    client_ctrl[dev_id][type].status = UBDRV_RAO_CLIENT_ENABLE;
    ka_task_mutex_unlock(&client_ctrl[dev_id][type].mutex_lock);
    ubdrv_info("Host register RAO client. (dev_id=%u; type=%d; len=0x%llx; perm=%d)\n", dev_id, type, len, perm);

    return 0;
}

int ubdrv_unregister_rao_client(u32 dev_id, enum devdrv_rao_client_type type)
{
    struct ubdrv_free_non_trans_cmd cmd = {0};
    rao_client_ctrl_arr_ptr client_ctrl;
    struct ubdrv_non_trans_chan *chan;
    struct ascend_ub_msg_dev *msg_dev;
    int ret;

    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id, get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (type >= DEVDRV_RAO_CLIENT_MAX) {
        ubdrv_err("Invalid client type. (dev_id=%u; type=%d)\n", dev_id, type);
        return -EINVAL;
    }

    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u; type=%d)\n", dev_id, type);
        return -EINVAL;
    }

    client_ctrl = get_global_rao_client_ctrl();
    ka_task_mutex_lock(&client_ctrl[dev_id][type].mutex_lock);
    if (client_ctrl[dev_id][type].status == UBDRV_RAO_CLIENT_DISABLE) {
        ka_task_mutex_unlock(&client_ctrl[dev_id][type].mutex_lock);
        ubdrv_warn("RAO client type is already unregistered. (dev_id=%u; type=%d)\n", dev_id, type);
        return -EINVAL;
    }
    chan = &msg_dev->rao.rao_msg_chan[type];
    cmd.chan_id = type;
    cmd.chan_mode = UBDRV_MSG_CHAN_FOR_RAO;
    ret = devdrv_ub_msg_free_non_trans_queue_process(chan, &cmd);
    if (ret != 0) {
        ubdrv_err("Host free RAO msg chan failed. (dev_id=%u; type=%d; ret=%d; chan_id=%u)\n", chan->msg_dev->dev_id,
                  type, ret, chan->chan_id);
    } else {
        ubdrv_info("Host free RAO msg chan finish. (dev_id=%u; type=%d; chan_id=%u)\n", dev_id, type, chan->chan_id);
    }

    client_ctrl[dev_id][type].status = UBDRV_RAO_CLIENT_DISABLE;
    ka_task_mutex_unlock(&client_ctrl[dev_id][type].mutex_lock);

    ubdrv_info("Host unregister RAO client. ( dev_id=%u; type=%d)\n", dev_id, type);
    return ret;
}

/* release all rao chan on both the host and device sides by send admin msg, when exit */
void ubdrv_free_all_rao_chan(u32 dev_id)
{
    int i;

    for (i = 0; i < DEVDRV_RAO_CLIENT_MAX; i++) {
        (void)ubdrv_unregister_rao_client(dev_id, i);
    }
    return;
}

STATIC void ubdrv_rao_wqe_prepare(struct ubdrv_non_trans_chan *chan, struct send_wr_cfg *wr_cfg, u64 offset, u64 len,
                                  enum ubcore_opcode opcode)
{
    wr_cfg->user_ctx = 0;
    wr_cfg->jfs = chan->msg_jetty.send_jetty.jfs; // local send jetty
    wr_cfg->tjetty = chan->r_tjetty;              // remote recv jetty
    wr_cfg->len = len;
    if (opcode == UBCORE_OPC_READ) {
        wr_cfg->sseg = chan->s_tseg;                        // read src: remote
        wr_cfg->tseg = chan->msg_jetty.recv_jetty.recv_seg; // read dst: local
    } else {
        wr_cfg->sseg = chan->msg_jetty.send_jetty.send_seg; // write src: local
        wr_cfg->tseg = chan->s_tseg;                        // write dst: remote
    }
    wr_cfg->sva = wr_cfg->sseg->seg.ubva.va + offset; // read src: remote write src: local
    wr_cfg->dva = wr_cfg->tseg->seg.ubva.va + offset; // read dst: local write dst: remote
}

int ubdrv_rao_read_para_check(u32 dev_id, enum devdrv_rao_client_type type, u64 offset, u64 len)
{
    struct ubdrv_non_trans_chan *chan;
    struct ascend_ub_msg_dev *msg_dev;

    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (type >= DEVDRV_RAO_CLIENT_MAX) {
        ubdrv_err("Invalid client type. (dev_id=%u; type=%d)\n", dev_id, type);
        return -EINVAL;
    }

    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u; type=%d)\n", dev_id, type);
        return -EINVAL;
    }
    chan = &msg_dev->rao.rao_msg_chan[type];
    if (len > KA_U32_MAX) {
        ubdrv_err("Len exceeds u32 max. (dev_id=%u; type=%d; len=0x%llx)\n", dev_id, type, len);
        return -EINVAL;
    }
    if ((offset >= chan->rao_info.len) || (len == 0) || (len > chan->rao_info.len) ||
        (offset > chan->rao_info.len - len)) {
        ubdrv_err("Invalid offset or len. (dev_id=%u; type=%d; offset=0x%llx; len=0x%llx)\n", dev_id, type, offset,
                  len);
        return -EINVAL;
    }

    return 0;
}

STATIC int ubdrv_rao_prepare_send(struct ubdrv_non_trans_chan *chan, struct send_wr_cfg *wr_cfg,
                                  struct ubdrv_rao_operate *operate)
{
    struct ascend_ub_jetty_ctrl *send_jetty = &chan->msg_jetty.send_jetty;
    struct ubdrv_msg_chan_stat *stat = &chan->chan_stat;
    int ret;

    ret = ubdrv_repair_chan_send_jetty(chan->dev_id, chan->chan_id, stat, send_jetty, wr_cfg);
    if (ret != 0) {
        return ret;
    }
    ubdrv_rao_wqe_prepare(chan, wr_cfg, operate->offset, operate->len, operate->opcode);
    return 0;
}

STATIC int ubdrv_rao_opcode_process(u32 dev_id, struct ubdrv_non_trans_chan *chan, struct ubdrv_rao_operate *operate)
{
    enum devdrv_rao_client_type type = operate->type;
    struct ubdrv_msg_chan_stat *stat = NULL;
    struct send_wr_cfg wr_cfg = {0};
    struct ubcore_cr cr = {0};
    bool check_status = false;
    int ta_timeout_cnt = 0;
    int ret = 0;

    ret = ubdrv_rao_prepare_send(chan, &wr_cfg, operate);
    if (ret != 0) {
        return ret;
    }
    stat = &chan->chan_stat;
    stat->tx_total++;
    check_status = (chan->chan_id == DEVDRV_RAO_CLIENT_DEVMNG ? true : false);
rao_msg_tatimeout:
    ret = ubdrv_post_rw_wr_process(&wr_cfg, operate->opcode);
    if (ret != 0) {
        stat->tx_post_send_err++;
        ubdrv_err("Post send wr failed. (ret=%d;dev_id=%u;chan_type=%u)\n", ret, dev_id, type);
        return ret;
    }
    ret = ubdrv_interval_poll_send_jfs_jfc(&chan->msg_jetty.send_jetty, (u64)wr_cfg.user_ctx, MSG_MAX_WAIT_CNT, &cr,
                                           stat, check_status);
    if ((ret == 0) && (cr.status == UBCORE_CR_ACK_TIMEOUT_ERR) && (ta_timeout_cnt < ubdrv_get_msg_retry_cnt(dev_id))) {
        (void)ubdrv_rebuild_chan_send_jetty(dev_id, chan->chan_id, stat, &chan->msg_jetty.send_jetty, &wr_cfg);
        ta_timeout_cnt++;
        goto rao_msg_tatimeout;
    } else if ((ret != 0) || (cr.status != UBCORE_CR_SUCCESS)) {
        ubdrv_warn("Rao chan send was unsuccessful. (ret=%d;cr_status=%d;dev_id=%u;chan_id=%u)\n", ret, cr.status,
                   dev_id, chan->chan_id);
        ret = ubdrv_msg_ret_cal(ret, cr.status);
    }

    return ret;
}

STATIC void ubdrv_rao_operate_pack(struct ubdrv_rao_operate *operate, enum devdrv_rao_client_type type,
                                   enum ubcore_opcode opcode, u64 offset, u64 len)
{
    operate->type = type;
    operate->opcode = opcode;
    operate->len = len;
    operate->offset = offset;
    return;
}

STATIC int ubdrv_rao_chan_process(u32 dev_id, enum devdrv_rao_client_type type, enum ubcore_opcode opcode, u64 offset,
                                  u64 len)
{
    struct ubdrv_non_trans_chan *chan = NULL;
    struct ascend_ub_msg_dev *msg_dev = NULL;
    struct ubdrv_rao_operate operate = {0};
    struct ascend_dev *asd_dev = NULL;
    int ret;

    asd_dev = ubdrv_get_asd_dev_by_devid(dev_id);
    if (asd_dev == NULL) {
        return -EINVAL;
    }
    ka_task_down_read(&asd_dev->rw_sem);
    ret = ubdrv_rao_read_para_check(dev_id, type, offset, len);
    if (ret != 0) {
        goto asd_up_read;
    }
    ubdrv_rao_operate_pack(&operate, type, opcode, offset, len);
    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    chan = &msg_dev->rao.rao_msg_chan[type];
    ubdrv_mutex_lock_polling(dev_id, &chan->tx_mutex);
    if (chan->status != UBDRV_CHAN_ENABLE) {
        ubdrv_err("Chan is invalid. (status=%u; dev_id=%u; chan_type=%u)\n", chan->status, dev_id, type);
        ret = -EINVAL;
        goto chan_unlock;
    }
    ret = ubdrv_rao_opcode_process(dev_id, chan, &operate);

chan_unlock:
    ka_task_mutex_unlock(&chan->tx_mutex);
asd_up_read:
    ka_task_up_read(&asd_dev->rw_sem);
    return ret;
}

int ubdrv_rao_read(u32 dev_id, enum devdrv_rao_client_type type, u64 offset, u64 len)
{
    return ubdrv_rao_chan_process(dev_id, type, UBCORE_OPC_READ, offset, len);
}

int ubdrv_rao_write(u32 dev_id, enum devdrv_rao_client_type type, u64 offset, u64 len)
{
    return ubdrv_rao_chan_process(dev_id, type, UBCORE_OPC_WRITE, offset, len);
}