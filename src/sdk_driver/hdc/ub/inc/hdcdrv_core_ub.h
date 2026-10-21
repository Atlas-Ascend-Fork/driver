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
#ifndef _HDCDRV_CORE_H_
#define _HDCDRV_CORE_H_

#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"

#include "ascend_hal_define.h"
#include "esched_kernel_interface.h"
#include "hdcdrv_cmd_enum.h"
#include "hdcdrv_mem_com_ub.h"
#include "hdcdrv_cmd_ioctl.h"
#include "hdcdrv_cmd_msg.h"
#include "hdcdrv_adapt_ub.h"
#include "hdcdrv_ctrl_msg.h"

#define HDC_MODULE_NAME "HISI_HDC"
#define HDCDRV_STR_NAME_LEN 32
#define HDCDRV_SESSION_FD_INVALID (-1)
#define HDCDRV_SERVICE_TYPE_INVALID (-1)
#define HDCDRV_INVALID_REMOTE_SESSION_ID (-2)

#define HDCDRV_SERVER_PROCESS_MAX_NUM 64

#define HDCDRV_SERVICE_SCOPE_GLOBAL 0
#define HDCDRV_SERVICE_SCOPE_PROCESS 1

#define HDCDRV_SERVICE_LOG_LIMIT 1
#define HDCDRV_SERVICE_NO_LOG_LIMIT 0

#define HDCDRV_SERVICE_HIGH_LEVEL 0
#define HDCDRV_SERVICE_LOW_LEVEL 1

#define HDCDRV_SERVICE_SHORT_CONN 0
#define HDCDRV_SERVICE_LONG_CONN 1
#define HDCDRV_SERVICE_SHORT_CONN_EXTEND 2

#define HDCDRV_SESSION_STATUS_IDLE 0
#define HDCDRV_SESSION_STATUS_CONN 1
#define HDCDRV_SESSION_STATUS_CLOSING 2

#define HDCDRV_DELAY_REMOTE_CLOSE_BIT 0

#define HDCDRV_SESSION_UNIQUE_VALUE_MASK 0x3FFFFFFFU
#define HDCDRV_SESSION_UNIQUE_VALUE_HOST_FLAG 0x80000000U
#define HDCDRV_SESSION_UNIQUE_VALUE_DEVICE_FLAG 0xC0000000U

#ifdef KA_VM_FAULT_SIGSEGV
#define HDCDRV_FAULT_ERROR KA_VM_FAULT_SIGSEGV
#else
#define HDCDRV_FAULT_ERROR KA_VM_FAULT_SIGBUS
#endif

#define HDCDRV_HOTRESET_CHECK_MAX_CNT 5000
#define HDCDRV_HOTRESET_CHECK_DELAY_MS 40

#define HDCDRV_INVALID_OWNER_PID 0xffffffffffffffffU

#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA)
#define HDCDRV_CONN_TIMEOUT (100 * KA_HZ)                 /* 100s for fpga */
#define HDCDRV_SESSION_REMOTE_CLOSED_TIMEOUT (10 * KA_HZ) /* 10s for fpga */
#define HDCDRV_SESSION_REMOTE_CLOSED_TIMEOUT_MS 10000     /* 10s for fpga */
#else
#define HDCDRV_CONN_TIMEOUT (30 * KA_HZ)                 /* 30s */
#define HDCDRV_SESSION_REMOTE_CLOSED_TIMEOUT (3 * KA_HZ) /* 3s */
#define HDCDRV_SESSION_REMOTE_CLOSED_TIMEOUT_MS 3000     /* 3s */
#endif

#define HDCDRV_WAKE_UP_WAIT_TIMEOUT 10000 /* 10s */

#define HDCDRV_SESSION_RECLAIM_TIMEOUT (60 * KA_HZ) /* 60s */

#define HDCDRV_NON_TRANS_RETRY_TIME 5

struct hdcdrv_ctx;

struct hdcdrv_notify_ts {
    u64 entry_ns;
    u64 search_end_ns;
    u64 reserved[5];
};

struct hdcdrv_conn_info_list {
    struct hdcdrv_conn_info_list *next;
    struct hdcdrv_event_connect conn_info;
    u64 add_list_jiffies;
    struct hdcdrv_notify_ts notify_ts;
};

struct hdcdrv_service {
    int listen_status;
    u64 listen_pid;
    u32 dev_id;
    int level;
    int service_scope;
    ka_list_head_t serv_list;
    struct hdcdrv_ctx *ctx;
    ka_mutex_t mutex;
    struct hdcdrv_conn_info_list *conn_list; // Used to store conn_info from client
};

struct hdcdrv_serv_list_node {
    struct hdcdrv_service service;
    ka_list_head_t list;
};

struct hdcdrv_event_record {
    struct hdcdrv_event_msg msg;
    int wait_flag;
    int valid_flag;
    u64 reply_wake_up_ns;
};

struct hdcdrv_session {
    ka_atomic_t status;
    u32 inner_checker;
    int local_session_fd;
    int remote_session_fd;
    int dev_id;
    int service_type;
    u32 delay_work_flag;
    int run_env;
    u64 create_pid;
    u64 peer_create_pid;
    u64 owner_pid;
    int remote_close_state;
    int local_close_state;
    u32 unique_val;
    int euid;
    int uid;
    int root_privilege;
    u32 container_id;
    u64 remote_close_jiff;
    struct hdcdrv_service *service;
    struct hdcdrv_ctx *ctx;
    ka_task_spinlock_t lock;
    bool remote_close_flag; // true means session has not closed, false means session has been close by remote or close
    int mem_pool_idx;
    ka_delayed_work_t remote_close;
    ka_wait_queue_head_t wq_conn;
    ka_wait_queue_head_t wq_dfx;
    struct hdcdrv_event_record conn_reply;
    struct hdcdrv_event_record dfx_reply;
    struct hdcdrv_notify_ts conn_notify_ts;
};

struct hdcdrv_ctx_mem_pool_info {
    ka_page_t *pool_page;
    u32 dev_ref;
    int idx;
};

struct hdcdrv_ctx {
    int dev_id;
    int service_type;
    int session_fd;
    u64 pid;
    u32 fid;
    int refcnt;
    struct hdcdrv_ctx_mem_pool_info pool_info;
    ka_mutex_t ctx_lock;
    struct hdcdrv_ctrl_msg_node *ctrl_node;
};

struct hdcdrv_dev {
    u32 valid;
    u32 dev_id;
    u32 dev_type; // Device type, which may require distinguishing between pf and vf
    struct hdcdrv_service service[HDCDRV_SUPPORT_MAX_SERVICE];
    struct hdcdrv_session *sessions;
    int cur_alloc_session;              // The currently assigned long session location
    int cur_alloc_short_session;        // The currently assigned short session location
    int cur_alloc_short_extend_session; // The currently assigned short extend session location (264-271)
    struct hdcdrv_mem_pool_list_node mem_pool_list[HDCDRV_MEM_POOL_NUM]; // idx of mem_pool for each dev
    ka_atomic_t ref;
    u32 host_pm_or_vm_flag;
    struct hdcdrv_event_msg_pool event_msg_pool;
    void *ctrl_msg_chan;
};

struct hdcdrv_service_attr {
    int level;
    int conn_feature;
    int service_scope;
    int log_limit;
};

struct hdcdrv_ctrl {
    ka_atomic_t unique_val;
    struct hdcdrv_dev *dev[HDCDRV_SUPPORT_MAX_DEV];
    struct hdcdrv_service_attr service_attr[HDCDRV_SUPPORT_MAX_SERVICE];
    ka_mutex_t mutex;
    ka_mutex_t dev_lock[HDCDRV_SUPPORT_MAX_DEV];
    u32 dev_ref[HDCDRV_SUPPORT_MAX_DEV];
};

extern struct hdcdrv_ctrl g_hdc_ctrl;

struct hdcdrv_sync_event_msg {
    struct event_sync_msg head;
    struct hdcdrv_event_msg data;
};

static inline void hdcdrv_set_session_status(struct hdcdrv_session *session, int status)
{
    ka_base_atomic_set(&session->status, status);
}
static inline int hdcdrv_get_session_status(const struct hdcdrv_session *session)
{
    return ka_base_atomic_read(&session->status);
}

int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);

struct hdcdrv_dev *hdcdrv_ub_get_dev(u32 dev_id);
void *hdcdrv_ub_kvmalloc(size_t size, int level);
void hdcdrv_ub_kvfree(void **addr, int level);
void hdcdrv_put_dev(u32 dev_id);

int hdcdrv_ub_init_module(void);
void hdcdrv_ub_exit_module(void);
void hdcdrv_uninit_esched(void);
void hdcdrv_uninit_ub(void);
int hdcdrv_ub_ctrl_msg_recv(u32 dev_id, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
#endif // _HDCDRV_CORE_H_