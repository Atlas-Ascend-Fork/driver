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

#include "ka_errno_pub.h"
#include "ka_compiler_pub.h"
#include "ka_kernel_def_pub.h"

#include "pbl/pbl_soc_res_sync.h"
#include "asdrv_ub_hotreset.h"
#include "asdrv_ub_non_trans_chan.h"
#include "asdrv_ub_urma_chan.h"
#include "asdrv_ub_pair_dev_info.h"
#include "asdrv_ub_proc_fs.h"
#include "asdrv_ub_admin_msg.h"

STATIC void ubdrv_admin_jfce_recv_handle(struct ubcore_jfc *jfc)
{
    struct ascend_ub_admin_chan *msg_chan;
    struct ascend_ub_jetty_ctrl *cfg;
    struct ubdrv_msg_chan_stat *stat;

    if ((jfc == NULL) || (jfc->jfc_cfg.jfc_context == NULL)) {
        return;
    }

    cfg = (struct ascend_ub_jetty_ctrl *)jfc->jfc_cfg.jfc_context;
    msg_chan = (struct ascend_ub_admin_chan *)cfg->msg_chan;
    if (msg_chan == NULL) {
        return;
    }
    ka_base_atomic_add(1, &msg_chan->user_cnt);
    if (msg_chan->valid != UB_ADMIN_MSG_ENABLE) {
        ka_base_atomic_sub(1, &msg_chan->user_cnt);
        return;
    }

    stat = &msg_chan->chan_stat;
    stat->rx_total++;
    stat->rx_work_stamp = ka_system_ktime_get_raw_ns();
    ka_task_queue_work(msg_chan->work_queue, &msg_chan->recv_work.work);
    ka_base_atomic_sub(1, &msg_chan->user_cnt);
    return;
}

int ubdrv_create_basic_jetty(struct ascend_ub_sync_jetty *jetty, u32 jfr_id)
{
    struct ascend_ub_jetty_ctrl *send_jetty;
    struct ascend_ub_jetty_ctrl *recv_jetty;
    int ret;

    send_jetty = &(jetty->send_jetty);
    recv_jetty = &(jetty->recv_jetty);
    ret = ubdrv_sync_send_jetty_init(send_jetty, NULL);
    if (ret != 0) {
        ubdrv_err("Ascend sync send jetty init failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = ubdrv_sync_recv_jetty_init(recv_jetty, jfr_id, ubdrv_admin_jfce_recv_handle);
    if (ret != 0) {
        ubdrv_err("Ascend sync recv jetty init failed. (ret=%d)\n", ret);
        ubdrv_sync_send_jetty_uninit(send_jetty);
        return ret;
    }
    ubdrv_sync_jetty_init(jetty);
    return 0;
}

void ubdrv_delete_basic_jetty(struct ascend_ub_sync_jetty *jetty)
{
    ubdrv_sync_jetty_uninit(jetty);
    ubdrv_sync_recv_jetty_uninit(&jetty->recv_jetty);
    ubdrv_sync_send_jetty_uninit(&jetty->send_jetty);
    return;
}

STATIC int ubdrv_admin_jetty_cfg_init(u32 dev_id, struct ub_idev *idev, struct ascend_ub_sync_jetty *cfg)
{
    u32 token;
    int ret;

    ret = ubdrv_get_local_token(dev_id, &token);
    if (ret != 0) {
        return ret;
    }

    cfg->recv_jetty.idev = idev;
    cfg->recv_jetty.ubc_dev = idev->ubc_dev;
    cfg->recv_jetty.eid_index = idev->eid_index;
    cfg->recv_jetty.send_msg_len = ASCEND_UB_ADMIN_SEND_SEG_SIZE;
    cfg->recv_jetty.recv_msg_len = ASCEND_UB_ADMIN_SEND_SEG_SIZE;
    cfg->recv_jetty.jfs_msg_depth = ASCEND_UB_ADMIN_SEND_SEG_COUNT;
    cfg->recv_jetty.jfr_msg_depth = ASCEND_UB_ADMIN_RECV_SEG_COUNT;
    cfg->recv_jetty.access = MEM_ACCESS_LOCAL;
    cfg->recv_jetty.token_value = token;
    cfg->recv_jetty.chan_mode = UBDRV_MSG_CHAN_FOR_ADMIN;

    cfg->send_jetty.idev = idev;
    cfg->send_jetty.ubc_dev = idev->ubc_dev;
    cfg->send_jetty.eid_index = idev->eid_index;
    cfg->send_jetty.send_msg_len = ASCEND_UB_ADMIN_SEND_SEG_SIZE;
    cfg->send_jetty.recv_msg_len = ASCEND_UB_ADMIN_SEND_SEG_SIZE;
    cfg->send_jetty.jfs_msg_depth = ASCEND_UB_ADMIN_SEND_SEG_COUNT;
    cfg->send_jetty.jfr_msg_depth = ASCEND_UB_ADMIN_RECV_SEG_COUNT;
    cfg->send_jetty.access = MEM_ACCESS_LOCAL;
    cfg->send_jetty.token_value = token;
    cfg->send_jetty.chan_mode = UBDRV_MSG_CHAN_FOR_ADMIN;
    return 0;
}

STATIC void ubdrv_admin_jetty_cfg_uninit(struct ascend_ub_sync_jetty *cfg)
{
    cfg->send_jetty.idev = NULL;
    cfg->send_jetty.ubc_dev = NULL;
    cfg->send_jetty.eid_index = 0;
    cfg->send_jetty.send_msg_len = 0;
    cfg->send_jetty.recv_msg_len = 0;
    cfg->send_jetty.jfs_msg_depth = 0;
    cfg->send_jetty.jfr_msg_depth = 0;
    cfg->send_jetty.access = 0;
    cfg->send_jetty.token_value = 0;
    cfg->send_jetty.chan_mode = UBDRV_MSG_CHAN_FOR_MAX;

    cfg->recv_jetty.idev = NULL;
    cfg->recv_jetty.ubc_dev = NULL;
    cfg->recv_jetty.eid_index = 0;
    cfg->recv_jetty.recv_msg_len = 0;
    cfg->recv_jetty.send_msg_len = 0;
    cfg->recv_jetty.jfs_msg_depth = 0;
    cfg->recv_jetty.jfr_msg_depth = 0;
    cfg->recv_jetty.access = 0;
    cfg->recv_jetty.token_value = 0;
    cfg->recv_jetty.chan_mode = UBDRV_MSG_CHAN_FOR_MAX;
    return;
}

int ubdrv_create_admin_jetty(struct ub_idev *idev, u32 dev_id, u32 jfr_id)
{
    struct ascend_ub_jetty_ctrl *send_jetty;
    struct ascend_ub_jetty_ctrl *recv_jetty;
    struct ascend_ub_link_res *link_res;
    int ret;

    link_res = ubdrv_get_link_res_by_devid(dev_id);
    if (link_res->admin_jetty != NULL) {
        ubdrv_err("Admin jetty has been initialized, create fail. (dev_id=%u)\n", dev_id);
        return -EALREADY;
    }
    link_res->admin_jetty = ubdrv_kzalloc(sizeof(struct ascend_ub_sync_jetty), KA_GFP_KERNEL);
    if (link_res->admin_jetty == NULL) {
        ubdrv_err("Admin jetty alloc mem fail. (dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }

    send_jetty = &(link_res->admin_jetty->send_jetty);
    recv_jetty = &(link_res->admin_jetty->recv_jetty);
    ret = ubdrv_admin_jetty_cfg_init(dev_id, idev, link_res->admin_jetty);
    if (ret != 0) {
        goto free_link_res;
    }
    ret = ubdrv_create_basic_jetty(link_res->admin_jetty, jfr_id);
    if (ret != 0) {
        ubdrv_err("Ascend admin jetty create failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        goto jetty_cfg_uninit;
    }
    ubdrv_info("Admin jetty create success. (dev_id=%u;send_jfr_id=%u; rcv_jfc_id=%u)\n", dev_id,
               send_jetty->jfr->jfr_id.id, recv_jetty->jfs_jfc->id);

    return 0;

jetty_cfg_uninit:
    ubdrv_admin_jetty_cfg_uninit(link_res->admin_jetty);
free_link_res:
    ubdrv_kfree(link_res->admin_jetty);
    link_res->admin_jetty = NULL;
    return ret;
}

void ubdrv_delete_admin_jetty(u32 dev_id)
{
    struct ascend_ub_jetty_ctrl *send_jetty;
    struct ascend_ub_jetty_ctrl *recv_jetty;
    struct ascend_ub_link_res *link_res;
    int ret;

    link_res = ubdrv_get_link_res_by_devid(dev_id);
    if (link_res->admin_jetty == NULL) {
        ubdrv_warn("Admin jetty is null, can't be deleted. (dev_id=%u)\n", dev_id);
        return;
    }

    send_jetty = &(link_res->admin_jetty->send_jetty);
    recv_jetty = &(link_res->admin_jetty->recv_jetty);
    ret = ubdrv_clear_jetty(send_jetty->jfs);
    if (ret != 0) {
        ubdrv_err("Flush send jetty failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
    }
    ret = ubdrv_clear_jetty(recv_jetty->jfs);
    if (ret != 0) {
        ubdrv_err("Flush recv jetty failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
    }

    ubdrv_delete_basic_jetty(link_res->admin_jetty);
    ubdrv_admin_jetty_cfg_uninit(link_res->admin_jetty);
    ubdrv_kfree(link_res->admin_jetty);
    link_res->admin_jetty = NULL;
    return;
}

int ubdrv_admin_chan_import_jfr(struct ub_idev *idev, struct ascend_ub_admin_chan *msg_chan,
                                struct jetty_exchange_data *data, u32 dev_id, enum ubdrv_log_level log_level)
{
    struct ubcore_tjetty *tjetty;
    u32 eid_index = idev->eid_index;

    tjetty = ascend_import_jfr(idev->ubc_dev, eid_index, data);
    if (KA_IS_ERR_OR_NULL(tjetty)) {
        UBDRV_LOG_LEVEL(log_level, "ascend_import_jfr unsuccessful. (dev_id=%u;ret=%ld)\n", dev_id, KA_PTR_ERR(tjetty));
        return -ETIMEDOUT;
    }
    msg_chan->tjetty = tjetty;
    return 0;
}

void ubdrv_admin_chan_unimport_jfr(struct ascend_ub_admin_chan *msg_chan, u32 dev_id)
{
    int ret;

    if (msg_chan->tjetty != NULL) {
        ret = ubcore_unimport_jfr(msg_chan->tjetty);
        if (ret != 0) {
            ubdrv_err("ubcore_unimport_jfr failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        }
    }
    msg_chan->tjetty = NULL;
    return;
}

int ubdrv_create_admin_msg_chan(u32 dev_id, struct ascend_ub_msg_dev *msg_dev)
{
    struct ascend_ub_sync_jetty *admin_jetty;
    struct ascend_ub_admin_chan *msg_chan;
    u32 mem_len;

    msg_chan = &(msg_dev->admin_msg_chan);
    admin_jetty = msg_chan->admin_jetty;
    mem_len = admin_jetty->recv_jetty.recv_msg_len;
    msg_chan->msg_desc = ubdrv_kzalloc(mem_len, KA_GFP_KERNEL);
    if (msg_chan->msg_desc == NULL) {
        ubdrv_err("Alloc chan msg desc mem fail. (dev_id=%u;mem_len=%u)\n", dev_id, mem_len);
        return -ENOMEM;
    }
    msg_chan->work_queue = ka_task_create_workqueue("ub_admin_workqueue");
    if (msg_chan->work_queue == NULL) {
        ubdrv_err("Create admin chan workqueue fail. (dev_id=%u)\n", dev_id);
        ubdrv_kfree(msg_chan->msg_desc);
        msg_chan->msg_desc = NULL;
        return -ENOMEM;
    }
    admin_jetty->recv_jetty.msg_chan = msg_chan;
    admin_jetty->send_jetty.msg_chan = msg_chan;
    msg_chan->recv_work.jfc = admin_jetty->recv_jetty.jfr_jfc;
    msg_chan->recv_work.stat = &msg_chan->chan_stat;
    KA_TASK_INIT_WORK(&msg_chan->recv_work.work, ubdrv_jfce_recv_work);
    ka_base_atomic_set(&msg_chan->msg_num, 0);
    ka_base_atomic_set(&msg_chan->user_cnt, 0);
    ka_task_mutex_lock(&msg_chan->mutex_lock);
    msg_chan->valid = UB_ADMIN_MSG_ENABLE;
    ka_task_mutex_unlock(&msg_chan->mutex_lock);
    ubdrv_record_chan_jetty_info(&msg_chan->chan_stat, admin_jetty);
    ubdrv_info("Create admin chan success. (dev_id=%u)\n", dev_id);
    return 0;
}

void ubdrv_del_admin_msg_chan(struct ascend_ub_msg_dev *msg_dev, u32 dev_id)
{
    struct ascend_ub_admin_chan *msg_chan;

    msg_chan = &(msg_dev->admin_msg_chan);
    ka_task_mutex_lock(&msg_chan->mutex_lock);
    msg_chan->valid = UB_ADMIN_MSG_DISABLE;
    ka_task_mutex_unlock(&msg_chan->mutex_lock);
    ubdrv_wait_chan_jfce_user_cnt(&msg_chan->user_cnt, dev_id, 0);
    ka_task_cancel_work_sync(&msg_chan->recv_work.work);
    ka_base_atomic_set(&msg_chan->user_cnt, 0);
    ka_base_atomic_set(&msg_chan->msg_num, 0);
    ka_task_destroy_workqueue(msg_chan->work_queue);
    msg_chan->work_queue = NULL;
    ubdrv_kfree(msg_chan->msg_desc);
    msg_chan->msg_desc = NULL;
    ubdrv_clear_chan_dfx(&msg_chan->chan_stat);
    return;
}

STATIC struct ascend_ub_admin_chan *ubdrv_get_admin_msg_chan(u32 dev_id)
{
    struct ascend_ub_admin_chan *msg_chan;
    struct ascend_ub_msg_dev *msg_dev;

    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ubdrv_err("Msg device is null. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    msg_chan = &(msg_dev->admin_msg_chan);
    ubdrv_mutex_lock_polling(dev_id, &msg_chan->mutex_lock);
    if (msg_chan->valid == UB_ADMIN_MSG_DISABLE) {
        ubdrv_err("Msg_chan is invalid. (dev_id=%u)\n", dev_id);
        ka_task_mutex_unlock(&msg_chan->mutex_lock);
        msg_chan = NULL;
    }

    return msg_chan;
}

STATIC void ubdrv_put_admin_msg_chan(struct ascend_ub_admin_chan *msg_chan)
{
    ka_task_mutex_unlock(&msg_chan->mutex_lock);
    return;
}

STATIC void ubdrv_clear_user_data(struct ascend_ub_admin_chan *msg_chan, struct ascend_ub_msg_desc *desc)
{
    u32 buf_len = msg_chan->admin_jetty->send_jetty.send_msg_len;
    u32 len = buf_len - ASCEND_UB_MSG_DESC_LEN;

    (void)memset_s(&(desc->user_data), len, 0, len);
    return;
}

STATIC int ubdrv_check_amdin_msg(u32 dev_id, struct ascend_ub_user_data *data)
{
    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Check dev_id failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (data == NULL) {
        ubdrv_err("Check data is null. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (data->size > ASCEND_UB_ADMIN_MAX_SEND_LEN) {
        ubdrv_err("Check send len failed. (dev_id=%u;send_size=%u)\n", dev_id, data->size);
        return -EINVAL;
    }
    return 0;
}

STATIC int ubdrv_basic_chan_prepare_send(struct ascend_ub_admin_chan *msg_chan, struct ascend_ub_msg_desc *desc,
                                         struct send_wr_cfg *wr_cfg, struct ascend_ub_user_data *data, u32 dev_id)
{
    struct ascend_ub_jetty_ctrl *send_jetty = &msg_chan->admin_jetty->send_jetty;
    struct ubdrv_msg_chan_stat *stat = &msg_chan->chan_stat;
    int ret;

    ret = ubdrv_msg_send_data_prepare(desc, send_jetty->send_msg_len, data);
    if (ret != 0) {
        ubdrv_err("ubdrv_msg_send_data_prepare failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }
    ret = ubdrv_repair_chan_send_jetty(dev_id, 0, stat, send_jetty, wr_cfg);
    if (ret != 0) {
        return ret;
    }
    ubdrv_msg_send_wqe_prepare(wr_cfg, send_jetty, desc, msg_chan->tjetty, data->size);
    return 0;
}

int ubdrv_basic_chan_send(u32 dev_id, struct ascend_ub_user_data *data, struct ascend_ub_admin_chan *msg_chan)
{
    struct ascend_ub_jetty_ctrl *jetty_ctrl;
    struct ascend_ub_msg_desc *rqe_desc;
    struct ubdrv_msg_chan_stat *stat;
    struct ascend_ub_msg_desc *desc;
    struct send_wr_cfg wr_cfg = {0};
    struct ubcore_cr cr = {0};
    int ta_timeout_cnt = 0;
    u32 msg_num;
    int ret;

    msg_num = (u32)ka_base_atomic_inc_return(&msg_chan->msg_num);
    desc = ubdrv_alloc_sync_send_seg(msg_chan->admin_jetty, dev_id, msg_num, UBDRV_ALLOC_SEG_TRY_CNT,
                                     UBDRV_ALLOC_SEG_WAIT_PER_US);
    if (desc == NULL) {
        ubdrv_err("ubdrv_alloc_sync_send_seg failed. (dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }

    ret = ubdrv_basic_chan_prepare_send(msg_chan, desc, &wr_cfg, data, dev_id);
    if (ret != 0) {
        ubdrv_err("ubdrv_basic_chan_prepare_send failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
        goto free_send_seg;
    }
    stat = &msg_chan->chan_stat;
    stat->tx_total++;
admin_msg_tatimeout:
    ret = ubdrv_post_send_wr(&wr_cfg, dev_id);
    if (ret != 0) {
        ubdrv_err("ubcore_post_jfs_wr failed. (seg_id=%u;dev_id=%u)\n", desc->seg_id, dev_id);
        stat->tx_post_send_err++;
        goto clear_user_data;
    }
    jetty_ctrl = &msg_chan->admin_jetty->send_jetty;
    ret = ubdrv_interval_poll_send_jfs_jfc(jetty_ctrl, (u64)desc->seg_id, MSG_MAX_WAIT_CNT, &cr, stat, true);
    if ((ret == 0) && (cr.status == UBCORE_CR_ACK_TIMEOUT_ERR) && (ta_timeout_cnt < ubdrv_get_msg_retry_cnt(dev_id))) {
        (void)ubdrv_rebuild_chan_send_jetty(dev_id, 0, stat, jetty_ctrl, &wr_cfg);
        ta_timeout_cnt++;
        goto admin_msg_tatimeout;
    } else if ((ret != 0) || (cr.status != UBCORE_CR_SUCCESS)) {
        ubdrv_warn("Admin chan send unsuccess. (ret=%d;cr_status=%d;dev_id=%u;dev_name=%s)\n", ret, cr.status, dev_id,
                   jetty_ctrl->ubc_dev->dev_name);
        ret = ubdrv_msg_ret_cal(ret, cr.status);
        goto clear_user_data;
    }

    rqe_desc = ubdrv_wait_sync_msg_rqe(dev_id, jetty_ctrl, msg_num, MSG_MAX_WAIT_CNT, stat);
    if (KA_IS_ERR(rqe_desc)) {
        ret = KA_PTR_ERR(rqe_desc);
        goto clear_user_data;
    } else if (rqe_desc->status != UB_MSG_PROCESS_SUCCESS) {
        goto repost_jfr;
    }
    ret = ubdrv_copy_rqe_data_to_user(rqe_desc, data, stat, jetty_ctrl->recv_msg_len);
    if (ret != 0) {
        ubdrv_err("Copy rqe data failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
    }

repost_jfr:
    ret = ubdrv_msg_result_process(ret, (int)rqe_desc->status, UBDRV_NONTRANS_TYPE_CNT);
    (void)ubdrv_post_jfr_wr(jetty_ctrl->jfr, jetty_ctrl->recv_seg, rqe_desc, jetty_ctrl->recv_msg_len,
                            rqe_desc->seg_id);
clear_user_data:
    ubdrv_clear_user_data(msg_chan, desc);
free_send_seg:
    ubdrv_free_sync_send_seg(msg_chan->admin_jetty, desc);
    return ret;
}

int ubdrv_admin_send_msg(u32 dev_id, struct ascend_ub_user_data *data)
{
    struct ascend_ub_admin_chan *msg_chan;
    int ret;

    ret = ubdrv_check_amdin_msg(dev_id, data);
    if (ret != 0) {
        ubdrv_err("Check send param failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        return -EINVAL;
    }

    if (ka_unlikely(ubdrv_get_device_status(dev_id) == UBDRV_DEVICE_DEAD)) {
        ubdrv_warn("Device status is dead. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    msg_chan = ubdrv_get_admin_msg_chan(dev_id);
    if (msg_chan == NULL) {
        ubdrv_err("Get admin msg chan failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = ubdrv_basic_chan_send(dev_id, data, msg_chan);
    if (ret != 0) {
        ubdrv_err("Send admin msg fail. (dev_id=%u;ret=%d)\n", dev_id, ret);
    }
    ubdrv_put_admin_msg_chan(msg_chan);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(ubdrv_admin_send_msg);
