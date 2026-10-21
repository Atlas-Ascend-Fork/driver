/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TRS_DEV_DRV_H__
#define TRS_DEV_DRV_H__

#include <stddef.h>
#include <pthread.h>
#include <stdbool.h>

#include "ascend_hal_define.h"
#include "drv_type.h"

#include "trs_user_pub_def.h"
#include "trs_user_interface.h"
#include "trs_feature_ioctl.h"
#include "trs_sqcq.h"

#define SVM_DEV_NAME "/dev/svm0"
#define SVM_IOCTL_PROCESS_BIND 0xffff

#define TRS_DEVICE_NUM_PER_BOARD 8U
typedef enum tag_trs_d2d_index {
    TRS_D2D_IN_BOARD_INDEX = 0U,
    TRS_D2D_CROSS_BOARD_INDEX = 1U,
    TRS_MAX_D2D_INDEX
} trs_d2d_index_t;

struct trs_ts_ctx {
    uint32_t normal_cq;
    struct trs_sq_map shm_sq;
    pthread_mutex_t trs_sqcq_mutex;
};

struct trs_dev_ctx {
    uint32_t ts_num;
    int hw_type;
    int connection_type;
    int die_id;
    trs_mode_type_t sq_send_mode;
    bool dev_open;
    void *d2d_dev_ctx[TRS_MAX_D2D_INDEX];
    pthread_mutex_t dev_mutex;
    pthread_mutex_t stream_mutex;
    pthread_mutex_t urma_mutex;
    uint64_t ubdma_cqe_addr;
    struct trs_ts_ctx ts_ctx[TRS_TS_NUM];
    trs_feat_bitmap_t feature_bitmap;
    bool feature_inited;
};

struct trs_dev_ctx *trs_get_dev_ctx(uint32_t dev_id);

struct trs_svm_process_info {
    pid_t vpid;
    unsigned long long ttbr;
    unsigned long long tcr;
    int pasid;
    unsigned int flags;
};

struct trs_dev_init_ops {
    int (*dev_init)(uint32_t dev_id);
    void (*dev_uninit)(uint32_t dev_id);
};

int trs_svm_proc_bind(void);
int trs_register_dev_init_ops(struct trs_dev_init_ops *ops);
void *trs_dev_mmap(uint32_t dev_id, void *addr, size_t length, int prot, int flags);
int trs_dev_munmap(void *addr, size_t length);
drvError_t trs_urma_proc_ctx_init_by_devid(uint32_t dev_id);
void trs_urma_proc_ctx_uninit_by_devid(uint32_t dev_id);
int trs_dev_io_ctrl(uint32_t dev_id, uint32_t cmd, void *para);
bool trs_feature_is_inited(uint32_t dev_id);
bool _trs_feature_is_support(uint32_t dev_id, drvFeature_t type);
int trs_dev_feature_init_locked(uint32_t dev_id);

int trs_get_connection_type(uint32_t dev_id);
uint32_t trs_get_ts_num(uint32_t dev_id);
int trs_get_sq_send_mode(uint32_t dev_id);
int trs_get_stars_die_id(uint32_t dev_id);
void trs_set_dev_open_status(uint32_t dev_id, bool dev_open);
bool trs_is_dev_open(uint32_t dev_id);
void trs_dev_ctx_mutex_lock(uint32_t dev_id);
void trs_dev_ctx_mutex_un_lock(uint32_t dev_id);
void trs_dev_ctx_stream_mutex_lock(uint32_t dev_id);
void trs_dev_ctx_stream_mutex_unlock(uint32_t dev_id);
void trs_dev_ctx_urma_mutex_lock(uint32_t dev_id);
void trs_dev_ctx_urma_mutex_unlock(uint32_t dev_id);
void *trs_get_d2d_dev_ctx(uint32_t recv_dev_id, trs_d2d_index_t d2d_index);
void trs_set_d2d_dev_ctx(uint32_t recv_dev_id, trs_d2d_index_t d2d_index, void *ctx);
void trs_set_ubdma_cqe_addr(uint32_t dev_id, uint64_t ubdma_cqe_addr);
uint64_t trs_get_ubdma_cqe_addr(uint32_t dev_id);

bool trs_is_stars_inst(uint32_t dev_id, uint32_t ts_id);
int trs_hw_info_init(uint32_t dev_id);

void trs_dev_mutex_lock(void);
void trs_dev_mutex_unlock(void);
#endif
