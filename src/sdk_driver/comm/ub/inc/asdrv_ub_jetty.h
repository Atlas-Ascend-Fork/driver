/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef ASDRV_UB_JETTY_H
#define ASDRV_UB_JETTY_H

#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ubcore_uapi.h"
#include "ubcore_types.h"
#include "ub_cmd_msg.h"
#include "comm_kernel_interface.h"

#define MEM_ACCESS_ALL (UBCORE_ACCESS_READ | UBCORE_ACCESS_WRITE | UBCORE_ACCESS_ATOMIC)
#define MEM_ACCESS_LOCAL UBCORE_ACCESS_LOCAL_ONLY
#define DEFAULT_TOKEN_VALUE 0x5a5a5a5aU // only for link chan in now

#define COMPLETE_ENABLE 1
#define UBDRV_UBCORE_EID_LEN sizeof(struct ubcore_eid_info)
#define DEFAULT_EID_INDEX 0

#define ASCEND_UB_ADMIN_SEND_SEG_COUNT 3
#define ASCEND_UB_ADMIN_SEND_SEG_SIZE 4096 // 4k
#define ASCEND_UB_ADMIN_RECV_SEG_COUNT 16
#define ASCEND_UB_POLL_JFC_ONE 1

#define ASCEND_UB_ADMIN_JETTY_JFC_DEPTH 128

#define ASCEND_UB_MSG_DESC_LEN sizeof(struct ascend_ub_msg_desc)
#define ASCEND_UB_MAX_SEND_LIMIT 4096 // 4k
#define ASCEND_UB_ADMIN_MAX_SEND_LEN                                                              \
    ((ASCEND_UB_ADMIN_SEND_SEG_SIZE > ASCEND_UB_MAX_SEND_LIMIT ? ASCEND_UB_MAX_SEND_LIMIT :       \
                                                                 ASCEND_UB_ADMIN_SEND_SEG_SIZE) - \
     ASCEND_UB_MSG_DESC_LEN)

#define ASCEND_UB_MSG_MAX_MEM_SIZE 0x2000000U // 32M
/* jfc cfg */
#define UB_URMA_JFC_CEQN 0

/* jfr cfg */
#define UB_URMA_QUEUE_LOCK 0
#define UB_SHARE_JFR 1
#define UB_NON_TRANS_DEFAULT_DEPTH 16
#define UB_JFR_RNR_TIME 10

/* jetty cfg */
#define UBDRV_JETTY_MAX_SEND_SEG 1
#define UBDRV_JETTY_MAX_RECV_SEG 1
#define UBDRV_JETTY_RNR_RETRY 7
#define UBDRV_JETTY_ERR_TIMEOUT 17 // 0-7: 128ms, 8-15:1s, 16-23:8s, 24-31:64s
#define ASCEND_UB_QOS_LVL_HIGH 10U

/* seg cfg */
#define UBDRV_SEG_NON_PIN 1

#define UB_MSG_TIME_NORMAL 0
#define UB_MSG_TIME_OUT 1

#define UBDRV_SCHED_RESP_TIME 1000000 // 1000000 us
struct ubdrv_rao_info {
    u64 va;
    u64 len;
    enum devdrv_rao_client_type type;
};

struct ascend_ub_jetty_ctrl {
    u32 eid_index;
    struct ub_idev *idev;
    struct ubcore_device *ubc_dev;
    struct ubcore_jfs *jfs;
    struct ubcore_jfc *jfs_jfc;
    struct ubcore_jfr *jfr;
    struct ubcore_jfc *jfr_jfc;
    u32 jfs_msg_depth;
    u32 send_msg_len;
    struct ubcore_target_seg *send_seg;
    u32 jfr_msg_depth;
    u32 recv_msg_len;
    struct ubcore_target_seg *recv_seg;
    u32 access;
    u32 token_value;
    void *msg_chan;
    enum ubdrv_msg_chan_mode chan_mode;
    struct ubdrv_rao_info *rao_info;
};

struct ascend_ub_sync_jetty {
    // for host admin jetty has only send jetty, for device only a recv jetty
    ka_mutex_t mutex_lock;
    ka_atomic_t user_cnt;
    u32 valid;
    struct ascend_ub_jetty_ctrl send_jetty;
    struct ascend_ub_jetty_ctrl recv_jetty;
};

struct send_wr_cfg {
    struct ubcore_jfs *jfs;
    struct ubcore_tjetty *tjetty;
    u32 user_ctx;
    struct ubcore_target_seg *sseg;
    struct ubcore_target_seg *tseg;
    u64 sva;
    u64 dva;
    u32 len;
};

struct ubdrv_sync_jfce_work {
    struct ubcore_jfc *jfc;
    struct ubdrv_msg_chan_stat *stat;
    ka_work_struct_t work;
};

#define UBDRV_DFX_INFO_LEN 128
u32 ubdrv_record_resp_time(u64 pre_jiffies, const char *str, u32 timeout);

u64 ubdrv_get_seg_offset(struct ubcore_target_seg *tseg, u32 seg_id, u32 seg_size);
struct ascend_ub_msg_desc *ubdrv_get_send_desc(struct ascend_ub_jetty_ctrl *cfg, u32 seg_id);
struct ascend_ub_msg_desc *ubdrv_get_recv_desc(struct ascend_ub_jetty_ctrl *cfg, u32 seg_id);
int ubdrv_post_rw_wr_process(struct send_wr_cfg *wr_cfg, enum ubcore_opcode opcode);
int ubdrv_post_send_wr(struct send_wr_cfg *wr_cfg, u32 dev_id);
struct ubcore_tjetty *ascend_import_jfr(struct ubcore_device *dev, u32 eid_index, struct jetty_exchange_data *data);
struct ubcore_target_seg *ascend_import_seg(struct ubcore_device *dev, struct jetty_exchange_data *data);
int ubdrv_create_sync_jfc(struct ascend_ub_jetty_ctrl *cfg, ubcore_comp_callback_t jfce_handler);
int ubdrv_create_sync_jfr(struct ascend_ub_jetty_ctrl *cfg, struct ubcore_jfc *jfc, u32 jfr_id);
int ubdrv_create_sync_jfs(struct ascend_ub_jetty_ctrl *cfg);
void ubdrv_delete_jfr(struct ascend_ub_jetty_ctrl *cfg);
void ubdrv_delete_sync_jfc(struct ascend_ub_jetty_ctrl *cfg);
void ubdrv_delete_sync_jfs(struct ascend_ub_jetty_ctrl *cfg);
int ubdrv_get_send_jetty_info(struct ascend_ub_jetty_ctrl *cfg, struct jetty_exchange_data *data);
int ubdrv_get_recv_jetty_info(struct ascend_ub_jetty_ctrl *cfg, struct jetty_exchange_data *data);
int ubdrv_sync_send_jetty_init(struct ascend_ub_jetty_ctrl *cfg, ubcore_comp_callback_t jfce_handler);
int ubdrv_sync_recv_jetty_init(
    struct ascend_ub_jetty_ctrl *cfg, u32 jfr_id,
    ubcore_comp_callback_t jfce_handler); // jetty id is 0 not fixed jetty id, only fixed id in admin
void ubdrv_sync_send_jetty_uninit(struct ascend_ub_jetty_ctrl *cfg);
void ubdrv_sync_recv_jetty_uninit(struct ascend_ub_jetty_ctrl *cfg);
void ubdrv_sync_jetty_init(struct ascend_ub_sync_jetty *sync_jetty);
void ubdrv_sync_jetty_uninit(struct ascend_ub_sync_jetty *sync_jetty);
void ubdrv_admin_jfce_send_handle(struct ubcore_jfc *jfc);
void ubdrv_jfce_recv_work(ka_work_struct_t *p_work);
int ubdrv_clear_jetty(struct ubcore_jfs *jfs);
int ubdrv_rearm_sync_jfc(struct ascend_ub_sync_jetty *sync_jetty);
int ubdrv_delete_sync_send_segment(struct ascend_ub_jetty_ctrl *cfg);
int ubdrv_post_jfr_wr(struct ubcore_jfr *jfr, struct ubcore_target_seg *seg, struct ascend_ub_msg_desc *va, u32 len,
                      u32 seg_id);
struct ascend_ub_msg_desc *ubdrv_wait_sync_msg_rqe(u32 dev_id, struct ascend_ub_jetty_ctrl *jetty_ctrl, u32 msg_num,
                                                   u32 cnt, struct ubdrv_msg_chan_stat *stat);
int ubdrv_interval_poll_send_jfs_jfc(struct ascend_ub_jetty_ctrl *jetty_ctrl, u64 user_ctx, u32 cnt,
                                     struct ubcore_cr *cr, struct ubdrv_msg_chan_stat *stat, bool check_dev_status);
int ubdrv_interval_poll_recv_jfs_jfc(struct ascend_ub_jetty_ctrl *jetty_ctrl, u64 user_ctx, u32 cnt,
                                     struct ubcore_cr *cr, struct ubdrv_msg_chan_stat *stat);
int ubdrv_rebuild_jfs_jfc(u32 dev_id, struct ascend_ub_jetty_ctrl *jetty_ctrl);
void ubdrv_print_ctx_info(u32 dev_id, u32 chan_id, struct ubcore_jfs *jfs, struct ubcore_jfr *jfr,
                          struct ubcore_jfc *jfc);
#endif