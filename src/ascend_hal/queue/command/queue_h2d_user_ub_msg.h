/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef QUEUE_H2D_USER_UB_MSG_H
#define QUEUE_H2D_USER_UB_MSG_H

#include "urma_api.h"
#include "ascend_hal_define.h"
#include "ascend_hal_external.h"
#include "queue_ioctl.h"
#include "uref.h"
#include "rbtree.h"

#define QUE_DATA_RW_JETTY_POOL_SEND_DEPTH (2 * 1024)
#define QUE_MAX_RW_WR_NUM QUE_DATA_RW_JETTY_POOL_SEND_DEPTH

typedef enum que_chan_type {
    CHAN_CREATE,
    CHAN_ATTACH,
    CHAN_INTER_DEV_ATTACH,
    QUEUE_CHAN_BUTT,
} QUEUE_CHAN_TYPE;

typedef enum que_agent_type {
    H2D_SYNC_ENQUE,
    H2D_SYNC_DEQUE,
    ASYNC_ENQUE,
    QUEUE_ENQUE_BUTT,
} QUEUE_AGENT_TYPE;

enum ini_cnt_type {
    INI_SEND_SUCCESS,
    INI_SEND_FAIL,
    INI_WAIT_SUCCESS,
    INI_WAIT_FAIL,
    INI_CNT_MAX,
};

enum tgt_cnt_type {
    TGT_RECV_SUCCESS,
    TGT_RECV_FAIL,
    TGT_SEND_ACK_SUCCESS,
    TGT_SEND_ACK_FAIL,
    TGT_CNT_MAX,
};

typedef enum que_mem_type {
    MEM_DEVICE_SVM,
    MEM_OTHERS_SVM,
    MEM_NOT_SVM,
    MEM_TYPE_BUTT,
} QUE_MEM_TYPE;

struct que_urma_token {
    urma_token_id_t *token_id;
    urma_token_t token;
};

struct que_mem_merge_ctx {
    pthread_rwlock_t rwlock;
    struct rbtree_root rb_root;
};

struct que_jfs_attr {
    unsigned int jfc_s_depth;
    unsigned int jfs_depth;
    unsigned int spec_jfce_s;
    unsigned int spec_jfc_s;
    unsigned int priority : 8;
    unsigned int rsv : 24;
    urma_jfce_t *jfce;
    urma_jfc_t *jfc;
};

struct que_jfs {
    unsigned int devid;
    struct que_jfs_attr attr;
    urma_jfs_t *jfs;
    urma_jfce_t *jfce_s;
    urma_jfc_t *jfc_s;
    urma_target_jetty_t *tjetty;
};

struct que_ack_jfs {
    unsigned int devid;
    struct que_jfs_attr attr;
    urma_jfs_t **jfs;
    urma_jfce_t *jfce_s;
    urma_jfc_t *jfc_s;
    urma_target_jetty_t *tjetty;
};

/* Alloced for event ub send and freed after send */
struct que_tx {
    QUE_MEM_TYPE mem_type;
    QUEUE_AGENT_TYPE que_type;

    bool default_wr_flag;
    unsigned int first_iovec_num;
    unsigned int remain_iovec_num;
    unsigned int sn : 8;
    unsigned int rsv : 24;

    uint64_t pkt_timestamp;
    unsigned long long pkt_size; /* Total pakets size */
    struct que_pkt *pkt;         /* Base address of all packet */
    urma_target_seg_t *pkt_tseg;

    unsigned int total_iovec_num;
    unsigned long long total_iovec_size;
    urma_target_seg_t *ctx_tseg;
    urma_target_seg_t **iovec_tseg;
};

struct que_jfr_attr {
    unsigned int jfc_r_depth;
    unsigned int jfr_depth;
    urma_jfce_t *jfce;
    urma_jfc_t *jfc;
};

struct que_jfr {
    unsigned int devid;
    struct que_jfr_attr attr;
    urma_jfr_t *jfr;
    urma_jfce_t *jfce_r;
    urma_jfc_t *jfc_r;
};

struct que_recv_para {
    unsigned int num;
    size_t size;
    unsigned long long addr;
    urma_target_seg_t *tseg;
};

typedef enum async_que_ini_status {
    INI_IDLE,
    INI_ENQUE_BUSY,
    INI_WAIT_F2NF,
    INI_ABNORMAL,
    INI_STATUS_BUTT,
} ASYNC_QUE_INI_STATUS;

struct que_mbuf_list {
    unsigned int head;
    unsigned int tail;
    unsigned int depth;
    void **mbuf_array;
    pthread_rwlock_t mbuf_lock;
};

struct que_jfs_pool_info {
    struct que_jfs *qjfs;
    bool jfs_busy_flag;
};

struct que_rx_mbuf {
    Mbuf *mbuf;

    urma_target_seg_t *ctx_aligned_tseg;

    void *mbuf_ctx_va;
    size_t mbuf_ctx_size;

    unsigned long long data_va;
    unsigned long long data_size;
    urma_target_seg_t *data_tseg;
};

struct que_rx {
    QUE_MEM_TYPE mem_type;
    QUEUE_AGENT_TYPE que_type;

    unsigned int copied_iovec_num;
    unsigned int total_iovec_num;

    unsigned long long copied_iovec_size;
    unsigned long long total_iovec_size;
    unsigned long long total_copy_size;

    struct que_rx_mbuf rx_mbuf;

    unsigned long long remain_pkt_base;
    unsigned long long remain_pkt_size;

    unsigned int first_iovec_num;
    unsigned int remain_iovec_num;

    urma_target_seg_t *remote_pkt_tseg;
    urma_target_seg_t *local_pkt_tseg;
    urma_target_seg_t *remote_ctx_tseg;

    unsigned int iovec_idx_in_wr_link;
    urma_target_seg_t *tseg[QUE_MAX_RW_WR_NUM];
};

struct que_jfs_rw_wr_attr {
    urma_opcode_t opcode;
    unsigned int wr_num;
};

struct que_jfs_rw_wr {
    struct que_jfs_rw_wr_attr attr;
    urma_jfs_wr_t *wr;
    unsigned int cur_wr_idx;
    unsigned int max_wr_num;
};

struct que_ini_proc {
    unsigned int devid;
    unsigned int qid;
    QUEUE_AGENT_TYPE que_type;

    struct que_jfs *pkt_send_jetty;
    struct que_tx *tx;

    struct que_jfr *imm_recv_jetty;
    struct que_recv_para *recv_para;
    struct que_urma_token token_info;
    struct que_mem_merge_ctx mem_ctx;

    ASYNC_QUE_INI_STATUS ini_status;
    unsigned int f2nf_back;
    unsigned int f2nf_update;
    uint64_t wait_f2nf_start_time;
    struct que_mbuf_list mbuf_list;
    struct buff_iovec *vector;
    pthread_rwlock_t ini_status_lock;

    unsigned int peer_qid;
    unsigned int peer_devid;
    struct que_jfs_pool_info *jfs_info; /* Send jfs pool for peer device, for pkt send */
    unsigned int jfs_idx;               /* Send jetty idx for peer device, for pkt send */
    urma_target_jetty_t *tjetty;        /* Imported jetty from peer device, for pkt send */
    struct que_jfr *qjfr;               /* Recv jetty for peer device, for async queue ack recv */
    unsigned int total_iovec_num;
    uint64_t *timestamp;
    unsigned int sn : 8;
    unsigned int tgt_time : 24;
    unsigned int cnt[INI_CNT_MAX];
    unsigned int d2d_flag;
};

struct que_tgt_proc {
    unsigned int devid;
    unsigned int qid;
    QUEUE_AGENT_TYPE que_type;

    struct que_rx *rx;

    unsigned int data_read_jetty_idx;
    struct que_jfs *data_read_jetty;

    bool default_wr_flag;
    struct que_jfs_rw_wr *rw_wr;

    struct que_ack_jfs ack_send_jetty;
    int tgt_proc_result;
    bool is_finished;
    unsigned int pre_pkt_sn : 9;
    unsigned int rsv : 23;
    unsigned long long usr_ctx_addr;
    unsigned int peer_qid;
    unsigned int total_iovec_num;
    uint64_t *timestamp;
    unsigned int cnt[TGT_CNT_MAX];
    unsigned int d2d_flag;
};

struct que_chan {
    /* Input */
    unsigned int devid;

    /* Created */
    unsigned int qid;
    unsigned long create_time;

    unsigned int chan_type;
    struct que_urma_token token;

    struct que_ini_proc *ini_proc[QUEUE_ENQUE_BUTT];
    struct que_tgt_proc *tgt_proc;

    unsigned int local_create_flag;
    struct uref ref;
};

struct que_node {
    unsigned long long va;
    unsigned long long size;
    urma_seg_t seg;
};

struct que_pkt_head {
    unsigned int qid;
    unsigned int peer_qid;
    unsigned int first_iovec_num;
    unsigned int remain_iovec_num;
    unsigned int total_iovec_num;
    unsigned int sn : 8;
    unsigned int rsv : 24;
    uint64_t pkt_timestamp;
    uint64_t ini_base_timestamp;
    unsigned long long total_iovec_size;

    QUE_MEM_TYPE mem_type;
    bool default_wr_flag;
    QUEUE_AGENT_TYPE que_type;
    struct que_node ctx_node;
    struct que_node pkt_node;
    urma_jfr_id_t jfr_id;
    urma_token_t token;
};

/* Stored in 4KB(QUE_UMA_MAX_SEND_SIZE) memory and sent to remote side */
struct que_pkt {
    struct que_pkt_head head;
    unsigned int iovec_node_num;
    struct que_node iovec_node[];
};

struct que_init_in_msg {
    pid_t hostpid;
    urma_jfr_id_t jfr_id;
    urma_token_t token;
};

struct que_init_out_msg {
    urma_jfr_id_t tjfr_id;
    urma_token_t token;
};

struct que_create_in_msg {
    QueueAttr que_attr;
};

struct que_create_out_msg {
    unsigned int qid;
    unsigned long create_time;
};

struct que_destoy_in_msg {
    unsigned int qid;
};

struct que_sub_event_in_msg {
    unsigned int qid;
    int pid;
    unsigned int grp_id;
    unsigned int tid;
    unsigned int event_id;
    unsigned int dst_phy_devid;
    unsigned int inner_sub_flag;
    unsigned int dst_engine;
};

struct que_unsub_event_in_msg {
    unsigned int qid;
};

struct que_peek_in_msg {
    unsigned int qid;
};

struct que_enque_in_msg {
    unsigned int qid;
};

struct que_query_info_in_msg {
    unsigned int qid;
};

struct que_query_info_out_msg {
    int size;
    int status;
    int work_mode;
    unsigned long long enque_cnt; // statistics of the successful enqueues
    unsigned long long deque_cnt; // statistics of the successful dequeues
};

struct que_get_status_in_msg {
    unsigned int qid;
    QUEUE_QUERY_ITEM query_item;
    unsigned int out_len;
};

struct que_set_in_msg {
    unsigned int qid;
    unsigned int work_mode;
};

struct que_get_status_out_msg {
    char data[EVENT_PROC_RSP_LEN];
};

struct que_finish_cb_in_msg {
    unsigned int qid;
    unsigned int grp_id;
    unsigned int event_id;
};

union que_attach_out_msg {
    unsigned long create_time;
    char rsv[EVENT_PROC_RSP_LEN];
};

struct que_attach_in_msg {
    unsigned int qid;
    int timeout;
};

struct que_inter_dev_attach_in_msg {
    unsigned int qid;
    unsigned int depth;
    urma_jfr_id_t tjfr_id;
    urma_token_t token;
};

struct que_reset_in_msg {
    unsigned int qid;
    unsigned int peer_deploy_flag;
};

struct que_inter_dev_export_import_in_msg {
    unsigned int dev_id;
    unsigned int peer_dev_id;
    unsigned int phy_peer_devid;
    char share_queue_name[SHARE_QUEUE_NAME_MAX_LEN];
    unsigned int qid;
    pid_t devpid;
    unsigned int grpid;
};

struct que_inter_dev_import_out_msg {
    unsigned int qid;
    unsigned int depth;
    int work_mode;
    int flow_ctrl_flag;
    unsigned int flow_ctrl_drop_time;
    pid_t devpid;
    unsigned int grpid;
};

#define QUE_MAX_QUE_LIST_NUM (50)
struct que_queue_info {
    unsigned int qid : 31;
    unsigned int alive : 1;
};

struct que_query_alive_msg {
    unsigned int num;
    struct que_queue_info qid_list[QUE_MAX_QUE_LIST_NUM];
};
#endif
