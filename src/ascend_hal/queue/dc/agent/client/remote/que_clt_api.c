/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ascend_hal.h"

#include "queue.h"
#include "que_con_type.h"
#include "que_compiler.h"

#include "que_clt_pci.h"
#ifdef CFG_FEATURE_QUE_SUPPORT_UB
#include "que_clt_ub.h"
#endif
#include "queue_interface.h"
#include "queue_client_comm.h"
#include "que_clt_api.h"
#include "que_platform.h"
#include "drv_current.h"
static struct que_clt_api *g_que_clt_api;

static inline bool que_is_client_type(unsigned int qid)
{
    return (queue_get_deployment_type_by_qid(qid) == CLIENT_QUEUE);
}

drvError_t halQueuePeek(unsigned int devId, unsigned int qid, uint64_t *buf_len, int timeout)
{
    int ret = DRV_ERROR_QUEUE_INNER_ERROR;

    /* It is not recommended to adjust the verification order; prioritize verifying whether it is supported,
        and then verify the validity of the parameters. */
    if (que_unlikely(que_is_client_type(qid) == false)) {
        return DRV_ERROR_NOT_SUPPORT;
    }
    if (que_unlikely(buf_len == NULL)) {
        QUEUE_LOG_ERR("buf_len is null.\n");
        report_arg_null_pointer(__FUNCTION__, "buf_len");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (que_unlikely(devId >= MAX_DEVICE)) {
        QUEUE_LOG_ERR("invalid para.(devid=%u)\n", devId);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (que_unlikely(qid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("invalid para.(qid=%u)\n", qid);
        report_arg_out_of_range(__FUNCTION__, "queue ID", qid, 0, MAX_SURPORT_QUEUE_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (que_likely((g_que_clt_api != NULL) && (g_que_clt_api->api_peek != NULL))) {
        ret = g_que_clt_api->api_peek(devId, qid, buf_len, timeout);
    }

    return ret;
}

drvError_t halQueueEnQueueBuff(unsigned int devId, unsigned int qid, struct buff_iovec *vector, int timeout)
{
    DRV_CURRENT_API_SCOPE();
    int ret;

    if (que_unlikely(que_is_client_type(qid) == false)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = queue_param_check(devId, qid, vector, __FUNCTION__);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }

    if (que_likely((g_que_clt_api != NULL) && (g_que_clt_api->api_enque_buf != NULL))) {
        ret = g_que_clt_api->api_enque_buf(devId, qid, vector, timeout);
    } else {
        ret = DRV_ERROR_QUEUE_INNER_ERROR;
    }

    return ret;
}

drvError_t halQueueDeQueueBuff(unsigned int devId, unsigned int qid, struct buff_iovec *vector, int timeout)
{
    DRV_CURRENT_API_SCOPE();
    int ret;

    if (que_unlikely(que_is_client_type(qid) == false)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = queue_param_check(devId, qid, vector, __FUNCTION__);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }

    if (que_likely((g_que_clt_api != NULL) && (g_que_clt_api->api_deque_buf != NULL))) {
        ret = g_que_clt_api->api_deque_buf(devId, qid, vector, timeout);
    } else {
        ret = DRV_ERROR_QUEUE_INNER_ERROR;
    }

    return ret;
}

drvError_t queue_subscribe(unsigned int dev_id, unsigned int qid, unsigned int subevent_id, struct event_res *res)
{
    int ret = DRV_ERROR_QUEUE_INNER_ERROR;
#ifndef EMU_ST
    if (que_likely((g_que_clt_api != NULL) && (g_que_clt_api->api_subscribe != NULL))) {
        ret = g_que_clt_api->api_subscribe(dev_id, qid, subevent_id, res);
    }
#endif
    return ret;
}

drvError_t queue_unsubscribe(unsigned int dev_id, unsigned int qid, unsigned int subevent_id)
{
    int ret = DRV_ERROR_QUEUE_INNER_ERROR;
#ifndef EMU_ST
    if (que_likely((g_que_clt_api != NULL) && (g_que_clt_api->api_unsubscribe != NULL))) {
        ret = g_que_clt_api->api_unsubscribe(dev_id, qid, subevent_id);
    }
#endif
    return ret;
}

static int que_clt_get_con_type(void)
{
#ifdef CFG_FEATURE_QUE_SUPPORT_UB
    int con_type;
    int64_t val;
    int ret;

    ret = dms_get_connect_type(&val);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("dms_get_connect_type failed. (ret=%d)\n", ret);
        return QUE_CON_UNKNOWN;
    }
    switch (val) {
        case HOST_DEVICE_CONNECT_TYPE_PCIE:
            con_type = QUE_CON_PCIE;
            break;
        case HOST_DEVICE_CONNECT_TYPE_UB:
            con_type = QUE_CON_UB;
            break;
        default:
            con_type = QUE_CON_UNKNOWN;
            break;
    }
    return con_type;
#else  /* !CFG_FEATURE_QUE_SUPPORT_UB */
    return QUE_CON_PCIE;
#endif /* CFG_FEATURE_QUE_SUPPORT_UB */
}

static void que_clt_api_init(void)
{
    int con_type = que_clt_get_con_type();

    switch (con_type) {
        case QUE_CON_PCIE:
            g_que_clt_api = que_clt_pci_get_api();
            break;
#ifdef CFG_FEATURE_QUE_SUPPORT_UB
        case QUE_CON_UB:
            g_que_clt_api = que_clt_ub_get_api();
            break;
#endif
#ifndef EMU_ST
        default:
            g_que_clt_api = NULL;
            QUEUE_LOG_ERR("que api init failed. (con_type=%d)\n", con_type);
            break;
#endif
    }
}

static void que_clt_var_init(void)
{
    int con_type = que_clt_get_con_type();

    switch (con_type) {
        case QUE_CON_PCIE:
            break;
#ifdef CFG_FEATURE_QUE_SUPPORT_UB
        case QUE_CON_UB:
            que_clt_ub_var_init();
            break;
#endif
#ifndef EMU_ST
        default:
            QUEUE_LOG_ERR("que var init failed. (con_type=%d)\n", con_type);
            break;
#endif
    }
}

static int __attribute__((constructor)) que_clt_module_init(void)
{
    que_clt_api_init();
    que_clt_var_init();
    return 0;
}

static void __attribute__((destructor)) que_clt_module_uninit(void)
{
    int con_type = que_clt_get_con_type();

    switch (con_type) {
        case QUE_CON_PCIE:
            break;
#ifdef CFG_FEATURE_QUE_SUPPORT_UB
        case QUE_CON_UB:
            que_clt_ub_uninit();
            break;
#endif
#ifndef EMU_ST
        default:
            QUEUE_LOG_ERR("que clt uninit failed. (con_type=%d)\n", con_type);
            break;
#endif
    }

    return;
}
