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
#include "ka_kernel_def_pub.h"
#include "asdrv_ub_main.h"
#include "asdrv_ub_admin_msg.h"
#include "asdrv_ub_urma_chan.h"

void ubdrv_urma_chan_uninit(struct ascend_ub_msg_dev *msg_dev)
{
    struct ubdrv_urma_chan *urma_chan;
    u32 i;

    for (i = 0; i < URMA_CHAN_MAX; ++i) {
        urma_chan = &msg_dev->urma_chan[i];
        urma_chan->chan_id = 0;
        urma_chan->dev_id = 0;
        urma_chan->status = UBDRV_CHAN_IDLE;
        urma_chan->msg_dev = NULL;
        urma_chan->chan_mode = UBDRV_MSG_CHAN_FOR_MAX;
        urma_chan->stat.dev_id = 0;
        urma_chan->stat.chan_id = 0;
        ka_task_mutex_destroy(&urma_chan->tx_mutex);
    }
}

void ubdrv_urma_chan_init(struct ascend_ub_msg_dev *msg_dev)
{
    struct ubdrv_urma_chan *urma_chan;
    u32 i;

    for (i = 0; i < URMA_CHAN_MAX; ++i) {
        urma_chan = &msg_dev->urma_chan[i];
        urma_chan->chan_id = i;
        urma_chan->dev_id = msg_dev->dev_id;
        urma_chan->status = UBDRV_CHAN_IDLE;
        urma_chan->msg_dev = msg_dev;
        urma_chan->chan_mode = UBDRV_MSG_CHAN_FOR_URMA;
        urma_chan->stat.dev_id = msg_dev->dev_id;
        urma_chan->stat.chan_id = i;
        ka_task_mutex_init(&urma_chan->tx_mutex);
    }
}

STATIC int ubdrv_urma_copy_para_check(u32 dev_id, enum devdrv_urma_chan_type type, enum devdrv_urma_copy_dir dir,
                                      struct devdrv_urma_copy *local, struct devdrv_urma_copy *peer)
{
    struct ubcore_target_seg *local_seg = NULL;
    struct ubcore_target_seg *peer_seg = NULL;
    u64 local_seg_len;
    u64 peer_seg_len;

    if ((local == NULL) || (peer == NULL) || (local->seg == NULL) || (peer->seg == NULL)) {
        ubdrv_err("Check urma chan data fail, copy data is null. (dev_id=%u;type=%u)\n", dev_id, type);
        return -EINVAL;
    }
    if ((dev_id >= ASCEND_UB_DEV_MAX_NUM) || (type >= URMA_CHAN_MAX) || (dir >= DIR_MAX)) {
        ubdrv_err("Check urma chan type fail. (dev_id=%u;type=%u;dir=%u)\n", dev_id, type, dir);
        return -EINVAL;
    }
    local_seg = (struct ubcore_target_seg *)local->seg;
    peer_seg = (struct ubcore_target_seg *)peer->seg;
    local_seg_len = local_seg->seg.len;
    peer_seg_len = peer_seg->seg.len;
    if ((local->offset >= local_seg_len) || (local->len > local_seg_len) ||
        ((local->len + local->offset) > local_seg_len) || ((local->len + local->offset) < local->len) ||
        ((local->len + local->offset) < local->offset)) {
        ubdrv_err("Check local copy len fail. (dev_id=%u;type=%u;len=%llu;offset=%llu;max_len=%llu)\n", dev_id, type,
                  local->len, local->offset, local_seg->seg.len);
        return -EINVAL;
    }
    if ((peer->offset >= peer_seg_len) || (peer->len > peer_seg_len) || ((peer->len + peer->offset) > peer_seg_len)) {
        ubdrv_err("Check peer copy len over. (dev_id=%u;type=%u;len=%llu;offset=%llu;max_len=%llu)\n", dev_id, type,
                  peer->len, peer->offset, peer_seg->seg.len);
        return -EINVAL;
    }
    if (local->len != peer->len) {
        ubdrv_err("Check copy len fail. (dev_id=%u;type=%u;local_len=%llu;peer_len=%llu)\n", dev_id, type, local->len,
                  peer->len);
        return -EINVAL;
    }
    return 0;
}

STATIC void ubdrv_urma_chan_wqe_prepare(struct send_wr_cfg *wr_cfg, struct ubdrv_urma_chan *urma_chan,
                                        enum devdrv_urma_copy_dir dir, struct devdrv_urma_copy *local,
                                        struct devdrv_urma_copy *peer)
{
    struct ubcore_target_seg *local_seg = NULL;
    struct ubcore_target_seg *peer_seg = NULL;

    local_seg = (struct ubcore_target_seg *)local->seg;
    peer_seg = (struct ubcore_target_seg *)peer->seg;
    wr_cfg->user_ctx = 0;
    wr_cfg->jfs = urma_chan->send_jetty.jfs; // local send jetty
    wr_cfg->tjetty = urma_chan->tjetty;      // remote recv jetty
    wr_cfg->len = local->len;
    if (dir == PEER_TO_LOCAL) {
        wr_cfg->sseg = peer_seg;  // read src: remote
        wr_cfg->tseg = local_seg; // read dst: local
        wr_cfg->sva = wr_cfg->sseg->seg.ubva.va + peer->offset;
        wr_cfg->dva = wr_cfg->tseg->seg.ubva.va + local->offset;
    } else {
        wr_cfg->sseg = local_seg; // write src: local
        wr_cfg->tseg = peer_seg;  // write dst: remote
        wr_cfg->sva = wr_cfg->sseg->seg.ubva.va + local->offset;
        wr_cfg->dva = wr_cfg->tseg->seg.ubva.va + peer->offset;
    }
    return;
}

STATIC void ubdrv_urma_copy_para_opcode(enum devdrv_urma_copy_dir dir, enum ubcore_opcode *opcode)
{
    if (dir == PEER_TO_LOCAL) {
        *opcode = UBCORE_OPC_READ;
    } else {
        *opcode = UBCORE_OPC_WRITE;
    }
}

STATIC int ubdrv_urma_chan_prepare_send(struct ubdrv_urma_chan *chan, struct send_wr_cfg *wr_cfg,
                                        enum devdrv_urma_copy_dir dir, struct devdrv_urma_copy *local,
                                        struct devdrv_urma_copy *peer)
{
    struct ascend_ub_jetty_ctrl *send_jetty = &chan->send_jetty;
    struct ubdrv_msg_chan_stat *stat = &chan->stat;
    int ret;

    ret = ubdrv_repair_chan_send_jetty(chan->dev_id, chan->chan_id, stat, send_jetty, wr_cfg);
    if (ret != 0) {
        return ret;
    }
    ubdrv_urma_chan_wqe_prepare(wr_cfg, chan, dir, local, peer);
    return 0;
}

int ubdrv_urma_copy(u32 dev_id, enum devdrv_urma_chan_type type, enum devdrv_urma_copy_dir dir,
                    struct devdrv_urma_copy *local, struct devdrv_urma_copy *peer)
{
    struct ascend_ub_msg_dev *msg_dev = NULL;
    struct ubdrv_urma_chan *urma_chan = NULL;
    struct ubdrv_msg_chan_stat *stat = NULL;
    struct send_wr_cfg wr_cfg = {0};
    enum ubcore_opcode opcode;
    struct ubcore_cr cr = {0};
    bool check_status = false;
    int ret;

    ret = ubdrv_urma_copy_para_check(dev_id, type, dir, local, peer);
    if (ret != 0) {
        return ret;
    }
    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    urma_chan = &msg_dev->urma_chan[type];
    stat = &urma_chan->stat;
    ubdrv_record_common_prof_time(&stat->tx_start_stamp);
    ubdrv_mutex_lock_polling(dev_id, &urma_chan->tx_mutex);
    if (urma_chan->status != UBDRV_CHAN_ENABLE) {
        ubdrv_err("Urma chan is invalid. (status=%u;dev_id=%u;chan_type=%u)\n", urma_chan->status, dev_id, type);
        ret = -EINVAL;
        goto urma_chan_unlock;
    }
    ret = ubdrv_urma_chan_prepare_send(urma_chan, &wr_cfg, dir, local, peer);
    if (ret != 0) {
        goto urma_chan_unlock;
    }
    ubdrv_urma_copy_para_opcode(dir, &opcode);
    check_status = (type == URMA_CHAN_BBOX ? false : true);
    ubdrv_record_common_prof_time(&stat->tx_post_stamp);
    ret = ubdrv_post_rw_wr_process(&wr_cfg, opcode);
    if (ret != 0) {
        ubdrv_err("Ub post jfs wr fail. (ret=%d;opcode=%u;dev_id=%u;chan_type=%u;jfs_id=%u;jfc_id=%u)\n", ret, opcode,
                  dev_id, type, stat->tx_jfs_id, stat->tx_jfs_jfc_id);
        goto urma_chan_unlock;
    }
    ret = ubdrv_interval_poll_send_jfs_jfc(&urma_chan->send_jetty, (u64)wr_cfg.user_ctx, MSG_MAX_WAIT_CNT, &cr, stat,
                                           check_status);
    if ((ret != 0) || (cr.status != UBCORE_CR_SUCCESS)) {
        ubdrv_warn("Ub poll jfs_jfc cqe unsuccess. (ret=%d;opcode=%u;cr_status=%d;dev_id=%u;chan_type=%u;jfs_id=%u;\
            jfc_id=%u)\n",
                   ret, opcode, cr.status, dev_id, type, stat->tx_jfs_id, stat->tx_jfs_jfc_id);
        if (cr.status == UBCORE_CR_ACK_TIMEOUT_ERR) {
            (void)ubdrv_rebuild_chan_send_jetty(dev_id, type, stat, &urma_chan->send_jetty, &wr_cfg);
        }
        ret = ubdrv_msg_ret_cal(ret, cr.status);
        goto urma_chan_unlock;
    }
    ubdrv_record_common_prof_time(&stat->tx_recv_stamp);
    ubdrv_urma_chan_tx_prof_info(dev_id, type, opcode, local->len, stat);
    ka_task_mutex_unlock(&urma_chan->tx_mutex);
    return 0;

urma_chan_unlock:
    ka_task_mutex_unlock(&urma_chan->tx_mutex);
    return ret;
}

STATIC void ubdrv_urma_chan_dfx_record(struct ubdrv_urma_chan *urma_chan)
{
    struct ascend_ub_jetty_ctrl *jetty = &urma_chan->send_jetty;
    struct ubdrv_msg_chan_stat *stat = &urma_chan->stat;

    stat->tx_jfr_jfc_id = jetty->jfr_jfc->id;
    stat->tx_jfs_jfc_id = jetty->jfs_jfc->id;
    stat->tx_jfr_id = jetty->jfr->jfr_id.id;
    stat->tx_jfs_id = jetty->jfs->jfs_id.id;
    return;
}

STATIC int ubdrv_set_urma_chan_cfg(u32 dev_id, struct ubdrv_urma_chan *urma_chan)
{
    struct ascend_ub_jetty_ctrl *cfg = &urma_chan->send_jetty;
    struct ub_idev *idev;
    u32 token;
    int ret;

    ret = ubdrv_get_local_token(dev_id, &token);
    if (ret != 0) {
        return ret;
    }
    idev = urma_chan->msg_dev->idev;
    cfg->jfr_msg_depth = ASCEND_UB_ADMIN_SEND_SEG_COUNT;
    cfg->jfs_msg_depth = ASCEND_UB_ADMIN_SEND_SEG_COUNT;
    cfg->recv_msg_len = 0;
    cfg->send_msg_len = 0;
    cfg->idev = idev;
    cfg->ubc_dev = idev->ubc_dev;
    cfg->eid_index = idev->eid_index;
    cfg->token_value = token;
    cfg->access = MEM_ACCESS_LOCAL;
    cfg->msg_chan = (void *)urma_chan;
    cfg->chan_mode = UBDRV_MSG_CHAN_FOR_URMA;
    return 0;
}

int ubdrv_urma_chan_alloc_jetty_res(struct ubdrv_urma_chan *urma_chan)
{
    struct ascend_ub_jetty_ctrl *cfg = &urma_chan->send_jetty;
    u32 dev_id = urma_chan->dev_id;
    u32 chan_id = urma_chan->chan_id;
    int ret;

    ret = ubdrv_set_urma_chan_cfg(dev_id, urma_chan);
    if (ret != 0) {
        return ret;
    }
    ret = ubdrv_create_sync_jfc(cfg, NULL);
    if (ret != 0) {
        ubdrv_err("Urma chan create jfc failed. (ret=%d;dev_id=%u;chan_id=%u)\n", ret, dev_id, chan_id);
        return ret;
    }

    ret = ubdrv_create_sync_jfr(cfg, cfg->jfr_jfc, UBDRV_DEFAULT_JETTY_ID);
    if (ret != 0) {
        ubdrv_err("Urma chan create jfr failed. (ret=%d;dev_id=%u;chan_id=%u)\n", ret, dev_id, chan_id);
        goto destroy_urma_chan_jfc;
    }
    ret = ubdrv_create_sync_jfs(cfg);
    if (ret != 0) {
        ubdrv_err("Urma chan create jfs failed. (ret=%d;dev_id=%u;chan_id=%u)\n", ret, dev_id, chan_id);
        goto destroy_urma_chan_jfr;
    }
    ubdrv_urma_chan_dfx_record(urma_chan);
    return 0;

destroy_urma_chan_jfr:
    ubdrv_delete_jfr(cfg);
destroy_urma_chan_jfc:
    ubdrv_delete_sync_jfc(cfg);
    return ret;
}

void ubdrv_urma_chan_free_jetty_res(struct ubdrv_urma_chan *urma_chan)
{
    struct ascend_ub_jetty_ctrl *cfg = &urma_chan->send_jetty;

    ubdrv_delete_sync_jfs(cfg);
    ubdrv_delete_jfr(cfg);
    ubdrv_delete_sync_jfc(cfg);
    return;
}

int ubdrv_check_urma_chan_status(struct ubdrv_urma_chan *urma_chan, enum ubdrv_chan_state expect_status)
{
    if (urma_chan == NULL) {
        ubdrv_err("Urma chan is null.\n");
        return -EINVAL;
    }
    if (urma_chan->status != expect_status) {
        ubdrv_warn("Urma chan status err. (dev_id=%u;chan_id=%u;status=%u;expect_status=%u)\n", urma_chan->dev_id,
                   urma_chan->chan_id, urma_chan->status, expect_status);
        return -EINVAL;
    }
    return 0;
}

int ubdrv_urma_chan_import_jfr(struct ascend_ub_msg_dev *msg_dev, struct ubdrv_urma_chan *urma_chan,
                               struct jetty_exchange_data *data)
{
    u32 chan_id = urma_chan->chan_id;
    u32 dev_id = msg_dev->dev_id;
    struct ubcore_tjetty *tjetty;
    struct ub_idev *idev;

    idev = msg_dev->idev;
    tjetty = ascend_import_jfr(idev->ubc_dev, idev->eid_index, data);
    if (KA_IS_ERR_OR_NULL(tjetty)) {
        ubdrv_err("Urma chan import jfr failed. (dev_id=%u;chan_id=%u;errno=%ld)\n", dev_id, chan_id,
                  KA_PTR_ERR(tjetty));
        return -EUBOPFAIL;
    }
    urma_chan->tjetty = tjetty;
    return 0;
}

void ubdrv_urma_chan_unimport_jfr(struct ubdrv_urma_chan *urma_chan)
{
    u32 chan_id = urma_chan->chan_id;
    u32 dev_id = urma_chan->dev_id;
    int ret;

    ret = ubcore_unimport_jfr(urma_chan->tjetty);
    if (ret != 0) {
        ubdrv_err("Urma chan unimport jfr failed. (dev_id=%u;chan_id=%u;ret=%d)\n", dev_id, chan_id, ret);
    }
    urma_chan->tjetty = NULL;
    return;
}

// host
STATIC void ubdrv_urma_chan_admin_msg_pack(struct ascend_ub_user_data *user_desc, u32 opcode, u32 len, void *cmd)
{
    user_desc->opcode = opcode;
    user_desc->size = len;
    user_desc->reply_size = len;
    user_desc->cmd = cmd;
    user_desc->reply = cmd;
    return;
}

STATIC int ubdrv_send_admin_alloc_urma_chan(u32 dev_id, u32 chan_id, struct ascend_ub_jetty_ctrl *cfg,
                                            struct ubdrv_create_urma_chan_cmd *cmd)
{
    u32 len = sizeof(struct ubdrv_create_urma_chan_cmd);
    struct ascend_ub_user_data user_desc = {0};
    int ret;

    ret = ubdrv_get_recv_jetty_info(cfg, &cmd->jetty_data);
    if (ret != 0) {
        return ret;
    }
    cmd->dev_id = dev_id;
    cmd->chan_id = chan_id;
    ubdrv_urma_chan_admin_msg_pack(&user_desc, UBDRV_CREATE_URMA_CHAN, len, cmd);
    return ubdrv_admin_send_msg(dev_id, &user_desc);
}

STATIC int ubdrv_send_admin_free_urma_chan(u32 dev_id, u32 chan_id, struct ascend_ub_jetty_ctrl *cfg,
                                           struct ubdrv_create_urma_chan_cmd *cmd)
{
    u32 len = sizeof(struct ubdrv_create_urma_chan_cmd);
    struct ascend_ub_user_data user_desc = {0};

    cmd->dev_id = dev_id;
    cmd->chan_id = chan_id;
    ubdrv_urma_chan_admin_msg_pack(&user_desc, UBDRV_FREE_URMA_CHAN, len, cmd);
    return ubdrv_admin_send_msg(dev_id, &user_desc);
}

STATIC int ubdrv_alloc_single_urma_chan(u32 dev_id, u32 chan_id)
{
    struct ubdrv_create_urma_chan_cmd cmd = {0};
    struct ascend_ub_jetty_ctrl *cfg;
    struct ascend_ub_msg_dev *msg_dev;
    struct ubdrv_urma_chan *urma_chan;
    int ret;

    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    urma_chan = &msg_dev->urma_chan[chan_id];
    ka_task_mutex_lock(&urma_chan->tx_mutex);
    ret = ubdrv_check_urma_chan_status(urma_chan, UBDRV_CHAN_IDLE);
    if (ret != 0) {
        goto host_urma_chan_unlock;
    }
    cfg = &urma_chan->send_jetty;
    ret = ubdrv_urma_chan_alloc_jetty_res(urma_chan);
    if (ret != 0) {
        goto host_urma_chan_unlock;
    }
    ret = ubdrv_send_admin_alloc_urma_chan(dev_id, chan_id, cfg, &cmd);
    if (ret != 0) {
        goto host_free_chan_res;
    }
    ret = ubdrv_urma_chan_import_jfr(msg_dev, urma_chan, &cmd.jetty_data);
    if (ret != 0) {
        goto host_free_dev_chan_res;
    }
    urma_chan->status = UBDRV_CHAN_ENABLE;
    ka_task_mutex_unlock(&urma_chan->tx_mutex);
    return 0;

host_free_dev_chan_res:
    (void)ubdrv_send_admin_free_urma_chan(dev_id, chan_id, cfg, &cmd);
host_free_chan_res:
    ubdrv_urma_chan_free_jetty_res(urma_chan);
host_urma_chan_unlock:
    ka_task_mutex_unlock(&urma_chan->tx_mutex);
    return ret;
}

STATIC void ubdrv_free_single_urma_chan(u32 dev_id, u32 chan_id)
{
    struct ubdrv_create_urma_chan_cmd cmd = {0};
    struct ascend_ub_msg_dev *msg_dev;
    struct ubdrv_urma_chan *urma_chan;
    int ret;

    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return;
    }
    urma_chan = &msg_dev->urma_chan[chan_id];
    ka_task_mutex_lock(&urma_chan->tx_mutex);
    ret = ubdrv_check_urma_chan_status(urma_chan, UBDRV_CHAN_ENABLE);
    if (ret != 0) {
        ka_task_mutex_unlock(&urma_chan->tx_mutex);
        return;
    }
    urma_chan->status = UBDRV_CHAN_DISABLE;
    ubdrv_urma_chan_unimport_jfr(urma_chan);
    (void)ubdrv_send_admin_free_urma_chan(dev_id, chan_id, &urma_chan->send_jetty, &cmd);
    ubdrv_urma_chan_free_jetty_res(urma_chan);
    urma_chan->status = UBDRV_CHAN_IDLE;
    ka_task_mutex_unlock(&urma_chan->tx_mutex);
    return;
}

void ubdrv_free_urma_chan(u32 dev_id)
{
    int i;

    for (i = 0; i < URMA_CHAN_MAX; i++) {
        ubdrv_free_single_urma_chan(dev_id, i);
    }
    return;
}

int ubdrv_alloc_urma_chan(u32 dev_id)
{
    int i, ret;

    for (i = 0; i < URMA_CHAN_MAX; i++) {
        ret = ubdrv_alloc_single_urma_chan(dev_id, i);
        if (ret != 0) {
            goto free_alloc_chan;
        }
    }
    ubdrv_info("Alloc urma chan success. (dev_id=%u)\n", dev_id);
    return 0;

free_alloc_chan:
    ubdrv_err("Alloc urma chan failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
    ubdrv_free_urma_chan(dev_id);
    return ret;
}

STATIC void ubdrv_seg_access_para(u32 access, u32 *para_access)
{
    if (access & DEVDRV_ACCESS_LOCAL_ONLY) {
        *para_access |= UBCORE_ACCESS_LOCAL_ONLY;
    }
    if (access & DEVDRV_ACCESS_READ) {
        *para_access |= UBCORE_ACCESS_READ;
    }
    if (access & DEVDRV_ACCESS_WRITE) {
        *para_access |= UBCORE_ACCESS_WRITE;
    }
    if (access & DEVDRV_ACCESS_ATOMIC) {
        *para_access |= UBCORE_ACCESS_ATOMIC;
    }
    return;
}

int ubdrv_register_seg(u32 dev_id, struct devdrv_seg_info *info, void **tseg, size_t *out_len)
{
    union ubcore_reg_seg_flag flag = {0};
    struct ubcore_seg_cfg seg_cfg = {0};
    struct ascend_ub_msg_dev *msg_dev;
    struct ubcore_target_seg *seg;
    u32 para_access = 0;
    int ret;

    if ((info == NULL) || (tseg == NULL) || (out_len == NULL) || (dev_id >= ASCEND_UB_DEV_MAX_NUM)) {
        ubdrv_err("Register seg para check fail. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    ret = ubdrv_add_device_status_ref(dev_id);
    if (ret != 0) {
        return ret;
    }
    ubdrv_seg_access_para(info->access, &para_access);
    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    flag.bs.token_policy = UBCORE_TOKEN_PLAIN_TEXT;
    flag.bs.access = para_access;
    flag.bs.non_pin = UBDRV_SEG_NON_PIN;
    seg_cfg.va = info->va;
    seg_cfg.len = info->mem_len;
    seg_cfg.flag = flag;
    seg_cfg.token_value.token = info->token_value;
    seg = ubcore_register_seg(msg_dev->ubc_dev, &seg_cfg, NULL);
    if (KA_IS_ERR_OR_NULL(seg)) {
        ubdrv_sub_device_status_ref(dev_id);
        ubdrv_err("ubcore_register_seg fail. (errno=%ld)\n", KA_PTR_ERR(seg));
        return -EUBOPFAIL;
    }
    ubdrv_sub_device_status_ref(dev_id);
    *out_len = sizeof(struct ubcore_target_seg);
    *tseg = seg;
    return 0;
}

int ubdrv_unregister_seg(u32 dev_id, void *tseg, size_t in_len)
{
    size_t len = sizeof(struct ubcore_target_seg);

    if ((tseg == NULL) || (dev_id >= ASCEND_UB_DEV_MAX_NUM)) {
        ubdrv_err("Unregister seg para check fail. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (in_len != len) {
        ubdrv_err("Unregister seg check len fail. (dev_id=%u;len=%zu;in_len=%zu)\n", dev_id, len, in_len);
        return -EINVAL;
    }
    return ubcore_unregister_seg((struct ubcore_target_seg *)tseg);
}

void *ubdrv_import_seg(u32 dev_id, u32 peer_token, void *peer_seg, size_t in_len, size_t *out_len)
{
    size_t len = sizeof(struct ubcore_target_seg);
    struct ubcore_target_seg_cfg cfg = {0};
    struct ascend_ub_msg_dev *msg_dev;
    struct ubcore_target_seg *seg;
    struct ubcore_target_seg *p_seg;
    int ret;

    if ((peer_seg == NULL) || ((out_len == NULL)) || (in_len != len) || (dev_id >= ASCEND_UB_DEV_MAX_NUM)) {
        ubdrv_err("Import seg para check fail. (dev_id=%u;len=%zu;in_len=%zu)\n", dev_id, len, in_len);
        return NULL;
    }

    ret = ubdrv_add_device_status_ref(dev_id);
    if (ret != 0) {
        return NULL;
    }
    p_seg = (struct ubcore_target_seg *)peer_seg;
    (void)memcpy_s(&cfg.seg, sizeof(struct ubcore_seg), &p_seg->seg, sizeof(struct ubcore_seg));
    cfg.token_value.token = peer_token;
    cfg.seg.attr.bs.token_policy = UBCORE_TOKEN_PLAIN_TEXT;
    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    seg = ubcore_import_seg(msg_dev->ubc_dev, &cfg, NULL);
    if (KA_IS_ERR_OR_NULL(seg)) {
        ubdrv_err("ubcore_import_seg fail. (errno=%ld)\n", KA_PTR_ERR(seg));
        seg = NULL;
        *out_len = 0;
    } else {
        *out_len = sizeof(struct ubcore_target_seg);
    }
    ubdrv_sub_device_status_ref(dev_id);
    return (void *)seg;
}

int ubdrv_unimport_seg(u32 dev_id, void *peer_tseg, size_t in_len)
{
    size_t len = sizeof(struct ubcore_target_seg);

    if ((peer_tseg == NULL) || (in_len != len) || (dev_id >= ASCEND_UB_DEV_MAX_NUM)) {
        ubdrv_err("Unimport seg para check fail. (dev_id=%u;len=%zu;in_len=%zu)\n", dev_id, len, in_len);
        return -EINVAL;
    }
    return ubcore_unimport_seg((struct ubcore_target_seg *)peer_tseg);
}