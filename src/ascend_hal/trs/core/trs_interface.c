/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <limits.h>
#include "ascend_hal.h"
#include "drv_user_common.h"
#include "trs_user_pub_def.h"
#include "trs_ioctl.h"
#include "trs_res.h"
#include "trs_sqcq.h"
#include "trs_dev_drv.h"
#include "trs_user_interface.h"
#include "trs_master_urma.h"
#include "trs_master_async.h"
#include "trs_master_event.h"
#include "trs_urma_async.h"

#ifndef EMU_ST
drvError_t __attribute__((weak)) trs_urma_res_config(uint32_t dev_id, struct halResourceIdInputInfo *in,
                                                     struct halResourceConfigInfo *para)
{
    (void)dev_id;
    (void)in;
    (void)para;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_sqcq_urma_alloc(uint32_t dev_id, struct halSqCqInputInfo *in,
                                                     struct halSqCqOutputInfo *out)
{
    (void)dev_id;
    (void)in;
    (void)out;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_sqcq_urma_free(uint32_t dev_id, struct halSqCqFreeInfo *info,
                                                    bool remote_free_flag)
{
    (void)dev_id;
    (void)info;
    (void)remote_free_flag;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_sq_cq_query_sync(uint32_t dev_id, struct halSqCqQueryInfo *info)
{
    (void)dev_id;
    (void)info;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_sq_cq_config_sync(uint32_t dev_id, struct halSqCqConfigInfo *info)
{
    (void)dev_id;
    (void)info;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_sq_task_args_async_copy(uint32_t dev_id, struct halSqTaskArgsInfo *info,
                                                             struct sqcq_usr_info *sq_info)
{
    (void)dev_id;
    (void)info;
    (void)sq_info;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_async_dma_wqe_create(uint32_t dev_id, struct trs_async_dma_input_para *in,
                                                          struct halAsyncDmaOutputPara *out)
{
    (void)dev_id;
    (void)in;
    (void)out;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_async_dma_wqe_destory(uint32_t dev_id, struct trs_async_dma_destroy_para *para)
{
    (void)dev_id;
    (void)para;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_sq_task_send_urma(uint32_t dev_id, struct halTaskSendInfo *info,
                                                       struct sqcq_usr_info *sq_info)
{
    (void)dev_id;
    (void)info;
    (void)sq_info;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_get_urma_tseg_info_by_va(uint32_t devid, uint64_t va, uint64_t size, uint32_t flag,
                                                              struct halTsegInfo *tsegInfo)
{
    (void)devid;
    (void)va;
    (void)size;
    (void)flag;
    (void)tsegInfo;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_put_urma_tseg_info(uint32_t devid, struct halTsegInfo *tsegInfo)
{
    (void)devid;
    (void)tsegInfo;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_sq_jetty_info_query(uint32_t devid, struct halSqCqQueryInfo *info)
{
    (void)devid;
    (void)info;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_async_dma_jetty_create(uint32_t dev_id, struct halAsyncDmaJettyCreateIn *in,
                                                            struct halAsyncDmaJettyCreateOut *out)
{
    (void)dev_id;
    (void)in;
    (void)out;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_async_dma_jetty_destroy(uint32_t dev_id, struct halAsyncJettyDestroyPara *para)
{
    (void)dev_id;
    (void)para;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_async_dma_jetty_query(uint32_t dev_id, struct halAsyncDmaJettyQueryIn *in,
                                                           struct halAsyncDmaJettyQueryOut *out)
{
    (void)dev_id;
    (void)in;
    (void)out;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_async_dma_wqe_convert(uint32_t dev_id, struct halAsyncDmaWqeInputPara *in,
                                                           struct halAsyncDmaWqeOutputPara *out)
{
    (void)dev_id;
    (void)in;
    (void)out;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) trs_async_dma_jetty_fill(uint32_t dev_id, struct halAsyncDmaJettyFillInfo *para)
{
    (void)dev_id;
    (void)para;
    return DRV_ERROR_NOT_SUPPORT;
}

#endif

static bool trs_is_remote_res_config_ops(uint32_t flag)
{
    if (drvGetRuntimeApiVer() >= TRS_MC2_BIND_LOGICCQ_FEATURE) {
        if ((flag & TSDRV_FLAG_REMOTE_ID) != 0) {
            return true;
        }
    } else {
        trs_debug("Not support. (runtime_ver=0x%x; drv_ver=0x%x)\n", drvGetRuntimeApiVer(), __HAL_API_VERSION);
    }
    return false;
}

drvError_t halResourceConfig(uint32_t devId, struct halResourceIdInputInfo *in, struct halResourceConfigInfo *para)
{
    drvError_t ret;

    ret = trs_res_config_para_check(devId, in, para);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    if ((trs_is_remote_res_config_ops(in->res[RESOURCEID_RESV_FLAG]))) {
        return trs_remote_res_config(devId, in, para);
    }

    if (((in->type == DRV_NOTIFY_ID) || (in->type == DRV_CNT_NOTIFY_ID)) &&
        ((para->prop == DRV_ID_RECORD) || (para->prop == DRV_ID_RESET))) {
        int connection_type = trs_get_connection_type(devId);
        switch (connection_type) {
            case TRS_CONNECT_PROTOCOL_PCIE:
            case TRS_CONNECT_PROTOCOL_HCCS:
            case TRS_CONNECT_PROTOCOL_RC:
                return trs_local_res_config(devId, in, para);
            case TRS_CONNECT_PROTOCOL_UB:
                return trs_urma_res_config(devId, in, para);
            default:
                trs_err("Invalid connection type. (dev_id=%u; connection_type=%d)\n", devId, connection_type);
                return DRV_ERROR_INVALID_DEVICE;
        }
    } else {
        return trs_local_res_config(devId, in, para);
    }
}

#define TRS_STREAM_TASK_QUEUE_MAX_DEPTH 32768U
drvError_t halStreamTaskFill(uint32_t dev_id, uint32_t stream_id, void *stream_mem, void *task_info, uint32_t task_cnt)
{
    drvError_t ret = 0;

    if (dev_id >= TRS_DEV_NUM) {
        trs_err("Invalid devId. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((task_info == NULL) || (task_cnt == 0) || (task_cnt > TRS_STREAM_TASK_QUEUE_MAX_DEPTH)) {
        trs_err("Null ptr or task_cnt invalid. (ptr_is_NULL=%d; task_cnt=%u)\n", (task_info == NULL), task_cnt);
        return DRV_ERROR_INVALID_VALUE;
    }

    trs_dev_ctx_stream_mutex_lock(dev_id);
    ret = trs_stream_task_fill(dev_id, stream_id, stream_mem, task_info, task_cnt);
    trs_dev_ctx_stream_mutex_unlock(dev_id);

    return ret;
}

drvError_t halSqSwitchStreamBatch(uint32_t dev_id, struct sq_switch_stream_info *info, uint32_t num)
{
    drvError_t ret = 0;

    if (dev_id >= TRS_DEV_NUM) {
        trs_err("Invalid devId. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    trs_dev_ctx_stream_mutex_lock(dev_id);
    ret = trs_sq_switch_stream_batch(dev_id, info, num);
    if (ret != 0) {
        trs_dev_ctx_stream_mutex_unlock(dev_id);
        return ret;
    }

    trs_dev_ctx_stream_mutex_unlock(dev_id);
    return ret;
}

drvError_t halSqCqAllocate(uint32_t devId, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
{
    drvError_t ret;

    ret = trs_sqcq_alloc_para_check(devId, in, out);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if ((in->type == DRV_NORMAL_TYPE) && !trs_is_remote_sqcq_ops(in->type, in->flag)) {
        int connection_type = trs_get_connection_type(devId);
        switch (connection_type) {
            case TRS_CONNECT_PROTOCOL_PCIE:
            case TRS_CONNECT_PROTOCOL_HCCS:
            case TRS_CONNECT_PROTOCOL_RC:
                ret = trs_sqcq_alloc(devId, in, out);
                break;
            case TRS_CONNECT_PROTOCOL_UB:
                ret = trs_sqcq_urma_alloc(devId, in, out);
                break;
            default:
                trs_err("Invalid connection type. (dev_id=%u; connection_type=%d)\n", devId, connection_type);
                return DRV_ERROR_INVALID_DEVICE;
        }
    } else {
        ret = trs_sqcq_alloc(devId, in, out);
    }

    return ret;
}

drvError_t halSqCqFree(uint32_t devId, struct halSqCqFreeInfo *info)
{
    drvError_t ret;

    ret = trs_sqcq_free_para_check(devId, info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if ((info->type == DRV_NORMAL_TYPE) && !trs_is_remote_sqcq_ops(info->type, info->flag)) {
        int connection_type = trs_get_connection_type(devId);
        switch (connection_type) {
            case TRS_CONNECT_PROTOCOL_PCIE:
            case TRS_CONNECT_PROTOCOL_HCCS:
            case TRS_CONNECT_PROTOCOL_RC:
                return trs_sqcq_free(devId, info);
            case TRS_CONNECT_PROTOCOL_UB:
                return trs_sqcq_urma_free(devId, info, true);
            default:
                trs_err("Invalid connection type. (dev_id=%u; connection_type=%d)\n", devId, connection_type);
                return DRV_ERROR_INVALID_DEVICE;
        }
    } else {
        return trs_sqcq_free(devId, info);
    }
}

static bool trs_is_remote_sq_cq_query(uint32_t devId, struct halSqCqQueryInfo *info)
{
    if (trs_is_remote_sqcq_ops(info->type, info->value[SQCQ_QUERY_INFO_FLAG])) {
        if (trs_get_sq_info(devId, info->tsId, info->type, info->sqId) == NULL) {
            return true;
        }
    }
    return false;
}

drvError_t halSqCqQuery(uint32_t devId, struct halSqCqQueryInfo *info)
{
    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (info->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u)\n", devId, info->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (trs_is_remote_sq_cq_query(devId, info)) {
        return trs_sq_cq_query_sync(devId, info);
    }

    if (info->type == DRV_NORMAL_TYPE) {
        int connection_type = trs_get_connection_type(devId);
        switch (connection_type) {
            case TRS_CONNECT_PROTOCOL_PCIE:
            case TRS_CONNECT_PROTOCOL_HCCS:
            case TRS_CONNECT_PROTOCOL_RC:
                if ((info->type == DRV_NORMAL_TYPE) &&
                    ((info->prop == DRV_SQCQ_PROP_H2D_ASYNC_JETTY_INFO) ||
                     (info->prop == DRV_SQCQ_PROP_D2D_ASYNC_JETTY_INFO) ||
                     (info->prop == DRV_SQCQ_PROP_D2D_CROSS_BOARD_ASYNC_JETTY_INFO))) {
                    return DRV_ERROR_NOT_SUPPORT;
                }
                return trs_sq_cq_query(devId, info);
            case TRS_CONNECT_PROTOCOL_UB:
                if ((info->type == DRV_NORMAL_TYPE) &&
                    ((info->prop == DRV_SQCQ_PROP_H2D_ASYNC_JETTY_INFO) ||
                     (info->prop == DRV_SQCQ_PROP_D2D_ASYNC_JETTY_INFO) ||
                     (info->prop == DRV_SQCQ_PROP_D2D_CROSS_BOARD_ASYNC_JETTY_INFO))) {
                    return trs_sq_jetty_info_query(devId, info);
                }
                if ((info->type == DRV_NORMAL_TYPE) &&
                    ((info->prop == DRV_SQCQ_PROP_SQ_HEAD) || (info->prop == DRV_SQCQ_PROP_SQ_CQE_STATUS) ||
                     (info->prop == DRV_SQCQ_PROP_SQ_DEPTH) || (info->prop == DRV_SQCQ_PROP_SQ_MEM_ATTR) ||
                     (info->prop == DRV_SQCQ_PROP_SQ_BASE))) {
                    return trs_sq_cq_query(devId, info);
                }
                return trs_sq_cq_query_sync(devId, info);
            default:
                trs_err("Invalid connection type. (dev_id=%u; connection_type=%d)\n", devId, connection_type);
                return DRV_ERROR_INVALID_DEVICE;
        }
    } else {
        return trs_sq_cq_query(devId, info);
    }
}

static bool trs_is_remote_sq_cq_config(uint32_t devId, struct halSqCqConfigInfo *info)
{
    if (trs_is_remote_sqcq_ops(info->type, info->value[SQCQ_CONFIG_INFO_FLAG])) {
        if (info->prop == DRV_SQCQ_PROP_SQCQ_RESET) {
            return true;
        }
        if ((info->sqId == UINT_MAX) && (info->cqId == UINT_MAX) &&
            ((info->prop == DRV_SQCQ_PROP_SQ_PAUSE) || (info->prop == DRV_SQCQ_PROP_SQ_RESUME))) {
            return true;
        }
        if (trs_get_sq_info(devId, info->tsId, info->type, info->sqId) == NULL) {
            return true;
        }
    }
    return false;
}

static bool trs_is_support_sq_cq_ops(struct halSqCqConfigInfo *info)
{
#ifndef CFG_FEATURE_SQCQ_RESET
    if (info->prop == DRV_SQCQ_PROP_SQCQ_RESET) {
        return false;
    }
#else
    (void)info;
#endif
    return true;
}

static drvError_t trs_sq_cq_urma_config(uint32_t devId, struct halSqCqConfigInfo *info)
{
    int ret = 0;

    if ((info->prop == DRV_SQCQ_PROP_SQCQ_RESET) || (info->prop == DRV_SQCQ_PROP_SQ_PAUSE) ||
        (info->prop == DRV_SQCQ_PROP_SQ_RESUME)) {
        return trs_sq_cq_config(devId, info);
    }

    if ((info->prop == DRV_SQCQ_PROP_SQ_HEAD) || (info->prop == DRV_SQCQ_PROP_SQ_TAIL)) {
        if (info->type == DRV_NORMAL_TYPE) {
            struct sqcq_usr_info *sq_info = trs_get_sq_info(devId, info->tsId, info->type, info->sqId);
            if (sq_info == NULL) {
                trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", devId, info->tsId, info->type,
                        info->sqId);
                return DRV_ERROR_INVALID_VALUE;
            }

            if (trs_is_sq_init_without_sq_mem(sq_info->flag)) { /* no mem sq config tail in host kernel in ub */
                return trs_sq_cq_config(devId, info);
            }
        }

        ret = trs_sq_cq_config(devId, info);
        if (ret != 0) {
            return ret;
        }
    }

    return trs_sq_cq_config_sync(devId, info);
}

drvError_t halSqCqConfig(uint32_t devId, struct halSqCqConfigInfo *info)
{
    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (info->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u)\n", devId, info->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!trs_is_support_sq_cq_ops(info)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (trs_is_remote_sq_cq_config(devId, info)) {
        return trs_sq_cq_config_sync(devId, info);
    }
    info->value[SQCQ_CONFIG_INFO_FLAG] = 0;

    if (info->type == DRV_NORMAL_TYPE) {
        int connection_type = trs_get_connection_type(devId);
        switch (connection_type) {
            case TRS_CONNECT_PROTOCOL_PCIE:
            case TRS_CONNECT_PROTOCOL_HCCS:
            case TRS_CONNECT_PROTOCOL_RC:
                return trs_sq_cq_config(devId, info);
            case TRS_CONNECT_PROTOCOL_UB:
                return trs_sq_cq_urma_config(devId, info);
            default:
                trs_err("Invalid connection type. (dev_id=%u; connection_type=%d)\n", devId, connection_type);
                return DRV_ERROR_INVALID_DEVICE;
        }
    } else {
        return trs_sq_cq_config(devId, info);
    }
}

drvError_t halSqTaskArgsAsyncCopy(uint32_t devId, struct halSqTaskArgsInfo *info)
{
    struct sqcq_usr_info *sq_info = NULL;
    drvError_t ret;

    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (info->tsId >= TRS_TS_NUM) || (info->src == 0) || (info->dst == 0) ||
        (info->size == 0) || (info->type != DRV_NORMAL_TYPE)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; src=0x%llx; dst=0x%llx; size=%u; type=%d)\n", devId, info->tsId,
                info->src, info->dst, info->size, info->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (trs_get_connection_type(devId) != TRS_CONNECT_PROTOCOL_UB) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    sq_info = trs_get_sq_info(devId, info->tsId, info->type, info->sqId);
    if (sq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; sq_id=%u)\n", devId, info->tsId, info->sqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = trs_sq_task_args_async_copy(devId, info, sq_info);
    if (ret != 0) {
        trs_warn_limit("Copy sq args warn. (devid=%u; sqid=%u)\n", devId, info->sqId);
        return ret;
    }
    return DRV_ERROR_NONE;
}

static drvError_t trs_async_para_check(uint32_t dev_id, struct halAsyncDmaInputPara *in,
                                       struct halAsyncDmaOutputPara *out)
{
    if ((in == NULL) || (out == NULL)) {
        trs_err("Invalid para.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((dev_id >= TRS_DEV_NUM) || (in->tsId >= TRS_TS_NUM) || (in->src == NULL) || (in->len == 0) ||
        (in->type != DRV_NORMAL_TYPE)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; len=%u; type=%d)\n", dev_id, in->tsId, in->len, in->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((in->dir != TRS_ASYNC_HOST_TO_DEVICE) && (in->dir != TRS_ASYNC_DEVICE_TO_HOST) &&
        (in->dir != TRS_ASYNC_DEVICE_TO_DEVICE)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((in->async_dma_type >= DRV_ASYNC_DMA_TYPE_MAX) ||
        ((in->async_dma_type != DRV_ASYNC_DMA_TYPE_NORMAL) && (in->async_dma_type != DRV_ASYNC_DMA_TYPE_SQE_UPDATE))) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

bool trs_feature_is_support(uint32_t dev_id, drvFeature_t type)
{
    if ((dev_id >= TRS_DEV_NUM) || (type >= FEATURE_MAX)) {
        trs_run_info("Para check completed. (dev_id=%u; type=%u; dev_max=%u; feature_max=%u)\n", dev_id, (uint32_t)type,
                     (uint32_t)TRS_DEV_NUM, (uint32_t)FEATURE_MAX);
        return false;
    }

    if (!trs_feature_is_inited(dev_id)) {
        if (trs_dev_feature_init_locked(dev_id) != 0) {
            trs_run_info("Trs feature uninit record. (dev_id=%u)\n", dev_id);
            return false;
        }
    }

    return _trs_feature_is_support(dev_id, type);
}

drvError_t halAsyncDmaCreate(uint32_t devId, struct halAsyncDmaInputPara *in, struct halAsyncDmaOutputPara *out)
{
    struct trs_async_dma_input_para input_para = {0};
    int connection_type;
    drvError_t ret;

    ret = trs_async_para_check(devId, in, out);
    if (ret != 0) {
        return ret;
    }

    connection_type = trs_get_connection_type(devId);
    switch (connection_type) {
        case TRS_CONNECT_PROTOCOL_PCIE:
            return trs_async_dma_desc_create(devId, in, out);
        case TRS_CONNECT_PROTOCOL_UB:
            input_para = trs_pack_async_dma_input_para(in, (enum trs_async_dma_type)in->async_dma_type, in->type,
                                                       in->tsId, in->sqId);
            return trs_async_dma_wqe_create(devId, &input_para, out);
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }
}

drvError_t halAsyncDmaWqeCreate(uint32_t devId, struct halAsyncDmaInputPara *in, struct halAsyncDmaOutputPara *out)
{
    return halAsyncDmaCreate(devId, in, out);
}

drvError_t halAsyncDmaDestory(uint32_t devId, struct halAsyncDmaDestoryPara *para)
{
    struct trs_async_dma_destroy_para destroy_para = {0};
    int connection_type;

    if (para == NULL) {
        trs_err("Invalid para.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (para->tsId >= TRS_TS_NUM) || (para->type != DRV_NORMAL_TYPE)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d)\n", devId, para->tsId, para->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!trs_is_dev_open(devId)) {
        trs_err("Device is not open. (dev_id=%u)\n", devId);
        return DRV_ERROR_INVALID_DEVICE;
    }

    connection_type = trs_get_connection_type(devId);
    switch (connection_type) {
        case TRS_CONNECT_PROTOCOL_PCIE:
            return trs_async_dma_destory(devId, para);
        case TRS_CONNECT_PROTOCOL_UB:
            destroy_para = trs_pack_async_dma_destroy_para(para, TRS_ASYNC_DMA_TYPE_NORMAL, para->type, para->tsId,
                                                           para->sqId);
            return trs_async_dma_wqe_destory(devId, &destroy_para);
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }
}

drvError_t halAsyncDmaWqeDestory(uint32_t devId, struct halAsyncDmaDestoryPara *para)
{
    return halAsyncDmaDestory(devId, para);
}

static int _trs_async_2d_para_check(struct drv2dWqeInputPara *para)
{
    if ((para->src == NULL) || (para->dst == NULL) || (para->width == 0) || (para->height == 0) ||
        (para->dpitch < para->width) || (para->spitch < para->width) || (para->height > TRS_2D_HEIGHT_MAX)) {
        trs_err("Invalid para. (src_null=%d; dst_null=%d; width=%llu; height=%llu; dpitch=%llu; spitch=%llu)\n",
                (para->src == NULL), (para->dst == NULL), para->width, para->height, para->dpitch, para->spitch);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->fixedSize >= (para->height * para->width)) {
        trs_err("Invalid fixedSize. (fixedSize=0x%llx; height=%llu; width=%llu)\n", para->fixedSize, para->height,
                para->width);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((para->height != 1) && (((uintptr_t)((char *)para->src + para->spitch) < (uintptr_t)para->src) ||
                                ((uintptr_t)((char *)para->dst + para->dpitch) < (uintptr_t)para->dst))) {
        trs_err("Invalid para. (src=%p; dst=%p; spitch=%llu; dpitch=%llu)\n", para->src, para->dst, para->spitch,
                para->dpitch);
        return DRV_ERROR_INVALID_VALUE;
    }

    return 0;
}

static int trs_async_2d_para_check(uint32_t devId, struct halAsyncDmaInput2DPara *in, struct halAsyncDmaOutputPara *out)
{
    struct drv2dWqeInputPara matrix_2d_para = {0};

    if ((in == NULL) || (out == NULL)) {
        trs_err("Invalid para.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (in->tsId >= TRS_TS_NUM) || (in->type != DRV_NORMAL_TYPE)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d)\n", devId, in->tsId, in->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((in->dir != TRS_ASYNC_HOST_TO_DEVICE) && (in->dir != TRS_ASYNC_DEVICE_TO_HOST) &&
        (in->dir != TRS_ASYNC_DEVICE_TO_DEVICE)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    matrix_2d_para.dst = in->dst;
    matrix_2d_para.dpitch = in->dpitch;
    matrix_2d_para.src = in->src;
    matrix_2d_para.spitch = in->spitch;
    matrix_2d_para.width = in->width;
    matrix_2d_para.height = in->height;
    matrix_2d_para.fixedSize = in->fixedSize;
    return _trs_async_2d_para_check(&matrix_2d_para);
}

drvError_t halAsyncDmaCreate2D(uint32_t devId, struct halAsyncDmaInput2DPara *in, struct halAsyncDmaOutputPara *out)
{
    struct trs_async_dma_input_para input_para = {0};
    int connection_type;
    drvError_t ret = 0;

    ret = trs_async_2d_para_check(devId, in, out);
    if (ret != 0) {
        return ret;
    }

    connection_type = trs_get_connection_type(devId);
    switch (connection_type) {
        case TRS_CONNECT_PROTOCOL_PCIE:
            return DRV_ERROR_NOT_SUPPORT;
        case TRS_CONNECT_PROTOCOL_UB:
            input_para = trs_pack_async_dma_input_para(in, TRS_ASYNC_DMA_TYPE_2D, in->type, in->tsId, in->sqId);
            return trs_async_dma_wqe_create(devId, &input_para, out);
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return ret;
}

drvError_t halAsyncDmaDestroy2D(uint32_t devId, struct halAsyncDmaDestroy2DPara *para)
{
    struct trs_async_dma_destroy_para destroy_para = {0};
    int connection_type;

    if (para == NULL) {
        trs_err("Invalid para.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (para->tsId >= TRS_TS_NUM) || (para->type != DRV_NORMAL_TYPE)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d)\n", devId, para->tsId, para->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    connection_type = trs_get_connection_type(devId);
    switch (connection_type) {
        case TRS_CONNECT_PROTOCOL_PCIE:
            return DRV_ERROR_NOT_SUPPORT;
        case TRS_CONNECT_PROTOCOL_UB:
            destroy_para = trs_pack_async_dma_destroy_para(para, TRS_ASYNC_DMA_TYPE_2D, para->type, para->tsId,
                                                           para->sqId);
            return trs_async_dma_wqe_destory(devId, &destroy_para);
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }
}

static int _trs_async_batch_para_check(unsigned long long *src, unsigned long long *dst, unsigned long long *len,
                                       unsigned long long count)
{
    if ((src == NULL) || (dst == NULL) || (len == NULL)) {
        trs_err("Invalid para. (src_is_null=%d; dst_is_null=%d; size_is_null=%d)\n", (src == NULL), (dst == NULL),
                (len == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((count == 0) || (count > TRS_CPY_BATCH_MAX_COUNT)) {
        trs_err("Invalid count. (count=%llu)\n", count);
        return DRV_ERROR_INVALID_VALUE;
    }

    return 0;
}

static int trs_async_batch_para_check(uint32_t devId, struct halAsyncDmaInputBatchPara *in,
                                      struct halAsyncDmaOutputPara *out)
{
    if ((in == NULL) || (out == NULL)) {
        trs_err("Invalid para.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (in->tsId >= TRS_TS_NUM) || (in->type != DRV_NORMAL_TYPE)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d)\n", devId, in->tsId, in->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((in->dir != TRS_ASYNC_HOST_TO_DEVICE) && (in->dir != TRS_ASYNC_DEVICE_TO_HOST)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return _trs_async_batch_para_check(in->src, in->dst, in->len, in->count);
}

drvError_t halAsyncDmaCreateBatch(uint32_t devId, struct halAsyncDmaInputBatchPara *in,
                                  struct halAsyncDmaOutputPara *out)
{
    struct trs_async_dma_input_para input_para = {0};
    int connection_type;
    drvError_t ret = 0;

    ret = trs_async_batch_para_check(devId, in, out);
    if (ret != 0) {
        return ret;
    }

    connection_type = trs_get_connection_type(devId);
    switch (connection_type) {
        case TRS_CONNECT_PROTOCOL_PCIE:
            return DRV_ERROR_NOT_SUPPORT;
        case TRS_CONNECT_PROTOCOL_UB:
            input_para = trs_pack_async_dma_input_para(in, TRS_ASYNC_DMA_TYPE_BATCH, in->type, in->tsId, in->sqId);
            return trs_async_dma_wqe_create(devId, &input_para, out);
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return ret;
}

drvError_t halAsyncDmaDestroyBatch(uint32_t devId, struct halAsyncDmaDestroyBatchPara *para)
{
    struct trs_async_dma_destroy_para destroy_para = {0};
    int connection_type;

    if (para == NULL) {
        trs_err("Invalid para.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (para->tsId >= TRS_TS_NUM) || (para->type != DRV_NORMAL_TYPE)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d)\n", devId, para->tsId, para->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    connection_type = trs_get_connection_type(devId);
    switch (connection_type) {
        case TRS_CONNECT_PROTOCOL_PCIE:
            return DRV_ERROR_NOT_SUPPORT;
        case TRS_CONNECT_PROTOCOL_UB:
            destroy_para = trs_pack_async_dma_destroy_para(para, TRS_ASYNC_DMA_TYPE_BATCH, para->type, para->tsId,
                                                           para->sqId);
            return trs_async_dma_wqe_destory(devId, &destroy_para);
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }
}

static inline int trs_interface_para_rsv_check(uint32_t *rsv, int rsv_cnt)
{
    int i = rsv_cnt;
    for (i = 0; i < rsv_cnt; i++) {
        if (rsv[i] != 0) {
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    return 0;
}

static int trs_async_jetty_create_para_check(uint32_t devId, struct halAsyncDmaJettyCreateIn *in,
                                             struct halAsyncDmaJettyCreateOut *out)
{
    if ((in == NULL) || (out == NULL)) {
        trs_err("Invalid null para.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (in->jettyType >= DRV_ASYNC_DMA_JETTY_TYPE_MAX) ||
        (in->piMode >= DRV_ASYNC_DMA_JETTY_PI_MODE_MAX) || (in->depth == 0)) {
        trs_err("Invalid para. (dev_id=%u; jetty_type=%d; pi_mode=%d; dir=%d; depth=%u)\n", devId, in->jettyType,
                in->piMode, in->dir, in->depth);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((in->dir < TRS_ASYNC_JETTY_HOST_DEVICE) || (in->dir >= TRS_ASYNC_JETTY_MAX_DIR)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((trs_interface_para_rsv_check(in->rsv, TRS_ASYNC_DMA_JETTY_INPUT_RSV_LEN) != 0) ||
        (trs_interface_para_rsv_check(out->rsv, TRS_ASYNC_DMA_JETTY_OUTPUT_RSV_LEN) != 0)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

drvError_t halAsyncDmaJettyCreate(uint32_t devId, struct halAsyncDmaJettyCreateIn *in,
                                  struct halAsyncDmaJettyCreateOut *out)
{
    int connection_type;
    drvError_t ret = 0;

    ret = trs_async_jetty_create_para_check(devId, in, out);
    if (ret != 0) {
        return ret;
    }

    connection_type = trs_get_connection_type(devId);
    if (connection_type != TRS_CONNECT_PROTOCOL_UB) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return trs_async_dma_jetty_create(devId, in, out);
}

static inline int trs_async_jetty_destroy_para_check(uint32_t devId, struct halAsyncJettyDestroyPara *para)
{
    if ((para == NULL) || (para->jettyHandle == NULL)) {
        trs_err("Invalid null para or jetty handle.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (devId >= TRS_DEV_NUM) {
        trs_err("Invalid para. (dev_id=%u)\n", devId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (trs_interface_para_rsv_check(para->rsv, TRS_ASYNC_DMA_JETTY_DESTROY_RSV_LEN) != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

drvError_t halAsyncDmaJettyDestroy(uint32_t devId, struct halAsyncJettyDestroyPara *para)
{
    int connection_type;
    drvError_t ret = 0;

    ret = trs_async_jetty_destroy_para_check(devId, para);
    if (ret != 0) {
        return ret;
    }

    connection_type = trs_get_connection_type(devId);
    if (connection_type != TRS_CONNECT_PROTOCOL_UB) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return trs_async_dma_jetty_destroy(devId, para);
}

drvError_t halAsyncDmaJettyQuery(uint32_t devId, struct halAsyncDmaJettyQueryIn *in,
                                 struct halAsyncDmaJettyQueryOut *out)
{
    int connection_type;

    if ((in == NULL) || (out == NULL) || (in->jettyHandle == NULL)) {
        trs_err("Invalid null para or jetty handle.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (devId >= TRS_DEV_NUM) {
        trs_err("Invalid devId. (dev_id=%u)\n", devId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((trs_interface_para_rsv_check(in->rsv, TRS_ASYNC_DMA_JETTY_QUERY_IN_RSV_LEN) != 0) ||
        (trs_interface_para_rsv_check(out->rsv, TRS_ASYNC_DMA_JETTY_QUERY_OUT_RSV_LEN) != 0)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    connection_type = trs_get_connection_type(devId);
    if (connection_type != TRS_CONNECT_PROTOCOL_UB) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return trs_async_dma_jetty_query(devId, in, out);
}

static int trs_async_wqe_convert_para_check(uint32_t devId, struct halAsyncDmaWqeInputPara *in,
                                            struct halAsyncDmaWqeOutputPara *out)
{
    if ((in == NULL) || (out == NULL)) {
        trs_err("Invalid null para.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((in->wqeType == DRV_ASYNC_DMA_TYPE_SQE_UPDATE) || (in->wqeType >= DRV_ASYNC_DMA_TYPE_MAX)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((devId >= TRS_DEV_NUM) || (in->wqeBuffer == NULL) || (in->wqeBufferLen == 0)) {
        trs_err("Invalid para. (dev_id=%u; wqe_buffer_null=%d; wqe_buffer_len=0x%llx)\n", devId,
                (in->wqeBuffer == NULL), in->wqeBufferLen);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((trs_interface_para_rsv_check(in->rsv, TRS_ASYNC_DMA_WQE_CONVERT_IN_RSV_LEN) != 0) ||
        (trs_interface_para_rsv_check(out->rsv, TRS_ASYNC_DMA_WQE_CONVERT_OUT_RSV_LEN) != 0)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    switch (in->wqeType) {
        case DRV_ASYNC_DMA_TYPE_NORMAL:
            if ((in->normal.src == NULL) || (in->normal.dst == NULL) || (in->normal.len == 0) ||
                (in->normal.asyncDmaType != DRV_ASYNC_DMA_TYPE_NORMAL)) {
                trs_err("Invalid para. (src_is_null=%d; dst_is_null=%d; len=0x%llx)\n", (in->normal.src == NULL),
                        (in->normal.dst == NULL), in->normal.len);
                return DRV_ERROR_INVALID_VALUE;
            }
            break;
        case DRV_ASYNC_DMA_TYPE_2D:
            return _trs_async_2d_para_check(&in->matrix2d);
        case DRV_ASYNC_DMA_TYPE_BATCH:
            return _trs_async_batch_para_check(in->batch.src, in->batch.dst, in->batch.len, in->batch.count);
        case DRV_ASYNC_DMA_TYPE_NOP:
            if (in->nop.nopCnt == 0) {
                trs_err("Invalid para. (nop_cnt=%llu)\n", in->nop.nopCnt);
                return DRV_ERROR_INVALID_VALUE;
            }
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

drvError_t halAsyncDmaWqeConvert(uint32_t devId, struct halAsyncDmaWqeInputPara *in,
                                 struct halAsyncDmaWqeOutputPara *out)
{
    int connection_type;
    drvError_t ret = 0;

    ret = trs_async_wqe_convert_para_check(devId, in, out);
    if (ret != 0) {
        return ret;
    }

    connection_type = trs_get_connection_type(devId);
    if (connection_type != TRS_CONNECT_PROTOCOL_UB) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return trs_async_dma_wqe_convert(devId, in, out);
}

drvError_t halAsyncDmaJettyWqeFill(uint32_t devId, struct halAsyncDmaJettyFillInfo *para)
{
    int connection_type;

    if ((para == NULL) || (para->jettyHandle == NULL) || (para->srcWqe == NULL) || (para->size == 0)) {
        trs_err("Invalid null para or jetty handle, or invalid null srcWqe, zero size.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (devId >= TRS_DEV_NUM) {
        trs_err("Invalid para. (dev_id=%u)\n", devId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((trs_interface_para_rsv_check(para->rsv, TRS_ASYNC_DMA_JETTY_WQE_FILL_RSV_LEN) != 0)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    connection_type = trs_get_connection_type(devId);
    if (connection_type != TRS_CONNECT_PROTOCOL_UB) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return trs_async_dma_jetty_fill(devId, para);
}

drvError_t halSqTaskSend(uint32_t devId, struct halTaskSendInfo *info)
{
    struct sqcq_usr_info *sq_info = NULL;
    int ret = 0;

    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (info->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u)\n", devId, info->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }

    sq_info = trs_get_sq_info(devId, info->tsId, info->type, info->sqId);
    if (sq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", devId, info->tsId, info->type, info->sqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((!trs_is_sq_support_send(sq_info)) || (trs_is_sq_init_without_sq_mem(sq_info->flag))) {
        trs_warn("Sq is only id type, not support task send. (dev_id=%u; ts_id=%u; sq_id=%u; flag=%u)\n", devId,
                 info->tsId, info->sqId, sq_info->flag);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (info->type == DRV_NORMAL_TYPE) {
        int connection_type = trs_get_connection_type(devId);
        switch (connection_type) {
            case TRS_CONNECT_PROTOCOL_PCIE:
            case TRS_CONNECT_PROTOCOL_HCCS:
            case TRS_CONNECT_PROTOCOL_RC:
                ret = trs_sq_task_send(devId, info, sq_info);
                break;
            case TRS_CONNECT_PROTOCOL_UB:
                ret = trs_sq_task_send_urma(devId, info, sq_info);
                break;
            default:
                trs_err("Invalid connection type. (dev_id=%u; connection_type=%d)\n", devId, connection_type);
                return DRV_ERROR_INVALID_DEVICE;
        }
    } else {
        ret = trs_sq_task_send(devId, info, sq_info);
    }

    return ret;
}

drvError_t halGetTsegInfoByVa(uint32_t devid, uint64_t va, uint64_t size, uint32_t flag, struct halTsegInfo *tsegInfo)
{
    int connection_type;

    if (devid >= TRS_DEV_NUM) {
        trs_err("Invalid para. (dev_id=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (tsegInfo == NULL) {
        trs_err("Invalid NULL tsegInfo. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    connection_type = trs_get_connection_type(devid);
    switch (connection_type) {
        case TRS_CONNECT_PROTOCOL_PCIE:
            return DRV_ERROR_NOT_SUPPORT;
        case TRS_CONNECT_PROTOCOL_UB:
            return trs_get_urma_tseg_info_by_va(devid, va, size, flag, tsegInfo);
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

drvError_t halPutTsegInfo(uint32_t devid, struct halTsegInfo *tsegInfo)
{
    int connection_type;

    if (devid >= TRS_DEV_NUM) {
        trs_err("Invalid para. (dev_id=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (tsegInfo == NULL) {
        trs_err("Invalid NULL tsegInfo. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    connection_type = trs_get_connection_type(devid);
    switch (connection_type) {
        case TRS_CONNECT_PROTOCOL_PCIE:
            return DRV_ERROR_NOT_SUPPORT;
        case TRS_CONNECT_PROTOCOL_UB:
            return trs_put_urma_tseg_info(devid, tsegInfo);
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}
