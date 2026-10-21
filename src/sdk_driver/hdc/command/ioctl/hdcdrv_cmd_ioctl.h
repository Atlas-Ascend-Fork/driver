/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _HDCDRV_CMD_IOCTL_H_
#define _HDCDRV_CMD_IOCTL_H_

#ifndef __ka_user
#define __ka_user
#endif

#define HDCDRV_SERVICE_TYPE_DMP 0
#define HDCDRV_SERVICE_TYPE_PROFILING 1 // use for profiling tool
#define HDCDRV_SERVICE_TYPE_IDE1 2
#define HDCDRV_SERVICE_TYPE_FILE_TRANS 3
#define HDCDRV_SERVICE_TYPE_IDE2 4
#define HDCDRV_SERVICE_TYPE_LOG 5
#define HDCDRV_SERVICE_TYPE_RDMA 6
#define HDCDRV_SERVICE_TYPE_BBOX 7
/* the follow used in cloud */
#define HDCDRV_SERVICE_TYPE_FRAMEWORK 8
#define HDCDRV_SERVICE_TYPE_TSD 9
#define HDCDRV_SERVICE_TYPE_TDT 10
#define HDCDRV_SERVICE_TYPE_PROF 11 // use for drv prof
#define HDCDRV_SERVICE_TYPE_IDE_FILE_TRANS 12
#define HDCDRV_SERVICE_TYPE_DUMP 13
#define HDCDRV_SERVICE_TYPE_USER3 14 // use for test
#define HDCDRV_SERVICE_TYPE_DVPP 15
#define HDCDRV_SERVICE_TYPE_QUEUE 16
#define HDCDRV_SERVICE_TYPE_UPGRADE 17
#define HDCDRV_SERVICE_TYPE_RDMA_V2 18
#define HDCDRV_SERVICE_TYPE_TEST 19
#define HDCDRV_SERVICE_TYPE_KMS 20
#define HDCDRV_SERVICE_TYPE_APPLY_MAX 21 // update this value if add new service not for user
#define HDCDRV_SERVICE_TYPE_USER_START 64
#define HDCDRV_SERVICE_TYPE_USER_END 127

#ifdef CFG_FEATURE_SRIOV
#ifdef CFG_ENV_DEV
#define HDCDRV_SUPPORT_MAX_DEV 64
#else
#define HDCDRV_SUPPORT_MAX_DEV 1124
#endif
#else
#ifdef CFG_FEATURE_HDC_REG_MEM
#ifdef DRV_UT
#define HDCDRV_SUPPORT_MAX_DEV 64
#else
#define HDCDRV_SUPPORT_MAX_DEV 2
#endif
#else
#ifdef CFG_ENV_DEV
#define HDCDRV_SUPPORT_MAX_DEV 4
#else
#define HDCDRV_SUPPORT_MAX_DEV 64
#endif
#endif
#endif

#define HDCDRV_DEV_MAX_VDEV_PER_DEVICE 16

#if defined(CFG_FEATURE_VFIO_DEVICE) || defined(CFG_FEATURE_VFIO)
#define HDCDRV_SINGLE_DEV_MAX_SESSION ((136 * HDCDRV_DEV_MAX_VDEV_PER_DEVICE) + 8)
#define HDCDRV_SINGLE_DEV_MAX_SHORT_SESSION ((8 * HDCDRV_DEV_MAX_VDEV_PER_DEVICE) + 8)
#else
#if defined(CFG_FEATURE_SRIOV) && !defined(CFG_ENV_DEV)
#define HDCDRV_SINGLE_DEV_MAX_SESSION ((136 * HDCDRV_DEV_MAX_VDEV_PER_DEVICE) + 8)
#define HDCDRV_SINGLE_DEV_MAX_SHORT_SESSION ((8 * HDCDRV_DEV_MAX_VDEV_PER_DEVICE) + 8)
#else
#define HDCDRV_SINGLE_DEV_MAX_SHORT_SESSION 8
#if defined(CFG_FEATURE_SRIOV) && defined(CFG_ENV_DEV)
#define HDCDRV_SINGLE_DEV_MAX_SESSION 136 /* 32 * 4(log + tsd + dvpp + reserved) + 8 */
#else
#define HDCDRV_SINGLE_DEV_MAX_SESSION 264 /* 64 * 4(log + tsd + dvpp + reserved) + 8 */
#endif
#endif
#endif

#define HDCDRV_SUPPORT_MAX_SESSION (HDCDRV_SINGLE_DEV_MAX_SESSION * HDCDRV_SUPPORT_MAX_DEV)
#define HDCDRV_SUPPORT_MAX_SHORT_SESSION (HDCDRV_SINGLE_DEV_MAX_SHORT_SESSION * HDCDRV_SUPPORT_MAX_DEV)
#define HDCDRV_SUPPORT_MAX_LONG_SESSION (HDCDRV_SUPPORT_MAX_SESSION - HDCDRV_SUPPORT_MAX_SHORT_SESSION)
#define HDCDRV_SUPPORT_MAX_SERVICE 128
#define HDCDRV_SUPPORT_MAX_FID_PID HDCDRV_SUPPORT_MAX_SERVICE
#define HDCDRV_SINGLE_DEV_MAX_LONG_SESSION (HDCDRV_SINGLE_DEV_MAX_SESSION - HDCDRV_SINGLE_DEV_MAX_SHORT_SESSION)

#define HDCDRV_OK 0
#define HDCDRV_ERR (-1)
#define HDCDRV_PARA_ERR (-2)
#define HDCDRV_COPY_FROM_USER_FAIL (-3)
#define HDCDRV_COPY_TO_USER_FAIL (-4)
#define HDCDRV_SERVICE_LISTENING (-5)
#define HDCDRV_SERVICE_NO_LISTENING (-6)
#define HDCDRV_SERVICE_ACCEPTING (-7)
#define HDCDRV_DMA_MEM_ALLOC_FAIL (-8)
#define HDCDRV_NO_SESSION (-9)
#define HDCDRV_SEND_CTRL_MSG_FAIL (-10)
#define HDCDRV_REMOTE_REFUSED_CONNECT (-11)
#define HDCDRV_CONNECT_TIMEOUT (-12)
#define HDCDRV_TX_QUE_FULL (-13)
#define HDCDRV_TX_LEN_ERR (-14)
#define HDCDRV_TX_REMOTE_CLOSE (-15)
#define HDCDRV_RX_BUF_SMALL (-16)
#define HDCDRV_DEVICE_NOT_READY (-17)
#define HDCDRV_DEVICE_RESET (-18)
#define HDCDRV_NOT_SUPPORT (-19)
#define HDCDRV_REMOTE_SERVICE_NO_LISTENING (-20)
#define HDCDRV_NO_BLOCK (-21)
#define HDCDRV_SESSION_HAS_CLOSED (-22)
#define HDCDRV_MEM_NOT_MATCH (-23)
#define HDCDRV_CONV_FAILED (-24)
#define HDC_LOW_POWER_STATE (-25)
#define HDCDRV_NO_EPOLL_FD (-26)
#define HDCDRV_RX_TIMEOUT (-27)
#define HDCDRV_TX_TIMEOUT (-28)
#define HDCDRV_DMA_MEM_ISUSED (-29)
#define HDCDRV_SESSION_ID_MISS_MATCH (-30)
#define HDCDRV_MEM_ALLOC_FAIL (-31)
#define HDCDRV_SQ_DESC_NULL (-32)
#define HDCDRV_F_NODE_SEARCH_FAIL (-33)
#define HDCDRV_DMA_COPY_FAIL (-34)
#define HDCDRV_SAFE_MEM_OP_FAIL (-35)
#define HDCDRV_CHAR_DEV_CREAT_FAIL (-36)
#define HDCDRV_DMA_MPA_FAIL (-37)
#define HDCDRV_FIND_VMA_FAIL (-38)
#define HDCDRV_DMA_QUE_FULL (-39)
#define HDCDRV_CMD_CONTINUE (-40)
#define HDCDRV_NO_PERMISSION (-41)
#define HDCDRV_EPOLL_CLOSE (-42)
#define HDCDRV_SESSION_CHAN_INVALID (-43)
#define HDCDRV_GET_NUMA_ID_FAILED (-44)
#define HDCDRV_NO_WAIT_MEM_INFO (-45)
#define HDCDRV_NO_WAIT_MEM_TIMEOUT (-46)
#define HDCDRV_BUFF_REPEATED_REGISTER (-47)
#define HDCDRV_VA_UNMAP_FAILED (-48)
#define HDCDRV_PEER_REBOOT (-49)
#define HDCDRV_UB_INTERFACE_ERR (-50)

#define HDCDRV_CLOSE_TYPE_NONE 0
#define HDCDRV_CLOSE_TYPE_USER 1
#define HDCDRV_CLOSE_TYPE_KERNEL 2
#define HDCDRV_CLOSE_TYPE_RELEASE 3
#define HDCDRV_CLOSE_TYPE_NOT_SET_OWNER 4
#define HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST 5
#define HDCDRV_CLOSE_TYPE_MAX 6

#define HDCDRV_MEM_MAX_LEN (512 * 1024 * 1024)
#define HDCDRV_CTRL_MEM_MAX_LEN (256 * 1024)
#define HDCDRV_MEM_MAX_LEN_BIT 22   /* 4M */
#define HDCDRV_MEM_1MB_LEN_BIT 20   /* 1M */
#define HDCDRV_MEM_512KB_LEN_BIT 19 /* 512k */
#define HDCDRV_MEM_MIN_LEN_BIT 18   /* 256k */
#define HDCDRV_MEM_64KB_LEN_BIT 16  /* 64kb */

#define HDCDRV_EPOLL_FD_EVENT_NUM 1024
#define HDCDRV_INVALID_PEER_PID (-1)

#define HDCDRV_DEFAULT_DEV_ID 0
#define HDCDRV_INVALID_FID (unsigned int)(-1)

#define HDCDRV_FEATURE_NOT_SUPPORT 0
#define HDCDRV_FEATURE_SUPPORT 1
#define HDCDRV_FEATURE_NPU_DEPEND 1

enum hdcdrv_session_attr_cmd_type {
    HDCDRV_SESSION_ATTR_RUN_ENV = 0,
    HDCDRV_SESSION_ATTR_VFID,
    HDCDRV_SESSION_ATTR_LOCAL_CREATE_PID,
    HDCDRV_SESSION_ATTR_PEER_CREATE_PID,
    HDCDRV_SESSION_ATTR_STATUS,
    HDCDRV_SESSION_ATTR_DFX,
    HDCDRV_SESSION_ATTR_MAX
};

enum hdcdrv_cmd_type {
    HDCDRV_CMD_SERVER_WAKEUP_WAIT = 0x4,
    HDCDRV_CMD_CLIENT_WAKEUP_WAIT = 0x5,
    HDCDRV_CMD_CLIENT_DESTROY = 0x6,
    HDCDRV_CMD_GET_PEER_DEV_ID = 0x7,
    HDCDRV_CMD_CONFIG = 0x8,
    HDCDRV_CMD_SET_SERVICE_LEVEL = 0x9,
    HDCDRV_CMD_GET_SPEC_DEVID = 0xa,
    HDCDRV_CMD_GET_P2P_COM_STATUS = 0xb,
    HDCDRV_CMD_SERVER_CREATE = 0x10,
    HDCDRV_CMD_SERVER_DESTROY = 0x11,
    HDCDRV_CMD_ACCEPT = 0x12,
    HDCDRV_CMD_CONNECT = 0x13,
    HDCDRV_CMD_CLOSE = 0x14,
    HDCDRV_CMD_SEND = 0x15,
    HDCDRV_CMD_RECV_PEEK = 0x16,
    HDCDRV_CMD_RECV = 0x17,
    HDCDRV_CMD_SET_SESSION_OWNER = 0x18,
    HDCDRV_CMD_GET_STAT = 0x19,
    HDCDRV_CMD_GET_SESSION_ATTR = 0x1a,
    HDCDRV_CMD_SET_SESSION_TIMEOUT = 0x1b,
    HDCDRV_CMD_GET_SESSION_UID = 0x1c,
    HDCDRV_CMD_GET_PAGE_SIZE = 0x1d,
    HDCDRV_CMD_GET_SESSION_INFO = 0x1e,
    HDCDRV_CMD_GET_FEATURE_INFO = 0x1f,
    HDCDRV_CMD_ALLOC_MEM = 0x20,
    HDCDRV_CMD_FREE_MEM = 0x21,
    HDCDRV_CMD_FAST_SEND = 0x22,
    HDCDRV_CMD_FAST_RECV = 0x23,
    HDCDRV_CMD_DMA_MAP = 0x24,
    HDCDRV_CMD_DMA_UNMAP = 0x25,
    HDCDRV_CMD_DMA_REMAP = 0x26,
    HDCDRV_CMD_REGISTER_MEM = 0x27,
    HDCDRV_CMD_UNREGISTER_MEM = 0x28,
    HDCDRV_CMD_WAIT_MEM = 0x29,
    HDCDRV_CMD_EPOLL_ALLOC_FD = 0x40,
    HDCDRV_CMD_EPOLL_FREE_FD = 0x41,
    HDCDRV_CMD_EPOLL_CTL = 0x42,
    HDCDRV_CMD_EPOLL_WAIT = 0x43,
    HDCDRV_CMD_WAIT_CTRL_MSG = 0x50,
    HDCDRV_CMD_SEND_CTRL_MSG = 0x51,
    HDCDRV_CMD_INIT_CTRL_NODE = 0x52,
    HDCDRV_CMD_UNINIT_CTRL_NODE = 0x53,
    HDCDRV_CMD_MAX
};

#define HDCDRV_CMD_MAGIC 'H'

struct hdcdrv_timeout {
    unsigned int send_timeout;
    unsigned int recv_timeout;
    unsigned int fast_send_timeout;
    unsigned int fast_recv_timeout;
};

struct hdcdrv_event {
    unsigned int events;
    int sub_data;            /* trans to user in epoll wait return */
    unsigned long long data; /* trans to user in epoll wait return */
};

struct hdcdrv_cmd_common {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
};

struct hdcdrv_cmd_get_peer_dev_id {
    int ret;
    int dev_id; /* input */
    unsigned long long pid;
    unsigned long long reserve_comm;
    int peer_dev_id; /* output */
};

struct hdcdrv_cmd_config {
    int ret;
    int dev_id;
    unsigned long long pid; /* input */
    unsigned long long reserve_comm;
    int segment; /* input,output */
    unsigned int reserved[4];
};

struct hdcdrv_cmd_set_service_level {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int service_type; /* input */
    int level;        /* input */
};

struct hdcdrv_cmd_server_create {
    int ret;
    int dev_id;             /* input */
    unsigned long long pid; /* input */
    unsigned long long reserve_comm;
    int service_type; /* input */
    unsigned int reserved[5];
};

struct hdcdrv_cmd_server_destroy {
    int ret;
    int dev_id; /* input */
    unsigned long long pid;
    unsigned long long reserve_comm;
    int service_type; /* input */
    unsigned int reserved[4];
};

struct hdcdrv_cmd_client_destroy {
    int ret;
    int dev_id; /* input */
    unsigned long long pid;
    unsigned long long reserve_comm;
    int service_type; /* input */
    unsigned int reserved[4];
};

struct hdcdrv_cmd_accept {
    int ret;
    int dev_id; /* input */
    unsigned long long pid;
    unsigned long long reserve_comm;
    int service_type;                   /* input */
    int session;                        /* output */
    unsigned int session_cur_alloc_idx; /* output */
    unsigned long long peer_pid;        /* only used in UB */
    unsigned int remote_session;        /* only used in UB */
    int run_env;                        /* only used in UB */
    int euid;                           /* only used in UB */
    int uid;                            /* only used in UB */
    int root_privilege;                 /* only used in UB */
    unsigned int unique_val;            /* only used in UB */
    unsigned long long user_va;         /* only used in UB */
    unsigned int reserved[10];
};

struct hdcdrv_cmd_connect {
    int peer_pid; /* input */
    int dev_id;   /* input */
    unsigned long long pid;
    unsigned long long reserve_comm;
    int service_type;                   /* input */
    int session;                        /* output */
    unsigned int timeout;               /* output */
    unsigned int session_cur_alloc_idx; /* output */
    int ret;                            /* only used in UB */
    unsigned int unique_val;            /* only used in UB */
    unsigned long long user_va;         /* only used in UB */
    unsigned int reserved[8];
};

struct hdcdrv_cmd_close {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int session; /* input */
    unsigned int unique_val;
    unsigned long long task_start_time;
    unsigned int remote_session;        /* only used in UB */
    int local_close_state;              /* only used in UB */
    int remote_local_state;             /* only used in UB */
    unsigned int session_cur_alloc_idx; /* output */
    unsigned int reserved[8];
};

struct hdcdrv_cmd_send {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int session;                  /* input */
    void __ka_user *src_buf;      /* input */
    int len;                      /* input */
    void __ka_user *pool_buf;     /* input */
    unsigned long long pool_addr; /* input */
    int wait_flag;                /* input */
    unsigned int timeout;         /* input */
    unsigned int reserved[8];
};

struct hdcdrv_cmd_recv_peek {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int session;          /* input */
    int len;              /* output, if remote session close, this is 0 */
    int wait_flag;        /* input */
    unsigned int timeout; /* input */
    int group_flag;       /* input */
    int count;            /* output, if remote session close, this is 0 */
    unsigned int session_cur_alloc_idx;
    unsigned int reserved[8];
};

#define HDCDRV_SESSION_RX_LIST_MAX_PKT 8

struct hdcdrv_cmd_recv {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int session;                                              /* input */
    void __ka_user *dst_buf;                                  /* output */
    int len;                                                  /* input */
    void __ka_user *pool_buf;                                 /* output */
    int out_len;                                              /* output */
    int buf_count;                                            /* output, for vhdc used */
    int group_flag;                                           /* input */
    void __ka_user *buf_list[HDCDRV_SESSION_RX_LIST_MAX_PKT]; /* output, for vhdc used in VM */
    unsigned int buf_len[HDCDRV_SESSION_RX_LIST_MAX_PKT];     /* output, for vhdc used in VM */
    unsigned int reserved[8];
};

struct hdcdrv_cmd_set_session_owner {
    int ret;
    int dev_id;
    unsigned long long pid; /* owner pid */
    unsigned long long reserve_comm;
    unsigned long long ppid;
    int session; /* input */
    unsigned int reserved[4];
};

struct hdcdrv_cmd_get_session_attr {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int cmd_type; /* input */
    int session;  /* input */
    int output;   /* output */
    unsigned int session_cur_alloc_idx;
    unsigned int reserved[4];
};

struct hdcdrv_cmd_set_session_timeout {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int session;                   /* input */
    struct hdcdrv_timeout timeout; /* input */
};

struct hdcdrv_cmd_get_uid_stat {
    int ret;
    int dev_id; /* input, -1 not care */
    unsigned long long pid;
    unsigned long long reserve_comm;
    int session; /* input, -1 not care */
    unsigned int euid;
    unsigned int uid;
    int root_privilege;
    unsigned int reserved[4];
};

struct hdcdrv_cmd_alloc_mem {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    unsigned int type;      /* input */
    unsigned int len;       /* input */
    unsigned long long va;  /* input */
    unsigned int page_type; /* input */
    int map;                /* input */
};

struct hdcdrv_cmd_free_mem {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    unsigned int type;      /* input */
    unsigned int len;       /* output */
    unsigned int page_type; /* output */
    unsigned long long va;  /* input */
};

struct hdcdrv_cmd_fast_send {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int session;                      /* input */
    int wait_flag;                    /* input */
    unsigned long long src_data_addr; /* input */
    unsigned long long dst_data_addr; /* input */
    unsigned long long src_ctrl_addr; /* input */
    unsigned long long dst_ctrl_addr; /* input */
    int data_len;                     /* input */
    int ctrl_len;                     /* input */
    unsigned int timeout;             /* input */
};

struct hdcdrv_cmd_fast_recv {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int session;                  /* input */
    int wait_flag;                /* input */
    unsigned int timeout;         /* input */
    unsigned long long data_addr; /* output */
    unsigned long long ctrl_addr; /* output */
    int data_len;                 /* output */
    int ctrl_len;                 /* output */
};

struct hdcdrv_cmd_dma_map {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    unsigned int type;     /* input */
    unsigned long long va; /* input */
};

struct hdcdrv_cmd_dma_unmap {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    unsigned int type;     /* input */
    unsigned long long va; /* input */
};

struct hdcdrv_cmd_dma_remap {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    unsigned int type;     /* input */
    unsigned long long va; /* input */
};

struct hdcdrv_cmd_register_mem {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    unsigned int type;     /* input */
    unsigned int len;      /* input */
    unsigned long long va; /* input */
    unsigned int flag;     /* input */
};

struct hdcdrv_cmd_unregister_mem {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    unsigned int type;      /* input */
    unsigned int len;       /* output */
    unsigned int page_type; /* output */
    unsigned long long va;  /* input */
};

struct hdcdrv_cmd_wait_mem {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int session;                  /* input */
    int timeout;                  /* input */
    unsigned int result_type;     /* input */
    unsigned long long data_addr; /* output */
    unsigned long long ctrl_addr; /* output */
    int data_len;                 /* output */
    int ctrl_len;                 /* output */
    int result;                   /* output */
};

struct hdcdrv_cmd_epoll_alloc_fd {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int size; /* input */
    int epfd; /* output */
    unsigned int reserved[4];
};

struct hdcdrv_cmd_epoll_free_fd {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int epfd; /* input */
    unsigned int reserved[4];
};

struct hdcdrv_cmd_epoll_ctl {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int epfd;                  /* input */
    int op;                    /* input */
    int para1;                 /* input, service:dev_id, session:session_fd */
    int para2;                 /* input, service:service_type */
    struct hdcdrv_event event; /* input */
    unsigned int reserved[4];
};

#define HDCDRV_VEPOLL_EVENT_MAX 5

struct hdcdrv_cmd_epoll_wait {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    int epfd;                             /* input */
    int timeout;                          /* input */
    int maxevents;                        /* input */
    int ready_event;                      /* output */
    struct hdcdrv_event __ka_user *event; /* output */
    struct hdcdrv_event vevent[HDCDRV_VEPOLL_EVENT_MAX];
    unsigned int reserved[4];
};

struct hdcdrv_cmd_get_page_size {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    unsigned int page_size;
    unsigned int hpage_size;
    unsigned int page_bit;
    unsigned int reserved[4];
};

struct hdcdrv_cmd_get_session_info {
    int ret;
    int dev_id;
    unsigned long long pid;
    unsigned long long reserve_comm;
    unsigned int fid;
    int session_fd;
    unsigned int reserved[4];
};

struct hdcdrv_cmd_client_wakeup_wait {
    int ret;
    int dev_id; /* input */
    unsigned long long pid;
    unsigned long long reserve_comm;
    int service_type; /* input */
};

struct hdcdrv_cmd_server_wakeup_wait {
    int ret;
    int dev_id; /* input */
    unsigned long long pid;
    unsigned long long reserve_comm;
    int service_type; /* input */
};

#define HDCDRV_CHIP_OS_NAME_STRING_MAX_LEN 15
#define HDCDRV_CHIP_OS_NAME_LEN_MAX (HDCDRV_CHIP_OS_NAME_STRING_MAX_LEN + 1)
struct hdcdrv_cmd_get_spec_devid {
    int ret;
    int dev_id; /* input */
    unsigned long long pid;
    unsigned long long reserve_comm;
    char chip_name[HDCDRV_CHIP_OS_NAME_LEN_MAX];
    char os_name[HDCDRV_CHIP_OS_NAME_LEN_MAX];
    unsigned int spec_devid; /* output */
};

struct hdcdrv_cmd_get_com_status {
    int ret;
    int dev_id; /* input */
    unsigned long long pid;
    unsigned long long reserve_comm;
    unsigned int buff_len; /* input/output */
    void *status_buff;     /* input/output */
};

/* send completion message type */
enum hdcdrv_feature_type {
    HDC_FEATURE_COMMON_CHAN = 0,
    HDC_FEATURE_FAST_CHAN = 1,
    HDC_FEATURE_EPOLL = 2,
    HDC_FEATURE_MAX
};

struct hdcdrv_cmd_get_feature_info {
    int ret;
    int dev_id; /* input */
    unsigned long long pid;
    unsigned long long reserve_comm;
    unsigned int feature_type; /* input */
    unsigned int version;      /* output */
    unsigned int is_support;   /* output */
    unsigned int reserved[4];
};

struct hdcdrv_cmd_wait_ctrl_msg { // Only used in UB
    int ret;
    int dev_id; /* input */
    unsigned long long pid;
    unsigned int msg_len;    /* output */
    void __ka_user *msg_buf; /* output */
};

struct hdcdrv_connect_perf { // Only used in UB
    unsigned long long k_cnt_start;
    unsigned long long k_cnt_send_ctrl_start;
    unsigned long long k_cnt_send_ctrl_end;
    unsigned long long k_cnt_reply_wake;
    unsigned long long k_cnt_wait_fin;
    unsigned long long k_cnt_cpy_end;
};

#define HDCDRV_ACCEPT_PERF_RESERVED 5

struct hdcdrv_accept_perf { // Only used in UB
    unsigned long long k_acc_notify_entry;
    unsigned long long k_acc_notify_search_end;
    unsigned long long k_acc_start;
    unsigned long long k_acc_send_ctrl_start;
    unsigned long long k_acc_send_ctrl_end;
    unsigned long long reserved[HDCDRV_ACCEPT_PERF_RESERVED];
};

struct hdcdrv_cmd_send_ctrl_msg { // Only used in UB
    int ret;
    int dev_id; /* input */
    unsigned long long pid;
    unsigned int msg_len;    /* output */
    void __ka_user *msg_buf; /* output */
    union {                  /* output, connect or accept depends on msg type */
        struct hdcdrv_connect_perf connect;
        struct hdcdrv_accept_perf accept;
    };
};

union hdcdrv_cmd {
    struct hdcdrv_cmd_common cmd_com;
    struct hdcdrv_cmd_get_peer_dev_id get_peer_dev_id;
    struct hdcdrv_cmd_config config;
    struct hdcdrv_cmd_set_service_level set_level;
    struct hdcdrv_cmd_client_destroy client_destroy;
    struct hdcdrv_cmd_server_create server_create;
    struct hdcdrv_cmd_server_destroy server_destroy;
    struct hdcdrv_cmd_accept accept;
    struct hdcdrv_cmd_connect connect;
    struct hdcdrv_cmd_close close;
    struct hdcdrv_cmd_send send;
    struct hdcdrv_cmd_recv_peek recv_peek;
    struct hdcdrv_cmd_recv recv;
    struct hdcdrv_cmd_set_session_owner set_owner;
    struct hdcdrv_cmd_get_session_attr get_session_attr;
    struct hdcdrv_cmd_set_session_timeout set_session_timeout;
    struct hdcdrv_cmd_get_uid_stat get_uid_stat;
    struct hdcdrv_cmd_alloc_mem alloc_mem;
    struct hdcdrv_cmd_free_mem free_mem;
    struct hdcdrv_cmd_fast_send fast_send;
    struct hdcdrv_cmd_fast_recv fast_recv;
    struct hdcdrv_cmd_dma_map dma_map;
    struct hdcdrv_cmd_dma_unmap dma_unmap;
    struct hdcdrv_cmd_dma_remap dma_remap;
    struct hdcdrv_cmd_register_mem register_mem;
    struct hdcdrv_cmd_unregister_mem unregister_mem;
    struct hdcdrv_cmd_wait_mem wait_mem;
    struct hdcdrv_cmd_epoll_alloc_fd epoll_alloc_fd;
    struct hdcdrv_cmd_epoll_free_fd epoll_free_fd;
    struct hdcdrv_cmd_epoll_ctl epoll_ctl;
    struct hdcdrv_cmd_epoll_wait epoll_wait;
    struct hdcdrv_cmd_get_page_size get_page_size;
    struct hdcdrv_cmd_get_session_info get_session_info;
    struct hdcdrv_cmd_client_wakeup_wait client_wakeup_wait;
    struct hdcdrv_cmd_server_wakeup_wait server_wakeup_wait;
    struct hdcdrv_cmd_get_spec_devid get_spec_devid;
    struct hdcdrv_cmd_wait_ctrl_msg wait_ctrl_msg;
    struct hdcdrv_cmd_send_ctrl_msg send_ctrl_msg;
    struct hdcdrv_cmd_common init_ctrl_node;
    struct hdcdrv_cmd_common uninit_ctrl_node;
    struct hdcdrv_cmd_get_com_status p2p_com_status;
    struct hdcdrv_cmd_get_feature_info get_feature_info;
};

typedef struct hdcdrv_jetty_info {
    unsigned int session_jfc_recv_id; // hdc remote session jfc_recv id
    unsigned int session_jfc_send_id; // hdc remote session jfc_send id
    unsigned int session_jfs_id;      // hdc remote session jfs id
    unsigned int session_jfr_id;      // hdc remote session jfr_4k id
} hdcdrv_jetty_info_t;

enum hdcdrv_notify_type {
    HDCDRV_NOTIFY_MSG_CONNECT,
    HDCDRV_NOTIFY_MSG_CONNECT_REPLY,
    HDCDRV_NOTIFY_MSG_CLOSE,
    HDCDRV_NOTIFY_MSG_CLOSE_REPLY,
    HDCDRV_NOTIFY_MSG_CLOSE_RELEASE,
    HDCDRV_NOTIFY_MSG_DFX,
    HDCDRV_NOTIFY_MSG_DFX_REPLY,
    HDCDRV_NOTIFY_MSG_MAX
};

/* send completion message type */
enum halHdcWaitMemMsgType {
    HDC_WAIT_ALL = 0,
    HDC_WAIT_ONLY_SUCCESS = 1,
    HDC_WAIT_ONLY_EXCEPTION = 2,
    HDC_WAIT_MAX
};

typedef struct hdc_ub_dbg_stat {
    unsigned long long tx;                  // hdc num of tx pkt
    unsigned long long tx_bytes;            // hdc total_len of tx
    unsigned long long rx_epoll;            // hdc num of rx epoll
    unsigned long long rx_full;             // hdc num of rx list full
    unsigned long long rx_add_list;         // hdc num of add to rx list
    unsigned long long rx;                  // hdc num of rx pkt
    unsigned long long rx_bytes;            // hdc total_len of rx
    unsigned long long tx_full;             // hdc tx_full pkt
    unsigned long long tx_fail_hdc;         // hdc tx_fail by hdc pkt
    unsigned long long tx_fail_ub;          // hdc tx_fail by ub pkt
    unsigned long long rx_fail_hdc;         // hdc rx_fail by hdc pkt
    unsigned long long rx_fail_ub;          // hdc rx_fail by ub pkt
    unsigned long long remote_rx_full;      // hdc remote rx_full pkt
    unsigned long long remote_rx_fail;      // hdc remote rx_fail pkt
    unsigned int epoll_thread_create;       // whether epoll thread has been created, 1 means created
    signed int epoll_thread_epfd;           // epfd of the epoll thread, -1 means not exist
    unsigned long long recv_event_abnormal; // hdc num of abnormal recv event in epoll thread
} hdc_ub_dbg_stat_t;

typedef struct hdc_ub_send_recv_info {
    unsigned long long timecost1;          // cost of find_idle_block in send
    unsigned long long timecost2;          // cost of memcpy in send
    unsigned long long timecost3;          // cost of urma_post_jfs_wr + urma_wait_jfc in send
    unsigned long long timecost4;          // cost of poll_jfc in send
    unsigned long long timecost5;          // cost of ack + rearm in send
    unsigned long long timecost_send;      // cost of total send
    unsigned long long timecost6;          // cost of poll_jfc in recv
    unsigned long long timecost7;          // cost of ack + rearm in recv
    unsigned long long timecost8;          // cost of memcpy in recv
    unsigned long long timecost9;          // cost of urma_post_jfr_wr in recv
    unsigned long long timecost_recv_peek; // cost of total recv_peek
    unsigned long long timecost_recv;      // cost of total recv
    unsigned long long timecost_exceed_cnt_send;
    unsigned long long timecost_exceed_cnt_recv;
} hdc_ub_send_recv_info_t;

struct hdcdrv_session_para_info {
    unsigned int unique_val;
    unsigned long long peer_pid;  // local side peer_pid, should be equal to remote side owner_pid
    unsigned long long owner_pid; // local side owner_pid, should be equal to remote side peer_pid
};

#define HDCDRV_EVENT_JETTY_INFO_MAX_LEN 64

struct hdcdrv_event_connect {
    int service_type;
    unsigned int client_session;
    int run_env;
    unsigned long long peer_create_pid;
    unsigned int unique_val;
    unsigned long long client_pid;
    int euid;
    int uid;
    int root_privilege;
    // Jetty information, which needs to be converted into the
    // corresponding structure when used, currently struct hdc_jfr_id_info
    char jetty_info[HDCDRV_EVENT_JETTY_INFO_MAX_LEN];
};

struct hdcdrv_event_connect_reply {
    unsigned int server_session;
    unsigned int client_session;
    int run_env;
    unsigned int unique_val;
    unsigned long long server_pid;
    unsigned long long peer_pid;
    int euid;
    int uid;
    int root_privilege;
    int server_error_code;
    // Jetty information, which needs to be converted into the
    // corresponding structure when used, currently struct hdc_jfr_id_info
    char jetty_info[HDCDRV_EVENT_JETTY_INFO_MAX_LEN];
};

struct hdcdrv_event_close {
    unsigned int local_session;  // close side local id, should be equal to remote side remote id
    unsigned int remote_session; // close side remote id, should be equal to remote side local id
    int session_close_state;
    unsigned int unique_val;      // both sides unique_val should be same
    unsigned long long peer_pid;  // close side peer_pid, should be equal to remote side owner_pid
    unsigned long long owner_pid; // close side owner_pid, should be equal to remote side peer_pid
};

struct hdcdrv_event_close_reply {
    int local_session;
    int remote_session;
    int session_close_state;
    unsigned int unique_val;
};

struct hdcdrv_event_dfx {
    unsigned int l_session_id;
    unsigned int r_session_id;
    struct hdcdrv_session_para_info para_info;
};

struct hdcdrv_event_dfx_reply {
    unsigned int l_session_id;
    unsigned int r_session_id;
    struct hdcdrv_session_para_info para_info;
    hdc_ub_dbg_stat_t dfx_info;
    hdcdrv_jetty_info_t remote_jetty_info;
    hdc_ub_send_recv_info_t send_recv_info;
};

struct hdcdrv_event_msg {
    int type;
    int error_code;
    unsigned long long peer_pid;
    union {
        struct hdcdrv_event_connect connect_msg;
        struct hdcdrv_event_connect_reply connect_msg_reply;
        struct hdcdrv_event_close close_msg;
        struct hdcdrv_event_close_reply close_msg_reply;
        struct hdcdrv_event_dfx dfx_msg;
        struct hdcdrv_event_dfx_reply dfx_msg_reply;
    };
};

#endif
