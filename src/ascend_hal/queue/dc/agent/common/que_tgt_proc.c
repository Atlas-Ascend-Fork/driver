/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <unistd.h>

#include "securec.h"
#include "ascend_hal.h"
#include "ascend_hal_error.h"
#include "drv_buff_mbuf.h"
#include "svm_user_interface.h"
#ifndef EMU_ST
#include "svm_user_interface.h"
#endif
#include "drv_buff_unibuff.h"
#include "queue.h"
#include "que_ub_msg.h"
#include "que_uma.h"
#include "que_compiler.h"
#include "queue_interface.h"
#include "que_comm_agent.h"
#include "queue_agent.h"
#include "que_jetty.h"
#include "que_tgt_proc.h"
#include "que_platform_ub.h"

#ifndef EMU_ST
static int g_tgt_level = 0;
static uint64_t tgt_base_time[MAX_DEVICE] = {0};
int que_get_tgt_log_level(void)
{
    return g_tgt_level;
}

uint64_t que_get_tgt_basetime(unsigned int devid)
{
    return tgt_base_time[devid];
}

void que_update_tgt_basetime(unsigned int devid)
{
    uint64_t cur_time = que_get_cur_time_ns();
    tgt_base_time[devid] = cur_time;
}

void que_tgt_time_stamp(struct que_tgt_proc *tgt_proc, QUE_TRACE_TGT_TIMESTAMP type)
{
    int tgt_log_level = que_get_tgt_log_level();
    unsigned int num = TRACE_TGT_LEVLE0_BUTT + tgt_proc->total_iovec_num *
                                                   (TRACE_TGT_LEVLE1_BUTT - TRACE_TGT_LEVLE1_START) *
                                                   (unsigned int)tgt_log_level;
    if ((unsigned int)type >= num) {
        return;
    }

    if (tgt_proc->timestamp == NULL) {
        return;
    }
    tgt_proc->timestamp[type] = que_get_cur_time_ns();
}

static struct que_tgt_proc *_que_tgt_proc_create(struct que_tgt_proc_attr *attr)
{
    struct que_tgt_proc *tgt_proc = NULL;

    tgt_proc = (struct que_tgt_proc *)calloc(1, sizeof(struct que_tgt_proc));
    if (que_unlikely(tgt_proc == NULL)) {
        QUEUE_LOG_ERR("que tgt proc malloc fail. (size=%ld; devid=%u; qid=%u)\n", sizeof(struct que_tgt_proc),
                      attr->devid, attr->qid);
        return NULL;
    }

    return tgt_proc;
}

static void _que_tgt_proc_destroy(struct que_tgt_proc *tgt_proc)
{
    if (que_likely(tgt_proc != NULL)) {
        free(tgt_proc);
    }
}

struct que_tgt_proc *que_tgt_proc_create(struct que_tgt_proc_attr *attr)
{
    struct que_tgt_proc *tgt_proc = NULL;

    tgt_proc = _que_tgt_proc_create(attr);
    if (que_unlikely(tgt_proc == NULL)) {
        QUEUE_LOG_ERR("que tgt proc create fail. (devid=%u; qid=%u)\n", attr->devid, attr->qid);
        return NULL;
    }

    return tgt_proc;
}

void que_tgt_proc_destroy(struct que_tgt_proc *tgt_proc)
{
    _que_tgt_proc_destroy(tgt_proc);
}

int que_rx_send_ack_and_wait(struct que_tgt_proc *tgt_proc, unsigned long long imm_data,
                             struct que_ack_jfs *ack_send_jfs, unsigned int d2d_flag)
{
    int ret;
    struct que_uma_wait_attr wait_attr;
    urma_cr_t cr = {0};

    wait_attr.jfce = ack_send_jfs->jfce_s;
    wait_attr.jfc = ack_send_jfs->jfc_s;
    wait_attr.num = 1;
    wait_attr.crs = &cr;
    ret = que_uma_send_ack(ack_send_jfs, imm_data);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que urma rw fail. (ret=%d; devid=%u; qid=%u)\n", ret, tgt_proc->devid, tgt_proc->qid);
        return ret;
    }

    ret = que_uma_wait(&wait_attr, QUE_UMA_WAIT_TIME);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("send wait fail. (ret=%d)\n", ret);
        return ret;
    }

    if ((cr.status == URMA_CR_ACK_TIMEOUT_ERR) || (cr.status == URMA_CR_REM_DATA_POISON)) {
        ret = que_recreate_jfs(ack_send_jfs->attr.jfs_depth, ack_send_jfs->jfc_s, ack_send_jfs->devid,
                               ack_send_jfs->jfs, d2d_flag);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que recreate send ack jfs fail. (ret=%d)\n", ret);
            return ret;
        }

        ret = que_uma_send_ack(ack_send_jfs, imm_data);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que urma rw fail again. (ret=%d; devid=%u; qid=%u)\n", ret, tgt_proc->devid, tgt_proc->qid);
            return ret;
        }

        cr.status = URMA_CR_SUCCESS;
        ret = que_uma_wait(&wait_attr, QUE_UMA_WAIT_TIME);
        if (que_unlikely((ret != DRV_ERROR_NONE) || (cr.status != URMA_CR_SUCCESS))) {
            QUEUE_LOG_ERR("send wait fail again. (ret=%d, status=%d)\n", ret, cr.status);
            return (ret == DRV_ERROR_NONE) ? DRV_ERROR_INNER_ERR : ret;
        }
    } else if (cr.status != URMA_CR_SUCCESS) {
        que_print_urma_ctx_info(ack_send_jfs->devid, *(ack_send_jfs->jfs), NULL, ack_send_jfs->jfc_s);
        QUEUE_LOG_ERR("cr status err. (status=%d)\n", cr.status);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

static struct que_rx *_que_rx_create(struct que_pkt *pkt)
{
    struct que_rx *rx = NULL;

    rx = calloc(1, sizeof(struct que_rx));
    if (que_unlikely(rx == NULL)) {
        return NULL;
    }

    rx->mem_type = pkt->head.mem_type;
    rx->que_type = pkt->head.que_type;
    rx->total_iovec_size = pkt->head.total_iovec_size;
    rx->total_copy_size = pkt->head.total_iovec_size;
    rx->total_iovec_num = pkt->head.total_iovec_num;
    rx->first_iovec_num = pkt->head.first_iovec_num;
    rx->remain_iovec_num = pkt->head.remain_iovec_num;
    return rx;
}

static void _que_rx_destroy(struct que_rx *rx)
{
    free(rx);
}

int que_mbuf_ctx_init(struct que_tgt_proc *tgt_proc, struct que_rx *rx, struct que_pkt *pkt)
{
    urma_target_seg_t *tseg = NULL;
    unsigned int size;
    unsigned int devid, qid;
    devid = tgt_proc->devid;
    qid = tgt_proc->qid;
    unsigned int access = ENQUE_TGT_ACCESS;
    void *addr = NULL;
    int ret;

    (void)pkt;

    ret = halMbufGetPrivInfo(rx->rx_mbuf.mbuf, &addr, &size);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("mbuf get priv info fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        return ret;
    }

    que_tgt_time_stamp(tgt_proc, TRACE_TGT_CTX_SEG_CREATE_START);
    if (que_is_share_mem((unsigned long long)(uintptr_t)addr) == false) {
        tseg = que_pin_seg_create(devid, (unsigned long long)(uintptr_t)addr, size, access, NULL, tgt_proc->d2d_flag);
    } else {
        tseg = que_get_urma_ctx_tseg(devid, tgt_proc->d2d_flag);
    }

    if (que_unlikely(tseg == NULL)) {
        QUEUE_LOG_ERR("que create seg fail. (devid=%u; qid=%u; ctx_addr=0x%llx; ctx_len=%u)\n", devid, qid,
                      (unsigned long long)(uintptr_t)addr, size);
        return DRV_ERROR_INNER_ERR;
    }
    que_tgt_time_stamp(tgt_proc, TRACE_TGT_CTX_SEG_CREATE_END);

    rx->rx_mbuf.ctx_aligned_tseg = tseg;

    rx->rx_mbuf.mbuf_ctx_va = addr;
    rx->rx_mbuf.mbuf_ctx_size = (size_t)pkt->head.total_iovec_size;

    return DRV_ERROR_NONE;
}

void que_mbuf_ctx_uninit(struct que_rx *rx)
{
    unsigned long long addr = (unsigned long long)(uintptr_t)rx->rx_mbuf.mbuf_ctx_va;
    if (que_is_share_mem(addr) == false) {
        que_seg_destroy(rx->rx_mbuf.ctx_aligned_tseg);
        rx->rx_mbuf.ctx_aligned_tseg = NULL;
    }

    rx->rx_mbuf.mbuf_ctx_va = NULL;
    rx->rx_mbuf.mbuf_ctx_size = 0;
}

int que_mbuf_ctx_wr_fill(struct que_tgt_proc *tgt_proc, struct que_pkt *pkt)
{
    struct que_rx *rx = tgt_proc->rx;
    struct que_node *ctx_node = &pkt->head.ctx_node;
    urma_target_seg_t *remote_ctx_tseg = NULL;
    struct que_urma_addr local, remote;
    size_t size;
    int ret;

    unsigned int access = (rx->que_type == H2D_SYNC_DEQUE) ? DEQUE_INI_ACCESS : ENQUE_INI_ACCESS;

    if (ctx_node->va == 0) {
        /* If ctx addr is 0, do not need to copy */
        return DRV_ERROR_NONE;
    }

    que_tgt_time_stamp(tgt_proc, TRACE_MBUF_SEG_IMPORT_START);
    remote_ctx_tseg = que_seg_import(tgt_proc->devid, tgt_proc->d2d_flag, &ctx_node->seg, access, &pkt->head.token);
    if (que_unlikely(remote_ctx_tseg == NULL)) {
        QUEUE_LOG_ERR("que seg import fail. (devid=%u; qid=%u)\n", tgt_proc->devid, tgt_proc->qid);
        return DRV_ERROR_INNER_ERR;
    }
    que_tgt_time_stamp(tgt_proc, TRACE_MBUF_SEG_IMPORT_END);

    /* Opcode is read. read from src to dst */
    local.va = (unsigned long long)(uintptr_t)rx->rx_mbuf.mbuf_ctx_va;
    local.tseg = rx->rx_mbuf.ctx_aligned_tseg;
    remote.va = ctx_node->va;
    remote.tseg = remote_ctx_tseg;
    size = que_min((size_t)rx->rx_mbuf.mbuf_ctx_size, ctx_node->size);

    struct que_jfs_rw_wr_data rw_data = {.remote = remote, .local = local, .size = size};
    rw_data.opcode = (rx->que_type == H2D_SYNC_DEQUE) ? URMA_OPC_WRITE : URMA_OPC_READ;
    ret = que_jfs_rw_wr_fill(tgt_proc->data_read_jetty, tgt_proc->rw_wr, &rw_data, true);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    rx->remote_ctx_tseg = remote_ctx_tseg;
    return DRV_ERROR_NONE;
}

int que_mbuf_ctx_read_post(struct que_tgt_proc *tgt_proc)
{
    struct que_rx *rx = tgt_proc->rx;
    int ret = DRV_ERROR_NONE;

    if (rx->remote_ctx_tseg != NULL) {
        que_seg_unimport(rx->remote_ctx_tseg);
        rx->remote_ctx_tseg = NULL;
    }
    return ret;
}

static int que_local_pkt_create(struct que_tgt_proc *tgt_proc, struct que_rx *rx, struct que_pkt *pkt)
{
    unsigned long long remain_pkt_size;
    void *remain_pkt_base = NULL;
    int ret;
    urma_target_seg_t *local_pkt_tseg = NULL;
    unsigned int access = URMA_ACCESS_LOCAL_ONLY;

    if (pkt->head.remain_iovec_num == 0) {
        return DRV_ERROR_NONE;
    }

    remain_pkt_size = _que_get_pkt_size(pkt->head.remain_iovec_num);
    ret = que_mem_alloc((void **)(&remain_pkt_base), remain_pkt_size);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que mem alloc fail. (qid=%u)\n", pkt->head.qid);
        return DRV_ERROR_QUEUE_INNER_ERROR;
    }

    que_tgt_time_stamp(tgt_proc, TRACE_REMAIN_PKT_ALLOC);
    if (que_is_share_mem((unsigned long long)(uintptr_t)remain_pkt_base) == false) {
        local_pkt_tseg = que_pin_seg_create(tgt_proc->devid, (unsigned long long)(uintptr_t)remain_pkt_base,
                                            remain_pkt_size, access, NULL, tgt_proc->d2d_flag);
    } else {
        local_pkt_tseg = que_get_urma_ctx_tseg(tgt_proc->devid, tgt_proc->d2d_flag);
    }
    if (que_unlikely(local_pkt_tseg == NULL)) {
        QUEUE_LOG_ERR("buf memory seg create fail. (devid=%u; qid=%u)\n", tgt_proc->devid, pkt->head.qid);
        que_mem_free(remain_pkt_base);
        return DRV_ERROR_QUEUE_INNER_ERROR;
    }
    que_tgt_time_stamp(tgt_proc, TRACE_TGT_PKT_SEG_CREATE);

    rx->local_pkt_tseg = local_pkt_tseg;
    rx->remain_pkt_base = (unsigned long long)(uintptr_t)remain_pkt_base;
    rx->remain_pkt_size = remain_pkt_size;
    return DRV_ERROR_NONE;
}

static void que_local_pkt_destroy(struct que_rx *rx)
{
    if (que_likely(rx->local_pkt_tseg != NULL)) {
        if (que_is_share_mem(rx->remain_pkt_base) == false) {
            que_seg_destroy(rx->local_pkt_tseg);
            rx->local_pkt_tseg = NULL;
        }
    }

    if (que_likely(rx->remain_pkt_base != 0)) {
        que_mem_free((void *)rx->remain_pkt_base);
        rx->remain_pkt_base = 0;
    }
}

static inline bool que_rx_mbuf_is_bare(struct que_rx *rx, struct que_pkt *pkt)
{
    return ((rx->mem_type == MEM_DEVICE_SVM) && (pkt->head.total_iovec_num == 1)); /* 1 iovec num */
}

static int _que_mbuf_create(unsigned int devid, unsigned int qid, struct que_rx *rx, struct que_pkt *pkt)
{
    unsigned long long mbuf_size;
    unsigned int _devid = que_get_chan_devid(devid);
    Mbuf *mbuf = NULL;
    int ret;

    if (rx->que_type == H2D_SYNC_DEQUE) {
        ret = queue_front(_devid, qid, &mbuf);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_WARN("halQueueFront not successful. (ret=%d; devid=%u; qid=%u)\n", ret, _devid, qid);
            return ret;
        }
        rx->total_copy_size = que_min(mbuf->data_len, rx->total_iovec_size);
    } else {
        if (que_rx_mbuf_is_bare(rx, pkt) == true) {
            mbuf_size = pkt->iovec_node[0].size;
            ret = (int)mbuf_build_bare_buff((void *)pkt->iovec_node[0].va, (uint64_t)mbuf_size, &mbuf);
        } else {
            mbuf_size = que_align_up(pkt->head.total_iovec_size, (size_t)getpagesize());
            ret = (int)hal_mbuf_alloc_align(mbuf_size, QUE_URMA_BUFF_ALIGN_VALUE, &mbuf);
        }
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("alloc mbuf fail. (ret=%d; devid=%u; qid=%u; mbuf_size=%llu)\n", ret, _devid, qid, mbuf_size);
            return ret;
        }
        (void)halMbufSetDataLen(mbuf, mbuf_size);
    }
    rx->rx_mbuf.mbuf = mbuf;

    return DRV_ERROR_NONE;
}

static void _que_mbuf_destroy(struct que_rx *rx)
{
    if (rx->que_type == H2D_SYNC_DEQUE) {
        if (que_likely(rx->rx_mbuf.mbuf != NULL)) {
            queue_un_front(rx->rx_mbuf.mbuf);
            rx->rx_mbuf.mbuf = NULL;
        }
    } else {
        if (que_likely(rx->rx_mbuf.mbuf != NULL)) {
            (void)halMbufFree(rx->rx_mbuf.mbuf);
            rx->rx_mbuf.mbuf = NULL;
        }
    }
}

static int _que_mbuf_data_init(unsigned int devid, unsigned int qid, struct que_rx *rx, struct que_pkt *pkt,
                               unsigned int d2d_flag)
{
    urma_target_seg_t *tseg = NULL;
    Mbuf *mbuf = rx->rx_mbuf.mbuf;
    unsigned int access = ENQUE_TGT_ACCESS;

    (void)pkt;

    if (rx->mem_type != MEM_DEVICE_SVM) {
        if (que_is_share_mem((unsigned long long)(uintptr_t)mbuf->data) == false) {
            if ((rx->que_type == H2D_SYNC_DEQUE) && (mbuf->buff_type == MBUF_BARE_BUFF)) {
                tseg = que_nonpin_seg_create(devid, (unsigned long long)(uintptr_t)mbuf->data, mbuf->data_len, access,
                                             NULL, d2d_flag);
            } else {
                tseg = que_pin_seg_create(devid, (unsigned long long)(uintptr_t)mbuf->data, mbuf->data_len, access,
                                          NULL, d2d_flag);
            }
        } else {
            tseg = que_get_urma_ctx_tseg(devid, d2d_flag);
        }
        if (que_unlikely(tseg == NULL)) {
            QUEUE_LOG_ERR(
                "mbuf data seg reg fail. (devid=%u; qid=%u; len=%llu; memtype=%d; quetype=%d; bufftype=%lu)\n", devid,
                qid, (unsigned long long)mbuf->data_len, rx->mem_type, rx->que_type, mbuf->buff_type);
            return DRV_ERROR_QUEUE_INNER_ERROR;
        }
    }

    rx->rx_mbuf.data_tseg = tseg;
    rx->rx_mbuf.data_va = (unsigned long long)(uintptr_t)mbuf->data;
    rx->rx_mbuf.data_size = mbuf->data_len;

    return DRV_ERROR_NONE;
}

static void _que_mbuf_data_uninit(struct que_rx *rx)
{
    if (rx->rx_mbuf.data_tseg != NULL) {
        if (que_is_share_mem(rx->rx_mbuf.data_va) == false) {
            que_seg_destroy(rx->rx_mbuf.data_tseg);
            rx->rx_mbuf.data_tseg = NULL;
        }
    }
    rx->rx_mbuf.data_va = 0;
    rx->rx_mbuf.data_size = 0;
}

static int que_mbuf_init(struct que_tgt_proc *tgt_proc, struct que_rx *rx, struct que_pkt *pkt)
{
    int ret;
    ret = _que_mbuf_create(tgt_proc->devid, tgt_proc->qid, rx, pkt);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }
    que_tgt_time_stamp(tgt_proc, TRACE_TGT_MBUF_CREATE);
    ret = que_mbuf_ctx_init(tgt_proc, rx, pkt);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        _que_mbuf_destroy(rx);
        return ret;
    }

    ret = _que_mbuf_data_init(tgt_proc->devid, tgt_proc->qid, rx, pkt, tgt_proc->d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        que_mbuf_ctx_uninit(rx);
        _que_mbuf_destroy(rx);
        return ret;
    }
    que_tgt_time_stamp(tgt_proc, TRACE_DATA_INIT);

    return DRV_ERROR_NONE;
}

static void que_mbuf_post_proc(struct que_rx *rx)
{
    _que_mbuf_data_uninit(rx);
    que_mbuf_ctx_uninit(rx);
    rx->rx_mbuf.mbuf = NULL; /* Do not need to free Mbuf */
}

static void que_mbuf_uninit(struct que_rx *rx)
{
    _que_mbuf_data_uninit(rx);
    que_mbuf_ctx_uninit(rx);
    _que_mbuf_destroy(rx);
}

static inline bool que_rx_is_finish(struct que_rx *rx)
{
    return rx->copied_iovec_num == rx->total_iovec_num;
}

static int que_rx_pkt_head_check(unsigned int devid, unsigned int qid, struct que_pkt *pkt)
{
    /* In the interface, the vector->count type is Unsigned int, with a maximum value of QUEUE_MAX_IOVEC_NUM. */
    if (que_unlikely((pkt->head.total_iovec_num == 0) ||
                     (pkt->head.total_iovec_num != (pkt->head.first_iovec_num + pkt->head.remain_iovec_num)))) {
        QUEUE_LOG_ERR(
            "invalid iovec num. (devid=%u; qid=%u; total_iovec_num=%u; first_iovec_num=%u; remain_iovec_num=%u)\n",
            devid, qid, pkt->head.total_iovec_num, pkt->head.first_iovec_num, pkt->head.remain_iovec_num);
        return DRV_ERROR_PARA_ERROR;
    }

    if (que_unlikely((pkt->head.first_iovec_num == 0) || (pkt->head.first_iovec_num != pkt->iovec_node_num) ||
                     (pkt->head.first_iovec_num > _que_get_node_num()))) {
        QUEUE_LOG_ERR("invalid pkt num. (devid=%u; qid=%u; pkt_num=%u)\n", devid, qid, pkt->head.first_iovec_num);
        return DRV_ERROR_PARA_ERROR;
    }

    unsigned int max_wr_num = QUE_MAX_RW_WR_NUM;
    unsigned long long first_pkt_size = _que_get_pkt_size(pkt->head.first_iovec_num);
    unsigned long long wr_num = que_align_up(first_pkt_size, QUE_URMA_MAX_SIZE) / QUE_URMA_MAX_SIZE;

    wr_num += (pkt->head.ctx_node.va != 0);
    for (unsigned int i = 0; i < pkt->iovec_node_num; i++) {
        wr_num += que_align_up(pkt->iovec_node[i].size, QUE_URMA_MAX_SIZE) / QUE_URMA_MAX_SIZE;
    }
    if (que_unlikely(wr_num > max_wr_num)) {
        QUEUE_LOG_ERR("que pkt read size over limit. (devid=%u; qid=%u; max_wr_num=%u; wr_num=%llu)\n", devid, qid,
                      max_wr_num, wr_num);
        return DRV_ERROR_PARA_ERROR;
    }
    return DRV_ERROR_NONE;
}

static int que_rx_remain_pkt_check(unsigned int devid, unsigned int qid, struct que_pkt *pkt, struct que_rx *rx)
{
    if (que_unlikely(pkt->head.total_iovec_num != rx->total_iovec_num)) {
        QUEUE_LOG_ERR("invalid pkt head. (devid=%u; qid=%u; total_iovec_num=%u; exp_total_iovec_num=%u)\n", devid, qid,
                      pkt->head.total_iovec_num, rx->total_iovec_num);
        return DRV_ERROR_PARA_ERROR;
    }

    if (que_unlikely(pkt->head.total_iovec_size != rx->total_iovec_size)) {
        QUEUE_LOG_ERR("invalid pkt head. (devid=%u; qid=%u; total_iovec_size=%llu; exp_total_iovec_size=%llu)\n", devid,
                      qid, pkt->head.total_iovec_size, rx->total_iovec_size);
        return DRV_ERROR_PARA_ERROR;
    }

    if (que_unlikely((pkt->head.mem_type != rx->mem_type) || (pkt->head.que_type != rx->que_type))) {
        QUEUE_LOG_ERR("invalid pkt head. (qid=%u; mem_type=%d; exp_mem_type=%d; que_type=%d; exp_que_type=%d)\n", qid,
                      pkt->head.mem_type, rx->mem_type, pkt->head.que_type, rx->que_type);
        return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

static int que_rx_init(struct que_tgt_proc *tgt_proc, struct que_pkt *pkt)
{
    struct que_rx *rx = NULL;
    int ret;

    if (tgt_proc->rx != NULL) {
        /* when the ini node starts the next proc, the previous proc at the tgt node may not have completed yet. */
        ATOMIC_INC((volatile int *)&tgt_proc->cnt[TGT_RECV_FAIL]);
        return DRV_ERROR_QUEUE_INNER_ERROR;
    }

    ret = que_rx_pkt_head_check(tgt_proc->devid, tgt_proc->qid, pkt);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }

    rx = _que_rx_create(pkt);
    if (que_unlikely(rx == NULL)) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = que_local_pkt_create(tgt_proc, rx, pkt);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        _que_rx_destroy(rx);
        return ret;
    }

    ret = que_mbuf_init(tgt_proc, rx, pkt);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        que_local_pkt_destroy(rx);
        _que_rx_destroy(rx);
        return ret;
    }

    tgt_proc->rx = rx;
    return DRV_ERROR_NONE;
}

static void que_rx_uninit(struct que_tgt_proc *tgt_proc)
{
    que_local_pkt_destroy(tgt_proc->rx);
    _que_rx_destroy(tgt_proc->rx);
    tgt_proc->rx = NULL;
}

static int que_tgt_post_proc(struct que_tgt_proc *tgt_proc)
{
    int ret = DRV_ERROR_INNER_ERR;
    unsigned int devid = que_get_chan_devid(tgt_proc->devid);
    struct que_rx *rx = tgt_proc->rx;
    unsigned int virtual_qid = queue_get_virtual_qid(tgt_proc->qid, LOCAL_QUEUE);

    if ((queue_is_inter_dev(devid, tgt_proc->qid)) && (rx->que_type != ASYNC_ENQUE)) {
        QUEUE_LOG_ERR("inter dev que not support other type. (devid=%u; qid=%u; que_type=%d)\n", devid, tgt_proc->qid,
                      rx->que_type);
        return DRV_ERROR_NOT_SUPPORT;
    }
    if (rx->que_type == H2D_SYNC_DEQUE) {
        que_mbuf_uninit(rx);
        if (que_rx_is_finish(rx) == true) {
            ret = halQueueDeQueue(devid, virtual_qid, (void **)&rx->rx_mbuf.mbuf);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
            ret = (drvError_t)halMbufFree(rx->rx_mbuf.mbuf);
            if (ret != DRV_ERROR_NONE) {
                QUEUE_LOG_ERR("halMbufFree failed. (ret=%d)\n", ret);
            }
            return ret;
        } else {
            QUEUE_LOG_ERR("que proc not finish. (qid=%u; que_type=%d; total_iovec_num=%u; copied_iovec_num=%u)\n",
                          tgt_proc->qid, rx->que_type, rx->total_iovec_num, rx->copied_iovec_num);
        }
    } else {
        if (que_rx_is_finish(rx) == true) {
            ret = queue_enqueue_local(devid, tgt_proc->qid, rx->rx_mbuf.mbuf);
            if (ret == DRV_ERROR_NONE) {
                que_mbuf_post_proc(rx);
                return DRV_ERROR_NONE;
            }
        } else {
            QUEUE_LOG_ERR("que proc not finish. (qid=%u; que_type=%d; total_iovec_num=%u; copied_iovec_num=%u)\n",
                          tgt_proc->qid, rx->que_type, rx->total_iovec_num, rx->copied_iovec_num);
        }
        que_mbuf_uninit(rx);
    }
    return ret;
}

static int que_data_wr_fill(struct que_tgt_proc *tgt_proc, struct que_node *node, unsigned int iovec_index,
                            urma_token_t *token)
{
    struct que_rx *rx = tgt_proc->rx;
    unsigned long long remain_size = rx->total_copy_size - rx->copied_iovec_size;
    struct que_urma_addr local, remote;
    urma_target_seg_t *tseg = NULL;
    unsigned long long copy_size, wr_num;
    unsigned int access = (rx->que_type == H2D_SYNC_DEQUE) ? DEQUE_INI_ACCESS : ENQUE_INI_ACCESS;
    int ret;

    if (remain_size == 0) {
        return DRV_ERROR_NONE;
    }

    copy_size = que_min(node->size, remain_size);
    wr_num = que_align_up(copy_size, QUE_URMA_MAX_SIZE) / QUE_URMA_MAX_SIZE;
    if ((wr_num + tgt_proc->rw_wr->cur_wr_idx) > tgt_proc->rw_wr->max_wr_num) {
        QUEUE_LOG_ERR("que wr num is over limit. (devid=%u; qid=%u; wr num=%llu; max_wr_num=%u; cur_wr_idx=%u)\n",
                      tgt_proc->devid, tgt_proc->qid, wr_num, tgt_proc->rw_wr->max_wr_num, tgt_proc->rw_wr->cur_wr_idx);
        return DRV_ERROR_OVER_LIMIT;
    }

    que_tgt_time_stamp(tgt_proc,
                       TRACE_IOVEC_SEG_IMPORT_START + iovec_index * (TRACE_TGT_LEVLE1_BUTT - TRACE_TGT_LEVLE1_START));
    tseg = que_seg_import(tgt_proc->devid, tgt_proc->d2d_flag, &node->seg, access, token);
    if (que_unlikely(tseg == NULL)) {
        QUEUE_LOG_ERR("buf memory seg import fail. (devid=%u; qid=%u)\n", tgt_proc->devid, tgt_proc->qid);
        return DRV_ERROR_QUEUE_INNER_ERROR;
    }
    que_tgt_time_stamp(tgt_proc,
                       TRACE_IOVEC_SEG_IMPORT_END + iovec_index * (TRACE_TGT_LEVLE1_BUTT - TRACE_TGT_LEVLE1_START));

    local.va = rx->rx_mbuf.data_va + rx->copied_iovec_size;
    local.tseg = rx->rx_mbuf.data_tseg;
    remote.va = node->va;
    remote.tseg = tseg;

    struct que_jfs_rw_wr_data rw_data = {.remote = remote, .local = local, .size = copy_size};
    rw_data.opcode = (rx->que_type == H2D_SYNC_DEQUE) ? URMA_OPC_WRITE : URMA_OPC_READ;
    ret = que_jfs_rw_wr_fill(tgt_proc->data_read_jetty, tgt_proc->rw_wr, &rw_data, false);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (rx->iovec_idx_in_wr_link >= QUE_MAX_RW_WR_NUM) {
        QUEUE_LOG_ERR("que iovec idx access tseg out of bounds. (devid=%u; qid=%u; iovec_idx_in_wr_link=%u)\n",
                      tgt_proc->devid, tgt_proc->qid, rx->iovec_idx_in_wr_link);
        return DRV_ERROR_OVER_LIMIT;
    }
    rx->tseg[rx->iovec_idx_in_wr_link] = tseg;
    rx->iovec_idx_in_wr_link++;

    rx->copied_iovec_size += copy_size;
    return DRV_ERROR_NONE;
}

static void que_data_read_post(struct que_tgt_proc *tgt_proc)
{
    struct que_rx *rx = tgt_proc->rx;
    for (unsigned int i = 0; i < rx->iovec_idx_in_wr_link; i++) {
        if (rx->tseg[i] != NULL) {
            que_seg_unimport(rx->tseg[i]);
            rx->tseg[i] = NULL;
        }
    }
    rx->iovec_idx_in_wr_link = 0;
}

static int que_rx_pkt_read_wr_fill(struct que_tgt_proc *tgt_proc, struct que_pkt *pkt)
{
    struct que_rx *rx = tgt_proc->rx;
    struct que_urma_addr local, remote;
    urma_target_seg_t *remote_pkt_tseg = NULL;
    unsigned int access = URMA_ACCESS_READ;
    int ret;

    if (rx->remain_iovec_num == 0) {
        return DRV_ERROR_NONE;
    }

    que_tgt_time_stamp(tgt_proc, TRACE_REMAIN_PKT_SEG_IMPORT_START);
    remote_pkt_tseg = que_seg_import(tgt_proc->devid, tgt_proc->d2d_flag, &pkt->head.pkt_node.seg, access,
                                     &pkt->head.token);
    if (que_unlikely(remote_pkt_tseg == NULL)) {
        QUEUE_LOG_ERR("que seg import fail. (devid=%u; qid=%u)\n", tgt_proc->devid, tgt_proc->qid);
        return DRV_ERROR_QUEUE_INNER_ERROR;
    }
    que_tgt_time_stamp(tgt_proc, TRACE_REMAIN_PKT_SEG_IMPORT_END);

    local.va = rx->remain_pkt_base;
    local.tseg = rx->local_pkt_tseg;
    remote.va = pkt->head.pkt_node.va + QUE_UMA_MAX_SEND_SIZE;
    remote.tseg = remote_pkt_tseg;

    struct que_jfs_rw_wr_data rw_data = {
        .remote = remote, .local = local, .size = rx->remain_pkt_size, .opcode = URMA_OPC_READ};
    ret = que_jfs_rw_wr_fill(tgt_proc->data_read_jetty, tgt_proc->rw_wr, &rw_data, true);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    rx->remote_pkt_tseg = remote_pkt_tseg;
    return DRV_ERROR_NONE;
}

static void que_rx_pkt_read_post(struct que_tgt_proc *tgt_proc)
{
    struct que_rx *rx = tgt_proc->rx;
    if (rx->remote_pkt_tseg != NULL) {
        que_seg_unimport(rx->remote_pkt_tseg);
        rx->remote_pkt_tseg = NULL;
    }
}

static int que_rx_pkt_buff_proc(struct que_tgt_proc *tgt_proc, struct que_pkt *pkt)
{
    int ret;
    unsigned int i;
    struct que_node *iovec_node = NULL;
    struct que_rx *rx = tgt_proc->rx;
    unsigned int cur_iovec_idx = 0;
    unsigned int index_delta = 0;

    if (rx->copied_iovec_num >= rx->first_iovec_num) {
        ret = que_rx_remain_pkt_check(tgt_proc->devid, tgt_proc->qid, pkt, rx);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que buff pkt check fail. (copied_iovec_num=%u; first_iovec_num=%u)\n", rx->copied_iovec_num,
                          rx->first_iovec_num);
            return ret;
        }

        rx->iovec_idx_in_wr_link = 0;
        tgt_proc->rw_wr->cur_wr_idx = 0;
        cur_iovec_idx = rx->copied_iovec_num - rx->first_iovec_num;
        index_delta = rx->first_iovec_num;
    }

    for (i = cur_iovec_idx; i < pkt->iovec_node_num; i++) {
        iovec_node = &pkt->iovec_node[i];
        ret = que_data_wr_fill(tgt_proc, iovec_node, (i + index_delta), &pkt->head.token);
        if (ret == DRV_ERROR_OVER_LIMIT) {
            return DRV_ERROR_NONE;
        }

        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que buff read fail. (ret=%d; devid=%u; qid=%u; i=%u)\n", ret, tgt_proc->devid, tgt_proc->qid,
                          i);
            que_data_read_post(tgt_proc);
            return ret;
        }
        rx->copied_iovec_num++;
    }
    return DRV_ERROR_NONE;
}

static int que_rx_pkt_bare_proc(struct que_tgt_proc *tgt_proc, struct que_pkt *pkt)
{
    return que_rx_pkt_bare_proc_platform(tgt_proc, pkt);
}

void que_tgt_timeout_print(struct que_tgt_proc *tgt_proc)
{
    uint64_t curr_delta;
    unsigned int iovec_idx, id_start, id_end;
    int tgt_log_level = que_get_tgt_log_level();
    uint64_t *timestamp = tgt_proc->timestamp;
    if (timestamp == NULL) {
        return;
    }

    curr_delta = timestamp[TRACE_TGT_ACK_END] - timestamp[TRACE_TGT_START];
    if ((curr_delta / NS_PER_SECOND) > QUE_TIMEOUT_SECOND) {
        QUEUE_RUN_LOG_INFO_FLOWCTRL(
            "que tgt proc timeout, que_type=%d, cost_time=%luns, "
            "start=%lu, import_jetty=%lu, chan_update=%lu, remain_pkt_alloc=%lu, pkt_seg_create=%lu, "
            "mbuf_create=%lu, ctx_seg_create_start=%lu, ctx_seg_create_end=%lu, data_init=%lu, "
            "remain_pkt_seg_import_start=%lu, "
            "remain_pkt_seg_import_end=%lu, mbuf_seg_import_start=%lu, mbuf_seg_import_end=%lu, first_pkt_proc=%lu, "
            "post_proc=%lu, ack_start=%lu, ack_end=%lu. (devid=%d, qid=%u)\n",
            tgt_proc->que_type, curr_delta, timestamp[TRACE_TGT_START], timestamp[TRACE_IMPORT_JETTY],
            timestamp[TRACE_CHAN_UPDATE], timestamp[TRACE_REMAIN_PKT_ALLOC], timestamp[TRACE_TGT_PKT_SEG_CREATE],
            timestamp[TRACE_TGT_MBUF_CREATE], timestamp[TRACE_TGT_CTX_SEG_CREATE_START],
            timestamp[TRACE_TGT_CTX_SEG_CREATE_END], timestamp[TRACE_DATA_INIT],
            timestamp[TRACE_REMAIN_PKT_SEG_IMPORT_START], timestamp[TRACE_REMAIN_PKT_SEG_IMPORT_END],
            timestamp[TRACE_MBUF_SEG_IMPORT_START], timestamp[TRACE_MBUF_SEG_IMPORT_END],
            timestamp[TRACE_FIRST_PKT_PROC], timestamp[TRACE_TGT_POST_PROC], timestamp[TRACE_TGT_ACK_START],
            timestamp[TRACE_TGT_ACK_END], tgt_proc->devid, tgt_proc->qid);
    }

    if (tgt_log_level != 0) {
        for (iovec_idx = 0; iovec_idx < tgt_proc->total_iovec_num; iovec_idx++) {
            id_start = TRACE_IOVEC_SEG_IMPORT_START + iovec_idx * (TRACE_TGT_LEVLE1_BUTT - TRACE_TGT_LEVLE1_START);
            id_end = TRACE_IOVEC_SEG_IMPORT_END + iovec_idx * (TRACE_TGT_LEVLE1_BUTT - TRACE_TGT_LEVLE1_START);
            QUEUE_RUN_LOG_INFO(
                "que tgt seg import timeout, que_type=%d, iovec_idx=%d, seg_create_start=%lu, seg_create_end=%lu. "
                "(devid=%d, qid=%u)\n",
                tgt_proc->que_type, iovec_idx, timestamp[id_start], timestamp[id_end], tgt_proc->devid, tgt_proc->qid);
        }
    }
}

static void que_tgt_proc_ack(struct que_tgt_proc *tgt_proc, int sn)
{
    que_ack_data ack_data;
    int ret;
    uint64_t delta_ns = 0;

    if (tgt_proc->timestamp[TRACE_TGT_ACK_START] > tgt_proc->timestamp[TRACE_TGT_START]) {
        delta_ns = tgt_proc->timestamp[TRACE_TGT_ACK_START] - tgt_proc->timestamp[TRACE_TGT_START];
    }
    ack_data.ack_msg.qid = QUE_ACK_PACK_Q16((tgt_proc->que_type != ASYNC_ENQUE) ? tgt_proc->qid : tgt_proc->peer_qid);
    ack_data.ack_msg.result = QUE_ACK_RESULT_PACK_16(tgt_proc->tgt_proc_result);
    ack_data.ack_msg.sn = QUE_ACK_PACK_SN(sn);
    ack_data.ack_msg.tgt_time = QUE_ACK_PACK_TGT_TIME_NS(delta_ns);

    ret = que_rx_send_ack_and_wait(tgt_proc, ack_data.imm_data, &tgt_proc->ack_send_jetty, tgt_proc->d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("send ack and wait fail. (devid=%u; qid=%u; ret=%d)\n", tgt_proc->devid, tgt_proc->qid, ret);
        ATOMIC_INC((volatile int *)&tgt_proc->cnt[TGT_SEND_ACK_FAIL]);
    } else {
        ATOMIC_INC((volatile int *)&tgt_proc->cnt[TGT_SEND_ACK_SUCCESS]);
    }
    QUEUE_LOG_DEBUG("que send ack . (que_type=%d; qid=%u; ret=%d; sn=%u)\n", tgt_proc->que_type,
                    (unsigned int)ack_data.ack_msg.qid, ack_data.ack_msg.result, sn);
}

static void que_tgt_last_wrlink_post_proc(struct que_tgt_proc *tgt_proc)
{
    que_data_read_post(tgt_proc);
    que_mbuf_ctx_read_post(tgt_proc);
    que_rx_pkt_read_post(tgt_proc);
    tgt_proc->rw_wr->cur_wr_idx = 0;
}

static int que_tgt_first_pkt_pre_proc(struct que_tgt_proc *tgt_proc, struct que_pkt *pkt)
{
    int ret;

    ret = que_rx_pkt_read_wr_fill(tgt_proc, pkt);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que pkt read wr fill fail. (ret=%d; devid=%u; qid=%u)\n", ret, tgt_proc->devid, tgt_proc->qid);
        return DRV_ERROR_INNER_ERR;
    }

    ret = que_mbuf_ctx_wr_fill(tgt_proc, pkt);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que mbuf ctx wr fill fail. (ret=%d; devid=%u; qid=%u)\n", ret, tgt_proc->devid, tgt_proc->qid);
        goto pkt_read_post;
    }

    if (tgt_proc->rx->mem_type == MEM_DEVICE_SVM) {
        ret = que_rx_pkt_bare_proc(tgt_proc, pkt);
    } else {
        ret = que_rx_pkt_buff_proc(tgt_proc, pkt);
    }
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que data rw wr fill fail. (ret=%d; devid=%u; qid=%u; mem_type=%d)\n", ret, tgt_proc->devid,
                      tgt_proc->qid, tgt_proc->rx->mem_type);
        goto ctx_read_post;
    }
    return DRV_ERROR_NONE;

ctx_read_post:
    que_mbuf_ctx_read_post(tgt_proc);
pkt_read_post:
    que_rx_pkt_read_post(tgt_proc);
    return DRV_ERROR_INNER_ERR;
}

static void que_tgt_first_pkt_post_proc(struct que_tgt_proc *tgt_proc)
{
    que_data_read_post(tgt_proc);
    que_mbuf_ctx_read_post(tgt_proc);
    que_rx_pkt_read_post(tgt_proc);
    tgt_proc->rw_wr->cur_wr_idx = 0;
}

void que_tgt_timestamp_destroy(struct que_tgt_proc *tgt_proc)
{
    if (que_likely(tgt_proc->timestamp != NULL)) {
        free(tgt_proc->timestamp);
        tgt_proc->timestamp = NULL;
    }
}

static void que_tgt_ack_proc(struct que_tgt_proc *tgt_proc, int ret, int sn)
{
    tgt_proc->tgt_proc_result = ret;
    que_tgt_time_stamp(tgt_proc, TRACE_TGT_ACK_START);
    que_tgt_proc_ack(tgt_proc, sn);
    que_tgt_time_stamp(tgt_proc, TRACE_TGT_ACK_END);
    que_tgt_timeout_print(tgt_proc);
    que_tgt_timestamp_destroy(tgt_proc);
    tgt_proc->is_finished = 1;
    ATOMIC_INC((volatile int *)&tgt_proc->cnt[TGT_RECV_SUCCESS]);
}

void que_tgt_pkt_proc_ex(struct que_tgt_proc *tgt_proc, int cr_status)
{
    int ret = DRV_ERROR_INNER_ERR;
    struct que_rx *rx = tgt_proc->rx;
    unsigned long long pkt_tmp;
    struct que_pkt *pkt = NULL;

    if (rx == NULL) {
        QUEUE_LOG_ERR("que rx init fail. (devid=%u; qid=%u; status=%d)\n", tgt_proc->devid, tgt_proc->qid, cr_status);
        goto ack;
    }

    if ((cr_status == URMA_CR_ACK_TIMEOUT_ERR) || (cr_status == URMA_CR_REM_DATA_POISON)) {
        ret = que_recreate_jfs(tgt_proc->data_read_jetty->attr.jfs_depth, tgt_proc->data_read_jetty->jfc_s,
                               tgt_proc->data_read_jetty->devid, &tgt_proc->data_read_jetty->jfs, tgt_proc->d2d_flag);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que recreate data read jfs fail. (ret=%d)\n", ret);
            goto ack;
        }
        ret = que_uma_rw_post_async(tgt_proc->usr_ctx_addr, tgt_proc->data_read_jetty, tgt_proc->rw_wr);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que rw post async fail. (ret=%d)\n", ret);
            goto ack;
        }
        return;
    }

    que_tgt_last_wrlink_post_proc(tgt_proc);
    if (cr_status != URMA_CR_SUCCESS) {
        QUEUE_LOG_ERR("que cqe error. (devid=%u; qid=%u; status=%d)\n", tgt_proc->devid, tgt_proc->qid, cr_status);
        goto mbuf_uninit;
    }

    if (rx->copied_iovec_num == rx->total_iovec_num) {
        goto enque;
    }

    pkt_tmp = (unsigned long long)(uintptr_t)rx->remain_pkt_base;
    pkt = (struct que_pkt *)pkt_tmp;
    if (rx->mem_type == MEM_DEVICE_SVM) {
        ret = que_rx_pkt_bare_proc(tgt_proc, pkt);
    } else {
        ret = que_rx_pkt_buff_proc(tgt_proc, pkt);
    }
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que buff read fail. (ret=%d; devid=%u; qid=%u; mem_type=%d)\n", ret, tgt_proc->devid,
                      tgt_proc->qid, tgt_proc->rx->mem_type);
        goto mbuf_uninit;
    }

    if (tgt_proc->rw_wr->cur_wr_idx > 0) {
        ret = que_uma_rw_post_async(tgt_proc->usr_ctx_addr, tgt_proc->data_read_jetty, tgt_proc->rw_wr);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            goto cur_pkt_post_proc;
        }
        return;
    }

enque:
    ret = que_tgt_post_proc(tgt_proc);
    goto rx_uninit;

cur_pkt_post_proc:
    que_data_read_post(tgt_proc);
mbuf_uninit:
    que_mbuf_uninit(tgt_proc->rx);
rx_uninit:
    que_rx_uninit(tgt_proc);
ack:
    que_tgt_ack_proc(tgt_proc, ret, tgt_proc->pre_pkt_sn);
    return;
}

void que_tgt_pkt_proc(struct que_tgt_proc *tgt_proc, struct que_pkt *pkt)
{
    int ret;

    ret = que_rx_init(tgt_proc, pkt);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        if (que_unlikely((ret != DRV_ERROR_QUEUE_EMPTY) && (ret != DRV_ERROR_TRANS_LINK_ACK_TIMEOUT_ERR))) {
            QUEUE_LOG_ERR("que rx init fail. (ret=%d; devid=%u; qid=%u)\n", ret, tgt_proc->devid, tgt_proc->qid);
        }
        goto ack;
    }

    ret = que_tgt_first_pkt_pre_proc(tgt_proc, pkt);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que wr fill fail. (ret=%d; devid=%u; qid=%u)\n", ret, tgt_proc->devid, tgt_proc->qid);
        goto mbuf_uninit;
    }
    que_tgt_time_stamp(tgt_proc, TRACE_FIRST_PKT_PROC);

    if (tgt_proc->rw_wr->cur_wr_idx > 0) {
        ret = que_uma_rw_post_async(tgt_proc->usr_ctx_addr, tgt_proc->data_read_jetty, tgt_proc->rw_wr);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            goto first_pkt_post_proc;
        }
        return;
    }

    ret = que_tgt_post_proc(tgt_proc);
    que_tgt_time_stamp(tgt_proc, TRACE_TGT_POST_PROC);
    goto rx_uninit;

first_pkt_post_proc:
    que_tgt_first_pkt_post_proc(tgt_proc);
mbuf_uninit:
    que_mbuf_uninit(tgt_proc->rx);
rx_uninit:
    que_rx_uninit(tgt_proc);
ack:
    que_tgt_ack_proc(tgt_proc, ret, pkt->head.sn);
    return;
}
#else /* EMU_ST */

void que_enque_emu_test(void)
{}

#endif
