/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef TRS_URMA_H
#define TRS_URMA_H

#include <stdlib.h>
#include <pthread.h>
#include "ascend_hal_define.h"
#include "drv_user_common.h"
#include "trs_user_pub_def.h"
#include "urma_types.h"
#include "udma_u_ctl.h"
#include "trs_dev_drv.h"
#include "trs_user_msg.h"
#include "trs_urma_async.h"

#define TRS_URMA_WR_CPY_MAX_SIZE 0x10000000ULL /* 256MB */

#define TRS_URMA_WR_PLACE_ORDER 2U
#define TRS_URMA_ARGS_CPY_WR_MAX_NUM 16U

#define TRS_UB_CQE_LINE 1
#define TRS_UB_SQE_WQE_BB_SIZE 64U
uint32_t trs_get_jfs_depth(void);
uint32_t trs_get_jfc_depth(void);
uint32_t trs_get_jfr_depth(void);
#define TRS_STARS_JFC_MAX_DEPTH (8192 * 2)
#define TRS_JFS_MAX_SGE_NUM 12U

#define TRS_URMA_DWQE_LEN 64U

enum trs_urma_wqe_opcode {
    TRS_URMA_OPCODE_WRITE = 0x3,
    TRS_URMA_OPCODE_READ = 0x6,
    TRS_URMA_OPCODE_NOP = 0x11,
};

struct trs_urma_jfc {
    urma_jfc_t *jfc;
    uint32_t id;            /* only used in stars jfc */
    uint64_t cqe_base_addr; /* only used in stars jfc */
};

struct trs_wr_sge_pack {
    urma_jfs_wr_t wr;
    urma_sge_t src_sge;
    urma_sge_t dst_sge;
};

struct trs_urma_jfs {
    struct trs_urma_jfc trs_jfc;
    urma_jfs_t *jfs;
    uint64_t *jfs_db_addr;
    uint32_t pi;
    struct trs_wr_sge_pack *wr_sge_array;
};

struct trs_urma_jfr {
    struct trs_urma_jfc trs_jfc;
    urma_jfr_t *jfr;
};

struct trs_urma_stars_jetty {
    struct trs_urma_jfc trs_jfc;
    urma_jetty_t *jetty;
    urma_target_seg_t **tseg_array;
    struct trs_wr_sge_pack *wr_sge_array;
    uint32_t pi;
    uint32_t id;
    uint32_t depth;
    int jetty_id_type;
    enum udma_u_jetty_type jetty_type;
    bool wqe_sink_flag;
    bool jetty_pre_fill_flag;
    bool init_flag;
    uint64_t jetty_queue_base;
    uint32_t jetty_queue_len;
};

#define STARS_ASYNC_DMA_WQE_SIZE 64
struct trs_async_dma_wqe_info {
    uint8_t wqe[STARS_ASYNC_DMA_WQE_SIZE]; /* wqe must be first member for free */
    urma_target_seg_t *async_tseg;         /* for async copy */
    uint32_t remote_dev_id;
    trs_d2d_index_t d2d_index;
    uint8_t flag; /* 0: svm addr and not need register segment; 1: non svm addr and need to register segment */
    uint8_t dir;
};

struct trs_d2d_dev_ctx {
    urma_jfr_id_t dst_jfr_id;
    uint32_t token_value;
    bool tpn_valid[TRS_DEV_NUM];
    uint32_t tpn[TRS_DEV_NUM];
};

struct trs_async_ctx {
    urma_jetty_id_t src_jetty_id;
    uint32_t tpn;
    uint32_t pi;
    uint32_t ci;
    uint32_t die_id;
    uint32_t func_id;
    bool init_flag;
};

struct trs_urma_ctx {
    urma_jfce_t *jfce;
    urma_context_t *urma_ctx;
    struct trs_urma_jfs trs_jfs;
    struct trs_urma_jfr trs_jfr;
    struct trs_urma_stars_jetty
        trs_jetty; /* for single task dwqe(cache lock jetty) and task sinking async(normal jetty) */
    struct trs_urma_stars_jetty trs_batch_2d_jetty; /* for single task 2d-batch async(normal jetty) */
    urma_target_jetty_t *tjetty;
    urma_target_seg_t *sq_que_tseg;
    urma_target_seg_t *sq_tail_tseg;
    urma_target_seg_t *local_tseg;
    void *notify_buffer;
    urma_target_seg_t local_notify_tseg;
    urma_token_id_t *token_id;
    urma_token_t token;
    urma_eid_t eid;
    struct trs_async_ctx async_ctx;
    struct trs_async_ctx batch_2d_async_ctx;
    struct trs_async_ctx d2d_async_ctx[TRS_MAX_D2D_INDEX];          /* d2d send side async ctx */
    struct trs_async_ctx d2d_batch_2d_async_ctx[TRS_MAX_D2D_INDEX]; /* d2d send side async ctx */
    pthread_mutex_t ctx_mutex;
    uint32_t devid;
    uint32_t die_id;  /* for async copy */
    uint32_t func_id; /* for async copy */
    uint64_t remote_sq_que_addr;
};

#define TRS_URMA_SEG_TOKEN_ALLOC_DIVIDE 8
struct trs_urma_proc_ctx {
    urma_jfce_t *jfce;
    struct trs_urma_jfr trs_jfr;
    urma_target_jetty_t *tjetty;
    urma_device_t *urma_dev;
    urma_context_t *urma_ctx;
    urma_token_t token;
    urma_eid_t eid;
    uint32_t eid_index;
    urma_token_id_t *token_id[TRS_URMA_SEG_TOKEN_ALLOC_DIVIDE];
    uint32_t dev_id;
    unsigned int die_id;
    unsigned int func_id;
    uint32_t tpn; /* only valid for h2d/d2h */
    bool tpn_valid;
};

struct trs_dma_wr_addr {
    uint64_t len;
    uint64_t src_ubva;
    uint64_t dst_ubva;
    urma_seg_t recv_seg;
    urma_seg_t send_seg;
    urma_token_t recv_token;
    uint32_t urma_opcode;
    uint32_t dir;
};

struct trs_remote_fill_wqe_info {
    uint32_t sq_id;
    uint32_t recv_dev_id;
    uint32_t d2d_index;
    enum trs_async_dma_type async_dma_type;
    uint32_t batch_num;
    struct trs_dma_wr_addr wr_addr[0];
};

struct trs_async_jetty_init_para {
    urma_jetty_id_t jetty_id;
    uint32_t tpn;
};

struct trs_remote_alloc_jetty_para {
    struct halAsyncDmaJettyCreateIn in;
    urma_jfr_id_t jfr_id;
    urma_token_t token;
};

struct trs_async_jetty_info {
    uint32_t die_id;
    uint32_t func_id;
    uint32_t jetty_id;
    uint32_t tpn;
    uint64_t jetty_queue_base;
    uint32_t jetty_queue_len;
    struct trs_urma_stars_jetty *slave_jetty_addr;
};

#define TRS_UB_PI_CI_DEPTH 65536U
#define TRS_STARS_CACHE_LOCK_DWQE_JETTY_DEPTH 64U
#define TRS_STARS_NORMAL_JETTY_DEPTH 2048U
#define TRS_URMA_ASYNC_CPY_WR_MAX_NUM TRS_STARS_NORMAL_JETTY_DEPTH

static inline uint32_t trs_get_stars_jetty_depth(bool is_direct_wqe)
{
    return (is_direct_wqe == true) ? TRS_STARS_CACHE_LOCK_DWQE_JETTY_DEPTH : TRS_STARS_NORMAL_JETTY_DEPTH;
}

/* send cnt need less than jetty depth in task sinking scene */
static inline uint32_t trs_get_async_pi_ci_max(bool is_wqe_sink)
{
    return (is_wqe_sink == false) ? TRS_UB_PI_CI_DEPTH : TRS_STARS_NORMAL_JETTY_DEPTH;
}

static inline bool trs_is_async_jetty_wqe_sink(uint32_t flag)
{
    return ((flag & TSDRV_FLAG_TASK_SINK_SQ) != 0);
}

static inline bool trs_is_async_jetty_pre_alloc(uint32_t flag)
{
    return ((flag & TSDRV_FLAG_PRE_ASYNC_SQ) != 0);
}

static inline bool trs_is_async_direct_wqe(uint32_t flag, enum trs_async_dma_type async_type)
{
    return (((flag & TSDRV_FLAG_TASK_SINK_SQ) == 0) && (async_type != TRS_ASYNC_DMA_TYPE_2D) &&
            (async_type != TRS_ASYNC_DMA_TYPE_BATCH));
}

static inline urma_import_seg_flag_t trs_get_seg_import_flag(void)
{
    urma_import_seg_flag_t import_flag = {0};

    import_flag.bs.cacheable = URMA_NON_CACHEABLE;
    import_flag.bs.access = URMA_ACCESS_READ | URMA_ACCESS_WRITE | URMA_ACCESS_ATOMIC;
    import_flag.bs.mapping = URMA_SEG_NOMAP;
    import_flag.bs.reserved = 0;

    return import_flag;
}

static inline urma_reg_seg_flag_t trs_get_seg_register_flag(void)
{
    urma_reg_seg_flag_t register_flag = {0};

    register_flag.bs.token_policy = URMA_TOKEN_PLAIN_TEXT;
    register_flag.bs.cacheable = URMA_NON_CACHEABLE;
    register_flag.bs.access = URMA_ACCESS_READ | URMA_ACCESS_WRITE | URMA_ACCESS_ATOMIC;
    register_flag.bs.non_pin = 1; /* alloc_pages memory need non pin, malloc need pin */
    register_flag.bs.token_id_valid = 1;
    register_flag.bs.reserved = 0;

    return register_flag;
}

static inline uint32_t trs_jetty_get_credit(uint32_t pi, uint32_t ci, uint32_t depth)
{
    if (pi >= ci) {
        return depth - (pi - ci + 1);
    } else {
        return ci - pi - 1;
    }
}

int trs_create_jfs(urma_context_t *urma_ctx, urma_jfce_t *jfce, struct trs_urma_jfs *trs_jfs, void *priv);
void trs_destroy_jfs(struct trs_urma_jfs *trs_jfs);
int trs_create_jfr(urma_context_t *urma_ctx, urma_jfce_t *jfce, urma_token_t token, struct trs_urma_jfr *trs_jfr);
void trs_destroy_jfr(struct trs_urma_jfr *trs_jfr);
int trs_stars_jetty_init(struct trs_urma_stars_jetty *trs_jetty, struct trs_urma_proc_ctx *proc_ctx, int res_type,
                         bool is_wqe_sink, bool is_2d_batch);
void trs_stars_jetty_uninit(struct trs_urma_stars_jetty *trs_jetty, urma_context_t *urma_ctx, uint32_t dev_id);
int trs_create_stars_jetty(urma_context_t *urma_ctx, urma_jfce_t *jfce, struct trs_urma_jfr *trs_jfr,
                           struct trs_urma_stars_jetty *trs_jetty);
struct trs_urma_ctx *trs_urma_ctx_create(void);
void trs_urma_ctx_destroy(struct trs_urma_ctx *urma_ctx);
struct trs_urma_proc_ctx *trs_get_urma_proc_ctx(uint32_t dev_id);
drvError_t trs_urma_proc_ctx_init(struct trs_urma_proc_ctx *proc_ctx);
void trs_urma_proc_ctx_uninit(struct trs_urma_proc_ctx *proc_ctx);
void trs_dev_ubdma_uninit(uint32_t dev_id);
int trs_query_fe_info(struct trs_urma_proc_ctx *proc_ctx);
urma_target_jetty_t *trs_import_jfr(struct trs_urma_proc_ctx *proc_ctx, urma_jfr_id_t *jfr_id, urma_token_t *token);
int trs_urma_cqe_poll_init_dev(uint32_t dev_id);
void trs_urma_cqe_poll_uninit_dev(uint32_t dev_id);
#endif
