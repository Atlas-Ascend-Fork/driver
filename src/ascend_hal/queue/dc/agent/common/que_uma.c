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
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <pthread.h>
#include "que_comm_agent.h"
#include "securec.h"
#include "ascend_hal_error.h"
#include "queue.h"
#include "comm_user_interface.h"
#include "que_compiler.h"
#include "que_uma.h"

int que_jfs_rw_wr_fill(struct que_jfs *qjfs, struct que_jfs_rw_wr *rw_wr, struct que_jfs_rw_wr_data *rw_data,
                       bool need_verify)
{
    unsigned int wr_num = (unsigned int)que_align_up(rw_data->size, QUE_URMA_MAX_SIZE) / QUE_URMA_MAX_SIZE;
    urma_jfs_wr_t *wr = &rw_wr->wr[rw_wr->cur_wr_idx];
    if ((need_verify == true) && ((wr_num + rw_wr->cur_wr_idx) > rw_wr->max_wr_num)) {
        QUEUE_LOG_ERR("que wr num is over limit. (wr_num=%u; max_wr_num=%u; cur_wr_idx=%u)\n", wr_num,
                      rw_wr->max_wr_num, rw_wr->cur_wr_idx);
        return DRV_ERROR_OVER_LIMIT;
    }
    for (unsigned int i = 0; i < wr_num; i++) {
        wr->tjetty = qjfs->tjetty;
        wr->opcode = rw_data->opcode;
        wr->flag.bs.place_order = 1;     /* 1：relax order. */
        wr->flag.bs.comp_order = 1;      /* 1：Completion order with previous WR. */
        wr->flag.bs.complete_enable = 0; /* 0：DO not Generate CR for this WR. */

        if (rw_data->opcode == URMA_OPC_READ) {
            urma_sge_t *sge = wr->rw.src.sge;
            sge->addr = rw_data->remote.va + i * QUE_URMA_MAX_SIZE;
            sge->len = (unsigned int)que_min(rw_data->size - i * QUE_URMA_MAX_SIZE, QUE_URMA_MAX_SIZE);
            sge->tseg = rw_data->remote.tseg;

            sge = wr->rw.dst.sge;
            sge->addr = rw_data->local.va + i * QUE_URMA_MAX_SIZE;
            sge->len = (unsigned int)que_min(rw_data->size - i * QUE_URMA_MAX_SIZE, QUE_URMA_MAX_SIZE);
            sge->tseg = rw_data->local.tseg;
        } else {
            urma_sge_t *sge = wr->rw.src.sge;
            sge->addr = rw_data->local.va + i * QUE_URMA_MAX_SIZE;
            sge->len = (unsigned int)que_min(rw_data->size - i * QUE_URMA_MAX_SIZE, QUE_URMA_MAX_SIZE);
            sge->tseg = rw_data->local.tseg;

            sge = wr->rw.dst.sge;
            sge->addr = rw_data->remote.va + i * QUE_URMA_MAX_SIZE;
            sge->len = (unsigned int)que_min(rw_data->size - i * QUE_URMA_MAX_SIZE, QUE_URMA_MAX_SIZE);
            sge->tseg = rw_data->remote.tseg;
        }
        wr->next = ((rw_wr->cur_wr_idx + i + 1) >= rw_wr->max_wr_num) ? NULL : (wr + 1);
        wr++;
    }
    rw_wr->cur_wr_idx += wr_num;
    return DRV_ERROR_NONE;
}

static int _que_uma_recv_init(unsigned int devid, struct que_recv_para *recv, unsigned int d2d_flag)
{
    unsigned long long size = recv->size * recv->num;
    urma_target_seg_t *tseg = NULL;
    unsigned int access = URMA_ACCESS_LOCAL_ONLY;
    void *addr;
    int ret;

    if (que_unlikely(recv->size > QUE_UMA_MAX_SEND_SIZE)) {
        QUEUE_LOG_ERR("invalid size. (devid=%u; size=%ld)\n", devid, recv->size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = que_mem_alloc((void **)(&addr), size);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que recv mem alloc fail. (ret=%d; devid=%u; size=%llu)\n", ret, devid, size);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    if (que_is_share_mem((unsigned long long)(uintptr_t)addr) == false) {
        tseg = que_pin_seg_create(devid, (unsigned long long)(uintptr_t)addr, size, access, NULL, d2d_flag);
    } else {
        tseg = que_get_urma_ctx_tseg(devid, d2d_flag);
    }

    if (que_unlikely(tseg == NULL)) {
        que_mem_free(addr);
        QUEUE_LOG_ERR("seg create fail. (devid=%u; size=%llu)\n", devid, size);
        return DRV_ERROR_INVALID_DEVICE;
    }
    recv->tseg = tseg;
    recv->addr = (unsigned long long)(uintptr_t)addr;

    return DRV_ERROR_NONE;
}

static void _que_uma_recv_uninit(struct que_recv_para *recv)
{
    if (que_is_share_mem(recv->addr) == false) {
        que_seg_destroy(recv->tseg);
        recv->tseg = NULL;
    }

    que_mem_free((void *)recv->addr);
    recv->addr = 0;
}

#define QUE_JFS_CTX_SIZE 256
#define QUE_JFR_CTX_SIZE 64
#define QUE_JFC_CTX_SIZE 128

static void que_print_jfs_ctx(unsigned int devid, urma_jfs_t *jfs)
{
    char buf[QUE_JFS_CTX_SIZE + 1] = {0};
    unsigned int id = jfs->jfs_id.id;
    int ret;
#ifndef EMU_ST
    ret = urma_get_jfs_opt(jfs, URMA_JFS_FULL_CTX, buf, QUE_JFS_CTX_SIZE);
    if (ret != URMA_SUCCESS) {
        QUEUE_LOG_ERR("que get jfs ctx fail .(devid=%u; ret=%d)\n", devid, ret);
        return;
    }

    QUEUE_ERR_HEX_DUMP(buf, QUE_JFS_CTX_SIZE, "JFS(devid=%u; jfs_id=%u)CTX", devid, id);
#endif
}

static void que_print_jfc_ctx(unsigned int devid, urma_jfc_t *jfc)
{
    char buf[QUE_JFC_CTX_SIZE + 1] = {0};
    unsigned int id = jfc->jfc_id.id;
    int ret;
#ifndef EMU_ST
    ret = urma_get_jfc_opt(jfc, URMA_JFC_FULL_CTX, buf, QUE_JFC_CTX_SIZE);
    if (ret != URMA_SUCCESS) {
        QUEUE_LOG_ERR("que get jfc ctx fail .(devid=%u; ret=%d)\n", devid, ret);
        return;
    }

    QUEUE_ERR_HEX_DUMP(buf, QUE_JFC_CTX_SIZE, "JFC(devid=%u; jfc_id=%u)CTX", devid, id);
#endif
}

static void que_print_jfr_ctx(unsigned int devid, urma_jfr_t *jfr)
{
    char buf[QUE_JFR_CTX_SIZE + 1] = {0};
    unsigned int id = jfr->jfr_id.id;
    int ret;
#ifndef EMU_ST
    ret = urma_get_jfr_opt(jfr, URMA_JFR_FULL_CTX, buf, QUE_JFR_CTX_SIZE);
    if (ret != URMA_SUCCESS) {
        QUEUE_LOG_ERR("que get jfr ctx fail .(devid=%u; ret=%d)\n", devid, ret);
        return;
    }

    QUEUE_ERR_HEX_DUMP(buf, QUE_JFR_CTX_SIZE, "JFR(devid=%u; jfr_id=%u)CTX", devid, id);
#endif
}

void que_print_urma_ctx_info(unsigned int devid, urma_jfs_t *jfs, urma_jfr_t *jfr, urma_jfc_t *jfc)
{
    if (jfs != NULL) {
        que_print_jfs_ctx(devid, jfs);
    }

    if (jfr != NULL) {
        que_print_jfr_ctx(devid, jfr);
    }

    if (jfc != NULL) {
        que_print_jfc_ctx(devid, jfc);
    }
}

static int _que_uma_recv_post_jfr(struct que_jfr *pkt_recv_jetty, urma_sge_t *sge, unsigned int user_ctx)
{
    urma_jfr_wr_t *bad_wr = NULL;
    urma_jfr_wr_t wr = {.src.sge = sge, .src.num_sge = 1, .user_ctx = user_ctx, .next = NULL};
    urma_status_t status;

    /* Post jfr */
    status = urma_post_jfr_wr(pkt_recv_jetty->jfr, &wr, &bad_wr);
    if (que_unlikely(status != URMA_SUCCESS)) {
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

static int que_uma_recv_post(struct que_jfr *pkt_recv_jetty, struct que_recv_para *recv)
{
    unsigned int i = 0;

    for (; i < recv->num; i++) {
        urma_sge_t sge = {.addr = recv->addr + i * recv->size, .len = (unsigned int)recv->size, .tseg = recv->tseg};
        int ret = _que_uma_recv_post_jfr(pkt_recv_jetty, &sge, i);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("recv post fail. (ret=%d; i=%u; num=%u; size=%ld)\n", ret, i, recv->num, recv->size);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

struct que_recv_para *que_uma_recv_create(unsigned int devid, struct que_jfr *pkt_recv_jetty,
                                          struct que_uma_recv_attr *attr, unsigned int d2d_flag)
{
    struct que_recv_para *recv = NULL;
    int ret;

    recv = (struct que_recv_para *)malloc(sizeof(struct que_recv_para));
    if (que_unlikely(recv == NULL)) {
        QUEUE_LOG_ERR("uma recv malloc fail. (size=%ld)\n", sizeof(struct que_recv_para));
        return NULL;
    }

    recv->num = attr->num;
    recv->size = attr->size;

    ret = _que_uma_recv_init(devid, recv, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        free(recv);
        QUEUE_LOG_ERR("recv init fail. (ret=%d; devid=%u; num=%u; size=%ld)\n", ret, devid, attr->num, attr->size);
        return NULL;
    }

    ret = que_uma_recv_post(pkt_recv_jetty, recv);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        _que_uma_recv_uninit(recv);
        free(recv);
        QUEUE_LOG_ERR("recv post fail. (ret=%d; devid=%u; num=%u; size=%ld)\n", ret, devid, attr->num, attr->size);
        return NULL;
    }

    return recv;
}

void que_uma_recv_destroy(struct que_recv_para *recv)
{
    _que_uma_recv_uninit(recv);
    free(recv);
}

#ifndef EMU_ST
int que_uma_wait_jfc(urma_jfce_t *jfce, int time_out, urma_jfc_t **jfc)
{
    int jfc_cnt = 1;
    int cnt;

    cnt = ascend_urma_wait_jfc(jfce, (unsigned int)jfc_cnt, time_out, jfc);
    if (que_unlikely(cnt != jfc_cnt)) {
        /* Do not add log here */
        return DRV_ERROR_WAIT_TIMEOUT;
    }

    return DRV_ERROR_NONE;
}

int que_uma_poll_send_jfc(urma_jfc_t *jfc, unsigned int cr_num, urma_cr_t *cr, unsigned int *cnt)
{
    int cnt_tmp;

    cnt_tmp = urma_poll_jfc(jfc, (int)cr_num, cr);
    if (que_unlikely(cnt_tmp < 0)) {
        QUEUE_LOG_ERR("poll jfc fail. (wr_num=%u; cnt=%d)\n", cr_num, cnt_tmp);
        return DRV_ERROR_INNER_ERR;
    }

    if (que_unlikely(cnt_tmp == 0)) {
        return DRV_ERROR_NO_EVENT;
    }

    *cnt = (unsigned int)cnt_tmp;
    return DRV_ERROR_NONE;
}

void que_uma_ack_jfc(urma_jfc_t *jfc, unsigned int cnt)
{
    urma_ack_jfc((urma_jfc_t **)&jfc, &cnt, 1);
}

int que_uma_rearm_jfc(urma_jfc_t *jfc)
{
    urma_status_t status;

    status = urma_rearm_jfc(jfc, false);
    if (que_unlikely(status != URMA_SUCCESS)) {
        QUEUE_LOG_ERR("rearm jfc fail. (status=%d)\n", status);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}
#endif

int que_recreate_jfs(unsigned int jfs_depth, urma_jfc_t *jfc_s, unsigned int devid, urma_jfs_t **jfs_s,
                     unsigned int d2d_flag)
{
#ifndef EMU_ST
    int ret = DRV_ERROR_NONE;
    int cnt_tmp = 0;
    urma_cr_t cr = {0};
    urma_jfs_t *jfs = NULL;
    urma_jfs_attr_t attr = {.mask = JFS_STATE, .state = URMA_JETTY_STATE_ERROR};
    urma_jfs_cfg_t jfs_cfg = {.depth = jfs_depth,
                              .trans_mode = URMA_TM_RM,
                              .max_sge = 1,
                              .max_rsge = 1,
                              .err_timeout = URMA_TYPICAL_ERR_TIMEOUT,
                              .jfc = jfc_s};
    if (*jfs_s == NULL) {
        goto create_jfs;
    }
    ret = urma_modify_jfs(*jfs_s, &attr);
    if (que_unlikely(ret != URMA_SUCCESS)) {
        QUEUE_LOG_ERR("Modify jfs fail. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    do {
        cnt_tmp = urma_poll_jfc(jfc_s, 1, &cr);
        if (que_unlikely(cnt_tmp < 0)) {
            QUEUE_LOG_ERR("Poll jfc fail. (cnt=%d)\n", cnt_tmp);
            return DRV_ERROR_INNER_ERR;
        }
    } while (cr.status != URMA_CR_WR_FLUSH_ERR_DONE);

    que_urma_jfs_destroy(*jfs_s);
    *jfs_s = NULL;
create_jfs:
    jfs = que_urma_jfs_create(devid, &jfs_cfg, d2d_flag);
    if (que_unlikely(jfs == NULL)) {
        QUEUE_LOG_ERR("Create jfs fail.\n");
        return DRV_ERROR_INNER_ERR;
    }
    *jfs_s = jfs;
#endif
    return DRV_ERROR_NONE;
}

int que_uma_wait(struct que_uma_wait_attr *attr, int time_out)
{
    urma_jfc_t *jfc = NULL;
    urma_status_t status;
    uint32_t ack_cnt = 1;
    int ret = DRV_ERROR_NONE;
    int cnt;

#ifndef EMU_ST
    cnt = ascend_urma_wait_jfc(attr->jfce, 1, time_out, &jfc);
    if (que_unlikely(cnt != 1 || attr->jfc != jfc)) {
        /* Do not add log here */
        return DRV_ERROR_WAIT_TIMEOUT;
    }
#endif

    cnt = urma_poll_jfc(attr->jfc, (int)attr->num, attr->crs);
    if (que_unlikely(cnt <= 0)) {
        QUEUE_LOG_ERR("poll jfc fail. (wr_num=%u; cnt=%d)\n", attr->num, cnt);
        ret = DRV_ERROR_INNER_ERR;
        goto ack_rearm_jfc;
    }

ack_rearm_jfc:
    urma_ack_jfc((urma_jfc_t **)&jfc, &ack_cnt, 1);
    status = urma_rearm_jfc(jfc, false);
    if (que_unlikely(status != URMA_SUCCESS)) {
        QUEUE_LOG_ERR("rearm jfc fail. (status=%d)\n", status);
        ret = DRV_ERROR_INNER_ERR;
    }

    return ret;
}

int que_uma_imm_wait(struct que_jfr *imm_recv_jetty, struct que_recv_para *recv_para, unsigned long long *imm_data,
                     int timeout)
{
    struct que_uma_wait_attr wait_attr;
    urma_cr_t cr = {0};
    int ret, post_ret;

    wait_attr.jfce = imm_recv_jetty->jfce_r;
    wait_attr.jfc = imm_recv_jetty->jfc_r;
    wait_attr.num = 1;
    wait_attr.crs = &cr;

    ret = que_uma_wait(&wait_attr, timeout);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }
    if (cr.status != URMA_CR_SUCCESS) {
        que_print_urma_ctx_info(imm_recv_jetty->devid, NULL, imm_recv_jetty->jfr, imm_recv_jetty->jfc_r);
        QUEUE_LOG_ERR("imm wait fail. (status=%d)\n", cr.status);
        ret = DRV_ERROR_INNER_ERR;
    }
    *imm_data = cr.imm_data;
#ifndef EMU_ST
    unsigned long base_addr = recv_para->addr + cr.user_ctx * recv_para->size;
    urma_sge_t sge = {.addr = base_addr, .len = (unsigned int)recv_para->size, .tseg = recv_para->tseg};
    post_ret = _que_uma_recv_post_jfr(imm_recv_jetty, &sge, (unsigned int)cr.user_ctx);
    if (que_unlikely(post_ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que recv post fail. (ret=%d)\n", post_ret);
    }
#endif
    return ret;
}

#ifndef EMU_ST
void que_uma_recv_put_addr(struct que_jfr *pkt_recv_jetty, struct que_recv_para *recv_para, unsigned long long addr)
{
    unsigned int offset = (unsigned int)((addr - recv_para->addr) / recv_para->size);

    if (que_likely((offset < recv_para->num))) {
        unsigned long base_addr = recv_para->addr + offset * recv_para->size;
        int ret;
        urma_sge_t sge = {.addr = base_addr, .len = (unsigned int)recv_para->size, .tseg = recv_para->tseg};
        ret = _que_uma_recv_post_jfr(pkt_recv_jetty, &sge, offset);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("repost recv data fail. (ret=%d; offset=%u)\n", ret, offset);
        }
    }
}
#endif

static int que_uma_send_post(struct que_jfs *send_jetty, urma_sge_t *sge)
{
    urma_jfs_wr_t *bad_wr = NULL;
    urma_jfs_wr_t wr = {0};
    urma_status_t status;

    wr.opcode = URMA_OPC_SEND;
    wr.tjetty = send_jetty->tjetty;
    wr.send.src.sge = sge;
    wr.send.src.num_sge = 1;
    wr.flag.bs.complete_enable = URMA_COMPLETE_ENABLE;
    wr.next = NULL;

    status = urma_post_jfs_wr(send_jetty->jfs, &wr, &bad_wr);
    if (que_unlikely(status != URMA_SUCCESS)) {
        QUEUE_LOG_ERR("send fail. (status=%d)\n", status);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

static int que_uma_send_imm_post(struct que_ack_jfs *send_jetty, unsigned long long imm_data)
{
    urma_jfs_wr_t *bad_wr = NULL;
    urma_jfs_wr_t send_wr = {0};
    urma_status_t status;

    send_wr.opcode = URMA_OPC_SEND_IMM;
    send_wr.tjetty = send_jetty->tjetty;
    send_wr.send.src.num_sge = 0;
    send_wr.send.imm_data = imm_data;
    send_wr.flag.bs.complete_enable = URMA_COMPLETE_ENABLE;
    send_wr.next = NULL;

    status = urma_post_jfs_wr(*(send_jetty->jfs), &send_wr, &bad_wr);
    if (que_unlikely(status != URMA_SUCCESS)) {
        QUEUE_LOG_ERR("send fail. (status=%d)\n", status);
        return DRV_ERROR_INNER_ERR;
    }
    return DRV_ERROR_NONE;
}

int que_uma_send_post_and_wait(struct que_jfs *send_jetty, urma_sge_t *sge, unsigned int d2d_flag)
{
    int ret;
    struct que_uma_wait_attr wait_attr;
    urma_cr_t cr = {0};

    wait_attr.jfce = send_jetty->jfce_s;
    wait_attr.jfc = send_jetty->jfc_s;
    wait_attr.num = 1;
    wait_attr.crs = &cr;

    ret = que_uma_send_post(send_jetty, sge);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("send post fail. (ret=%d)\n", ret);
        return ret;
    }

    ret = que_uma_wait(&wait_attr, QUE_UMA_WAIT_TIME);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("send wait fail. (ret=%d)\n", ret);
        return ret;
    }

    if ((cr.status == URMA_CR_ACK_TIMEOUT_ERR) || (cr.status == URMA_CR_REM_DATA_POISON)) {
        ret = que_recreate_jfs(send_jetty->attr.jfs_depth, send_jetty->jfc_s, send_jetty->devid, &send_jetty->jfs,
                               d2d_flag);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que recreate send ack jfs fail. (ret=%d)\n", ret);
            return DRV_ERROR_WAIT_TIMEOUT;
        }

        ret = que_uma_send_post(send_jetty, sge);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("send post fail. (ret=%d)\n", ret);
            return ret;
        }

        cr.status = URMA_CR_SUCCESS;
        ret = que_uma_wait(&wait_attr, QUE_UMA_WAIT_TIME);
        if (que_unlikely((ret != DRV_ERROR_NONE) || (cr.status != URMA_CR_SUCCESS))) {
            QUEUE_LOG_ERR("send wait fail again. (ret=%d, status=%d)\n", ret, cr.status);
            return (ret == DRV_ERROR_NONE) ? DRV_ERROR_INNER_ERR : ret;
        }
    } else if (cr.status != URMA_CR_SUCCESS) {
        que_print_urma_ctx_info(send_jetty->devid, send_jetty->jfs, NULL, send_jetty->jfc_s);
        QUEUE_LOG_ERR("cr status err. (status=%d)\n", cr.status);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

int que_uma_send_ack(struct que_ack_jfs *send_jetty, unsigned long long imm_data)
{
    int ret;
    ret = que_uma_send_imm_post(send_jetty, imm_data);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("read wr post fail. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    return DRV_ERROR_NONE;
}

int que_uma_rw_post_async(unsigned long long usr_ctx, struct que_jfs *qjfs, struct que_jfs_rw_wr *rw_wr)
{
    urma_jfs_wr_t *bad_wr = NULL;
    urma_status_t status;
    urma_jfs_wr_t *last_wr = &rw_wr->wr[rw_wr->cur_wr_idx - 1];

    last_wr->next = NULL;
    last_wr->flag.bs.place_order = 2;     /* 2：strong order. */
    last_wr->flag.bs.comp_order = 1;      /*  1：Completion order with previous WR. */
    last_wr->flag.bs.complete_enable = 1; /* 1：Generate CR for this WR after the WR is completed. */
    last_wr->user_ctx = usr_ctx;

    status = urma_post_jfs_wr(qjfs->jfs, &rw_wr->wr[0], &bad_wr);
    if (que_unlikely(status != URMA_SUCCESS)) {
        QUEUE_LOG_ERR("wr post fail. (status=%d)\n", status);
        return DRV_ERROR_INNER_ERR;
    }
    return DRV_ERROR_NONE;
}
