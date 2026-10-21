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

#ifndef TRS_H2D_MSG_H
#define TRS_H2D_MSG_H

#include "drv_type.h"

#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
#include "uba/ubcore_adapt.h"
#endif

/* trs_msg.h */
struct trs_msg_head {
    u32 devid;
    u32 tsid;
    u32 cmdtype;
    u16 valid;  /* validity judgement, 0x5A5A is valid */
    s16 result; /* process result from rp, zero for succ, non zero for fail */
};

#define TRS_MSG_TOTAL_LEN 512
#define TRS_MSG_DATA_LEN (TRS_MSG_TOTAL_LEN - sizeof(struct trs_msg_head) - sizeof(u64))

#define TRS_MSG_SEND_MAGIC 0x5A5A
#define TRS_MSG_RCV_MAGIC 0xA5A5
#define TRS_MSG_INVALID_RESULT 0x1A

struct trs_msg_data {
    struct trs_msg_head header;
    u64 data_len; /* u64 for 8 Bytes align */
    char payload[TRS_MSG_DATA_LEN];
};

enum trs_msg_cmd_type {
    TRS_MSG_GET_SSID = 1,
    TRS_MSG_PUT_RES_ID,
    TRS_MSG_GET_RES_ID,
    TRS_MSG_GET_RES_CAP,
    TRS_MSG_GET_PHY_ADDR,
    TRS_MSG_GET_CQ_GROUP,
    TRS_MSG_GET_PROC_NUM,
    TRS_MSG_SQ_REG_MAP,
    TRS_MSG_SQ_REG_UNMAP,
    TRS_MSG_CDQM_INIT,
    TRS_MSG_CDQM_CREATE,
    TRS_MSG_CDQM_DESTROY,
    TRS_MSG_CDQM_BATCH_ABNORMAL,
    TRS_MSG_GET_RES_AVAIL_NUM,
    TRS_MSG_CHAN_ABNORMAL,
    TRS_MSG_SET_TS_STATUS,
    TRS_MSG_FLUSH_RES_ID,
    TRS_MSG_RES_ID_CHECK,
    TRS_MSG_RES_ID_GET,
    TRS_MSG_RES_ID_PUT,
    TRS_MSG_SQ_RSVMEM_MAP,
    TRS_MSG_SQ_RSVMEM_UNMAP,
    TRS_MSG_INIT_JETTY,
    TRS_MSG_UNINIT_JETTY,
    TRS_MSG_NOTICE_TS,
    TRS_MSG_TS_RPC_CALL,
    TRS_MSG_TS_CQ_PROCESS,
    TRS_MSG_RAS_REPORT,
    TRS_MSG_NOTICE_PROC_RECYCLE,
    TRS_MSG_RES_NUM_QUERY,
    TRS_MSG_MAX
};

struct trs_msg_sync_ssid {
    u32 hpid;
    u32 vfid;
    int ssid;
};

struct trs_msg_res_id_check {
    u32 hpid;
    int id_type;
    u32 res_id;
};

struct trs_msg_res_id_get {
    u32 hpid;
    int id_type;
    u32 res_id;
    u32 seq; /* global sequence for idempotency of remote get/put */
};

struct trs_msg_id_sync_head {
    int type;
    u32 flag;
    u16 req_num; // request id num
    u16 ret_num; // return id num
};

#define TRS_MSG_ID_SYNC_MAX_NUM 64
struct trs_msg_id_sync {
    struct trs_msg_id_sync_head head;
    u32 id[TRS_MSG_ID_SYNC_MAX_NUM];
    u32 num; /* id num of specified range */
};

struct trs_msg_id_cap {
    int type;
    u32 id_start;
    u32 id_end;
    u32 total_num;
    u32 split;
    u32 isolate_num;
};

#define SYNC_MAX_NAME_LEN 64
struct trs_msg_get_phy_addr {
    u64 addr;
    u32 size;
    char name[SYNC_MAX_NAME_LEN];
};

#define MAX_GROUP_NUM 16
struct trs_msg_cq_group {
    u32 group_num;
    u32 group[MAX_GROUP_NUM];
};

struct trs_msg_proc_num {
    u32 proc_num;
};

struct trs_msg_stream_bind_sqcq {
    u32 stream_id;
};

struct trs_msg_map_stars_sq {
    u64 va; /* stars va addr for cp proc  */
    size_t size;
};

struct trs_msg_sqcq_sync {
    u32 sq_id;
    u32 cq_id;
    int remote_tgid;
    struct trs_msg_stream_bind_sqcq bind_sqcq;
    struct trs_msg_map_stars_sq sq_map;
};

struct trs_msg_res_num {
    int type;
    u32 avail_num;
};

struct trs_msg_map_sq_mem {
    u32 phy_devid;
    u32 vfid;
    int remote_tgid;
    u64 dev_paddr; /* sq device rsv mem phy addr */
    size_t size;
    struct trs_msg_map_stars_sq sq_map;
};

struct trs_msg_proc_recycle {
    int pid;
};

#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
struct trs_msg_jetty_info {
    u32 cq_jfr_id;
    u32 sq_jfr_id;
    uba_ubcore_seg cq_seg;
    uba_ubcore_seg sq_seg;
    uba_ubcore_eid_info eid_info;
    uba_ubcore_seg notify_seg;
    uba_ubcore_seg cnt_notify_seg;
    uba_ubcore_seg sq_reg_seg;
    u32 die_id;
    u32 func_id;
    u32 vfid;
    u32 token_value;
    u32 rmt_token_value;
    u32 rsv[4];
};

struct trs_msg_unbind_jetty {
    u32 vfid;
    u32 rsv[4];
};
#endif

/* trs_abnormal_info.h */
typedef enum abnormal_task_type {
    ABNORMAL_TASK_TYPE_AICPU,
    ABNORMAL_TASK_TYPE_DVPP,
    ABNORMAL_TASK_TYPE_AICORE,
    ABNORMAL_TASK_TYPE_UNKNOW,
    ABNORMAL_TASK_TYPE_UB,
    ABNORMAL_TASK_TYPE_MAX
} ABNORMAL_TASK_TYPE;

typedef enum abnormal_err_type {
    ABNORMAL_ERR_TYPE_TASK_TIMEOUT,
    ABNORMAL_ERR_TYPE_CQ_FULL,
    ABNORMAL_ERR_TYPE_TASK_KILL,
    ABNORMAL_ERR_TYPE_ABNORMAL_ASYNC_COPY,
    ABNORMAL_ERR_TYPE_MAX
} ABNORMAL_ERR_TYPE;

#pragma pack(4)
struct aicpu_task_info {
    u32 dst_engine;
    u64 mb_bitmap;
};
#pragma pack()

#if defined(CFG_SOC_PLATFORM_CLOUD_V2) || defined(CFG_SOC_PLATFORM_CLOUD_V4)
#define TRS_ICM_MSG_DATA_LENGTH 86
#else
#define TRS_ICM_MSG_DATA_LENGTH 22
#endif

struct stars_abnormal_info {
    u32 vfid;
    u16 sqid;
    u16 sqe_id; /* prepare for future */
    u16 task_id;
    u8 task_type;
    u8 err_type;
    union {
        struct aicpu_task_info aicpu_info;
        u8 abnormal_data[TRS_ICM_MSG_DATA_LENGTH - 12];
    };
};

#define TRS_MAINT_MAX_CQE_SIZE 64

struct trs_msg_ts_cq_process {
    int cq_type;
    u32 cqid;
    u8 cqe[TRS_MAINT_MAX_CQE_SIZE];
};

/* trs_mailbox_def.h start */
#define TRS_MBOX_NOTICE_ACK_IRQ_VALUE 0U
#define TRS_MBOX_CREATE_CQSQ_CALC 1U  // normal sqcq alloc
#define TRS_MBOX_RELEASE_CQSQ_CALC 2U // normal sqcq free
#define TRS_MBOX_LOG_CQSQ_CREATE 3U
#define TRS_MBOX_LOG_CQSQ_RELEASE 4U
#define TRS_MBOX_DBG_CQSQ_CREATE 5U
#define TRS_MBOX_DBG_CQSQ_RELEASE 6U
#define TRS_MBOX_CREATE_PROF_SQCQ 7U
#define TRS_MBOX_RELEASE_PROF_SQCQ 8U
#define TRS_MBOX_CREATE_HB_SQCQ 9U
#define TRS_MBOX_RELEASE_HB_SQCQ 10U
#define TRS_MBOX_SEND_RDMA_INFO 15U
#define TRS_MBOX_RESET_NOTIFY 16U
#define TRS_MBOX_RECYCLE_PID 18U
#define TRS_MBOX_RECORD_NOTIFY 19U
#define TRS_MBOX_CREATE_TASKSCHED_SQCQ 24U
#define TRS_MBOX_RELEASE_TASKSCHED_SQCQ 25U
#define TRS_MBOX_CREATE_CB_CQ 26U
#define TRS_MBOX_RELEASE_CB_CQ 27U
#define TRS_MBOX_CREATE_MIA 30U
#define TRS_MBOX_DESTROY_MIA 31U
#define TRS_MBOX_RESET_EVENT_ID 32U
#define TRS_MBOX_SHM_SQCQ_ALLOC 33U
#define TRS_MBOX_SHM_SQCQ_FREE 34U
#define TRS_MBOX_LOGIC_CQ_ALLOC 35U
#define TRS_MBOX_LOGIC_CQ_FREE 36U
#define TRS_MBOX_CREATE_TOPIC_SQCQ 37U  // esched alloc
#define TRS_MBOX_RELEASE_TOPIC_SQCQ 38U // esched alloc
#define TRS_MBOX_RES_MAP 39U
#define TRS_MBOX_RECYCLE_CHECK 40U
#define TRS_MBOX_ALLOC_STREAM 41U
#define TRS_MBOX_FREE_STREAM 42U
#define TRS_MBOX_CREATE_KERNEL_SQCQ 43U  // dvpp alloc
#define TRS_MBOX_RELEASE_KERNEL_SQCQ 44U // dvpp free
#define TRS_MBOX_NOTICE_SSID 45U         // notice ssid to tsfw from device
#define TRS_MBOX_QUERY_SSID 46U          // notice ssid to tsfw from device
#define TRS_MBOX_NOTICE_TS_SQCQ_CREATE 47U
#define TRS_MBOX_NOTICE_TS_SQCQ_FREE 48U
#define TRS_MBOX_CREATE_CTRL_CQSQ 49U
#define TRS_MBOX_RELEASE_CTRL_CQSQ 50U
#define TRS_MBOX_NOTICE_SQ_TRIGGER 51U
#define TRS_MBOX_RPC_CALL 52U
#define TRS_MBOX_CONFIG_STARS_URPC 53U
#define TRS_MBOX_SQ_TASK_SEND 54U
#define TRS_MBOX_CQ_REPORT_RECV 55U
#define TRS_MBOX_QUERY_SQ_STATUS 56U
#define TRS_MBOX_QUERY_CORE_RATE_START 57U
#define TRS_MBOX_QUERY_CORE_RATE_END 58U
#define TRS_MBOX_CORE_RATE_SHARE_MEMORY 59U
#define TRS_MBOX_MEM_DISPATCH 60U
#define TRS_MBOX_DSMI_RPC_CALL 61U
#define TRS_MBOX_NOTICE_TS_CMDLIST_INFO 62U
#define TRS_MBOX_SQ_SWITCH_STREAM 64U
#define TRS_MBOX_CMD_MAX 65U

#define TRS_MBOX_INVALID_INDEX 0xFFFF
#define TRS_MBOX_MESSAGE_VALID 0x5A5A

#define TRS_MBOX_SEND_FROM_DEVICE 0
#define TRS_MBOX_SEND_FROM_HOST 1

#define TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS 3000

#define TSFW_MAILBOX_SIZE 64U

struct trs_mb_header {
    u16 valid;    /* validity judgement, 0x5a5a is valid */
    u16 cmd_type; /* identify command or operation */
    u32 result;   /* TS's process result succ or fail: no error: 0, error: not 0 */
};

#define SQCQ_INFO_LENGTH 5
#pragma pack(4)
struct trs_sqcq_ext_info {
    u32 ext_msg_len;
    void *ext_msg;
    u32 *info; /* len is SQCQ_INFO_LENGTH */
};
#pragma pack()
struct trs_normal_cqsq_mailbox {
    struct trs_mb_header header;

    u64 sq_addr; /* invalid addr: 0x0 */
    u64 cq0_addr;

    u16 sq_index;  /* invalid idx: 0xFFFF */
    u16 cq0_index; /* sq's return */

    u8 app_type : 1;    /* inform TS, app is in host or device, device: 0 host: 1 */
    u8 sw_reg_flag : 1; /* 1: sq saves head and tail in share memory at the last 2 sqe, 0: not save */
    u8 fid : 6;         /* 0:host, 1~16:virt machine */

    u8 sq_cq_side : 2; /* bit 0 sq side, bit 1 cq side. device: 0 host: 1  */
    u8 master_pid_flag : 1;
    u8 sq_addr_is_virtual : 1;
    u8 cq_addr_is_virtual : 1;
    u8 is_convert_pid : 1;
    u8 rsv : 2;

    u8 sqesize;
    u8 cqesize; /* calculation cq's slot size, default: 12 bytes */
    u16 cqdepth;
    u16 sqdepth;
    pid_t pid;
    u16 cq_irq;
    u16 ssid;

    union {
        u32 info[SQCQ_INFO_LENGTH];
        struct trs_sqcq_ext_info ts_info;
    };
};

struct trs_alloc_stream_mbox {
    struct trs_mb_header header;
    u32 priority;
    u32 stream_id;
    u32 vf_id;
    u32 pid;
    u8 rsv[40];
};

struct trs_maint_sqcq_mbox {
    struct trs_mb_header header;

    u64 sq_addr; /* invalid addr: 0x0 */
    u64 cq0_addr;
    u64 cq1_addr;  /* cq1_addr or sq size */
    u64 cq2_addr;  /* cq2_addr or cq size */
    u64 cq3_addr;  /* reserved or pid */
    u16 sq_index;  /* invalid idx: 0xFFFF */
    u16 cq0_index; /* sq's return */
    u16 cq1_index; /* ts's return */
    u16 cq2_index; /* ai cpu's return */
    u16 cq3_index; /* reserved */
    u16 cq_irq;
    u8 plat_type;    /* inform TS, msg is sent from host or device, device: 0 host: 1 */
    u8 cq_slot_size; /* calculation cq's slot size, default: 12 bytes */
};

struct trs_task_sched_sqcq_alloc_mbox {
    struct trs_mb_header header;
    u64 sq_addr;
    u64 cq_addr;
    u32 sq_index; /* sq idx */
    u32 cq_index;
    u16 sqe_size;
    u16 cqe_size;
    u16 sq_depth; /* sq depth */
    u16 cq_depth;
    u8 plat_type;
    u8 reserved[3]; /* reserved */
    u32 cq_irq;
    u8 rsv[16];
};

struct trs_task_sched_sqcq_free_mbox {
    struct trs_mb_header header;
    u32 sq_index;
    u32 cq_index;
    u8 plat_type;
    u8 reserved[3]; /* reserved */
    u8 rsv[44];
};

struct trs_cb_cq_mbox {
    struct trs_mb_header header;
    u32 vpid;
    u32 grpid;
    u32 logic_cqid;
    u32 phy_cqid;
    u32 cq_irq;
    u32 phy_sqid;
    u8 plat_type;
    u8 reserved[3]; /* reserved */
    u8 rsv[28];
};

#pragma pack(4)
struct trs_ts_sqcq_mbox {
    struct trs_mb_header header;
    u32 sqid;
    u32 cqid;
    u32 vfid;
    pid_t pid;
    u16 ssid;
    u16 app_type : 1; /* inform TS, msg is sent from host or device, device: 0 host: 1 */
    u16 rsv : 15;
    union {
        u32 info[SQCQ_INFO_LENGTH];
        struct {
            u64 sq_addr;
            u64 cq_addr;
            u16 sqesize;
            u16 cqesize;
            u16 sqdepth;
            u16 cqdepth;
        };
    };
    u8 rsv1[12];
};
#pragma pack()

struct trs_shm_sqcq_mbox {
    struct trs_mb_header header;

    u64 sq_addr; /* invalid addr: 0x0 */
    u64 cq_addr;

    u16 sq_id; /* invalid idx: 0xFFFF */
    u16 cq_id; /* sq's return */

    u8 app_type : 2; /* inform TS, msg is sent from host or device, device: 0 host: 1 */
    u8 fid : 6;      /* 0:hsot, 1~16:virt machine */
    u8 sq_cq_side;   /* bit 0 sq side, bit 1 cq side. device: 0 host: 1  */

    u8 sqesize;
    u8 cqesize; /* calculation cq's slot size, default: 12 bytes */
    u16 cqdepth;
    u16 sqdepth;
    pid_t pid;
    u32 cq_irq;
    u32 info[SQCQ_INFO_LENGTH];
};

struct trs_logic_cq_create_mbox {
    u64 phy_cq_addr;
    u16 cqe_size;
    u16 cq_depth;
    u32 vpid;
    u16 logic_cqid;
    u16 phy_cqid;
    u16 cq_irq;
    u8 app_flag;
    u8 thread_bind_irq_flag;
    u8 vfid;
    u8 rsv[3];
    u32 info[SQCQ_INFO_LENGTH];
};

struct trs_logic_cq_release_mbox {
    u32 vpid;
    u16 logic_cqid;
    u16 phy_cqid;
    u8 vfid;
    u8 rsv[3];
};

struct trs_logic_cq_mbox {
    struct trs_mb_header header;
    union {
        struct trs_logic_cq_create_mbox mb_alloc;
        struct trs_logic_cq_release_mbox mb_free;
    };
};

struct trs_rpc_call_header {
    u32 pid;
    u8 app_flag;
    u8 vfid;
    u8 len; /* in, out */
    u8 rsv[5];
};

#define TRS_RPC_MAX_DATA_LEN 44
struct trs_rpc_call_msg {
    struct trs_mb_header header;
    struct trs_rpc_call_header rpc_call_header;
    u8 data[TRS_RPC_MAX_DATA_LEN];
};

struct trs_event_msg {
    struct trs_mb_header header;
    u32 event_id;
};

struct trs_mem_dispatch_msg {
    struct trs_mb_header header;
    void *addr;
    u64 size;
    u32 hostpid;
    u8 vfid;
    u8 is_addr_ext; /* 0 use paddr 1 use ext_addr */
    u8 flag;        /* 0 disable 1 enable */
    u8 rsv;
    u32 addr_len;
};

#define TRS_MBOX_NOTIFY_TYPE 0
#define TRS_MBOX_EVENT_TYPE 1
#define TRS_MBOX_CNT_NOTIFY_TYPE 2
struct trs_notify_msg {
    struct trs_mb_header header;
    u32 phy_notifyId;
    u32 tgid;
    u16 plat_type;
    u16 notifyId; /* ts use for convert id to notifyid */
    u8 fid;
    u8 notify_type;
    u8 reserved[42];
};

struct trs_stream_msg {
    struct trs_mb_header header;
    u32 stream_id;
};

struct trs_res_map_msg {
    struct trs_mb_header header;
    u8 vf_id;
    u8 resource_type;  // 0:notify id, 1:event id
    u8 operation_type; // 0:map, 1:unmap
    u8 reserve0;
    u16 id;
    u16 phy_id;
    u32 host_pid;
    u8 reserve[44];
};

struct trs_reset_event_id_msg {
    struct trs_mb_header header;
    u32 event_id;
};

struct trs_accelerator_util_mailbox {
    struct trs_mb_header header;
    u32 core_type;
    u32 reserved; /* reserved */
};

struct trs_mbox_ack_irq_msg {
    struct trs_mb_header header;
    u32 ack_irq;
};

struct trs_ssid_msg {
    struct trs_mb_header header;
    u8 vfid;
    u8 vfgid;
    u16 ssid;
    u32 hostpid;
};

#define MAX_RDMA_INFO_LEN 56
struct trs_rdma_info {
    struct trs_mb_header header;
    u8 buf[MAX_RDMA_INFO_LEN];
};

/* inform   pid to ts recycle resource */
#define MAX_INFORM_PID_INFO 12
struct exit_proc_info {
    u32 app_cnt;
    int pid[MAX_INFORM_PID_INFO];
    u8 plat_type;
    u8 fid;
    u8 reserved[2];
};

struct recycle_proc_msg {
    struct trs_mb_header header;
    struct exit_proc_info proc_info;
};

struct trs_mia_cfg_msg {
    struct trs_mb_header header;
    u8 vfid;
    u8 rsv[3];
};

struct trs_sq_trigger_msg {
    struct trs_mb_header header;
    u32 db;
    u32 irq;
    u8 vfid;
    u8 rsv[3];
};

struct trs_urpc_jetty_config {
    u16 jetty_ci;
    u16 jetty_pi;
    u16 jetty_length_shift;
    u64 jetty_base_addr;
    u64 doorbell_addr;
    u32 jetty_id;
    u8 func_id;
    u8 die_id;

    u32 trans_obj_id : 24;
    u32 rsv : 2;
    u32 odr : 3;
    u32 token_en : 1;
    u32 rmt_jetty_type : 2;

    u32 rmt_jetty_or_seg_id;

    u64 rmt_eid_low;
    u64 rmt_eid_high;

    u32 rmt_token_value;

    union {
        u8 int_mode;      // cqe
        u64 sq_base_addr; // sq head
    };

    u32 urpc_aw_wqe_setting_0;
    u32 urpc_db_wqe_setting_0;
    u8 urpc_aw_wqe_setting_1;
    u8 urpc_db_wqe_setting_1;
};

struct trs_urpc_config_mailbox {
    struct trs_mb_header header;

    struct trs_urpc_jetty_config *cq_jetty_info;
    struct trs_urpc_jetty_config *sq_jetty_info;
    u32 vfid;
};

#define TRS_MAINT_MAX_SQ_DEPTH 4
#define TRS_MAINT_MAX_CQ_DEPTH 4
#define TRS_MAINT_MAX_SQE_SIZE 64
#define TRS_MAINT_MAX_CQE_SIZE 64
struct trs_sq_task_send_mailbox {
    struct trs_mb_header header;

    u32 sq_type;
    u32 sqid;
    u32 sq_tail;
    u32 pos;
    u8 sqe[TRS_MAINT_MAX_SQE_SIZE];
};

struct trs_cq_report_recv_mailbox {
    struct trs_mb_header header;

    u32 cq_type;
    u32 cqid;
    u32 cq_head;
};

struct trs_ts_cmdlist_info_mailbox {
    struct trs_mb_header header;

    uint32_t op;
    u64 cmdlist_kva;
    u64 cmdlist_uva;
    u32 ssid;
};

#pragma pack(4)
#define TRS_MAX_SWITCH_NODE_CNT 3
struct trs_sq_switch_stream_node {
    u16 sq_id;
    u16 sq_depth;
    u32 stream_id;
    u64 sq_addr;
};

struct trs_sq_switch_stream_info {
    struct trs_mb_header header;

    u32 cnt;
    struct trs_sq_switch_stream_node nodes[TRS_MAX_SWITCH_NODE_CNT];
    u8 is_virtual[TRS_MAX_SWITCH_NODE_CNT];
    u8 rsv;
};
#pragma pack()
/* trs_mailbox_def.h end */

#endif
