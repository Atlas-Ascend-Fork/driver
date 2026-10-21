/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef TRS_URMA_ASYNC_H
#define TRS_URMA_ASYNC_H

#include "ascend_hal_define.h"

#define TRS_2D_HEIGHT_MAX (5ULL * 1024 * 1024)
#define TRS_CPY_BATCH_MAX_COUNT 2048ULL

struct trs_async_addr_pack {
    uint64_t src;
    uint64_t dst;
    uint64_t len;
};

enum trs_async_dma_type {
    TRS_ASYNC_DMA_TYPE_NORMAL = 0,
    TRS_ASYNC_DMA_TYPE_SQE_UPDATE,
    TRS_ASYNC_DMA_TYPE_2D,
    TRS_ASYNC_DMA_TYPE_BATCH,
    TRS_ASYNC_DMA_TYPE_NOP,
    TRS_ASYNC_DMA_TYPE_MAX
};

struct trs_async_dma_input_para {
    enum trs_async_dma_type async_dma_type;
    drvSqCqType_t type;
    uint32_t tsId;
    uint32_t sqId;
    uint32_t dir;
    union {
        struct drvNormalWqeInputPara async_normal_in;
        struct drv2dWqeInputPara async_2d_in;
        struct drvBatchWqeInputPara async_batch_in;
        struct drvNopWqeInputPara async_nop_in;
    };
};

struct trs_async_dma_destroy_para {
    enum trs_async_dma_type async_dma_type;
    drvSqCqType_t type;
    uint32_t tsId;
    uint32_t sqId;
    union {
        struct halAsyncDmaDestoryPara *normal_para;
        struct halAsyncDmaDestroy2DPara *async_2d_para;
        struct halAsyncDmaDestroyBatchPara *async_batch_para;
    };
};

static inline struct trs_async_dma_input_para trs_pack_async_dma_input_para(void *input_para,
                                                                            enum trs_async_dma_type async_dma_type,
                                                                            drvSqCqType_t type, uint32_t tsId,
                                                                            uint32_t sqId)
{
    struct trs_async_dma_input_para async_input = {0};
    async_input.async_dma_type = async_dma_type;
    async_input.type = type;
    async_input.tsId = tsId;
    async_input.sqId = sqId;

    if ((async_dma_type == TRS_ASYNC_DMA_TYPE_NORMAL) || (async_dma_type == TRS_ASYNC_DMA_TYPE_SQE_UPDATE)) {
        struct halAsyncDmaInputPara *in = (struct halAsyncDmaInputPara *)input_para;
        async_input.async_normal_in.asyncDmaType = in->async_dma_type;
        async_input.async_normal_in.len = in->len;
        async_input.async_normal_in.src = in->src;
        async_input.async_normal_in.dst = in->dst;
    } else if (async_dma_type == TRS_ASYNC_DMA_TYPE_2D) {
        struct halAsyncDmaInput2DPara *in = (struct halAsyncDmaInput2DPara *)input_para;
        async_input.async_2d_in.dst = in->dst;
        async_input.async_2d_in.dpitch = in->dpitch;
        async_input.async_2d_in.src = in->src;
        async_input.async_2d_in.spitch = in->spitch;
        async_input.async_2d_in.width = in->width;
        async_input.async_2d_in.height = in->height;
        async_input.async_2d_in.fixedSize = in->fixedSize;
    } else if (async_dma_type == TRS_ASYNC_DMA_TYPE_BATCH) {
        struct halAsyncDmaInputBatchPara *in = (struct halAsyncDmaInputBatchPara *)input_para;
        async_input.async_batch_in.dst = in->dst;
        async_input.async_batch_in.src = in->src;
        async_input.async_batch_in.len = in->len;
        async_input.async_batch_in.count = in->count;
    }

    return async_input;
}

static inline struct trs_async_dma_destroy_para trs_pack_async_dma_destroy_para(void *in,
                                                                                enum trs_async_dma_type async_dma_type,
                                                                                drvSqCqType_t type, uint32_t tsId,
                                                                                uint32_t sqId)
{
    struct trs_async_dma_destroy_para destroy_para = {0};
    destroy_para.async_dma_type = async_dma_type;
    destroy_para.normal_para = in;
    destroy_para.type = type;
    destroy_para.tsId = tsId;
    destroy_para.sqId = sqId;
    return destroy_para;
}

int trs_async_urma_init(uint32_t dev_id);
void trs_async_urma_uninit(uint32_t dev_id);

#endif /* TRS_URMA_ASYNC_H */
