/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HDC_PCIE_DRV_H
#define HDC_PCIE_DRV_H

#include "mmpa_api.h"
#include "ascend_hal.h"
#include "hdc_cmn.h"
#include "hdcdrv_cmd_ioctl.h"

/* pcie driver definition */
#define PCIE_DEV_NAME "/dev/hisi_hdc"

#define HDC_DFX

struct hdc_mem_stat_info {
    UINT64 huge_alloc_count;
    UINT64 huge_free_count;
    UINT64 huge_alloc_size;
    UINT64 huge_free_size;
    UINT64 va32bit_alloc_count;
    UINT64 va32bit_free_count;
    UINT64 va32bit_alloc_size;
    UINT64 va32bit_free_size;
    UINT64 total_alloc_count;
    UINT64 total_free_count;
    UINT64 total_alloc_size;
    UINT64 total_free_size;
};

struct hdc_mem_fd_mng {
    mmProcess mem_fd;
    pid_t mem_pid;
    int count;
    mmMutex_t mutex;
#ifdef HDC_DFX
    struct hdc_mem_stat_info mem_stat;
#endif
};

struct hdc_alloc_mem {
    unsigned int alloc_len;
    signed int devid;
    signed int map;
};

extern struct hdc_mem_fd_mng g_mem_fd_mng;

#define HDCDRV_CLIENT_WAKEUP_WAIT _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_CLIENT_WAKEUP_WAIT)
#define HDCDRV_SERVER_WAKEUP_WAIT _IO(HDCDRV_CMD_MAGIC, HDCDRV_CMD_SERVER_WAKEUP_WAIT)

mmProcess hdc_pcie_open(void);

mmProcess hdc_pcie_create_bind_fd(void);
void hdc_pcie_close_bind_fd(mmProcess fd);
mmProcess hdc_pcie_mem_bind_fd(void);
void hdc_pcie_mem_unbind_fd(void);

signed int hdc_pcie_get_peer_devid(mmProcess handle, signed int devId, signed int *peerDevId);
signed int hdc_pcie_config(mmProcess handle, signed int *segMent);
signed int hdc_pcie_set_service_level(mmProcess handle, signed int serviceType);
signed int hdc_pcie_server_create(mmProcess handle, signed int devId, signed int serviceType);
signed int hdc_pcie_server_destroy(mmProcess handle, signed int devId, signed int serviceType);
signed int hdc_pcie_accept(mmProcess handle, signed int devId, signed int serviceType, struct hdc_session *pSession);
signed int hdc_pcie_connect(mmProcess handle, signed int devId, signed int serviceType, unsigned int flag,
                            signed int peerpid, struct hdc_session *pSession);
signed int hdc_pcie_close(mmProcess handle, unsigned int devId, const struct hdc_session *pSession);
signed int hdc_pcie_send(mmProcess handle, const struct hdc_session *pSession, struct drvHdcMsg *pMsg, signed int wait,
                         unsigned int timeout);
signed int hdc_pcie_recv_peek(mmProcess handle, const struct hdc_session *pSession, signed int *len,
                              struct hdc_recv_config *recvConfig);
signed int hdc_pcie_recv(mmProcess handle, const struct hdc_session *pSession, char *buf, signed int len,
                         signed int *outLen, struct hdc_recv_config *recvConfig);
signed int hdc_pcie_set_session_owner(mmProcess handle, const struct hdc_session *pSession);
signed int hdc_pcie_get_session_uid(mmProcess handle, const struct hdc_session *pSession, int *root_privilege);
signed int hdc_pcie_alloc_mem(mmProcess handle, signed int type, const struct hdc_alloc_mem *mem_info, UINT64 va,
                              unsigned int page_type);
signed int hdc_pcie_free_mem(mmProcess handle, signed int type, UINT64 va, unsigned int *len, unsigned int *page_type);
signed int hdc_pcie_fast_send(mmProcess handle, const struct hdc_session *pSession, signed int wait_flag,
                              const struct drvHdcFastSendMsg *msg, unsigned int timeout);
signed int hdc_pcie_fast_recv(mmProcess handle, const struct hdc_session *pSession, signed int wait_flag,
                              struct drvHdcFastRecvMsg *msg, unsigned int timeout);
signed int hdc_pcie_dma_map(mmProcess handle, signed int type, UINT64 va, signed int devid);
signed int hdc_pcie_dma_unmap(mmProcess handle, signed int type, UINT64 va);
signed int hdc_pcie_dma_remap(mmProcess handle, signed int type, UINT64 va, signed int devid);

signed int hdc_pcie_register_mem(mmProcess handle, unsigned int type, signed int devid, UINT64 va,
                                 unsigned int alloc_len);
signed int hdc_pcie_unregister_mem(mmProcess handle, signed int type, UINT64 va, unsigned int *len,
                                   unsigned int *page_type);
signed int hdc_pcie_fast_wait(mmProcess handle, const struct hdc_session *pSession, struct drvHdcFastSendFinishMsg *msg,
                              int timeout, int result_type);

void hdc_sid_copy(char *dst_session_id, int dst_len, char *src_session_id, int src_len);

signed int hdc_pcie_epoll_alloc_fd(mmProcess handle, signed int size, signed int *epfd);
signed int hdc_pcie_epoll_free_fd(mmProcess handle, signed int epfd);
signed int hdc_pcie_epoll_ctl(mmProcess handle, const struct hdcdrv_cmd_epoll_ctl *epollctl,
                              const struct drvHdcEvent *event);
signed int hdc_pcie_epoll_wait(mmProcess handle, struct hdcdrv_cmd_epoll_wait *epollwait);
signed int hdc_pcie_get_session_attr(mmProcess handle, const struct hdc_session *pSession, int attr, int *value);
signed int hdc_pcie_set_session_time_out(mmProcess handle, struct hdc_session *pSession,
                                         struct hdcdrv_timeout *timeout);
signed int hdc_pcie_get_page_size(mmProcess handle, unsigned int *page_size, unsigned int *huge_page_size,
                                  unsigned int *page_size_bit);
signed int hdc_pcie_get_session_info(mmProcess handle, const struct hdc_session *pSession,
                                     struct drvHdcSessionInfo *info);
signed int hdc_pcie_client_wakeup_conn_wait(mmProcess handle, signed int devId, signed int serviceType);
signed int hdc_pcie_server_wakeup_conn_wait(mmProcess handle, signed int devId, signed int serviceType);
signed int drv_hdc_get_max_session_num_by_type(signed int serviceType);
signed int hdc_pcie_get_feature_info(mmProcess handle, int device_id, unsigned int feature_type,
                                     unsigned int *is_support, unsigned int *version);
#endif
