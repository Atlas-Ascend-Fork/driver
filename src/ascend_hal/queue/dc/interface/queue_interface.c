/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdio.h>
#include <string.h>
#include "ascend_hal.h"
#include "dms_user_interface.h"
#include "securec.h"

#include "pbl_uda_user.h"
#include "esched_user_interface.h"

#include "queue_user_manage.h"
#include "queue.h"
#include "que_compiler.h"
#include "queue_user_interface.h"
#include "queue_interface.h"
#include "que_platform.h"
#include "queue_feature.h"

static struct queue_comm_interface_list *queue_comm_interface[DEPLOYMENT_MAX_TYPE] = {NULL};

static bool g_queue_init_call_flag[MAX_DEVICE] = {false};

unsigned int que_get_unified_devid(unsigned int dev_id)
{
    return que_get_unified_devid_platform(dev_id);
}

void queue_set_comm_interface(QUEUE_DEPLOYMENT_TYPE type, struct queue_comm_interface_list *interface)
{
    queue_comm_interface[type] = interface;
}

unsigned int queue_get_virtual_qid(unsigned int actual_qid, QUEUE_DEPLOYMENT_TYPE type)
{
    if (g_default_client_queue && g_enable_local_queue && ((type == LOCAL_QUEUE) || (type == INTER_DEV_QUEUE))) {
        return (actual_qid + CLIENT_QID_OFFSET);
    }

    return actual_qid;
}

unsigned int queue_get_actual_qid(unsigned int virtual_qid)
{
    if (g_default_client_queue && g_enable_local_queue) {
        return (virtual_qid < CLIENT_QID_OFFSET) ? virtual_qid : virtual_qid - CLIENT_QID_OFFSET;
    }

    return virtual_qid;
}

/*
 * The deployment of host default client queue is as follows:
 * enable_local_queue  virtual qid range             DeploymentType
 *       true          0~CLIENT_QID_OFFSET - 1       CLIENT_QUEUE
 *       true          CLIENT_QID_OFFSET~            LOCAL_QUEUE
 *       false         0~CLIENT_QID_OFFSET - 1       CLIENT_QUEUE
 * In other cases, deploy local queues.
 */
QUEUE_DEPLOYMENT_TYPE queue_get_deployment_type_by_qid(unsigned int qid)
{
    if (g_default_client_queue) {
        if (g_enable_local_queue) {
            return qid < CLIENT_QID_OFFSET ? CLIENT_QUEUE : LOCAL_QUEUE;
        }
        return CLIENT_QUEUE;
    }
    return LOCAL_QUEUE;
}

static QUEUE_DEPLOYMENT_TYPE queue_get_deployment_type(void)
{
    QUEUE_DEPLOYMENT_TYPE type = LOCAL_QUEUE;

    if (g_default_client_queue) {
        if (g_enable_local_queue) {
            type = LOCAL_QUEUE;
        } else {
            type = CLIENT_QUEUE;
        }
    }

    return type;
}

#ifdef CFG_FEATURE_QUE_SUPPORT_UB
int que_get_inter_dev_status(unsigned int dev_id, unsigned int qid, int *inter_dev_state, char *share_que_name)
{
    struct queue_manages *que_manage = NULL;

    if ((dev_id >= MAX_DEVICE) || (qid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("para is error. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!queue_get(qid)) {
        return DRV_ERROR_NOT_EXIST;
    }

    que_manage = queue_get_local_mng(qid);
    if (que_manage == NULL) {
        queue_put(qid);
        QUEUE_LOG_ERR("Queue local manage is null. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if (que_manage->valid != QUEUE_CREATED) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue(%u) is not created.\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    *inter_dev_state = que_manage->inter_dev_state;
    if (share_que_name != NULL) {
        (void)memcpy_s(share_que_name, SHARE_QUEUE_NAME_LEN, que_manage->share_queue_name, SHARE_QUEUE_NAME_LEN);
    }

    queue_put(qid);
    return DRV_ERROR_NONE;
}

static int que_get_inter_dev_deploy_type(unsigned int dev_id, unsigned int qid, QUEUE_OP_SUPPORT_TYPE op_type,
                                         QUEUE_DEPLOYMENT_TYPE *type)
{
    int ret;
    int inter_dev_state = QUEUE_STATE_DISABLED;

    ret = que_get_inter_dev_status(dev_id, qid, &inter_dev_state, NULL);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (inter_dev_state == QUEUE_STATE_DISABLED) {
        return DRV_ERROR_NONE;
    }

    if ((op_type == BOTH_SUPPORT) &&
        ((inter_dev_state == QUEUE_STATE_EXPORTED) || (inter_dev_state == QUEUE_STATE_IMPORTED))) {
        *type = INTER_DEV_QUEUE;
        return DRV_ERROR_NONE;
    }

    if (((op_type == EXPORT_SUPPORT) && (inter_dev_state == QUEUE_STATE_EXPORTED)) ||
        ((op_type == IMPORT_SUPPORT) && (inter_dev_state == QUEUE_STATE_IMPORTED))) {
        *type = INTER_DEV_QUEUE;
        return DRV_ERROR_NONE;
    }
    *type = INVALID_INTER_DEV_QUEUE;
    QUEUE_LOG_INFO("not support curr que state. (op_type=%u; state=%d)\n", (unsigned int)op_type, inter_dev_state);

    return DRV_ERROR_NONE;
}
#endif
QUEUE_DEPLOYMENT_TYPE que_get_deploy_type_for_inter_dev(unsigned int dev_id, unsigned int qid,
                                                        QUEUE_OP_SUPPORT_TYPE op_type)
{
    (void)dev_id;
    (void)op_type;
    QUEUE_DEPLOYMENT_TYPE type = LOCAL_QUEUE;
    if (g_default_client_queue) {
        if (g_enable_local_queue) {
            type = (qid < CLIENT_QID_OFFSET) ? CLIENT_QUEUE : LOCAL_QUEUE;
        } else {
            type = CLIENT_QUEUE;
        }
    }

#ifdef CFG_FEATURE_QUE_SUPPORT_UB
    if (type == LOCAL_QUEUE) {
        unsigned int actualqid = queue_get_actual_qid(qid);
        (void)que_get_inter_dev_deploy_type(dev_id, actualqid, op_type, &type);
    }
#endif
    return type;
}

unsigned int halGetHostDevid(void)
{
#ifdef CFG_FEATURE_QUE_SUPPORT_UB
    unsigned int host_id;
    drvError_t ret = halGetHostID(&host_id);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("drv update host_id failed. (ret=%d)\n", ret);
        return 0;
    }
    return host_id;
#else
    return 0;
#endif
}

static inline void que_get_init_deploy_type(unsigned int dev_id, QUEUE_DEPLOYMENT_TYPE *type)
{
#ifdef CFG_FEATURE_QUE_SUPPORT_UB
    que_check_convert_to_inter_dev_queue_platform(dev_id, (int *)type);
#endif
    (void)dev_id;
    (void)type;
}

drvError_t halQueueInit(unsigned int devId)
{
    QUEUE_DEPLOYMENT_TYPE type = LOCAL_QUEUE;
    drvError_t ret = DRV_ERROR_NOT_SUPPORT;

    if (que_unlikely(devId >= MAX_DEVICE)) {
        QUEUE_LOG_ERR("para is error. (devid=%u; max_devid=%u)\n", devId, (unsigned int)MAX_DEVICE);
        return DRV_ERROR_INVALID_DEVICE;
    }

    /*
     * if enable local_queue, client queue will be initialized in halQueueCreate or halQueueAttach.
     * because client queue may fail to initialize when the device side process is not pulled up.
     */
    if (g_default_client_queue) {
        if (g_enable_local_queue) {
            g_queue_init_call_flag[devId] = true;
            return DRV_ERROR_NONE;
        }
        type = CLIENT_QUEUE;
    }

    que_get_init_deploy_type(devId, &type);

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_dc_init != NULL) {
        ret = queue_comm_interface[type]->queue_dc_init(devId);
    }

    return ret;
}

void queue_uninit(unsigned int dev_id, unsigned int scene)
{
    if (que_likely(dev_id < MAX_DEVICE)) {
        QUEUE_DEPLOYMENT_TYPE type = g_default_client_queue ? CLIENT_QUEUE : LOCAL_QUEUE;
        if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_uninit != NULL) {
            queue_comm_interface[type]->queue_uninit(dev_id, scene);
        }
    }
}

static drvError_t queue_real_init_iner(unsigned int dev_id, QUEUE_DEPLOYMENT_TYPE type)
{
    QUEUE_DEPLOYMENT_TYPE _type = type;
    if (g_default_client_queue) {
        if (g_enable_local_queue && (dev_id < MAX_DEVICE) && (g_queue_init_call_flag[dev_id] == true)) {
            que_get_init_deploy_type(dev_id, &_type);
            if (queue_comm_interface[_type] == NULL) {
                QUEUE_LOG_INFO("queue comm interface not set. (deploy_type=%u)\n", _type);
                return DRV_ERROR_NOT_SUPPORT;
            }
            drvError_t ret = queue_comm_interface[_type]->queue_dc_init(dev_id);
            if (ret != DRV_ERROR_NONE && ret != DRV_ERROR_REPEATED_INIT) {
                QUEUE_LOG_ERR("queue init failed. (devid=%u; deploy=%u; ret=%d)\n", dev_id, _type, ret);
                return ret;
            }
        }
    }
    return DRV_ERROR_NONE;
}

drvError_t halQueueCreate(unsigned int devId, const QueueAttr *queAttr, unsigned int *qid)
{
    QUEUE_DEPLOYMENT_TYPE type = LOCAL_QUEUE;
    drvError_t ret;

    if (que_unlikely(devId >= MAX_DEVICE)) {
        QUEUE_LOG_ERR("invalid para.(devid=%u;max_devid=%u;)\n", devId, (unsigned int)MAX_DEVICE);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (que_unlikely(queAttr == NULL)) {
        QUEUE_LOG_ERR("queAttr is null.\n");
        report_arg_null_pointer(__FUNCTION__, "queAttr");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (que_unlikely(qid == NULL)) {
        QUEUE_LOG_ERR("qid is null.\n");
        report_arg_null_pointer(__FUNCTION__, "qid");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_default_client_queue) {
        type = CLIENT_QUEUE;
        if (g_enable_local_queue) {
            type = (queAttr->deploy_type == CLIENT_QUEUE_DEPLOY) ? CLIENT_QUEUE : LOCAL_QUEUE;
        }
        /*
         * For the host supports both local queue and client queue,
         * the local queue or client queue may not be initialized and needs to be initialized.
         */
        ret = queue_real_init_iner(devId, type);
        if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
            return ret;
#endif
        }
    }

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_create != NULL) {
        ret = queue_comm_interface[type]->queue_create(devId, queAttr, qid);
        if (ret == DRV_ERROR_NONE) {
            *qid = queue_get_virtual_qid(*qid, type);
            QUEUE_RUN_LOG_INFO("create queue successful. (virtual_qid=%u, actual_qid=%u, deploy_type=%u)\n", *qid,
                               queue_get_actual_qid(*qid), type);
        }
        return ret;
    }

    QUEUE_LOG_INFO("not set queue_create_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueGrant(unsigned int devId, int qid, int pid, QueueShareAttr attr)
{
    QUEUE_DEPLOYMENT_TYPE type = queue_get_deployment_type_by_qid((unsigned int)qid);
    unsigned int actualqid = queue_get_actual_qid((unsigned int)qid);
    if (que_unlikely(devId >= MAX_DEVICE)) {
        QUEUE_LOG_ERR("invalid para.(devid=%u;max_devid=%u;)\n", devId, (unsigned int)MAX_DEVICE);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (que_unlikely(actualqid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("invalid para.(qid=%u;actualqid=%u;max_qid=%u)\n", qid, actualqid,
                      (unsigned int)MAX_SURPORT_QUEUE_NUM);
        report_arg_out_of_range(__FUNCTION__, "queue ID", (unsigned int)qid, 0, MAX_SURPORT_QUEUE_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_grant != NULL) {
        return queue_comm_interface[type]->queue_grant(devId, actualqid, pid, attr);
    }

    QUEUE_LOG_INFO("not set queue_grant_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueAttach(unsigned int devId, unsigned int qid, int timeOut)
{
    QUEUE_DEPLOYMENT_TYPE type = queue_get_deployment_type_by_qid(qid);
    unsigned int actualqid = queue_get_actual_qid(qid);
    drvError_t ret;

    if (que_unlikely((devId >= MAX_DEVICE) || (actualqid >= MAX_SURPORT_QUEUE_NUM) || (timeOut < -1))) {
        QUEUE_LOG_ERR("invalid para. (devid=%u; max_devid=%u; qid=%u; actualqid=%u; max_qid=%u; timeout=%dms)\n", devId,
                      (unsigned int)MAX_DEVICE, qid, actualqid, (unsigned int)MAX_SURPORT_QUEUE_NUM, timeOut);
        return DRV_ERROR_INVALID_VALUE;
    }

    /*
     * For the host supports both local queue and client queue,
     * the local queue or client queue may not be initialized and needs to be initialized.
     */
    ret = queue_real_init_iner(devId, type);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        return ret;
#endif
    }

    que_get_init_deploy_type(devId, &type);

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_attach != NULL) {
        return queue_comm_interface[type]->queue_attach(devId, actualqid, timeOut);
    }

    QUEUE_LOG_INFO("not set queue_attach_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueDestroy(unsigned int devId, unsigned int qid)
{
    QUEUE_DEPLOYMENT_TYPE type = queue_get_deployment_type_by_qid(qid);
    unsigned int actualqid = queue_get_actual_qid(qid);
    drvError_t ret;

    if (que_unlikely(devId >= MAX_DEVICE)) {
        QUEUE_LOG_ERR("invalid para.(devid=%u;max_devid=%u;)\n", devId, (unsigned int)MAX_DEVICE);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (que_unlikely(actualqid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("invalid para.(qid=%u;actualqid=%u;max_qid=%u)\n", qid, actualqid,
                      (unsigned int)MAX_SURPORT_QUEUE_NUM);
        report_arg_out_of_range(__FUNCTION__, "queue ID", qid, 0, MAX_SURPORT_QUEUE_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    /*
     * For the host supports both local queue and client queue,
     * the local queue or client queue may not be initialized and needs to be initialized.
     */
    ret = queue_real_init_iner(devId, type);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        return ret;
#endif
    }

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_destroy != NULL) {
        return queue_comm_interface[type]->queue_destroy(devId, actualqid);
    }

    QUEUE_LOG_INFO("not set queue_destroy_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueReset(unsigned int devId, unsigned int qid)
{
    QUEUE_DEPLOYMENT_TYPE type = que_get_deploy_type_for_inter_dev(devId, qid, BOTH_NOT_SUPPORT);
    unsigned int actualqid = queue_get_actual_qid(qid);
    if (que_unlikely(devId >= MAX_DEVICE)) {
        QUEUE_LOG_ERR("invalid para.(devid=%u;max_devid=%u;)\n", devId, (unsigned int)MAX_DEVICE);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (que_unlikely(actualqid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("invalid para.(qid=%u;actualqid=%u;max_qid=%u)\n", qid, actualqid,
                      (unsigned int)MAX_SURPORT_QUEUE_NUM);
        report_arg_out_of_range(__FUNCTION__, "queue ID", qid, 0, MAX_SURPORT_QUEUE_NUM);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_reset != NULL) {
        return queue_comm_interface[type]->queue_reset(devId, actualqid);
    }

    QUEUE_LOG_INFO("not set queue_reset_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueEnQueue(unsigned int devId, unsigned int qid, void *mbuf)
{
    QUEUE_DEPLOYMENT_TYPE type = que_get_deploy_type_for_inter_dev(devId, qid, IMPORT_SUPPORT);
    unsigned int actual_qid = queue_get_actual_qid(qid);

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_en_queue != NULL) {
        return queue_comm_interface[type]->queue_en_queue(devId, actual_qid, mbuf);
    }

    QUEUE_LOG_INFO("not set queue_en_queue_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueDeQueue(unsigned int devId, unsigned int qid, void **mbuf)
{
    QUEUE_DEPLOYMENT_TYPE type = que_get_deploy_type_for_inter_dev(devId, qid, EXPORT_SUPPORT);
    unsigned int actual_qid = queue_get_actual_qid(qid);

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_de_queue != NULL) {
        return queue_comm_interface[type]->queue_de_queue(devId, actual_qid, mbuf);
    }

    QUEUE_LOG_INFO("not set queue_de_queue_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueSubscribe(unsigned int devId, unsigned int qid, unsigned int groupId, int type)
{
    QUEUE_DEPLOYMENT_TYPE deploy_type = que_get_deploy_type_for_inter_dev(devId, qid, BOTH_NOT_SUPPORT);
    unsigned int actual_qid = queue_get_actual_qid(qid);

    if (queue_comm_interface[deploy_type] != NULL && queue_comm_interface[deploy_type]->queue_subscribe != NULL) {
        return queue_comm_interface[deploy_type]->queue_subscribe(devId, actual_qid, groupId, type);
    }

    QUEUE_LOG_INFO("not set queue_de_queue_iner. (deploy_type=%u)\n", deploy_type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueUnsubscribe(unsigned int devId, unsigned int qid)
{
    QUEUE_DEPLOYMENT_TYPE type = que_get_deploy_type_for_inter_dev(devId, qid, BOTH_NOT_SUPPORT);
    unsigned int actual_qid = queue_get_actual_qid(qid);

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_unsubscribe != NULL) {
        return queue_comm_interface[type]->queue_unsubscribe(devId, actual_qid);
    }

    QUEUE_LOG_INFO("not set queue_unsubscribe_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueSubF2NFEvent(unsigned int devId, unsigned int qid, unsigned int groupId)
{
    QUEUE_DEPLOYMENT_TYPE type = que_get_deploy_type_for_inter_dev(devId, qid, BOTH_NOT_SUPPORT);
    unsigned int actual_qid = queue_get_actual_qid(qid);

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_sub_f_to_nf_event != NULL) {
        return queue_comm_interface[type]->queue_sub_f_to_nf_event(devId, actual_qid, groupId);
    }

    QUEUE_LOG_INFO("not set queue_sub_f_to_nf_event_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueUnsubF2NFEvent(unsigned int devId, unsigned int qid)
{
    QUEUE_DEPLOYMENT_TYPE type = que_get_deploy_type_for_inter_dev(devId, qid, BOTH_NOT_SUPPORT);
    unsigned int actual_qid = queue_get_actual_qid(qid);

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_unsub_f_to_nf_event != NULL) {
        return queue_comm_interface[type]->queue_unsub_f_to_nf_event(devId, actual_qid);
    }

    QUEUE_LOG_INFO("not set queue_unsub_f_to_nf_event_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueSubEvent(struct QueueSubPara *subPara)
{
    QUEUE_DEPLOYMENT_TYPE type;
    QUEUE_OP_SUPPORT_TYPE op_type;

    if (que_unlikely(subPara == NULL)) {
        QUEUE_LOG_ERR("subPara is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    op_type = (subPara->eventType == QUEUE_F2NF_EVENT) ? IMPORT_SUPPORT : EXPORT_SUPPORT;
    type = que_get_deploy_type_for_inter_dev(subPara->devId, subPara->qid, op_type);
    subPara->qid = queue_get_actual_qid(subPara->qid);

    if (que_unlikely((subPara->devId >= MAX_DEVICE) || (subPara->qid >= MAX_SURPORT_QUEUE_NUM) ||
                     (subPara->eventType >= QUEUE_EVENT_TYPE_MAX))) {
        QUEUE_LOG_ERR("invalid para. (devid=%u; max_devid=%u; qid=%u; max_qid=%u; event_type=%d; max_type=%d)\n",
                      subPara->devId, (unsigned int)MAX_DEVICE, subPara->qid, (unsigned int)MAX_SURPORT_QUEUE_NUM,
                      subPara->eventType, QUEUE_EVENT_TYPE_MAX);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_sub_event != NULL) {
        return queue_comm_interface[type]->queue_sub_event(subPara);
    }

    QUEUE_LOG_INFO("not set queue_sub_event_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueUnsubEvent(struct QueueUnsubPara *unsubPara)
{
    QUEUE_DEPLOYMENT_TYPE type;
    QUEUE_OP_SUPPORT_TYPE op_type;

    if (que_unlikely(unsubPara == NULL)) {
        QUEUE_LOG_ERR("sub_para is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    op_type = (unsubPara->eventType == QUEUE_F2NF_EVENT) ? IMPORT_SUPPORT : EXPORT_SUPPORT;
    type = que_get_deploy_type_for_inter_dev(unsubPara->devId, unsubPara->qid, op_type);
    unsubPara->qid = queue_get_actual_qid(unsubPara->qid);

    if (que_unlikely((unsubPara->devId >= MAX_DEVICE) || (unsubPara->qid >= MAX_SURPORT_QUEUE_NUM) ||
                     (unsubPara->eventType >= QUEUE_EVENT_TYPE_MAX))) {
        QUEUE_LOG_ERR("invalid para. (devid=%u; max_devid=%u; qid=%u; max_qid=%u; event_type=%d; max_type=%d)\n",
                      unsubPara->devId, (unsigned int)MAX_DEVICE, unsubPara->qid, (unsigned int)MAX_SURPORT_QUEUE_NUM,
                      unsubPara->eventType, QUEUE_EVENT_TYPE_MAX);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_unsub_event != NULL) {
        return queue_comm_interface[type]->queue_unsub_event(unsubPara);
    }

    QUEUE_LOG_INFO("not set queue_unsubscribe_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueQueryInfo(unsigned int devId, unsigned int qid, QueueInfo *queInfo)
{
    QUEUE_DEPLOYMENT_TYPE type = que_get_deploy_type_for_inter_dev(devId, qid, BOTH_SUPPORT);
    unsigned int actualqid = queue_get_actual_qid(qid);
    drvError_t ret;

    if (que_unlikely(devId >= MAX_DEVICE)) {
        QUEUE_LOG_ERR("invalid para.(devid=%u;max_devid=%u;)\n", devId, (unsigned int)MAX_DEVICE);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (que_unlikely(actualqid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("invalid para.(qid=%u; max_qid=%u;)\n", qid, (unsigned int)MAX_SURPORT_QUEUE_NUM);
        report_arg_out_of_range(__FUNCTION__, "queue ID", qid, 0, MAX_SURPORT_QUEUE_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (que_unlikely(queInfo == NULL)) {
        QUEUE_LOG_ERR("queInfo is null.\n");
        report_arg_null_pointer(__FUNCTION__, "queInfo");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_query_info != NULL) {
        ret = queue_comm_interface[type]->queue_query_info(devId, actualqid, queInfo);
        if (queInfo != NULL) {
            queInfo->id = (int)queue_get_virtual_qid((unsigned int)queInfo->id, type);
        }
        return ret;
    }

    QUEUE_LOG_INFO("not set halQueueQueryInfo. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueGetStatus(unsigned int devId, unsigned int qid, QUEUE_QUERY_ITEM queryItem, unsigned int len,
                             void *data)
{
    QUEUE_DEPLOYMENT_TYPE type;
    unsigned int actualqid = queue_get_actual_qid(qid);

    if ((queryItem == QUERY_INTER_QUEUE_ALL_IMPORT_STATUS) || (queryItem == QUERY_INTER_QUEUE_IMPORT_STATUS)) {
        type = queue_get_deployment_type_by_qid(qid);
        if (type != LOCAL_QUEUE) {
            QUEUE_LOG_ERR("invalid para. (devid=%u; qid=%u; actualqid=%u; type=%u)\n", devId, qid, actualqid,
                          (unsigned int)type);
            return DRV_ERROR_INVALID_VALUE;
        }
        type = INTER_DEV_QUEUE;
    } else {
        type = que_get_deploy_type_for_inter_dev(devId, qid, BOTH_SUPPORT);
    }

    if (que_unlikely((devId >= MAX_DEVICE) || (len > EVENT_PROC_RSP_LEN) || (data == NULL) ||
                     ((queryItem != QUERY_INTER_QUEUE_ALL_IMPORT_STATUS) && (actualqid >= MAX_SURPORT_QUEUE_NUM)))) {
        QUEUE_LOG_ERR("invalid para. (devid=%u; qid=%u; actualqid=%u; len=%u)\n", devId, qid, actualqid, len);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_get_status != NULL) {
        return queue_comm_interface[type]->queue_get_status(devId, actualqid, queryItem, len, data);
    }

    QUEUE_LOG_INFO("not set queue_get_status_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueCtrlEvent(struct QueueSubscriber *subscriber, QUE_EVENT_CMD cmdType)
{
    QUEUE_DEPLOYMENT_TYPE type = queue_get_deployment_type();
    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_ctrl_event != NULL) {
        return queue_comm_interface[type]->queue_ctrl_event(subscriber, cmdType);
    }

    QUEUE_LOG_INFO("not set queue_ctrl_event_iner. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueGetQidbyName(unsigned int devId, const char *name, unsigned int *qid)
{
    QUEUE_DEPLOYMENT_TYPE type = queue_get_deployment_type();
    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_get_qid_by_name != NULL) {
        return queue_comm_interface[type]->queue_get_qid_by_name(devId, name, qid);
    }

    QUEUE_LOG_INFO("not set halQueueGetQidbyName. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueGetQidsbyPid(unsigned int devId, unsigned int pid, unsigned int maxQueSize, QidsOfPid *info)
{
    QUEUE_DEPLOYMENT_TYPE type = queue_get_deployment_type();
    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_get_qids_by_pid != NULL) {
        return queue_comm_interface[type]->queue_get_qids_by_pid(devId, pid, maxQueSize, info);
    }

    QUEUE_LOG_INFO("not set queue_get_qids_by_pid. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

static drvError_t queue_query_para_check(unsigned int dev_id, QueueQueryCmdType cmd, QueueQueryInputPara *in_put,
                                         QueueQueryOutputPara *out_put, const char *func_name)
{
    if (cmd >= QUEUE_QUERY_CMD_MAX) {
        QUEUE_LOG_ERR("Input para error.(cmd=%d)\n", cmd);
        report_enum_arg_out_of_range(func_name, "cmd", cmd, QUEUE_QUERY_CMD_TYPE_NAME);
        return DRV_ERROR_NOT_SUPPORT;
    }
    if (dev_id >= MAX_DEVICE) {
        QUEUE_LOG_ERR("Input para error.(devId=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    if ((in_put == NULL)) {
        QUEUE_LOG_ERR("in_put is null.\n");
        report_arg_null_pointer(func_name, "inPut");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (out_put == NULL) {
        QUEUE_LOG_ERR("out_put is null.\n");
        report_arg_null_pointer(func_name, "outPut");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (out_put->outBuff == NULL) {
        QUEUE_LOG_ERR("out_buff is null.\n");
        report_arg_null_pointer(func_name, "outPut->outBuff");
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static drvError_t queue_query_deploy_type(QueueQueryInputPara *in_put, QueueQueryOutputPara *out_put)
{
    QueQueryDeployInfo *out_buff = NULL;
    QueQueryDeploy *in_buff = NULL;

    if ((in_put->inBuff == NULL) || (in_put->inLen < sizeof(QueQueryDeploy)) ||
        (out_put->outLen < sizeof(QueQueryDeployInfo))) {
        QUEUE_LOG_ERR("para is invalid. (in_buff_is_null=%d; in_len=%u; out_len=%u)\n", in_put->inBuff == NULL,
                      in_put->inLen, out_put->outLen);
        return DRV_ERROR_PARA_ERROR;
    }

    in_buff = (QueQueryDeploy *)in_put->inBuff;
    if (in_buff->qid >= (DEPLOYMENT_EXTER_MAX_TYPE * MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("qid is invalid. (qid=%u)\n", in_buff->qid);
        return DRV_ERROR_PARA_ERROR;
    }

    out_buff = (QueQueryDeployInfo *)out_put->outBuff;
    out_buff->deployType = queue_get_deployment_type_by_qid(in_buff->qid);

    return DRV_ERROR_NONE;
}

static drvError_t queue_query_support_inter_dev_que(QueueQueryOutputPara *out_put)
{
    return queue_query_support_inter_dev_que_platform(out_put); // devid parameter not used in original code
}

static void queue_query_update_input(QueueQueryCmdType cmd, QueueQueryInputPara *in_put)
{
    QueueQueryInput *in_buff = (QueueQueryInput *)(in_put->inBuff);

    if (in_buff == NULL) {
        return;
    }

    switch (cmd) {
        case QUEUE_QUERY_QUE_ATTR_OF_CUR_PROC:
            in_buff->queQueryQueueAttr.qid = (int)queue_get_actual_qid((unsigned int)in_buff->queQueryQueueAttr.qid);
            break;
        case QUEUE_QUERY_QUEUE_MBUF_INFO:
            in_buff->queQueryQueueMbuf.qid = queue_get_actual_qid(in_buff->queQueryQueueMbuf.qid);
            break;
        default:
            break;
    }
}

static void queue_query_update_output(QueueQueryCmdType cmd, QUEUE_DEPLOYMENT_TYPE type, QueueQueryOutputPara *out_put)
{
    QueueQueryOutput *out_buff = (QueueQueryOutput *)(out_put->outBuff);

    switch (cmd) {
        case QUEUE_QUERY_QUES_OF_CUR_PROC: {
            unsigned int num = out_put->outLen / sizeof(QueQueryQuesOfProcInfo);
            unsigned int i;
            for (i = 0; i < num; i++) {
                out_buff->queQueryQuesOfProcInfo[i].qid = (int)queue_get_virtual_qid(
                    (unsigned int)out_buff->queQueryQuesOfProcInfo[i].qid, type);
            }
            break;
        }
        default:
            break;
    }
}

drvError_t halQueueQuery(unsigned int devId, QueueQueryCmdType cmd, QueueQueryInputPara *inPut,
                         QueueQueryOutputPara *outPut)
{
    QUEUE_DEPLOYMENT_TYPE type;
    drvError_t ret;

    ret = queue_query_para_check(devId, cmd, inPut, outPut, __FUNCTION__);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (cmd == QUEUE_QUERY_DEPLOY_TYPE) {
        return queue_query_deploy_type(inPut, outPut);
    }

    if (cmd == QUEUE_QUERY_SUPPORT_INTER_DEV_QUE) {
        return queue_query_support_inter_dev_que(outPut);
    }

    type = queue_get_deployment_type();
    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_query != NULL) {
        queue_query_update_input(cmd, inPut);
        ret = queue_comm_interface[type]->queue_query(devId, cmd, inPut, outPut);
        if (ret == DRV_ERROR_NONE) {
            queue_query_update_output(cmd, type, outPut);
        }
        return ret;
    }
    QUEUE_LOG_INFO("not set queue_query. (deploy_type=%u)\n", type);

    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueuePeekData(unsigned int devId, unsigned int qid, unsigned int flag, QueuePeekDataType type,
                            void **mbuf)
{
    QUEUE_DEPLOYMENT_TYPE deploy_type = que_get_deploy_type_for_inter_dev(devId, qid, EXPORT_SUPPORT);
    unsigned int actual_qid = queue_get_actual_qid(qid);
    if ((devId >= MAX_DEVICE) || (actual_qid >= MAX_SURPORT_QUEUE_NUM) || (mbuf == NULL)) {
        QUEUE_LOG_ERR("input param is invalid. (devId=%u; deploy_type=%u; in_qid=%u; actual_qid=%u; mbuf_is_null=%d)\n",
                      devId, deploy_type, qid, actual_qid, mbuf == NULL);
        return DRV_ERROR_PARA_ERROR;
    }

    if (type >= QUEUE_PEEK_DATA_TYPE_MAX) {
        QUEUE_LOG_INFO("no support this type. (devId=%u; qid=%u; type=%u)\n", devId, qid, type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (queue_comm_interface[deploy_type] != NULL && queue_comm_interface[deploy_type]->queue_peek_data != NULL) {
        return queue_comm_interface[deploy_type]->queue_peek_data(devId, actual_qid, flag, type, mbuf);
    }

    QUEUE_LOG_INFO("not set queue_peek_data. (deploy_type=%u)\n", deploy_type);
    return DRV_ERROR_NOT_SUPPORT;
}

static drvError_t queue_enable_local_queue(void)
{
    if (g_enable_local_queue) {
        return DRV_ERROR_NONE;
    }
#ifdef CFG_FEATURE_HOST_SUPPORT_LOCAL_QUEUE
    unsigned int split_mode;
    drvError_t ret;
    if (queue_is_client_queue_deploy()) {
        ret = halGetDeviceSplitMode(0, &split_mode);
        if ((ret == DRV_ERROR_NONE) && (split_mode == VMNG_NORMAL_NONE_SPLIT_MODE)) {
            g_enable_local_queue = true;
            QUEUE_RUN_LOG_INFO("enable local queue succ.\n");
            return DRV_ERROR_NONE;
        }
    }
#endif
    QUEUE_RUN_LOG_INFO("no support enable local queue.\n");
    return DRV_ERROR_NOT_SUPPORT;
}

static void queueSetUpdateInput(QueueSetCmdType cmd, QueueSetInputPara *input)
{
    QueueSetInput *in_buff = NULL;

    if ((input == NULL) || (input->inBuff == NULL)) {
        return;
    }

    in_buff = (QueueSetInput *)(input->inBuff);
    switch (cmd) {
        case QUEUE_SET_WORK_MODE:
            if (input->inLen >= sizeof(QueueSetWorkMode)) {
                in_buff->queSetWorkMode.qid = queue_get_actual_qid(in_buff->queSetWorkMode.qid);
            }
            break;
        default:
            break;
    }
}

drvError_t halQueueSet(unsigned int devId, QueueSetCmdType cmd, QueueSetInputPara *input)
{
    QueueSetInput *in_buff = NULL;
    QUEUE_DEPLOYMENT_TYPE type = queue_get_deployment_type();
    if (cmd == QUEUE_ENABLE_LOCAL_QUEUE) {
        return queue_enable_local_queue();
    } else if (cmd == QUEUE_ENABLE_CLIENT_EVENT_MCAST) {
        type = CLIENT_QUEUE; /* Only clent queue deploy supports multicast */
    } else if (cmd == QUEUE_SET_WORK_MODE) {
        if ((type == LOCAL_QUEUE) && (input != NULL)) {
            in_buff = (QueueSetInput *)(input->inBuff);
            if ((in_buff != NULL) && (input->inLen >= sizeof(QueueSetInput))) {
                type = que_get_deploy_type_for_inter_dev(devId, in_buff->queSetWorkMode.qid, EXPORT_SUPPORT);
            }
        }
    }

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_set != NULL) {
        queueSetUpdateInput(cmd, input);
        return queue_comm_interface[type]->queue_set(devId, cmd, input);
    }

    QUEUE_LOG_INFO("not set queue_set. (deploy_type=%u)\n", type);
    return DRV_ERROR_NOT_SUPPORT;
}

void clear_inter_dev_queue(unsigned int dev_id)
{
#ifdef CFG_FEATURE_QUE_SUPPORT_UB
    int ret;
    unsigned int qid_idx, inter_qid;
    int inter_dev_state = QUEUE_STATE_DISABLED;
    struct shareQueInfo que_info = {0};
    unsigned int local_dev_id;

    que_get_inter_dev_local_devid_platform(&que_info, dev_id, &local_dev_id);
    for (qid_idx = 0; qid_idx < CLIENT_QID_OFFSET; qid_idx++) {
        ret = que_get_inter_dev_status(local_dev_id, qid_idx, &inter_dev_state, que_info.shareQueName);
        if (ret != DRV_ERROR_NONE) {
            continue;
        }
        inter_qid = queue_get_virtual_qid(qid_idx, INTER_DEV_QUEUE);
        if (inter_dev_state == QUEUE_STATE_EXPORTED) {
            (void)halQueueDestroy(local_dev_id, inter_qid);
        } else if (inter_dev_state == QUEUE_STATE_IMPORTED) {
            (void)halQueueUnimport(local_dev_id, inter_qid, &que_info);
        } else {
        }
    }
#endif
    (void)dev_id;
    return;
}

drvError_t queue_device_open(uint32_t devid, halDevOpenIn *in, halDevOpenOut *out)
{
    (void)in;
    (void)out;

    if (devid >= MAX_DEVICE) {
        QUEUE_LOG_ERR("Input para error. (dev_id=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }
    QUEUE_RUN_LOG_INFO("queue open finish. (dev_id=%u)\n", devid);

    return DRV_ERROR_NONE;
}
drvError_t queue_device_close(uint32_t devid, halDevCloseIn *in)
{
    (void)in;

    if (devid >= MAX_DEVICE) {
        QUEUE_LOG_ERR("Input para error. (dev_id=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    clear_inter_dev_queue(devid);
    queue_uninit(devid, CLOSE_ALL_RES);
    QUEUE_RUN_LOG_INFO("queue close finish. (dev_id=%u)\n", devid);
    return DRV_ERROR_NONE;
}

drvError_t queue_device_close_user(uint32_t devid, halDevCloseIn *in)
{
    (void)in;

    if (devid >= MAX_DEVICE) {
        QUEUE_LOG_ERR("Input para error. (dev_id=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    clear_inter_dev_queue(devid);
    queue_uninit(devid, CLOSE_USER_RES);
    QUEUE_RUN_LOG_INFO("queue close finish. (dev_id=%u)\n", devid);
    return DRV_ERROR_NONE;
}
void queue_finish_callback(unsigned int devid, unsigned int grp_id, esched_event_info event_info)
{
    unsigned int qid = queue_get_actual_qid(event_info.subevent_id);
    unsigned int notify_dev = devid;
    QUEUE_DEPLOYMENT_TYPE type;

    if (que_unlikely((devid >= MAX_DEVICE) || (qid >= MAX_SURPORT_QUEUE_NUM))) {
        QUEUE_LOG_ERR("invalid para. (devid=%u; max_devid=%u; qid=%u; max_qid=%u)\n", devid, (unsigned int)MAX_DEVICE,
                      qid, (unsigned int)MAX_SURPORT_QUEUE_NUM);
        return;
    }
    if (queue_is_client_queue_deploy()) {
        struct QueueEventMsg *que_msg = (struct QueueEventMsg *)event_info.msg;
        type = (que_msg->src_location == EVENT_SRC_LOCATION_DEVICE) ? CLIENT_QUEUE : LOCAL_QUEUE;
        if (type == CLIENT_QUEUE) {
            int ret = uda_get_devid_by_udevid(que_msg->src_udevid, &notify_dev);
            if (ret) {
                QUEUE_LOG_ERR("uda_get_devid_by_udevid failed. (src_udevid=%u; ret=%d)\n", que_msg->src_udevid, ret);
                return;
            }
        }
    } else {
        type = LOCAL_QUEUE;
    }

    if (queue_comm_interface[type] != NULL && queue_comm_interface[type]->queue_finish_cb != NULL) {
        queue_comm_interface[type]->queue_finish_cb(notify_dev, qid, grp_id, event_info.event_id);
        return;
    }

    QUEUE_LOG_INFO("not set queue_finish_callback. (deploy_type=%u)\n", type);
    return;
}

drvError_t queue_register_callback(unsigned int groupid)
{
    drvError_t ret;

    ret = esched_register_finish_func_ex((unsigned int)groupid, EVENT_QUEUE_ENQUEUE, queue_finish_callback);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("register e_sched_register_finish_func failed. (grpid=%u)\n", groupid);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

drvError_t halQueueModeNotify(PSM_STATUS status, void *rsv)
{
    (void)status;
    (void)rsv;
    return DRV_ERROR_NOT_SUPPORT;
}

bool queue_inter_devid_invalid(unsigned int dev_id)
{
    return queue_inter_devid_invalid_platform(dev_id);
}

static drvError_t queue_inter_dev_para_check(unsigned int dev_id, unsigned int qid, unsigned int actual_qid,
                                             struct shareQueInfo *que_info)
{
    int len;

    if (actual_qid >= MAX_SURPORT_QUEUE_NUM) {
        QUEUE_LOG_ERR("para is error. (dev_id=%u, qid=%u, actual_qid=%u)\n", dev_id, qid, actual_qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (que_info == NULL) {
        QUEUE_LOG_ERR("share que info is NULL. (dev_id=%u, qid=%u, actual_qid=%u)\n", dev_id, qid, actual_qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    len = (int)strnlen(que_info->shareQueName, SHARE_QUEUE_NAME_LEN);
    if ((len > SHARE_QUEUE_NAME_LEN) || (len == 0)) {
        QUEUE_LOG_ERR("share que name len err. (dev_id=%u, qid=%u, actual_qid=%u, len=%d, max_len=%d)\n", dev_id, qid,
                      actual_qid, len, SHARE_QUEUE_NAME_LEN);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t halQueueExport(unsigned int devId, unsigned int qid, struct shareQueInfo *queInfo)
{
    unsigned int actual_qid = queue_get_actual_qid(qid);
    drvError_t ret;

    ret = queue_inter_dev_para_check(devId, qid, actual_qid, queInfo);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (queue_comm_interface[INTER_DEV_QUEUE] != NULL && queue_comm_interface[INTER_DEV_QUEUE]->queue_export != NULL) {
        return queue_comm_interface[INTER_DEV_QUEUE]->queue_export(devId, actual_qid, queInfo);
    }

    QUEUE_LOG_INFO("not set queue_export.\n");
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueUnexport(unsigned int devId, unsigned int qid, struct shareQueInfo *queInfo)
{
    unsigned int actual_qid = queue_get_actual_qid(qid);
    drvError_t ret;

    ret = queue_inter_dev_para_check(devId, qid, actual_qid, queInfo);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (queue_comm_interface[INTER_DEV_QUEUE] != NULL &&
        queue_comm_interface[INTER_DEV_QUEUE]->queue_unexport != NULL) {
        return queue_comm_interface[INTER_DEV_QUEUE]->queue_unexport(devId, actual_qid, queInfo);
    }

    QUEUE_LOG_INFO("not set queue_unexport.\n");
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueImport(unsigned int devId, struct shareQueInfo *queInfo, unsigned int *qid)
{
    drvError_t ret;
    int len;

    if ((queInfo == NULL) || (qid == NULL)) {
        QUEUE_LOG_ERR("Input para error. (devId=%u, que_info_is_null=%d, qid_is_null=%d)\n", devId,
                      (int)(queInfo == NULL), (int)(qid == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    len = (int)strnlen(queInfo->shareQueName, SHARE_QUEUE_NAME_LEN);
    if ((len > SHARE_QUEUE_NAME_LEN) || (len == 0)) {
        QUEUE_LOG_ERR("key len err. (devId=%u, len=%d, max_len=%d)\n", devId, len, SHARE_QUEUE_NAME_LEN);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = queue_real_init_iner(devId, LOCAL_QUEUE);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        return ret;
#endif
    }

    if (queue_comm_interface[INTER_DEV_QUEUE] != NULL && queue_comm_interface[INTER_DEV_QUEUE]->queue_import != NULL) {
        ret = queue_comm_interface[INTER_DEV_QUEUE]->queue_import(devId, queInfo, qid);
        if (ret == DRV_ERROR_NONE) {
            QUEUE_RUN_LOG_INFO("queue import success. (virtual_qid=%u, actual_qid=%u, deploy_type=%u)\n", *qid,
                               queue_get_actual_qid(*qid), LOCAL_QUEUE);
        }
        return ret;
    }

    QUEUE_LOG_INFO("not set queue_import.\n");
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueUnimport(unsigned int devId, unsigned int qid, struct shareQueInfo *queInfo)
{
    unsigned int actual_qid = queue_get_actual_qid(qid);
    drvError_t ret;

    ret = queue_inter_dev_para_check(devId, qid, actual_qid, queInfo);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = queue_real_init_iner(devId, LOCAL_QUEUE);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        return ret;
#endif
    }

    if (queue_comm_interface[INTER_DEV_QUEUE] != NULL &&
        queue_comm_interface[INTER_DEV_QUEUE]->queue_unimport != NULL) {
        return queue_comm_interface[INTER_DEV_QUEUE]->queue_unimport(devId, actual_qid, queInfo);
    }

    QUEUE_LOG_INFO("not set queue_unimport.\n");
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halQueueGetDqsQueInfo(unsigned int devId, unsigned int qid, DqsQueueInfo *info)
{
    (void)devId;
    (void)qid;
    (void)info;
    return DRV_ERROR_NOT_SUPPORT;
}