/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "svm_urma_def.h"
#include "svm_pub.h"
#include "svm_log.h"
#include "svm_sys_cmd.h"
#include "svm_user_adapt.h"
#include "svm_sub_event_type_uk_msg.h"
#include "svm_criu.h"
#include "svm_urma_jetty.h"
#include "svm_urma_chan_msg.h"
#include "svm_umc_client.h"
#include "svm_apbi.h"
#include "svm_urma_chan.h"
#include "svm_timestamp.h"

#define SVM_JETTY_NUM_PER_POOL 32U

struct svm_urma_jetty_pool g_chan_inst[SVM_MAX_AGENT_NUM];
static urma_target_jetty_t *g_tjfr[SVM_MAX_AGENT_NUM];
static urma_token_id_t *g_chan_token_id[SVM_MAX_AGENT_NUM][SVM_JETTY_NUM_PER_POOL];

/* todo set depth smaller to test faster */
#define SVM_JETTY_DEPTH 512U
#define SVM_URMA_CHAN_WAIT_TIMEOUT_MS 60000ULL

static void svm_urma_chan_free_token_ids(u32 devid)
{
    u32 i;

    for (i = 0; i < SVM_JETTY_NUM_PER_POOL; i++) {
        if (g_chan_token_id[devid][i] != NULL) {
            ascend_urma_free_token_id(g_chan_token_id[devid][i]);
            g_chan_token_id[devid][i] = NULL;
        }
    }
}

static int svm_urma_chan_alloc_token_ids(void *ctx, u32 devid)
{
    u32 i;

    for (i = 0; i < SVM_JETTY_NUM_PER_POOL; i++) {
        g_chan_token_id[devid][i] = ascend_urma_alloc_token_id(ctx);
        if (g_chan_token_id[devid][i] == NULL) {
            svm_err("Alloc chan token id failed. (devid=%u; i=%u)\n", devid, i);
            svm_urma_chan_free_token_ids(devid);
            return DRV_ERROR_INNER_ERR;
        }
    }
    return DRV_ERROR_NONE;
}

static int svm_local_chan_init(void *ctx, u32 devid)
{
    struct svm_urma_jetty_pool_cfg cfg;
    urma_token_t token;
    int ret;

    ret = ascend_urma_get_token_val(devid, &token.token);
    if (ret != DRV_ERROR_NONE) {
        svm_err("ascend_urma_get_token_val failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = svm_urma_chan_alloc_token_ids(ctx, devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_urma_jetty_pool_cfg_pack(devid, ascend_to_urma_ctx(ctx), token, SVM_JETTY_NUM_PER_POOL, SVM_JETTY_DEPTH, &cfg);
    ret = svm_urma_jetty_pool_init(&g_chan_inst[devid], &cfg);
    if (ret != DRV_ERROR_NONE) {
        svm_urma_chan_free_token_ids(devid);
    }
    return ret;
}

static void svm_local_chan_uninit(u32 devid)
{
    svm_urma_jetty_pool_uninit(&g_chan_inst[devid]);
    svm_urma_chan_free_token_ids(devid);
}

static int _svm_remote_chan_init(u32 devid, urma_jfr_id_t *rjfr_id, u32 *token_val)
{
    struct svm_umc_msg_head head;
    struct svm_urma_chan_info_msg chan_info;
    struct svm_umc_msg msg = {.msg_in = NULL,
                              .msg_in_len = 0,
                              .msg_out = (char *)(uintptr_t)&chan_info,
                              .msg_out_len = sizeof(struct svm_urma_chan_info_msg)};
    struct svm_apbi apbi;
    int ret;

    ret = svm_apbi_query(devid, DEVDRV_PROCESS_CP1, &apbi);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_umc_msg_head_pack(devid, apbi.tgid, apbi.grp_id, SVM_URMA_CHAN_AGENT_INIT_EVENT, &head);
    ret = svm_umc_h2d_send(&head, &msg);
    if (ret == DRV_ERROR_NONE) {
        *rjfr_id = chan_info.jfr_id;
        *token_val = chan_info.token_val;
    } else if (ret == DRV_ERROR_NO_PROCESS) {
        (void)svm_apbi_clear(devid, DEVDRV_PROCESS_CP1);
    }

    return ret;
}

static int _svm_remote_chan_uninit(u32 devid)
{
    struct svm_umc_msg_head head;
    struct svm_umc_msg msg = {.msg_in = NULL, .msg_in_len = 0, .msg_out = NULL, .msg_out_len = 0};
    struct svm_apbi apbi;
    int ret;

    ret = svm_apbi_query(devid, DEVDRV_PROCESS_CP1, &apbi);
    if (ret != DRV_ERROR_NONE) {
        /* process exit, no need msg to uninit chan, return ok derictly */
        return (ret == DRV_ERROR_NO_PROCESS) ? DRV_ERROR_NONE : ret;
    }

    svm_umc_msg_head_pack(devid, apbi.tgid, apbi.grp_id, SVM_URMA_CHAN_AGENT_UNINIT_EVENT, &head);
    ret = svm_umc_h2d_send(&head, &msg);
    if (ret == DRV_ERROR_NO_PROCESS) {
        (void)svm_apbi_clear(devid, DEVDRV_PROCESS_CP1);
    }

    return (ret == DRV_ERROR_NO_PROCESS) ? DRV_ERROR_NONE : ret;
}

static int svm_remote_chan_init(void *ctx, u32 devid)
{
    urma_jfr_id_t rjfr_id;
    urma_rjfr_t rjfr = {0};
    urma_token_t token;
    int ret;

    ret = _svm_remote_chan_init(devid, &rjfr_id, &token.token);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    rjfr.jfr_id = rjfr_id;
    rjfr.trans_mode = URMA_TM_RM;
    rjfr.flag.bs.token_policy = URMA_TOKEN_PLAIN_TEXT;
    g_tjfr[devid] = ascend_urma_import_jfr(ascend_to_urma_ctx(ctx), &rjfr, &token);
    if (g_tjfr[devid] == NULL) {
        svm_err("Urma import jfr failed.\n");
        (void)_svm_remote_chan_uninit(devid);
        return DRV_ERROR_INNER_ERR;
    }

    svm_debug("Urma remote chan init. (devid=%u; remote_jfr_id=%u)\n", devid, g_tjfr[devid]->id.id);

    return DRV_ERROR_NONE;
}

static void svm_remote_chan_uninit(u32 devid)
{
    (void)_svm_remote_chan_uninit(devid);
    if (!svm_criu_is_resetting(devid) && (g_tjfr[devid] != NULL)) {
        svm_debug("Urma remote chan uninit. (devid=%u; remote_jfr_id=%u)\n", devid, g_tjfr[devid]->id.id);
        (void)urma_unimport_jfr(g_tjfr[devid]);
    }
    g_tjfr[devid] = NULL;
}

int svm_urma_chan_init(u32 devid)
{
    u64 start, end_local, end_remote;
    void *ctx = NULL;
    int ret;

    if (devid >= SVM_MAX_AGENT_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INNER_ERR;
    }

    ctx = ascend_urma_ctx_get(devid);
    if (ctx == NULL) {
        svm_err("Get urma ctx failed. (devid=%u)\n", devid);
        return DRV_ERROR_INNER_ERR;
    }

    start = svm_get_timestamp_ns();

    ret = svm_local_chan_init(ctx, devid);
    if (ret != DRV_ERROR_NONE) {
        ascend_urma_ctx_put(ctx);
        return ret;
    }

    end_local = svm_get_timestamp_ns();

    ret = svm_remote_chan_init(ctx, devid);
    if (ret != DRV_ERROR_NONE) {
        svm_local_chan_uninit(devid);
        ascend_urma_ctx_put(ctx);
        return ret;
    }
    ascend_urma_ctx_put(ctx);

    end_remote = svm_get_timestamp_ns();

    svm_run_info("Urma chan init ok. (devid=%u; local=%llu ns; remote=%llu ns)\n", devid, end_local - start,
                 end_remote - end_local);

    return 0;
}

void svm_urma_chan_uninit(u32 devid)
{
    if (devid >= SVM_MAX_AGENT_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", devid);
        return;
    }

    svm_remote_chan_uninit(devid);
    svm_local_chan_uninit(devid);
}

int svm_urma_chan_try_alloc(u32 devid, u32 *chan_id, int64_t timeout_s)
{
    struct svm_urma_jetty *out_jetty = NULL;
    int ret;

    if (devid >= SVM_MAX_AGENT_NUM) {
        svm_err("Invalid chan try alloc arg. (devid=%u)\n", devid);
        return DRV_ERROR_INNER_ERR;
    }

    ret = svm_urma_jetty_alloc(&g_chan_inst[devid], &out_jetty, timeout_s);
    if (ret == DRV_ERROR_NONE) {
        *chan_id = out_jetty->id;
    }

    return ret;
}

int svm_urma_chan_alloc(u32 devid, u32 *chan_id)
{
    return svm_urma_chan_try_alloc(devid, chan_id, -1);
}

void svm_urma_chan_free(u32 devid, u32 chan_id)
{
    if (devid >= SVM_MAX_AGENT_NUM || chan_id >= SVM_JETTY_NUM_PER_POOL) {
        svm_err("Invalid chan free arg. (devid=%u; chan_id=%u)\n", devid, chan_id);
        return;
    }
    svm_urma_jetty_free(&g_chan_inst[devid], g_chan_inst[devid].jettys[chan_id]);
}

int svm_urma_chan_wait_one(u32 devid, u32 chan_id, int timeout_ms)
{
    return svm_urma_chan_wait(devid, chan_id, 1, timeout_ms);
}

/* Each chan has its own token_id. Copies within the same page cannot share a token,
 * so one chan corresponds to one token to avoid token conflict within a page. */
int svm_urma_chan_get_token(u32 devid, u32 chan_id, urma_token_id_t **token, urma_token_t *token_val)
{
    int ret;

    if (devid >= SVM_MAX_AGENT_NUM || chan_id >= SVM_JETTY_NUM_PER_POOL) {
        return DRV_ERROR_INVALID_VALUE;
    }
    *token = g_chan_token_id[devid][chan_id];
    ret = ascend_urma_get_token_val(devid, &token_val->token);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get chan token val failed. (devid=%u; chan_id=%u; ret=%d)\n", devid, chan_id, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

u64 svm_urma_chan_max_once_cpy_size(void)
{
    return (u64)SVM_JETTY_DEPTH * SVM_URMA_JETTY_WRITE_READ_MAX_SIZE;
}

int svm_urma_chan_submit(u32 devid, u32 chan_id, struct svm_urma_chan_submit_para *para, u32 *wr_num)
{
    struct svm_urma_jetty *jetty = NULL;
    struct svm_urma_jetty_post_para post_para = {0};
    drvError_t ret;

    if (devid >= SVM_MAX_AGENT_NUM || chan_id >= SVM_JETTY_NUM_PER_POOL) {
        svm_err("Invalid chan submit arg. (devid=%u; chan_id=%u)\n", devid, chan_id);
        return DRV_ERROR_INNER_ERR;
    }

    jetty = g_chan_inst[devid].jettys[chan_id];

    svm_debug("(id=%u; jfs=%u; jfc=%u)\n", jetty->id, jetty->jfs->jfs_id.id, jetty->jfc->jfc_id.id);

    svm_debug("src=0x%llx; dst=0x%llx; size=%llu; src_tseg=%p; dst_tseg=%p\n", para->src, para->dst, para->size,
              para->src_tseg, para->dst_tseg);

    post_para.src = para->src;
    post_para.dst = para->dst;
    post_para.size = para->size;
    post_para.opcode = para->opcode;
    post_para.src_tseg = para->src_tseg;
    post_para.dst_tseg = para->dst_tseg;
    post_para.tjfr = g_tjfr[devid];

jetty_post:
    ret = svm_urma_jetty_post(jetty, &post_para, wr_num);
    if (ret == DRV_ERROR_BUSY) {
        ret = svm_urma_chan_wait(devid, chan_id, 1, SVM_URMA_CHAN_WAIT_TIMEOUT_MS);
        if (ret != 0) {
            svm_err("svm_urma_chan_wait failed. (ret=%d)\n", ret);
            return ret;
        }
        goto jetty_post;
    }

    return ret;
}

int svm_urma_chan_wait(u32 devid, u32 chan_id, int wr_num, int timeout_ms)
{
    struct svm_urma_jetty *jetty = NULL;

    if (devid >= SVM_MAX_AGENT_NUM || chan_id >= SVM_JETTY_NUM_PER_POOL) {
        svm_err("Invalid chan wait arg. (devid=%u; chan_id=%u)\n", devid, chan_id);
        return DRV_ERROR_INNER_ERR;
    }

    jetty = g_chan_inst[devid].jettys[chan_id];
    return svm_urma_jetty_wait(jetty, wr_num, timeout_ms);
}

void svm_urma_chan_set_user_ctx(u32 devid, u32 chan_id, void *ctx)
{
    if (devid >= SVM_MAX_AGENT_NUM || chan_id >= SVM_JETTY_NUM_PER_POOL) {
        svm_err("Set chan user ctx invalid. (devid=%u; chan_id=%u)\n", devid, chan_id);
        return;
    }
    g_chan_inst[devid].jettys[chan_id]->user_ctx = ctx;
}

void *svm_urma_chan_get_user_ctx(u32 devid, u32 chan_id)
{
    if (devid >= SVM_MAX_AGENT_NUM || chan_id >= SVM_JETTY_NUM_PER_POOL) {
        svm_err("Get chan user ctx invalid. (devid=%u; chan_id=%u)\n", devid, chan_id);
        return NULL;
    }
    return g_chan_inst[devid].jettys[chan_id]->user_ctx;
}
