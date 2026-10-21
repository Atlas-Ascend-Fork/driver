/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <errno.h>
#include <string.h>

#include "comm_user_interface.h"

#include "svm_pub.h"
#include "svm_log.h"
#include "svm_urma_def.h"
#include "svm_user_adapt.h"
#include "svm_criu.h"
#include "svm_urma_jetty.h"

#define SVM_URMA_JFC_CTX_SIZE 128U

static struct svm_urma_jetty *svm_urma_jetty_create(urma_context_t *urma_ctx, urma_token_t token_val, u32 depth, u32 id)
{
    struct svm_urma_jetty *jetty = NULL;
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfs_cfg_t jfs_cfg = {0};
    urma_jfr_cfg_t jfr_cfg = {0};
    urma_status_t urma_ret;

    jetty = svm_ua_calloc(1, sizeof(struct svm_urma_jetty));
    if (jetty == NULL) {
        svm_err("Malloc svm_urma_jetty failed. (size=%llu)\n", (u64)sizeof(struct svm_urma_jetty));
        return NULL;
    }

    jetty->depth = depth;
    jetty->src_sges = svm_ua_calloc(depth, sizeof(urma_sge_t));
    if (jetty->src_sges == NULL) {
        svm_err("Malloc src_sges failed. (depth=%u)\n", depth);
        goto free_jetty;
    }

    jetty->dst_sges = svm_ua_calloc(depth, sizeof(urma_sge_t));
    if (jetty->dst_sges == NULL) {
        svm_err("Malloc dst_sges failed. (depth=%u)\n", depth);
        goto free_src_sges;
    }

    jetty->jfs_wrs = svm_ua_calloc(depth, sizeof(urma_jfs_wr_t));
    if (jetty->jfs_wrs == NULL) {
        svm_err("Malloc urma_jfs_wr_t[] failed. (depth=%u)\n", depth);
        goto free_dst_sges;
    }

    jetty->crs = svm_ua_calloc(depth, sizeof(urma_cr_t));
    if (jetty->crs == NULL) {
        svm_err("Malloc urma_cr_t[] failed. (depth=%u)\n", depth);
        goto free_jfs_wrs;
    }

    jetty->id = id;
    jetty->jfce = urma_create_jfce(urma_ctx);
    if (jetty->jfce == NULL) {
        svm_err("Create jfce failed.\n");
        goto free_crs;
    }

    jfc_cfg.depth = depth;
    jfc_cfg.jfce = jetty->jfce;
    jetty->jfc = urma_create_jfc(urma_ctx, &jfc_cfg);
    if (jetty->jfc == NULL) {
        svm_err("Create jfc failed.\n");
        goto delete_jfce;
    }
    urma_ret = urma_rearm_jfc(jetty->jfc, false);
    if (urma_ret != URMA_SUCCESS) {
        svm_err("Urma rearm jfc failed. (ret=%d)\n", urma_ret);
        goto delete_jfc;
    }

    jfs_cfg.depth = depth;
    jfs_cfg.trans_mode = URMA_TM_RM;
    jfs_cfg.jfc = jetty->jfc;
    jfs_cfg.max_sge = 1;
    jfs_cfg.priority = ASCEND_URMA_PRIORITY_MIDDLE;
    jfs_cfg.max_inline_data = 0;
    jfs_cfg.rnr_retry = URMA_TYPICAL_RNR_RETRY;
    jfs_cfg.err_timeout = URMA_TYPICAL_ERR_TIMEOUT;
    jetty->jfs = urma_create_jfs(urma_ctx, &jfs_cfg);
    if (jetty->jfs == NULL) {
        svm_err("Create jfs failed.\n");
        goto delete_jfc;
    }

    jfr_cfg.depth = depth;
    jfr_cfg.trans_mode = URMA_TM_RM;
    jfr_cfg.jfc = jetty->jfc;
    jfr_cfg.flag.bs.tag_matching = URMA_NO_TAG_MATCHING;
    jfr_cfg.min_rnr_timer = URMA_TYPICAL_MIN_RNR_TIMER;
    jfr_cfg.max_sge = 1;
    jfr_cfg.token_value = token_val;
    jfr_cfg.flag.bs.token_policy = URMA_TOKEN_PLAIN_TEXT;
    jetty->jfr = urma_create_jfr(urma_ctx, &jfr_cfg);
    if (jetty->jfr == NULL) {
        svm_err("Create jfr failed. (depth=%u; max_sge=%u)\n", jfr_cfg.depth, jfr_cfg.max_sge);
        goto delete_jfs;
    }

    jetty->post_wr_id = 0;
    jetty->ack_wr_id = 0;

    jetty->state = SVM_URMA_JETTY_IDLE;
    return jetty;

delete_jfs:
    (void)urma_delete_jfs(jetty->jfs);
delete_jfc:
    (void)urma_delete_jfc(jetty->jfc);
delete_jfce:
    (void)urma_delete_jfce(jetty->jfce);
free_crs:
    free(jetty->crs);
free_jfs_wrs:
    free(jetty->jfs_wrs);
free_dst_sges:
    free(jetty->dst_sges);
free_src_sges:
    free(jetty->src_sges);
free_jetty:
    free(jetty);
    return NULL;
}

static void svm_urma_jetty_destroy(u32 devid, struct svm_urma_jetty *jetty)
{
    jetty->state = SVM_URMA_JETTY_UNINITED;
    if (!svm_criu_is_resetting(devid)) {
        (void)urma_delete_jfr(jetty->jfr);
        (void)urma_delete_jfs(jetty->jfs);
        (void)urma_delete_jfc(jetty->jfc);
        (void)urma_delete_jfce(jetty->jfce);
    }

    free(jetty->crs);
    free(jetty->jfs_wrs);
    free(jetty->dst_sges);
    free(jetty->src_sges);
    free(jetty);
}

int svm_urma_jetty_pool_init(struct svm_urma_jetty_pool *pool, struct svm_urma_jetty_pool_cfg *cfg)
{
    u32 i, j;

    (void)pthread_rwlock_init(&pool->rwlock, NULL);
    (void)sem_init(&pool->sem, 0, cfg->jetty_num);
    pool->devid = cfg->devid;
    pool->jetty_num = cfg->jetty_num;
    pool->jettys = svm_ua_calloc(cfg->jetty_num, sizeof(struct svm_urma_jetty *));
    if (pool->jettys == NULL) {
        svm_err("Malloc svm_urma_jetty* failed. (num=%u)\n", cfg->jetty_num);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    for (i = 0; i < cfg->jetty_num; i++) {
        pool->jettys[i] = svm_urma_jetty_create(cfg->urma_ctx, cfg->token_val, cfg->depth_per_jetty, i);
        if (pool->jettys[i] == NULL) {
            goto jettys_destroy;
        }
    }

    return DRV_ERROR_NONE;
jettys_destroy:
    for (j = 0; j < i; j++) {
        svm_urma_jetty_destroy(pool->devid, pool->jettys[j]);
    }
    free(pool->jettys);
    return DRV_ERROR_INNER_ERR;
}

void svm_urma_jetty_pool_uninit(struct svm_urma_jetty_pool *pool)
{
    u32 i;

    (void)pthread_rwlock_wrlock(&pool->rwlock);
    for (i = 0; i < pool->jetty_num; i++) {
        svm_urma_jetty_destroy(pool->devid, pool->jettys[i]);
    }
    if (pool->jettys != NULL) {
        free(pool->jettys);
        pool->jettys = NULL;
        pool->jetty_num = 0;
    }
    (void)pthread_rwlock_unlock(&pool->rwlock);
    sem_destroy(&pool->sem);
}

static int _svm_urma_jetty_alloc(struct svm_urma_jetty_pool *pool, struct svm_urma_jetty **out_jetty)
{
    u32 i, j;

    (void)pthread_rwlock_wrlock(&pool->rwlock);
    for (i = 0; i < pool->jetty_num; i++) {
        j = (pool->next_id + i) % pool->jetty_num;
        if (pool->jettys[j]->state == SVM_URMA_JETTY_IDLE) {
            pool->jettys[j]->state = SVM_URMA_JETTY_BUSY;
            *out_jetty = pool->jettys[j];
            pool->next_id = (j + 1) % pool->jetty_num;
            (void)pthread_rwlock_unlock(&pool->rwlock);
            return DRV_ERROR_NONE;
        }
    }
    (void)pthread_rwlock_unlock(&pool->rwlock);

    return DRV_ERROR_BUSY;
}

int svm_urma_jetty_alloc(struct svm_urma_jetty_pool *pool, struct svm_urma_jetty **out_jetty, int64_t time_out_s)
{
    struct timespec wait_time;
    struct timeval now;
    int ret;
    int sem_ret;

    if (time_out_s == -1) {
        sem_ret = sem_wait(&pool->sem);
    } else {
        gettimeofday(&now, NULL);
        wait_time.tv_sec = now.tv_sec + time_out_s;
        wait_time.tv_nsec = now.tv_usec * 1000; /* 1000ns/us */
        sem_ret = sem_timedwait(&pool->sem, &wait_time);
    }
    if (sem_ret != 0) {
        svm_err("Sem wait failed. (errno=%d; errstr=%s)\n", errno, strerror(errno));
        return DRV_ERROR_WAIT_TIMEOUT;
    }

    ret = _svm_urma_jetty_alloc(pool, out_jetty);
    if (ret != 0) {
        svm_err("Alloc jetty failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

void svm_urma_jetty_free(struct svm_urma_jetty_pool *pool, struct svm_urma_jetty *jetty)
{
    (void)pthread_rwlock_wrlock(&pool->rwlock);
    jetty->ack_wr_id = 0;
    jetty->post_wr_id = 0;
    jetty->user_ctx = NULL;
    jetty->state = SVM_URMA_JETTY_IDLE;
    (void)pthread_rwlock_unlock(&pool->rwlock);

    (void)sem_post(&pool->sem);
}

static u32 svm_urma_jetty_get_post_wr_id_within_depth(struct svm_urma_jetty *jetty)
{
    return (u32)(jetty->post_wr_id % jetty->depth);
}

/* post_wr_id and ack_wr_id are monotonic counters; their diff tracks posted-but-not-acked WRs
 * and can span the full [0, depth] range.
 * Since it is synchronous copy, post_wr_id and ack_wr_id will not be flipped.
 * If asynchronous scenarios are provided later, adaptation is required */
static u32 svm_urma_jetty_get_jfs_wrs_post_num(struct svm_urma_jetty *jetty)
{
    return (u32)(jetty->post_wr_id - jetty->ack_wr_id);
}

static bool svm_urma_jetty_is_no_enough_idle_wrs(struct svm_urma_jetty *jetty, u32 need_fill_num)
{
    return need_fill_num > (jetty->depth - svm_urma_jetty_get_jfs_wrs_post_num(jetty));
}

static int svm_urma_jetty_fill_jfs_wrs(struct svm_urma_jetty *jetty, struct svm_urma_jetty_post_para *para,
                                       u32 *out_wr_num)
{
    u32 wr_num = (u32)(svm_align_up(para->size, SVM_URMA_JETTY_WRITE_READ_MAX_SIZE) /
                       SVM_URMA_JETTY_WRITE_READ_MAX_SIZE);
    u32 start_wrs_id = svm_urma_jetty_get_post_wr_id_within_depth(jetty);
    u32 i, j, next_wrs_id;

    svm_debug("Svm_fill_jfs_info. (src=0x%llx; dst=0x%llx; post_id=%llu; ack_id=%llu; depth=%u)\n", para->src,
              para->dst, jetty->post_wr_id, jetty->ack_wr_id, jetty->depth);

    if (wr_num > jetty->depth) {
        svm_err("Size out of jetty depth. (depth=%u; size=%llu; wr_num=%u)\n", jetty->depth, para->size, wr_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_urma_jetty_is_no_enough_idle_wrs(jetty, wr_num)) {
        return DRV_ERROR_BUSY;
    }

    for (i = 0, j = start_wrs_id; i < wr_num; i++) {
        jetty->src_sges[j].addr = para->src + i * SVM_URMA_JETTY_WRITE_READ_MAX_SIZE;
        jetty->src_sges[j].len = (u32)svm_min(para->size - i * SVM_URMA_JETTY_WRITE_READ_MAX_SIZE,
                                              SVM_URMA_JETTY_WRITE_READ_MAX_SIZE);
        jetty->src_sges[j].tseg = para->src_tseg;
        jetty->dst_sges[j].addr = para->dst + i * SVM_URMA_JETTY_WRITE_READ_MAX_SIZE;
        jetty->dst_sges[j].len = (u32)svm_min(para->size - i * SVM_URMA_JETTY_WRITE_READ_MAX_SIZE,
                                              SVM_URMA_JETTY_WRITE_READ_MAX_SIZE);
        jetty->dst_sges[j].tseg = para->dst_tseg;

        jetty->jfs_wrs[j].tjetty = para->tjfr;
        jetty->jfs_wrs[j].opcode = para->opcode;
        jetty->jfs_wrs[j].flag.bs.complete_enable = 1;

        jetty->jfs_wrs[j].rw.src.sge = &jetty->src_sges[j];
        jetty->jfs_wrs[j].rw.src.num_sge = 1;
        jetty->jfs_wrs[j].rw.dst.sge = &jetty->dst_sges[j];
        jetty->jfs_wrs[j].rw.dst.num_sge = 1;

        jetty->jfs_wrs[j].flag.bs.place_order = 2; /* 2 to keep memcpy order */
        jetty->jfs_wrs[j].flag.bs.comp_order = 1;

        next_wrs_id = (j + 1) % jetty->depth;
        jetty->jfs_wrs[j].next = ((i + 1) < wr_num) ? &jetty->jfs_wrs[next_wrs_id] : NULL;
        j = next_wrs_id;
    }

    *out_wr_num = wr_num;
    return DRV_ERROR_NONE;
}

int svm_urma_jetty_post(struct svm_urma_jetty *jetty, struct svm_urma_jetty_post_para *para, u32 *out_wr_num)
{
    urma_jfs_wr_t *bad_wr = NULL;
    urma_status_t urma_ret;
    int ret;

    ret = svm_urma_jetty_fill_jfs_wrs(jetty, para, out_wr_num);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    urma_ret = urma_post_jfs_wr(jetty->jfs, &jetty->jfs_wrs[svm_urma_jetty_get_post_wr_id_within_depth(jetty)],
                                &bad_wr);
    if (urma_ret != URMA_SUCCESS) {
        svm_err("Urma post failed. (ret=%d; opcode=%d)\n", urma_ret, para->opcode);
        return DRV_ERROR_INNER_ERR;
    }

    jetty->post_wr_id += *out_wr_num;

    return DRV_ERROR_NONE;
}

static void svm_urma_get_jfc_opt(urma_jfc_t *jfc)
{
    char jetty_buf[SVM_URMA_JFC_CTX_SIZE] = {0};
    int ret;

    ret = urma_get_jfc_opt(jfc, URMA_JFC_FULL_CTX, jetty_buf, SVM_URMA_JFC_CTX_SIZE);
    if (ret == 0) {
        svm_err_hex_dump(jetty_buf, SVM_URMA_JFC_CTX_SIZE, "Jetty context dump for err CQE");
    } else {
        svm_err("Failed to get jetty context. (ret=%d; jfc_id=%u)\n", ret, jfc->jfc_id.id);
    }
}

int svm_urma_jetty_wait(struct svm_urma_jetty *jetty, int wr_num, int timeout_ms)
{
    u32 i, wait_num, ack_cnt = 1;
    bool poll_failed = false;
    urma_jfc_t *jfc = NULL;
    urma_status_t urma_ret;
    u64 post_id = jetty->post_wr_id;
    u64 ack_id = jetty->ack_wr_id;
    int cnt;

    wait_num = (wr_num >= 0) ? (u32)wr_num : svm_urma_jetty_get_jfs_wrs_post_num(jetty);

    for (i = 0; i < wait_num; i++) {
#ifndef EMU_ST /* to be delete */
        cnt = ascend_urma_wait_jfc(jetty->jfce, 1, timeout_ms, &jfc);
        if ((cnt != 1) || (jetty->jfc != jfc)) {
            svm_err("Urma wait jfc failed. (i=%u; cnt=%d; jfs=%u; jfc=%u; wait_num=%u; jetty_post_id=%llu; "
                    "jetty_ack_id=%llu; post_id=%llu; ack_id=%llu)\n",
                    i, cnt, jetty->jfs->jfs_id.id, jetty->jfc->jfc_id.id, wait_num, jetty->post_wr_id, jetty->ack_wr_id,
                    post_id, ack_id);
            return DRV_ERROR_INNER_ERR;
        }

        cnt = urma_poll_jfc(jetty->jfc, 1, jetty->crs);
        if ((cnt != (int)1) || (jetty->crs[0].status != URMA_CR_SUCCESS)) {
            svm_err("Urma poll jfc failed. (i=%u; ret_cnt=%d; status=%d; jfs=%u; jfc=%u)\n", i, cnt,
                    jetty->crs[0].status, jetty->jfs->jfs_id.id, jetty->jfc->jfc_id.id);
            poll_failed = true;
        }
#endif
        urma_ack_jfc((urma_jfc_t **)&jfc, &ack_cnt, 1);
        urma_ret = urma_rearm_jfc(jetty->jfc, false);
        if (urma_ret != URMA_SUCCESS) {
            svm_err("Urma rearm jfc failed. (ret=%d)\n", urma_ret);
            return DRV_ERROR_INNER_ERR;
        }

        jetty->ack_wr_id++;

#ifndef EMU_ST /* to be delete */
        if (poll_failed) {
            svm_urma_get_jfc_opt(jetty->jfc);
            return DRV_ERROR_PARA_ERROR;
        }
#endif
    }

    return DRV_ERROR_NONE;
}
