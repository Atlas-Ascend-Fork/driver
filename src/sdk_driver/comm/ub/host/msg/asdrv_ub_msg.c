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
#include "asdrv_ub_common_msg.h"
#include "asdrv_ub_main.h"
#include "asdrv_ub_non_trans_chan.h"

struct ubdrv_common_msg g_common_msg = {0};

struct ubdrv_common_msg_stat *ubdrv_get_common_stat_dfx(u32 dev_id, u32 type)
{
    return &g_common_msg.com_msg_stat[dev_id][type];
}

int ubdrv_msg_get_token_val(struct ascend_ub_msg_dev *msg_dev, void *data)
{
    return 0;
}

void ubdrv_init_common_msg_ctrl_rwsem()
{
    int i;

    for (i = 0; i < ASCEND_UB_DEV_MAX_NUM; i++) {
        ka_task_init_rwsem(&g_common_msg.rwlock[i]);
    }
}

int ubdrv_rx_msg_common_msg_process_proc(struct ascend_ub_msg_desc *desc, struct ubdrv_common_msg_stat *stat,
                                         u32 dev_id, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    int ret = 0;

    ka_task_down_read(&g_common_msg.rwlock[dev_id]);
    if (g_common_msg.common_fun[dev_id][desc->client_type] == NULL) {
        ka_task_up_read(&g_common_msg.rwlock[dev_id]);
        UBDRV_LOG_LEVEL(ubdrv_get_device_status(dev_id) != UBDRV_DEVICE_ONLINE ? UBDRV_WARN_LEVEL : UBDRV_ERR_LEVEL,
                        "Common client process func is null. (dev_id=%u;msg_type=%u;ret=%d)\n", dev_id,
                        desc->client_type, ret);
        stat->rx_null_cb_err++;
        return -EUNATCH;
    }
    ret = g_common_msg.common_fun[dev_id][desc->client_type](dev_id, data, in_data_len, out_data_len, real_out_len);
    ka_task_up_read(&g_common_msg.rwlock[dev_id]);
    return ret;
}

int devdrv_ub_register_common_msg_client(const struct devdrv_common_msg_client *msg_client)
{
    /* if need add a lock */
    struct ascend_ub_msg_dev *msg_dev;
    u32 type;
    int i;

    if (msg_client == NULL) {
        ubdrv_err("Input parameter is invalid.\n");
        return -EINVAL;
    }
    type = (u32)msg_client->type;
    if (type >= DEVDRV_COMMON_MSG_TYPE_MAX) {
        ubdrv_err("Msg client type is not supported yet. (msg_client_type=%u)\n", type);
        return -EOPNOTSUPP;
    }

    for (i = 0; i < ASCEND_UB_DEV_MAX_NUM; i++) {
        ka_task_down_write(&g_common_msg.rwlock[i]);
        if (g_common_msg.common_fun[i][type] != NULL) {
            ka_task_up_write(&g_common_msg.rwlock[i]);
            continue;
        }
        g_common_msg.common_fun[i][type] = msg_client->common_msg_recv;
        ka_task_up_write(&g_common_msg.rwlock[i]);
        msg_dev = ubdrv_get_msg_dev_by_devid(i);
        if ((msg_client->init_notify != NULL) && (msg_dev != NULL)) {
            msg_client->init_notify((u32)i, 0);
        }
    }
    ubdrv_info("Common client type register success. (type=%d)\n", type);
    return 0;
}

int devdrv_ub_unregister_common_msg_client(u32 devid, const struct devdrv_common_msg_client *msg_client)
{
    u32 type;

    if (devid >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    if (msg_client == NULL) {
        ubdrv_err("Input parameter is invalid. (dev_id=%u)\n", devid);
        return -EINVAL;
    }
    type = (u32)msg_client->type;
    if (type >= DEVDRV_COMMON_MSG_TYPE_MAX) {
        ubdrv_err("Msg client type is not supported yet. (dev_id=%u; msg_client_type=%u)\n", devid, type);
        return -EOPNOTSUPP;
    }
    ka_task_down_write(&g_common_msg.rwlock[devid]);
    g_common_msg.common_fun[devid][type] = NULL;
    ka_task_up_write(&g_common_msg.rwlock[devid]);
    ubdrv_info("Common client type unregister success. (dev_id=%u;type=%d)\n", devid, type);
    return 0;
}

int devdrv_ub_common_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len,
                              enum devdrv_common_msg_type msg_type)
{
    struct ascend_ub_user_data user_data = {0};
    struct ubdrv_common_msg_stat *stat;
    u32 msg_type_tmp;
    void *chan;
    int ret;

    if (ka_unlikely(ubdrv_get_device_status(devid) == UBDRV_DEVICE_DEAD)) {
        ubdrv_warn("Device status is dead. (dev_id=%u)\n", devid);
        return -ENODEV;
    }

    msg_type_tmp = (u32)msg_type;
    if (ubdrv_comm_msg_send_check(devid, data, real_out_len) != 0) {
        ubdrv_err("Invalid comm msg send param. (dev_id=%u;msg_type=%u)\n", devid, msg_type);
        return -EINVAL;
    }
    if (msg_type_tmp >= DEVDRV_COMMON_MSG_TYPE_MAX) {
        ubdrv_err("Msg client type is not supported yet. (dev_id=%u; msg_client_type=%u)\n", devid, msg_type_tmp);
        return -EOPNOTSUPP;
    }
    stat = ubdrv_get_common_stat_dfx(devid, msg_type_tmp);
    stat->tx_total++;
    chan = g_common_msg.msg_chan[devid];
    if (chan == NULL) {
        ubdrv_err("Chan is NULL. (dev_id=%u;msg_type=%u)\n", devid, msg_type_tmp);
        stat->tx_chan_null_err++;
        return -EINVAL;
    }
    ubdrv_sync_send_prepare_user_data(&user_data, data, in_data_len, out_data_len, msg_type_tmp);
    ret = devdrv_sync_msg_send_inner(devid, chan, &user_data, real_out_len);
    ubdrv_common_msg_send_ret_process(ret, devid, msg_type_tmp, stat);
    return ret;
}

STATIC struct devdrv_non_trans_msg_chan_info g_common_msg_chan_info = {
    .msg_type = devdrv_msg_client_common,
    .flag = 0,
    .level = DEVDRV_MSG_CHAN_LEVEL_LOW,
    .s_desc_size = UBDRV_NON_TRANS_MSG_DEFAULT_DESC_SIZE,
    .c_desc_size = UBDRV_NON_TRANS_MSG_DEFAULT_DESC_SIZE,
    .rx_msg_process = ubdrv_rx_msg_common_msg_process,
};

int ubdrv_alloc_common_msg_queue(struct ascend_ub_msg_dev *msg_dev)
{
    u32 dev_id = msg_dev->dev_id;
    void *chan;

    chan = devdrv_pcimsg_alloc_non_trans_queue(dev_id, &g_common_msg_chan_info);
    if (chan == NULL) {
        ubdrv_err("Alloc common msg chan failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    g_common_msg.msg_chan[dev_id] = chan;
    ubdrv_info("Alloc common msg chan success. (dev_id=%u)\n", dev_id);
    return 0;
}

int ubdrv_free_common_msg_queue(u32 dev_id)
{
    size_t len = sizeof(struct ubdrv_common_msg_stat) * DEVDRV_COMMON_MSG_TYPE_MAX;
    void *chan;
    int ret;

    chan = g_common_msg.msg_chan[dev_id];
    ret = devdrv_pcimsg_free_non_trans_queue(chan);
    if (ret != 0) {
        ubdrv_err("Free common msg chan failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
        return -EINVAL;
    }
    g_common_msg.msg_chan[dev_id] = NULL;
    (void)memset_s(g_common_msg.com_msg_stat[dev_id], len, 0, len);
    ubdrv_info("Free common msg chan success. (dev_id=%u)\n", dev_id);
    return 0;
}