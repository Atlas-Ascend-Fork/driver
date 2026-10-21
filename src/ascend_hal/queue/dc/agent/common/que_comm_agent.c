/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdlib.h>

#ifndef EMU_ST
#include "svm_user_interface.h"
#endif
#include "queue.h"
#include "que_ub_msg.h"
#include "que_uma.h"
#include "que_compiler.h"
#include "queue_interface.h"
#include "que_comm_agent.h"
#include "que_platform_ub.h"

#ifndef EMU_ST
struct que_agent_interface_list que_agent_interface = {NULL};

struct que_agent_interface_list *que_get_agent_interface(void)
{
    return &que_agent_interface;
}

struct que_ctx *que_ctx_get(unsigned int devid)
{
    return que_agent_interface.que_ctx_get(devid);
}

void que_ctx_put(struct que_ctx *ctx)
{
    que_agent_interface.que_ctx_put(ctx);
}

int que_ctx_add(struct que_ctx *ctx)
{
    return que_agent_interface.que_ctx_add(ctx);
}

int que_ctx_del(struct que_ctx *ctx)
{
    return que_agent_interface.que_ctx_del(ctx);
}

struct que_chan *que_chan_get(unsigned int devid, unsigned int qid)
{
    return que_agent_interface.que_chan_get(devid, qid);
}

void que_chan_put(struct que_chan *chan)
{
    que_agent_interface.que_chan_put(chan);
}

int que_chan_add(struct que_chan *chan)
{
    return que_agent_interface.que_chan_add(chan);
}

int que_chan_del(struct que_chan *chan)
{
    return que_agent_interface.que_chan_del(chan);
}

int que_ub_res_init(unsigned int devid)
{
    return que_agent_interface.que_ub_res_init(devid);
}

void que_ub_res_uninit(unsigned int devid, bool uninit_flag)
{
    return que_agent_interface.que_ub_res_uninit(devid, uninit_flag);
}

drvError_t que_mem_alloc(void **addr, unsigned long long size)
{
    return que_agent_interface.que_mem_alloc(addr, size);
}

void que_mem_free(void *addr)
{
    return que_agent_interface.que_mem_free(addr);
}

bool que_is_share_mem(unsigned long long addr)
{
    return que_is_share_mem_platform(addr);
}

void queue_agent_update_time(struct timeval start, struct timeval end, int *timeout)
{
    if (*timeout > 0) {
#ifndef EMU_ST
        long tmp = *timeout;
        tmp -= (end.tv_sec - start.tv_sec) * MS_PER_SECOND + (end.tv_usec - start.tv_usec) / US_PER_MSECOND;
        *timeout = tmp > 0 ? (int)tmp : 0;
#endif
    }
}

int que_get_peer_que_info(unsigned int dev_id, unsigned int qid, unsigned int *remote_qid,
                          struct que_peer_que_attr *peer_que_attr)
{
    struct queue_manages *que_manage = NULL;

    if ((dev_id >= MAX_DEVICE) || (qid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("para is error. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
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
        QUEUE_LOG_ERR(" queue(%u) is not created.\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if ((que_manage->inter_dev_state != QUEUE_STATE_EXPORTED) &&
        (que_manage->inter_dev_state != QUEUE_STATE_IMPORTED)) {
        queue_put(qid);
        return DRV_ERROR_PARA_ERROR;
    }

    *remote_qid = que_manage->remote_qid;
    if (peer_que_attr != NULL) {
        peer_que_attr->inter_dev_state = que_manage->inter_dev_state;
        peer_que_attr->tjfr_id = que_manage->tjfr_id;
        peer_que_attr->tjfr_valid_flag = que_manage->tjfr_valid_flag;
        peer_que_attr->depth = queue_get_local_depth(qid);
        peer_que_attr->peer_devid = que_manage->remote_devid;
        peer_que_attr->token = que_manage->token;
    }

    /* Ensure other parameters are valid */
    if ((que_manage->remote_qid == QUEUE_INVALID_VALUE) || (*remote_qid == QUEUE_INVALID_VALUE)) {
        queue_put(qid);
        return DRV_ERROR_NO_RESOURCES; /* export but not import */
    }

    queue_put(qid);
    return DRV_ERROR_NONE;
}

int que_get_peer_proc_info(unsigned int dev_id, unsigned int qid, pid_t *remote_pid, unsigned int *remote_devid,
                           unsigned int *remote_grpid)
{
    struct queue_manages *que_manage = NULL;

    if ((dev_id >= MAX_DEVICE) || (qid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("para is error. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
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
        QUEUE_LOG_ERR(" queue(%u) is not created.\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    *remote_pid = que_manage->remote_devpid;
    *remote_devid = que_manage->phy_remote_devid;
    *remote_grpid = (unsigned int)que_manage->remote_grpid;
    queue_put(qid);
    return DRV_ERROR_NONE;
}

int que_save_inter_dev_attach_cfg(unsigned int dev_id, unsigned int qid, const struct que_inter_dev_attach_in_msg *in)
{
    struct queue_manages *que_manage = NULL;

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (dev_id=%u; qid=%u)\n", dev_id, qid);
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
        QUEUE_LOG_ERR(" queue(%u) is not created.\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if ((que_manage->inter_dev_state != QUEUE_STATE_EXPORTED) &&
        (que_manage->inter_dev_state != QUEUE_STATE_IMPORTED)) {
        queue_put(qid);
        return DRV_ERROR_NOT_SUPPORT;
    }

    que_manage->tjfr_id = in->tjfr_id;
    que_manage->token = in->token;
    queue_set_local_depth(qid, in->depth);
    que_manage->tjfr_valid_flag = 1;
    queue_put(qid);
    return DRV_ERROR_NONE;
}

drvError_t que_inter_dev_send_f2nf(unsigned int dev_id, unsigned int qid)
{
    struct queue_manages *que_manage = NULL;

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (dev_id=%u; qid=%u)\n", dev_id, qid);
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
        QUEUE_LOG_ERR(" queue(%u) is not created.\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    send_f_to_nf_event(dev_id, que_manage);
    queue_put(qid);
    return DRV_ERROR_NONE;
}

static inline bool que_inter_export_queue_status(unsigned int dev_id, int pid, unsigned int qid, int *inter_dev_state)
{
    struct queue_manages *que_manage = NULL;

    if (!queue_get(qid)) {
        return false;
    }

    que_manage = queue_get_local_mng(qid);
    if (que_manage == NULL) {
        queue_put(qid);
        return false;
    }

    if ((que_manage->valid != QUEUE_CREATED) || (que_manage->dev_id != dev_id) || (que_manage->creator_pid != pid)) {
        queue_put(qid);
        return false;
    }

    *inter_dev_state = que_manage->inter_dev_state;
    if ((que_manage->inter_dev_state != QUEUE_STATE_EXPORTED) || (que_manage->remote_qid == QUEUE_INVALID_VALUE)) {
        queue_put(qid);
        return false;
    }
    queue_put(qid);
    return true;
}

void que_get_single_export_que_import_stat(unsigned int dev_id, unsigned int qid, unsigned int *status)
{
    int is_imported, inter_dev_state;
    unsigned int not_imported_num = 0;
    int inter_export_queue_num = 0;
    int pid = getpid();

    inter_dev_state = QUEUE_STATE_DISABLED;
    is_imported = que_inter_export_queue_status(dev_id, pid, qid, &inter_dev_state);
    if (inter_dev_state == QUEUE_STATE_EXPORTED) {
        if (is_imported == false) {
            not_imported_num++;
        }
        inter_export_queue_num++;
    }
    if (inter_export_queue_num == 0) {
        not_imported_num = QUEUE_INVALID_VALUE;
    }

    *status = not_imported_num;
    return;
}

void que_get_all_export_que_import_stat(unsigned int dev_id, unsigned int *status)
{
    int qid_idx, is_imported, inter_dev_state;
    unsigned int not_imported_num = 0;
    int inter_export_queue_num = 0;
    int pid = getpid();

    for (qid_idx = 0; qid_idx < MAX_SURPORT_QUEUE_NUM; qid_idx++) {
        inter_dev_state = QUEUE_STATE_DISABLED;
        is_imported = que_inter_export_queue_status(dev_id, pid, (unsigned int)qid_idx, &inter_dev_state);
        if (inter_dev_state == QUEUE_STATE_EXPORTED) {
            if (is_imported == false) {
                not_imported_num++;
            }
            inter_export_queue_num++;
        }
    }

    if (inter_export_queue_num == 0) {
        not_imported_num = QUEUE_INVALID_VALUE;
    }

    *status = not_imported_num;
    return;
}

unsigned int que_get_chan_devid(unsigned int devid)
{
    return que_get_chan_devid_platform(devid);
}

unsigned int que_get_urma_devid(unsigned int devid, unsigned int peer_devid)
{
    return (devid == halGetHostDevid()) ? peer_devid : devid;
}

uint64_t que_get_cur_time_ns(void)
{
    struct timespec timestamp;
    (void)clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return (uint64_t)((timestamp.tv_sec * NS_PER_SECOND) + timestamp.tv_nsec);
}
#else /* EMU_ST */

void que_comm_agent_emu_test(void)
{}

#endif /* EMU_ST */
