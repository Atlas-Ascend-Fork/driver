/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HDC_UB_DRV_H
#define HDC_UB_DRV_H

#include <semaphore.h>

#include "hdc_cmn.h"
#include "hdcdrv_cmd_ioctl.h"

#define HDC_MODULE_NAME "HISI_HDC"
#define HDC_SESSION_STATUS_IDLE 0
#define HDC_SESSION_STATUS_CONN 1
#define HDC_SESSION_STATUS_REMOTE_CLOSE 2

#define HDCDRV_STR_NAME_LEN 32

#define HDCDRV_INVALID_PID 0x7FFFFFFEULL

#ifdef CFG_PLATFORM_ESL
#define HDC_TIMEOUT_CQE_STATUS URMA_CR_REM_ACCESS_ABORT_ERR
#else
#define HDC_TIMEOUT_CQE_STATUS URMA_CR_RNR_RETRY_CNT_EXC_ERR
#endif

#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA)
#define HDC_CONN_WAIT_TIMEOUT 100000              // 100s
#define HDC_SET_SESSION_OWNER_WAIT_TIMEOUT 600000 // 600s
#define HDC_SESSION_CLOSE_WAIT_TIMEOUT 15000      // 15s
#else
#define HDC_CONN_WAIT_TIMEOUT 30000              // 30s
#define HDC_SET_SESSION_OWNER_WAIT_TIMEOUT 60000 // 60s
#define HDC_SESSION_CLOSE_WAIT_TIMEOUT 5000      // 5s
#endif

#define HDC_UB_VALID 1
#define HDC_UB_INVALID 0

#define HDC_TIMEOUT_RETRY (-0xffff0001)
#define HDC_NON_BLOCK_RETRY (-0xffff0002)
#define HDC_TIMEOUT_REBUILD (-0xffff0003)
#define HDC_RETRY_MAX_TIME 5

#ifdef CFG_FEATURE_SUPPORT_UB
#define SESSION_REMOTE_CLOSED_TIME_US (3 * CONVERT_S_TO_US) // 3s
#include "urma_api.h"

typedef struct hdc_urma_info {
    urma_context_t *urma_ctx;
    urma_device_attr_t dev_attr;
    urma_token_id_t *token_id;
    urma_target_seg_t *tseg;
    int cnt;
} hdc_urma_info_t;

typedef struct hdc_urma_jfc {
    urma_jfce_t *jfce;
    urma_jfc_t *jfc;
} hdc_urma_jfc_t;

#define HDC_URMA_MAX_JFR_DEPTH 16
#define HDC_URMA_MAX_JFS_DEPTH 16
typedef struct hdc_urma_jfr {
    urma_jfr_t *jfr;
    uint64_t seg_len;
} hdc_urma_jfr_t;

#define HDC_MEM_BLOCK_SIZE 4096
// If the value of this macro needs to be changed, the macro with the same name in kernel mode also needs to be changed.
#define HDCDRV_UB_MEM_POOL_LEN (4 * 1024 * 16 * 2) // 4K(block_size) * 16(block_num) * 2(pool num)
typedef struct hdc_urma_jfs {
    urma_jfs_t *jfs; // use urma_post_jfs_wr to send
    int jfs_tseg_flag[HDC_URMA_MAX_JFS_DEPTH];
} hdc_urma_jfs_t;

struct hdc_ub_rx_buf {
    urma_target_seg_t *tseg;
    uint64_t addr;
    uint32_t len;
    uint64_t idx; // record mem idx in tseg; 0-15 used for jfs, 16-31 used for jfr
    struct hdc_time_record_for_single_recv recv_record;
};

typedef struct hdc_ub_context {
    urma_context_t *urma_ctx;

    hdc_urma_jfc_t jfc_send;
    hdc_urma_jfc_t jfc_recv;

    hdc_urma_jfs_t jfs;

    // local JFRs
    hdc_urma_jfr_t jfr;

    // Remote JFRs
    urma_target_jetty_t *tjfr; // use urma_import_jfr to get info,  The input parameter needs remote EID jfr_id uasid

    urma_target_seg_t *tseg; // used for urma_register_seg

    urma_token_t token;
    urma_token_id_t *token_id;
    void *token_id_node;
    uint32_t uasid;
    uint32_t devid;
    uint32_t session_fd;
} hdc_ub_context_t;

// Used to exchange information between two sides.
typedef struct hdc_jfr_id_info {
    urma_eid_t eid;
    uint32_t uasid;
    /* Send local two JFR info to remote */
    uint32_t jfr_id;
    urma_token_t token_val;
} hdc_jfr_id_info_t;

struct hdc_remote_close_thread_para {
    unsigned int dev_id;
    unsigned int local_session;
    unsigned int remote_session;
    int session_close_state;
    unsigned int unique_val;
    struct hdc_time_record_for_remote_close record;
};

#define HDC_RX_LIST_LEN (HDC_URMA_MAX_JFR_DEPTH + 1)
struct hdc_ub_epoll_node {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    int ref;     // record how many ref for node, if ref is not equal to 0, this node can not be free
    int pkt_num; // num of cqe waiting for recv
    struct hdc_ub_rx_buf rx_list[HDC_RX_LIST_LEN];
    hdc_ub_context_t *ctx;
    uint32_t head;
    uint32_t tail;
    void *session;
};

struct hdc_ub_send_info {
    struct drvHdcMsgBuf *buf_list;
    uint32_t seg_id;
    urma_sge_t src;
    urma_jfs_wr_t wr;
};

static inline void hdc_pack_jetty_info(hdc_ub_context_t *ctx, hdc_jfr_id_info_t *info)
{
    info->eid = ctx->urma_ctx->eid;
    info->uasid = ctx->urma_ctx->uasid;
    info->jfr_id = ctx->jfr.jfr->jfr_id.id;
    info->token_val = ctx->token;
}

static inline bool hdc_use_kernel_pool(int service_type)
{
    if (service_type == HDC_SERVICE_TYPE_DMP) {
        return true;
    }

    return false;
}

static inline bool hdc_rx_list_is_full(struct hdc_ub_epoll_node *node)
{
    if ((node->tail + 1) % HDC_RX_LIST_LEN == node->head) {
        return true;
    }

    return false;
}

static inline bool hdc_rx_list_is_empty(struct hdc_ub_epoll_node *node)
{
    if (node->tail == node->head) {
        return true;
    }

    return false;
}

static inline bool hdc_service_type_vaild(int service_type)
{
    if ((service_type >= HDC_SERVICE_TYPE_MAX) || (service_type < 0)) {
        return false;
    }

    return true;
}

static inline bool hdc_check_session_valid_by_idx(struct hdcConfig *hdcConfig, int idx, struct hdc_ub_session *session)
{
    if (hdcConfig->info_list[idx].status == HDC_SESSION_STATUS_IDLE || hdcConfig->info_list[idx].session != session) {
        return false;
    }
    return true;
}

#define HDC_UB_TX 0
#define HDC_UB_RX 1

enum drv_hdc_ub_op {
    HDC_URMA_WAIT_FAIL = 0,                    // fail by urma_wait_jfc
    HDC_URMA_POLL_FAIL = 1,                    // fail by urma_poll_jfc
    HDC_URMA_POLL_FAIL_BY_REM_ACESS_ABORT = 2, // fail by urma_poll_jfc, with URMA_CR_REM_ACCESS_ABORT_ERR
    HDC_URMA_REARM_FAIL = 3,                   // fail by urma_rearm_jfc
    HDC_URMA_OP_FAIL_MAX
};

struct hdc_mem_res_info {
    mmProcess bind_fd;
    int service_type;
    unsigned long long user_va;
};

struct hdc_event_wait_info {
    unsigned int grp_id;
    unsigned int tid;
};

struct hdc_pipe_event_msg {
    struct hdcdrv_event_msg event_msg;
    struct timespec pipe_write_start;
};

typedef struct hdc_ub_accept_info {
    unsigned long long conn_wait;
    unsigned long long res_init;
    unsigned long long alloc_session;
    unsigned long long pre_init;
    unsigned long long urma_init;
    unsigned long long add_ctrl;
    unsigned long long send_ctrl_msg;
    unsigned long long accept;
    unsigned long long notify_search;
    unsigned long long k2u_trans;
    unsigned long long k_pre;
    unsigned long long k_send;
    unsigned long long k_ret;
    unsigned long long pipe_trans;
} hdc_ub_accept_info_t;

typedef struct hdc_ub_connect_info {
    unsigned long long link_pre_init;
    unsigned long long mem_res_init;
    unsigned long long alloc_session;
    unsigned long long pre_init;
    unsigned long long create_ub_ctx;
    unsigned long long get_res_info_and_add_ctrl;
    unsigned long long send_ctrl_msg;
    unsigned long long check_reply;
    unsigned long long free_tid_and_fill_jetty_info;
    unsigned long long connect_end;
    unsigned long long k_pre;
    unsigned long long k_send;
    unsigned long long k_wait;
    unsigned long long k_copy;
    unsigned long long k_sched;
    unsigned long long k_ret;
} hdc_ub_connect_info_t;

typedef struct hdc_urma_init_info {
    unsigned long long ctx_calloc;
    unsigned long long get_token_val;
    unsigned long long attr_calloc;
    unsigned long long create_ctx;
    unsigned long long alloc_token_id;
    unsigned long long register_share_seg;
    unsigned long long urma_init_end;
    unsigned long long lock_wait;
    unsigned long long own_reg_re;
    unsigned long long reg_alloc_tid;
    unsigned long long own_reg_new;
    unsigned long long reg_dmp;
    unsigned long long create_jfce;
    unsigned long long create_jfc;
    unsigned long long rearm_jfc;
    unsigned long long create_jfs;
    unsigned long long create_jfr;
    unsigned long long post_jfr_wr;
    unsigned long long res_init_end;
    unsigned long long get_tp_list;
    unsigned long long import_jfr;
    unsigned long long get_dev_info;
} hdc_urma_init_info_t;

typedef struct hdc_ub_close_info {
    unsigned long long timecost_close;
    unsigned long long del_jfs;  // remote_close
    unsigned long long del_jfcs; // remote_close

    unsigned long long del_recv_epoll;
    unsigned long long sub_close_event;
    unsigned long long del_data_epoll;
    unsigned long long unimport_jetty;

    unsigned long long del_jfr; // urma_uninit
    unsigned long long wait_data_fin;
    unsigned long long del_jfcr;
    unsigned long long del_jfs_1;
    unsigned long long del_jfcs_1;
    unsigned long long own_unreg; // urma_uninit

    unsigned long long share_unreg;
    unsigned long long free_token_id;
    unsigned long long del_ctx; // urma_uninit

    unsigned long long del_urma;
    unsigned long long close_kernel;
    unsigned long long mem_uninit;
    unsigned long long wake_recv;
    unsigned long long close_notify;

    unsigned long long write_file;      // remote_close
    unsigned long long session_free;    // user_close
    unsigned long long del_close_epoll; // user_close
} hdc_ub_close_info_t;

void hdc_ub_fill_dfx_info(hdc_ub_dbg_stat_t *dbg_stat, struct hdc_ub_session *session);
void hdc_jfc_dbg_fill(int tx_rx_flag, struct hdc_ub_session *session, enum drv_hdc_ub_op op);
void hdc_ub_init_dfx_info(hdc_ub_dbg_stat_t *dbg_stat);
int hdc_dfx_query_handle(int dev_id, struct hdc_ub_session *session, struct hdcdrv_event_dfx *dfx_msg);
unsigned int hdc_get_jfc_id_by_type(hdcdrv_jetty_info_t *jetty_info, int tx_rx_flag);

void hdc_touch_close_notify(int dev_id, unsigned long long peer_pid, int service_type);
void hdc_touch_connect_notify(int dev_id, unsigned long long peer_pid, int service_type);
void hdc_touch_data_in_notify(int dev_id, int service_type);
int hdc_session_alive_check(int dev_id, int l_id, unsigned int unique_val);
bool hdc_has_data_in_thread(int service_type);
void hdc_ub_notiy_init(struct hdcConfig *hdc_config);
int hdc_rearm_jfc(hdc_urma_jfc_t *hdc_jfc, struct hdc_ub_session *session, int tx_rx_flag);

void hdc_get_time_record(struct timespec *time_val, int *fail_count);
unsigned long long hdc_get_time_cost(struct timespec *start_tval, struct timespec *end_tval);
void hdc_get_recv_time_cost(struct hdc_time_record_for_single_recv *recv_record, struct hdc_ub_session *session);
void hdc_get_send_time_cost(struct hdc_time_record_for_single_send *send_record, struct hdc_ub_session *session);
void hdc_get_accept_time_cost(struct hdc_time_record_for_accept *accept_record, struct hdc_ub_session *session,
                              bool succ_flag, const union hdcdrv_cmd *send_cmd);
void hdc_get_connect_time_cost(struct hdc_time_record_for_connect *connect_record, struct hdc_ub_session *session,
                               const union hdcdrv_cmd *send_cmd);
void hdc_get_close_time_cost(struct hdc_time_record_for_close *close_record);
void hdc_get_ub_res_info(hdc_ub_context_t *ctx, hdc_ub_res_info_t *ub_res_info);
void hdc_update_session_recv_record(struct hdc_ub_rx_buf *buf, struct hdc_time_record_for_single_recv *recv_record);

#define HDC_PERF_LOG_LIMIT_TIME 60000    /* 60s */
#define HDC_PERF_LOG_LIMIT_BRANCH_RATE 1 /* print 1 counts per 60s */
#define HDC_PERF_LOG_LIMIT_TIME_ENV "ASCEND_HDC_PERF_LOG_LIMIT"

#define HDC_POLL_JFC_LOG_LIMIT_TIME 60000    /* 60s */
#define HDC_POLL_JFC_LOG_LIMIT_BRANCH_RATE 1 /* print 1 counts per 60s */

#define HDC_PERF_LOG_PRINT_NOT_LIMIT 0
#define HDC_PERF_LOG_PRINT_LIMIT 1

#ifdef CFG_BUILD_DEBUG
#define HDC_PERF_LOG_RUN_INFO_LIMIT(format, ...)                                                          \
    do {                                                                                                  \
        static int __drv_log_cnt = 0;                                                                     \
        if (hdc_ub_perf_log_not_limit() ||                                                                \
            !drv_log_rate_limit(&__drv_log_cnt, HDC_PERF_LOG_LIMIT_BRANCH_RATE, HDC_PERF_LOG_LIMIT_TIME)) \
            DRV_RUN_INFO(HAL_MODULE_TYPE_HDC, format, ##__VA_ARGS__);                                     \
    } while (0)
#else
#define HDC_PERF_LOG_RUN_INFO_LIMIT(format, ...)                                                          \
    do {                                                                                                  \
        static int __drv_log_cnt = 0;                                                                     \
        if (!drv_log_rate_limit(&__drv_log_cnt, HDC_PERF_LOG_LIMIT_BRANCH_RATE, HDC_PERF_LOG_LIMIT_TIME)) \
            DRV_RUN_INFO(HAL_MODULE_TYPE_HDC, format, ##__VA_ARGS__);                                     \
    } while (0)
#endif

#define HDC_PERF_LOG_RUN_INFO(limit, format, ...)                     \
    do {                                                              \
        if (limit)                                                    \
            HDC_PERF_LOG_RUN_INFO_LIMIT(format, ##__VA_ARGS__);       \
        else                                                          \
            DRV_RUN_INFO(HAL_MODULE_TYPE_HDC, format, ##__VA_ARGS__); \
    } while (0)

#define HDC_POLL_JFC_LOG_ERROR(limit, format, ...)                                                                \
    do {                                                                                                          \
        static int __log_cnt = 0;                                                                                 \
        if (limit) {                                                                                              \
            if (!drv_log_rate_limit(&__log_cnt, HDC_POLL_JFC_LOG_LIMIT_BRANCH_RATE, HDC_POLL_JFC_LOG_LIMIT_TIME)) \
                HDC_LOG_ERR(format, ##__VA_ARGS__);                                                               \
        } else {                                                                                                  \
            HDC_LOG_ERR(format, ##__VA_ARGS__);                                                                   \
        }                                                                                                         \
    } while (0)

bool hdc_ub_perf_log_not_limit(void);
void hdc_ub_perf_log_env_init(void);
int hdc_recv_data_in_event_handle(struct hdc_ub_epoll_node *node);
int hdc_ub_epoll_thread_init(void);
void hdc_ub_epoll_thread_uninit(void);
int hdc_ub_add_ctl_to_thread_epoll(struct hdc_ub_session *session);
void hdc_ub_del_ctl_to_thread_epoll(struct hdc_ub_session *session);
bool hdc_ub_is_epoll_thread_created(void);
signed int hdc_ub_get_epoll_thread_epfd(void);
unsigned long long hdc_ub_get_epoll_recv_abnormal_cnt(void);
int hdc_poll_jfc(hdc_urma_jfc_t *hdc_jfc, urma_cr_t *cr, struct hdc_ub_session *session, int tx_rx_flag,
                 int service_type);
void hdc_ack_jfc(hdc_urma_jfc_t *hdc_jfc);
uint8_t hdc_ub_get_jfs_priority_by_type(int service_type);
int hdc_mem_res_init(struct hdc_mem_res_info *mem_info, int service_type);
void hdc_mem_res_uninit(struct hdc_mem_res_info *mem_info, unsigned int dev_id);
int hdc_register_share_urma_seg(struct hdcConfig *hdc_config, unsigned int dev_id, unsigned int token_val);
void hdc_unregister_share_urma_seg(urma_target_seg_t *tseg);
int hdc_alloc_token_id(urma_context_t *urma_ctx, bool is_global, urma_token_id_t **token_id);
void hdc_free_token_id(urma_token_id_t *token_id);
int hdc_register_own_urma_seg(hdc_ub_context_t *ctx, unsigned long long len, unsigned long long va, int service_type,
                              struct hdc_time_record_for_urma_init *record);
void hdc_unregister_own_urma_seg(urma_target_seg_t *tseg, void *token_id_node, uint32_t dev_id, int service_type);
int hdc_register_dev_dmp_urma_seg(hdc_ub_context_t *ctx, unsigned long long len, unsigned long long va,
                                  int service_type);
void hdc_unregister_dmp_urma_seg(urma_target_seg_t *tseg, int service_type);
void hdc_token_id_list_init(void);

int hdc_process_thread_init(unsigned int dev_id, const char *func_name);
void hdc_process_thread_uninit(unsigned int dev_id);

void hdc_ub_get_urma_dfx(struct hdc_ub_session *session, hdc_urma_jfs_t *hdc_jfs, hdc_urma_jfr_t *hdc_jfr,
                         hdc_urma_jfc_t *hdc_jfc);
void hdc_ub_get_urma_dfx_by_flag(struct hdc_ub_session *session, hdc_ub_context_t *ctx, int hdc_rx_tx_flag);
// used for EMU_ST
#ifdef EMU_ST
int hdc_mem_res_init_stub(int fd, unsigned long long *user_va, unsigned long size);
void hdc_mem_res_uninit_stub(int fd, unsigned long long *user_va);
#define HDC_ERR_HEX_DUMP(buf, len, fmt, ...)                           \
    do {                                                               \
        const unsigned char *data_ptr = (const unsigned char *)(buf);  \
        size_t remain = (len);                                         \
        size_t offset = 0;                                             \
        char line_buf[129];                                            \
                                                                       \
        if ((data_ptr == NULL) || (remain == 0)) {                     \
            break;                                                     \
        }                                                              \
                                                                       \
        DRV_ERR(HAL_MODULE_TYPE_HDC, fmt " %s\n", ##__VA_ARGS__, buf); \
    } while (0)
#else
#define HDC_ERR_HEX_DUMP(buf, len, fmt, ...) DRV_ERR_HEX_DUMP(HAL_MODULE_TYPE_HDC, buf, len, fmt, ##__VA_ARGS__)
#endif

#endif // CFG_FEATURE_SUPPORT_UB

signed int __attribute__((weak)) hdc_ub_get_session_attr(mmProcess handle, const struct hdc_session *p_session,
                                                         int attr, int *value);
signed int __attribute__((weak)) hdc_ub_server_create(mmProcess handle, signed int dev_id, signed int service_type,
                                                      struct hdc_server_head *p_head);
hdcError_t __attribute__((weak)) hdc_ub_server_destroy(struct hdc_server_head *p_serv, signed int dev_id,
                                                       signed int service_type);
signed int __attribute__((weak)) hdc_ub_client_destroy(mmProcess handle, signed int devId, signed int serviceType);
signed int __attribute__((weak)) hdc_ub_accept(struct hdc_server_head *p_serv, signed int dev_id,
                                               signed int service_type, struct hdc_session *p_session);
signed int __attribute__((weak)) hdc_ub_connect(signed int dev_id, struct hdc_client_head *p_head, signed int peer_pid,
                                                struct hdc_session *p_session);
mmProcess __attribute__((weak)) hdc_ub_open(void);
void __attribute__((weak)) hdc_ub_close(mmProcess handle);
signed int __attribute__((weak)) hdc_ub_session_close(unsigned int dev_id, struct hdc_session *p_session,
                                                      int close_state, int close_flag);
signed int __attribute__((weak)) hdc_ub_send(const struct hdc_session *p_session, struct drvHdcMsg *p_msg,
                                             signed int wait, unsigned int timeout);
signed int __attribute__((weak)) hdc_ub_recv_peek(const struct hdc_session *p_session, signed int *len,
                                                  struct hdc_recv_config *recv_config);
signed int __attribute__((weak)) hdc_ub_recv(const struct hdc_session *p_session, char *buf, signed int len,
                                             signed int *out_len, struct hdc_recv_config *recv_config);
signed int __attribute__((weak)) hdc_ub_set_session_owner(const struct hdc_session *p_session);
signed int __attribute__((weak)) HdcUbGetSessionUid(mmProcess handle, const struct hdc_session *pSession,
                                                    int *root_privilege);

void __attribute__((weak)) hdc_ub_init(struct hdcConfig *hdc_config);
void __attribute__((weak)) hdc_ub_uninit(struct hdcConfig *hdc_config);
drvError_t __attribute__((weak)) hdc_connect_event_proc(uint32_t dev_id, void *msg, uint32_t msg_len);
drvError_t __attribute__((weak)) hdc_dfx_query_event_proc(uint32_t dev_id, void *msg, uint32_t msg_len);
drvError_t __attribute__((weak)) hdc_remote_close_proc(uint32_t dev_id, void *msg, uint32_t msg_len);
void __attribute__((weak)) hdc_release_remote_session(void);
signed int __attribute__((weak)) hdc_link_event_pre_init(unsigned int dev_id, const char *func_name);
void __attribute__((weak)) hdc_link_event_pre_uninit(signed int dev_id);
const char *__attribute__((weak)) hdc_get_sevice_str(int service_type);
int __attribute__((weak)) hdc_get_lock_index(int dev_id, int session_id);
struct hdc_ub_session *__attribute__((weak)) hdc_find_session_in_list(unsigned int fd, int dev_id, uint32_t unique_val,
                                                                      int idx);
int __attribute__((weak)) hdc_ub_get_session_dfx(unsigned int dev_id, struct hdc_ub_session *session);
void __attribute__((weak)) hdc_ub_fill_jetty_info(hdcdrv_jetty_info_t *info, struct hdc_ub_session *session);
hdcError_t __attribute__((weak)) hdc_ub_notify_register(int service_type, struct HdcSessionNotify *notify);
void __attribute__((weak)) hdc_ub_notify_unregister(int service_type);
signed int __attribute__((weak)) hdc_ub_get_peer_devId(mmProcess handle, signed int dev_id, signed int *peer_dev_id);
signed int __attribute__((weak)) hdc_ub_ioctl(mmProcess handle, signed int ioctl_code, union hdcdrv_cmd *hdc_cmd);
void __attribute__((weak)) hdc_fill_event_msg(struct hdcdrv_event_msg *msg, union hdcdrv_cmd *hdc_cmd,
                                              struct hdc_ub_session *session, enum hdcdrv_notify_type notify_type);
void __attribute__((weak)) hdc_fill_send_ctrl_msg_cmd(union hdcdrv_cmd *hdc_cmd, int dev_id, void *event_msg,
                                                      size_t msg_len);
signed int __attribute__((weak)) hdc_send_ctrl_msg(mmProcess fd, union hdcdrv_cmd *hdc_cmd);
void __attribute__((weak)) hdc_ub_fill_send_recv_info(hdc_ub_send_recv_info_t *send_recv_info,
                                                      struct hdc_ub_session *session);
#endif
