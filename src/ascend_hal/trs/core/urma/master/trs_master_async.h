/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef TRS_MASTER_ASYNC_H
#define TRS_MASTER_ASYNC_H
#include "ascend_hal_define.h"
#include "trs_sqcq.h"
#include "trs_urma_async.h"

struct trs_async_devid {
    uint32_t src_devid;
    uint32_t dst_devid;
    uint32_t local_devid;
    uint32_t remote_devid;
    trs_d2d_index_t d2d_index;
};

static inline void trs_pack_async_devid(uint32_t local_devid, uint32_t src_devid, uint32_t dst_devid,
                                        trs_d2d_index_t d2d_index, struct trs_async_devid *async_devid)
{
    async_devid->local_devid = local_devid;
    async_devid->remote_devid = (src_devid == local_devid) ? dst_devid : src_devid;
    async_devid->src_devid = src_devid;
    async_devid->dst_devid = dst_devid;
    async_devid->d2d_index = d2d_index;
}

drvError_t trs_async_dma_wqe_create(uint32_t dev_id, struct trs_async_dma_input_para *in,
                                    struct halAsyncDmaOutputPara *out);
drvError_t trs_async_dma_wqe_destory(uint32_t dev_id, struct trs_async_dma_destroy_para *para);
drvError_t trs_async_dma_wqe_convert(uint32_t dev_id, struct halAsyncDmaWqeInputPara *in,
                                     struct halAsyncDmaWqeOutputPara *out);
drvError_t trs_async_dma_jetty_create(uint32_t dev_id, struct halAsyncDmaJettyCreateIn *in,
                                      struct halAsyncDmaJettyCreateOut *out);
drvError_t trs_async_dma_jetty_destroy(uint32_t dev_id, struct halAsyncJettyDestroyPara *para);
drvError_t trs_async_dma_jetty_query(uint32_t dev_id, struct halAsyncDmaJettyQueryIn *in,
                                     struct halAsyncDmaJettyQueryOut *out);
drvError_t trs_async_dma_jetty_fill(uint32_t dev_id, struct halAsyncDmaJettyFillInfo *para);
int trs_destroy_remote_d2d_jetty(uint32_t dev_id);
int trs_async_uninit_async_ctx(uint32_t dev_id, uint32_t sq_id, void *master_ctx);
drvError_t trs_sq_jetty_info_query(uint32_t dev_id, struct halSqCqQueryInfo *info);
#endif
