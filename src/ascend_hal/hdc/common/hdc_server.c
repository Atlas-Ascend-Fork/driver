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
#include "hdc_cmn.h"

#include "hdcdrv_cmd_ioctl.h"
#include "hdc_pcie_drv.h"
#include "hdc_ub_drv.h"
#include "hdc_server.h"
#include "hdc_core.h"

STATIC void drv_hdc_preset_session_init(struct hdc_server_head *pHead, int session_num)
{
#ifdef CFG_FEATURE_PRESET_SESSION
    int i;
    for (i = 0; i < session_num; ++i) {
        pHead->session[i].alloc = false;
        pHead->session[i].session.magic = HDC_MAGIC_WORD;
        pHead->session[i].session.sockfd = -1;
        pHead->session[i].session.bind_fd = HDC_SESSION_FD_INVALID;
    }
#else
    (void)pHead;
    (void)session_num;
#endif
}

STATIC hdcError_t drv_hdc_pcie_server_create(signed int devid, signed int serviceType, HDC_SERVER *pServer)
{
    struct hdc_server_head *pHead = NULL;
    mmProcess fd;
    signed int ret;
    size_t session_size = 0, size;
    int session_num = drv_hdc_get_max_session_num_by_type(serviceType);

#ifdef CFG_FEATURE_PRESET_SESSION
    session_size = (size_t)session_num * sizeof(struct hdc_server_session);
#endif
    size = sizeof(struct hdc_server_head) + session_size;
    pHead = malloc(size);
    if (pHead == NULL) {
        HDC_LOG_ERR("Call malloc failed.\n");
        hdc_report_out_of_mem_err_msg("drvHdcServerCreate", size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    pHead->serviceType = serviceType;

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
        ret = hdc_pcie_set_service_level(g_hdcConfig.pcie_handle, serviceType);
        if (ret != DRV_ERROR_NONE) {
            free(pHead);
            pHead = NULL;
            HDC_LOG_ERR("HDC init, set service level failed. (errno=%d)\n", ret);
            if (ret == (-HDCDRV_DEVICE_NOT_READY)) {
                /* 17 mean HDCDRV_DEVICE_NOT_READY in kernel */
                return DRV_ERROR_DEVICE_NOT_READY;
            }
            return DRV_ERROR_IOCRL_FAIL;
        }
    }

    fd = hdc_pcie_create_bind_fd();
    if (fd == (mmProcess)EN_ERROR) {
        free(pHead);
        pHead = NULL;
        HDC_LOG_ERR("Server create open pcie device failed. (strerror=\"%s\")\n", strerror(errno));
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        ret = hdc_ub_server_create(fd, devid, serviceType, pHead);
    } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
        ret = hdc_pcie_server_create(fd, devid, serviceType);
    } else {
        HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
        ret = DRV_ERROR_INVALID_HANDLE;
    }
    if (ret != DRV_ERROR_NONE) {
        free(pHead);
        pHead = NULL;
        hdc_pcie_close_bind_fd(fd);
        fd = HDC_SESSION_FD_INVALID;

        /* 17 mean HDCDRV_DEVICE_NOT_READY in kernel */
        if (ret == (-HDCDRV_DEVICE_NOT_READY)) {
            return DRV_ERROR_DEVICE_NOT_READY;
        } else if (ret == (-HDCDRV_SERVICE_LISTENING)) {
            HDC_LOG_WARN("Server has already been created. (listen_device=%d, serviceType=%d) \n", devid, serviceType);
            return DRV_ERROR_SERVER_HAS_BEEN_CREATED;
        } else {
            HDC_LOG_ERR("Create server failed. (listen_device=%d; ret=%d) \n", devid, ret);
            return DRV_ERROR_SERVER_CREATE_FAIL;
        }
    }

    pHead->bind_fd = fd;
    pHead->listenFd = devid;
    drv_hdc_preset_session_init(pHead, session_num);
    *pServer = (HDC_SERVER)pHead;
    return DRV_ERROR_NONE;
}

#ifdef CFG_FEATURE_PRESET_SESSION
STATIC void drv_hdc_server_alloc_session(struct hdc_server_head *pServ, struct hdc_server_session **pp_serv_session)
{
    int session_num = drv_hdc_get_max_session_num_by_type(pServ->serviceType);
    struct hdc_server_session *pSession = NULL;
    int i = 0;

    *pp_serv_session = NULL;
    (void)mmMutexLock(&pServ->mutex);
    pSession = pServ->session;
    for (i = 0; i < session_num; ++i) {
        if (!pSession[i].alloc) {
            pSession[i].alloc = true;
            break;
        }
    }

    (void)mmMutexUnLock(&pServ->mutex);
    if (i == session_num) {
        HDC_LOG_ERR("no free server session.\n");
        return;
    }

    *pp_serv_session = &pSession[i];
    return;
}

STATIC void drv_hdc_server_free_session(struct hdc_server_head *pServ, struct hdc_server_session *p_serv_session)
{
    (void)mmMutexLock(&pServ->mutex);
    p_serv_session->alloc = false;
    p_serv_session->session.sockfd = -1;
    (void)mmMutexUnLock(&pServ->mutex);
}
#endif

STATIC hdcError_t drv_hdc_pcie_session_accept(HDC_SERVER server, HDC_SESSION *pSession)
{
    struct hdc_server_head *pServ = NULL;
    struct hdc_server_session *p_serv_session = NULL;
    signed int ret, deviceId, serviceType;
    struct hdc_session *session = NULL;

    pServ = (struct hdc_server_head *)server;
    deviceId = pServ->deviceId;
    serviceType = pServ->serviceType;
#ifdef CFG_FEATURE_PRESET_SESSION
    drv_hdc_server_alloc_session(pServ, &p_serv_session);
#else
    p_serv_session = (struct hdc_server_session *)drv_hdc_zalloc(sizeof(struct hdc_server_session));
#endif
    if (p_serv_session == NULL) {
        HDC_LOG_ERR("Call malloc failed.\n");
        hdc_report_out_of_mem_err_msg("drvHdcSessionAccept", sizeof(struct hdc_server_session));
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    session = &p_serv_session->session;
    session->bind_fd = HDC_SESSION_FD_INVALID;

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        ret = hdc_ub_accept(pServ, deviceId, serviceType, &p_serv_session->session);
    } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
        ret = hdc_pcie_accept(g_hdcConfig.pcie_handle, deviceId, serviceType, &p_serv_session->session);
    } else {
        HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
        ret = DRV_ERROR_INVALID_HANDLE;
    }
    if (ret != DRV_ERROR_NONE) {
#ifdef CFG_FEATURE_PRESET_SESSION
        drv_hdc_server_free_session(pServ, p_serv_session);
#else
        free(p_serv_session);
        p_serv_session = NULL;
#endif
        if (ret == -HDCDRV_PEER_REBOOT) {
            HDC_LOG_INFO("peer reset. (dev_id=%d; server=%d; ret=%d)\n", pServ->deviceId, pServ->serviceType, ret);
            return DRV_ERROR_DEV_PROCESS_HANG;
        }

        if (ret == -HDCDRV_DEVICE_RESET) {
            HDC_LOG_INFO("Server is destroyed or wakeup wait. (dev_id=%d; server=%d; ret=%d)\n", pServ->deviceId,
                         pServ->serviceType, ret);
            return DRV_ERROR_DEVICE_NOT_READY;
        }

        if (ret == -HDCDRV_DEVICE_NOT_READY) {
            HDC_LOG_RUN_INFO_LIMIT("Dev not ready or has been freed. (dev_id=%d; server=%d; ret=%d)\n", pServ->deviceId,
                                   pServ->serviceType, ret);
        } else {
            HDC_LOG_ERR("Accept failed. (dev_id=%d; server=%d; ret=%d)\n", pServ->deviceId, pServ->serviceType, ret);
        }

        return DRV_ERROR_SOCKET_ACCEPT;
    }

    *pSession = p_serv_session;
    return DRV_ERROR_NONE;
}

STATIC hdcError_t drv_hdc_server_create_para_check(signed int devid, signed int serviceType, const HDC_SERVER *pServer)
{
    if ((devid >= hdc_get_max_device_num()) || (devid < 0)) {
        HDC_LOG_ERR("Input parameter is error. (dev_id=%d)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (pServer == NULL) {
        HDC_LOG_ERR("Input parameter is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((serviceType < 0) || (serviceType >= HDC_SERVICE_TYPE_MAX)) {
        HDC_LOG_ERR("Input parameter is error. (service_type=%d)\n", serviceType);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drvHdcServerCreate
 * Function Description  : Create and initialize HDC Server
 * Input Parameters      : signed int devid             only support [0, 64)
 *                        signed int serviceType       select server type
 * Output Parameters     : HDC_SERVER *server     Created HDC server
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcServerCreate(signed int devid, signed int serviceType, HDC_SERVER *pServer)
{
    struct hdc_server_head *pHead = NULL;
    hdcError_t ret;

    ret = drv_hdc_server_create_para_check(devid, serviceType, pServer);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) {
        ret = drv_hdc_pcie_server_create(devid, serviceType, (HDC_SERVER *)(&pHead));
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
        HDC_LOG_INFO("Create server success. (listen_device=%d)\n", devid);
    } else {
        ret = drv_hdc_socket_server_create(devid, serviceType, (PPC_SERVER *)(&pHead));
        if (ret != DRV_ERROR_NONE) {
            HDC_LOG_ERR("Ppc server create failed. (errno=%d)\n", ret);
            return DRV_ERROR_SERVER_CREATE_FAIL;
        }

        HDC_LOG_INFO("Create ppc server success. (service_type=%d) \n", serviceType);
    }

    pHead->serviceType = serviceType;
    pHead->magic = HDC_MAGIC_WORD;
    pHead->session_num = 0;
    pHead->deviceId = devid;
    (void)mmMutexInit(&pHead->mutex);

    *pServer = (HDC_SERVER)pHead;

    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drvHdcServerDestroy
 * Function Description  : Release HDC Server
 * Input Parameters      : HDC_SERVER server  HDC server to be released
 * Output Parameters     : None
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcServerDestroy(HDC_SERVER server)
{
    signed int ret;
    struct hdc_server_head *pServ = NULL;
    unsigned int session_num;
    mmSockHandle listenFd;
    int retry_time = 0;

    if (server == NULL) {
        HDC_LOG_ERR("Input parameter server is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    pServ = (struct hdc_server_head *)server;
    if ((pServ == NULL) || (pServ->deviceId >= hdc_get_max_device_num()) || (pServ->deviceId < 0)) {
        HDC_LOG_ERR("Input parameter deviceId is error. (deviceId=%d)\n", pServ->deviceId);
        return DRV_ERROR_INVALID_DEVICE;
    }

    (void)mmMutexLock(&pServ->mutex);
    if (pServ->magic != HDC_MAGIC_WORD) {
        (void)mmMutexUnLock(&pServ->mutex);
        HDC_LOG_ERR("Input parameter magic is error. (magic=%#x)\n", pServ->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    session_num = pServ->session_num;
    if (session_num > 0) {
        (void)mmMutexUnLock(&pServ->mutex);
        HDC_LOG_WARN("Destroy server not success, there have session alive. (session_num=%d)\n", session_num);
        return DRV_ERROR_SERVER_BUSY;
    }
    pServ->magic = 0;
    (void)mmMutexUnLock(&pServ->mutex);

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) {
        if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
            ret = hdc_ub_server_destroy(pServ, pServ->deviceId, pServ->serviceType);
        } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
            ret = hdc_pcie_server_destroy(pServ->bind_fd, pServ->deviceId, pServ->serviceType);
        } else {
            HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
            ret = DRV_ERROR_INVALID_HANDLE;
        }
        if (ret != 0) {
            HDC_LOG_WARN("Destroy pcie server not success. (dev_id=%d; errno=%d)\n", pServ->deviceId, ret);
        }

        if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
            hdc_link_event_pre_uninit(pServ->deviceId);
        }

        hdc_pcie_close_bind_fd(pServ->bind_fd);
        pServ->bind_fd = HDC_SESSION_FD_INVALID;
    } else {
        listenFd = pServ->listenFd;
        pServ->listenFd = -1;
        wmb();
        (void)shutdown(listenFd, SHUT_RDWR);
        (void)mm_close_socket(listenFd);
        while ((pServ->accept_wait == HDC_ACCEPT_WAITING) && (retry_time < HDC_SOCKET_MAX_RETRY_TIMES)) {
            usleep(HDC_SERVER_DESTORY_WAIT_TIME_US);
            retry_time++;
        }
        if (retry_time == HDC_SOCKET_MAX_RETRY_TIMES) {
            HDC_LOG_ERR("Accept exit timeout. listenFd = %d\n", listenFd);
        }
    }

    HDC_LOG_INFO("Destroy server success. (deviceId=%d; service_Type=%d)\n", pServ->deviceId, pServ->serviceType);
    (void)mmMutexDestroy(&pServ->mutex);
    free(pServ);
    pServ = NULL;

    return DRV_ERROR_NONE;
}

hdcError_t drv_hdc_get_server_dev_id(HDC_SERVER server, signed int *devid)
{
    struct hdc_server_head *pServ = NULL;

    if ((server == NULL) || (devid == NULL)) {
        HDC_LOG_ERR("Input parameter server or devid is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    pServ = (struct hdc_server_head *)server;
    if ((pServ->deviceId >= hdc_get_max_device_num()) || (pServ->deviceId < 0)) {
        HDC_LOG_ERR("Input parameter deviceId is error. (deviceId=%d)\n", pServ->deviceId);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (pServ->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Input parameter magic is error. (magic=%#x)\n", pServ->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    *devid = pServ->deviceId;

    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drvHdcSessionAccept
 * Function Description  : Open HDC Session for communication between Host and Device
 * Input Parameters      : HDC_SERVER server      HDC server to which the newly created session belongs
 * Output Parameters     : HDC_SESSION *session   Created session
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcSessionAccept(HDC_SERVER server, HDC_SESSION *session)
{
    struct hdc_server_head *pServ = NULL;
    struct hdc_server_session *p_serv_session = NULL;
    signed int ret = DRV_ERROR_NONE;

    if ((server == NULL) || (session == NULL)) {
        HDC_LOG_ERR("Input parameter server or session is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    pServ = (struct hdc_server_head *)server;
    if ((pServ->deviceId >= hdc_get_max_device_num()) || (pServ->deviceId < 0)) {
        HDC_LOG_ERR("Input parameter deviceId is error. (deviceId=%d)\n", pServ->deviceId);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (pServ->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Input parameter magic is error. (magic=%#x)\n", pServ->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) {
        ret = drv_hdc_pcie_session_accept((HDC_SERVER)pServ, (HDC_SESSION *)(&p_serv_session));
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    } else {
        ret = drv_hdc_socket_session_accept((PPC_SERVER)pServ, (PPC_SESSION *)(&p_serv_session));
        if (ret != DRV_ERROR_NONE) {
            if (ret == DRV_ERROR_SOCKET_CLOSE) {
                HDC_LOG_WARN("PPC server session accept exited. (errno=%d)\n", ret);
            } else {
                HDC_LOG_ERR("PPC server session accept failed. (errno=%d)\n", ret);
            }
            return DRV_ERROR_SOCKET_ACCEPT;
        }
    }

    p_serv_session->session.magic = HDC_MAGIC_WORD;
    p_serv_session->deviceId = (unsigned int)pServ->deviceId;
    p_serv_session->session.type = HDC_SESSION_SERVER;
    p_serv_session->server = pServ;

    (void)mmMutexLock(&pServ->mutex);
    pServ->session_num++;
    (void)mmMutexUnLock(&pServ->mutex);

    *session = (HDC_SESSION)p_serv_session;
    HDC_LOG_DEBUG("Add server session. (sockfd=%d)\n", p_serv_session->session.sockfd);

    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name       : drv_hdc_server_session_close
 * Description         : Close HDC Session
 * Input Parameters    : HDC_SESSION session    The session to be closed
 * Output Parameters   : None
 * Return Value        : DRV_ERROR_NONE
 *                     DRV_ERROR_INVALID_VALUE
 * Called Functions    :
 * Calling Functions   :
 *
 * Modification History:
 * 1.Date        : January 15, 2018
 *  Modification : Function newly created
 *
 *****************************************************************************/
hdcError_t drv_hdc_server_session_close(HDC_SESSION session, int close_state, int flag)
{
    struct hdc_server_head *pServ = NULL;
    struct hdc_server_session *p_serv_session = NULL;
    signed int ret;

    p_serv_session = (struct hdc_server_session *)session;
    if (p_serv_session->deviceId >= (unsigned int)hdc_get_max_device_num()) {
        HDC_LOG_ERR("Input parameter deviceId is error. (deviceId=%#x)\n", p_serv_session->deviceId);
        return DRV_ERROR_INVALID_VALUE;
    }

    HDC_LOG_INFO("Close server session. (sockfd=%d)\n", p_serv_session->session.sockfd);

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) {
#ifdef CFG_FEATURE_PRESET_SESSION
        if (!p_serv_session->alloc) {
            HDC_LOG_ERR("Parameter alloc is error. (alloc=%d)\n", (signed int)p_serv_session->alloc);
            return DRV_ERROR_INVALID_VALUE;
        }
#endif
        if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
            ret = hdc_ub_session_close(p_serv_session->deviceId, &p_serv_session->session, close_state, flag);
        } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
            ret = hdc_pcie_close(g_hdcConfig.pcie_handle, p_serv_session->deviceId, &p_serv_session->session);
        } else {
            HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
            ret = DRV_ERROR_INVALID_HANDLE;
        }
        if (ret != 0) {
            HDC_LOG_WARN("Close pcie session not success. (device=%d; sessionID=%d; errno=%d)\n",
                         p_serv_session->deviceId, p_serv_session->session.sockfd, ret);
        }

        if (g_hdcConfig.h2d_type != HDC_TRANS_USE_UB) {
            hdc_pcie_close_bind_fd(p_serv_session->session.bind_fd);
            p_serv_session->session.bind_fd = HDC_SESSION_FD_INVALID;
        }
    } else {
        (void)shutdown(p_serv_session->session.sockfd, SHUT_RDWR);
        (void)mm_close_socket(p_serv_session->session.sockfd);
    }

    pServ = p_serv_session->server;

    (void)mmMutexLock(&pServ->mutex);
    pServ->session_num--;
    (void)mmMutexUnLock(&pServ->mutex);

#ifdef CFG_FEATURE_PRESET_SESSION
    if (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) {
        drv_hdc_server_free_session(pServ, p_serv_session);
        return DRV_ERROR_NONE;
    }
#endif
    p_serv_session->session.magic = 0;
    free(p_serv_session);
    p_serv_session = NULL;
    return DRV_ERROR_NONE;
}
