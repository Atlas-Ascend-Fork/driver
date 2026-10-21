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
#include "asdrv_ub_main.h"
#include "asdrv_ub_admin_msg.h"
#include "asdrv_ub_non_trans_chan.h"

bool ubdrv_lock_non_trans_client_by_type(u32 type)
{
    if (type >= devdrv_msg_client_max) {
        ubdrv_err("Invalid client type. (client_type=%u)\n", type);
        return false;
    }

    return true;
}

void ubdrv_unlock_non_trans_client_by_type(u32 type)
{
    return;
}

struct ubdrv_non_trans_chan *ubdrv_get_non_trans_chan(struct ascend_ub_msg_dev *msg_dev)
{
    struct ubdrv_non_trans_chan *chan = NULL;
    u32 i;

    for (i = 0; i < msg_dev->chan_cnt; ++i) {
        chan = &msg_dev->non_trans_chan[i];
        ka_task_mutex_lock(&chan->tx_mutex);
        if (chan->status == UBDRV_CHAN_IDLE) {
            chan->status = UBDRV_CHAN_OCCUPY;
            ka_task_mutex_unlock(&chan->tx_mutex);
            break;
        }
        ka_task_mutex_unlock(&chan->tx_mutex);
    }
    if (i >= msg_dev->chan_cnt) {
        ubdrv_err("Can't find a idle non_trans chan. (dev_id=%u)\n", msg_dev->dev_id);
        return NULL;
    }
    return chan;
}

STATIC void ubdrv_put_non_trans_chan(struct ubdrv_non_trans_chan *chan)
{
    ka_task_mutex_lock(&chan->tx_mutex);
    chan->status = UBDRV_CHAN_IDLE;
    ka_task_mutex_unlock(&chan->tx_mutex);
}

STATIC int ubdrv_prepare_non_trans_alloc_data(struct ascend_ub_user_data *user_desc, struct ubdrv_non_trans_chan *chan,
                                              struct ubdrv_create_non_trans_cmd *cmd, u32 dev_id)
{
    u32 len = sizeof(struct ubdrv_create_non_trans_cmd);
    int ret;

    ret = ubdrv_prepare_non_trans_jetty_info(chan, cmd);
    if (ret != 0) {
        ubdrv_err("Prepare non trans jetty info failed. (chan_id=%u;dev_id=%u)\n", chan->chan_id, dev_id);
        return ret;
    }
    user_desc->opcode = UBDRV_CREATE_MSG_QUEUE;
    user_desc->size = len;
    user_desc->reply_size = len;
    user_desc->cmd = cmd;
    user_desc->reply = cmd;
    return 0;
}

STATIC void ubdrv_set_non_trans_cmd_by_chan_info(struct ubdrv_create_non_trans_cmd *cmd,
                                                 struct devdrv_non_trans_msg_chan_info *chan_info, u32 chan_id)
{
    cmd->chan_id = chan_id;
    cmd->sq_depth = UB_NON_TRANS_DEFAULT_DEPTH;
    cmd->cq_depth = UB_NON_TRANS_DEFAULT_DEPTH;
    cmd->sq_size = chan_info->s_desc_size;
    cmd->cq_size = chan_info->c_desc_size;
    cmd->msg_type = chan_info->msg_type;
    cmd->chan_mode = UBDRV_MSG_CHAN_FOR_NON_TRANS;
}

STATIC void ubdrv_prepare_non_trans_free_data(struct ascend_ub_user_data *user_desc,
                                              struct ubdrv_free_non_trans_cmd *cmd)
{
    u32 len = sizeof(struct ubdrv_free_non_trans_cmd);
    user_desc->opcode = UBDRV_FREE_MSG_QUEUE;
    user_desc->size = len;
    user_desc->reply_size = 0;
    user_desc->cmd = cmd;
    user_desc->reply = NULL;
}

STATIC int ubdrv_alloc_non_trans_param_check(const struct ascend_ub_msg_dev *msg_dev,
                                             const struct devdrv_non_trans_msg_chan_info *chan_info, u32 dev_id)
{
    u32 type;

    if (msg_dev == NULL) {
        ubdrv_err("Msg dev is NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (chan_info == NULL) {
        ubdrv_err("Chan info is NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (chan_info->rx_msg_process == NULL) {
        ubdrv_err("Chan rx msg process is NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    type = chan_info->msg_type;
    if (type >= devdrv_msg_client_max) {
        ubdrv_err("Chan msg type is not supported yet. (dev_id=%u;msg_type=%u)\n", dev_id, type);
        return -EOPNOTSUPP;
    }
    if ((chan_info->c_desc_size != chan_info->s_desc_size) || (chan_info->c_desc_size <= ASCEND_UB_MSG_DESC_LEN)) {
        ubdrv_err("Invalid cq size and sq size. (dev_id=%u;msg_type=%u;cq_size=%u;sq_size=%u)\n", dev_id, type,
                  chan_info->c_desc_size, chan_info->s_desc_size);
        return -EINVAL;
    }
    return 0;
}

STATIC void ubdrv_free_device_non_trans(u32 dev_id, u32 chan_id, u32 type)
{
    struct ascend_ub_user_data user_desc = {0};
    struct ubdrv_free_non_trans_cmd cmd = {0};
    int ret;

    cmd.chan_id = chan_id;
    ubdrv_prepare_non_trans_free_data(&user_desc, &cmd);
    ret = ubdrv_admin_send_msg(dev_id, &user_desc);
    if (ret != 0) {
        ubdrv_err("Free device non trans error by admin msg. (ret=%d;dev_id=%u;chan_id=%u;msg_type=%u)\n", ret, dev_id,
                  chan_id, type);
    }
    ubdrv_info("Free device non trans chan success. (dev_id=%u;chan_id=%u;type=%u)\n", dev_id, chan_id, type);
}

STATIC void ubdrv_prepare_enable_device_chan_data(struct ascend_ub_user_data *user_desc,
                                                  struct ubdrv_create_non_trans_cmd *cmd)
{
    user_desc->opcode = UBDRV_ENABLE_MSG_QUEUE;
    user_desc->size = sizeof(struct ubdrv_create_non_trans_cmd);
    user_desc->reply_size = 0;
    user_desc->cmd = cmd;
    user_desc->reply = NULL;
    return;
}

/* host interface */
int devdrv_ub_msg_alloc_non_trans_queue_process(struct ubdrv_non_trans_chan *chan,
                                                struct ubdrv_create_non_trans_cmd *cmd)
{
    struct ascend_ub_user_data user_desc = {0};
    struct ascend_ub_msg_dev *msg_dev;
    u32 dev_id;
    int ret;

    msg_dev = chan->msg_dev;
    dev_id = msg_dev->dev_id;
    ret = ubdrv_create_non_trans_jetty(msg_dev->ubc_dev, chan, cmd);
    if (ret != 0) {
        ubdrv_err("ubdrv_create_non_trans_jetty failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        return ret;
    }
    ret = ubdrv_prepare_non_trans_alloc_data(&user_desc, chan, cmd, dev_id);
    if (ret != 0) {
        ubdrv_err("Prepare non trans jetty info failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        goto delete_jetty;
    }

    ret = ubdrv_admin_send_msg(dev_id, &user_desc);
    if (ret != 0) {
        ubdrv_err("ubdrv_admin_send_msg failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        goto delete_jetty;
    }
    ret = ubdrv_init_non_trans_chan(chan, msg_dev->ubc_dev, cmd, dev_id, cmd->msg_type);
    if (ret != 0) {
        ubdrv_err("ubdrv_init_non_trans_chan failed. (ret=%d;dev_id=%u)\n", ret, msg_dev->dev_id);
        goto free_device_msg_chan;
    }
    /* must rearm jfc at last, before */
    ret = ubdrv_rearm_sync_jfc(&chan->msg_jetty);
    if (ret != 0) {
        ubdrv_err("Non trans chan rearm jfc failed. (dev_id=%u;chan_id=%u)\n", dev_id, chan->chan_id);
        goto uninit_non_trans_chan;
    }

    if (cmd->chan_mode != UBDRV_MSG_CHAN_FOR_RAO) {
        ubdrv_prepare_enable_device_chan_data(&user_desc, cmd);
        ret = ubdrv_admin_send_msg(dev_id, &user_desc);
        if (ret != 0) {
            ubdrv_err("Host enable device msg chan failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
            goto uninit_non_trans_chan;
        }
    }

    ubdrv_record_chan_jetty_info(&chan->chan_stat, &chan->msg_jetty);
    return 0;

uninit_non_trans_chan:
    ubdrv_uninit_non_trans_chan(chan, dev_id);
free_device_msg_chan:
    ubdrv_free_device_non_trans(dev_id, chan->chan_id, cmd->msg_type);
delete_jetty:
    ubdrv_delete_non_trans_jetty(chan);
    return ret;
}

void *devdrv_ub_msg_alloc_non_trans_queue(u32 dev_id, struct devdrv_non_trans_msg_chan_info *chan_info)
{
    struct ascend_ub_msg_dev *msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    struct ubdrv_create_non_trans_cmd cmd = {0};
    struct ubdrv_non_trans_chan *chan;
    u32 type;
    int ret;

    if (ubdrv_alloc_non_trans_param_check(msg_dev, chan_info, dev_id) != 0) {
        ubdrv_err("Check non trans chan info failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    type = chan_info->msg_type;
    chan = ubdrv_get_non_trans_chan(msg_dev);
    if (chan == NULL) {
        ubdrv_err("ubdrv_get_non_trans_chan failed. (dev_id=%u;msg_type=%u)\n", dev_id, type);
        return NULL;
    }

    chan->rx_msg_process = chan_info->rx_msg_process;
    chan->send_jfce_handler = NULL;
    chan->recv_jfce_handler = ubdrv_non_trans_jfce_recv_handle;
    ubdrv_set_non_trans_cmd_by_chan_info(&cmd, chan_info, chan->chan_id);
    ret = devdrv_ub_msg_alloc_non_trans_queue_process(chan, &cmd);
    if (ret != 0) {
        ubdrv_err("Host alloc msg chan failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        goto put_chan;
    }
    ubdrv_info("Host alloc non trans chan success. (dev_id=%u;msg_type=%u;chan_id=%u)\n", dev_id, chan_info->msg_type,
               chan->chan_id);
    return ubdrv_get_non_trans_chan_handle(chan);

put_chan:
    chan->rx_msg_process = NULL;
    ubdrv_put_non_trans_chan(chan);
    return NULL;
}

int devdrv_ub_msg_free_non_trans_queue_process(struct ubdrv_non_trans_chan *chan, struct ubdrv_free_non_trans_cmd *cmd)
{
    int ret = 0;
    struct ascend_ub_user_data user_desc = {0};
    u32 dev_id;
    u32 chan_id;

    ka_task_mutex_lock(&chan->tx_mutex);
    dev_id = chan->msg_dev->dev_id;
    chan_id = chan->chan_id;
    if (chan->status != UBDRV_CHAN_ENABLE) {
        ubdrv_warn("Chan does not need to be freed. (status=%u;dev_id=%u;chan_id=%u)\n", chan->status, dev_id, chan_id);
        ka_task_mutex_unlock(&chan->tx_mutex);
        return -EINVAL;
    }
    ubdrv_prepare_non_trans_free_data(&user_desc, cmd);
    if (ka_likely(ubdrv_get_device_status(dev_id) != UBDRV_DEVICE_DEAD)) {
        /* only device in live, then send msg to device notice free chan */
        ret = ubdrv_admin_send_msg(dev_id, &user_desc);
        if (ret != 0) {
            ubdrv_err("ubdrv_admin_send_msg failed, device free non_trans error. (ret=%d;dev_id=%u;chan_id=%u)\n", ret,
                      dev_id, chan_id);
        }
    }
    ubdrv_delete_non_trans_jetty(chan);
    ubdrv_uninit_non_trans_chan(chan, dev_id);
    chan->rx_msg_process = NULL;
    chan->status = UBDRV_CHAN_IDLE;
    ka_task_mutex_unlock(&chan->tx_mutex);
    ubdrv_clear_chan_dfx(&chan->chan_stat);
    return 0;
}

int devdrv_ub_msg_free_non_trans_queue(void *msg_chan)
{
    int ret = 0;
    struct ubdrv_non_trans_chan *chan;
    struct ubdrv_free_non_trans_cmd cmd = {0};

    if (msg_chan == NULL) {
        ubdrv_err("Invalid msg_chan handle, msg_chan is null.\n");
        return -EINVAL;
    }

    chan = ubdrv_find_msg_chan_by_handle(msg_chan);
    if (chan == NULL) {
        ubdrv_err("Invalid handle, can't find chan by chan_handle.\n");
        return -EINVAL;
    }
    cmd.chan_id = chan->chan_id;
    cmd.chan_mode = UBDRV_MSG_CHAN_FOR_NON_TRANS;
    ret = devdrv_ub_msg_free_non_trans_queue_process(chan, &cmd);
    if (ret != 0) {
        ubdrv_err("Host free non trans chan failed. (dev_id=%u;ret=%d;msg_type=%u;chan_id=%u)\n", chan->msg_dev->dev_id,
                  ret, chan->msg_type, chan->chan_id);
    } else {
        ubdrv_info("Host free non trans chan finish. (dev_id=%u;msg_type=%u;chan_id=%u)\n", chan->msg_dev->dev_id,
                   chan->msg_type, chan->chan_id);
    }
    return ret;
}

int devdrv_ub_sync_msg_send(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    struct ascend_ub_user_data user_data = {0};
    u32 dev_id = ubdrv_get_devid_by_non_trans_handle(msg_chan);
    if (ka_unlikely(ubdrv_get_device_status(dev_id) == UBDRV_DEVICE_DEAD)) {
        ubdrv_warn("Device status is dead. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    if (ubdrv_sync_send_check_param(msg_chan, data, real_out_len) != 0) {
        ubdrv_err("Host sync msg send failed, param check is error.\n");
        return -EINVAL;
    }
    ubdrv_sync_send_prepare_user_data(&user_data, data, in_data_len, out_data_len, UBDRV_MSG_TYPE_DEFAULT);
    return devdrv_sync_msg_send_inner(dev_id, msg_chan, &user_data, real_out_len);
}

void ubdrv_free_all_non_trans_chan(u32 dev_id)
{
    struct ubdrv_non_trans_chan *chan = NULL;
    struct ascend_ub_msg_dev *msg_dev = NULL;
    u32 i;

    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ubdrv_err("Get msg dev fail, when free all non trans chan. (dev_id=%u)\n", dev_id);
        return;
    }

    for (i = 0; i < msg_dev->chan_cnt; ++i) {
        chan = &msg_dev->non_trans_chan[i];
        ka_task_mutex_lock(&chan->tx_mutex);
        if (chan->status == UBDRV_CHAN_IDLE) {
            ka_task_mutex_unlock(&chan->tx_mutex);
            continue;
        }
        ka_task_mutex_unlock(&chan->tx_mutex);
        (void)devdrv_pcimsg_free_non_trans_queue(ubdrv_get_non_trans_chan_handle(chan));
    }

    return;
}