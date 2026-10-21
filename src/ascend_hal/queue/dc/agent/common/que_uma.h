/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef QUE_UMA_H
#define QUE_UMA_H

#include "urma_api.h"
#include "que_urma.h"
#include "que_jetty.h"

#if (defined CFG_PLATFORM_FPGA)
#define QUE_UMA_WAIT_TIME 5000000           /* 5000000ms */
#define QUE_JETTY_ALLOC_TIME_OUT_MS 5000000 /* 5000000ms */
#else
#define QUE_UMA_WAIT_TIME 5000           /* 5000ms */
#define QUE_JETTY_ALLOC_TIME_OUT_MS 5000 /* 5000ms */
#endif

#define QUE_URMA_BUFF_ALIGN_VALUE 4096   /* 4096 bytes aligned for performance */
#define QUE_URMA_MAX_SIZE 0x10000000ULL  /* 256MB */
#define QUE_UMA_MAX_SEND_SIZE (4 * 1024) /* For send/recv, max size is 4K */
#define QUE_DATA_RW_JETTY_POOL_JFC_DEPTH 64U
#define QUE_DATA_RW_JETTY_POOL_JFR_DEPTH 64U
#define QUE_DEFAULT_RW_WR_NUM (2 * 1024) /* 8 * 2K wrlink need memory 1.25M */

struct que_uma_jetty {
    urma_jfc_t *jfc;   /* jfc_r */
    urma_jfce_t *jfce; /* jfce_r */
    urma_jetty_t *jetty;
};

struct que_uma_recv_attr {
    unsigned int num;
    size_t size;
};

struct que_uma_wait_attr {
    urma_jfc_t *jfc;
    urma_jfce_t *jfce;
    unsigned int num;
    urma_cr_t *crs;
};

struct que_recv_para *que_uma_recv_create(unsigned int devid, struct que_jfr *pkt_recv_jetty,
                                          struct que_uma_recv_attr *attr, unsigned int d2d_flag);
void que_uma_recv_destroy(struct que_recv_para *recv);

void que_uma_recv_put_addr(struct que_jfr *pkt_recv_jetty, struct que_recv_para *recv_para, unsigned long long addr);
int que_uma_wait(struct que_uma_wait_attr *attr, int time_out);

int que_uma_wait_jfc(urma_jfce_t *jfce, int time_out, urma_jfc_t **jfc);
int que_recreate_jfs(unsigned int jfs_depth, urma_jfc_t *jfc_s, unsigned int devid, urma_jfs_t **jfs_s,
                     unsigned int d2d_flag);
int que_uma_send_post_and_wait(struct que_jfs *send_jetty, urma_sge_t *sge, unsigned int d2d_flag);
int que_uma_poll_send_jfc(urma_jfc_t *jfc, unsigned int cr_num, urma_cr_t *cr, unsigned int *cnt);
void que_uma_ack_jfc(urma_jfc_t *jfc, unsigned int cnt);
int que_uma_rearm_jfc(urma_jfc_t *jfc);

int que_uma_send_ack(struct que_ack_jfs *send_jetty, unsigned long long imm_data);
int que_uma_imm_wait(struct que_jfr *imm_recv_jetty, struct que_recv_para *recv_para, unsigned long long *imm_data,
                     int timeout);
void que_print_urma_ctx_info(unsigned int devid, urma_jfs_t *jfs, urma_jfr_t *jfr, urma_jfc_t *jfc);
int que_jfs_rw_wr_fill(struct que_jfs *qjfs, struct que_jfs_rw_wr *rw_wr, struct que_jfs_rw_wr_data *rw_data,
                       bool need_verify);
int que_uma_rw_post_async(unsigned long long usr_ctx, struct que_jfs *qjfs, struct que_jfs_rw_wr *rw_wr);
#endif
