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
#include "ubcore_opcode.h"
#include "ubcore_api.h"

#include "asdrv_ub_main.h"
#include "asdrv_ub_admin_msg.h"
#include "asdrv_ub_link_chan.h"
#include "asdrv_ub_non_trans_chan.h"
#include "asdrv_ub_proc_fs.h"
#include "asdrv_ub_jetty.h"

u64 ubdrv_get_seg_offset(struct ubcore_target_seg *tseg, u32 seg_id, u32 seg_size)
{
    return tseg->seg.ubva.va + seg_id * seg_size;
}

struct ascend_ub_msg_desc *ubdrv_get_send_desc(struct ascend_ub_jetty_ctrl *cfg, u32 seg_id)
{
    return (struct ascend_ub_msg_desc *)(uintptr_t)ubdrv_get_seg_offset(cfg->send_seg, seg_id, cfg->send_msg_len);
}

struct ascend_ub_msg_desc *ubdrv_get_recv_desc(struct ascend_ub_jetty_ctrl *cfg, u32 seg_id)
{
    return (struct ascend_ub_msg_desc *)(uintptr_t)ubdrv_get_seg_offset(cfg->recv_seg, seg_id, cfg->recv_msg_len);
}

STATIC struct ubdrv_msg_chan_stat *ubdrv_get_chan_stat(struct ascend_ub_jetty_ctrl *cfg)
{
    struct ubdrv_non_trans_chan *chan;
    struct ascend_ub_admin_chan *admin_chan;
    struct ubdrv_urma_chan *urma_chan;
    struct ubdrv_msg_chan_stat *stat;

    if (cfg->msg_chan == NULL) {
        ubdrv_err("Msg chan is null in jfc ctx.\n");
        return NULL;
    }
    if ((cfg->chan_mode == UBDRV_MSG_CHAN_FOR_NON_TRANS) || (cfg->chan_mode == UBDRV_MSG_CHAN_FOR_RAO)) {
        chan = cfg->msg_chan;
        stat = &chan->chan_stat;
    } else if (cfg->chan_mode == UBDRV_MSG_CHAN_FOR_URMA) {
        urma_chan = cfg->msg_chan;
        stat = &urma_chan->stat;
    } else {
        admin_chan = cfg->msg_chan;
        stat = &admin_chan->chan_stat;
    }
    return stat;
}

STATIC struct ubdrv_msg_chan_stat *ubdrv_get_chan_stat_by_jfc(struct ubcore_jfc *jfc)
{
    struct ascend_ub_jetty_ctrl *cfg;

    if ((jfc == NULL) || (jfc->jfc_cfg.jfc_context == NULL)) {
        ubdrv_err("Jfc is null.\n");
        return NULL;
    }

    cfg = (struct ascend_ub_jetty_ctrl *)jfc->jfc_cfg.jfc_context;
    return ubdrv_get_chan_stat(cfg);
}

STATIC struct ubdrv_msg_chan_stat *ubdrv_get_chan_stat_by_jfr(struct ubcore_jfr *jfr)
{
    struct ascend_ub_jetty_ctrl *cfg;

    if ((jfr == NULL) || (jfr->jfr_cfg.jfr_context == NULL)) {
        ubdrv_err("Jfr is null.\n");
        return NULL;
    }

    cfg = (struct ascend_ub_jetty_ctrl *)jfr->jfr_cfg.jfr_context;
    return ubdrv_get_chan_stat(cfg);
}

STATIC struct ubdrv_msg_chan_stat *ubdrv_get_chan_stat_by_jfs(struct ubcore_jfs *jfs)
{
    struct ascend_ub_jetty_ctrl *cfg;

    if ((jfs == NULL) || (jfs->jfs_cfg.jfs_context == NULL)) {
        ubdrv_err("Jetty is null.\n");
        return NULL;
    }

    cfg = (struct ascend_ub_jetty_ctrl *)jfs->jfs_cfg.jfs_context;
    return ubdrv_get_chan_stat(cfg);
}

STATIC void ubdrv_jfae_handle(struct ubcore_event *event, struct ubcore_ucontext *ctx)
{
    struct ubdrv_msg_chan_stat *stat = NULL;
    (void)ctx;

    if (event == NULL) {
        ubdrv_err("Event is null in jfae handle.\n");
        return;
    }

    switch (event->event_type) {
        case UBCORE_EVENT_JFC_ERR:
            stat = ubdrv_get_chan_stat_by_jfc(event->element.jfc);
            break;
        case UBCORE_EVENT_JFR_ERR:
        case UBCORE_EVENT_JFR_LIMIT_REACHED:
            stat = ubdrv_get_chan_stat_by_jfr(event->element.jfr);
            break;
        case UBCORE_EVENT_JFS_ERR:
            stat = ubdrv_get_chan_stat_by_jfs(event->element.jfs);
            break;
        default:
            ubdrv_err("Unprocessed abnormal event call. (event_type=%d)\n", event->event_type);
    }
    ubdrv_warn("Recv abnormal event call. (event_type=%d)\n", event->event_type);
    if (stat == NULL) {
        ubdrv_err("Chan dfx stat is null. (event_type=%d)\n", event->event_type);
        return;
    }
    stat->jfae_err++;
    return;
}

STATIC struct ubcore_target_seg *ubdrv_msg_seg_init(struct ascend_ub_jetty_ctrl *cfg, struct ubcore_device *ubc_dev,
                                                    u32 msg_size, u32 msg_count)
{
    size_t mem_size = (size_t)msg_size * msg_count;
    void *mem_base;
    u32 i;
    struct ubcore_target_seg *seg;
    struct ascend_ub_msg_desc *desc;
    union ubcore_reg_seg_flag flag = {0};
    struct ubcore_seg_cfg seg_cfg = {0};

    if ((ubc_dev == NULL) || (mem_size == 0) || (mem_size > ASCEND_UB_MSG_MAX_MEM_SIZE) ||
        (msg_size > ASCEND_UB_MSG_MAX_MEM_SIZE) || (msg_count > ASCEND_UB_MSG_MAX_MEM_SIZE)) {
        ubdrv_err("Invalid ubc_dev and segment_count. (msg_count=%u;msg_size=%u)\n", msg_count, msg_size);
        return NULL;
    }
    /* rao msg chan va from user */
    if (cfg->chan_mode == UBDRV_MSG_CHAN_FOR_RAO) {
        mem_base = (void *)(uintptr_t)cfg->rao_info->va;
    } else {
        mem_base = ubdrv_kzalloc(mem_size, KA_GFP_KERNEL);
    }
    if (mem_base == NULL) {
        ubdrv_err("Alloc mem_base fail.\n");
        return NULL;
    }
    if (cfg->chan_mode != UBDRV_MSG_CHAN_FOR_RAO) {
        (void)memset_s(mem_base, mem_size, 0, mem_size);
        for (i = 0; i < msg_count; ++i) {
            desc = (struct ascend_ub_msg_desc *)(mem_base + msg_size * i);
            desc->seg_id = i;
            desc->status = UB_MSG_INIT;
            desc->msg_num = 0;
            desc->time_out = UB_MSG_TIME_NORMAL;
        }
    }
    flag.bs.token_policy = UBCORE_TOKEN_PLAIN_TEXT;
    flag.bs.access = cfg->access;
    flag.bs.non_pin = UBDRV_SEG_NON_PIN;
    seg_cfg.va = (u64)(uintptr_t)mem_base;
    seg_cfg.len = mem_size;
    seg_cfg.flag = flag;
    seg_cfg.token_value.token = cfg->token_value;
    seg = ubcore_register_seg(ubc_dev, &seg_cfg, NULL);
    if (KA_IS_ERR_OR_NULL(seg)) {
        ubdrv_err("ubcore_register_seg fail. (errno=%ld)\n", KA_PTR_ERR(seg));
        goto free_mem;
    }
    return seg;
free_mem:
    if (cfg->chan_mode != UBDRV_MSG_CHAN_FOR_RAO) {
        ubdrv_kfree(mem_base);
    }
    return NULL;
}

STATIC int ubdrv_msg_seg_uninit(struct ascend_ub_jetty_ctrl *cfg, struct ubcore_target_seg *tseg)
{
    u64 va;
    int ret;

    if (tseg == NULL) {
        ubdrv_err("Check ub segment failed.\n");
        return -EINVAL;
    }

    va = tseg->seg.ubva.va;
    ret = ubcore_unregister_seg(tseg);
    if (ret != 0) {
        ubdrv_err("Unregister segment fail, when delete seg. (ret=%d)\n", ret);
    }
    if (cfg->chan_mode != UBDRV_MSG_CHAN_FOR_RAO) {
        ubdrv_kfree((void *)(uintptr_t)va);
    }
    return ret;
}

int ubdrv_post_jfr_wr(struct ubcore_jfr *jfr, struct ubcore_target_seg *seg, struct ascend_ub_msg_desc *va, u32 len,
                      u32 seg_id)
{
    struct ubcore_jfr_wr *bad_wr = NULL;
    struct ubcore_jfr_wr wr;
    struct ubcore_sge sge;
    int ret;

    wr.src.sge = &sge;
    wr.src.sge->tseg = seg;
    wr.src.sge->addr = (uint64_t)(uintptr_t)va;
    wr.src.sge->len = len;
    wr.src.num_sge = 1;
    wr.user_ctx = seg_id; // remember seg_id, find recv data seg in cr of cqe
    wr.next = NULL;

    ret = ubcore_post_jfr_wr(jfr, &wr, &bad_wr);
    if (ret != 0) {
        ubdrv_err("ubcore_post_jfr_wr failed. (seg_id=%u;len=%u)\n", seg_id, len);
    }
    return ret;
}

int ubdrv_clear_jetty(struct ubcore_jfs *jfs)
{
#if !defined(CFG_PLATFORM_ESL) && !defined(CFG_PLATFORM_FPGA)
    struct ubcore_jfs_attr attr = {0};
    struct ubcore_cr cr;
    int ret;

    if (jfs == NULL) {
        ubdrv_warn("Jfs is null, flush return.\n");
        return -EINVAL;
    }
    attr.mask = UBCORE_JFS_STATE;
    attr.state = UBCORE_JETTY_STATE_ERROR;
    ret = ubcore_modify_jfs(jfs, &attr, NULL);
    if (ret != 0) {
        ubdrv_err("ubcore_modify_jfs failed. (ret=%d)\n", ret);
    }

    ret = ubcore_flush_jfs(jfs, 1, &cr);
    if (ret < 0) {
        ubdrv_err("ubcore_flush_jfs failed. (ret=%d)\n", ret);
    }
    ubdrv_info("ubcore_flush_jfs success. (ret=%d)\n", ret);
#endif
    return 0;
}

STATIC int ubdrv_poll_jfc(struct ubcore_jfc *jfc, struct ubcore_cr *cr)
{
    int ret = 0;

    ret = ubcore_poll_jfc(jfc, ASCEND_UB_POLL_JFC_ONE, cr);
    if (ret != ASCEND_UB_POLL_JFC_ONE) {
        ubdrv_err("Polled cqe failed. (ret=%d)\n", ret);
        ret = ubcore_rearm_jfc(jfc, false);
        ubdrv_err("Ubcore poll jfc failed, and rearm jfc. (ret=%d)\n", ret);
        return -EIO;
    }
    return ubcore_rearm_jfc(jfc, false);
}

int ubdrv_rearm_sync_jfc(struct ascend_ub_sync_jetty *sync_jetty)
{
    int ret;

    ret = ubcore_rearm_jfc(sync_jetty->recv_jetty.jfr_jfc, false);
    if (ret != 0) {
        goto return_err;
    }
    return 0;

return_err:
    ubdrv_err("Ubcore rearm send jfc fail. (ret=%d)\n", ret);
    return ret;
}

STATIC bool ubdrv_check_sync_jetty(struct ascend_ub_jetty_ctrl *cfg)
{
    if ((cfg == NULL) || (cfg->ubc_dev == NULL)) {
        ubdrv_err("Check sync send jetty and ubc_dev failed.\n");
        return false;
    }
    return true;
}

STATIC struct ubcore_jfc *ubdrv_create_jfc(struct ascend_ub_jetty_ctrl *cfg, ubcore_comp_callback_t jfce_handler)
{
    struct ubcore_jfc_cfg jfc_cfg = {0};

    jfc_cfg.depth = ASCEND_UB_ADMIN_JETTY_JFC_DEPTH;
    jfc_cfg.jfc_context = (void *)cfg;
    jfc_cfg.ceqn = UB_URMA_JFC_CEQN;
    jfc_cfg.flag.bs.lock_free = UB_URMA_QUEUE_LOCK;
    return ubcore_create_jfc(cfg->ubc_dev, &jfc_cfg, jfce_handler, ubdrv_jfae_handle, NULL);
}

int ubdrv_create_sync_jfc(struct ascend_ub_jetty_ctrl *cfg, ubcore_comp_callback_t jfce_handler)
{
    struct ubcore_jfc *jfs_jfc = NULL;
    struct ubcore_jfc *jfr_jfc = NULL;

    jfs_jfc = ubdrv_create_jfc(cfg, NULL); // jfs_jfc not use jfce
    if (KA_IS_ERR_OR_NULL(jfs_jfc)) {
        ubdrv_err("Create jfc fail. (errno=%ld)\n", KA_PTR_ERR(jfs_jfc));
        return -EUBINITFAIL;
    }

    jfr_jfc = ubdrv_create_jfc(cfg, jfce_handler); // jfr_jfc use jfce call, but only in recv jfr
    if (KA_IS_ERR_OR_NULL(jfr_jfc)) {
        ubdrv_err("Create jfc fail. (errno=%ld)\n", KA_PTR_ERR(jfr_jfc));
        ubcore_delete_jfc(jfs_jfc);
        return -EUBINITFAIL;
    }
    cfg->jfs_jfc = jfs_jfc;
    cfg->jfr_jfc = jfr_jfc;
    return 0;
}

int ubdrv_create_sync_jfr(struct ascend_ub_jetty_ctrl *cfg, struct ubcore_jfc *jfc, u32 jfr_id)
{
    struct ubcore_jfr_cfg jfr_cfg = {0};
    struct ubcore_jfr *jfr;

    jfr_cfg.depth = cfg->jfr_msg_depth;
    jfr_cfg.flag.bs.tag_matching = 0;
    jfr_cfg.trans_mode = UBCORE_TP_RM;
    jfr_cfg.min_rnr_timer = UB_JFR_RNR_TIME;
    jfr_cfg.max_sge = UBDRV_JETTY_MAX_SEND_SEG;
    jfr_cfg.flag.bs.lock_free = UB_URMA_QUEUE_LOCK;
    jfr_cfg.flag.bs.token_policy = UBCORE_TOKEN_PLAIN_TEXT;
    jfr_cfg.token_value.token = cfg->token_value;
    jfr_cfg.jfr_context = (void *)cfg;
    jfr_cfg.eid_index = cfg->eid_index;
    jfr_cfg.jfc = jfc;
    jfr_cfg.id = jfr_id;

    jfr = ubcore_create_jfr(cfg->ubc_dev, &jfr_cfg, ubdrv_jfae_handle, NULL);
    if (KA_IS_ERR_OR_NULL(jfr)) {
        ubdrv_err("Create jfr fail. (errno=%ld)\n", KA_PTR_ERR(jfr));
        return -EUBINITFAIL;
    }
    cfg->jfr = jfr;
    return 0;
}

void ubdrv_delete_jfr(struct ascend_ub_jetty_ctrl *cfg)
{
    int ret;

    if (cfg->jfr == NULL) {
        ubdrv_err("Check sync jfr is null.\n");
        return;
    }

    ret = ubcore_delete_jfr(cfg->jfr);
    if (ret != 0) {
        ubdrv_err("Delete sync send jfr failed. (ret=%d)\n", ret);
    }
    cfg->jfr = NULL;
    return;
}

void ubdrv_delete_sync_jfc(struct ascend_ub_jetty_ctrl *cfg)
{
    int ret = 0;

    if ((cfg->jfs_jfc == NULL) || (cfg->jfr_jfc == NULL)) {
        ubdrv_err("Check sync jfc is null.\n");
        return;
    }

    ret = ubcore_delete_jfc(cfg->jfr_jfc);
    if (ret != 0) {
        ubdrv_err("ubdrv delete jfr_jfc failed. (ret=%d)\n", ret);
    }

    ret = ubcore_delete_jfc(cfg->jfs_jfc);
    if (ret != 0) {
        ubdrv_err("ubdrv delete jfs_jfc failed. (ret=%d)\n", ret);
    }
    cfg->jfs_jfc = NULL;
    cfg->jfr_jfc = NULL;
    return;
}

int ubdrv_create_sync_jfs(struct ascend_ub_jetty_ctrl *cfg)
{
    struct ubcore_jfs *jfs = NULL;
    struct ubcore_jfs_cfg jfs_cfg = {0};

    jfs_cfg.priority = ASCEND_UB_QOS_LVL_HIGH;
    jfs_cfg.depth = cfg->jfs_msg_depth;
    jfs_cfg.trans_mode = UBCORE_TP_RM;
    jfs_cfg.eid_index = cfg->eid_index;
    jfs_cfg.max_sge = UBDRV_JETTY_MAX_SEND_SEG;
    jfs_cfg.max_rsge = UBDRV_JETTY_MAX_RECV_SEG;
    jfs_cfg.rnr_retry = UBDRV_JETTY_RNR_RETRY;
    jfs_cfg.err_timeout = UBDRV_JETTY_ERR_TIMEOUT;
    jfs_cfg.jfs_context = (void *)cfg;
    jfs_cfg.jfc = cfg->jfs_jfc;

    jfs = ubcore_create_jfs(cfg->ubc_dev, &jfs_cfg, ubdrv_jfae_handle, NULL);
    if (KA_IS_ERR_OR_NULL(jfs)) {
        ubdrv_err("Create sync jfs failed. (errno=%ld)\n", KA_PTR_ERR(jfs));
        return -EUBINITFAIL;
    }
    cfg->jfs = jfs;
    return 0;
}

void ubdrv_delete_sync_jfs(struct ascend_ub_jetty_ctrl *cfg)
{
    int ret;

    if (cfg->jfs == NULL) {
        ubdrv_err("Check sync jfs failed.\n");
        return;
    }

    ret = ubcore_delete_jfs(cfg->jfs);
    if (ret != 0) {
        ubdrv_err("ubdrv delete jfs failed. (ret=%d)\n", ret);
    }
    cfg->jfs = NULL;
    return;
}

STATIC int ubdrv_create_sync_segment(struct ascend_ub_jetty_ctrl *cfg)
{
    struct ubcore_target_seg *send_seg;
    struct ubcore_target_seg *recv_seg;

    send_seg = ubdrv_msg_seg_init(cfg, cfg->ubc_dev, cfg->send_msg_len, cfg->jfs_msg_depth);
    if (send_seg == NULL) {
        ubdrv_err("Create send segment failed.\n");
        return -ENOMEM;
    }

    recv_seg = ubdrv_msg_seg_init(cfg, cfg->ubc_dev, cfg->recv_msg_len, cfg->jfr_msg_depth);
    if (recv_seg == NULL) {
        ubdrv_err("Create recv segment failed.\n");
        ubdrv_msg_seg_uninit(cfg, send_seg);
        return -ENOMEM;
    }
    cfg->send_seg = send_seg;
    cfg->recv_seg = recv_seg;
    return 0;
}

int ubdrv_delete_sync_send_segment(struct ascend_ub_jetty_ctrl *cfg)
{
    int ret;

    if (cfg->send_seg != NULL) {
        ret = ubdrv_msg_seg_uninit(cfg, cfg->send_seg);
        if (ret != 0) {
            ubdrv_err("Free send segment failed. (ret=%d)\n", ret);
        }
        cfg->send_seg = NULL;
        ubdrv_info("Free send seg success.\n");
    }
    if (cfg->recv_seg != NULL) {
        ret = ubdrv_msg_seg_uninit(cfg, cfg->recv_seg);
        if (ret != 0) {
            ubdrv_err("Free recv segment failed. (ret=%d)\n", ret);
        }
        cfg->recv_seg = NULL;
        ubdrv_info("Free recv seg success.\n");
    }
    return 0;
}

STATIC int ubdrv_post_sync_jfr_wr(struct ascend_ub_jetty_ctrl *cfg)
{
    struct ascend_ub_msg_desc *desc;
    struct ubcore_target_seg *seg;
    struct ubcore_jfr *jfr;
    u32 jfr_depth;
    int ret;
    u32 len;
    u32 i;

    if (cfg->chan_mode == UBDRV_MSG_CHAN_FOR_RAO) {
        return 0;
    }
    jfr_depth = cfg->jfr_msg_depth;
    jfr = cfg->jfr;
    seg = cfg->recv_seg;
    len = cfg->recv_msg_len;
    for (i = 0; i < jfr_depth; ++i) {
        desc = ubdrv_get_recv_desc(cfg, i);
        desc->status = UB_MSG_IDLE;
        ret = ubdrv_post_jfr_wr(jfr, seg, desc, len, i);
        if (ret != 0) {
            ubdrv_err("ascend_post_jfr_wr error. (ret=%d;seg_id=%u)\n", ret, i);
            return ret;
        }
    }
    return 0;
}

STATIC int ubdrv_create_sync_jetty_res(struct ascend_ub_jetty_ctrl *cfg, u32 jfr_id,
                                       ubcore_comp_callback_t jfce_handler)
{
    int ret;

    if (ubdrv_check_sync_jetty(cfg) == false) {
        ubdrv_err("Invalid cfg.\n");
        return -EINVAL;
    }

    ret = ubdrv_create_sync_segment(cfg);
    if (ret != 0) {
        ubdrv_err("Create cfg segment failed.\n");
        return ret;
    }

    ret = ubdrv_create_sync_jfc(cfg, jfce_handler);
    if (ret != 0) {
        ubdrv_err("Create sync send jfc failed.\n");
        goto destroy_send_segment;
    }

    ret = ubdrv_create_sync_jfr(cfg, cfg->jfr_jfc, jfr_id);
    if (ret != 0) {
        ubdrv_err("Create sync send jfr failed. (ret=%d)\n", ret);
        goto destroy_send_jfc;
    }
    ret = ubdrv_create_sync_jfs(cfg);
    if (ret != 0) {
        ubdrv_err("Create sync send jetty failed.\n");
        goto destroy_send_jfr;
    }
    return 0;

destroy_send_jfr:
    ubdrv_delete_jfr(cfg);
destroy_send_jfc:
    ubdrv_delete_sync_jfc(cfg);
destroy_send_segment:
    (void)ubdrv_delete_sync_send_segment(cfg);
    return ret;
}

STATIC void ubdrv_delete_sync_jetty_res(struct ascend_ub_jetty_ctrl *cfg)
{
    int ret;

    if (ubdrv_check_sync_jetty(cfg) == false) {
        ubdrv_err("Invalid cfg.\n");
        return;
    }

    ubdrv_delete_sync_jfs(cfg);
    ubdrv_delete_jfr(cfg);
    ubdrv_delete_sync_jfc(cfg);
    ret = ubdrv_delete_sync_send_segment(cfg);
    if (ret != 0) {
        ubdrv_err("ubdrv_delete_sync_send_segment failed. (ret=%d)\n", ret);
    }
    return;
}

int ubdrv_sync_send_jetty_init(struct ascend_ub_jetty_ctrl *cfg, ubcore_comp_callback_t jfce_handler)
{
    int ret;

    ret = ubdrv_create_sync_jetty_res(cfg, UBDRV_DEFAULT_JETTY_ID, jfce_handler);
    if (ret != 0) {
        ubdrv_err("Create jetty resource failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = ubdrv_post_sync_jfr_wr(cfg);
    if (ret != 0) {
        ubdrv_err("ubdrv_post_sync_jfr_wr failed. (ret=%d)\n", ret);
        ubdrv_delete_sync_jetty_res(cfg);
        return ret;
    }

    return 0;
}

void ubdrv_sync_send_jetty_uninit(struct ascend_ub_jetty_ctrl *cfg)
{
    if (ubdrv_check_sync_jetty(cfg) == false) {
        ubdrv_err("Invalid cfg.\n");
        return;
    }
    ubdrv_delete_sync_jetty_res(cfg);
    return;
}

int ubdrv_sync_recv_jetty_init(struct ascend_ub_jetty_ctrl *cfg, u32 jfr_id, ubcore_comp_callback_t jfce_handler)
{
    int ret;

    ret = ubdrv_create_sync_jetty_res(cfg, jfr_id, jfce_handler);
    if (ret != 0) {
        ubdrv_err("Create jetty resource failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = ubdrv_post_sync_jfr_wr(cfg);
    if (ret != 0) {
        ubdrv_err("ubdrv_post_sync_jfr_wr failed.\n");
        ubdrv_delete_sync_jetty_res(cfg);
        return ret;
    }

    return 0;
}

void ubdrv_sync_recv_jetty_uninit(struct ascend_ub_jetty_ctrl *cfg)
{
    if (ubdrv_check_sync_jetty(cfg) == false) {
        ubdrv_err("Invalid cfg.\n");
        return;
    }
    ubdrv_delete_sync_jetty_res(cfg);
    return;
}

void ubdrv_sync_jetty_init(struct ascend_ub_sync_jetty *sync_jetty)
{
    ka_base_atomic_set(&sync_jetty->user_cnt, 0);
    ka_task_mutex_init(&sync_jetty->mutex_lock);
}

void ubdrv_sync_jetty_uninit(struct ascend_ub_sync_jetty *sync_jetty)
{
    ka_task_mutex_destroy(&sync_jetty->mutex_lock);
    ka_base_atomic_set(&sync_jetty->user_cnt, 0);
}

STATIC int ubdrv_post_send_wr_check_para(struct ubcore_jfs *jfs, struct ubcore_jfs_wr *wr)
{
    if (ka_unlikely(jfs == NULL)) {
        ubdrv_err("jfs is null.\n");
        return -EINVAL;
    }

    if (ka_unlikely(wr->tjetty == NULL)) {
        ubdrv_err("tjetty is null.\n");
        return -EINVAL;
    }

    if (ka_unlikely(wr->send.src.sge->tseg == NULL)) {
        ubdrv_err("wr_send_src_tseg is null.\n");
        return -EINVAL;
    }

    return 0;
}

int ubdrv_post_send_wr(struct send_wr_cfg *wr_cfg, u32 dev_id)
{
    u32 len = (ASCEND_UB_MSG_DESC_LEN + wr_cfg->len);
    struct ubcore_jfs *jfs = wr_cfg->jfs;
    struct ubcore_jfs_wr *bad_wr = NULL;
    struct ubcore_jfs_wr wr = {0};
    struct ubcore_sge sge;
    int ret;

    if (len > ASCEND_UB_MAX_SEND_LIMIT) {
        len = ASCEND_UB_MAX_SEND_LIMIT; // max send len 4k
    }
    wr.send.src.sge = &sge;
    wr.opcode = UBCORE_OPC_SEND;
    wr.flag.bs.complete_enable = COMPLETE_ENABLE;
    wr.user_ctx = wr_cfg->user_ctx;
    wr.tjetty = wr_cfg->tjetty;
    wr.send.src.sge->addr = wr_cfg->sva;
    wr.send.src.sge->len = len;
    wr.send.src.sge->tseg = wr_cfg->sseg;
    wr.send.src.num_sge = 1;
    wr.send.tseg = NULL; // Not used in send opcode
    wr.next = NULL;

    if (ubdrv_post_send_wr_check_para(jfs, &wr) != 0) {
        ubdrv_err("Check send_wr para failed. (seg_id=%u;dev_id=%u)\n", wr_cfg->user_ctx, dev_id);
        return -EINVAL;
    }

    ret = ubcore_post_jfs_wr(jfs, &wr, &bad_wr);
    if (ret != 0) {
        ubdrv_err("ubcore_post_jfs_wr failed. (seg_id=%u;dev_id=%u;ret=%d)\n", wr_cfg->user_ctx, dev_id, ret);
        return ret;
    }
    return 0;
}

STATIC int ubdrv_post_rw_wr_check_para(struct ubcore_jfs *jfs, struct ubcore_jfs_wr *wr)
{
    if (ka_unlikely(jfs == NULL)) {
        ubdrv_err("Check jfs is null.\n");
        return -EINVAL;
    }

    if (ka_unlikely(wr->tjetty == NULL)) {
        ubdrv_err("tjetty is null.\n");
        return -EINVAL;
    }

    if (ka_unlikely(wr->rw.src.sge->tseg == NULL)) {
        ubdrv_err("rw_src_tseg is null.\n");
        return -EINVAL;
    }

    if (ka_unlikely(wr->rw.dst.sge->tseg == NULL)) {
        ubdrv_err("rw_dst_tseg is null.\n");
        return -EINVAL;
    }

    return 0;
}

int ubdrv_post_rw_wr_process(struct send_wr_cfg *wr_cfg, enum ubcore_opcode opcode)
{
    struct ubcore_jfs *jfs = wr_cfg->jfs;
    struct ubcore_jfs_wr *bad_wr = NULL;
    struct ubcore_jfs_wr wr = {0};
    struct ubcore_sge ssge = {0};
    struct ubcore_sge dsge = {0};
    int ret;

    wr.rw.src.sge = &ssge;
    wr.rw.dst.sge = &dsge;
    wr.opcode = opcode;
    wr.flag.bs.complete_enable = COMPLETE_ENABLE;
    wr.user_ctx = wr_cfg->user_ctx; // local id seg_id
    wr.tjetty = wr_cfg->tjetty;
    wr.rw.src.sge->addr = wr_cfg->sva;
    wr.rw.src.sge->len = wr_cfg->len;
    wr.rw.src.sge->tseg = wr_cfg->sseg;
    wr.rw.src.num_sge = 1;

    wr.rw.dst.sge->addr = wr_cfg->dva;
    wr.rw.dst.sge->len = wr_cfg->len;
    wr.rw.dst.sge->tseg = wr_cfg->tseg;
    wr.rw.dst.num_sge = 1;
    wr.next = NULL;

    if (ubdrv_post_rw_wr_check_para(jfs, &wr) != 0) {
        ubdrv_err("Check rw_wr para failed. (local_seg_id=%u)\n", wr_cfg->user_ctx);
        return -EINVAL;
    }

    ret = ubcore_post_jfs_wr(jfs, &wr, &bad_wr);
    if (ret != 0) {
        ubdrv_err("ubcore_post_jfs_wr failed. (ret=%d;local_seg_id=%u;jfs_id=%u;dev_name=%s)\n", ret, wr_cfg->user_ctx,
                  wr_cfg->jfs->jfs_id.id, wr_cfg->jfs->ub_dev->dev_name);
        return -EAGAIN;
    }
    return 0;
}

struct ubcore_tjetty *ascend_import_jfr(struct ubcore_device *dev, u32 eid_index, struct jetty_exchange_data *data)
{
    int ret;
    struct ubcore_tjetty_cfg cfg = {0};
#ifdef SSAPI_USE_MAMI
    struct ubcore_active_tp_cfg active_tp_cfg = {0};
    struct ubcore_get_tp_cfg tp_flag = {0};
    struct ubcore_eid_info *eid_info;
    struct ubcore_tp_info tpid_info = {0};
    u32 tp_cnt = 1;
    u32 i, eid_cnt;
#endif

    ret = memcpy_s(&cfg.id.eid, sizeof(union ubcore_eid), &data->eid.eid, sizeof(union ubcore_eid));
    if (ret != 0) {
        ubdrv_err("Copy jetty info failed. (ret=%d)\n", ret);
        return NULL;
    }
    cfg.id.id = data->id;
    cfg.trans_mode = UBCORE_TP_RM;
    cfg.type = UBCORE_JFR;
    cfg.eid_index = eid_index;
    cfg.flag.bs.token_policy = UBCORE_TOKEN_PLAIN_TEXT;
    cfg.token_value.token = data->token_value; // peer token
    ubdrv_info("Import info. (dev_name=%s;jfr_id=%u;eid_idx=%u;eid=" EID_FMT ")\n", dev->dev_name, data->id, eid_index,
               EID_ARGS(data->eid.eid));
#ifdef SSAPI_USE_MAMI
    cfg.tp_type = UBCORE_CTP;
    tp_flag.flag.bs.ctp = 1;
    tp_flag.trans_mode = UBCORE_TP_RM;
    // copy remote_eid to tp_flag.remote_eid
    ret = memcpy_s(&tp_flag.peer_eid, sizeof(union ubcore_eid), &data->eid.eid, sizeof(union ubcore_eid));
    if (ret != 0) {
        ubdrv_err("Copy jetty info failed. (ret=%d)\n", ret);
        return NULL;
    }

    // copy local_eid to tp_flag.local_eid, eid use which eid min_idx
    eid_info = ubcore_get_eid_list(dev, &eid_cnt);
    if (KA_IS_ERR_OR_NULL(eid_info)) {
        ubdrv_err("ubcore_get_eid_list failed.\n");
        return NULL;
    }
    for (i = 0; i < eid_cnt; i++) {
        if (eid_info[i].eid_index == eid_index) {
            (void)memcpy_s(&tp_flag.local_eid, sizeof(union ubcore_eid), &eid_info[i].eid, sizeof(union ubcore_eid));
            break;
        }
    }
    ubcore_free_eid_list(eid_info);
    if (i == eid_cnt) {
        ubdrv_err("get local_eid failed. (eid_cnt=%d;eid_index=%u)\n", eid_cnt, eid_index);
        return NULL;
    }

    ret = ubcore_get_tp_list(dev, &tp_flag, &tp_cnt, &tpid_info, NULL);
    if ((ret != 0) || (tp_cnt == 0)) {
        ubdrv_err("Get tp failed. (ret=%d;tp_cnt=%u)\n", ret, tp_cnt);
        return NULL;
    }

    active_tp_cfg.tp_handle = tpid_info.tp_handle;

    return ubcore_import_jfr_ex(dev, &cfg, &active_tp_cfg, NULL);
#else
    return ubcore_import_jfr(dev, &cfg, NULL);
#endif
}

struct ubcore_target_seg *ascend_import_seg(struct ubcore_device *dev, struct jetty_exchange_data *data)
{
    int ret;
    struct ubcore_target_seg_cfg cfg = {0};

    ret = memcpy_s(&cfg.seg, sizeof(struct ubcore_seg), &data->seg, sizeof(struct ubcore_seg));
    if (ret != 0) {
        ubdrv_err("Copy segment info failed. (ret=%d)\n", ret);
        return NULL;
    }
    cfg.token_value.token = data->token_value; // peer token
    cfg.seg.attr.bs.token_policy = UBCORE_TOKEN_PLAIN_TEXT;
    return ubcore_import_seg(dev, &cfg, NULL);
}

STATIC int ubdrv_get_jetty_eid_info(struct ubcore_eid_info *eid, struct ubcore_device *ubc_dev, u32 eid_index)
{
    struct ubcore_eid_info *eid_info;
    u32 cnt;
    u32 i;

    eid_info = ubcore_get_eid_list(ubc_dev, &cnt);
    if (KA_IS_ERR_OR_NULL(eid_info)) {
        ubdrv_err("ubcore_get_eid_list failed.\n");
        return -EINVAL;
    }
    for (i = 0; i < cnt; i++) {
        if (eid_info[i].eid_index == eid_index) {
            (void)memcpy_s(eid, UBDRV_UBCORE_EID_LEN, &eid_info[i], UBDRV_UBCORE_EID_LEN);
            ubcore_free_eid_list(eid_info);
            return 0;
        }
    }
    ubcore_free_eid_list(eid_info);
    return -EINVAL;
}

int ubdrv_get_send_jetty_info(struct ascend_ub_jetty_ctrl *cfg, struct jetty_exchange_data *data)
{
    int ret;

    if ((cfg == NULL) || (cfg->ubc_dev == NULL) || (cfg->jfr == NULL) || (cfg->send_seg == NULL)) {
        ubdrv_err("Check cfg failed.\n");
        return -EINVAL;
    }
    data->id = cfg->jfr->jfr_id.id;
    data->token_value = cfg->token_value;
    ret = ubdrv_get_jetty_eid_info(&data->eid, cfg->ubc_dev, cfg->eid_index);
    if (ret != 0) {
        ubdrv_err("Get eid failed. (ret=%d)\n", ret);
        return ret;
    }

    (void)memcpy_s(&(data->seg), sizeof(struct ubcore_seg), &(cfg->send_seg->seg), sizeof(struct ubcore_seg));
    ubdrv_info("Get send jetty success. (dev_name=%s;jfr_id=%u;eid_idx=%u;eid:" EID_FMT ")\n", cfg->ubc_dev->dev_name,
               data->id, cfg->eid_index, EID_ARGS(data->eid.eid));
    return 0;
}

int ubdrv_get_recv_jetty_info(struct ascend_ub_jetty_ctrl *cfg, struct jetty_exchange_data *data)
{
    int ret;

    if ((cfg == NULL) || (cfg->ubc_dev == NULL) || (cfg->jfr == NULL)) {
        ubdrv_err("Check cfg failed.\n");
        return -EINVAL;
    }
    data->id = cfg->jfr->jfr_id.id;
    data->token_value = cfg->token_value;
    ret = ubdrv_get_jetty_eid_info(&data->eid, cfg->ubc_dev, cfg->eid_index);
    if (ret != 0) {
        ubdrv_err("Get eid failed. (ret=%d)\n", ret);
        return ret;
    }
    ubdrv_info("Get recv jetty success. (dev_name=%s;jfr_id=%u;eid_idx=%u;eid:" EID_FMT ")\n", cfg->ubc_dev->dev_name,
               data->id, cfg->eid_index, EID_ARGS(data->eid.eid));
    return 0;
}

STATIC struct ascend_ub_msg_desc *ubdrv_jfce_recv_cqe_process(struct ubcore_jfc *jfc, struct ubdrv_msg_chan_stat *stat,
                                                              u32 *seg_id)
{
    struct ascend_ub_jetty_ctrl *cfg = NULL;
    struct ubcore_cr cr = {0};
    int ret;

    ret = ubdrv_poll_jfc(jfc, &cr);
    if (ret != 0) {
        ubdrv_err("ubdrv_poll_jfc failed. (ret=%d;jfc_id=%u;dev_id=%u;chan_id=%u;dev_name=%s)\n", ret, jfc->id,
                  stat->dev_id, stat->chan_id, jfc->ub_dev->dev_name);
        stat->rx_poll_cqe_err++;
        return NULL;
    }

    if (cr.status == UBCORE_CR_WR_FLUSH_ERR_DONE) {
        ubdrv_info("JFC destroy cqe status is flush done. (jfc_id=%u;dev_id=%u;chan_id=%u;dev_name=%s)\n", jfc->id,
                   stat->dev_id, stat->chan_id, jfc->ub_dev->dev_name);
        return NULL;
    } else if (cr.status != UBCORE_CR_SUCCESS) {
        ubdrv_err("CQE status is error. (cr_status=%d;jfc_id=%u;dev_id=%u;chan_id=%u;dev_name=%s)\n", cr.status,
                  jfc->id, stat->dev_id, stat->chan_id, jfc->ub_dev->dev_name);
        stat->rx_cqe_status_err++;
        return NULL;
    }
    cfg = (struct ascend_ub_jetty_ctrl *)jfc->jfc_cfg.jfc_context;
    *seg_id = (u32)cr.user_ctx; // local_seg_id
    if (*seg_id >= cfg->jfr_msg_depth) {
        ubdrv_err("Recv segment id is invalid. (seg_id=%u;jfc_id=%u;dev_id=%u;chan_id=%u;dev_name=%s)", *seg_id,
                  jfc->id, stat->dev_id, stat->chan_id, jfc->ub_dev->dev_name);
        return NULL;
    }

    return ubdrv_get_recv_desc(cfg, *seg_id);
}

STATIC void ubdrv_jfce_recv_work_func(struct ubcore_jfc *jfc, struct ubdrv_msg_chan_stat *stat)
{
    char dfx_info[UBDRV_DFX_INFO_LEN] = {0};
    struct ascend_ub_msg_desc *chan_desc;
    struct ascend_ub_msg_desc *rqe_desc;
    struct ascend_ub_jetty_ctrl *cfg;
    u32 sche_work_time;
    u32 msg_num;
    u32 seg_id;
    int ret = 0;

    stat->rx_work++;
    (void)sprintf_s(dfx_info, UBDRV_DFX_INFO_LEN, "device-%u chan_id-%u jfce recv work sche", stat->dev_id,
                    stat->chan_id);
    sche_work_time = ubdrv_record_resp_time(stat->rx_work_stamp, dfx_info, UBDRV_SCHED_RESP_TIME);
    if ((sche_work_time > stat->rx_work_max_time) || (ubdrv_procfs_get_prof_state() == true)) {
        stat->rx_work_max_time = sche_work_time;
    }

    cfg = (struct ascend_ub_jetty_ctrl *)jfc->jfc_cfg.jfc_context;
    rqe_desc = ubdrv_jfce_recv_cqe_process(jfc, stat, &seg_id);
    if (rqe_desc == NULL) {
        return;
    }

    chan_desc = ubdrv_copy_msg_from_rqe_to_chan(cfg, rqe_desc, seg_id);
    if (chan_desc == NULL) {
        goto repost_rqe;
    }
    msg_num = rqe_desc->msg_num;
    ubdrv_recv_msg_call_process(cfg, chan_desc, seg_id);
repost_rqe:
    ret = ubdrv_post_jfr_wr(cfg->jfr, cfg->recv_seg, rqe_desc, cfg->recv_msg_len, seg_id);
    if (ret != 0) {
        stat->rx_post_jfr_err++;
        ubdrv_err("Post rqe fail in recv process. (ret=%d;seg_id=%u;dev_id=%u;chan_id=%u;dev_name=%s)\n", ret, seg_id,
                  stat->dev_id, stat->chan_id, jfc->ub_dev->dev_name);
    }
    ubdrv_msg_chan_rx_prof_info(stat->dev_id, stat->chan_id, msg_num, stat);
    stat->rx_work_finish++;
    return;
}

void ubdrv_jfce_recv_work(ka_work_struct_t *p_work)
{
    struct ubdrv_sync_jfce_work *my_work;
    struct ubdrv_msg_chan_stat *stat;
    struct ubcore_jfc *jfc;

    my_work = ka_container_of(p_work, struct ubdrv_sync_jfce_work, work);
    jfc = my_work->jfc;
    stat = my_work->stat;
    ubdrv_jfce_recv_work_func(jfc, stat);
}

STATIC int ubdrv_check_msg_loop_condition(u32 dev_id, u32 chan_id, struct ub_idev *idev, bool check_dev_status)
{
    if (ka_unlikely(ubdrv_get_device_status(dev_id) == UBDRV_DEVICE_DEAD) && (check_dev_status == true)) {
        ubdrv_warn("Device status is dead. (dev_id=%u;chan_id=%u)\n", dev_id, chan_id);
        return -ENODEV;
    }
    if (ka_unlikely(idev->status == UBDRV_UDMA_UNPROBE)) {
        ubdrv_warn("Udma device status is unprobe. (dev_id=%u;chan_id=%u)\n", dev_id, chan_id);
        return -ENODEV;
    }
    return 0;
}

int ubdrv_interval_poll_send_jfs_jfc(struct ascend_ub_jetty_ctrl *jetty_ctrl, u64 user_ctx, u32 cnt,
                                     struct ubcore_cr *cr, struct ubdrv_msg_chan_stat *stat, bool check_dev_status)
{
    struct ubcore_jfc *jfc = jetty_ctrl->jfs_jfc;
    int ret = 0;
    u32 i;

    for (i = 0; i < cnt; i++) {
        ret = ubdrv_check_msg_loop_condition(stat->dev_id, stat->chan_id, jetty_ctrl->idev, check_dev_status);
        if (ret != 0) {
            return ret;
        }
        ret = ubcore_poll_jfc(jfc, ASCEND_UB_POLL_JFC_ONE, cr);
        if (ret == 0) {
            ka_system_usleep_range(UBDRV_WAIT_JFC_MIN, UBDRV_WAIT_JFC_MAX);
        } else if (ret < 0) {
            ubdrv_err("Poll send jfs cqe failed, retry poll cqe. (ret=%d;jfc_id=%u;dev_id=%u;chan_id=%u;\
            udma_name=%s)\n",
                      ret, jfc->id, stat->dev_id, stat->chan_id, jfc->ub_dev->dev_name);
            stat->tx_poll_cqe_err++;
        } else {
            if (cr->user_ctx == user_ctx) {
                break;
            }
            ubdrv_warn("Poll send jfs_jfr an old cqe, continue poll last cqe. (ret=%d;jfc_id=%u;expect_ctx=%llu;\
            cr_ctx=%llu;dev_id=%u;chan_id=%u;udma_name=%s)\n",
                       ret, jfc->id, user_ctx, cr->user_ctx, stat->dev_id, stat->chan_id, jfc->ub_dev->dev_name);
        }
    }
    if (i == cnt) {
        ubdrv_err("Poll send jfs cqe timeout. (cnt=%u;jfc_id=%u;jfs_id=%u;dev_id=%u;chan_id=%u;udma_name=%s)\n", cnt,
                  jfc->id, stat->tx_jfs_id, stat->dev_id, stat->chan_id, jfc->ub_dev->dev_name);
        stat->tx_cqe_timeout++;
        ubdrv_print_ctx_info(stat->dev_id, stat->chan_id, jetty_ctrl->jfs, NULL, jetty_ctrl->jfs_jfc);
        ubdrv_msg_tx_dfx_info(stat);
        return -ETIMEDOUT;
    }
    stat->tx_cqe++;
    if (cr->status != UBCORE_CR_SUCCESS) {
        ubdrv_err("CR status is error. (cr_status=%d;jfc_id=%u;dev_id=%u;chan_id=%u;udma_name=%s)\n", cr->status,
                  jfc->id, stat->dev_id, stat->chan_id, jfc->ub_dev->dev_name);
        stat->tx_cqe_status_err++;
        ubdrv_print_ctx_info(stat->dev_id, stat->chan_id, jetty_ctrl->jfs, NULL, jetty_ctrl->jfs_jfc);
    }
    return 0;
}

int ubdrv_interval_poll_recv_jfs_jfc(struct ascend_ub_jetty_ctrl *jetty_ctrl, u64 user_ctx, u32 cnt,
                                     struct ubcore_cr *cr, struct ubdrv_msg_chan_stat *stat)
{
    struct ubcore_jfc *jfc = jetty_ctrl->jfs_jfc;
    u32 jfc_id = jfc->id;
    int ret = 0;
    u32 i;

    for (i = 0; i < cnt; i++) {
        ret = ubcore_poll_jfc(jfc, ASCEND_UB_POLL_JFC_ONE, cr);
        if (ret == 0) {
            ka_system_usleep_range(UBDRV_WAIT_JFC_MIN, UBDRV_WAIT_JFC_MAX);
        } else if (ret < 0) {
            ubdrv_err("Poll recv jfs cqe failed, retry poll cqe. (ret=%d;jfs_jfc_id=%u)\n", ret, jfc_id);
            stat->rx_tx_poll_cqe_err++;
        } else {
            if (cr->user_ctx == user_ctx) {
                break;
            }
            // poll old cqe
            ubdrv_warn("Poll recv jfs_jfr an old cqe, continue poll last cqe. (ret=%d;expect_ctx=%llu;\
                cr_ctx=%llu;jfs_jfc_id=%u)\n",
                       ret, user_ctx, cr->user_ctx, jfc_id);
        }
    }

    if (i == cnt) {
        ubdrv_err("Poll recv jfs cqe timeout. (cnt=%u;jfs_id=%u;jfs_jfc_id=%u)\n", cnt, stat->rx_jfs_id, jfc_id);
        stat->rx_tx_cqe_timeout++;
        ubdrv_print_ctx_info(stat->dev_id, stat->chan_id, jetty_ctrl->jfs, NULL, jetty_ctrl->jfs_jfc);
        return -ETIMEDOUT;
    }
    stat->rx_tx_cqe++;
    if (cr->status != UBCORE_CR_SUCCESS) {
        ubdrv_err("CR status is error. (cr_status=%d;jfs_jfc_id=%u)\n", cr->status, jfc_id);
        stat->rx_tx_cqe_status_err++;
        ubdrv_print_ctx_info(stat->dev_id, stat->chan_id, jetty_ctrl->jfs, NULL, jetty_ctrl->jfs_jfc);
    }
    return 0;
}

STATIC int ubdrv_interval_poll_jfr_jfc(u32 dev_id, struct ascend_ub_jetty_ctrl *jetty_ctrl, u32 cnt,
                                       struct ubcore_cr *cr, struct ubdrv_msg_chan_stat *stat)
{
    struct ubcore_jfc *jfc = jetty_ctrl->jfr_jfc;
    u32 i = 0, retry_cnt = cnt;
    int ret = 0;

    if (ubdrv_get_msg_retry_cnt(dev_id) == ASCEND_MSG_MAX_RETRY_CNT) {
        retry_cnt = MSG_MAX_WAIT_CNT_BEFORE_ONLINE;
    }
    for (i = 0; i < retry_cnt; i++) {
        ret = ubdrv_check_msg_loop_condition(dev_id, stat->chan_id, jetty_ctrl->idev, true);
        if (ret != 0) {
            return ret;
        }
        ret = ubcore_poll_jfc(jfc, ASCEND_UB_POLL_JFC_ONE, cr);
        if (ret == 0) {
            ka_system_usleep_range(UBDRV_WAIT_JFC_MIN, UBDRV_WAIT_JFC_MAX);
        } else if (ret < 0) {
            ubdrv_err("Polled cqe failed. (ret=%d;dev_id=%u;jfc_id=%u;dev_name=%s)\n", ret, dev_id, jfc->id,
                      jfc->ub_dev->dev_name);
            stat->tx_recv_poll_cqe_err++;
        } else {
            break;
        }
    }
    if (i >= retry_cnt) {
        ubdrv_err("Polled cqe timeout. (dev_id=%u;real_cnt=%u;expect_cnt=%u;jfc_id=%u;dev_name=%s)\n", dev_id, i,
                  retry_cnt, jfc->id, jfc->ub_dev->dev_name);
        stat->tx_recv_cqe_timeout++;
        ubdrv_print_ctx_info(stat->dev_id, stat->chan_id, NULL, jetty_ctrl->jfr, jetty_ctrl->jfr_jfc);
        ubdrv_msg_tx_dfx_info(stat);
        return -ETIMEDOUT;
    }
    stat->tx_recv_cqe++;
    if (cr->status != UBCORE_CR_SUCCESS) {
        ubdrv_err("CR status is error. (dev_id=%u;cr_status=%d;jfc_id=%u;jfr_id=%u;dev_name=%s)\n", dev_id, cr->status,
                  jfc->id, stat->tx_jfr_id, jfc->ub_dev->dev_name);
        stat->tx_recv_cqe_status_err++;
        ubdrv_print_ctx_info(stat->dev_id, stat->chan_id, NULL, jetty_ctrl->jfr, jetty_ctrl->jfr_jfc);
    }
    return 0;
}

struct ascend_ub_msg_desc *ubdrv_wait_sync_msg_rqe(u32 dev_id, struct ascend_ub_jetty_ctrl *jetty_ctrl, u32 msg_num,
                                                   u32 cnt, struct ubdrv_msg_chan_stat *stat)
{
    struct ascend_ub_msg_desc *desc = NULL;
    struct ubcore_cr cr = {0};
    u32 seg_id = 0;
    int ret = 0;

retry_poll_rqe:
    ret = ubdrv_check_msg_loop_condition(dev_id, stat->chan_id, jetty_ctrl->idev, true);
    if (ret != 0) {
        return KA_ERR_PTR(ret);
    }

    ret = ubdrv_interval_poll_jfr_jfc(dev_id, jetty_ctrl, cnt, &cr, stat);
    if ((ret != 0) || (cr.status != UBCORE_CR_SUCCESS)) {
        ret = ubdrv_msg_ret_cal(ret, cr.status);
        return KA_ERR_PTR(ret);
    }
    seg_id = (u32)cr.user_ctx;
    if (seg_id >= jetty_ctrl->jfr_msg_depth) {
        ubdrv_err("Para cr error, seg id invalid. (dev_id=%u;chan_id=%u;seg_id=%u;max_seg_id=%u;dev_name=%s)\n", dev_id,
                  stat->chan_id, seg_id, jetty_ctrl->jfr_msg_depth, jetty_ctrl->ubc_dev->dev_name);
        return KA_ERR_PTR(-EINVAL);
    }
    desc = ubdrv_get_recv_desc(jetty_ctrl, seg_id);
    desc->seg_id = seg_id;
    if (desc->msg_num != msg_num) {
        stat->tx_recv_data_err++;
        ubdrv_err("Para cr error, msg num invalid. (dev_id=%u;chan_id=%u;msg_num=%u;cr_msg_num=%u;dev_name=%s)\n",
                  dev_id, stat->chan_id, msg_num, desc->msg_num, jetty_ctrl->ubc_dev->dev_name);
        (void)ubdrv_post_jfr_wr(jetty_ctrl->jfr, jetty_ctrl->recv_seg, desc, jetty_ctrl->recv_msg_len, seg_id);
        goto retry_poll_rqe;
    }
    return desc;
}

int ubdrv_rebuild_jfs_jfc(u32 dev_id, struct ascend_ub_jetty_ctrl *jetty_ctrl)
{
    int ret;

    if (jetty_ctrl->jfs != NULL) {
        ubdrv_delete_sync_jfs(jetty_ctrl);
        jetty_ctrl->jfs = NULL;
    }
    if (jetty_ctrl->jfs_jfc != NULL) {
        (void)ubcore_delete_jfc(jetty_ctrl->jfs_jfc);
        jetty_ctrl->jfs_jfc = NULL;
    }
    jetty_ctrl->jfs_jfc = ubdrv_create_jfc(jetty_ctrl, NULL);
    if (KA_IS_ERR_OR_NULL(jetty_ctrl->jfs_jfc)) {
        ubdrv_err("Rebuild jfc failed. (dev_id=%u;dev_name=%s;errno=%ld)\n", dev_id, jetty_ctrl->ubc_dev->dev_name,
                  KA_PTR_ERR(jetty_ctrl->jfs_jfc));
        jetty_ctrl->jfs_jfc = NULL;
        return -EUBINITFAIL;
    }

    ret = ubdrv_create_sync_jfs(jetty_ctrl);
    if (ret != 0) {
        ubdrv_err("Rebuild jfs failed. (ret=%d;dev_id=%u;dev_name=%s)", ret, dev_id, jetty_ctrl->ubc_dev->dev_name);
        goto del_rebuild_jfc;
    }
    return 0;

del_rebuild_jfc:
    (void)ubcore_delete_jfc(jetty_ctrl->jfs_jfc);
    jetty_ctrl->jfs_jfc = NULL;
    return ret;
}

u32 ubdrv_record_resp_time(u64 pre_jiffies, const char *str, u32 timeout)
{
    u64 c_jiffies = ka_system_ktime_get_raw_ns();
    u32 resp_time;

    resp_time = (c_jiffies > pre_jiffies) ? (c_jiffies - pre_jiffies) / KA_NSEC_PER_USEC : 0;
    if (resp_time > timeout) {
        ubdrv_info("Get resp_time. (resp_time=%uus; cpu=%d; str=\"%s\")\n", resp_time, raw_smp_processor_id(), str);
    }
    return resp_time;
}

#define UB_JFS_CTX_SIZE 256
#define UB_JFR_CTX_SIZE 64
#define UB_JFC_CTX_SIZE 128
STATIC void ubdrv_print_jfs_ctx(u32 dev_id, struct ubcore_jfs *jfs)
{
    char buf[UB_JFS_CTX_SIZE + 1] = {0};
    u32 id = jfs->jfs_id.id;
    int ret;

    ret = ubcore_get_jfs_opt(jfs, UBCORE_JFS_FULL_CTX, buf, UB_JFS_CTX_SIZE, NULL);
    if (ret != 0) {
        ubdrv_err("Get jfs ctx fail. (dev_id=%u;ret=%d)\n", dev_id, ret);
        return;
    }
    ubdrv_err_hex_dump(buf, UB_JFS_CTX_SIZE, "JFS(dev_id=%u;jfs_id=%u)CTX", dev_id, id);
}

STATIC void ubdrv_print_jfc_ctx(u32 dev_id, struct ubcore_jfc *jfc)
{
    char buf[UB_JFC_CTX_SIZE + 1] = {0};
    u32 id = jfc->id;
    int ret;

    ret = ubcore_get_jfc_opt(jfc, UBCORE_JFC_FULL_CTX, buf, UB_JFC_CTX_SIZE, NULL);
    if (ret != 0) {
        ubdrv_err("Get jfc ctx fail. (dev_id=%u;ret=%d)\n", dev_id, ret);
        return;
    }
    ubdrv_err_hex_dump(buf, UB_JFC_CTX_SIZE, "JFC(dev_id=%u;jfc_id=%u)CTX", dev_id, id);
}

STATIC void ubdrv_print_jfr_ctx(u32 dev_id, struct ubcore_jfr *jfr)
{
    char buf[UB_JFR_CTX_SIZE + 1] = {0};
    u32 id = jfr->jfr_id.id;
    int ret;

    ret = ubcore_get_jfr_opt(jfr, UBCORE_JFR_FULL_CTX, buf, UB_JFR_CTX_SIZE, NULL);
    if (ret != 0) {
        ubdrv_err("Get jfr ctx fail. (dev_id=%u;ret=%d)\n", dev_id, ret);
        return;
    }
    ubdrv_err_hex_dump(buf, UB_JFR_CTX_SIZE, "JFR(dev_id=%u;jfr_id=%u)CTX", dev_id, id);
}

void ubdrv_print_ctx_info(u32 dev_id, u32 chan_id, struct ubcore_jfs *jfs, struct ubcore_jfr *jfr,
                          struct ubcore_jfc *jfc)
{
    static unsigned long last_log_time[ASCEND_UB_DEV_MAX_NUM] = {0};
    static unsigned long ctx_total_cnt[ASCEND_UB_DEV_MAX_NUM] = {0};
    static unsigned long ctx_print_cnt[ASCEND_UB_DEV_MAX_NUM] = {0};

    if (ka_system_time_after(ka_jiffies, (last_log_time[dev_id] + 10 * KA_HZ)) == false) {
        ctx_total_cnt[dev_id]++;
        return;
    }
    last_log_time[dev_id] = ka_jiffies;
    ctx_print_cnt[dev_id]++;
    if (ctx_total_cnt[dev_id] > ctx_print_cnt[dev_id]) {
        ubdrv_info("Print ctx limit. (dev_id=%u;total_cnt=%lu;print_cnt=%lu)\n", dev_id, ctx_total_cnt[dev_id],
                   ctx_print_cnt[dev_id]);
    }

    if (jfs != NULL) {
        ubdrv_print_jfs_ctx(dev_id, jfs);
    }
    if (jfr != NULL) {
        ubdrv_print_jfr_ctx(dev_id, jfr);
    }
    if (jfc != NULL) {
        ubdrv_print_jfc_ctx(dev_id, jfc);
    }
}