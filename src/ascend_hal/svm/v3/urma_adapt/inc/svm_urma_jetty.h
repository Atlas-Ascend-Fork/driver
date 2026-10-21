/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SVM_URMA_JETTY_H
#define SVM_URMA_JETTY_H

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>

#include "ascend_hal_error.h"

#include "svm_urma_def.h"
#include "svm_pub.h"

#define SVM_URMA_JETTY_WRITE_READ_MAX_SIZE 0x10000000ULL /* 256MB */

enum svm_urma_jetty_state {
    SVM_URMA_JETTY_BUSY = 0U,
    SVM_URMA_JETTY_IDLE,
    SVM_URMA_JETTY_UNINITED,
};

struct svm_urma_jetty {
    u32 id;
    enum svm_urma_jetty_state state;

    u32 depth;

    urma_jfce_t *jfce;
    urma_jfc_t *jfc;

    urma_jfs_t *jfs;
    urma_jfr_t *jfr;

    urma_sge_t *src_sges;
    urma_sge_t *dst_sges;
    urma_jfs_wr_t *jfs_wrs;
    urma_cr_t *crs;

    u64 post_wr_id;
    u64 ack_wr_id;

    void *user_ctx; /* per-channel opaque context owned by the upper layer; NULL when idle */
};

struct svm_urma_jetty_pool {
    pthread_rwlock_t rwlock;

    sem_t sem;
    u32 devid;
    u32 jetty_num;
    struct svm_urma_jetty **jettys;

    u32 next_id;
};

struct svm_urma_jetty_pool_cfg {
    u32 devid;
    urma_context_t *urma_ctx;
    urma_token_t token_val;
    u32 jetty_num;
    u32 depth_per_jetty;
};

struct svm_urma_jetty_post_para {
    u64 src;
    u64 dst;
    u64 size;
    urma_opcode_t opcode;
    urma_target_seg_t *src_tseg;
    urma_target_seg_t *dst_tseg;

    urma_target_jetty_t *tjfr;
};

static inline void svm_urma_jetty_pool_cfg_pack(u32 devid, urma_context_t *urma_ctx, urma_token_t token_val,
                                                u32 jetty_num, u32 depth_per_jetty, struct svm_urma_jetty_pool_cfg *cfg)
{
    cfg->devid = devid;
    cfg->urma_ctx = urma_ctx;
    cfg->token_val = token_val;
    cfg->jetty_num = jetty_num;
    cfg->depth_per_jetty = depth_per_jetty;
}

int svm_urma_jetty_pool_init(struct svm_urma_jetty_pool *pool, struct svm_urma_jetty_pool_cfg *cfg);
void svm_urma_jetty_pool_uninit(struct svm_urma_jetty_pool *pool);

int svm_urma_jetty_alloc(struct svm_urma_jetty_pool *pool, struct svm_urma_jetty **out_jetty, int64_t time_out_s);
void svm_urma_jetty_free(struct svm_urma_jetty_pool *pool, struct svm_urma_jetty *jetty);

int svm_urma_jetty_post(struct svm_urma_jetty *jetty, struct svm_urma_jetty_post_para *para, u32 *out_wr_num);
int svm_urma_jetty_wait(struct svm_urma_jetty *jetty, int wr_num, int timeout_ms);

#endif
