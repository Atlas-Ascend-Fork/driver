/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef __HDC_CMN_H__
#define __HDC_CMN_H__

#ifndef HDC_UT_TEST
#include "mmpa_api.h"
#endif
#include <arpa/inet.h>
#include <sys/epoll.h>

#include "securec.h"

#include "drv_user_common.h"
#include "hdcdrv_cmd_ioctl.h"
#include "hdc_file.h"
#include "hdc_log.h"
#include "hdc_ub_dfx.h"
/* ----------------------------------------------*
 * External variable description                                 *
 *---------------------------------------------- */
/* ----------------------------------------------*
 * External variable prototype description                             *
 *---------------------------------------------- */
/* ----------------------------------------------*
 * Macro definitions                                       *
 *---------------------------------------------- */
#define HDC_SESSION_SERVER 0
#define HDC_SESSION_CLINET 1

#define HDC_CFG_FILE_NAME "/etc/hdcBasic.cfg"
#define HDC_DOCKER_CFG_FILE_NMAE "/usr/local/identity"

/* Configuration file keyword name */
#define HDC_TRANS_TYPE "TRANS_TYPE"
#define HDC_SOCKET_SEGMENT "SOCKET_SEGMENT"
#define HDC_PCIE_SEGMENT "PCIE_SEGMENT"
#define HDC_DMP_SERVICE_PORT "DMP_SERVICE_PORT"
#define HDC_MATRIX_SERVICE_PORT "MATRIX_SERVICE_PORT"
#define HDC_IDE_SERVICE_PORT1 "IDE_SERVICE_PORT1"
#define HDC_IDE_SERVICE_PORT2 "IDE_SERVICE_PORT2"
#define HDC_FILE_TRANS_SERVICE_PORT "FILE_TRANS_SERVICE_PORT"
#define HDC_LOG_SERVICE_PORT "LOG_SERVICE_PORT"
#define HDC_RDMA_SERVICE_PORT "RDMA_SERVICE_PORT"
#define HDC_USER3_SERVICE_PORT "USER3_SERVICE_PORT"
#define HDC_FRAMEWORK_SERVICE_PORT "FRAMEWORK_SERVICE_PORT"
#define HDC_TSD_SERVICE_PORT "TSD_SERVICE_PORT"
#define HDC_TDT_SERVICE_PORT "TDT_SERVICE_PORT"
#define HDC_PROF_SERVICE_PORT "PROF_SERVICE_PORT"
#define HDC_DVPP_SERVICE_PORT "DVPP_SERVICE_PORT"

#define HDC_LOCAL_BASE_IPADDR "LOCAL_BASE_IPADDR"
#define HDC_REMOTE_BASE_IPADDR "REMOTE_BASE_IPADDR"

#define HDC_MAGIC_WORD 0x484443FF

#define HDC_PCIE_MAX_SEGMENT 524288 /* 512K */

#define HDC_CONFIG_INIT_FINISHED 1

#define HDC_ACCESS_COUNT 10
#define HDC_CONFIG_COUNT 200

#define HDC_ACCESS_COUNT_V3 1
#define HDC_CONFIG_COUNT_V3 1

#define HDC_SLEEP_TIME 1000

#define HDC_ACCEPT_WAITING 1

#define HDC_ACCEPT_NOT_WAITING 0

#define HDC_SOCKET_MAX_RETRY_TIMES 50

#define HDC_SERVER_DESTORY_WAIT_TIME_US 1000

/* Ip type */
#define HDC_IPADDR_TYPE_NUM 2
#define HDC_LOCAL_IP_IDX 0
#define HDC_REMORE_IP_IDX 1

#define HDC_MAX_DEVICE_NUM_V3 2
#define MAX_VF_DEVID_START_V3 32
/* Service priority */
#define HDC_SERVICE_LEVEL_0 0
#define HDC_SERVICE_LEVEL_1 1

#define HDC_IPADDR_STR_LEN 16
#define HDC_PEERNODE_NUM 254

#define HDCDRV_SID_LEN 32

#define HDCD_SOCKER_LISTEN_LEN 5

#ifndef KEY_CHIP_TYPE_INDEX
#define KEY_CHIP_TYPE_INDEX (8U)
#endif

#ifndef CHIP_TYPE_ASCEND_V1
#define CHIP_TYPE_ASCEND_V1 (1U)
#endif

#ifndef CHIP_TYPE_ASCEND_V2
#define CHIP_TYPE_ASCEND_V2 (2U)
#endif

#ifndef container_of
#define container_of(ptr, type, member)                    \
    ({                                                     \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member)); \
    })
#endif

#define HDC_SESSION_FD_INVALID ((mmProcess) - 1)

#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA)
#define HDC_PERF_STANDER 100000000               // HDC recv/send performance 100 s
#define HDC_ACCEPT_PERF_STANDER 100000000000     // HDC accept performance 100000 s
#define HDC_CLOSE_PERF_STANDER 10000000000000ULL // HDC close performance 10000000 s
#else
#define HDC_PERF_STANDER 10            // HDC recv/send performance 10 us
#define HDC_ACCEPT_PERF_STANDER 10000  // HDC accept performance 10 ms
#define HDC_CLOSE_PERF_STANDER 1000000 // HDC close performance 1 s
#endif

#define HDC_CLOSE_PERF_STANDER_ENV "ASCEND_HDC_CLOSE_PERF_STANDER" // HDC close performance environment name

extern const char *g_errno_str[];
unsigned int get_err_str_count(void);

#define STRERROR(errno) \
    (((unsigned int)(errno) < get_err_str_count()) ? g_errno_str[(unsigned int)(errno)] : g_errno_str[1])

typedef void *PPC_CLIENT;
typedef void *PPC_SESSION;
typedef void *PPC_SERVER;

struct drv_ppc_msg_buf {
    char *pBuf;
    int len;
};

struct drv_ppc_msg {
    int count;
    struct drv_ppc_msg_buf bufList[1]; /**< 1 just erase pclint warning. here should be 0 */
};

struct ppc_msg_head {
    bool freeBuf;
    struct drv_ppc_msg msg;
};

#if defined(HDC_UT_TEST) || defined(EMU_ST)
#define STATIC
#define dsb(opt)
#else
#if defined(__arm__) || defined(__aarch64__)
#define dsb(opt)                                  \
    {                                             \
        asm volatile("dsb " #opt : : : "memory"); \
    }
#else
#define dsb(opt)
#endif
#define STATIC static
#endif
#define rmb() dsb(ld) /* read fence */
#define wmb() dsb(st) /* write fence */
#define HDC_WAIT_ALWAYS 0
#define HDC_NOWAIT 1
#define HDC_WAIT_TIMEOUT 2

#define CONVERT_MS_TO_S 1000
#define CONVERT_MS_TO_US 1000
#define CONVERT_US_TO_NS 1000
#define CONVERT_MS_TO_NS 1000000
#define CONVERT_S_TO_US 1000000
#define CONVERT_S_TO_NS 1000000000
#define CONVERT_TO_TIMEOUT_MASK 0xFFU

#define HDCDRV_GET_PEER_DEV_ID _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_GET_PEER_DEV_ID)
#define HDCDRV_HDCDRV_CONFIG _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_CONFIG)
#define HDCDRV_SET_SERVICE_LEVEL _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_SET_SERVICE_LEVEL)
#define HDCDRV_GET_SPEC_DEVID _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_GET_SPEC_DEVID)
#define HDCDRV_SERVER_CREATE _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_SERVER_CREATE)
#define HDCDRV_SERVER_DESTROY _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_SERVER_DESTROY)
#define HDCDRV_ACCEPT _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_ACCEPT)
#define HDCDRV_CONNECT _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_CONNECT)
#define HDCDRV_CLOSE _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_CLOSE)
#define HDCDRV_SEND _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_SEND)
#define HDCDRV_RECV_PEEK _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_RECV_PEEK)
#define HDCDRV_RECV _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_RECV)
#define HDCDRV_SET_SESSION_OWNER _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_SET_SESSION_OWNER)
#define HDCDRV_GET_SESSION_ATTR _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_GET_SESSION_ATTR)
#define HDCDRV_SET_SESSION_TIMEOUT _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_SET_SESSION_TIMEOUT)
#define HDCDRV_GET_SESSION_UID _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_GET_SESSION_UID)
#define HDCDRV_GET_PAGE_SIZE _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_GET_PAGE_SIZE)
#define HDCDRV_GET_SESSION_FID _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_GET_SESSION_INFO)
#define HDCDRV_GET_FEATURE_INFO _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_GET_FEATURE_INFO)
#define HDCDRV_ALLOC_MEM _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_ALLOC_MEM)
#define HDCDRV_FREE_MEM _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_FREE_MEM)
#define HDCDRV_FAST_SEND _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_FAST_SEND)
#define HDCDRV_FAST_RECV _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_FAST_RECV)
#define HDCDRV_DMA_MAP _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_DMA_MAP)
#define HDCDRV_DMA_UNMAP _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_DMA_UNMAP)
#define HDCDRV_DMA_REMAP _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_DMA_REMAP)
#define HDCDRV_REGISTER_MEM _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_REGISTER_MEM)
#define HDCDRV_UNREGISTER_MEM _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_UNREGISTER_MEM)
#define HDCDRV_WAIT_MEM_RELEASE _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_WAIT_MEM)
#define HDCDRV_GET_P2P_COM_STATUS _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_GET_P2P_COM_STATUS)

#define HDCDRV_EPOLL_ALLOC_FD _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_EPOLL_ALLOC_FD)
#define HDCDRV_EPOLL_FREE_FD _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_EPOLL_FREE_FD)
#define HDCDRV_EPOLL_CTL _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_EPOLL_CTL)
#define HDCDRV_EPOLL_WAIT _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_EPOLL_WAIT)

#define HDCDRV_CLIENT_DESTROY _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_CLIENT_DESTROY)

#define HDCDRV_WAIT_CTRL_MSG _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_WAIT_CTRL_MSG)
#define HDCDRV_SEND_CTRL_MSG _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_SEND_CTRL_MSG)
#define HDCDRV_INIT_CTRL_NODE _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_INIT_CTRL_NODE)
#define HDCDRV_UNINIT_CTRL_NODE _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_UNINIT_CTRL_NODE)
/* ----------------------------------------------*
 * Internal function prototype description                             *
 *---------------------------------------------- */
/* ----------------------------------------------*
 * Global variable                                     *
 *---------------------------------------------- */
/* ----------------------------------------------*
 * Module-level variable                                   *
 *---------------------------------------------- */
/* ----------------------------------------------*
 * Constant definitions                                     *
 *---------------------------------------------- */
#define HDC_TRANS_USE_UB 2
#ifdef CFG_FEATURE_SUPPORT_UB

// When the value below changes, the value of the kernel_space macro with the same name also needs to be modified.
#define HDCDRV_UB_SINGLE_DEV_MAX_SESSION 272 /* 64 * 4(log + tsd + dvpp + reserved) + 8(short) + 8(short_extend) */
struct hdc_ub_context;
typedef struct hdc_ub_context hdc_ub_context_t;

struct hdc_urma_info;
typedef struct hdc_urma_info hdc_urma_info_t;

struct hdc_ub_epoll_node;
typedef struct hdc_ub_epoll_node hdc_ub_epoll_node_t;

struct hdc_time_record_for_single_send;
typedef struct hdc_time_record_for_single_send HdcTimeRecordForSingleSend_t;

struct hdc_time_record_for_single_recv;
typedef struct hdc_time_record_for_single_send hdc_time_record_for_single_send;

typedef struct hdc_ub_res_info {
    unsigned int l_jfs_id;
    unsigned int l_jfr_id;
    unsigned int l_jfc_s_id;
    unsigned int l_jfc_r_id;
    signed int l_jfce_r_fd; // According to urma_type.h, jfce->fd is int
} hdc_ub_res_info_t;

struct hdc_ub_session {
    unsigned int local_id;
    unsigned int remote_id;
    unsigned int lock_idx;
    int dev_id;
    int service_type;
    int status; // only use 0 or 1
    unsigned long long create_pid;
    unsigned long long peer_create_pid;
    struct hdc_client_head *client_head;
    struct hdc_server_head *server_head;
    int local_close_state;
    int remote_close_state;
    unsigned int unique_val;
    hdc_ub_context_t *ctx;
    struct list_head node;
    hdc_ub_dbg_stat_t dbg_stat;
    hdcdrv_jetty_info_t jetty_info;
    void *psession_ptr;
    int bind_fd;
    uint64_t user_va;
    hdc_ub_send_recv_info_t send_recv_info;
    hdc_ub_res_info_t ub_res_info;
    hdc_ub_epoll_node_t *epoll_event_node;
    int recv_eventfd;
};

struct hdc_session_notify_mng {
    int valid;
    struct HdcSessionNotify notify;
};

struct hdc_session_base_info {
    signed char status;
    unsigned int unique_val;
    struct hdc_ub_session *session;
};

#endif
struct hdc_session {
    unsigned int magic;
    unsigned int device_id;
    signed int sockfd;
    unsigned int type;
    mmProcess bind_fd;
    unsigned int session_cur_alloc_idx;
#ifdef CFG_FEATURE_SUPPORT_UB
    struct hdc_ub_session *ub_session;
    struct hdc_epoll_head *epoll_head;
    int close_eventfd;
#endif
};

struct hdc_client_session {
    struct hdc_session session;
    bool alloc;
    signed int node;
    signed int devid;
    unsigned int portno;
    unsigned int servaddr;
    struct hdc_client_head *client;
};

struct hdc_client_head {
    unsigned int magic;
    signed int serviceType;
    unsigned int flag;
    unsigned int maxSessionNum;
    mmMutex_t mutex;
    struct hdc_client_session session[0];
};

struct hdc_server_session {
    struct hdc_session session;
#ifdef CFG_FEATURE_PRESET_SESSION
    bool alloc;
#endif
    unsigned int deviceId;
    struct sockaddr_in client_addr;
    struct hdc_server_head *server;
};

struct hdc_server_head {
    unsigned int magic;
    signed int serviceType;
    unsigned int session_num;
    unsigned int portno;
    unsigned int servaddr;
    mmSockHandle listenFd;
    signed int deviceId;
    mmProcess bind_fd;
    mmMutex_t mutex;
    int accept_wait;
    struct hdc_server_session session[0];
#ifdef CFG_FEATURE_SUPPORT_UB
    int conn_wait;
    int conn_notify;
#endif
};

struct hdc_epoll_head {
    unsigned int magic;
    mmSockHandle epfd;
    union {
        mmProcess bind_fd;     /* for hdc device fd over pcie */
        HDC_SERVER hdc_server; /* HDC Server handle, use in UB scenario */
    };
    int size;
    struct epoll_event *epoll_events;
#ifdef CFG_FEATURE_SUPPORT_UB
    struct list_head session_list;
    struct list_head server_list;
    mmMutex_t epoll_lock;
#endif
};

struct hdc_msg_head {
    bool freeBuf;
    struct drvHdcMsg msg;
};

#define HDC_MAX_GROUP_NUM 8
struct hdcConfig {
    unsigned int portno[HDC_SERVICE_TYPE_MAX];
    signed int socket_segment;
    signed int pcie_segment;
    enum halHdcTransType trans_type;
    mmProcess pcie_handle;
    char ipaddr[HDC_IPADDR_TYPE_NUM][HDC_IPADDR_STR_LEN];
    bool config_set_flag;
    signed int h2d_type;
#ifdef CFG_FEATURE_SUPPORT_UB
    hdc_urma_info_t *urma_attr[HDC_MAX_UB_DEV_CNT];
    mmMutex_t urma_attr_lock[HDC_MAX_UB_DEV_CNT];
    HDC_SERVER server_list[HDC_MAX_UB_DEV_CNT][HDCDRV_SUPPORT_MAX_SERVICE];
    mmMutex_t session_lock[HDC_MAX_UB_DEV_CNT * HDCDRV_UB_SINGLE_DEV_MAX_SESSION]; // used when close session
    struct hdc_session_base_info info_list[HDC_MAX_UB_DEV_CNT * HDCDRV_UB_SINGLE_DEV_MAX_SESSION]; // session base info
    int f_pid;
    struct hdc_session_notify_mng notify_list[HDC_SERVICE_TYPE_MAX];
#endif
};

/* Need to consider the self-sequencing issue. */
union hdc_ip_addr {
    unsigned int ipaddr;
    struct {
        UINT8 one;
        UINT8 two;
        UINT8 three;
        UINT8 four;
    } byte;
};

struct hdc_recv_config {
    signed int wait;
    unsigned int timeout;
    int group_flag;
    int buf_count;
};

static inline char *StrError(int errnum)
{
    return strerror(errnum);
}

/* hdcConfig needs to be modified according to the situation of subsequent iterations;
   this only guarantees that the functions of iteration five are available. */
extern struct hdcConfig g_hdcConfig;

extern hdcError_t hdc_init_mutex(struct hdcConfig *hdcConfig);
extern drvError_t drv_hdc_get_peer_dev_id(signed int devId, signed int *peerDevId);

extern hdcError_t hdc_socket_recv_peek(signed int sockfd, unsigned int *msg_len, signed int wait, unsigned int timeout);
extern signed int drv_hdc_socket_recv(signed int sockfd, char *pBuf, unsigned int msgLen, unsigned int *recvLen);
extern signed int drv_hdc_socket_send(signed int sockfd, const char *buf, signed int len);
extern void *drv_hdc_zalloc(size_t size);
extern drvError_t drv_hdc_socket_session_connect(int dev_id, signed int server_pid, PPC_SESSION *session);
extern drvError_t drv_hdc_socket_server_create(int dev_id, signed int server_pid, PPC_SERVER *server);
extern drvError_t drv_hdc_socket_session_accept(PPC_SERVER server, PPC_SESSION *session);
extern signed int hdc_ioctl(mmProcess handle, signed int ioctl_code, union hdcdrv_cmd *hdcCmd);
extern hdcError_t hdc_pcie_init(struct hdcConfig *hdcConfig);

#endif /* __HDC_CMN_H__ */
