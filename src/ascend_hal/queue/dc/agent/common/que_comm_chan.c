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

#include "securec.h"
#include "ascend_hal.h"
#include "ascend_hal_error.h"
#include "queue_client_comm.h"
#include "uref.h"
#include "drv_buff_common.h"
#include "que_compiler.h"
#include "queue.h"
#include "queue_interface.h"
#include "que_uma.h"
#include "que_ub_msg.h"
#include "que_comm_event.h"
#include "que_ini_proc.h"
#include "que_tgt_proc.h"
#include "que_comm_agent.h"
#include "esched_user_interface.h"
#include "que_mem_merge.h"
#include "que_comm_chan.h"

#ifndef EMU_ST
struct que_chan *_que_chan_create(unsigned int devid, unsigned int qid, QUEUE_CHAN_TYPE chan_type,
                                  unsigned long create_time)
{
    struct que_chan *chan = NULL;
    chan = (struct que_chan *)calloc(1, sizeof(struct que_chan));
    if (que_unlikely(chan == NULL)) {
        QUEUE_LOG_ERR("que chan alloc fail. (devid=%u; qid=%u; size=%ld)\n", devid, qid, sizeof(struct que_chan));
        return NULL;
    }

    chan->devid = devid;
    chan->qid = qid;
    chan->create_time = create_time;
    chan->chan_type = chan_type;
    chan->token.token_id = NULL;
    chan->token.token.token = 0;
    uref_init(&chan->ref);
    QUEUE_LOG_DEBUG("que chan create. (devid=%u; qid=%u; create_time=%lu)\n", devid, qid, create_time);

    return chan;
}

void _que_chan_destroy(struct que_chan *chan)
{
    if (que_likely(chan != NULL)) {
        free(chan);
    }
}

static int que_chan_ini_init(struct que_chan *chan)
{
    struct que_ini_proc *ini_proc = NULL;
    ini_proc = que_ini_proc_create();
    if (que_unlikely(ini_proc == NULL)) {
        return DRV_ERROR_QUEUE_INNER_ERROR;
    }

    for (int i = 0; i < QUEUE_ENQUE_BUTT; i++) {
        chan->ini_proc[i] = ini_proc + i;
        que_mem_ctx_init(&chan->ini_proc[i]->mem_ctx);
    }
    (void)pthread_rwlock_init(&chan->ini_proc[ASYNC_ENQUE]->ini_status_lock, NULL);
    return DRV_ERROR_NONE;
}

static void que_chan_ini_uninit(struct que_chan *chan)
{
    if (que_likely(chan->ini_proc[0] != NULL)) {
        void **mbuf_array = chan->ini_proc[ASYNC_ENQUE]->mbuf_list.mbuf_array;
        if (mbuf_array != NULL) {
            free(mbuf_array);
            chan->ini_proc[ASYNC_ENQUE]->mbuf_list.mbuf_array = NULL;
        }

        que_ini_proc_destroy(chan->ini_proc[0]);
        for (int i = 0; i < QUEUE_ENQUE_BUTT; i++) {
            chan->ini_proc[i] = NULL;
        }
    }
}

int que_chan_tgt_init(struct que_chan *chan)
{
    struct que_tgt_proc_attr attr = {.devid = chan->devid, .qid = chan->qid};
    struct que_tgt_proc *tgt_proc = NULL;

    tgt_proc = que_tgt_proc_create(&attr);
    if (que_unlikely(tgt_proc == NULL)) {
        QUEUE_LOG_ERR("que tgt proc create fail. (devid=%u; qid=%u)\n", chan->devid, chan->qid);
        return DRV_ERROR_QUEUE_INNER_ERROR;
    }
    tgt_proc->pre_pkt_sn = 0x1FFu; /* pre_pkt_sn is initialized to an invalid value. */
    chan->tgt_proc = tgt_proc;

    return DRV_ERROR_NONE;
}

void que_chan_tgt_uninit(struct que_chan *chan)
{
    if (que_likely(chan->tgt_proc != NULL)) {
        que_tgt_proc_destroy(chan->tgt_proc);
        chan->tgt_proc = NULL;
    }
}

static int que_chan_init(struct que_chan *chan, unsigned int d2d_flag)
{
    int ret;

    ret = que_chan_ini_init(chan);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que chan enque init fail. (ret=%d; devid=%u; qid=%u)\n", ret, chan->devid, chan->qid);
        return ret;
    }

    ret = que_chan_tgt_init(chan);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        que_chan_ini_uninit(chan);
        QUEUE_LOG_ERR("que chan enque init fail. (ret=%d; devid=%u; qid=%u)\n", ret, chan->devid, chan->qid);
        return ret;
    }
    if ((chan->chan_type == CHAN_INTER_DEV_ATTACH) && (chan->devid == halGetHostDevid())) {
        return DRV_ERROR_NONE;
    }
    ret = que_urma_token_alloc(chan->devid, &chan->token, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        que_chan_tgt_uninit(chan);
        que_chan_ini_uninit(chan);
        QUEUE_LOG_ERR("urma token init fail. (ret=%d; urma_devid=%u)\n", ret, chan->devid);
        return ret;
    }

    return ret;
}

void que_chan_uninit(struct que_chan *chan)
{
    que_chan_tgt_uninit(chan);
    que_chan_ini_uninit(chan);
    que_urma_token_free(chan->devid, &chan->token);
}

int que_chan_create_check(unsigned int devid, unsigned int qid, unsigned long create_time)
{
    int ret;
    struct que_chan *chan = NULL;

    chan = que_chan_get(devid, qid);
    if (chan == NULL) {
        return DRV_ERROR_NONE;
    }
    if (chan->create_time == create_time) {
        QUEUE_LOG_DEBUG("que chan repeat create. (devid=%u; qid=%u; create_time=%lu)\n", devid, qid, create_time);
        que_chan_put(chan);
        return DRV_ERROR_QUEUE_REPEEATED_INIT;
    } else {
        ret = que_chan_del(chan);
        if (que_likely(ret == DRV_ERROR_NONE)) {
            que_chan_put(chan);
        }
        que_chan_put(chan);
    }
    return DRV_ERROR_NONE;
}

int que_chan_create(unsigned int devid, unsigned int qid, QUEUE_CHAN_TYPE chan_type, unsigned long create_time,
                    unsigned int d2d_flag)
{
    struct que_chan *chan = NULL;
    int ret;
    chan = que_chan_get(devid, qid);
    if (chan != NULL) {
        que_urma_token_free(devid, &chan->token);
        ret = que_urma_token_alloc(devid, &chan->token, d2d_flag);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que chan token update fail. (ret=%d; devid=%u)\n", ret, devid);
        }
        que_chan_put(chan);
        return ret;
    }
    chan = _que_chan_create(devid, qid, chan_type, create_time);
    if (que_unlikely(chan == NULL)) {
        QUEUE_LOG_ERR("que chan create fail. (devid=%u; qid=%u)\n", devid, qid);
        return DRV_ERROR_QUEUE_INNER_ERROR;
    }

    ret = que_chan_init(chan, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que chan init fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        goto err_que_chan_init;
    }

    ret = que_chan_add(chan);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_INFO("que chan add check. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        goto err_que_chan_add;
    }

    return DRV_ERROR_NONE;
err_que_chan_add:
    que_chan_uninit(chan);
err_que_chan_init:
    _que_chan_destroy(chan);
    return ret;
}

int que_chan_destroy(unsigned int devid, unsigned int qid)
{
    int ret = DRV_ERROR_NONE; /* According to the interface, if the queue has not been created, return success. */
    struct que_chan *chan = NULL;

    chan = que_chan_get(devid, qid);
    if (que_likely(chan != NULL)) {
        ret = que_chan_del(chan);
        if (que_likely(ret == DRV_ERROR_NONE)) {
            que_chan_put(chan);
        }
        que_chan_put(chan);
    }

    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que destroy fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
    }

    return ret;
}

int que_chan_update_jfs_info(unsigned int devid, unsigned int qid, struct que_jfs_pool_info *jfs_pool,
                             struct que_jfr *qjfr, urma_jfr_id_t *tjfr_id, urma_token_t token)
{
    struct que_chan *chan = NULL;

    chan = que_chan_get(devid, qid);
    if (que_unlikely(chan == NULL)) {
        QUEUE_LOG_ERR("que chan get fail. (devid=%u; qid=%u)\n", devid, qid);
        return DRV_ERROR_UNINIT;
    }

    if (chan->ini_proc[ASYNC_ENQUE] != NULL) {
        chan->ini_proc[ASYNC_ENQUE]->jfs_info = jfs_pool;
        chan->ini_proc[ASYNC_ENQUE]->qjfr = qjfr;
        chan->ini_proc[ASYNC_ENQUE]->token_info.token = token;
        chan->ini_proc[ASYNC_ENQUE]->token_info.token_id = NULL;
    }
    *tjfr_id = qjfr->jfr->jfr_id;
    que_chan_put(chan);
    return DRV_ERROR_NONE;
}

static pthread_mutex_t g_queue_ctx_mutex = PTHREAD_MUTEX_INITIALIZER;
static int _que_qjfs_alloc(struct que_jfs_pool_info *jfs_pool, int *idx)
{
    int index;
    (void)pthread_mutex_lock(&g_queue_ctx_mutex);
    for (index = 0; index < QUE_PKT_SEND_JETTY_POOL_DEPTH; index++) {
        if (jfs_pool[index].jfs_busy_flag == false) {
            *idx = index;
            jfs_pool[index].jfs_busy_flag = true;
            (void)pthread_mutex_unlock(&g_queue_ctx_mutex);
            return DRV_ERROR_NONE;
        }
    }
    (void)pthread_mutex_unlock(&g_queue_ctx_mutex);
    return DRV_ERROR_WAIT_TIMEOUT;
}

int que_qjfs_alloc(struct que_jfs_pool_info *jfs_pool, int timeout, int *idx, unsigned int d2d_flag)
{
    int idx_tmp = 0;
    struct que_jfs *qjfs = NULL;
    int ret = DRV_ERROR_NONE;
    int timeout_ms_ = timeout;

    while (timeout_ms_ > 0) {
        struct timeval start, end;
        que_get_time(&start);
        ret = _que_qjfs_alloc(jfs_pool, &idx_tmp);
        if (ret == DRV_ERROR_NONE) {
            break;
        }
        que_get_time(&end);
        queue_updata_timeout(start, end, &timeout_ms_);
    }
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que qjfs alloc fail. (ret=%d)\n", ret);
        return ret;
    }

    qjfs = jfs_pool[idx_tmp].qjfs;
    if (qjfs->jfs == NULL) {
        ret = que_recreate_jfs(qjfs->attr.jfs_depth, qjfs->jfc_s, qjfs->devid, &qjfs->jfs, d2d_flag);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que recreate jfs fail. (ret=%d)\n", ret);
            que_qjfs_free(jfs_pool, idx_tmp);
            return ret;
        }
    }
    *idx = idx_tmp;
    return DRV_ERROR_NONE;
}

void que_qjfs_free(struct que_jfs_pool_info *jfs_pool, int idx)
{
    (void)pthread_mutex_lock(&g_queue_ctx_mutex);
    jfs_pool[idx].jfs_busy_flag = false;
    (void)pthread_mutex_unlock(&g_queue_ctx_mutex);
}

static int _que_qjfr_alloc(struct que_jfr_pool_info *jfr_pool, int *idx)
{
    int index;
    (void)pthread_mutex_lock(&g_queue_ctx_mutex);
    for (index = 0; index < QUE_PKT_SEND_JETTY_POOL_DEPTH; index++) {
        if (jfr_pool[index].jfr_busy_flag == false) {
            *idx = index;
            jfr_pool[index].jfr_busy_flag = true;
            (void)pthread_mutex_unlock(&g_queue_ctx_mutex);
            return DRV_ERROR_NONE;
        }
    }
    (void)pthread_mutex_unlock(&g_queue_ctx_mutex);
    return DRV_ERROR_WAIT_TIMEOUT;
}

int que_qjfr_alloc(struct que_jfr_pool_info *jfr_pool, int timeout, int *idx)
{
    int ret = DRV_ERROR_NONE;
    int timeout_ms_ = timeout;

    while (timeout_ms_ > 0) {
        struct timeval start, end;
        que_get_time(&start);
        ret = _que_qjfr_alloc(jfr_pool, idx);
        if (ret == DRV_ERROR_NONE) {
            break;
        }
        que_get_time(&end);
        queue_updata_timeout(start, end, &timeout_ms_);
    }
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("Que qjfr alloc fail. (ret=%d)\n", ret);
    }
    return ret;
}

void que_qjfr_free(struct que_jfr_pool_info *jfr_pool, int idx)
{
    (void)pthread_mutex_lock(&g_queue_ctx_mutex);
    jfr_pool[idx].jfr_busy_flag = false;
    (void)pthread_mutex_unlock(&g_queue_ctx_mutex);
}

void que_ini_timeout_print(struct que_ini_proc *ini_proc)
{
    uint64_t curr_delta;
    unsigned int iovec_idx, id_start, id_end;
    int ini_log_level = que_get_ini_log_level();
    uint64_t *timestamp = ini_proc->timestamp;
    if (timestamp == NULL) {
        return;
    }

    curr_delta = timestamp[TRACE_FINISH] - timestamp[TRACE_INI_START];
    if ((curr_delta / NS_PER_SECOND) > QUE_TIMEOUT_SECOND) {
        QUEUE_RUN_LOG_INFO_FLOWCTRL(
            "que ini proc timeout, que_type=%d, cost_time=%luns, "
            "start=%lu, update=%lu, pkt_seg_create_start=%lu, pkt_seg_create_end=%lu, ctx_seg_create_start=%lu, "
            "ctx_seg_create_end=%lu, tx_create=%lu, pkt_fill=%lu, tx_send=%lu, ack_wait_start=%lu, tgt_time=%uns, "
            "ack_wait_end=%lu, "
            "finish=%lu. (devid=%d, qid=%u)\n",
            ini_proc->que_type, curr_delta, timestamp[TRACE_INI_START], timestamp[TRACE_UPDATE],
            timestamp[TRACE_PKT_SEG_CREATE_START], timestamp[TRACE_PKT_SEG_CREATE_END],
            timestamp[TRACE_CTX_SEG_CREATE_START], timestamp[TRACE_CTX_SEG_CREATE_END], timestamp[TRACE_TX_CREATE],
            timestamp[TRACE_PKT_FILL], timestamp[TRACE_TX_SEND], timestamp[TRACE_ACK_WAIT_START], ini_proc->tgt_time,
            timestamp[TRACE_ACK_WAIT_END], timestamp[TRACE_FINISH], ini_proc->devid, ini_proc->qid);
    }

    if (ini_log_level != 0) {
        for (iovec_idx = 0; iovec_idx < ini_proc->total_iovec_num; iovec_idx++) {
            id_start = TRACE_INI_IOVEC_SEG_CREATE_START + iovec_idx * (TRACE_INI_LEVLE1_BUTT - TRACE_INI_LEVLE1_START);
            id_end = TRACE_INI_IOVEC_SEG_CREATE_END + iovec_idx * (TRACE_INI_LEVLE1_BUTT - TRACE_INI_LEVLE1_START);
            QUEUE_RUN_LOG_INFO(
                "que seg create timeout, que_type=%d, iovec_idx=%d, seg_create_start=%lu, seg_create_end=%lu. "
                "(devid=%d, qid=%u)\n",
                ini_proc->que_type, iovec_idx, timestamp[id_start], timestamp[id_end], ini_proc->devid, ini_proc->qid);
        }
    }
}

void que_ini_timestamp_destroy(struct que_ini_proc *ini_proc)
{
    if (que_likely(ini_proc->timestamp != NULL)) {
        free(ini_proc->timestamp);
        ini_proc->timestamp = NULL;
    }
}

int que_chan_done(unsigned int devid, unsigned int qid, QUEUE_AGENT_TYPE que_type)
{
    struct que_chan *chan = NULL;

    chan = que_chan_get(devid, qid);
    if (que_unlikely(chan == NULL)) {
        QUEUE_LOG_ERR("que chan get fail. (devid=%u; qid=%u)\n", devid, qid);
        return DRV_ERROR_QUEUE_NOT_CREATED;
    }
    que_ini_time_stamp(chan->ini_proc[que_type], TRACE_FINISH);
    que_ini_timeout_print(chan->ini_proc[que_type]);
    que_ini_timestamp_destroy(chan->ini_proc[que_type]);
    que_ini_proc_done(chan->ini_proc[que_type]);
    que_chan_put(chan);
    return DRV_ERROR_NONE;
}

static int que_chan_alloc_jetty_for_data_wr(unsigned int devid, struct que_tgt_proc *tgt_proc, bool default_wr_flag)
{
    unsigned int jetty_idx;
    struct que_jfs_rw_wr_attr attr = {.wr_num = QUE_MAX_RW_WR_NUM, .opcode = URMA_OPC_READ};
    struct que_jfs_rw_wr *rw_wr = NULL;

    jetty_idx = que_rw_jetty_alloc(devid, tgt_proc->d2d_flag);
    if (jetty_idx >= QUE_DATA_RW_JETTY_POOL_DEPTH) {
        QUEUE_LOG_ERR("que jetty alloc fail. (devid=%u)\n", devid);
        return DRV_ERROR_INNER_ERR;
    }

    tgt_proc->data_read_jetty_idx = jetty_idx;
    tgt_proc->data_read_jetty = que_qjfs_get(devid, jetty_idx, tgt_proc->d2d_flag);

    if (default_wr_flag) {
        tgt_proc->rw_wr = que_send_wr_get(devid, jetty_idx, tgt_proc->d2d_flag);
        tgt_proc->rw_wr->cur_wr_idx = 0;
        tgt_proc->rw_wr->max_wr_num = QUE_DEFAULT_RW_WR_NUM;
    } else {
        rw_wr = que_jfs_rw_wr_create(&attr);
        if (que_unlikely(rw_wr == NULL)) {
            QUEUE_LOG_ERR("que rw wr alloc fail. (devid=%u)\n", devid);
            goto jetty_free;
        }
        tgt_proc->rw_wr = rw_wr;
        tgt_proc->rw_wr->cur_wr_idx = 0;
        tgt_proc->rw_wr->max_wr_num = QUE_MAX_RW_WR_NUM;
    }
    tgt_proc->default_wr_flag = default_wr_flag;
    return DRV_ERROR_NONE;

jetty_free:
    que_rw_jetty_free(devid, jetty_idx, tgt_proc->d2d_flag);
    return DRV_ERROR_INNER_ERR;
}

static void que_chan_free_jetty_for_data_wr(struct que_tgt_proc *tgt_proc)
{
    que_rw_jetty_free(tgt_proc->devid, tgt_proc->data_read_jetty_idx, tgt_proc->d2d_flag);
    if (!tgt_proc->default_wr_flag) {
        que_jfs_rw_wr_destroy(tgt_proc->rw_wr);
    }
    tgt_proc->rw_wr = NULL;
}

static void que_fill_ack_send_jfs(struct que_jfs *qjfs, struct que_ack_jfs *ack_send_jfs)
{
    ack_send_jfs->attr = qjfs->attr;
    ack_send_jfs->devid = qjfs->devid;
    ack_send_jfs->jfce_s = qjfs->jfce_s;
    ack_send_jfs->jfc_s = qjfs->jfc_s;
    ack_send_jfs->jfs = &qjfs->jfs;
}

static void que_abnormal_ack(struct que_pkt *pkt, struct que_jfs *qjfs, urma_target_jetty_t *tjetty,
                             struct que_tgt_proc *tgt_proc, uint64_t tgt_start_time, int result)
{
    que_ack_data ack_data;
    struct que_ack_jfs ack_send_jetty;
    uint64_t curtime;
    int ret;

    curtime = que_get_cur_time_ns();
    ack_data.ack_msg.sn = QUE_ACK_PACK_SN(pkt->head.sn);
    ack_data.ack_msg.tgt_time = QUE_ACK_PACK_TGT_TIME_NS((curtime > tgt_start_time) ? (curtime - tgt_start_time) :
                                                                                      0ULL);
    ack_data.ack_msg.result = QUE_ACK_RESULT_PACK_16(result);
    ack_send_jetty.tjetty = tjetty;

    que_fill_ack_send_jfs(qjfs, &ack_send_jetty);

    ret = que_rx_send_ack_and_wait(tgt_proc, ack_data.imm_data, &ack_send_jetty, tgt_proc->d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("send ack and wait fail. (qid=%u; ret=%d)\n", pkt->head.qid, ret);
        ATOMIC_INC((volatile int *)&tgt_proc->cnt[TGT_SEND_ACK_FAIL]);
    } else {
        ATOMIC_INC((volatile int *)&tgt_proc->cnt[TGT_SEND_ACK_SUCCESS]);
    }
}

void que_tgt_timestamp_update(struct que_tgt_proc *tgt_proc, struct que_pkt *pkt, uint64_t *stamp)
{
    int tgt_log_level = que_get_tgt_log_level();
    unsigned int num = TRACE_TGT_LEVLE0_BUTT + pkt->head.total_iovec_num *
                                                   (TRACE_TGT_LEVLE1_BUTT - TRACE_TGT_LEVLE1_START) *
                                                   (unsigned int)tgt_log_level;
    uint64_t *timestamp = calloc(num, sizeof(uint64_t));
    if (timestamp == NULL) {
        return;
    }
    timestamp[TRACE_TGT_START] = stamp[TRACE_TGT_START];
    timestamp[TRACE_IMPORT_JETTY] = stamp[TRACE_IMPORT_JETTY];
    tgt_proc->total_iovec_num = pkt->head.total_iovec_num;
    tgt_proc->timestamp = timestamp;
}

static int que_chan_tgt_update(struct que_chan *chan, unsigned int devid, unsigned int qid, struct que_jfs *qjfs,
                               struct que_pkt *pkt, uint64_t *tgt_import_jetty_time, uint64_t tgt_start_time,
                               unsigned int d2d_flag)
{
    int ret, result = DRV_ERROR_BUSY;
    urma_target_jetty_t *tjetty = NULL;
    struct que_tgt_proc *tgt_proc = NULL;

    tjetty = que_jfr_import(devid, &pkt->head.jfr_id, &pkt->head.token, d2d_flag);
    if (que_unlikely(tjetty == NULL)) {
        QUEUE_LOG_ERR("que tjetty import fail. (devid=%u; qid=%u)\n", devid, qid);
        return DRV_ERROR_PARA_ERROR;
    }
    *tgt_import_jetty_time = que_get_cur_time_ns();

    if (qjfs->jfs == NULL) {
        ret = que_recreate_jfs(qjfs->attr.jfs_depth, qjfs->jfc_s, qjfs->devid, &qjfs->jfs, d2d_flag);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que recreate jfs fail. (ret=%d)\n", ret);
            return ret;
        }
    }

    tgt_proc = chan->tgt_proc;
    if (tgt_proc->rx != NULL) {
        /* when the ini node starts the next proc, the previous proc at the tgt node may not have completed yet. */
        QUEUE_LOG_ERR("que chan last proc is not init. (qid=%u)\n", qid);
        goto abnormal_ack;
    }

    if (tgt_proc->pre_pkt_sn == pkt->head.sn) {
        QUEUE_LOG_WARN("The same packet has already been received. (qid=%u, pre_sn=%u, sn=%u)\n", tgt_proc->qid,
                       tgt_proc->pre_pkt_sn, pkt->head.sn);
        result = DRV_ERROR_TRANS_LINK_ACK_TIMEOUT_ERR;
        goto abnormal_ack;
    }

    tgt_proc->devid = devid;
    tgt_proc->qid = qid;
    tgt_proc->que_type = pkt->head.que_type;
    tgt_proc->peer_qid = pkt->head.peer_qid;
    tgt_proc->d2d_flag = d2d_flag;

    ret = que_chan_alloc_jetty_for_data_wr(devid, tgt_proc, pkt->head.default_wr_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que chan is alloc jetty wr fail. (qid=%u)\n", qid);
        goto abnormal_ack;
    }

    que_fill_ack_send_jfs(qjfs, &tgt_proc->ack_send_jetty);

    tgt_proc->data_read_jetty->tjetty = tjetty;
    tgt_proc->ack_send_jetty.tjetty = tjetty;
    tgt_proc->pre_pkt_sn = pkt->head.sn;
    tgt_proc->is_finished = 0;
    tgt_proc->usr_ctx_addr = (unsigned long long)(uintptr_t)chan;
    return DRV_ERROR_NONE;

abnormal_ack:
    que_abnormal_ack(pkt, qjfs, tjetty, tgt_proc, tgt_start_time, result);
    return DRV_ERROR_INNER_ERR;
}

static void que_chan_tgt_rollback(struct que_chan *chan)
{
    que_chan_free_jetty_for_data_wr(chan->tgt_proc);
}

int que_chan_tgt_recv(unsigned int urma_devid, struct que_jfs *qjfs, struct que_pkt *pkt, unsigned int d2d_flag)
{
    int ret;
    struct que_chan *chan = NULL;
    unsigned int actual_qid;
    uint64_t cur_time, tgt_basetime = 0;
    uint64_t stamp[TRACE_IMPORT_JETTY_BUFF] = {0};
    unsigned int devid = que_get_chan_devid(urma_devid);

    if (pkt == NULL) {
        QUEUE_LOG_ERR("que chan pkt addr is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    actual_qid = queue_get_actual_qid(pkt->head.qid);
    cur_time = que_get_cur_time_ns();
    tgt_basetime = que_get_tgt_basetime(devid);
    /* Logs are recorded when the time of the UB link is longer NS_PER_SECOND than the time of the que init send event.
     */
    if ((cur_time + pkt->head.ini_base_timestamp) > (tgt_basetime + pkt->head.pkt_timestamp + NS_PER_SECOND)) {
        QUEUE_RUN_LOG_INFO_FLOWCTRL(
            "que chan pkt send over time, send_cost_time=%luns; clt_base=%lu, svr_base=%lu,"
            "ini_send=%lu, tgt_proc=%lu. (qid=%u; devid=%u)\n",
            ((cur_time + pkt->head.ini_base_timestamp) - (tgt_basetime + pkt->head.pkt_timestamp)),
            pkt->head.ini_base_timestamp, tgt_basetime, pkt->head.pkt_timestamp, cur_time, actual_qid, devid);
    }

    chan = que_chan_get(devid, actual_qid);
    if (que_unlikely(chan == NULL)) {
        QUEUE_LOG_ERR("que chan is not init. (qid=%u)\n", pkt->head.qid);
        return DRV_ERROR_QUEUE_NOT_CREATED;
    }

    stamp[TRACE_TGT_START] = que_get_cur_time_ns();
    ret = que_chan_tgt_update(chan, urma_devid, actual_qid, qjfs, pkt, &stamp[TRACE_IMPORT_JETTY],
                              stamp[TRACE_TGT_START], d2d_flag);
    if (ret != DRV_ERROR_NONE) {
        que_chan_put(chan);
        return ret;
    }

    que_tgt_timestamp_update(chan->tgt_proc, pkt, stamp);
    que_tgt_time_stamp(chan->tgt_proc, TRACE_CHAN_UPDATE);
    que_tgt_pkt_proc(chan->tgt_proc, pkt);
    if (chan->tgt_proc->is_finished) {
        que_chan_tgt_rollback(chan); /* Symmetric with que_chan_tgt_update */
        que_chan_put(chan);
    }
    return DRV_ERROR_NONE;
}

int que_chan_tgt_data_read_and_ack(urma_cr_t *cr)
{
    struct que_chan *chan = (struct que_chan *)cr->user_ctx;
    int cr_status = cr->status;

    if (que_unlikely(chan == NULL)) {
        QUEUE_LOG_ERR("que chan is not init, cr_status=%d.\n", cr->status);
        return DRV_ERROR_QUEUE_NOT_CREATED;
    }
    que_tgt_pkt_proc_ex(chan->tgt_proc, cr_status);
    if (chan->tgt_proc->is_finished) {
        que_chan_tgt_rollback(chan); /* Symmetric with que_chan_tgt_update */
        que_chan_put(chan);
    }
    return DRV_ERROR_NONE;
}

static bool que_mbuf_array_is_full(struct que_mbuf_list *mbuf_list)
{
    unsigned int slot = (mbuf_list->tail >= mbuf_list->head) ?
                            (mbuf_list->head + mbuf_list->depth - 1 - mbuf_list->tail) :
                            (mbuf_list->head - mbuf_list->tail - 1);
    return (slot == 0);
}

static inline bool que_mbuf_array_is_empty(struct que_mbuf_list *mbuf_list)
{
    return (mbuf_list->tail == mbuf_list->head);
}

static int que_mbuf_node_add_tail(struct que_mbuf_list *mbuf_list, void *mbuf)
{
    (void)pthread_rwlock_wrlock(&mbuf_list->mbuf_lock);
    unsigned int tail = mbuf_list->tail;

    if (que_unlikely(que_mbuf_array_is_full(mbuf_list))) {
        (void)pthread_rwlock_unlock(&mbuf_list->mbuf_lock);
        return DRV_ERROR_QUEUE_FULL;
    }
    mbuf_list->mbuf_array[tail] = mbuf;
    mbuf_list->tail = (tail + 1) % mbuf_list->depth;

    (void)pthread_rwlock_unlock(&mbuf_list->mbuf_lock);
    return DRV_ERROR_NONE;
}

static void *que_mbuf_node_peek(struct que_mbuf_list *mbuf_list)
{
    (void)pthread_rwlock_rdlock(&mbuf_list->mbuf_lock);
    void *mbuf = NULL;
    unsigned int head = mbuf_list->head;

    if (que_unlikely(que_mbuf_array_is_empty(mbuf_list))) {
        (void)pthread_rwlock_unlock(&mbuf_list->mbuf_lock);
        return NULL;
    }
    mbuf = mbuf_list->mbuf_array[head];
    (void)pthread_rwlock_unlock(&mbuf_list->mbuf_lock);
    return mbuf;
}

static void free_mbuf_for_async_queue(uint32_t devid, uint32_t qid, void *mbuf)
{
    int ret;
    uint64_t buff_len = 0;
    void *in_buff = NULL, *buff = NULL;
    struct MbufTypeInfo type_info;
    unsigned int out_len = sizeof(struct MbufTypeInfo);

    in_buff = mbuf;
    ret = halBuffGetInfo(BUFF_GET_MBUF_TYPE_INFO, (void *)(uintptr_t)&in_buff, sizeof(in_buff),
                         (void *)(uintptr_t)&type_info, &out_len);
    if ((ret == DRV_ERROR_NONE) && (type_info.type == MBUF_CREATE_BY_BUILD)) {
        ret = halMbufUnBuild(in_buff, &buff, &buff_len);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("mbuf unbuild fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        } else {
            halBuffPut(NULL, buff);
        }
    } else {
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("mbuf get type info fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        }
        halMbufFree(in_buff);
    }
}

static void que_mbuf_node_del_head(unsigned int devid, unsigned int qid, struct que_mbuf_list *mbuf_list)
{
    bool full_flag = 0;
    int ret;
    (void)pthread_rwlock_wrlock(&mbuf_list->mbuf_lock);
    unsigned int head = mbuf_list->head;
    if (que_unlikely(que_mbuf_array_is_empty(mbuf_list))) {
        (void)pthread_rwlock_unlock(&mbuf_list->mbuf_lock);
        return;
    }

    free_mbuf_for_async_queue(devid, qid, mbuf_list->mbuf_array[head]);
    mbuf_list->mbuf_array[head] = NULL;

    full_flag = que_mbuf_array_is_full(mbuf_list);
    if (full_flag) {
        ret = que_inter_dev_send_f2nf(devid, qid);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("send f2nf fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        }
    }

    mbuf_list->head = (head + 1) % mbuf_list->depth;
    (void)pthread_rwlock_unlock(&mbuf_list->mbuf_lock);
}

static void que_mbuf_node_clear(unsigned int devid, unsigned int qid, struct que_mbuf_list *mbuf_list)
{
    unsigned int head;

    if (mbuf_list->mbuf_array == NULL) {
        return;
    }

    (void)pthread_rwlock_wrlock(&mbuf_list->mbuf_lock);
    while (!que_mbuf_array_is_empty(mbuf_list)) {
        head = mbuf_list->head;
        free_mbuf_for_async_queue(devid, qid, mbuf_list->mbuf_array[head]);
        mbuf_list->mbuf_array[head] = NULL;
        mbuf_list->head = (head + 1) % mbuf_list->depth;
    }
    (void)pthread_rwlock_unlock(&mbuf_list->mbuf_lock);
}

static void mbuf_convert_to_buff_iovec(void *mbuf, struct buff_iovec *vector)
{
    void *usr_data = NULL;
    unsigned int size = 0;
    Mbuf *mbuf_ = (Mbuf *)mbuf;

    (void)halMbufGetPrivInfo(mbuf_, &usr_data, &size);
    vector->context_base = usr_data;
    vector->context_len = size;
    vector->count = 1;
    vector->ptr[0].iovec_base = mbuf_->data;
    vector->ptr[0].len = (mbuf_->data_len == 0) ? mbuf_->total_len : mbuf_->data_len; // follow flowgw
}

static int que_get_iovector(struct que_ini_proc *ini_proc)
{
    struct buff_iovec *vector = NULL;
    void *mbuf_head = NULL;

    vector = malloc(sizeof(struct buff_iovec) + sizeof(struct iovec_info));
    if (vector == NULL) {
        QUEUE_LOG_ERR("buff_iovec alloc failed\n");
        return DRV_ERROR_OVER_LIMIT;
    }

    mbuf_head = que_mbuf_node_peek(&ini_proc->mbuf_list);
    if (mbuf_head == NULL) {
        free(vector);
        return DRV_ERROR_QUEUE_EMPTY;
    }

    mbuf_convert_to_buff_iovec(mbuf_head, vector);
    ini_proc->vector = vector;
    return DRV_ERROR_NONE;
}

void que_put_iovector(struct que_ini_proc *ini_proc)
{
    free(ini_proc->vector);
    ini_proc->vector = NULL;
}

static int que_get_inter_dev_tjetty(unsigned int devid, unsigned int qid, urma_jfr_id_t *tjfr_id, urma_token_t *token,
                                    struct que_ini_proc *ini_proc)
{
    unsigned int urma_devid = que_get_urma_devid(devid, ini_proc->peer_devid);
    urma_target_jetty_t *tjetty = NULL;

    tjetty = que_jfr_import(urma_devid, tjfr_id, token, ini_proc->d2d_flag);
    if (que_unlikely(tjetty == NULL)) {
        QUEUE_LOG_ERR("que jfr import fail. (devid=%u; urma_devid=%u; qid=%d)\n", devid, urma_devid, qid);
        return DRV_ERROR_QUEUE_INNER_ERROR;
    }
    ini_proc->tjetty = tjetty;
    return DRV_ERROR_NONE;
}

void que_get_d2d_flag(unsigned int devid, unsigned int peer_devid, unsigned int *d2d_flag)
{
    if ((devid != halGetHostDevid()) && (peer_devid != halGetHostDevid())) {
        *d2d_flag = 1; /* 1:d2d */
    } else {
        *d2d_flag = 0; /* 1:not d2d */
    }
}

static int que_chan_ini_pre_proc(unsigned int devid, unsigned int qid, struct que_ini_proc *ini_proc)
{
    int ret;
    unsigned int peer_qid, d2d_flag;
    void **mbuf_array = NULL;
    struct que_peer_que_attr peer_que_attr;

    ini_proc->devid = devid;
    ini_proc->qid = qid;
    ini_proc->que_type = ASYNC_ENQUE;

    ret = que_get_peer_que_info(devid, qid, &peer_qid, &peer_que_attr);
    if (ret != DRV_ERROR_NONE) {
        return (ret == DRV_ERROR_NO_RESOURCES) ? DRV_ERROR_RESUME : ret;
    }

    if (!peer_que_attr.tjfr_valid_flag) {
        return DRV_ERROR_RESUME;
    }

    if (ini_proc->mbuf_list.mbuf_array != NULL) {
        return DRV_ERROR_NONE;
    }
    que_get_d2d_flag(devid, peer_que_attr.peer_devid, &d2d_flag);
    ini_proc->peer_qid = peer_qid;
    ini_proc->peer_devid = peer_que_attr.peer_devid;
    ini_proc->d2d_flag = d2d_flag;

    (void)pthread_rwlock_wrlock(&ini_proc->ini_status_lock);
    if (ini_proc->mbuf_list.mbuf_array == NULL) {
        ret = que_get_inter_dev_tjetty(devid, qid, &peer_que_attr.tjfr_id, &peer_que_attr.token, ini_proc);
        if (ret != DRV_ERROR_NONE) {
            (void)pthread_rwlock_unlock(&ini_proc->ini_status_lock);
            return ret;
        }

        mbuf_array = (void **)calloc(peer_que_attr.depth, sizeof(void *));
        if (que_unlikely(mbuf_array == NULL)) {
            (void)pthread_rwlock_unlock(&ini_proc->ini_status_lock);
            return DRV_ERROR_QUEUE_INNER_ERROR;
        }
        ini_proc->mbuf_list.mbuf_array = mbuf_array;
        ini_proc->mbuf_list.depth = peer_que_attr.depth;
        (void)pthread_rwlock_init(&ini_proc->mbuf_list.mbuf_lock, NULL);
    }
    (void)pthread_rwlock_unlock(&ini_proc->ini_status_lock);
    return DRV_ERROR_NONE;
}

int que_chan_async_pre_proc(unsigned int devid, unsigned int qid, void *mbuf)
{
    int ret = DRV_ERROR_INNER_ERR;
    struct que_chan *chan = NULL;
    struct que_ini_proc *ini_proc = NULL;

    chan = que_chan_get(devid, qid);
    if (que_unlikely(chan == NULL)) {
        QUEUE_LOG_ERR("que chan is not init. (qid=%u)\n", qid);
        return DRV_ERROR_QUEUE_NOT_CREATED;
    }

    ini_proc = chan->ini_proc[ASYNC_ENQUE];
    ret = que_chan_ini_pre_proc(devid, qid, ini_proc);
    if (ret != DRV_ERROR_NONE) {
        goto out;
    }

    if (ini_proc->ini_status == INI_ABNORMAL) {
        ret = DRV_ERROR_TRANS_LINK_ABNORMAL;
        goto out;
    }

    ret = que_mbuf_node_add_tail(&ini_proc->mbuf_list, mbuf);
    if (ret != DRV_ERROR_NONE) {
        goto out;
    }

out:
    que_chan_put(chan);
    return ret;
}

static int que_inter_dev_ini_init(unsigned int devid, unsigned int qid, struct que_ini_proc *ini_proc)
{
    int ret;
    int jfs_idx;
    ret = que_qjfs_alloc(ini_proc->jfs_info, QUE_JETTY_ALLOC_TIME_OUT_MS, &jfs_idx, ini_proc->d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que jfs alloc fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        return ret;
    }
    ini_proc->jfs_idx = (unsigned int)jfs_idx;
    ini_proc->pkt_send_jetty = ini_proc->jfs_info[jfs_idx].qjfs;
    ini_proc->tx = NULL;
    ini_proc->imm_recv_jetty = NULL;
    ini_proc->recv_para = NULL;
    ini_proc->pkt_send_jetty->tjetty = ini_proc->tjetty;
    return DRV_ERROR_NONE;
}

static void que_inter_dev_ini_uninit(struct que_ini_proc *ini_proc)
{
    que_qjfs_free(ini_proc->jfs_info, (int)ini_proc->jfs_idx);
    ini_proc->pkt_send_jetty->tjetty = NULL;
    ini_proc->pkt_send_jetty = NULL;
}

static bool que_wait_f2nf_timeout(uint64 timestamp, struct que_ini_proc *ini_proc)
{
    if (((timestamp - ini_proc->wait_f2nf_start_time) / NS_PER_SECOND) > QUE_TIMEOUT_SECOND) {
        ini_proc->wait_f2nf_start_time = timestamp;
        return true;
    }
    return false;
}

static bool que_update_status(struct que_ini_proc *ini_proc, ASYNC_QUE_INI_EVENT event)
{
    bool ini_try_flag = 0;
    uint64 curr_time = que_get_cur_time_ns();
    (void)pthread_rwlock_wrlock(&ini_proc->ini_status_lock);
    switch (ini_proc->ini_status) {
        case INI_ABNORMAL:
            break;
        case INI_IDLE:
            if (event == INI_ENQUE_TRY) {
                ini_try_flag = 1;
                ini_proc->ini_status = INI_ENQUE_BUSY;
            }
            break;
        case INI_ENQUE_BUSY:
            if (event == INI_ENQUE_EMPTY) {
                ini_proc->ini_status = INI_IDLE;
            } else if (event == INI_ENQUE_ERROR) {
                ini_proc->ini_status = INI_ABNORMAL;
            } else if (event == INI_ACK_ERROR) {
                ini_proc->ini_status = INI_ABNORMAL;
            } else if (event == INI_ACK_FULL) {
                if (ini_proc->f2nf_back == ini_proc->f2nf_update) {
                    ini_proc->ini_status = INI_WAIT_F2NF;
                    ini_proc->wait_f2nf_start_time = curr_time;
                } else {
                    ini_proc->f2nf_back = ini_proc->f2nf_update;
                    ini_try_flag = 1;
                    ini_proc->ini_status = INI_ENQUE_BUSY;
                }
            } else if (event == INI_ACK_NORMAL) {
                que_mbuf_node_del_head(ini_proc->devid, ini_proc->qid, &ini_proc->mbuf_list);
                ini_proc->f2nf_back = ini_proc->f2nf_update;
                ini_try_flag = 1;
                ini_proc->ini_status = INI_ENQUE_BUSY;
            } else if (event == INI_RECV_F2NF) {
                ini_proc->f2nf_update++;
            }
            break;
        case INI_WAIT_F2NF:
            if ((event == INI_RECV_F2NF) || ((event == INI_ENQUE_TRY) && que_wait_f2nf_timeout(curr_time, ini_proc))) {
                ini_try_flag = 1;
                ini_proc->ini_status = INI_ENQUE_BUSY;
            }
            break;
        default:
            break;
    }
    (void)pthread_rwlock_unlock(&ini_proc->ini_status_lock);
    return ini_try_flag;
}

bool que_chan_update_ini_status(unsigned int devid, unsigned int qid, ASYNC_QUE_INI_EVENT event)
{
    struct que_chan *chan = NULL;
    struct que_ini_proc *ini_proc = NULL;
    bool ini_try_flag = 0;

    chan = que_chan_get(devid, qid);
    if (que_unlikely(chan == NULL)) {
        QUEUE_LOG_ERR("que chan is not init. (qid=%u)\n", qid);
        return ini_try_flag;
    }

    ini_proc = chan->ini_proc[ASYNC_ENQUE];
    ini_try_flag = que_update_status(ini_proc, event);
    que_chan_put(chan);
    return ini_try_flag;
}

static int que_inter_dev_ini_proc(unsigned int devid, unsigned int qid, struct que_ini_proc *ini_proc)
{
    int ret;
    ret = que_get_iovector(ini_proc);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }

    ret = que_inter_dev_ini_init(devid, qid, ini_proc);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        goto free_iovector;
    }

    ret = que_ini_pkt_send(ini_proc, ini_proc->vector);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ini pkt send fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
    }

    que_inter_dev_ini_uninit(ini_proc);
free_iovector:
    que_put_iovector(ini_proc);
    return ret;
}

int que_chan_inter_dev_ini_proc(unsigned int devid, unsigned int qid)
{
    int ret;
    struct que_chan *chan = NULL;
    struct que_ini_proc *ini_proc = NULL;

    chan = que_chan_get(devid, qid);
    if (que_unlikely(chan == NULL)) {
        QUEUE_LOG_ERR("que chan is not init. (qid=%u)\n", qid);
        return DRV_ERROR_QUEUE_NOT_CREATED;
    }

    ini_proc = chan->ini_proc[ASYNC_ENQUE];
    ret = que_inter_dev_ini_proc(devid, qid, ini_proc);
    que_chan_put(chan);
    return ret;
}

int que_chan_inter_dev_clear_mbuf(unsigned int devid, unsigned int qid)
{
    struct que_chan *chan = NULL;
    struct que_ini_proc *ini_proc = NULL;

    chan = que_chan_get(devid, qid);
    if (que_unlikely(chan == NULL)) {
        QUEUE_LOG_ERR("que chan is not init. (qid=%u)\n", qid);
        return DRV_ERROR_QUEUE_NOT_CREATED;
    }

    ini_proc = chan->ini_proc[ASYNC_ENQUE];
    if (ini_proc != NULL) {
        que_mbuf_node_clear(devid, qid, &ini_proc->mbuf_list);
    }

    que_chan_put(chan);
    return DRV_ERROR_NONE;
}

void que_chan_cnt_info(unsigned int devid, unsigned int qid)
{
    struct que_chan *chan = NULL;
    struct que_ini_proc *ini_proc = NULL;
    struct que_tgt_proc *tgt_proc = NULL;
    int i;

    chan = que_chan_get(devid, qid);
    if (que_unlikely(chan == NULL)) {
        QUEUE_LOG_ERR("que chan is not init. (qid=%u)\n", qid);
        return;
    }

    for (i = H2D_SYNC_ENQUE; i < QUEUE_ENQUE_BUTT; i++) {
        ini_proc = chan->ini_proc[i];
        QUEUE_RUN_LOG_INFO("que chan ini send_succ_cnt=%u, send_fail_cnt=%u, wait_succ_cnt=%u, wait_fail_cnt=%u. "
                           "que_type=%d. (qid=%u, devid=%u)\n",
                           ini_proc->cnt[INI_SEND_SUCCESS], ini_proc->cnt[INI_SEND_FAIL],
                           ini_proc->cnt[INI_WAIT_SUCCESS], ini_proc->cnt[INI_WAIT_FAIL], i, qid, devid);
    }

    tgt_proc = chan->tgt_proc;
    QUEUE_RUN_LOG_INFO("que chan tgt recv_succ_cnt=%u, recv_fail_cnt=%u, send_ack_succ_cnt=%u, send_ack_fail_cnt=%u. "
                       "(qid=%u, devid=%u)\n",
                       tgt_proc->cnt[TGT_RECV_SUCCESS], tgt_proc->cnt[TGT_RECV_FAIL],
                       tgt_proc->cnt[TGT_SEND_ACK_SUCCESS], tgt_proc->cnt[TGT_SEND_ACK_FAIL], qid, devid);

    que_chan_put(chan);
}

static int __attribute__((constructor)) que_comm_chan_init(void)
{
    struct que_chan_ctx_agent_list *list = que_get_chan_ctx_agent();
    list->que_chan_cnt_info_print = que_chan_cnt_info;
    return DRV_ERROR_NONE;
}
#else
void que_comm_chan_emu_test(void)
{}
#endif
