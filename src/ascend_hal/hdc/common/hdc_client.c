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
#include "hdc_client.h"
#include "hdc_adapt.h"
#include "hdc_core.h"

STATIC hdcError_t drv_hdc_client_create_para_checker(const HDC_CLIENT *pClient, signed int maxSessionNum,
                                                     signed int serviceType, signed int flag)
{
    signed int max_session_num_by_type;

    if (pClient == NULL) {
        HDC_LOG_ERR("Input parameter pClient is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    max_session_num_by_type = drv_hdc_get_max_session_num_by_type(serviceType);
    if ((maxSessionNum <= 0) || (maxSessionNum > max_session_num_by_type)) {
        HDC_LOG_ERR("Input parameter maxSessionNum is error. (maxSessionNum=%d)\n", maxSessionNum);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((serviceType >= HDC_SERVICE_TYPE_MAX) || (serviceType < 0)) {
        HDC_LOG_ERR("Input parameter serviceType is error. (serviceType=%d)\n", serviceType);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((flag < 0) || (flag > HDC_SESSION_CONN_TIMEOUT)) {
        HDC_LOG_ERR("Input parameter flag is error. (flag=%d).\n", flag);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name        : drvHdcClientCreate
 * Function Description : Create an HDC client and initialize it based on the maximum number of sessions and
 *                        service type
 * Input Parameters     : int maxSessionNum  The maximum number of sessions currently required by Client
 *                        int serviceType  select service type
 *                        int flag         Reserved parameters, [bit0 - bit7] session connect timeout, other fixed to 0
 * Output Parameters    : HDC_CLIENT *client  Created HDC Client pointer
 * Return Value         : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function      :
 * Called Function      :
 *
 * Modification History :
 * 1.Date               : January 15, 2018
 *  Modification        : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcClientCreate(HDC_CLIENT *client, signed int maxSessionNum, signed int serviceType, signed int flag)
{
    struct hdc_client_session *pSession = NULL;
    struct hdc_client_head *pHead = NULL;
    size_t size;
    signed int cnt;

    /* sanity check */
    if (drv_hdc_client_create_para_checker(client, maxSessionNum, serviceType, flag) != DRV_ERROR_NONE) {
        return DRV_ERROR_INVALID_VALUE;
    }

    size = sizeof(struct hdc_client_session) * (size_t)maxSessionNum + sizeof(struct hdc_client_head);

    /* malloc memory for client */
    pHead = (struct hdc_client_head *)drv_hdc_zalloc(size);
    if (pHead == NULL) {
        HDC_LOG_ERR("Call malloc failed.\n");
        hdc_report_out_of_mem_err_msg(__func__, size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    /* initialization */
    pHead->serviceType = serviceType;
    pHead->magic = HDC_MAGIC_WORD;
    pHead->flag = (unsigned int)((flag == 0) ? HDC_SESSION_CONN_TIMEOUT : flag);
    pHead->maxSessionNum = (unsigned int)maxSessionNum;
    (void)mmMutexInit(&pHead->mutex);

    pSession = pHead->session;

    for (cnt = 0; cnt < maxSessionNum; cnt++) {
        pSession[cnt].session.magic = HDC_MAGIC_WORD;
        pSession[cnt].session.sockfd = -1;
        pSession[cnt].session.bind_fd = HDC_SESSION_FD_INVALID;
        pSession[cnt].alloc = false;
        pSession[cnt].node = -1;
        pSession[cnt].devid = -1;
    }

    *client = (HDC_CLIENT)pHead;

    HDC_LOG_DEBUG("Create client success.\n");

    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drvHdcClientDestroy
 * Function Description  : Release HDC Client
 * Input Parameters      : HDC_CLIENT client  HDC Client to be released
 * Output Parameters     : None
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 *                        DRV_ERROR_CLIENT_BUSY
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcClientDestroy(HDC_CLIENT client)
{
    struct hdc_client_session *pSession = NULL;
    struct hdc_client_head *pHead = NULL;
    bool alive = false;
    unsigned int maxSessionNum;
    unsigned int cnt;
    signed int ret;

    if (client == NULL) {
        HDC_LOG_ERR("Input parameter client is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    pHead = (struct hdc_client_head *)client;

    if (pHead->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Variable pHead_magic is invalid. (pHead_magic=%#x)\n", pHead->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((pHead->maxSessionNum <= 0) || (pHead->maxSessionNum > HDCDRV_SUPPORT_MAX_SESSION)) {
        HDC_LOG_ERR("Input parameter maxSessionNum is error. (maxSessionNum=%u)\n", pHead->maxSessionNum);
        return DRV_ERROR_INVALID_VALUE;
    }

    maxSessionNum = pHead->maxSessionNum;
    pSession = pHead->session;

    (void)mmMutexLock(&pHead->mutex);

    for (cnt = 0; cnt < maxSessionNum; cnt++) {
        if (pSession[cnt].session.sockfd != -1) {
            alive = true;
            HDC_LOG_WARN("Session still alive. (session=%u)\n", pSession[cnt].session.sockfd);
        }
    }
    if (alive) {
        (void)mmMutexUnLock(&pHead->mutex);
        HDC_LOG_WARN("Must close session before destroy client, serviceType %d.\n", pHead->serviceType);
        return DRV_ERROR_CLIENT_BUSY;
    }

    (void)mmMutexUnLock(&pHead->mutex);

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) {
        if ((ret = hdc_pcie_client_destroy(g_hdcConfig.pcie_handle, 0, pHead->serviceType)) != 0) {
            HDC_LOG_WARN("Destroy pcie client not success. (errno=%d)\n", ret);
        }
    }
    (void)mmMutexDestroy(&pHead->mutex);
    pHead->magic = 0;
    free(client);
    pHead = NULL;

    HDC_LOG_DEBUG("Destroy client.\n");
    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drv_hdc_client_alloc_session
 * Function Description  : Find an available Session
 * Input Parameters      : struct hdc_client_head
 * Output Parameters     : struct hdc_client_session **ppSession
 * Return Value          :
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 16, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
STATIC bool drv_hdc_client_alloc_session(struct hdc_client_head *pHead, struct hdc_client_session **ppSession)
{
    struct hdc_client_session *pSession = NULL;
    unsigned int maxSessionNum;
    unsigned int cnt;

    maxSessionNum = (unsigned int)drv_hdc_get_max_session_num_by_type(pHead->serviceType);
    if ((pHead->maxSessionNum <= 0) || (pHead->maxSessionNum > maxSessionNum)) {
        HDC_LOG_ERR("Input parameter maxSessionNum is error. (maxSessionNum=%u)\n", pHead->maxSessionNum);
        return false;
    }

    (void)mmMutexLock(&pHead->mutex);

    pSession = pHead->session;
    maxSessionNum = pHead->maxSessionNum;
    for (cnt = 0; cnt < maxSessionNum; cnt++) {
        if (!(pSession[cnt].alloc)) {
            pSession[cnt].alloc = true;
            pSession[cnt].session.sockfd = -1;
            pSession[cnt].session.bind_fd = HDC_SESSION_FD_INVALID;
#ifdef CFG_FEATURE_SUPPORT_UB
            pSession[cnt].session.close_eventfd = -1;
            pSession[cnt].session.epoll_head = NULL;
#endif
            break;
        }
    }
    if (cnt == maxSessionNum) {
        (void)mmMutexUnLock(&pHead->mutex);
        HDC_LOG_ERR("No free session.\n");
        return false;
    }

    *ppSession = &pSession[cnt];

    (void)mmMutexUnLock(&pHead->mutex);

    HDC_LOG_DEBUG("Client alloc session success.\n");
    return true;
}

/*****************************************************************************
 * Function Name         : drv_hdc_client_free_session
 * Function Description  : Release Client-side Session
 * Input Parameters      : struct hdc_client_session *pSession
 * Output Parameters     : None
 * Return Value          :
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 17, 2018
 *  Modification         : New generated function
 *****************************************************************************/
STATIC void drv_hdc_client_free_session(struct hdc_client_head *pHead, struct hdc_client_session *pSession)
{
    (void)mmMutexLock(&pHead->mutex);
    pSession->alloc = false;
    pSession->session.sockfd = -1;
    (void)mmMutexUnLock(&pHead->mutex);

    HDC_LOG_DEBUG("Client free session.\n");
    return;
}

STATIC hdcError_t drv_hdc_connect_para_check(signed int peer_node, signed int peer_devid,
                                             const struct hdc_client_head *pHead, const HDC_SESSION *pSession)
{
    if (peer_node != 0) {
        HDC_LOG_ERR("Input parameter peer_node is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (pHead == NULL) {
        HDC_LOG_ERR("Input parameter pHead is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (pSession == NULL) {
        HDC_LOG_ERR("Input parameter pSession is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if ((peer_devid >= hdc_get_max_device_num()) || (peer_devid < 0)) {
        HDC_LOG_ERR("Input parameter peer_devid is error. (dev_id=%d)\n", peer_devid);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (pHead->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Input parameter magic is error. (pHead_magic=%#x)\n", pHead->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

/*lint -e413*/
hdcError_t halHdcSessionConnectEx(int peer_node, int peer_devid, int peer_pid, HDC_CLIENT client, HDC_SESSION *pSession)
{
    struct hdc_client_head *pHead = (struct hdc_client_head *)client;
    struct hdc_client_session *p_client_session_ppc = NULL;
    struct hdc_client_session *p_client_session = NULL;
    bool result = false;
    hdcError_t err_code;
    signed int ret;

    err_code = drv_hdc_connect_para_check(peer_node, peer_devid, pHead, pSession);
    if (err_code != DRV_ERROR_NONE) {
        return err_code;
    }

    result = drv_hdc_client_alloc_session(pHead, &p_client_session);
    if ((!result) || (p_client_session == NULL)) {
        HDC_LOG_ERR("No available session.\n");
        return DRV_ERROR_OVER_LIMIT;
    }

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) {
        if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
            ret = hdc_ub_connect(peer_devid, pHead, peer_pid, &p_client_session->session);
        } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
            ret = hdc_pcie_connect(g_hdcConfig.pcie_handle, peer_devid, pHead->serviceType, pHead->flag, peer_pid,
                                   &p_client_session->session);
        } else {
            HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
            ret = DRV_ERROR_INVALID_HANDLE;
        }
        if (ret != 0) {
            drv_hdc_client_free_session(pHead, p_client_session);
            if (ret == -HDCDRV_PEER_REBOOT) {
                return DRV_ERROR_DEV_PROCESS_HANG;
            }
            if (ret == (-HDCDRV_REMOTE_SERVICE_NO_LISTENING)) {
                return DRV_ERROR_REMOTE_NOT_LISTEN;
            }
            if (ret == (-HDCDRV_NO_SESSION)) {
                return DRV_ERROR_REMOTE_NO_SESSION;
            }
            if (ret == (-HDCDRV_DEVICE_NOT_READY)) {
                return DRV_ERROR_DEVICE_NOT_READY;
            }
            if (ret == (-HDCDRV_SESSION_CHAN_INVALID)) {
                return DRV_ERROR_SOCKET_CONNECT;
            }
            if (ret == (-HDCDRV_CONNECT_TIMEOUT)) {
                return DRV_ERROR_WAIT_TIMEOUT;
            }
            if (ret == (-HDCDRV_SESSION_HAS_CLOSED)) {
                return DRV_ERROR_SOCKET_CLOSE;
            }
            if (ret == (-HDCDRV_SEND_CTRL_MSG_FAIL)) {
                return DRV_ERROR_NO_DEVICE;
            }
            return (hdcError_t)ret;
        }
    } else {
        err_code = drv_hdc_socket_session_connect(peer_devid, pHead->serviceType,
                                                  (HDC_SESSION *)(&p_client_session_ppc));
        if (err_code != DRV_ERROR_NONE) {
            drv_hdc_client_free_session(pHead, p_client_session);
            HDC_LOG_WARN("PPC client session connect not success. (err_code=%d)\n", (signed int)err_code);
            return DRV_ERROR_SOCKET_CONNECT;
        }

        /* copy session magic/sockfd to session of alloc, and free ppc. */
        p_client_session->session.magic = p_client_session_ppc->session.magic;
        p_client_session->session.sockfd = p_client_session_ppc->session.sockfd;
        free(p_client_session_ppc);
        p_client_session_ppc = NULL;
    }

    p_client_session->session.type = HDC_SESSION_CLINET;
    p_client_session->node = peer_node;
    p_client_session->devid = peer_devid;
    p_client_session->client = pHead;
    *pSession = (HDC_SESSION)p_client_session;
    return DRV_ERROR_NONE;
}
/*lint +e413*/
/*****************************************************************************
 * Function Name         : drvHdcSessionConnect
 * Function Description  : Create HDC Session for Host and Device communication
 * Input Parameters      : signed int peer_node       The node number of the node where the Device is located.
 *                         Currently only 1 node is supported. Remote nodes are not supported. You need
 *                         to pass a fixed value of 0
 *                         signed int peer_devid      Device's uniform ID in the host (number in each node)
 *                         HDC_CLIENT client          HDC Client handle corresponding to the newly created Session
 * Output Parameters     : HDC_SESSION *session       Created session
 * Return Value          : DRV_ERROR_NONE
 *                         DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcSessionConnect(int peer_node, int peer_devid, HDC_CLIENT client, HDC_SESSION *session)
{
    return halHdcSessionConnectEx(peer_node, peer_devid, HDCDRV_INVALID_PEER_PID, client, session);
}
/*****************************************************************************
 * Function Name         : drv_hdc_client_session_close
 * Function Description  : Release HDC Session
 * Input Parameters      : HDC_SESSION session    The session to be released
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
hdcError_t drv_hdc_client_session_close(HDC_SESSION session, int close_state, int flag)
{
    struct hdc_client_session *p_client_session = NULL;
    struct hdc_client_head *pHead = NULL;
    signed int ret;

    p_client_session = (struct hdc_client_session *)session;

    if (!(p_client_session->alloc)) {
        HDC_LOG_ERR("Parameter alloc is error. (alloc=%d)\n", (signed int)p_client_session->alloc);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (p_client_session->client == NULL) {
        HDC_LOG_ERR("Parameter client is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    HDC_LOG_INFO("Destroy client session. (sock=%d)\n", p_client_session->session.sockfd);

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) {
        if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
            ret = hdc_ub_session_close((unsigned int)p_client_session->devid, &p_client_session->session, close_state,
                                       flag);
        } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
            ret = hdc_pcie_close(g_hdcConfig.pcie_handle, (unsigned int)p_client_session->devid,
                                 &p_client_session->session);
        } else {
            HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
            ret = DRV_ERROR_INVALID_HANDLE;
        }
        if (ret != 0) {
            HDC_LOG_WARN("Close device not success. (device=%d; errno=%d)\n", (signed int)(p_client_session->devid),
                         ret);
        }

        if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
            hdc_link_event_pre_uninit(p_client_session->devid);
        }

        hdc_pcie_close_bind_fd(p_client_session->session.bind_fd);
        p_client_session->session.bind_fd = HDC_SESSION_FD_INVALID;
    } else {
        (void)shutdown(p_client_session->session.sockfd, SHUT_RDWR);
        (void)mm_close_socket(p_client_session->session.sockfd);
    }

    pHead = p_client_session->client;
    drv_hdc_client_free_session(pHead, p_client_session);

    return DRV_ERROR_NONE;
}
