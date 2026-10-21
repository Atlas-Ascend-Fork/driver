/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdlib.h>
#include <pthread.h>
#include "securec.h"
#include "ascend_hal_error.h"
#ifndef EMU_ST
#include "svm_user_interface.h"
#endif
#include "uref.h"
#include "queue_interface.h"
#include "queue.h"
#include "que_compiler.h"
#include "que_urma.h"
#include "que_jetty.h"
#include "que_ub_msg.h"
#include "que_comm_event.h"
#include "que_comm_thread.h"
#include "que_comm_chan.h"
#include "dms_user_interface.h"
#include "que_comm_ctx.h"
#include "que_platform_ub.h"
#ifndef EMU_ST
#define QUE_PKT_RECV_DEPTH 4096U /* If you modify QUE_PKT_RECV_DEPTH, pay attention to QUE_TOPIC_SQ_DEPTH */
#define QUE_ACK_SEND_DEPTH 4096U
#define QUE_URMA_WAIT_TIME 500 /*ms*/

static void que_jfs_pool_free(int qjfs_num, struct que_jfs_pool_info *jfs_pool)
{
    int idx;
    for (idx = 0; idx < qjfs_num; idx++) {
        if (que_likely(jfs_pool[idx].qjfs != NULL)) {
            que_jfs_destroy(jfs_pool[idx].qjfs);
            jfs_pool[idx].qjfs = NULL;
        }
    }
}

static int que_jfs_pool_create(unsigned int devid, struct que_jfs_pool_info *jfs_pool, unsigned int d2d_flag)
{
    int idx;
    struct que_jfs_attr jfs_attr = {
        .jfs_depth = QUE_SEND_PKT_DEPTH, .jfc_s_depth = QUE_SEND_PKT_DEPTH, .priority = ASCEND_URMA_PRIORITY_MIDDLE};

    for (idx = 0; idx < QUE_PKT_SEND_JETTY_POOL_DEPTH; idx++) {
        jfs_pool[idx].qjfs = que_jfs_create(devid, &jfs_attr, d2d_flag);
        if (que_unlikely(jfs_pool[idx].qjfs == NULL)) {
            que_jfs_pool_free(idx, jfs_pool);
            return DRV_ERROR_INNER_ERR;
        }
        jfs_pool[idx].jfs_busy_flag = false;
    }

    return DRV_ERROR_NONE;
}

static void que_jfr_pool_free(int qjfr_num, struct que_jfr_pool_info *jfr_pool)
{
    int idx;
    for (idx = 0; idx < qjfr_num; idx++) {
        if (que_likely(jfr_pool[idx].qjfr != NULL)) {
            que_jfr_destroy(jfr_pool[idx].qjfr);
            jfr_pool[idx].qjfr = NULL;
        }
    }
}

static int que_jfr_pool_create(unsigned int devid, struct que_jfr_pool_info *jfr_pool, unsigned int d2d_flag)
{
    int idx;
    struct que_jfr_attr jfr_attr = {.jfr_depth = QUE_ACK_PKT_DEPTH, .jfc_r_depth = QUE_ACK_PKT_DEPTH};

    for (idx = 0; idx < QUE_PKT_SEND_JETTY_POOL_DEPTH; idx++) {
        jfr_pool[idx].qjfr = que_jfr_create(devid, &jfr_attr, d2d_flag);
        if (que_unlikely(jfr_pool[idx].qjfr == NULL)) {
            que_jfr_pool_free(idx, jfr_pool);
            return DRV_ERROR_INNER_ERR;
        }
        jfr_pool[idx].jfr_busy_flag = false;
    }

    return DRV_ERROR_NONE;
}

static void que_uma_recv_destroy_pool(int recv_num, struct que_jfr_pool_info *jfr_pool)
{
    int idx;
    for (idx = 0; idx < recv_num; idx++) {
        if (que_likely(jfr_pool[idx].recv_para != NULL)) {
            que_uma_recv_destroy(jfr_pool[idx].recv_para);
            jfr_pool[idx].recv_para = NULL;
        }
    }
}

static int que_uma_recv_create_pool(unsigned int devid, struct que_jfr_pool_info *jfr_pool, unsigned int d2d_flag)
{
    int idx;
    struct que_uma_recv_attr recv_attr = {.num = QUE_ACK_PKT_DEPTH, .size = QUE_ACK_PKT_SIZE};

    for (idx = 0; idx < QUE_PKT_SEND_JETTY_POOL_DEPTH; idx++) {
        jfr_pool[idx].recv_para = que_uma_recv_create(devid, jfr_pool[idx].qjfr, &recv_attr, d2d_flag);
        if (que_unlikely(jfr_pool[idx].recv_para == NULL)) {
            que_uma_recv_destroy_pool(idx, jfr_pool);
            return DRV_ERROR_INNER_ERR;
        }
    }
    return DRV_ERROR_NONE;
}

/* This function is used to create the jfs pool of the PKT that carries the iovec address information. */
static int que_jfs_pool_create_for_pkt_send(struct que_ctx *ctx, unsigned int d2d_flag)
{
    int ret;
    struct que_jfs_pool_info jfs_pool[QUE_PKT_SEND_JETTY_POOL_DEPTH] = {0};

    ret = que_jfs_pool_create(ctx->devid, jfs_pool, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que jfs pool create fail. (ret=%d; devid=%u; d2d_flag=%u)\n", ret, ctx->devid, d2d_flag);
        return ret;
    }

    ret = memcpy_s((void *)ctx->jfs_pool[d2d_flag], sizeof(struct que_jfs_pool_info) * QUE_PKT_SEND_JETTY_POOL_DEPTH,
                   (void *)jfs_pool, sizeof(struct que_jfs_pool_info) * QUE_PKT_SEND_JETTY_POOL_DEPTH);
    if (que_unlikely(ret != EOK)) {
        QUEUE_LOG_ERR("Que jfs pool copy fail. (ret=%d; devid=%u; d2d_flag=%u)\n", ret, ctx->devid, d2d_flag);
        ret = DRV_ERROR_INNER_ERR;
        goto jfs_pool_free;
    }
    return DRV_ERROR_NONE;

jfs_pool_free:
    que_jfs_pool_free(QUE_PKT_SEND_JETTY_POOL_DEPTH, jfs_pool);
    return ret;
}

static void que_jfs_pool_free_for_pkt_send(struct que_ctx *ctx, unsigned int d2d_flag)
{
    que_jfs_pool_free(QUE_PKT_SEND_JETTY_POOL_DEPTH, ctx->jfs_pool[d2d_flag]);
}

static int que_jfr_pool_create_for_ack_recv(struct que_ctx *ctx, unsigned int d2d_flag)
{
    int ret;
    struct que_jfr_pool_info jfr_pool[QUE_PKT_SEND_JETTY_POOL_DEPTH] = {0};

    ret = que_jfr_pool_create(ctx->devid, jfr_pool, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("Que jfr pool create fail. (ret=%d; devid=%u)\n", ret, ctx->devid);
        return ret;
    }

    ret = que_uma_recv_create_pool(ctx->devid, jfr_pool, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que uma recv create fail. (ret=%d; devid=%u)\n", ret, ctx->devid);
        goto jfr_pool_free;
    }

    ret = memcpy_s((void *)ctx->jfr_pool[d2d_flag], sizeof(struct que_jfr_pool_info) * QUE_PKT_SEND_JETTY_POOL_DEPTH,
                   (void *)jfr_pool, sizeof(struct que_jfr_pool_info) * QUE_PKT_SEND_JETTY_POOL_DEPTH);
    if (que_unlikely(ret != EOK)) {
        QUEUE_LOG_ERR("que jfr pool copy fail. (ret=%d; devid=%u)\n", ret, ctx->devid);
        ret = DRV_ERROR_INNER_ERR;
        goto uma_recv_free;
    }
    return DRV_ERROR_NONE;

uma_recv_free:
    que_uma_recv_destroy_pool(QUE_PKT_SEND_JETTY_POOL_DEPTH, jfr_pool);
jfr_pool_free:
    que_jfr_pool_free(QUE_PKT_SEND_JETTY_POOL_DEPTH, jfr_pool);
    return ret;
}

static void que_jfr_pool_free_for_ack_recv(struct que_ctx *ctx, unsigned int d2d_flag)
{
    que_uma_recv_destroy_pool(QUE_PKT_SEND_JETTY_POOL_DEPTH, ctx->jfr_pool[d2d_flag]);
    que_jfr_pool_free(QUE_PKT_SEND_JETTY_POOL_DEPTH, ctx->jfr_pool[d2d_flag]);
}

/* This function is used to create the jfs and jfr pool. */
static int que_jfs_jfr_pool_create_for_pkt_send_ack_recv(struct que_ctx *ctx, unsigned int d2d_flag)
{
    int ret;
    ret = que_jfs_pool_create_for_pkt_send(ctx, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }

    ret = que_jfr_pool_create_for_ack_recv(ctx, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        que_jfs_pool_free_for_pkt_send(ctx, d2d_flag);
        return ret;
    }
    return DRV_ERROR_NONE;
}

static void que_jfs_jfr_pool_free_for_pkt_send_ack_recv(struct que_ctx *ctx, unsigned int d2d_flag)
{
    que_jfs_pool_free_for_pkt_send(ctx, d2d_flag);
    que_jfr_pool_free_for_ack_recv(ctx, d2d_flag);
}

static int que_jfr_jfs_create_for_pkt_recv_ack_send(struct que_ctx *ctx, unsigned int d2d_flag)
{
    struct que_jfs *ack_send_jetty = NULL;
    struct que_jfr *pkt_recv_jetty = NULL;
    struct que_recv_para *recv = NULL;
    struct que_jfr_attr jfr_attr = {.jfr_depth = QUE_PKT_RECV_DEPTH, .jfc_r_depth = QUE_PKT_RECV_DEPTH};
    struct que_jfs_attr jfs_attr = {
        .jfs_depth = QUE_ACK_SEND_DEPTH, .jfc_s_depth = QUE_ACK_SEND_DEPTH, .priority = ASCEND_URMA_PRIORITY_HIGH};
    struct que_uma_recv_attr uma_recv_attr = {.num = QUE_PKT_RECV_DEPTH, .size = QUE_UMA_MAX_SEND_SIZE};

    pkt_recv_jetty = que_jfr_create(ctx->devid, &jfr_attr, d2d_flag);
    if (que_unlikely(pkt_recv_jetty == NULL)) {
        QUEUE_LOG_ERR("que jfr create fail. (devid=%u; hostpid=%d)\n", ctx->devid, ctx->hostpid);
        return DRV_ERROR_INNER_ERR;
    }

    ack_send_jetty = que_jfs_create(ctx->devid, &jfs_attr, d2d_flag);
    if (que_unlikely(ack_send_jetty == NULL)) {
        QUEUE_LOG_ERR("Que jfs create fail. (devid=%u; hostpid=%d)\n", ctx->devid, ctx->hostpid);
        goto pkt_recv_jetty_free;
    }

    recv = que_uma_recv_create(ctx->devid, pkt_recv_jetty, &uma_recv_attr, d2d_flag);
    if (que_unlikely(recv == NULL)) {
        QUEUE_LOG_ERR("que pkt recv buff init fail. (devid=%u; hostpid=%d)\n", ctx->devid, ctx->hostpid);
        goto ack_send_jetty_free;
    }

    ctx->ack_send_jetty[d2d_flag] = ack_send_jetty;
    ctx->pkt_recv_jetty[d2d_flag] = pkt_recv_jetty;
    ctx->recv_para[d2d_flag] = recv;
    return DRV_ERROR_NONE;

ack_send_jetty_free:
    que_jfs_destroy(ack_send_jetty);
pkt_recv_jetty_free:
    que_jfr_destroy(pkt_recv_jetty);
    return DRV_ERROR_INNER_ERR;
}

static void que_jfr_jfs_free_for_pkt_recv_ack_send(struct que_ctx *ctx, unsigned int d2d_flag)
{
    if (que_likely(ctx->recv_para[d2d_flag] != NULL)) {
        que_uma_recv_destroy(ctx->recv_para[d2d_flag]);
        ctx->recv_para[d2d_flag] = NULL;
    }

    if (que_likely(ctx->ack_send_jetty[d2d_flag] != NULL)) {
        que_jfs_destroy(ctx->ack_send_jetty[d2d_flag]);
        ctx->ack_send_jetty[d2d_flag] = NULL;
    }

    if (que_likely(ctx->pkt_recv_jetty[d2d_flag] != NULL)) {
        que_jfr_destroy(ctx->pkt_recv_jetty[d2d_flag]);
        ctx->pkt_recv_jetty[d2d_flag] = NULL;
    }
}

static int que_jfs_pool_create_for_data_rw(struct que_ctx *ctx, unsigned int d2d_flag)
{
    int ret;
    struct que_jfs_attr attr = {.jfs_depth = QUE_DATA_RW_JETTY_POOL_SEND_DEPTH,
                                .jfc_s_depth = QUE_DATA_RW_JETTY_POOL_JFC_DEPTH,
                                .priority = ASCEND_URMA_PRIORITY_MIDDLE};

    attr.spec_jfce_s = 1;
    attr.jfce = ctx->pkt_recv_jetty[d2d_flag]->jfce_r;
    ret = que_jfs_pool_init(ctx->devid, &attr, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que data rw jetty pool init fail. (ret=%d; devid=%u; hostpid=%d)\n", ret, ctx->devid,
                      ctx->hostpid);
        return ret;
    }

    ctx->jfc[d2d_flag] = attr.jfc;
    return DRV_ERROR_NONE;
}

static void que_jfs_pool_free_for_data_rw(struct que_ctx *ctx, unsigned int d2d_flag)
{
    ctx->jfc[d2d_flag] = NULL;
    que_jfs_pool_uninit(ctx->devid, d2d_flag);
}

static int que_ctx_cr_token_init(struct que_ctx *ctx, unsigned int d2d_flag)
{
    int ret;
    ret = que_fill_ctx_token(ctx->devid, &ctx->token[d2d_flag], d2d_flag);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que fill ctx token fail. (ret=%d; devid=%u)\n", ret, ctx->devid);
        return ret;
    }

    urma_cr_t *cr = calloc(QUE_DATA_RW_JETTY_POOL_DEPTH, sizeof(urma_cr_t));
    if (cr == NULL) {
        QUEUE_LOG_ERR("que malloc cr fail. (cr_num=%u)\n", QUE_DATA_RW_JETTY_POOL_DEPTH);
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    ctx->cr[d2d_flag] = cr;
    return DRV_ERROR_NONE;
}

static void que_ctx_cr_token_uninit(struct que_ctx *ctx, unsigned int d2d_flag)
{
    if (que_likely(ctx->cr[d2d_flag] != NULL)) {
        free(ctx->cr[d2d_flag]);
        ctx->cr[d2d_flag] = NULL;
    }
    ctx->token[d2d_flag].token = 0;
}

static int que_ctx_event_res_init(struct que_ctx *ctx, unsigned int d2d_flag)
{
    int ret;
    struct event_res res;
    int32_t event_type;

    (void)d2d_flag;

    if (ctx->f2nf_res.res_alloc_flag == 1) {
        return DRV_ERROR_NONE;
    }
    que_get_sched_event_type(ctx->devid, &event_type);
    ret = esched_alloc_event_res(ctx->devid, event_type, &res);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_WARN("event resource can not alloc. (ret=%d; devid=%u; event_type=%u)\n", ret, ctx->devid,
                       event_type);
        return DRV_ERROR_NO_EVENT_RESOURCES;
    }

    ctx->f2nf_res.f2nf_event_res = res;
    ctx->f2nf_res.pid = (int)getpid();
    ctx->f2nf_res.dst_engine = que_get_sched_engine_type(ctx->devid);
    ctx->f2nf_res.res_alloc_flag = 1;
    return DRV_ERROR_NONE;
}

static void que_ctx_event_res_uninit(struct que_ctx *ctx, unsigned int d2d_flag)
{
    (void)d2d_flag;

    if (que_likely(ctx->f2nf_res.res_alloc_flag == 1)) {
        esched_free_event_res(ctx->devid, QUEUE_EVENT, &ctx->f2nf_res.f2nf_event_res);
        ctx->f2nf_res.res_alloc_flag = 0;
    }
}

/* don't modify the initialization order of the resources because of
 * dependencies between the resources. */
que_ub_res_alloc g_ub_res_init[MAX_RES_TYPE] = {
    que_jfs_jfr_pool_create_for_pkt_send_ack_recv,
    que_jfr_jfs_create_for_pkt_recv_ack_send,
    que_jfs_pool_create_for_data_rw,
    que_ctx_cr_token_init,
    que_ctx_event_res_init,
};

que_ub_res_free g_ub_res_uninit[MAX_RES_TYPE] = {
    que_jfs_jfr_pool_free_for_pkt_send_ack_recv,
    que_jfr_jfs_free_for_pkt_recv_ack_send,
    que_jfs_pool_free_for_data_rw,
    que_ctx_cr_token_uninit,
    que_ctx_event_res_uninit,
};

static int que_ub_ctx_res_init(struct que_ctx *ctx, int res_type)
{
    int ret;
    ret = g_ub_res_init[res_type](ctx, TRANS_D2H_H2D);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ub ctx res init h2d d2h failed. (ret=%d; res_type=%d)\n", ret, res_type);
        return ret;
    }
    return que_ub_ctx_res_init_d2d_platform(ctx, res_type, g_ub_res_init, g_ub_res_uninit);
}

static void que_ub_ctx_res_uninit(struct que_ctx *ctx, int res_type)
{
    g_ub_res_uninit[res_type](ctx, TRANS_D2H_H2D);
    que_ub_ctx_res_uninit_d2d_platform(ctx, res_type, g_ub_res_uninit);
}

int que_ctx_init(struct que_ctx *ctx)
{
    int ret;
    int res_type, res_type_free;

    for (res_type = 0; res_type < MAX_RES_TYPE; res_type++) {
        if (((ctx->res_bitmap >> res_type) & 0x1) == 0) {
            continue;
        }

        ret = que_ub_ctx_res_init(ctx, res_type);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            goto out;
        }
    }
    return DRV_ERROR_NONE;

out:
    for (res_type_free = (res_type - 1); res_type_free >= 0; res_type_free--) {
        if ((ctx->res_bitmap >> res_type_free) & 0x1) {
            que_ub_ctx_res_uninit(ctx, res_type_free);
        }
    }
    return ret;
}

void que_ctx_uninit(struct que_ctx *ctx)
{
    int res_type;
    if (que_likely(ctx != NULL)) {
        for (res_type = (MAX_RES_TYPE - 1); res_type >= 0; res_type--) {
            if ((ctx->res_bitmap >> res_type) & 0x1) {
                que_ub_ctx_res_uninit(ctx, res_type);
            }
        }
    }
}

int que_ctx_chan_check(unsigned int devid, unsigned int qid, unsigned long create_time)
{
    struct que_ctx *ctx = NULL;
    int ret;

    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }

    ret = que_chan_create_check(devid, qid, create_time);
    que_ctx_put(ctx);
    return ret;
}

int que_ctx_chan_create(unsigned int devid, unsigned int qid, QUEUE_CHAN_TYPE chan_type, unsigned long create_time,
                        unsigned int d2d_flag)
{
    struct que_ctx *ctx = NULL;
    int ret;

    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }

    ret = que_chan_create(devid, qid, chan_type, create_time, d2d_flag);
    que_ctx_put(ctx);

    return ret;
}

int que_ctx_chan_update(unsigned int devid, unsigned int peer_devid, unsigned int qid, urma_jfr_id_t *tjfr_id,
                        urma_token_t *token)
{
    int ret;
    unsigned int d2d_flag = 0;
    struct que_ctx *ctx = NULL;
    unsigned int urma_devid = que_get_urma_devid(devid, peer_devid);
    que_get_d2d_flag(devid, peer_devid, &d2d_flag);
    ret = que_ub_res_init(urma_devid);
    if (que_unlikely((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_REPEATED_INIT))) {
        QUEUE_LOG_ERR("que clt ub res init fail. (ret=%d; devid=%u)\n", ret, urma_devid);
        return ret;
    }

    ctx = que_ctx_get(urma_devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }
    *token = ctx->token[d2d_flag];

    ret = que_chan_update_jfs_info(devid, qid, ctx->jfs_pool[d2d_flag], ctx->pkt_recv_jetty[d2d_flag], tjfr_id,
                                   ctx->token[d2d_flag]);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que chan update fail. (ret=%d; devid=%u; qid=%u; devpid=%d)\n", ret, devid, qid, ctx->devpid);
    }
    que_ctx_put(ctx);
    return ret;
}

int que_ctx_get_f2nf_res(unsigned int devid, unsigned int qid, struct que_f2nf_res *f2nf_res)
{
    struct que_ctx *ctx = NULL;

    (void)qid;

    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }
    *f2nf_res = ctx->f2nf_res;
    que_ctx_put(ctx);
    return DRV_ERROR_NONE;
}

int que_ctx_chan_destroy(unsigned int devid, unsigned int qid)
{
    struct que_ctx *ctx = NULL;
    int ret;

    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }

    ret = que_chan_destroy(devid, qid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que chan destroy fail. (ret=%d; devid=%u; qid=%u; devpid=%d)\n", ret, devid, qid, ctx->devpid);
    }
    que_ctx_put(ctx);

    return ret;
}

static int que_get_running_chan_list(unsigned int devid, struct que_query_alive_msg *qid_list)
{
    unsigned int i;
    unsigned int que_num = 0;
    static unsigned int pre_check_qid_id = 0;
    struct que_chan *chan = NULL;

    if (pre_check_qid_id >= CLIENT_QID_OFFSET) {
        pre_check_qid_id = 0;
    }

    for (i = pre_check_qid_id; i < CLIENT_QID_OFFSET; i++) {
        chan = que_chan_get(devid, i);
        if (que_unlikely(chan == NULL)) {
            continue;
        }
        if (chan->chan_type != CHAN_ATTACH) {
            que_chan_put(chan);
            continue;
        }
        if (que_num >= QUE_MAX_QUE_LIST_NUM) {
            que_chan_put(chan);
            break;
        }
        qid_list->qid_list[que_num].qid = ((unsigned int)(i) & 0x7FFFFFFFu);
        qid_list->qid_list[que_num].alive = 1u;
        que_chan_put(chan);
        que_num++;
    }
    pre_check_qid_id = i;
    if (que_num == 0) {
        return DRV_ERROR_QUEUE_EMPTY;
    }
    qid_list->num = que_num;
    return DRV_ERROR_NONE;
}

void que_ctx_chan_recycle(unsigned int devid, struct que_query_alive_msg *qid_list)
{
    int ret;
    unsigned int qid, i;

    qid_list->num = 0;
    ret = que_get_running_chan_list(devid, qid_list);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return;
    }

    ret = que_clt_query_que_alive(devid, qid_list);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return;
    }

    for (i = 0; i < qid_list->num; i++) {
        if (qid_list->qid_list[i].alive) {
            continue;
        }
        qid = qid_list->qid_list[i].qid;
        if (qid >= CLIENT_QID_OFFSET) {
            continue;
        }
        ret = que_ctx_chan_destroy(devid, qid);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("Failed to destroy queue channel. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        }
    }
    return;
}
static ASYNC_QUE_INI_EVENT que_get_enque_event(int ret)
{
    ASYNC_QUE_INI_EVENT event;
    if (ret == DRV_ERROR_QUEUE_EMPTY) {
        event = INI_ENQUE_EMPTY;
    } else if (ret == DRV_ERROR_NONE) {
        event = INI_ENQUE_NORMAL;
    } else {
        event = INI_ENQUE_ERROR;
    }
    return event;
}

int que_ctx_async_ini(unsigned int devid, unsigned int qid, void *mbuf)
{
    struct que_ctx *ctx = NULL;
    ASYNC_QUE_INI_EVENT event;
    bool ini_try_flag;
    int ret;

    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }

    ret = que_chan_async_pre_proc(devid, qid, mbuf);
    if ((que_unlikely(ret != DRV_ERROR_NONE)) && (que_unlikely(ret != DRV_ERROR_QUEUE_FULL))) {
        que_ctx_put(ctx);
        return ret;
    }

    ini_try_flag = que_chan_update_ini_status(devid, qid, INI_ENQUE_TRY);
    if (ini_try_flag) {
        ret = que_chan_inter_dev_ini_proc(devid, qid);
        event = que_get_enque_event(ret);
        (void)que_chan_update_ini_status(devid, qid, event);
    }

    que_ctx_put(ctx);
    return ret;
}

static void que_ctx_ack_recv(unsigned int urma_devid, urma_cr_t *cr)
{
    int result, ret;
    unsigned int qid, ini_try_flag;
    ASYNC_QUE_INI_EVENT event;
    que_ack_data ack_data = {0};
    unsigned int devid = que_get_chan_devid(urma_devid);

    ack_data.imm_data = cr->imm_data;
    result = ack_data.ack_msg.result;
    qid = queue_get_actual_qid((unsigned int)ack_data.ack_msg.qid);

    que_chan_done(devid, qid, ASYNC_ENQUE);
    if ((cr->status != URMA_CR_SUCCESS) || ((result != DRV_ERROR_NONE) && (result != DRV_ERROR_QUEUE_FULL))) {
        QUEUE_LOG_ERR("que enque ack error. (cr_status=%d; ret=%d; devid=%u; urma_devid=%u; qid=%u; immdata=%llu)\n",
                      cr->status, result, devid, urma_devid, qid, ack_data.imm_data);
        event = INI_ACK_ERROR;
    } else if (result == DRV_ERROR_QUEUE_FULL) {
        event = INI_ACK_FULL;
    } else {
        event = INI_ACK_NORMAL;
    }

    ini_try_flag = que_chan_update_ini_status(devid, qid, event);
    if (ini_try_flag) {
        ret = que_chan_inter_dev_ini_proc(devid, qid);
        event = que_get_enque_event(ret);
        (void)que_chan_update_ini_status(devid, qid, event);
    }
}

static unsigned long _que_uma_recv_get_addr(struct que_ctx *ctx, unsigned int offset, unsigned int d2d_flag)
{
    return ctx->recv_para[d2d_flag]->addr + offset * ctx->recv_para[d2d_flag]->size;
}

static int que_ctx_recv_proc(struct que_ctx *ctx, urma_cr_t *cr, unsigned int d2d_flag)
{
    int ret = DRV_ERROR_NONE;
    struct que_pkt *pkt = NULL;

    if (que_unlikely((cr->user_ctx >= ctx->recv_para[d2d_flag]->num))) {
        QUEUE_LOG_ERR("invalid offset. (offset=%lu; num=%u; recv_size=%ld)\n", cr->user_ctx,
                      ctx->recv_para[d2d_flag]->num, ctx->recv_para[d2d_flag]->size);
        return DRV_ERROR_PARA_ERROR;
    }

    pkt = (struct que_pkt *)_que_uma_recv_get_addr(ctx, (unsigned int)cr->user_ctx, d2d_flag);
    if (cr->opcode == URMA_CR_OPC_SEND_WITH_IMM) {
        que_ctx_ack_recv(ctx->devid, cr);
    } else {
        if (que_unlikely(cr->status != URMA_CR_SUCCESS)) {
            QUEUE_LOG_ERR("invalid cr status. (status=%d)\n", cr->status);
            goto recv_put;
        }
        ret = que_chan_tgt_recv(ctx->devid, ctx->ack_send_jetty[d2d_flag], pkt, d2d_flag);
    }

recv_put:
    que_uma_recv_put_addr(ctx->pkt_recv_jetty[d2d_flag], ctx->recv_para[d2d_flag], (uintptr_t)pkt);
    return ret;
}

typedef enum {
    JFC_DATA_RW,
    JFC_PKT_RECV,
    JFC_TYPE_BUTT,
} jfc_type;

static jfc_type que_jfc_id_check(urma_jfc_t *jfc_data_rw, urma_jfc_t *jfc_pkt_recv, urma_jfc_t *jfc)
{
    urma_jfc_id_t jfc_id_data_rw = jfc_data_rw->jfc_id;
    urma_jfc_id_t jfc_id_pkt_recv = jfc_pkt_recv->jfc_id;
    urma_jfc_id_t jfc_id = jfc->jfc_id;

    if ((jfc_id_data_rw.id == jfc_id.id) && (jfc_id_data_rw.uasid == jfc_id.uasid)) {
        return JFC_DATA_RW;
    }

    if ((jfc_id_pkt_recv.id == jfc_id.id) && (jfc_id_pkt_recv.uasid == jfc_id.uasid)) {
        return JFC_PKT_RECV;
    }
    return JFC_TYPE_BUTT;
}

static void que_ctx_log_cr_status(struct que_ctx *ctx, urma_cr_t *cr, unsigned int d2d_flag, jfc_type cur_jfc_type,
                                  urma_jfc_t *jfc)
{
    if (cr->status == URMA_CR_SUCCESS) {
        return;
    }

    if (cur_jfc_type == JFC_DATA_RW) {
        que_print_urma_ctx_info(ctx->devid, ctx->ack_send_jetty[d2d_flag]->jfs, NULL, jfc);
        return;
    }

    que_print_urma_ctx_info(ctx->devid, NULL, ctx->pkt_recv_jetty[d2d_flag]->jfr, jfc);
}

static void que_ctx_update_poll_cnt(struct que_ctx *ctx, int ret)
{
    if (ret == DRV_ERROR_NONE) {
        ATOMIC_INC((volatile int *)&ctx->cnt[POLL_SEND_JFC_SUCCESS]);
        return;
    }

    ATOMIC_INC((volatile int *)&ctx->cnt[POLL_SEND_JFC_FAIL]);
}

static int que_ctx_handle_cr(struct que_ctx *ctx, urma_cr_t *cr, unsigned int d2d_flag, jfc_type cur_jfc_type)
{
    int ret;

    ATOMIC_INC((volatile int *)&ctx->cnt[RECV_CQE_TOTAL]);
    if (cur_jfc_type == JFC_DATA_RW) {
        ret = que_chan_tgt_data_read_and_ack(cr);
    } else {
        ret = que_ctx_recv_proc(ctx, cr, d2d_flag);
    }

    if (ret != DRV_ERROR_NONE) {
        ATOMIC_INC((volatile int *)&ctx->cnt[RECV_CQE_FAIL]);
    }
    return ret;
}

static void que_ctx_process_cr_batch(struct que_ctx *ctx, urma_jfc_t *jfc, unsigned int d2d_flag, jfc_type cur_jfc_type,
                                     unsigned int cnt)
{
    unsigned int cr_idx;
    int ret;
    urma_cr_t *cr = ctx->cr[d2d_flag];

    for (cr_idx = 0; cr_idx < cnt; cr_idx++) {
        que_ctx_log_cr_status(ctx, &cr[cr_idx], d2d_flag, cur_jfc_type, jfc);
        ret = que_ctx_handle_cr(ctx, &cr[cr_idx], d2d_flag, cur_jfc_type);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("que chan enque fail. (ret=%d; hostpid=%d; cr_idx=%d)\n", ret, ctx->hostpid, cr_idx);
        }
    }
}

static void que_ctx_poll_and_process(struct que_ctx *ctx, urma_jfc_t *jfc, unsigned int d2d_flag, jfc_type cur_jfc_type,
                                     unsigned int cr_num)
{
    int ret;
    unsigned int cnt;
    urma_cr_t *cr = ctx->cr[d2d_flag];

    ret = que_uma_poll_send_jfc(jfc, cr_num, cr, &cnt);
    que_ctx_update_poll_cnt(ctx, ret);
    if (ret != DRV_ERROR_NONE) {
        return;
    }

    que_ctx_process_cr_batch(ctx, jfc, d2d_flag, cur_jfc_type, cnt);
}

static void que_ctx_data_rw_proc(struct que_ctx *ctx, urma_jfc_t *jfc, unsigned int d2d_flag,
                                 unsigned int *next_rearm_flag)
{
    int ret;
    unsigned int idle_jetty_num;

    que_ctx_poll_and_process(ctx, jfc, d2d_flag, JFC_DATA_RW, QUE_DATA_RW_JETTY_POOL_DEPTH);
    idle_jetty_num = que_idle_jetty_find(ctx->devid, d2d_flag);
    if ((idle_jetty_num == 0) || (*next_rearm_flag != 1)) {
        return;
    }

    ret = que_uma_rearm_jfc(ctx->pkt_recv_jetty[d2d_flag]->jfc_r);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que rearm jfc fail. (ret=%d)\n", ret);
    }
    *next_rearm_flag = 0;
}

static void que_ctx_pkt_recv_proc(struct que_ctx *ctx, urma_jfc_t *jfc, unsigned int d2d_flag,
                                  unsigned int *cur_rearm_flag, unsigned int *next_rearm_flag)
{
    unsigned int cr_num;

    cr_num = que_idle_jetty_find(ctx->devid, d2d_flag);
    if (cr_num == 0) {
        *cur_rearm_flag = 0;
        *next_rearm_flag = 1;
        return;
    }

    que_ctx_poll_and_process(ctx, jfc, d2d_flag, JFC_PKT_RECV, cr_num);
}

static int que_ctx_tgt_proc(struct que_ctx *ctx, urma_jfc_t *jfc, unsigned int d2d_flag)
{
    int ret;
    jfc_type cur_jfc_type;
    static unsigned int next_rearm_flag = 0;
    unsigned int cur_rearm_flag = 1;

    cur_jfc_type = que_jfc_id_check(ctx->jfc[d2d_flag], ctx->pkt_recv_jetty[d2d_flag]->jfc_r, jfc);
    if (cur_jfc_type == JFC_TYPE_BUTT) {
        return DRV_ERROR_INVALID_VALUE;
    }
    QUEUE_LOG_DEBUG("que rcv new jfc. (devid=%u; cur_jfc_type=%d)\n", ctx->devid, cur_jfc_type);

    if (cur_jfc_type == JFC_DATA_RW) {
        que_ctx_data_rw_proc(ctx, jfc, d2d_flag, &next_rearm_flag);
    } else {
        que_ctx_pkt_recv_proc(ctx, jfc, d2d_flag, &cur_rearm_flag, &next_rearm_flag);
    }

    que_uma_ack_jfc(jfc, 1);
    if (cur_rearm_flag) {
        ret = que_uma_rearm_jfc(jfc);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("que rearm jfc fail. (ret=%d)\n", ret);
        }
        ATOMIC_INC((volatile int *)&ctx->cnt[POLL_SEND_JFC_SUCCESS]);
    } else {
        ATOMIC_INC((volatile int *)&ctx->cnt[POLL_SEND_JFC_FAIL]);
    }
    return DRV_ERROR_NONE;
}

static int _que_ctx_poll(struct que_ctx *ctx, unsigned int d2d_flag)
{
    int ret;
    urma_jfc_t *jfc = NULL;

    do {
        ret = que_uma_wait_jfc(ctx->pkt_recv_jetty[d2d_flag]->jfce_r, QUE_URMA_WAIT_TIME, &jfc);
        if (ret == DRV_ERROR_NONE) {
            ATOMIC_INC((volatile int *)&ctx->cnt[WAIT_JFC_SUCCESS]);
            ret = que_ctx_tgt_proc(ctx, jfc, d2d_flag);
            if (ret != DRV_ERROR_NONE) {
                break;
            }
        } else {
            ATOMIC_INC((volatile int *)&ctx->cnt[WAIT_JFC_FAIL]);
            break;
        }
    } while (1);
    return (ret == DRV_ERROR_WAIT_TIMEOUT) ? DRV_ERROR_NONE : ret;
}

void que_ctx_poll(unsigned int devid, unsigned int d2d_flag)
{
    struct que_ctx *ctx = NULL;
    int ret;

    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        QUEUE_LOG_ERR("que ctx get fail. (devid=%u)\n", devid);
        return;
    }

    ret = _que_ctx_poll(ctx, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ctx chan poll fail. (ret=%d; devid=%u; hostpid=%d)\n", ret, ctx->devid, ctx->hostpid);
    }
    que_ctx_put(ctx);
}

static void que_ctx_f2nf_recv(unsigned int devid, unsigned int vqid, struct que_ctx *ctx)
{
    int ret;
    bool ini_try_flag;
    ASYNC_QUE_INI_EVENT event;

    (void)ctx;

    unsigned int qid = queue_get_actual_qid(vqid); /* trans to inner qid */
    ini_try_flag = que_chan_update_ini_status(devid, qid, INI_RECV_F2NF);
    if (ini_try_flag) {
        ret = que_chan_inter_dev_ini_proc(devid, qid);
        event = que_get_enque_event(ret);
        (void)que_chan_update_ini_status(devid, qid, event);
    }
}

void que_ctx_wait_f2nf(unsigned int devid)
{
    struct que_ctx *ctx = NULL;
    struct event_res *e_res = NULL;
    struct event_info back_event_info = {0};
    struct event_proc_result *result = NULL;
    int ret;

    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        QUEUE_LOG_ERR("que ctx get fail. (devid=%u)\n", devid);
        return;
    }

    if (ctx->f2nf_res.res_alloc_flag == 0) {
        QUEUE_LOG_ERR("que ctx event res not alloc. (devid=%u)\n", devid);
        goto out;
    }

    e_res = &ctx->f2nf_res.f2nf_event_res;

    while (1) {
        ret = halEschedWaitEvent(devid, e_res->gid, e_res->tid, -1, &back_event_info);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_WARN("halEschedWaitEvent invalid. (ret=%d; event_id=%u; gid=%u; tid=%u)\n", ret, e_res->event_id,
                           e_res->gid, e_res->tid);
            continue;
        }

        result = (struct event_proc_result *)back_event_info.priv.msg;
        if ((back_event_info.priv.msg_len == sizeof(struct event_proc_result)) &&
            (result->ret == QUEUE_IS_CLEAR_MAGIC)) {
            goto out;
        }
        que_ctx_f2nf_recv(devid, back_event_info.comm.subevent_id, ctx);
    }
out:
    que_ctx_put(ctx);
}

int que_ctx_bulid_f2nf_event(unsigned int devid)
{
    struct que_ctx *ctx = NULL;
    struct que_f2nf_res *f2nf_res = NULL;
    struct event_summary back_event = {0};
    struct event_proc_result rsp = {0};
    int ret;
    unsigned int dst_devid = (devid == halGetHostDevid()) ? devid : SCHED_INVALID_DEVID;

    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_UNINIT;
    }

    f2nf_res = &ctx->f2nf_res;
    back_event.dst_engine = f2nf_res->dst_engine;
    back_event.policy = ONLY;
    back_event.pid = f2nf_res->pid;
    back_event.grp_id = f2nf_res->f2nf_event_res.gid;
    back_event.event_id = f2nf_res->f2nf_event_res.event_id;
    back_event.subevent_id = f2nf_res->f2nf_event_res.subevent_id;
    back_event.msg_len = (unsigned int)sizeof(struct event_proc_result);
    back_event.msg = (char *)&rsp;
    back_event.tid = f2nf_res->f2nf_event_res.tid;
    rsp.ret = QUEUE_IS_CLEAR_MAGIC;

    ret = halEschedSubmitEventEx(devid, dst_devid, &back_event);
    que_ctx_put(ctx);
    return ret;
}

void que_ctx_cnt_info(unsigned int devid)
{
    struct que_ctx *ctx = NULL;
    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        QUEUE_LOG_ERR("que ctx get fail. (devid=%u)\n", devid);
        return;
    }
    QUEUE_RUN_LOG_INFO(
        "que ctx recv_cqe_total_cnt=%u, recv_cqe_fail_cnt=%u, wait_jfc_succ_cnt=%u, wait_jfc_fail_cnt=%u, "
        "poll_send_jfc_succ_cnt=%u, poll_send_jfc_fail_cnt=%u. (devid=%u)\n",
        ctx->cnt[RECV_CQE_TOTAL], ctx->cnt[RECV_CQE_FAIL], ctx->cnt[WAIT_JFC_SUCCESS], ctx->cnt[WAIT_JFC_FAIL],
        ctx->cnt[POLL_SEND_JFC_SUCCESS], ctx->cnt[POLL_SEND_JFC_FAIL], devid);
    que_ctx_put(ctx);
}

static int __attribute__((constructor)) que_comm_ctx_init(void)
{
    struct que_chan_ctx_agent_list *list = que_get_chan_ctx_agent();
    list->que_ctx_cnt_info_print = que_ctx_cnt_info;
    return DRV_ERROR_NONE;
}
#else /* EMU_ST */

void que_comm_ctx_emu_test(void)
{}

#endif /* EMU_ST */
