/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef QUE_COMM_AGENT_H
#define QUE_COMM_AGENT_H

#include "urma_api.h"
#include "ascend_hal_define.h"
#include "que_uma.h"
#define NS_PER_SECOND 1000000000
#define NS_PER_MS 1000000
#define QUE_TIMEOUT_SECOND 10 /*queue time out set 10s*/

typedef enum async_que_ini_event {
    INI_ENQUE_TRY,
    INI_ENQUE_EMPTY,
    INI_ENQUE_NORMAL,
    INI_ENQUE_ERROR,
    INI_ACK_NORMAL,
    INI_ACK_FULL,
    INI_ACK_ERROR,
    INI_RECV_F2NF,
    INI_EVENT_BUTT,
} ASYNC_QUE_INI_EVENT;

typedef enum que_trace_ini_timestamp {
    TRACE_INI_START,
    TRACE_UPDATE,
    TRACE_UPDATE_BUFF,
    TRACE_PKT_SEG_CREATE_START = TRACE_UPDATE_BUFF,
    TRACE_PKT_SEG_CREATE_END,
    TRACE_CTX_SEG_CREATE_START,
    TRACE_CTX_SEG_CREATE_END,
    TRACE_TX_CREATE,
    TRACE_PKT_FILL,
    TRACE_TX_SEND,
    TRACE_ACK_WAIT_START,
    TRACE_ACK_WAIT_END,
    TRACE_WAIT_EVENT,
    TRACE_WAIT_EVENT_FINISH,
    TRACE_POST_PROC,
    TRACE_FINISH,
    TRACE_INI_LEVLE0_BUTT,
    TRACE_INI_LEVLE1_START,
    TRACE_INI_IOVEC_SEG_CREATE_START = TRACE_INI_LEVLE1_START,
    TRACE_INI_IOVEC_SEG_CREATE_END,
    TRACE_INI_LEVLE1_BUTT,
} QUE_TRACE_INI_TIMESTAMP;

typedef enum que_trace_tgt_timestamp {
    TRACE_TGT_START,
    TRACE_IMPORT_JETTY,
    TRACE_IMPORT_JETTY_BUFF,
    TRACE_CHAN_UPDATE = TRACE_IMPORT_JETTY_BUFF,
    TRACE_REMAIN_PKT_ALLOC,
    TRACE_TGT_PKT_SEG_CREATE,
    TRACE_TGT_MBUF_CREATE,
    TRACE_TGT_CTX_SEG_CREATE_START,
    TRACE_TGT_CTX_SEG_CREATE_END,
    TRACE_DATA_INIT,
    TRACE_REMAIN_PKT_SEG_IMPORT_START,
    TRACE_REMAIN_PKT_SEG_IMPORT_END,
    TRACE_MBUF_SEG_IMPORT_START,
    TRACE_MBUF_SEG_IMPORT_END,
    TRACE_FIRST_PKT_PROC,
    TRACE_TGT_POST_PROC,
    TRACE_TGT_ACK_START,
    TRACE_TGT_ACK_END,
    TRACE_TGT_LEVLE0_BUTT,
    TRACE_TGT_LEVLE1_START,
    TRACE_IOVEC_SEG_IMPORT_START = TRACE_TGT_LEVLE1_START,
    TRACE_IOVEC_SEG_IMPORT_END,
    TRACE_TGT_LEVLE1_BUTT,
} QUE_TRACE_TGT_TIMESTAMP;

typedef enum async_que_attr_type {
    PEER_JETTY_ID,
    PEER_QUE_FLAG,
    QUE_ATTR_TYPE_BUTT,
} ASYNC_QUE_ATTR_TYPE;

struct que_peer_que_attr {
    int inter_dev_state;
    unsigned int peer_devid;
    urma_jfr_id_t tjfr_id;
    urma_token_t token;
    unsigned int tjfr_valid_flag;
    unsigned int depth;
};

struct que_agent_interface_list {
    struct que_ctx *(*que_ctx_get)(unsigned int);
    void (*que_ctx_put)(struct que_ctx *);
    int (*que_ctx_add)(struct que_ctx *);
    int (*que_ctx_del)(struct que_ctx *);

    struct que_chan *(*que_chan_get)(unsigned int, unsigned int);
    void (*que_chan_put)(struct que_chan *);
    int (*que_chan_add)(struct que_chan *);
    int (*que_chan_del)(struct que_chan *);

    drvError_t (*que_ub_res_init)(unsigned int);
    void (*que_ub_res_uninit)(unsigned int, bool);

    drvError_t (*que_mem_alloc)(void **, unsigned long long);
    void (*que_mem_free)(void *);
};

struct que_agent_interface_list *que_get_agent_interface(void);
struct que_ctx *que_ctx_get(unsigned int devid);
void que_ctx_put(struct que_ctx *ctx);
int que_ctx_add(struct que_ctx *ctx);
int que_ctx_del(struct que_ctx *ctx);
struct que_chan *que_chan_get(unsigned int devid, unsigned int qid);
void que_chan_put(struct que_chan *chan);
int que_chan_add(struct que_chan *chan);
int que_chan_del(struct que_chan *chan);
int que_ub_res_init(unsigned int devid);
void que_ub_res_uninit(unsigned int devid, bool uninit_flag);
drvError_t que_mem_alloc(void **addr, unsigned long long size);
void que_mem_free(void *addr);
bool que_is_share_mem(unsigned long long addr);
void queue_agent_update_time(struct timeval start, struct timeval end, int *timeout);
int que_get_peer_que_info(unsigned int dev_id, unsigned int qid, unsigned int *remote_qid,
                          struct que_peer_que_attr *peer_que_attr);
void que_get_single_export_que_import_stat(unsigned int dev_id, unsigned int qid, unsigned int *status);
void que_get_all_export_que_import_stat(unsigned int dev_id, unsigned int *status);
int que_get_peer_proc_info(unsigned int dev_id, unsigned int qid, pid_t *remote_pid, unsigned int *remote_devid,
                           unsigned int *remote_grpid);
int que_save_inter_dev_attach_cfg(unsigned int dev_id, unsigned int qid, const struct que_inter_dev_attach_in_msg *in);
drvError_t que_inter_dev_send_f2nf(unsigned int dev_id, unsigned int qid);
unsigned int que_get_chan_devid(unsigned int devid);
unsigned int que_get_urma_devid(unsigned int devid, unsigned int peer_devid);
uint64_t que_get_cur_time_ns(void);
#endif