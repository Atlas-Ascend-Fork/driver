/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef QUEUE_IOCTL_H
#define QUEUE_IOCTL_H

#include "queue_command_base.h"
#include "esched_kernel_interface_cmd.h"
#include "hal_pkg/esched_pkg.h"

#define DEV_QUEUE_STATUS_RECORE_START 10
#define QUEUE_FEATURE_DEV_NUM 1140

enum queue_status_time_type {
    HOST_START_MAKE_DMA_LIST = 0,
    HOST_END_MAKE_DMA_LIST,
    HOST_END_HDC_SNED,
    HOST_END_WAIT_REPLY,
    HOST_HDC_RECV,
    HOST_WAKE_UP,
    HOST_FINISH_QUEUE_MSG,

    DEV_START_SUBMIT_EVENT = DEV_QUEUE_STATUS_RECORE_START,
    DEV_END_SUBMIT_EVENT,
    DEV_START_MAKE_DMA_LIST,
    DEV_END_MAKE_DMA_LIST,
    DEV_END_DMA_COPY,
    DEV_END_REPLY,
    DEV_FINISH_QUEUE_MSG,
    TIME_RECORD_TYPE_MAX,
};

struct queue_ioctl_host_common_op {
    unsigned int devid;
    unsigned int op_flag;
};

struct queue_ioctl_enqueue {
    struct sched_published_event_info event_info;
    unsigned int devid;
    unsigned int qid;
    unsigned int type;
    int time_out;
    unsigned int iovec_count;
    struct buff_iovec *vector;
};

struct queue_ioctl_ctrl_msg_send {
    struct sched_published_event_info event_info;
    unsigned int devid;
    void *ctrl_data_addr;
    unsigned int ctrl_data_len;
    unsigned int host_timestamp;
};

struct queue_ioctl_copy {
    unsigned int op_flag;
    unsigned int devid;
    unsigned int qid;
    unsigned int type;
    unsigned int hostpid;
    void *ctx_addr;
    unsigned long long ctx_addr_len;
    void *addr;
    unsigned long long addr_len;
    unsigned int copy_flag;
    unsigned int count;
    struct iovec_info *ptr;
};

struct queue_ioctl_reply {
    unsigned int devid;
    unsigned int qid;
    int hostpid;
};

struct queue_init_para {
    unsigned int devid;
    int hostpid;
};

struct queue_ioctl_ctrl_msg_recv {
    struct queue_ctrl_data_search search_flag;
    void *ctrl_data_addr;
};

struct queue_ioctl_iovec_num {
    unsigned int devid;
    unsigned int qid;
    int hostpid;
    unsigned int num;
};

struct queue_mcast_para {
    unsigned int gid : 8;
    unsigned int mcast_flag : 1;
    unsigned int rsv : 1;
    unsigned int event_sn : 22;
};

struct queue_common_para {
    unsigned int devid : 16;
    unsigned int qid : 16;
    unsigned int host_timestamp;
    struct queue_mcast_para mcast_para;
};

struct queue_event_msg_head {
    struct event_sync_msg sync;
    struct queue_common_para comm;
};

enum queue_host_common_op {
    QUEUE_INIT,
    QUEUE_UNINIT,
    QUEUE_OP_MAX
};

enum queue_memory_type {
    QUEUE_BUFF,
    QUEUE_BARE_BUFF,
    QUEUE_TYPE_MAX
};

union queue_req_arg {
    struct queue_ioctl_copy copy_arg;
};

/*
 * Queue feature ID definitions.
 * Shared between kernel and user-space via this ioctl header.
 * User-space visible features use QUEUE_FEATURE_xxx naming (without _USER_)
 * and their enum values are placed after kernel-internal features.
 * The bitmap returned by QUEUE_GET_ALL_FEATURE_CMD uses the enum value
 * as the index directly (QUEUE_FEATURE_xxx).
 */
enum queue_feature_id {
    QUEUE_FEATURE_PLATFORM_MINI_DMA = 0,
    QUEUE_FEATURE_SURPORT_PCIE_DMA_SVA,
    QUEUE_FEATURE_SHARE_LOG,
    QUEUE_FEATURE_KA_ALLOC_INTERFACE,
    QUEUE_FEATURE_HOST_LOG,

    /* User-space visible features (auto-incremented from HOST_LOG + 1) */
    QUEUE_FEATURE_SUPPORT_SRIOV,
    QUEUE_FEATURE_HOST_SUPPORT_LOCAL_QUEUE,
    QUEUE_SOC_PLATFORM_960,
    QUEUE_FEATURE_MAX
};
#define QUEUE_FEATURE_MAX_LIMIT 10000

struct queue_ioctl_feature {
    /* input: user-space's current feature count (covers both chip and version features) */
    unsigned int user_feature_num;
    /* output: feature support bitmap (only indices < user_feature_num are valid) */
    unsigned char feature_bitmaps[QUEUE_FEATURE_DEV_NUM][QUEUE_FEATURE_MAX];
};

#define DAVINCI_QUEUE_SUB_MODULE_NAME "QUEUE"
#define QUEUE_IOC_MAGIC 'Q'

#define QUEUE_HOST_COMMON_OP_CMD _IOW(QUEUE_IOC_MAGIC, 1, struct queue_ioctl_host_common_op)
#define QUEUE_ENQUEUE_CMD _IOW(QUEUE_IOC_MAGIC, 2, struct queue_ioctl_enqueue)
#define QUEUE_CTRL_MSG_SEND_CMD _IOW(QUEUE_IOC_MAGIC, 6, struct queue_ioctl_ctrl_msg_send)
#define QUEUE_COPY_CMD _IOWR(QUEUE_IOC_MAGIC, 3, struct queue_ioctl_copy)
#define QUEUE_REPLY_CLIENT_CMD _IOW(QUEUE_IOC_MAGIC, 4, struct queue_ioctl_reply)
#define QUEUE_SESSION_INIT _IOW(QUEUE_IOC_MAGIC, 5, struct queue_init_para)
#define QUEUE_CTRL_MSG_RECV_CMD _IOWR(QUEUE_IOC_MAGIC, 7, struct queue_ioctl_ctrl_msg_recv)
#define QUEUE_GET_IOVEC_NUM_CMD _IOWR(QUEUE_IOC_MAGIC, 8, struct queue_ioctl_iovec_num)
#define QUEUE_GET_ALL_FEATURE_CMD _IOR(QUEUE_IOC_MAGIC, 9, struct queue_ioctl_feature)

#define QUEUE_DEV_FULL_NAME "/dev/hi-queue-manage"
#define QUEUE_ENQUEUE_FLAG 0
#define QUEUE_DEQUEUE_FLAG 1

/* user agent and kernel use */
#define MSG_MAX_LEN 64
#define QUEUE_CTRL_MSG_SEND_CMD _IOW(QUEUE_IOC_MAGIC, 6, struct queue_ioctl_ctrl_msg_send)
#define QUEUE_CMD_MAX 10

#endif