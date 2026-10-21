/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef TRS_IOCTL_H
#define TRS_IOCTL_H

#include <linux/ioctl.h>

#include "drv_type.h"
#include "hal_pkg/trs_pkg.h"
#include "trs_res_id_def.h"
#include "trs_feature_ioctl.h"
#include "trs_stl_comm.h"

#define TRS_MODULE_NAME "TSDRV"
#define DAVINCI_INTF_MODULE_TSDRV_MMAP_NAME "TSDRV_MMAP"

#define TRS_CTRL_USE_SQE_NUM 2

enum trs_uio_addr_type {
    TRS_UIO_SHR_INFO,
    TRS_UIO_HEAD,
    TRS_UIO_TAIL,
    TRS_UIO_DB,
    TRS_UIO_HEAD_REG,
    TRS_UIO_TAIL_REG,
    TRS_UIO_ADDR_MAX,
};

struct trs_uio_info {
    unsigned long sq_que_addr;                    /* input/output */
    unsigned long sq_ctrl_addr[TRS_UIO_ADDR_MAX]; /* input/output */
    uint8_t sq_mem_local_flag;
    uint8_t soft_que_flag; /* output: 1 use soft que, 2 use hw que */
    uint8_t uio_flag;      /* output: 0 not use uio, 1 use uio */
};

struct trs_alloc_para { /* point to struct halSqCqInputInfo res, size <= 24 bytes */
    struct trs_uio_info *uio_info;
};

static inline struct trs_alloc_para *get_alloc_para_addr(struct halSqCqInputInfo *in)
{
    return (struct trs_alloc_para *)&in->res[0]; /* 0 for sq_addr 8 byte align */
}

struct trs_sq_shr_info {
    uint32_t cqe_status;
    uint8_t rsv[1020]; /* 1020 for reduce cache miss, kernel write cqe status, user write stat */
    unsigned long long send_full;
    unsigned long long send_ok;
    uint32_t jfs_id;
    uint32_t jfc_id;
};

struct trs_res_id_usr_para {
    unsigned int stream_pri;
    unsigned int flag;
    unsigned int vfid;
};

struct trs_res_id_para {
    unsigned int tsid;
    int res_type;
    unsigned int id;
    struct trs_res_id_usr_para usr_para;
    unsigned int para;
    unsigned int flag;
    int prop;
    uint32_t value[2]; /* for id addr */
};

struct trs_ssid_query_para {
    int ssid;
};

enum {
    TRS_CONNECT_PROTOCOL_PCIE = 0,
    TRS_CONNECT_PROTOCOL_HCCS,
    TRS_CONNECT_PROTOCOL_UB,
    TRS_CONNECT_PROTOCOL_RC,
    TRS_CONNECT_PROTOCOL_UNKNOWN
};

struct trs_hw_info_query_para {
    int hw_type;
    int tsnum;
    int connection_type;
    int sq_send_mode;
    int die_id;
    uint32_t rsv[3];
};

struct trs_set_close_para {
    uint32_t tsid;
    uint32_t close_type;
    uint32_t rsv[4];
};

struct trs_ub_info_query_para {
    uint32_t tsid;
    unsigned int die_id;
    unsigned int func_id;
    uint32_t rsv[4];
};

#define TRS_CTRL_MSG_MAX_LEN 256
struct trs_ctrl_msg_para {
    uint32_t devid;
    uint32_t tsid;
    uint32_t msg_len; /* in, out */
    uint8_t msg[TRS_CTRL_MSG_MAX_LEN];
    uint32_t rsv[4];
};

struct trs_cmd_res_map {
    unsigned int devid;            /* input */
    struct res_addr_info res_info; /* input */
    unsigned long va;              /* output */
    unsigned int len;              /* output */
};

struct trs_cmd_dma_desc {
    uint32_t tsid;               /* input */
    drvSqCqType_t type;          /* input */
    void *src;                   /* input */
    uint32_t sq_id;              /* input */
    uint32_t sqe_pos;            /* input */
    uint32_t len;                /* input */
    uint32_t dir;                /* input */
    unsigned long long dma_base; /* output */
    unsigned int dma_node_num;   /* output */
    uint32_t rsv[4];
};

struct trs_cmd_cmdlist_map_unmap {
    uint32_t tsid; /* input */
    uint32_t op;   /* input */
};

struct trs_stream_task_para {
    void *stream_mem;
    void *task_info;
    uint32_t task_cnt;
    uint32_t stream_id;
};

struct trs_sq_switch_stream_para {
    struct sq_switch_stream_info *info;
    uint32_t num;
};

struct trs_feature_query_para {
    u32 dev_id;
    u32 bitmap_size;
    trs_feat_bitmap_t bitmap;
};

#define TRS_RES_ID_ALLOC _IOWR('X', 0, struct trs_res_id_para)
#define TRS_RES_ID_FREE _IOW('X', 1, struct trs_res_id_para)
#define TRS_RES_ID_ENABLE _IOW('X', 2, struct trs_res_id_para)
#define TRS_RES_ID_DISABLE _IOW('X', 3, struct trs_res_id_para)
#define TRS_RES_ID_NUM_QUERY _IOWR('X', 4, struct trs_res_id_para)
#define TRS_RES_ID_MAX_QUERY _IOWR('X', 5, struct trs_res_id_para)
#define TRS_RES_ID_USED_NUM_QUERY _IOWR('X', 6, struct trs_res_id_para)
#define TRS_RES_ID_AVAIL_NUM_QUERY _IOWR('X', 7, struct trs_res_id_para)
#define TRS_RES_ID_REG_OFFSET_QUERY _IOWR('X', 8, struct trs_res_id_para)
#define TRS_RES_ID_REG_SIZE_QUERY _IOWR('X', 9, struct trs_res_id_para)
#define TRS_RES_ID_CFG _IOW('X', 10, struct trs_res_id_para)

#define TRS_SSID_QUERY _IOWR('X', 11, struct trs_ssid_query_para)
#define TRS_HW_INFO_QUERY _IOWR('X', 12, struct trs_hw_info_query_para)
#define TRS_MSG_CTRL _IOWR('X', 13, struct trs_ctrl_msg_para)

#define TRS_SQCQ_ALLOC _IOWR('X', 15, struct halSqCqInputInfo)
#define TRS_SQCQ_FREE _IOW('X', 16, struct halSqCqFreeInfo)
#define TRS_SQCQ_CONFIG _IOW('X', 17, struct halSqCqConfigInfo)
#define TRS_SQCQ_QUERY _IOWR('X', 18, struct halSqCqQueryInfo)
#define TRS_SQCQ_SEND _IOW('X', 19, struct halTaskSendInfo)
#define TRS_SQCQ_RECV _IOWR('X', 20, struct halReportRecvInfo)

#define TRS_STL_BIND _IO('X', 21)
#define TRS_STL_LAUNCH _IOW('X', 22, struct trs_stl_launch_para)
#define TRS_STL_QUERY _IOR('X', 23, struct trs_stl_query_para)

#define TRS_ID_RES_MAP _IOR('X', 24, struct trs_cmd_res_map)
#define TRS_UB_INFO_QUERY _IOR('X', 25, struct trs_ub_info_query_para)
#define TRS_SET_CLOSE_TYPE _IOR('X', 26, struct trs_set_close_para)
#define TRS_ID_SQCQ_GET _IOWR('X', 27, struct halSqCqInputInfo)
#define TRS_ID_SQCQ_RESTORE _IOWR('X', 28, struct halSqCqInputInfo)
#define TRS_DMA_DESC_CREATE _IOWR('X', 29, struct trs_cmd_dma_desc)
#define TRS_TS_CMDLIST_MAP_UNMAP _IOWR('X', 30, struct trs_cmd_cmdlist_map_unmap)
#define TRS_STREAM_TASK_FILL _IOWR('X', 31, struct trs_stream_task_para)
#define TRS_SQ_SWITCH_STREAM _IOWR('X', 32, struct trs_sq_switch_stream_para)

#define TRS_MAX_CMD 33

#endif
