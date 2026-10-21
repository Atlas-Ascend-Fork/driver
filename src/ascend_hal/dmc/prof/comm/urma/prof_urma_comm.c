/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef UT_TEST
#include "dms_user_interface.h"
#include "comm_user_interface.h"
#include "prof_communication.h"
#include "prof_urma_comm.h"
#include "urma_api.h"

struct prof_urma_info g_prof_urma_info[PROF_URMA_MAX_DEVNUM] = {{0}};
uint32_t g_prof_devid_to_urma_devid[DEV_NUM];
struct prof_urma_info *prof_get_urma_info(uint32_t dev_id)
{
    uint32_t urma_id = g_prof_devid_to_urma_devid[dev_id];

    if (urma_id >= PROF_URMA_MAX_DEVNUM) {
        return NULL;
    }

    return &g_prof_urma_info[urma_id];
}

STATIC urma_jfc_t *prof_create_jfc(struct prof_urma_info *urma_info, struct prof_urma_chan_info *urma_chan_info)
{
    urma_jfc_t *jfc = NULL;
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_status_t urma_ret;

    (void)urma_chan_info;

    jfc_cfg.jfce = urma_info->jfce;
    jfc_cfg.depth = TRS_JFC_MAX_DEPTH;
    jfc = urma_create_jfc(urma_info->urma_ctx, &jfc_cfg);
    if (jfc == NULL) {
#ifndef PROF_UNIT_TEST
        PROF_ERR("Failed to create jfc.\n");
        return NULL;
#endif
    }

    urma_ret = urma_rearm_jfc(jfc, false);
    if (urma_ret != URMA_SUCCESS) {
#ifndef PROF_UNIT_TEST
        PROF_ERR("Failed to rearm urma jfc . (ret=%d)\n", urma_ret);
        (void)urma_delete_jfc(jfc);
        return NULL;
#endif
    }

    return jfc;
}

STATIC void prof_destroy_jfc(urma_jfc_t *jfc)
{
    (void)urma_delete_jfc(jfc);
}

int prof_create_jfs(struct prof_urma_info *urma_info, struct prof_urma_chan_info *urma_chan_info)
{
    urma_chan_info->jfs_cfg.jfc = prof_create_jfc(urma_info, urma_chan_info);
    if (urma_chan_info->jfs_cfg.jfc == NULL) {
        PROF_ERR("Failed to create jfc for jfs.\n");
        return DRV_ERROR_INNER_ERR;
    }

    urma_chan_info->jfs_cfg.depth = TRS_JFS_MAX_DEPTH;
    urma_chan_info->jfs_cfg.trans_mode = URMA_TM_RM;
    urma_chan_info->jfs_cfg.priority = ASCEND_URMA_PRIORITY_LOW; /* Lowest priority */
    urma_chan_info->jfs_cfg.rnr_retry = URMA_TYPICAL_RNR_RETRY;
    urma_chan_info->jfs_cfg.err_timeout = URMA_TYPICAL_ERR_TIMEOUT;
    urma_chan_info->jfs_cfg.max_sge = 1;
    urma_chan_info->jfs = urma_create_jfs(urma_info->urma_ctx, &urma_chan_info->jfs_cfg);
    if (urma_chan_info->jfs == NULL) {
        PROF_ERR("Failed to create jfs.\n");
        prof_destroy_jfc(urma_chan_info->jfs_cfg.jfc);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

void prof_destroy_jfs(struct prof_urma_chan_info *urma_chan_info)
{
    (void)urma_delete_jfs(urma_chan_info->jfs);
    prof_destroy_jfc(urma_chan_info->jfs_cfg.jfc);
}

int prof_create_jfr(struct prof_urma_info *urma_info, struct prof_urma_chan_info *urma_chan_info)
{
    urma_chan_info->jfr_cfg.jfc = prof_create_jfc(urma_info, urma_chan_info);
    if (urma_chan_info->jfr_cfg.jfc == NULL) {
#ifndef PROF_UNIT_TEST
        PROF_ERR("Failed to create jfc for jfr.\n");
        return DRV_ERROR_INNER_ERR;
#endif
    }

    urma_chan_info->jfr_cfg.depth = TRS_JFR_MAX_DEPTH;
    urma_chan_info->jfr_cfg.trans_mode = URMA_TM_RM;
    urma_chan_info->jfr_cfg.min_rnr_timer = URMA_TYPICAL_MIN_RNR_TIMER;
    urma_chan_info->jfr_cfg.max_sge = 1;
    urma_chan_info->jfr_cfg.flag.bs.token_policy = URMA_TOKEN_PLAIN_TEXT;
    urma_chan_info->jfr_cfg.token_value = urma_info->token;
    urma_chan_info->jfr = urma_create_jfr(urma_info->urma_ctx, &urma_chan_info->jfr_cfg);
    if (urma_chan_info->jfr == NULL) {
#ifndef PROF_UNIT_TEST
        PROF_ERR("Failed to create jfr.\n");
        prof_destroy_jfc(urma_chan_info->jfr_cfg.jfc);
        return DRV_ERROR_INNER_ERR;
#endif
    }

    return DRV_ERROR_NONE;
}

void prof_destroy_jfr(struct prof_urma_chan_info *urma_chan_info)
{
    (void)urma_delete_jfr(urma_chan_info->jfr);
    prof_destroy_jfc(urma_chan_info->jfr_cfg.jfc);
}

STATIC drvError_t prof_create_jetty(struct prof_urma_info *urma_info, struct prof_urma_chan_info *urma_chan_info)
{
    urma_jetty_cfg_t jetty_cfg = {0};
    drvError_t ret;

    ret = prof_create_jfs(urma_info, urma_chan_info);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to create jfs. (ret=%d)\n", (int)ret);
        return ret;
    }

    ret = prof_create_jfr(urma_info, urma_chan_info);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to create jfr. (ret=%d)\n", (int)ret);
        (void)prof_destroy_jfs(urma_chan_info);
        return ret;
    }

    jetty_cfg.jfs_cfg = urma_chan_info->jfs_cfg;
    jetty_cfg.flag.bs.share_jfr = 1;
    jetty_cfg.shared.jfc = urma_chan_info->jfr_cfg.jfc;
    jetty_cfg.shared.jfr = urma_chan_info->jfr;
    urma_chan_info->jetty = urma_create_jetty(urma_info->urma_ctx, &jetty_cfg);
    if (urma_chan_info->jetty == NULL) {
#ifndef PROF_UNIT_TEST
        PROF_ERR("Failed to create jetty.\n");
        (void)prof_destroy_jfr(urma_chan_info);
        (void)prof_destroy_jfs(urma_chan_info);
        return DRV_ERROR_INNER_ERR;
#endif
    }

    return DRV_ERROR_NONE;
}

void prof_destroy_jetty(struct prof_urma_chan_info *urma_chan_info)
{
    (void)urma_delete_jetty(urma_chan_info->jetty);
    (void)prof_destroy_jfr(urma_chan_info);
    (void)prof_destroy_jfs(urma_chan_info);
}

drvError_t prof_urma_post_jetty_recv_wr(urma_jetty_t *jetty, urma_target_seg_t *tseg, uint64_t user_ctx, uint32_t depth)
{
    urma_jfr_wr_t *bad_wr = NULL;
    urma_jfr_wr_t wr;
    urma_sge_t sge;
    drvError_t ret;
    uint32_t i;

    wr.src.sge = &sge;
    wr.src.sge->tseg = tseg;
    wr.src.sge->addr = tseg->seg.ubva.va;
    wr.src.sge->len = (uint32_t)tseg->seg.len;
    wr.src.num_sge = 1;
    wr.user_ctx = user_ctx;
    wr.next = NULL;
    for (i = 0; i < depth; i++) {
        ret = urma_post_jetty_recv_wr(jetty, &wr, &bad_wr);
        if (ret != DRV_ERROR_NONE) {
#ifndef PROF_UNIT_TEST
            PROF_ERR("Failed to call urma_post_jetty_recv_wr. (user_ctx=%lu, i=%d)\n", wr.user_ctx, i);
            return ret;
#endif
        }
    }

    PROF_INFO("urma_post_jetty_recv_wr success. (user_ctx=%lu, depth=%u)\n", wr.user_ctx, depth);
    return DRV_ERROR_NONE;
}

drvError_t prof_urma_post_recv(struct prof_urma_chan_info *urma_chan_info, uint32_t depth)
{
    uint64_t user_ctx = 0;

    user_ctx = (uint64_t)urma_chan_info->dev_id << 32; /* 32: dev_id in hign 32 bits */
    user_ctx |= urma_chan_info->chan_id;

    return prof_urma_post_jetty_recv_wr(urma_chan_info->jetty, urma_chan_info->user_buff_tseg, user_ctx, depth);
}

STATIC drvError_t prof_urma_write_with_imm_post_proc(uint64_t user_ctx, uint32_t imm_data)
{
    struct prof_comm_core_notifier *notifier = prof_comm_get_notifier();
    uint32_t w_ptr, dev_id, chan_id;
    drvError_t ret;

    dev_id = (uint32_t)((user_ctx >> 32) & 0xffffffff); /* 32: dev_id in high 32 bits */
    chan_id = (uint32_t)(user_ctx & 0xffffffff);
    w_ptr = imm_data & (0x7FFFFFFF); /* 0x7FFFFFFF get the new wptr */

    ret = notifier->chan_report(dev_id, chan_id, &w_ptr, sizeof(uint32_t), false);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to post jetty. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return ret;
    }

    /* debug log need to be del before merge*/
    PROF_INFO("Receive data success. (dev_id=%u, chan_id=%u, w_ptr=%u)\n", dev_id, chan_id, w_ptr);

    return DRV_ERROR_NONE;
}

static void prof_show_jfc_ctx(urma_jfc_t *jfc)
{
    char buf[PROF_URMA_JFC_CTX_LEN] = {0};
    int ret = 0;

    if (jfc == NULL) {
        return;
    }

    ret = urma_get_jfc_opt(jfc, URMA_JFC_FULL_CTX, buf, PROF_URMA_JFC_CTX_LEN);
    if (ret != 0) {
        PROF_ERR("Failed to get jfc ctx. (jfc_id=%u; ret=%d)\n", jfc->jfc_id.id, ret);
        return;
    }

    PROF_ERR_HEX_DUMP(buf, PROF_URMA_JFC_CTX_LEN, "Prof jfc context. (jfc_id=%u)", jfc->jfc_id.id);
}

STATIC void prof_urma_recv_data(struct prof_urma_info *urma_info)
{
    int timeout_ms = 500; /* 500 ms */
    urma_status_t urma_ret;
    urma_jfc_t *jfc = NULL;
    uint32_t ack_cnt = 1;
    drvError_t ret;
    urma_cr_t cr;
    int cnt;

    cnt = ascend_urma_wait_jfc(urma_info->jfce, 1, timeout_ms, &jfc);
    if (cnt != 1) {
        return;
    }

    cnt = urma_poll_jfc(jfc, 1, &cr);
    if (cr.status == URMA_CR_WR_FLUSH_ERR_DONE) {
        goto rearm_jfc;
    }
    if (cnt == 0) {
        PROF_WARN("The result of poll jfc is 0.\n");
        goto rearm_jfc;
    }
    if (cnt < 0 || cr.status != URMA_CR_SUCCESS) {
        PROF_ERR("Failed to poll jfc. (cnt=%d, cr_status=%d)\n", cnt, cr.status);
        prof_show_jfc_ctx(jfc);
        goto rearm_jfc;
    }

    if (cr.flag.bs.s_r == 0) {
        PROF_INFO("Send success. (jfc_id=%u)\n", jfc->jfc_id.id);
        goto rearm_jfc;
    }

    if (cr.opcode == URMA_CR_OPC_WRITE_WITH_IMM) {
        ret = prof_urma_write_with_imm_post_proc(cr.user_ctx, (uint32_t)cr.imm_data);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("Failed to post process write with imm. (ret=%d, jfc_id=%u)\n", ret, jfc->jfc_id.id);
        }
    }

rearm_jfc:
    urma_ack_jfc((urma_jfc_t **)&jfc, &ack_cnt, 1);
    urma_ret = urma_rearm_jfc(jfc, false);
    if (urma_ret != URMA_SUCCESS) {
        PROF_ERR("Failed to rearm urma jfc. (ret=%d)\n", urma_ret);
    }
}

STATIC void prof_urma_local_seg_pack(uint64_t data_buff, uint64_t data_buff_len, urma_seg_cfg_t *seg_cfg,
                                     struct prof_urma_info *urma_info)
{
    urma_reg_seg_flag_t flag = {.bs.token_policy = URMA_TOKEN_PLAIN_TEXT,
                                .bs.cacheable = URMA_NON_CACHEABLE,
                                .bs.access = URMA_ACCESS_READ | URMA_ACCESS_WRITE | URMA_ACCESS_ATOMIC,
                                .bs.token_id_valid = 1,
                                .bs.reserved = 0};

    seg_cfg->va = data_buff;
    seg_cfg->len = data_buff_len;
    seg_cfg->token_value = urma_info->token;
    seg_cfg->flag = flag;
    seg_cfg->token_id = urma_info->token_id;
}

drvError_t prof_urma_local_seg_register(struct prof_urma_start_para *urma_start_para, struct prof_urma_info *urma_info,
                                        struct prof_urma_chan_info *urma_chan_info)
{
    urma_seg_cfg_t seg_cfg = {0};
    int ret;

    prof_urma_local_seg_pack(urma_start_para->data_buff_addr, urma_start_para->data_buff_len, &seg_cfg, urma_info);
    urma_chan_info->user_buff_tseg = urma_register_seg(urma_info->urma_ctx, &seg_cfg);
    if (urma_chan_info->user_buff_tseg == NULL) {
#ifndef PROF_UNIT_TEST
        PROF_ERR("Failed to register data buff segment.\n");
        return DRV_ERROR_INNER_ERR;
#endif
    }

    seg_cfg.va = urma_start_para->rptr_addr;
    seg_cfg.len = urma_start_para->rptr_size;
    urma_chan_info->local_r_ptr_tseg = urma_register_seg(urma_info->urma_ctx, &seg_cfg);
    if (urma_chan_info->local_r_ptr_tseg == NULL) {
#ifndef PROF_UNIT_TEST
        urma_unregister_seg(urma_chan_info->user_buff_tseg);
        PROF_ERR("Failed to register read ptr addr segment. (rptr_addr=%pK)\n", urma_start_para->rptr_addr);
        return DRV_ERROR_INNER_ERR;
#endif
    }

    urma_chan_info->user_buff = (char *)urma_start_para->data_buff_addr;
    urma_chan_info->user_buff_len = urma_start_para->data_buff_len;
    urma_chan_info->local_r_ptr = (char *)urma_start_para->rptr_addr;

    ret = prof_urma_post_recv(urma_chan_info, urma_chan_info->jfr_cfg.depth);
    if (ret != DRV_ERROR_NONE) {
#ifndef PROF_UNIT_TEST
        urma_unregister_seg(urma_chan_info->local_r_ptr_tseg);
        urma_unregister_seg(urma_chan_info->user_buff_tseg);
        PROF_ERR("Failed to post jetty. (jetty_id=%u, segment_len=%u)\n", urma_chan_info->jetty->jetty_id.id,
                 urma_chan_info->user_buff_len);
        return ret;
#endif
    }

    return DRV_ERROR_NONE;
}

void prof_urma_local_seg_unregister(struct prof_urma_chan_info *urma_chan_info)
{
    urma_unregister_seg(urma_chan_info->local_r_ptr_tseg);
    urma_unregister_seg(urma_chan_info->user_buff_tseg);
}

drvError_t prof_urma_remote_info_import(struct prof_urma_info *urma_info, struct prof_urma_chan_info *urma_chan_info,
                                        struct prof_start_sync_msg *sync_msg)
{
#ifdef SSAPI_USE_MAMI
    urma_get_tp_cfg_t tpid_cfg = {
        .trans_mode = URMA_TM_RM,
        .local_eid = urma_info->urma_ctx->eid, // local_eid
        .flag.bs.ctp = true,
    };
    uint32_t tp_cnt = 1;
    urma_tp_info_t tpid_info = {0};
    urma_status_t status;
    urma_active_tp_cfg_t active_tp_cfg = {0};
#endif
    int ret;
    urma_token_t remote_token;
    uint32_t dev_id = urma_chan_info->dev_id;
    urma_rjetty_t rjetty = {0};
    urma_seg_t r_ptr_seg = {0};
    urma_import_seg_flag_t flag = {.bs.cacheable = URMA_NON_CACHEABLE,
                                   .bs.access = URMA_ACCESS_READ | URMA_ACCESS_WRITE | URMA_ACCESS_ATOMIC,
                                   .bs.mapping = URMA_SEG_NOMAP,
                                   .bs.reserved = 0};

    rjetty.jetty_id.id = sync_msg->jetty_id;
    ret = memcpy_s(&rjetty.jetty_id.eid, PROF_EID_SIZE, sync_msg->eid_raw, PROF_EID_SIZE);
    if (ret != 0) {
        PROF_ERR("Memcpy failed. (dev_id=%u, chan_id=%u)\n", dev_id, urma_chan_info->chan_id);
        return DRV_ERROR_INNER_ERR;
    }
    rjetty.trans_mode = URMA_TM_RM;
    rjetty.type = URMA_JETTY;
    rjetty.flag.bs.token_policy = URMA_TOKEN_PLAIN_TEXT;
#ifdef SSAPI_USE_MAMI
    ret = memcpy_s(tpid_cfg.peer_eid.raw, PROF_EID_SIZE, sync_msg->eid_raw, PROF_EID_SIZE);
    if (ret != 0) {
        PROF_ERR("Memcpy failed. (dev_id=%u, chan_id=%u)\n", dev_id, urma_chan_info->chan_id);
        return DRV_ERROR_INNER_ERR;
    }

    status = ascend_urma_get_tp_list(urma_info->urma_ctx, &tpid_cfg, &tp_cnt, &tpid_info);
    if ((status != 0) || (tp_cnt == 0)) {
        PROF_ERR("urma get tp list failed (status=%d; tp_cnt=%u)\n", status, tp_cnt);
        return DRV_ERROR_INNER_ERR;
    }

    active_tp_cfg.tp_handle = tpid_info.tp_handle;
    rjetty.tp_type = URMA_CTP;
    remote_token.token = sync_msg->token_val;
    urma_chan_info->tjetty = ascend_urma_import_jetty_ex(urma_info->urma_ctx, &rjetty, &remote_token, &active_tp_cfg);
#else
    urma_chan_info->tjetty = urma_import_jetty(urma_info->urma_ctx, &rjetty, &remote_token);
#endif
    if (urma_chan_info->tjetty == NULL) {
#ifndef PROF_UNIT_TEST
        PROF_ERR("Failed to import remote jetty. (dev_id=%u, chan_id=%u, remote_jetty_id=%u, remote_eid=%u)\n", dev_id,
                 urma_chan_info->chan_id, sync_msg->jetty_id, sync_msg->eid);
        return DRV_ERROR_INNER_ERR;
#endif
    }

    r_ptr_seg.attr.bs.token_policy = URMA_TOKEN_PLAIN_TEXT;
    r_ptr_seg.ubva.va = (uint64_t)(uintptr_t)sync_msg->r_ptr;
    ret = memcpy_s(&r_ptr_seg.ubva.eid, PROF_EID_SIZE, sync_msg->eid_raw, PROF_EID_SIZE);
    if (ret != 0) {
        (void)ascend_urma_unimport_jetty_ex(urma_chan_info->tjetty);
        urma_chan_info->tjetty = NULL;
        PROF_ERR("Memcpy failed. (dev_id=%u, chan_id=%u)\n", dev_id, urma_chan_info->chan_id);
        return DRV_ERROR_INNER_ERR;
    }
    r_ptr_seg.token_id = sync_msg->token_id;
    urma_chan_info->r_ptr_tseg = urma_import_seg(urma_info->urma_ctx, &r_ptr_seg, &remote_token, 0, flag);
    if (urma_chan_info->r_ptr_tseg == NULL) {
#ifndef PROF_UNIT_TEST
        (void)ascend_urma_unimport_jetty_ex(urma_chan_info->tjetty);
        urma_chan_info->tjetty = NULL;
        PROF_ERR("Failed to import remote segment. (dev_id=%u, chan_id=%u, remote_eid=%u, "
                 "remote_token_id=%u)\n",
                 dev_id, urma_chan_info->chan_id, sync_msg->eid, sync_msg->token_id);
        return DRV_ERROR_INNER_ERR;
#endif
    }

    PROF_INFO("Import remote jetty and segment successfully. (dev_id=%u, chan_id=%u, remote_jetty_id=%u, "
              "remote_eid=%u, remote_token_id=%u)\n",
              dev_id, urma_chan_info->chan_id, sync_msg->jetty_id, sync_msg->eid, sync_msg->token_id);

    return DRV_ERROR_NONE;
}

void prof_urma_remote_info_unimport(struct prof_urma_info *urma_info, struct prof_urma_chan_info *urma_chan_info)
{
    (void)urma_info;

    urma_unimport_seg(urma_chan_info->r_ptr_tseg);
    ascend_urma_unimport_jetty_ex(urma_chan_info->tjetty);
}

drvError_t prof_urma_write_remote_rptr(uint32_t dev_id, uint32_t chan_id, struct prof_urma_chan_info *urma_chan_info)
{
    urma_sge_t src_sge, dst_sge;
    urma_jfs_wr_t *bad_wr = NULL;
    urma_jfs_wr_t wr = {0};
    drvError_t ret;

    wr.opcode = URMA_OPC_WRITE;
    wr.tjetty = urma_chan_info->tjetty;
    wr.flag.bs.place_order = 2;     /* 2: strong order */
    wr.flag.bs.comp_order = 1;      /* 1: Completion order with previous WR */
    wr.flag.bs.complete_enable = 1; /* 1: Notify local process after the task is completed */
    wr.rw.src.sge = &src_sge;
    wr.rw.src.sge->addr = (uint64_t)(uintptr_t)urma_chan_info->local_r_ptr;
    wr.rw.src.sge->len = sizeof(uint32_t);
    wr.rw.src.sge->tseg = urma_chan_info->local_r_ptr_tseg;
    wr.rw.src.num_sge = 1;
    wr.rw.dst.sge = &dst_sge;
    wr.rw.dst.sge->addr = urma_chan_info->r_ptr_tseg->seg.ubva.va;
    wr.rw.dst.sge->len = sizeof(uint32_t);
    wr.rw.dst.sge->tseg = urma_chan_info->r_ptr_tseg;
    wr.rw.dst.num_sge = 1;

    ret = urma_post_jetty_send_wr(urma_chan_info->jetty, &wr, &bad_wr);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to write read ptr. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    /* debug info , need to be del before merge */
    PROF_INFO("Post send read ptr success. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
    return DRV_ERROR_NONE;
}

STATIC void *prof_urma_recv_thread(void *args)
{
    struct prof_urma_info *urma_info = (struct prof_urma_info *)args;
    while (urma_info->recv_thread_status) {
        prof_urma_recv_data(urma_info);
    }

    return NULL;
}

void prof_urma_recv_thread_create(struct prof_urma_info *urma_info)
{
    pthread_attr_t attr;
    int ret;

    urma_info->recv_thread_status = 1;

    (void)pthread_attr_init(&attr);
    ret = pthread_create(&urma_info->recv_thread, &attr, prof_urma_recv_thread, (void *)urma_info);
    if (ret != DRV_ERROR_NONE) {
        urma_info->recv_thread_status = 0;
        PROF_ERR("Failed to create the thread. (ret=%d)\n", ret);
    }

    PROF_INFO("Create wait thread success. (pid=%u)\n", getpid());
}

void prof_urma_recv_thread_destroy(struct prof_urma_info *urma_info)
{
    if (urma_info->recv_thread_status == 0) {
        return;
    }

    urma_info->recv_thread_status = 0;
    (void)pthread_cancel(urma_info->recv_thread);
    (void)pthread_join(urma_info->recv_thread, NULL);
}

STATIC drvError_t prof_urma_creat(uint32_t dev_id, struct prof_urma_info *info)
{
    uint32_t token_val = 0;
    drvError_t ret;

    ret = dms_get_token_val(dev_id, SHARED_TOKEN_VAL, &token_val);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to get token_val. (dev_id=%u)\n", dev_id);
        return ret;
    }

    info->urma_ctx = ascend_urma_create_context(info->urma_dev, info->eid_index);
    if (info->urma_ctx == NULL) {
        PROF_ERR("Failed to create urma proc ctx. (eid_index=%u)\n", info->eid_index);
        return DRV_ERROR_INNER_ERR;
    }

    info->token.token = token_val;
    info->token_id = urma_alloc_token_id(info->urma_ctx);
    if (info->token_id == NULL) {
        (void)ascend_urma_delete_context(info->urma_ctx);
        PROF_ERR("Failed to alloc urma token id.\n");
        return DRV_ERROR_INNER_ERR;
    }

    info->jfce = urma_create_jfce(info->urma_ctx);
    if (info->jfce == NULL) {
        (void)urma_free_token_id(info->token_id);
        (void)ascend_urma_delete_context(info->urma_ctx);
        PROF_ERR("Failed to create jfce.\n");
        return DRV_ERROR_INNER_ERR;
    }

    prof_urma_recv_thread_create(info);

    return DRV_ERROR_NONE;
}

void prof_urma_destroy(struct prof_urma_info *info)
{
    if (info->init_flag == 0) {
        return;
    }

    prof_urma_recv_thread_destroy(info);
    (void)urma_delete_jfce(info->jfce);
    (void)urma_free_token_id(info->token_id);
    (void)ascend_urma_delete_context(info->urma_ctx);
}

STATIC drvError_t prof_urma_info_id_alloc(uint32_t dev_id, uint32_t *urma_info_id)
{
    struct dms_ub_dev_info dev_info = {0};
    urma_device_t *urma_dev;
    int num;
    uint32_t i;
    drvError_t ret;

    ret = dms_get_ub_dev_info(dev_id, &dev_info, &num);
    if ((ret != 0) || (num == 0)) {
        PROF_ERR("dms_get_ub_dev_info failed.(dev_id=%u; ret=%d; num=%d)\n", dev_id, ret, num);
        return DRV_ERROR_INNER_ERR;
    }

    urma_dev = (urma_device_t *)dev_info.urma_dev[0];
    for (i = 0; i < PROF_URMA_MAX_DEVNUM; i++) {
        if (g_prof_urma_info[i].init_flag == 0) {
            g_prof_urma_info[i].urma_dev = urma_dev;
            g_prof_urma_info[i].eid_index = dev_info.eid_index[0];
            break;
        } else {
            if (urma_dev == g_prof_urma_info[i].urma_dev) {
                break;
            }
        }
    }

    if (i >= PROF_URMA_MAX_DEVNUM) {
        PROF_ERR("Failed to alloc urma id. (ret=%d, dev_id=%u)\n", (int)ret, dev_id);
        return DRV_ERROR_NO_RESOURCES;
    }

    *urma_info_id = i;

    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_urma_info_init(uint32_t dev_id)
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    struct prof_urma_info *urma_info;
    uint32_t urma_id;
    drvError_t ret;

    if (g_prof_devid_to_urma_devid[dev_id] != PROF_URMA_DEVID_INVALID) {
        return DRV_ERROR_NONE;
    }

    (void)pthread_mutex_lock(&init_mutex);
    if (g_prof_devid_to_urma_devid[dev_id] != PROF_URMA_DEVID_INVALID) {
        (void)pthread_mutex_unlock(&init_mutex);
        return DRV_ERROR_NONE;
    }

    ret = prof_urma_info_id_alloc(dev_id, &urma_id);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&init_mutex);
        return ret;
    }

    urma_info = &g_prof_urma_info[urma_id];
    if (urma_info->init_flag != 0) {
        g_prof_devid_to_urma_devid[dev_id] = urma_id;
        (void)pthread_mutex_unlock(&init_mutex);
        return DRV_ERROR_NONE;
    }

    ret = prof_urma_creat(dev_id, urma_info);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&init_mutex);
        PROF_ERR("Failed to urma create. (ret=%d)\n", (int)ret);
        return ret;
    }

    ATOMIC_SET(&urma_info->init_flag, 1);
    g_prof_devid_to_urma_devid[dev_id] = urma_id;
    (void)pthread_mutex_unlock(&init_mutex);

    PROF_INFO("Init urma chan info success.\n");

    return DRV_ERROR_NONE;
}

struct prof_urma_chan_info *prof_urma_chan_info_creat(uint32_t dev_id, uint32_t chan_id)
{
    struct prof_urma_chan_info *urma_chan_info;
    struct prof_urma_info *urma_info;
    drvError_t ret;

    ret = prof_urma_info_init(dev_id);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to init urma info. (ret=%d)\n", (int)ret);
        return NULL;
    }

    urma_info = prof_get_urma_info(dev_id);
    if (urma_info == NULL) {
        PROF_ERR("Failed to get urma_info. (dev_id=%u)\n", dev_id);
        return NULL;
    }

    urma_chan_info = (struct prof_urma_chan_info *)calloc(1, sizeof(struct prof_urma_chan_info));
    if (urma_chan_info == NULL) {
        PROF_ERR("Failed to alloc ub_info.\n");
        return NULL;
    }

    ret = prof_create_jetty(urma_info, urma_chan_info);
    if (ret != DRV_ERROR_NONE) {
        free(urma_chan_info);
        PROF_ERR("Failed to create jetty. (ret=%d)\n", (int)ret);
        return NULL;
    }

    urma_chan_info->dev_id = dev_id;
    urma_chan_info->chan_id = chan_id;
    PROF_INFO("Init chan info success.(dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
    return urma_chan_info;
}

void prof_urma_chan_info_destroy(struct prof_urma_chan_info *urma_chan_info)
{
    uint32_t dev_id = urma_chan_info->dev_id;
    uint32_t chan_id = urma_chan_info->chan_id;

    prof_destroy_jetty(urma_chan_info);
    free(urma_chan_info);
    PROF_INFO("Uninit chan_ctx success.(dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
}

STATIC void __attribute__((constructor)) prof_urma_init(void)
{
    int i;

    for (i = 0; i < DEV_NUM; i++) {
        g_prof_devid_to_urma_devid[i] = PROF_URMA_DEVID_INVALID;
    }
}

STATIC void __attribute__((destructor)) prof_urma_uninit(void)
{
    int i;

    for (i = 0; i < PROF_URMA_MAX_DEVNUM; i++) {
        prof_urma_destroy(&g_prof_urma_info[i]);
    }
}

#else
int prof_urma_comm_ut_test(void)
{
    return 0;
}
#endif
