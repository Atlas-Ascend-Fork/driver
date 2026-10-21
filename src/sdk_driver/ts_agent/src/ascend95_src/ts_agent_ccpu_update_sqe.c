/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifdef CFG_SOC_PLATFORM_DAVID
#include "securec.h"
#include "ts_agent_log.h"
#include "ts_agnet_ccpu.h"
#include "comm_kernel_interface.h"
#include "trs_adapt.h"
#ifndef TS_AGENT_UT
#include "tsch/task_struct.h"
#else
#include "task_struct.h"
#endif

typedef int32_t (*sqe_hook_proc_t)(uint32_t devid, uint32_t tsid, int32_t pid, uint32_t sqid, ts_stars_sqe_t *sqe);
static sqe_hook_proc_t g_sqe_proc_fn[TS_STARS_SQE_TYPE_END] = {NULL};

STATIC int32_t sqe_proc_ccpu_pciedma(uint32_t devid, uint32_t tsid, int32_t pid, uint32_t sqid, ts_stars_sqe_t *sqe)
{
#ifdef CFG_DEVICE_ENV
    ts_agent_err("task is invalid, devid=%u, stream_id=%u, task_id=%u, pid=%d, tsid=%u, type=%u", devid, sqe->stream_id,
                 sqe->task_id, pid, tsid, sqe->type);
    return -EINVAL;
#endif
    int32_t ret = EOK;
    ccpu_stars_pcie_dma_sqe *pcie_sqe = (ccpu_stars_pcie_dma_sqe *)sqe;
    struct devdrv_dma_desc_info check_dma = {
        .sq_dma_addr = ((uint64_t)pcie_sqe->pcie_dma_sq_addr_high << 32U) | (uint64_t)pcie_sqe->pcie_dma_sq_addr_low,
        .sq_size = (uint64_t)pcie_sqe->pcie_dma_sq_tail_ptr,
    };

    ret = devdrv_dma_sqcq_desc_check(devid, &check_dma);
    if (ret != 0) {
        ts_agent_err("pciedma desc check failed, ret=%d, devid=%u, stream_id=%u, task_id=%u, "
                     "sq_dma_addr=%#llx, sq_tail=%llu.",
                     ret, devid, pcie_sqe->header.rt_stream_id, pcie_sqe->header.task_id, check_dma.sq_dma_addr,
                     check_dma.sq_size);
    }
    return ret;
}

STATIC int32_t update_info_check(uint32_t devid, uint32_t tsid, struct trs_sqe_update_info *update_info)
{
    if (update_info == NULL) {
        ts_agent_err("update_info is NULL, devid=%u, tsid=%u.", devid, tsid);
        return -EINVAL;
    }

    if (update_info->long_sqe_cnt == NULL) {
        ts_agent_err("long_sqe_cnt is NULL, devid=%u, tsid=%u, sqid=%u, pid=%d.", devid, tsid, update_info->sqid,
                     update_info->pid);
        return -EINVAL;
    }

    if (update_info->sqe == NULL) {
        ts_agent_err("sqe is NULL, devid=%u, tsid=%u, sqid=%u, pid=%d.", devid, tsid, update_info->sqid,
                     update_info->pid);
        return -EINVAL;
    }
    return EOK;
}

int32_t tsagent_sqe_update(uint32_t devid, uint32_t tsid, struct trs_sqe_update_info *update_info)
{
    ts_stars_sqe_t *sqe = NULL;
    sqe_hook_proc_t proc_fn = NULL;
    int32_t ret = EOK;
    uint32_t old_cnt = 0U;

    ret = update_info_check(devid, tsid, update_info);
    if (ret != 0) {
        ts_agent_err("update_info check failed, ret=%d.", ret);
        return ret;
    }

    if (*(update_info->long_sqe_cnt) != 0U) {
        ts_agent_debug("sqe is not header, no need to update, devid=%u, pid=%d, sqid=%u, long_sqe_cnt=%u.", devid,
                       update_info->pid, update_info->sqid, *(update_info->long_sqe_cnt));
        return EOK;
    }

    sqe = (ts_stars_sqe_t *)update_info->sqe;
    if (sqe->sqe_length > TS_AGENT_SQE_LENGTH_MAX) {
        ts_agent_err("sqe_length is invalid, valid range is [0, %u], sqe_length=%hhu, devid=%u, tsid=%u, "
                     "sqid=%u, pid=%d.",
                     TS_AGENT_SQE_LENGTH_MAX, sqe->sqe_length, devid, tsid, update_info->sqid, update_info->pid);
        sqe->type = TS_STARS_SQE_TYPE_INVALID;
        return -EINVAL;
    }

    if (sqe->type >= TS_STARS_SQE_TYPE_END) {
        ts_agent_err("sqe type is invalid, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.", sqe->type, devid,
                     sqe->stream_id, sqe->task_id, update_info->pid);
        sqe->type = TS_STARS_SQE_TYPE_INVALID;
        return -EINVAL;
    }

    old_cnt = *(update_info->long_sqe_cnt);
    if (sqe->sqe_length != 0) {
        *(update_info->long_sqe_cnt) = sqe->sqe_length + 1;
    }

    proc_fn = g_sqe_proc_fn[sqe->type];
    if (proc_fn == NULL) {
        ts_agent_debug("sqe no need update, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.", sqe->type, devid,
                       sqe->stream_id, sqe->task_id, update_info->pid);
        return EOK;
    }

    if (update_info->sqid == KA_U32_MAX) {
        ts_agent_debug("tsagent_sqe_update: stream_id=%u, sqe_type=%u, devid=%u, task_id=%u, pid=%d.", sqe->stream_id,
                       sqe->type, devid, sqe->task_id, update_info->pid);
    }

    ret = proc_fn(devid, tsid, update_info->pid, update_info->sqid, sqe);
    if (ret != 0) {
        ts_agent_err("sqe update failed, ret=%d, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.", ret,
                     sqe->type, devid, sqe->stream_id, sqe->task_id, update_info->pid);
        sqe->type = TS_STARS_SQE_TYPE_INVALID;
        *(update_info->long_sqe_cnt) = old_cnt;
        return ret;
    }
    return EOK;
}

STATIC int32_t update_src_info_check(uint32_t devid, uint32_t tsid, struct trs_sqe_update_info *update_info)
{
    if (update_info == NULL) {
        ts_agent_err("update_info is NULL, devid=%u, tsid=%u.", devid, tsid);
        return -EINVAL;
    }

    // 目的sqe基地址
    if (update_info->sq_base == NULL) {
        ts_agent_err("sq_base is NULL, devid=%u, tsid=%u, sqid=%u, pos=%u, pid=%d.", devid, tsid, update_info->sqid,
                     update_info->sqeid, update_info->pid);
        return -EINVAL;
    }

    if (update_info->sqe == NULL) {
        ts_agent_err("sqe is NULL, devid=%u, tsid=%u, sqid=%u, pid=%d.", devid, tsid, update_info->sqid,
                     update_info->pid);
        return -EINVAL;
    }

    return EOK;
}

int32_t tsagent_sqe_update_src_check(uint32_t devid, uint32_t tsid, struct trs_sqe_update_info *update_info)
{
    ts_stars_sqe_t *sqe = NULL;
    ts_stars_sqe_t *sqe_dest = NULL;

    int32_t ret = update_src_info_check(devid, tsid, update_info);
    if (ret != EOK) {
        ts_agent_err("update_info check failed, ret=%d.", ret);
        return ret;
    }

    sqe = (ts_stars_sqe_t *)update_info->sqe;
    if (update_info->addr_non_continuous) {
        sqe_dest = (ts_stars_sqe_t *)update_info->dst_sqe;
    } else {
        sqe_dest = (ts_stars_sqe_t *)(update_info->sq_base) + update_info->sqeid;
    }

    if ((sqe->sqe_length > TS_AGENT_SQE_LENGTH_MAX) ||
        ((sqe->sqe_length + 1) * sizeof(ts_stars_sqe_t) != update_info->size)) {
        ts_agent_err("sqe_length is invalid, valid range is [0, %u], sqe_length=%hhu, sqe_size=%u, devid=%u, tsid=%u,"
                     " sqid=%u, pid=%d.",
                     TS_AGENT_SQE_LENGTH_MAX, sqe->sqe_length, update_info->size, devid, tsid, update_info->sqid,
                     update_info->pid);
        return -EINVAL;
    }

    if ((sqe->type >= TS_STARS_SQE_TYPE_END) || (sqe->type == TS_STARS_SQE_TYPE_ASYNCDMA)) {
        ts_agent_err("sqe type is invalid, sqe_type=%u, devid=%u, stream_id=%u, task_id=%u, pid=%d.", sqe->type, devid,
                     sqe->stream_id, sqe->task_id, update_info->pid);
        return -EINVAL;
    }

    if ((sqe->sqe_length != sqe_dest->sqe_length) || (sqe->type != sqe_dest->type) ||
        (sqe->stream_id != sqe_dest->stream_id) || (sqe->task_id != sqe_dest->task_id)) {
        ts_agent_err("sqe info is incorrect, sqe_type=%u,%u, stream_id=%hu,%hu, task_id=%hu,%hu, devid=%u, pid=%d.",
                     sqe_dest->type, sqe->type, sqe_dest->stream_id, sqe->stream_id, sqe_dest->task_id, sqe->task_id,
                     devid, update_info->pid);
        return -EINVAL;
    }

    return EOK;
}

void init_task_convert_func(void)
{
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_AIC] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_AIV] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_FUSION] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_PLACE_HOLDER] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_AICPU_H] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_AICPU_D] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_NOTIFY_RECORD] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_NOTIFY_WAIT] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_WRITE_VALUE] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_UBDMA] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_ASYNCDMA] = sqe_proc_ccpu_pciedma; // david only check pcie dma
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_SDMA] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_VPC] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_JPEGE] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_JPEGD] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_CMO] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_CCU] = NULL;
    g_sqe_proc_fn[TS_STARS_SQE_TYPE_COND] = NULL;
}
#endif
