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
#include <pthread.h>
#include "securec.h"
#include "ascend_hal_error.h"
#include "queue_client_comm.h"
#include "uref.h"
#include "queue.h"
#include "que_compiler.h"
#include "que_urma.h"
#include "que_jetty.h"
#include "que_comm_agent.h"
#include "que_comm_chan.h"
#include "que_comm_event.h"
#include "que_comm_thread.h"
#include "que_chan.h"
#include "que_topic_sched.h"
#include "que_ub_msg.h"
#include "queue_interface.h"
#include "que_ctx.h"

#ifndef EMU_ST
static struct que_ctx *g_que_ctx[MAX_DEVICE] = {NULL};
static pthread_rwlock_t g_que_ctx_lock = PTHREAD_RWLOCK_INITIALIZER;

static inline void que_ctx_wrlock(void)
{
    (void)pthread_rwlock_wrlock(&g_que_ctx_lock);
}

static inline void que_ctx_rdlock(void)
{
    (void)pthread_rwlock_rdlock(&g_que_ctx_lock);
}

static inline void que_ctx_unlock(void)
{
    (void)pthread_rwlock_unlock(&g_que_ctx_lock);
}

static struct queue_sub_flag *que_subflag_init(unsigned int devid)
{
    struct queue_sub_flag *subflag = NULL;
    int i;

    subflag = (struct queue_sub_flag *)malloc(sizeof(struct queue_sub_flag) * MAX_SURPORT_QUEUE_NUM);
    if (que_unlikely(subflag == NULL)) {
        QUEUE_LOG_ERR("que subflag alloc fail. (devid=%u; size=%ld)\n", devid, sizeof(struct queue_sub_flag));
        return NULL;
    }
    for (i = 0; i < MAX_SURPORT_QUEUE_NUM; i++) {
        subflag[i].en_que = false;
        subflag[i].f2nf = false;
    }
    return subflag;
}

static struct que_ctx *_que_ctx_create(unsigned int devid, pid_t hostpid, pid_t devpid)
{
    struct que_ctx *ctx = NULL;

    ctx = (struct que_ctx *)calloc(1, sizeof(struct que_ctx));
    if (que_unlikely(ctx == NULL)) {
        QUEUE_LOG_ERR("que ctx alloc fail. (devid=%u; size=%ld; hostpid=%d; devpid=%d)\n", devid,
                      sizeof(struct que_ctx), hostpid, devpid);
        return NULL;
    }

    ctx->devid = devid;
    ctx->devpid = devpid;
    ctx->hostpid = hostpid;
    ctx->res_bitmap = (devid == halGetHostDevid()) ? 0x10 : 0xf;
    uref_init(&ctx->ref);

    return ctx;
}

static void _que_ctx_destroy(struct que_ctx *ctx)
{
    if (que_likely(ctx != NULL)) {
        free(ctx);
    }
}

static int _que_ctx_add(struct que_ctx *ctx)
{
    unsigned int devid = ctx->devid;

    if (que_unlikely(g_que_ctx[devid] != NULL)) {
        return DRV_ERROR_REPEATED_INIT;
    }
    g_que_ctx[devid] = ctx;

    return DRV_ERROR_NONE;
}

static int _que_ctx_del(struct que_ctx *ctx)
{
    unsigned int devid = ctx->devid;

    if (que_unlikely(g_que_ctx[devid] != ctx)) {
        return DRV_ERROR_UNINIT;
    }
    g_que_ctx[devid] = NULL;

    return DRV_ERROR_NONE;
}

static int que_clt_ctx_add(struct que_ctx *ctx)
{
    int ret;

    que_ctx_wrlock();
    ret = _que_ctx_add(ctx);
    que_ctx_unlock();

    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ctx add fail. (ret=%d; devid=%u; devpid=%d)\n", ret, ctx->devid, ctx->devpid);
    }

    return ret;
}

static int que_clt_ctx_del(struct que_ctx *ctx)
{
    int ret;

    que_ctx_wrlock();
    ret = _que_ctx_del(ctx);
    que_ctx_unlock();

    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ctx del fail. (ret=%d; devid=%u; devpid=%d)\n", ret, ctx->devid, ctx->devpid);
    }

    return ret;
}

drvError_t que_h2d_info_fill(unsigned int dev_id, struct que_event_attr *attr, struct que_init_in_msg *in)
{
    struct que_ctx *ctx = NULL;
    ctx = que_ctx_get(dev_id);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }

    /* To improve performance, only the first Jetty in the Jetty pool is passed to the device for jetty import. */
    attr->hostpid = ctx->hostpid;
    attr->devpid = ctx->devpid;
    in->hostpid = ctx->hostpid;
    in->jfr_id = ctx->jfr_pool[0][0].qjfr->jfr->jfr_id;
    in->token = ctx->token[TRANS_D2H_H2D];
    que_ctx_put(ctx);
    return DRV_ERROR_NONE;
}

drvError_t que_ctx_h2d_init(unsigned int devid, urma_jfr_id_t *tjfr_id, urma_token_t *token)
{
    urma_target_jetty_t *tjetty = NULL;
    struct queue_sub_flag *subflag = NULL;
    struct que_ctx *ctx = NULL;

    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }

    subflag = que_subflag_init(devid);
    if (que_unlikely(subflag == NULL)) {
        QUEUE_LOG_ERR("que subflag init fail. (devid=%u)\n", devid);
        que_ctx_put(ctx);
        return DRV_ERROR_INNER_ERR;
    }

    tjetty = que_jfr_import(devid, tjfr_id, token, TRANS_D2H_H2D);
    if (que_unlikely(tjetty == NULL)) {
        free(subflag);
        QUEUE_LOG_ERR("que jfr import fail. (devid=%u)\n", devid);
        que_ctx_put(ctx);
        return DRV_ERROR_INNER_ERR;
    }
    ctx->sub_flag = subflag;
    ctx->tjetty = tjetty;
    que_ctx_put(ctx);
    return DRV_ERROR_NONE;
}

static void que_ctx_h2d_uninit(struct que_ctx *ctx)
{
    if (que_likely(ctx->sub_flag != NULL)) {
        free(ctx->sub_flag);
        ctx->sub_flag = NULL;
    }
    if (que_likely(ctx->tjetty != NULL)) {
        que_jfr_unimport(ctx->tjetty);
        ctx->tjetty = NULL;
    }
}

static void que_ctx_release(struct uref *uref)
{
    struct que_ctx *ctx = container_of(uref, struct que_ctx, ref);
    que_ctx_h2d_uninit(ctx);
    que_ctx_uninit(ctx);
    _que_ctx_destroy(ctx);
}

static struct que_ctx *que_clt_ctx_get(unsigned int devid)
{
    struct que_ctx *ctx = NULL;

    if (que_unlikely(devid >= MAX_DEVICE)) {
        QUEUE_LOG_ERR("invalid devid. (devid=%u; max_devid=%u)\n", devid, (unsigned int)MAX_DEVICE);
        return NULL;
    }

    que_ctx_rdlock();
    ctx = g_que_ctx[devid];
    if (que_likely(ctx != NULL)) {
        if (uref_get_unless_zero(&ctx->ref) == 0) {
            que_ctx_unlock();
            return NULL;
        }
    }
    que_ctx_unlock();

    return ctx;
}

static void que_clt_ctx_put(struct que_ctx *ctx)
{
    uref_put(&ctx->ref, que_ctx_release);
}

int que_sub_status_set(struct QueueSubPara *sub_para, unsigned int devid, unsigned int qid)
{
    struct que_ctx *ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }
    if (que_unlikely(ctx->sub_flag == NULL)) {
        QUEUE_LOG_ERR("que sub flag get fail. (devid=%u; qid=%u)\n", devid, qid);
        que_ctx_put(ctx);
        return DRV_ERROR_PARA_ERROR;
    }

    struct queue_sub_flag *sub_flag = ctx->sub_flag;
    if (sub_para->eventType == QUEUE_F2NF_EVENT) {
        sub_flag[qid].f2nf = true;
    } else {
        sub_flag[qid].en_que = true;
    }
    que_ctx_put(ctx);
    return DRV_ERROR_NONE;
}

int que_unsub_status_set(struct QueueUnsubPara *unsub_para, unsigned int devid, unsigned int qid)
{
    struct que_ctx *ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }
    if (que_unlikely(ctx->sub_flag == NULL)) {
        QUEUE_LOG_ERR("que unsub flag get fail. (devid=%u; qid=%u)\n", devid, qid);
        que_ctx_put(ctx);
        return DRV_ERROR_PARA_ERROR;
    }

    struct queue_sub_flag *sub_flag = ctx->sub_flag;
    if (unsub_para->eventType == QUEUE_F2NF_EVENT) {
        sub_flag[unsub_para->qid].f2nf = false;
    } else {
        sub_flag[unsub_para->qid].en_que = false;
    }
    que_ctx_put(ctx);
    return DRV_ERROR_NONE;
}

bool que_is_need_sync_wait(unsigned int devid, unsigned int qid, unsigned int subevent_id)
{
    /* The user has subscribed events, so we does not need to wait. */
    struct que_ctx *ctx = NULL;
    struct queue_sub_flag *sub_flag = NULL;
    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        QUEUE_LOG_ERR("que ctx get fail. (devid=%u; qid=%u)\n", devid, qid);
        return false;
    }
    sub_flag = ctx->sub_flag;
    if (que_unlikely(sub_flag == NULL)) {
        QUEUE_LOG_ERR("que subflag get fail. (devid=%u; qid=%u)\n", devid, qid);
        que_ctx_put(ctx);
        return false;
    }
    if (subevent_id == DRV_SUBEVENT_ENQUEUE_MSG) {
        if (sub_flag[qid].f2nf) {
            QUEUE_LOG_INFO("sub event does not need to wait when enque. (dev_id=%u; qid=%u)\n", devid, qid);
            que_ctx_put(ctx);
            return false;
        }
    } else {
        if (sub_flag[qid].en_que) {
            QUEUE_LOG_INFO("sub event does not need to wait when deque. (dev_id=%u; qid=%u)\n", devid, qid);
            que_ctx_put(ctx);
            return false;
        }
    }
    que_ctx_put(ctx);
    return true;
}

int que_ctx_get_pids(unsigned int devid, pid_t *hostpid, pid_t *devpid)
{
    struct que_ctx *ctx = NULL;

    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }
    if (que_likely(hostpid != NULL)) {
        *hostpid = ctx->hostpid;
    }
    if (que_likely(devpid != NULL)) {
        *devpid = ctx->devpid;
    }
    que_ctx_put(ctx);

    return DRV_ERROR_NONE;
}

/*
 * tjfr_id: Remote que_ctx's tjetty id
 */
int que_ctx_create(unsigned int devid, pid_t hostpid, pid_t devpid)
{
    struct que_ctx *ctx = NULL;
    int ret;

    ctx = _que_ctx_create(devid, hostpid, devpid);
    if (que_unlikely(ctx == NULL)) {
        QUEUE_LOG_ERR("que ctx create fail. (devid=%u; devpid=%d)\n", devid, devpid);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = que_ctx_init(ctx);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ctx init fail. (ret=%d; devid=%u; hostpid=%d)\n", ret, devid, hostpid);
        goto ctx_destroy;
    }

    ret = que_ctx_add(ctx);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ctx add fail. (ret=%d; devid=%u; devpid=%d)\n", ret, devid, devpid);
        goto ctx_uninit;
    }
    return DRV_ERROR_NONE;

ctx_uninit:
    que_ctx_uninit(ctx);
ctx_destroy:
    _que_ctx_destroy(ctx);
    return ret;
}

void que_ctx_destroy(unsigned int devid)
{
    struct que_ctx *ctx = NULL;

    ctx = que_ctx_get(devid);
    if (que_likely(ctx != NULL)) {
        int ret = que_ctx_del(ctx);
        if (que_likely(ret == DRV_ERROR_NONE)) {
            que_ctx_put(ctx);
        }
        que_ctx_put(ctx);
    }
}

void que_clt_ub_uninit(void)
{
    unsigned int devid;

    /* Run the for loop twice to perform destroy and put operations to prevent the urma from deleting the context in
     * use. */
    for (devid = 0; devid < MAX_DEVICE; devid++) {
        que_ctx_destroy(devid);
    }
    for (devid = 0; devid < MAX_DEVICE; devid++) {
        que_urma_ctx_put_ex(devid);
    }
}

int que_ctx_update(unsigned int devid, unsigned int qid, struct que_ctx *ctx, QUEUE_AGENT_TYPE que_type, int *jfs_idx,
                   int *jfr_idx)
{
    int ret;
    unsigned int d2d_flag = TRANS_D2H_H2D;

    ret = que_qjfs_alloc(ctx->jfs_pool[d2d_flag], QUE_JETTY_ALLOC_TIME_OUT_MS, jfs_idx, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que jfs alloc fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        return ret;
    }

    ret = que_qjfr_alloc(ctx->jfr_pool[d2d_flag], QUE_JETTY_ALLOC_TIME_OUT_MS, jfr_idx);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        que_qjfs_free(ctx->jfs_pool[d2d_flag], *jfs_idx);
        QUEUE_LOG_ERR("que jfr alloc fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        return ret;
    }

    ret = que_chan_ini_update(devid, qid, &(ctx->jfs_pool[d2d_flag][*jfs_idx]), &(ctx->jfr_pool[d2d_flag][*jfr_idx]),
                              ctx->tjetty, ctx->token[d2d_flag], que_type, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        que_qjfr_free(ctx->jfr_pool[d2d_flag], *jfr_idx);
        que_qjfs_free(ctx->jfs_pool[d2d_flag], *jfs_idx);
        QUEUE_LOG_ERR("Que chan enque update fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
    }

    return ret;
}

int que_ctx_enque(unsigned int devid, unsigned int qid, struct buff_iovec *vector, int timeout)
{
    int ret, wait_ret, jfs_idx, jfr_idx, timeout_ms_ = timeout;
    uint64_t stamp[TRACE_UPDATE_BUFF] = {0};

    struct que_ctx *ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }
    stamp[TRACE_INI_START] = que_get_cur_time_ns();

    ret = que_ctx_update(devid, qid, ctx, H2D_SYNC_ENQUE, &jfs_idx, &jfr_idx);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ctx update fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        que_ctx_put(ctx);
        return ret;
    }
    stamp[TRACE_UPDATE] = que_get_cur_time_ns();

    do {
        struct timeval start, end;
        ret = que_chan_pkt_send(devid, qid, H2D_SYNC_ENQUE, vector, stamp);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que enque send fail. (ret=%d; devid=%u; qid=%u; devpid=%d)\n", ret, devid, qid, ctx->devpid);
            break;
        }

        ret = que_chan_wait(devid, qid, H2D_SYNC_ENQUE, QUEUE_SYNC_TIMEOUT);
        if (que_unlikely(ret == DRV_ERROR_NONE)) {
            break;
        }

        if (ret != DRV_ERROR_QUEUE_FULL) {
            QUEUE_LOG_ERR("que enque buff fail. (ret=%d; devid=%u; qid=%u; devpid=%d)\n", ret, devid, qid, ctx->devpid);
            break;
        }

        if (!que_is_need_sync_wait(devid, qid, DRV_SUBEVENT_ENQUEUE_MSG)) {
            break;
        }

        que_get_time(&start);
        wait_ret = queue_wait_event(devid, qid, ret, timeout_ms_);
        que_get_time(&end);
        if (que_unlikely(wait_ret != DRV_ERROR_NONE)) {
            break;
        }

        queue_updata_timeout(start, end, &timeout_ms_);
        que_chan_done(devid, qid, H2D_SYNC_ENQUE);
    } while (true);
    queue_report_insufficient_device_memory(vector, ret);
    que_chan_done(devid, qid, H2D_SYNC_ENQUE);
    que_qjfs_free(ctx->jfs_pool[TRANS_D2H_H2D], jfs_idx);
    que_qjfr_free(ctx->jfr_pool[TRANS_D2H_H2D], jfr_idx);
    que_ctx_put(ctx);
    return ret;
}

static inline bool que_deque_is_bar_buf(struct buff_iovec *vector)
{
#ifndef EMU_ST
    uint64_t ptr = (uint64_t)(uintptr_t)vector->ptr[0].iovec_base;
    struct DVattribute attr;
    drvError_t ret;

    ret = drvMemGetAttribute(ptr, &attr);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_DEBUG("call drvMemGetAttribute failed. (ret=%d; ptr=0x%llx)\n", ret, ptr);
        return false;
    }

    if (attr.memType == DV_MEM_LOCK_DEV || attr.memType == DV_MEM_LOCK_DEV_DVPP) {
        return true;
    }

    return false;
#else
    return true;
#endif
}

int que_deque_para_check(unsigned int devid, unsigned int qid, struct buff_iovec *vector)
{
    bool is_bar_buf = que_deque_is_bar_buf(vector);
    if (is_bar_buf == true) {
        if (que_unlikely(vector->count != 1)) {
            QUEUE_LOG_ERR("input param is invalid. (devid=%u; qid=%u; count=%u)\n", devid, qid, vector->count);
            return DRV_ERROR_INVALID_VALUE;
        }
    }
    return DRV_ERROR_NONE;
}

static bool queue_wait_and_update(unsigned int devid, unsigned int qid, int ret, int *timeout_ms_)
{
    int wait_ret;
    struct timeval start, end;
    que_get_time(&start);
    wait_ret = queue_wait_event(devid, qid, ret, *timeout_ms_);
    que_get_time(&end);
    if (que_unlikely(wait_ret != DRV_ERROR_NONE)) {
        return false;
    }
    queue_updata_timeout(start, end, timeout_ms_);
    return true;
}

int que_ctx_deque(unsigned int devid, unsigned int qid, struct buff_iovec *vector, int timeout)
{
    int ret, jfs_idx, jfr_idx, timeout_ms_ = timeout;
    uint64_t stamp[TRACE_UPDATE + 1] = {0};

    struct que_ctx *ctx = que_ctx_get(devid);
    if (que_unlikely(ctx == NULL)) {
        return DRV_ERROR_QUEUE_NOT_INIT;
    }
    stamp[TRACE_INI_START] = que_get_cur_time_ns();

    ret = que_deque_para_check(devid, qid, vector);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        goto out_put;
    }

    ret = que_ctx_update(devid, qid, ctx, H2D_SYNC_DEQUE, &jfs_idx, &jfr_idx);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ctx update fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, qid);
        goto out_put;
    }
    stamp[TRACE_UPDATE] = que_get_cur_time_ns();

    do {
        ret = que_chan_pkt_send(devid, qid, H2D_SYNC_DEQUE, vector, stamp);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que deque send fail. (ret=%d; devid=%u; qid=%u; devpid=%d)\n", ret, devid, qid, ctx->devpid);
            break;
        }

        ret = que_chan_wait(devid, qid, H2D_SYNC_DEQUE, QUEUE_SYNC_TIMEOUT);
        if (que_unlikely(ret == DRV_ERROR_NONE)) {
            break;
        }

        if (ret != DRV_ERROR_QUEUE_EMPTY) {
            QUEUE_LOG_ERR("que deque buff fail. (ret=%d; devid=%u; qid=%u; devpid=%d)\n", ret, devid, qid, ctx->devpid);
            break;
        }

        if (!que_is_need_sync_wait(devid, qid, DRV_SUBEVENT_DEQUEUE_MSG)) {
            break;
        }

        if (!queue_wait_and_update(devid, qid, ret, &timeout_ms_)) {
            break;
        }

        que_chan_done(devid, qid, H2D_SYNC_DEQUE);
    } while (true);
    queue_report_insufficient_device_memory(vector, ret);
    que_chan_done(devid, qid, H2D_SYNC_DEQUE);
    que_qjfr_free(ctx->jfr_pool[TRANS_D2H_H2D], jfr_idx);
    que_qjfs_free(ctx->jfs_pool[TRANS_D2H_H2D], jfs_idx);
out_put:
    que_ctx_put(ctx);
    return ret;
}

int queue_clt_init_check(unsigned int devid)
{
    struct que_ctx *ctx = NULL;
    ctx = que_ctx_get(devid);
    if (que_unlikely(ctx != NULL)) {
        que_ctx_put(ctx);
        return DRV_ERROR_REPEATED_INIT;
    }

    return DRV_ERROR_NONE;
}

static int __attribute__((constructor)) que_clt_ctx_init(void)
{
    struct que_agent_interface_list *list = que_get_agent_interface();
    list->que_ctx_get = que_clt_ctx_get;
    list->que_ctx_put = que_clt_ctx_put;
    list->que_ctx_add = que_clt_ctx_add;
    list->que_ctx_del = que_clt_ctx_del;
    return DRV_ERROR_NONE;
}
#else /* EMU_ST */

void que_clt_ctx_emu_test(void)
{}

#endif /* EMU_ST */
