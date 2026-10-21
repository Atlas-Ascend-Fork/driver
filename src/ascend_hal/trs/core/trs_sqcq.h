/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TRS_SQCQ_H__
#define TRS_SQCQ_H__
#include <pthread.h>
#include "ascend_hal_define.h"
#include "trs_ioctl.h"

#define TRS_HW_SQE_SIZE 64
#define TRS_SQCQ_BUFF_LEN 256

#define TRS_SQ_CTRL_BY_TRS_FLAG 0U
#define TRS_SQ_CTRL_BY_USER_FLAG 1U

#define TRS_TASK_SEND_TRACE_ENV "ASCEND_TRS_TASK_SEND_TRACE" /* set env var to enable trace*/
#define TRS_TASK_SEND_TRACE_POINT_NUM 3
#define TRS_TASK_SEND_TRACE_TOTAL_NUM (TRS_TASK_SEND_TRACE_POINT_NUM + 2)

#ifdef CFG_SOC_PLATFORM_ESL_FPGA
#define TRS_TASK_SEND_TIME_LIMIT 5000000U /* ns */
#else
#define TRS_TASK_SEND_TIME_LIMIT 5000U /* ns */
#endif

struct trs_sq_map_addr_info {
    void *addr;
    size_t len;
};

struct trs_sq_map {
    struct trs_sq_map_addr_info que;
    struct trs_sq_map_addr_info ctrl[TRS_UIO_ADDR_MAX];
    struct trs_sq_map_addr_info non_reg_addr;
    struct trs_sq_map_addr_info reg_addr;
};

struct trs_sq_ctrl {
    uint32_t soft_que_flag;
    uint32_t mem_local_flag;
    void *que_addr;
    void *db;
    void *head;
    void *tail;
    void *head_reg;
    void *tail_reg;
    struct trs_sq_shr_info *shr_info;
};

struct sqcq_usr_info {
    uint32_t valid;
    uint32_t head;
    uint32_t tail;
    uint32_t depth;
    uint32_t e_size;
    uint32_t max_num;
    uint32_t cur_num;
    uint32_t pos;
    uint8_t buf[TRS_SQCQ_BUFF_LEN];
    struct trs_sq_ctrl sq_ctrl;
    struct trs_sq_map sq_map;
    void *sq_que_spec_addr;
    void *urma_ctx;
    pthread_mutex_t sq_send_mutex;
    pthread_rwlock_t mutex;
    uint32_t flag;
    uint32_t status;
    uint32_t switch_stream_flag;
    uint32_t stream_id;
};

static inline uint64_t trs_align_up_power2(uint64_t value, uint64_t align)
{
    if (align == 0) {
        return value;
    }

    return (value + (align - 1)) & ~(align - 1);
}

static inline uint64_t trs_align_down_power2(uint64_t value, uint64_t align)
{
    if (align == 0) {
        return value;
    }

    return value & ~(align - 1);
}

static inline uint64_t trs_align_up(uint64_t value, uint64_t align)
{
    if (align == 0) {
        return value;
    }

    return ((value + align - 1) / align) * align;
}

int trs_dev_sq_cq_init(uint32_t dev_id);
void trs_dev_sq_cq_uninit(uint32_t dev_id, uint32_t close_type);
bool trs_is_remote_sqcq_ops(drvSqCqType_t type, uint32_t flag);
struct sqcq_usr_info *trs_get_sq_info(uint32_t dev_id, uint32_t ts_id, drvSqCqType_t type, uint32_t sq_id);
struct sqcq_usr_info *_trs_get_sq_info(uint32_t dev_id, uint32_t ts_id, drvSqCqType_t type, uint32_t id);
drvError_t _halSqCqAllocate(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out);
drvError_t _halSqCqFree(uint32_t dev_id, struct halSqCqFreeInfo *info);
uint32_t trs_get_sq_num(uint32_t dev_id, uint32_t ts_id, drvSqCqType_t type);
drvError_t trs_local_sqcq_alloc(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out);
drvError_t trs_local_sqcq_free(uint32_t dev_id, struct halSqCqFreeInfo *info);
drvError_t trs_sqcq_alloc_para_check(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out);
drvError_t trs_sqcq_free_para_check(uint32_t dev_id, struct halSqCqFreeInfo *info);
drvError_t trs_sqcq_alloc(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out);
drvError_t trs_sqcq_free(uint32_t dev_id, struct halSqCqFreeInfo *info);
drvError_t trs_local_sq_cq_id_free(uint32_t dev_id, struct halSqCqFreeInfo *info);
void trs_sq_cq_info_un_init(uint32_t dev_id, struct halSqCqFreeInfo *info);
drvError_t trs_sq_task_send_check(uint32_t dev_id, struct halTaskSendInfo *info, struct sqcq_usr_info *sq_info);
void trs_sq_task_fill(struct halTaskSendInfo *info, struct sqcq_usr_info *sq_info);
drvError_t trs_sq_task_send(uint32_t dev_id, struct halTaskSendInfo *info, struct sqcq_usr_info *sq_info);
drvError_t trs_sq_switch_stream_batch(uint32_t dev_id, struct sq_switch_stream_info *info, uint32_t num);
drvError_t trs_stream_task_fill(uint32_t dev_id, uint32_t stream_id, void *task_addr, void *task_info, uint32_t size);
drvError_t trs_sq_cq_query(uint32_t dev_id, struct halSqCqQueryInfo *info);
drvError_t trs_sq_cq_config(uint32_t dev_id, struct halSqCqConfigInfo *info);
int trs_set_sq_info_head(uint32_t dev_id, uint32_t ts_id, int type, uint32_t sq_id, uint32_t head);
int trs_set_sq_info_tail(uint32_t dev_id, uint32_t ts_id, int type, uint32_t sq_id, uint32_t tail);
int trs_recycle_sq_cq_with_urma(uint32_t dev_id, uint32_t ts_id, uint32_t sq_id, uint32_t cq_id, bool remote_free_flag);
drvError_t trs_async_dma_desc_create(uint32_t dev_id, struct halAsyncDmaInputPara *in,
                                     struct halAsyncDmaOutputPara *out);
drvError_t trs_async_dma_destory(uint32_t dev_id, struct halAsyncDmaDestoryPara *para);
struct trs_sqcq_remote_ops {
    drvError_t (*sqcq_alloc)(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out);
    drvError_t (*sqcq_free)(uint32_t dev_id, struct halSqCqFreeInfo *in);
};

void trs_register_sqcq_remote_ops(struct trs_sqcq_remote_ops *ops);

struct trs_sqcq_mem_ops {
    int (*mem_alloc)(uint32_t dev_id, uint64_t *va, uint64_t size);
    void (*mem_free)(uint32_t dev_id, uint64_t va, uint64_t size);
};

void trs_register_sqcq_mem_ops(struct trs_sqcq_mem_ops *ops);

static inline bool trs_is_sq_init_without_sq_mem(uint32_t flag)
{
    return ((flag & TSDRV_FLAG_NO_SQ_MEM) != 0);
}

static inline bool trs_is_sq_support_send(struct sqcq_usr_info *info)
{
    return ((info->flag & TSDRV_FLAG_ONLY_SQCQ_ID) == 0);
}

static inline void trs_sq_send_ok_stat(struct sqcq_usr_info *sq_info, uint32_t num)
{
    if (sq_info->sq_ctrl.shr_info != NULL) {
        sq_info->sq_ctrl.shr_info->send_ok += num;
    }
}

static inline uint32_t trs_get_sq_ctrl_flag(struct sqcq_usr_info *sq_info, struct halTaskSendInfo *info)
{
    if (((uintptr_t)info->sqe_addr < (uintptr_t)sq_info->sq_ctrl.que_addr) ||
        ((uintptr_t)info->sqe_addr >=
         ((uintptr_t)sq_info->sq_ctrl.que_addr + (uint64_t)sq_info->depth * sq_info->e_size))) {
        return TRS_SQ_CTRL_BY_TRS_FLAG;
    }

    return TRS_SQ_CTRL_BY_USER_FLAG;
}

static inline bool trs_is_task_trace_env_set(void)
{
#if defined(CFG_BUILD_DEBUG) && defined(CFG_SOC_PLATFORM_CLOUD_V4)
    return (getenv(TRS_TASK_SEND_TRACE_ENV) != NULL);
#else
    return false;
#endif
}

int trs_sq_mmap(uint32_t dev_id, struct halSqCqInputInfo *in, struct trs_sq_map *sq_map);
void trs_sq_munmap(struct trs_sq_map *sq_map);

struct sqcq_usr_info *trs_get_cq_info(uint32_t dev_id, uint32_t ts_id, int type, uint32_t cq_id);
void trs_sq_info_init(uint32_t dev_id, struct halSqCqInputInfo *in, struct trs_sq_map *sq_map);
void trs_cq_info_init(uint32_t dev_id, struct halSqCqInputInfo *in);

#endif
