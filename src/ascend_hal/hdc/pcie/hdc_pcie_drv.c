/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "mmpa_api.h"

#include "ascend_hal.h"
#include "hdc_pcie_drv.h"
#include "hdcdrv_cmd_ioctl.h"

#ifndef ERESTARTSYS
#define ERESTARTSYS 512
#endif

struct hdc_mem_fd_mng g_mem_fd_mng = {0};

mmProcess hdc_pcie_open(void)
{
    return mmOpen2(PCIE_DEV_NAME, M_RDONLY, M_IRUSR);
}

STATIC signed int get_pid(void)
{
    return getpid();
}

#ifndef TMP_UT
STATIC void hdc_mem_stat_init(struct hdc_mem_fd_mng *fdMng)
{
#ifdef HDC_DFX
    struct hdc_mem_stat_info *mem_stat_info = &(fdMng->mem_stat);

    mem_stat_info->huge_alloc_count = 0;
    mem_stat_info->huge_alloc_size = 0;
    mem_stat_info->huge_free_count = 0;
    mem_stat_info->huge_free_size = 0;
    mem_stat_info->va32bit_alloc_count = 0;
    mem_stat_info->va32bit_alloc_size = 0;
    mem_stat_info->va32bit_free_count = 0;
    mem_stat_info->va32bit_free_size = 0;
    mem_stat_info->total_alloc_count = 0;
    mem_stat_info->total_alloc_size = 0;
    mem_stat_info->total_free_count = 0;
    mem_stat_info->total_free_size = 0;

#endif
}
#endif

mmProcess hdc_pcie_mem_bind_fd(void)
{
    struct hdc_mem_fd_mng *fd_mng = &g_mem_fd_mng;
    mmProcess fd = HDC_SESSION_FD_INVALID;
    pid_t pid;

    pid = get_pid();

    (void)mmMutexLock(&fd_mng->mutex);

    if (fd_mng->mem_fd > 0) {
#ifndef TMP_UT
        if (fd_mng->mem_pid == pid) {
            fd_mng->count++;
            (void)mmMutexUnLock(&fd_mng->mutex);
            return fd_mng->mem_fd;
        } else {
            (void)mm_close_file(fd_mng->mem_fd);
            fd_mng->mem_fd = 0;
            hdc_mem_stat_init(fd_mng);
        }
#endif
    }

    fd = mmOpen2(PCIE_DEV_NAME, M_RDWR, M_IRUSR);
    if (fd != (mmProcess)EN_ERROR) {
        fd_mng->mem_fd = fd;
        fd_mng->mem_pid = pid;
        fd_mng->count = 1;
    }

    (void)mmMutexUnLock(&fd_mng->mutex);
    return fd;
}

void hdc_pcie_mem_unbind_fd(void)
{
    struct hdc_mem_fd_mng *fd_mng = &g_mem_fd_mng;
    pid_t pid = get_pid();

    (void)mmMutexLock(&fd_mng->mutex);
    if ((fd_mng->mem_fd > 0) && (fd_mng->mem_pid == pid)) {
        fd_mng->count--;
        /* close by process quit */
    }
    (void)mmMutexUnLock(&fd_mng->mutex);
}

#ifndef TMP_UT
void hdc_sid_copy(char *dst_session_id, signed int dst_len, char *src_session_id, signed int src_len)
{
    UINT64 *dst = (UINT64 *)dst_session_id;
    UINT64 *src = (UINT64 *)src_session_id;

    (void)dst_len;
    (void)src_len;

    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}
#endif

signed int hdc_ioctl(mmProcess handle, signed int ioctl_code, union hdcdrv_cmd *hdcCmd)
{
    int ret;
    mmIoctlBuf ioctl_buf;

    ioctl_buf.inbuf = hdcCmd;
    ioctl_buf.inbufLen = sizeof(union hdcdrv_cmd);
    ioctl_buf.outbuf = hdcCmd;
    ioctl_buf.outbufLen = sizeof(union hdcdrv_cmd);

    ioctl_buf.oa = NULL;
    do {
        ret = mmIoctl(handle, ioctl_code, &ioctl_buf);
        if (ret != 0) {
            ret = mm_get_error_code();
            HDC_SHARE_LOG_READ_ERR();
        }
        HDC_SHARE_LOG_READ_INFO();
    } while (ret == EINTR);

    if (ret == -HDCDRV_NOT_SUPPORT) {
        ret = DRV_ERROR_NOT_SUPPORT;
    }
    return ret;
}

signed int hdc_pcie_get_peer_devid(mmProcess handle, signed int devId, signed int *peerDevId)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    hdcCmd.get_peer_dev_id.dev_id = devId;

    ret = hdc_ioctl(handle, HDCDRV_GET_PEER_DEV_ID, &hdcCmd);

    *peerDevId = hdcCmd.get_peer_dev_id.peer_dev_id;

    return ret;
}

signed int hdc_pcie_config(mmProcess handle, signed int *segMent)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.config.segment = *segMent;
    hdcCmd.config.dev_id = HDCDRV_DEFAULT_DEV_ID;

    ret = hdc_ioctl(handle, HDCDRV_HDCDRV_CONFIG, &hdcCmd);

    *segMent = hdcCmd.config.segment;

    return ret;
}

signed int hdc_pcie_set_service_level(mmProcess handle, signed int serviceType)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.set_level.service_type = serviceType;
    hdcCmd.set_level.dev_id = HDCDRV_DEFAULT_DEV_ID;

    switch (serviceType) {
        case HDC_SERVICE_TYPE_FRAMEWORK:
        case HDC_SERVICE_TYPE_TDT:
        case HDC_SERVICE_TYPE_TSD:
            hdcCmd.set_level.level = HDC_SERVICE_LEVEL_0;
            break;
        default:
            hdcCmd.set_level.level = HDC_SERVICE_LEVEL_1;
            break;
    }

    ret = hdc_ioctl(handle, HDCDRV_SET_SERVICE_LEVEL, &hdcCmd);

    return ret;
}

signed int hdc_pcie_server_create(mmProcess handle, signed int devId, signed int serviceType)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.server_create.dev_id = devId;
    hdcCmd.server_create.service_type = serviceType;

    ret = hdc_ioctl(handle, HDCDRV_SERVER_CREATE, &hdcCmd);

    return ret;
}

signed int hdc_pcie_server_destroy(mmProcess handle, signed int devId, signed int serviceType)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.server_destroy.dev_id = devId;
    hdcCmd.server_destroy.service_type = serviceType;

    ret = hdc_ioctl(handle, HDCDRV_SERVER_DESTROY, &hdcCmd);

    return ret;
}

signed int hdc_pcie_accept(mmProcess handle, signed int devId, signed int serviceType, struct hdc_session *pSession)
{
    union hdcdrv_cmd hdcCmd;
    int retry_cnt = 0;
    signed int ret;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.accept.dev_id = devId;
    hdcCmd.accept.service_type = serviceType;

    do {
        ret = hdc_ioctl(handle, HDCDRV_ACCEPT, &hdcCmd);
        if (ret == 0) {
            pSession->sockfd = hdcCmd.accept.session;
            pSession->device_id = (unsigned int)devId;
            pSession->session_cur_alloc_idx = hdcCmd.accept.session_cur_alloc_idx;
            return 0;
        }
        if (ret == ERESTARTSYS) {
            retry_cnt++;
            continue;
        }
        return ret;
    } while (retry_cnt < 1000); // 1000 is retry times

    return ret;
}

signed int hdc_pcie_connect(mmProcess handle, signed int devId, signed int serviceType, unsigned int flag,
                            signed int peerpid, struct hdc_session *pSession)
{
    struct timespec now_time = {0, 0};
    unsigned long start_time = 0;
    unsigned long cost_time = 0;
    union hdcdrv_cmd hdcCmd;
    signed int ret;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.connect.dev_id = devId;
    hdcCmd.connect.service_type = serviceType;
    hdcCmd.connect.peer_pid = peerpid;
    hdcCmd.connect.timeout = (flag & CONVERT_TO_TIMEOUT_MASK);

    (void)clock_gettime(CLOCK_MONOTONIC, &now_time);
    start_time = (unsigned long)((now_time.tv_sec * CONVERT_MS_TO_S) + (now_time.tv_nsec / CONVERT_MS_TO_NS));

    while (1) {
        ret = hdc_ioctl(handle, HDCDRV_CONNECT, &hdcCmd);
        if (ret == 0) {
            pSession->sockfd = hdcCmd.connect.session;
            pSession->device_id = (unsigned int)devId;
            pSession->session_cur_alloc_idx = hdcCmd.connect.session_cur_alloc_idx;
            return 0;
        } else if (ret == ERESTARTSYS) {
            (void)clock_gettime(CLOCK_MONOTONIC, &now_time);
            cost_time = ((unsigned long)((now_time.tv_sec * CONVERT_MS_TO_S) + (now_time.tv_nsec / CONVERT_MS_TO_NS)) -
                         start_time);
            if (cost_time > (hdcCmd.connect.timeout * CONVERT_MS_TO_S)) {
                return -HDCDRV_CONNECT_TIMEOUT;
            }
        } else {
            return ret;
        }
    }
}

signed int hdc_pcie_close(mmProcess handle, unsigned int devId, const struct hdc_session *pSession)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;
    (void)devId;

    if ((handle == (mmProcess)EN_ERROR) || (pSession == NULL)) {
        HDC_LOG_INFO("Handle is not available or session is NULL. (handle=%d;pSession_is_null=%d;)\n", handle,
                     (pSession == NULL));
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.close.session = pSession->sockfd;
    hdcCmd.close.dev_id = (int)pSession->device_id;
    hdcCmd.close.session_cur_alloc_idx = pSession->session_cur_alloc_idx;

    ret = hdc_ioctl(handle, HDCDRV_CLOSE, &hdcCmd);

    return ret;
}

signed int hdc_pcie_send(mmProcess handle, const struct hdc_session *pSession, struct drvHdcMsg *pMsg, signed int wait,
                         unsigned int timeout)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;
    struct timespec now_time = {0, 0};
    unsigned long start_time;
    unsigned long cost_time = 0;

    if ((handle == (mmProcess)EN_ERROR) || (pSession == NULL)) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.send.session = pSession->sockfd;
    hdcCmd.send.src_buf = pMsg->bufList[0].pBuf;
    hdcCmd.send.len = pMsg->bufList[0].len;
    hdcCmd.send.wait_flag = wait;
    hdcCmd.send.timeout = timeout;
    hdcCmd.send.dev_id = (int)pSession->device_id;

    if (wait == HDC_WAIT_TIMEOUT) {
        (void)clock_gettime(CLOCK_MONOTONIC, &now_time);
        start_time = (unsigned long)((now_time.tv_sec * CONVERT_MS_TO_S) + (now_time.tv_nsec / CONVERT_MS_TO_NS));
    }

retry:
    ret = hdc_ioctl(handle, HDCDRV_SEND, &hdcCmd);

    if (ret == ERESTARTSYS) {
        if (wait == HDC_WAIT_TIMEOUT) {
            (void)clock_gettime(CLOCK_MONOTONIC, &now_time);
            cost_time = ((unsigned long)((now_time.tv_sec * CONVERT_MS_TO_S) + (now_time.tv_nsec / CONVERT_MS_TO_NS)) -
                         start_time);
            if (cost_time < (unsigned long)timeout) {
                hdcCmd.send.timeout = timeout - (unsigned int)cost_time;
                goto retry;
            } else {
                return -HDCDRV_TX_TIMEOUT;
            }
        } else {
            goto retry;
        }
    }

    return ret;
}

signed int hdc_pcie_recv_peek(mmProcess handle, const struct hdc_session *pSession, signed int *len,
                              struct hdc_recv_config *recvConfig)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;
    struct timespec now_time = {0, 0};
    unsigned long start_time = 0;
    unsigned long cost_time = 0;

    if ((handle == (mmProcess)EN_ERROR) || (pSession == NULL)) {
        HDC_LOG_INFO("Handle is not available or session is NULL. (handle=%d;pSession_is_null=%d;)\n", handle,
                     (pSession == NULL));
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.recv_peek.session = pSession->sockfd;
    hdcCmd.recv_peek.wait_flag = recvConfig->wait;
    hdcCmd.recv_peek.dev_id = (int)pSession->device_id;
    hdcCmd.recv_peek.timeout = recvConfig->timeout;
    hdcCmd.recv_peek.group_flag = recvConfig->group_flag;
    hdcCmd.recv_peek.session_cur_alloc_idx = pSession->session_cur_alloc_idx;

    if (recvConfig->wait == HDC_WAIT_TIMEOUT) {
        (void)clock_gettime(CLOCK_MONOTONIC, &now_time);
        start_time = (unsigned long)((now_time.tv_sec * CONVERT_MS_TO_S) + (now_time.tv_nsec / CONVERT_MS_TO_NS));
    }

retry:
    ret = hdc_ioctl(handle, HDCDRV_RECV_PEEK, &hdcCmd);

    if (ret == 0) {
        /* len == 0 mean remote session is closed */
        *len = hdcCmd.recv_peek.len;
        recvConfig->buf_count = hdcCmd.recv_peek.count;
    }
    if (ret == ERESTARTSYS) {
        if (recvConfig->wait == HDC_WAIT_TIMEOUT) {
            (void)clock_gettime(CLOCK_MONOTONIC, &now_time);
            cost_time = ((unsigned long)((now_time.tv_sec * CONVERT_MS_TO_S) + (now_time.tv_nsec / CONVERT_MS_TO_NS)) -
                         start_time);
            if (cost_time < recvConfig->timeout) {
                hdcCmd.recv_peek.timeout = recvConfig->timeout - (unsigned int)cost_time;
                goto retry;
            } else {
                return -HDCDRV_RX_TIMEOUT;
            }
        } else {
            goto retry;
        }
    }

    return ret;
}

signed int hdc_pcie_recv(mmProcess handle, const struct hdc_session *pSession, char *buf, signed int len,
                         signed int *outLen, struct hdc_recv_config *recvConfig)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if ((handle == (mmProcess)EN_ERROR) || (pSession == NULL)) {
        HDC_LOG_INFO("Handle is not available or session is NULL. (handle=%d;pSession_is_null=%d;)\n", handle,
                     (pSession == NULL));
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.recv.session = pSession->sockfd;
    hdcCmd.recv.dst_buf = buf;
    hdcCmd.recv.len = len;
    hdcCmd.recv.dev_id = (int)pSession->device_id;
    hdcCmd.recv.group_flag = recvConfig->group_flag;
    hdcCmd.recv.buf_count = recvConfig->buf_count;

    ret = hdc_ioctl(handle, HDCDRV_RECV, &hdcCmd);
    if (ret == 0) {
        *outLen = hdcCmd.recv.out_len;
    }

    return ret;
}

signed int hdc_pcie_set_session_owner(mmProcess handle, const struct hdc_session *pSession)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.set_owner.session = pSession->sockfd;
    hdcCmd.set_owner.dev_id = (int)pSession->device_id;

    ret = hdc_ioctl(handle, HDCDRV_SET_SESSION_OWNER, &hdcCmd);

    return ret;
}

signed int hdc_pcie_get_session_uid(mmProcess handle, const struct hdc_session *pSession, signed int *root_privilege)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.get_uid_stat.session = pSession->sockfd;
    hdcCmd.get_uid_stat.uid = (unsigned int)-1;
    hdcCmd.get_uid_stat.euid = (unsigned int)-1;
    hdcCmd.get_uid_stat.root_privilege = 0;
    hdcCmd.get_uid_stat.dev_id = (int)pSession->device_id;

    ret = hdc_ioctl(handle, HDCDRV_GET_SESSION_UID, &hdcCmd);
    if (ret == 0) {
        *root_privilege = hdcCmd.get_uid_stat.root_privilege;
    }
    return ret;
}

signed int hdc_pcie_alloc_mem(mmProcess handle, signed int type, const struct hdc_alloc_mem *mem_info, UINT64 va,
                              unsigned int page_type)
{
#ifndef TMP_UT
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.alloc_mem.type = (unsigned int)type;
    hdcCmd.alloc_mem.len = (unsigned int)mem_info->alloc_len;
    hdcCmd.alloc_mem.va = va;
    hdcCmd.alloc_mem.dev_id = mem_info->devid;
    hdcCmd.alloc_mem.map = mem_info->map;
    hdcCmd.alloc_mem.page_type = page_type;

    ret = hdc_ioctl(handle, HDCDRV_ALLOC_MEM, &hdcCmd);
    return ret;
#else
    return 0;
#endif
}

signed int hdc_pcie_free_mem(mmProcess handle, signed int type, UINT64 va, unsigned int *len, unsigned int *page_type)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.free_mem.type = (unsigned int)type;
    hdcCmd.free_mem.va = va;
    hdcCmd.free_mem.dev_id = HDCDRV_DEFAULT_DEV_ID;

    ret = hdc_ioctl(handle, HDCDRV_FREE_MEM, &hdcCmd);
    if (ret == 0) {
        *len = hdcCmd.free_mem.len;
        *page_type = hdcCmd.free_mem.page_type;
    }

    return ret;
}

signed int hdc_pcie_fast_send(mmProcess handle, const struct hdc_session *pSession, signed int wait_flag,
                              const struct drvHdcFastSendMsg *msg, unsigned int timeout)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.fast_send.session = pSession->sockfd;
    hdcCmd.fast_send.wait_flag = wait_flag;
    hdcCmd.fast_send.src_data_addr = msg->srcDataAddr;
    hdcCmd.fast_send.dst_data_addr = msg->dstDataAddr;
    hdcCmd.fast_send.data_len = (signed int)msg->dataLen;
    hdcCmd.fast_send.src_ctrl_addr = msg->srcCtrlAddr;
    hdcCmd.fast_send.dst_ctrl_addr = msg->dstCtrlAddr;
    hdcCmd.fast_send.ctrl_len = (signed int)msg->ctrlLen;
    hdcCmd.fast_send.timeout = timeout;
    hdcCmd.fast_send.dev_id = (int)pSession->device_id;

    ret = hdc_ioctl(handle, HDCDRV_FAST_SEND, &hdcCmd);
    return ret;
}

signed int hdc_pcie_fast_recv(mmProcess handle, const struct hdc_session *pSession, signed int wait_flag,
                              struct drvHdcFastRecvMsg *msg, unsigned int timeout)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.fast_recv.session = pSession->sockfd;
    hdcCmd.fast_recv.wait_flag = wait_flag;
    hdcCmd.fast_recv.dev_id = (int)pSession->device_id;
    hdcCmd.fast_recv.timeout = timeout;

    ret = hdc_ioctl(handle, HDCDRV_FAST_RECV, &hdcCmd);
    if (ret == 0) {
        msg->dataAddr = (UINT64)hdcCmd.fast_recv.data_addr;
        msg->dataLen = (unsigned int)hdcCmd.fast_recv.data_len;
        msg->ctrlAddr = (UINT64)hdcCmd.fast_recv.ctrl_addr;
        msg->ctrlLen = (unsigned int)hdcCmd.fast_recv.ctrl_len;
    }

    return ret;
}

signed int hdc_pcie_dma_map(mmProcess handle, signed int type, UINT64 va, signed int devid)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.dma_map.type = (unsigned int)type;
    hdcCmd.dma_map.va = va;
    hdcCmd.dma_map.dev_id = devid;

    ret = hdc_ioctl(handle, HDCDRV_DMA_MAP, &hdcCmd);

    return ret;
}

signed int hdc_pcie_dma_unmap(mmProcess handle, signed int type, UINT64 va)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.dma_unmap.type = (unsigned int)type;
    hdcCmd.dma_unmap.va = va;
    hdcCmd.dma_unmap.dev_id = HDCDRV_DEFAULT_DEV_ID;

    ret = hdc_ioctl(handle, HDCDRV_DMA_UNMAP, &hdcCmd);

    return ret;
}

signed int hdc_pcie_dma_remap(mmProcess handle, signed int type, UINT64 va, signed int devid)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.dma_remap.type = (unsigned int)type;
    hdcCmd.dma_remap.va = va;
    hdcCmd.dma_remap.dev_id = devid;

    ret = hdc_ioctl(handle, HDCDRV_DMA_REMAP, &hdcCmd);

    return ret;
}

#ifndef TMP_UT
signed int hdc_pcie_register_mem(mmProcess handle, unsigned int type, signed int devid, UINT64 va,
                                 unsigned int alloc_len)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.register_mem.type = type;
    hdcCmd.register_mem.len = alloc_len;
    hdcCmd.register_mem.va = va;
    hdcCmd.register_mem.dev_id = devid;
    hdcCmd.register_mem.flag = 0;

    ret = hdc_ioctl(handle, HDCDRV_REGISTER_MEM, &hdcCmd);

    return ret;
}

signed int hdc_pcie_unregister_mem(mmProcess handle, signed int type, UINT64 va, unsigned int *len,
                                   unsigned int *page_type)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.unregister_mem.type = (unsigned int)type;
    hdcCmd.unregister_mem.va = va;
    hdcCmd.unregister_mem.dev_id = HDCDRV_DEFAULT_DEV_ID;

    ret = hdc_ioctl(handle, HDCDRV_UNREGISTER_MEM, &hdcCmd);
    if (ret == 0) {
        *len = hdcCmd.unregister_mem.len;
        *page_type = hdcCmd.unregister_mem.page_type;
    }

    return ret;
}

signed int hdc_pcie_fast_wait(mmProcess handle, const struct hdc_session *pSession, struct drvHdcFastSendFinishMsg *msg,
                              int timeout, int result_type)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.wait_mem.session = pSession->sockfd;
    hdcCmd.wait_mem.dev_id = (int)pSession->device_id;
    hdcCmd.wait_mem.timeout = timeout;
    hdcCmd.wait_mem.result_type = (unsigned int)result_type;

    ret = hdc_ioctl(handle, HDCDRV_WAIT_MEM_RELEASE, &hdcCmd);
    if (ret != 0) {
        return ret;
    }

    msg->dataAddr = (UINT64)hdcCmd.wait_mem.data_addr;
    msg->dataLen = (unsigned int)hdcCmd.wait_mem.data_len;
    msg->ctrlAddr = (UINT64)hdcCmd.wait_mem.ctrl_addr;
    msg->ctrlLen = (unsigned int)hdcCmd.wait_mem.ctrl_len;
    msg->result = (unsigned int)hdcCmd.wait_mem.result;

    return ret;
}
#endif

signed int hdc_pcie_epoll_alloc_fd(mmProcess handle, signed int size, signed int *epfd)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    hdcCmd.epoll_alloc_fd.size = size;
    hdcCmd.epoll_alloc_fd.dev_id = HDCDRV_DEFAULT_DEV_ID;

    ret = hdc_ioctl(handle, HDCDRV_EPOLL_ALLOC_FD, &hdcCmd);
    if (ret == 0) {
        *epfd = hdcCmd.epoll_alloc_fd.epfd;
    }

    return ret;
}

signed int hdc_pcie_epoll_free_fd(mmProcess handle, signed int epfd)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    hdcCmd.epoll_free_fd.epfd = epfd;
    hdcCmd.epoll_free_fd.dev_id = HDCDRV_DEFAULT_DEV_ID;

    ret = hdc_ioctl(handle, HDCDRV_EPOLL_FREE_FD, &hdcCmd);

    return ret;
}

signed int hdc_pcie_epoll_ctl(mmProcess handle, const struct hdcdrv_cmd_epoll_ctl *epollctl,
                              const struct drvHdcEvent *event)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    hdcCmd.epoll_ctl.epfd = epollctl->epfd;
    hdcCmd.epoll_ctl.op = epollctl->op;
    hdcCmd.epoll_ctl.para1 = epollctl->para1;
    hdcCmd.epoll_ctl.para2 = epollctl->para2;
    hdcCmd.epoll_ctl.event.events = event->events;
    hdcCmd.epoll_ctl.event.data = (UINT64)event->data;
    hdcCmd.epoll_ctl.dev_id = HDCDRV_DEFAULT_DEV_ID;

    ret = hdc_ioctl(handle, HDCDRV_EPOLL_CTL, &hdcCmd);

    return ret;
}

signed int hdc_pcie_epoll_wait(mmProcess handle, struct hdcdrv_cmd_epoll_wait *epollwait)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    hdcCmd.epoll_wait.epfd = epollwait->epfd;
    hdcCmd.epoll_wait.maxevents = epollwait->maxevents;
    hdcCmd.epoll_wait.timeout = epollwait->timeout;
    hdcCmd.epoll_wait.event = epollwait->event;
    hdcCmd.epoll_wait.dev_id = HDCDRV_DEFAULT_DEV_ID;

    ret = hdc_ioctl(handle, HDCDRV_EPOLL_WAIT, &hdcCmd);
    if (ret == 0) {
        epollwait->ready_event = hdcCmd.epoll_wait.ready_event;
    }

    return ret;
}

signed int hdc_pcie_get_session_attr(mmProcess handle, const struct hdc_session *pSession, int attr, int *value)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    hdcCmd.get_session_attr.session = pSession->sockfd;
    hdcCmd.get_session_attr.dev_id = (int)pSession->device_id;
    hdcCmd.get_session_attr.cmd_type = attr;
    hdcCmd.get_session_attr.session_cur_alloc_idx = pSession->session_cur_alloc_idx;

    ret = hdc_ioctl(handle, HDCDRV_GET_SESSION_ATTR, &hdcCmd);
    if (ret == 0) {
        *value = hdcCmd.get_session_attr.output;
    }

    return ret;
}

signed int hdc_pcie_get_page_size(mmProcess handle, unsigned int *page_size, unsigned int *huge_page_size,
                                  unsigned int *page_size_bit)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.get_page_size.dev_id = HDCDRV_DEFAULT_DEV_ID;

    ret = hdc_ioctl(handle, HDCDRV_GET_PAGE_SIZE, &hdcCmd);
    if (ret == 0) {
        *page_size = hdcCmd.get_page_size.page_size;
        *huge_page_size = hdcCmd.get_page_size.hpage_size;
        *page_size_bit = hdcCmd.get_page_size.page_bit;
    }
    return ret;
}

signed int hdc_pcie_get_session_info(mmProcess handle, const struct hdc_session *pSession,
                                     struct drvHdcSessionInfo *info)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.get_session_info.session_fd = pSession->sockfd;
    hdcCmd.get_session_info.dev_id = (int)pSession->device_id;

    hdcCmd.get_session_info.fid = HDCDRV_INVALID_FID;

    ret = hdc_ioctl(handle, HDCDRV_GET_SESSION_FID, &hdcCmd);
    if (ret == 0) {
        info->devid = pSession->device_id;
        info->fid = hdcCmd.get_session_info.fid;
    }

    return ret;
}

signed int hdc_pcie_get_feature_info(mmProcess handle, int device_id, unsigned int feature_type,
                                     unsigned int *is_support, unsigned int *version)
{
    signed int ret;
    union hdcdrv_cmd hdcCmd;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter handle is invalid.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (is_support == NULL || version == NULL) {
        HDC_LOG_ERR("Input parameter is_support or version is null.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    hdcCmd.get_feature_info.feature_type = feature_type;
    hdcCmd.get_feature_info.dev_id = device_id;

    ret = hdc_ioctl(handle, HDCDRV_GET_FEATURE_INFO, &hdcCmd);
    if (ret == 0) {
        *is_support = hdcCmd.get_feature_info.is_support;
        *version = hdcCmd.get_feature_info.version;
    }

    return ret;
}
