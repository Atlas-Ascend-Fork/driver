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
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <errno.h>
#include "securec.h"
#include "urma_api.h"
#include "udma_u_ctl.h"
#include "svm_user_interface.h"
#include "trs_dev_drv.h"
#include "trs_uk_msg.h"
#include "esched_user_interface.h"
#include "trs_user_pub_def.h"
#include "trs_urma.h"
#include "trs_res.h"
#include "trs_sqcq.h"
#include "trs_master_async.h"
#include "trs_master_event.h"
#include "trs_master_urma.h"

#define TRS_URMA_JFC_CTX_SIZE 128U
#define TRS_URMA_JFC_WAIT_TIMEOUT_MS 1
#define TRS_URMA_POLL_THREAD_IDLE_US 8000

#if defined(__arm__) || defined(__aarch64__)
#define trs_smp_wmb                          \
    {                                        \
        asm volatile("dmb st" ::: "memory"); \
    }
#else
#define trs_smp_wmb
#endif

struct trs_urma_epoll {
    int epfd;
    struct epoll_event *epoll_events;
    bool init_flag;
};

enum {
    TRS_URMA_POLL_THREAD_STATUS_DOWN = 0,
    TRS_URMA_POLL_THREAD_STATUS_RUNNING = 1,
    TRS_URMA_POLL_THREAD_STATUS_EXIT = 2,
};

static struct trs_urma_epoll g_trs_urma_epoll = {0};
static pthread_t g_trs_urma_poll_thread;
static bool g_trs_urma_poll_thread_inited = false;
static int g_trs_urma_poll_thread_status = TRS_URMA_POLL_THREAD_STATUS_DOWN;
static urma_jfc_t **g_wait_jfc_buffer = NULL;
static uint32_t g_wait_jfc_buffer_num = 0;
static urma_cr_t *g_poll_jfc_cr = NULL;

drvError_t __attribute__((weak)) halSvmAccessRegister(uint32_t dev_id, uint64_t va, uint64_t size, uint64_t flag,
                                                      uint64_t *access_va)
{
    (void)dev_id;
    (void)size;
    (void)flag;

    *access_va = va;
    return DRV_ERROR_NONE;
}

drvError_t __attribute__((weak)) halSvmAccessUnRegister(uint32_t dev_id, uint64_t va, uint64_t flag)
{
    (void)dev_id;
    (void)va;
    (void)flag;
    return DRV_ERROR_NONE;
}

drvError_t __attribute__((weak)) halSvmAccess(uint32_t dev_id, uint64_t dst, uint64_t src, uint64_t size, uint32_t flag)
{
    (void)dev_id;
    (void)dst;
    (void)src;
    (void)size;
    (void)flag;
    return DRV_ERROR_NONE;
}

#define TRS_PAGE_SIZE (0x1000ULL)
drvError_t trs_register_reg(uint32_t dev_id, uint64_t va, uint32_t size)
{
    if ((va != 0) && (size != 0)) {
        uint64_t align_va = trs_align_down_power2(va, TRS_PAGE_SIZE);
        uint64_t align_len = trs_align_up_power2(va - align_va + size, TRS_PAGE_SIZE);
        uint64_t addr;
        int ret;

        ret = halSvmAccessRegister(dev_id, align_va, align_len, SVM_REGISTER_FLAG_ACCESS_BY_DMA, &addr);
        if (ret != DRV_ERROR_NONE) {
            trs_err("Failed to register SVM. (dev_id=%u; va=0x%llx; len=%u; ret=%d)\n", dev_id, align_va, align_len,
                    ret);
            return DRV_ERROR_INNER_ERR;
        }

        return DRV_ERROR_NONE;
    }
    return DRV_ERROR_NONE;
}

void trs_unregister_reg(uint32_t dev_id, uint64_t va, uint32_t size)
{
    (void)size;

    if (va != 0) {
        uint64_t align_va = trs_align_down_power2(va, TRS_PAGE_SIZE);
        (void)halSvmAccessUnRegister(dev_id, align_va, SVM_REGISTER_FLAG_ACCESS_BY_DMA);
    }
}

static drvError_t trs_urma_res_id_config(uint32_t dev_id, uint32_t ts_id, drvIdType_t type, uint32_t resource_id,
                                         uint32_t value)
{
    struct res_id_usr_info *info = NULL;
    int ret;

    info = trs_get_res_id_info(dev_id, ts_id, type, resource_id);
    if (info == NULL) {
        return DRV_ERROR_NOT_EXIST;
    }

    if (info->res_addr == 0) {
        ret = trs_res_map_reg_init(dev_id, ts_id, resource_id, type, info);
        if (ret != 0) {
            trs_err("Failed to init res id info. (dev_id=%u; type=%d; id=%u)\n", dev_id, type, resource_id);
            return ret;
        }
    }
    ret = halSvmAccess(dev_id, (uint64_t)info->res_addr, (uint64_t)(uintptr_t)&value, info->res_len,
                       SVM_MEM_ACCESS_WRITE);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Failed to config res. (dev_id=%u; ts_id=%u; res_type=%d; res_id=%u; va=0x%llx; len=%u; ret=%d)\n",
                dev_id, ts_id, type, resource_id, info->res_addr, info->res_len, ret);
    }
    return ret;
}

drvError_t trs_urma_res_config(uint32_t dev_id, struct halResourceIdInputInfo *in, struct halResourceConfigInfo *para)
{
    drvError_t ret;

    if (in->resourceId == UINT_MAX) {
        uint32_t res_id_num = trs_get_res_id_num(dev_id, in->tsId, in->type);
        uint32_t id;
        for (id = 0; id < res_id_num; id++) {
            ret = trs_urma_res_id_config(dev_id, in->tsId, in->type, id, para->value[0]);
            if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_NOT_EXIST)) {
                return ret;
            }
        }
        return DRV_ERROR_NONE;
    }

    return trs_urma_res_id_config(dev_id, in->tsId, in->type, in->resourceId, para->value[0]);
}

uint32_t trs_get_jfs_depth(void)
{
    return 8192U; /* 8192U: max jetty depth, which * (max_sge * 16 / 64 + 1) equal to 32k */
}

uint32_t trs_get_jfc_depth(void)
{
    return 5000U; /* 64: minimum jfc depth in urma */
}

uint32_t trs_get_jfr_depth(void)
{
    return 64U; /* 64: minimum jfr depth in urma */
}

static drvError_t trs_master_register_sq_que_segment(uint32_t dev_id, struct sqcq_usr_info *sq_info, uint32_t sq_id)
{
    struct trs_urma_ctx *urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;
    urma_reg_seg_flag_t flag = trs_get_seg_register_flag();
    urma_seg_cfg_t seg_cfg = {0};
    int register_errno = 0;

    seg_cfg.token_id = trs_get_urma_proc_ctx(dev_id)->token_id[sq_id % TRS_URMA_SEG_TOKEN_ALLOC_DIVIDE];
    seg_cfg.va = (uint64_t)(uintptr_t)sq_info->sq_map.que.addr;
    seg_cfg.len = (uint64_t)(sq_info->sq_map.que.len);
    seg_cfg.token_value = urma_ctx->token;
    seg_cfg.flag = flag;

    urma_ctx->local_tseg = urma_register_seg(urma_ctx->urma_ctx, &seg_cfg);
    if (urma_ctx->local_tseg != NULL) {
        return 0;
    }

    /* check if extend ummu table fail, use per-sq token id */
    register_errno = errno;
    if (register_errno == ERANGE) {
        urma_ctx->token_id = urma_alloc_token_id(urma_ctx->urma_ctx);
        if (urma_ctx->token_id == NULL) {
            trs_err("Failed to alloc sq token id. (dev_id=%u; sq_id=%u)\n", dev_id, sq_id);
            return DRV_ERROR_INNER_ERR;
        }

        trs_debug("Alloc per sq token id. (devid=%u; sq_id=%u)\n", dev_id, sq_id);
        seg_cfg.token_id = urma_ctx->token_id;
        urma_ctx->local_tseg = urma_register_seg(urma_ctx->urma_ctx, &seg_cfg);
    }

    if (urma_ctx->local_tseg == NULL) {
        trs_err("Failed to register sq queue segment. (dev_id=%u; sq_id=%u; register_errno=%d; errno=%d)\n", dev_id,
                sq_id, register_errno, errno);
        if (urma_ctx->token_id != NULL) {
            (void)urma_free_token_id(urma_ctx->token_id);
            urma_ctx->token_id = NULL;
        }
        return DRV_ERROR_INNER_ERR;
    }

    return 0;
}

static void trs_master_unregister_sq_que_segment(struct trs_urma_ctx *urma_ctx)
{
    if (urma_ctx->local_tseg != NULL) {
        (void)urma_unregister_seg(urma_ctx->local_tseg);
        urma_ctx->local_tseg = NULL;
    }

    if (urma_ctx->token_id != NULL) {
        (void)urma_free_token_id(urma_ctx->token_id);
        urma_ctx->token_id = NULL;
    }
}

extern int halMemGetSeg(uint32_t dev_id, uint64_t va, uint64_t size, urma_seg_t *seg, urma_token_t *token);
drvError_t trs_master_register_segment(uint32_t dev_id, struct sqcq_usr_info *sq_info, uint32_t sq_id)
{
    struct trs_urma_ctx *urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;
    urma_token_t notify_token;
    urma_seg_t notify_seg;
    void *notify_buffer = NULL;
    int ret = 0;

    if (trs_is_sq_init_without_sq_mem(sq_info->flag)) {
        return 0;
    }

    ret = trs_master_register_sq_que_segment(dev_id, sq_info, sq_id);
    if (ret != 0) {
        return ret;
    }

    ret = halMemAlloc(&notify_buffer, (sq_info->depth * 8),
                      MEM_HOST | MEM_TYPE_DDR | ((uint64_t)TSDRV_MODULE_ID << MEM_MODULE_ID_BIT));
    if (ret != DRV_ERROR_NONE) {
        trs_master_unregister_sq_que_segment(urma_ctx);
        trs_err("Alloc host notify buffer mem fail. (devid=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = halMemGetSeg(dev_id, (uint64_t)(uintptr_t)notify_buffer, (sq_info->depth * 8), &notify_seg, &notify_token);
    if (ret != 0) {
        (void)halMemFree(notify_buffer);
        trs_master_unregister_sq_que_segment(urma_ctx);
        trs_err("Failed to get src segment. (ret=%d; va=0x%llx; depth=%u)\n", ret, (uint64_t)(uintptr_t)notify_buffer,
                sq_info->depth);
        return ret;
    }

    urma_ctx->local_notify_tseg.seg = notify_seg;
    urma_ctx->notify_buffer = notify_buffer;

    return DRV_ERROR_NONE;
}

static void trs_master_unregister_segment(struct trs_urma_ctx *urma_ctx)
{
    if (urma_ctx->notify_buffer != NULL) {
        (void)halMemFree(urma_ctx->notify_buffer);
        urma_ctx->notify_buffer = NULL;
    }

    trs_master_unregister_sq_que_segment(urma_ctx);
}

int trs_remote_urma_info_init(struct trs_urma_ctx *urma_ctx, struct halSqCqInputInfo *in,
                              struct trs_remote_sync_info *sync_info)
{
    urma_import_seg_flag_t flag = trs_get_seg_import_flag();
    struct trs_urma_proc_ctx *proc_ctx = trs_get_urma_proc_ctx(urma_ctx->devid);
    if (proc_ctx->tjetty == NULL) {
#ifdef SSAPI_USE_MAMI
        urma_get_tp_cfg_t tpid_cfg = {
            .trans_mode = URMA_TM_RM,
            .local_eid = urma_ctx->eid, // local_eid
            .peer_eid = sync_info->eid, // remote_eid
            .flag.bs.ctp = true,
        };
        uint32_t tp_cnt = 1;
        urma_tp_info_t tpid_info = {0};
        urma_status_t status;
        urma_active_tp_cfg_t active_tp_cfg = {0};
#endif
        urma_rjfr_t rjfr = {0};

        rjfr.jfr_id = sync_info->jfr_id;
        rjfr.trans_mode = URMA_TM_RM;
        rjfr.flag.bs.token_policy = URMA_TOKEN_PLAIN_TEXT;
#ifdef SSAPI_USE_MAMI
        status = urma_get_tp_list(urma_ctx->urma_ctx, &tpid_cfg, &tp_cnt, &tpid_info);
        if ((status != 0) || (tp_cnt == 0)) {
            trs_err("Failed to get tp list. (status=%d; tp_cnt=%u)\n", status, tp_cnt);
            return DRV_ERROR_INNER_ERR;
        }
        active_tp_cfg.tp_handle = tpid_info.tp_handle;
        rjfr.tp_type = URMA_CTP;
        proc_ctx->tjetty = urma_import_jfr_ex(urma_ctx->urma_ctx, &rjfr, &sync_info->token, &active_tp_cfg);
#else
        proc_ctx->tjetty = urma_import_jfr(urma_ctx->urma_ctx, &rjfr, &sync_info->token);
#endif
        if (proc_ctx->tjetty == NULL) {
            trs_err("Failed to import remote jfr.\n");
            return DRV_ERROR_INNER_ERR;
        }
    }

    if (trs_is_sq_init_without_sq_mem(in->flag) == false) {
        urma_ctx->sq_que_tseg = urma_import_seg(urma_ctx->urma_ctx, &sync_info->sq_que_seg, &sync_info->token, 0, flag);
        if (urma_ctx->sq_que_tseg == NULL) {
            trs_err("Failed to import remote sq queue segment.\n");
            return DRV_ERROR_INNER_ERR;
        }
        urma_ctx->sq_tail_tseg = urma_import_seg(urma_ctx->urma_ctx, &sync_info->sq_tail_seg, &sync_info->token, 0,
                                                 flag);
        if (urma_ctx->sq_tail_tseg == NULL) {
            trs_err("Failed to import remote sq tail segment.\n");
            (void)urma_unimport_seg(urma_ctx->sq_que_tseg);
            return DRV_ERROR_INNER_ERR;
        }
    }

    urma_ctx->tjetty = proc_ctx->tjetty;
    urma_ctx->batch_2d_async_ctx.init_flag = false;
    urma_ctx->remote_sq_que_addr = sync_info->sq_que_addr;
    if (trs_is_async_jetty_pre_alloc(in->flag) == true) {
        urma_ctx->async_ctx.src_jetty_id = sync_info->jetty_id;
        urma_ctx->async_ctx.tpn = sync_info->tpn;
        urma_ctx->async_ctx.die_id = urma_ctx->die_id;
        urma_ctx->async_ctx.func_id = urma_ctx->func_id;
        urma_ctx->async_ctx.init_flag = true;
    } else {
        urma_ctx->async_ctx.init_flag = false;
    }

    return DRV_ERROR_NONE;
}

void trs_remote_urma_info_uninit(struct trs_urma_ctx *urma_ctx)
{
    if (urma_ctx->sq_tail_tseg != NULL) {
        (void)urma_unimport_seg(urma_ctx->sq_tail_tseg);
        urma_ctx->sq_tail_tseg = NULL;
    }

    if (urma_ctx->sq_que_tseg != NULL) {
        (void)urma_unimport_seg(urma_ctx->sq_que_tseg);
        urma_ctx->sq_que_tseg = NULL;
    }

    urma_ctx->tjetty = NULL;
}

static int _trs_master_urma_ctx_init(uint32_t dev_id, struct trs_urma_ctx *master_ctx)
{
    struct trs_urma_proc_ctx *urma_proc_ctx = trs_get_urma_proc_ctx(dev_id);
    if (urma_proc_ctx == NULL) {
        trs_err("Not open.\n");
        return DRV_ERROR_INVALID_DEVICE;
    }

    master_ctx->urma_ctx = urma_proc_ctx->urma_ctx;
    master_ctx->die_id = urma_proc_ctx->die_id;
    master_ctx->func_id = urma_proc_ctx->func_id;
    master_ctx->jfce = urma_proc_ctx->jfce;
    master_ctx->token = urma_proc_ctx->token;
    master_ctx->eid = urma_proc_ctx->eid;
    master_ctx->devid = dev_id;
    master_ctx->trs_jfr = urma_proc_ctx->trs_jfr;
    (void)pthread_mutex_init(&master_ctx->ctx_mutex, NULL);
    return 0;
}

static void _trs_master_urma_ctx_uninit(struct trs_urma_ctx *master_ctx)
{
    (void)pthread_mutex_destroy(&master_ctx->ctx_mutex);
}

static struct trs_urma_ctx *trs_master_urma_ctx_init(uint32_t dev_id)
{
    struct trs_urma_ctx *master_ctx = trs_urma_ctx_create();
    if (master_ctx == NULL) {
        trs_err("Failed to create master urma ctx.\n");
        return NULL;
    }

    if (_trs_master_urma_ctx_init(dev_id, master_ctx) != 0) {
        trs_urma_ctx_destroy(master_ctx);
        trs_err("Failed to init master urma ctx.\n");
        return NULL;
    }
    return master_ctx;
}

static void trs_master_urma_ctx_uninit(struct trs_urma_ctx *master_ctx)
{
    _trs_master_urma_ctx_uninit(master_ctx);
    trs_urma_ctx_destroy(master_ctx);
}

static inline int trs_master_urma_ctx_jfs_init(struct trs_urma_ctx *master_ctx, uint32_t sq_id)
{
    int ret = trs_create_jfs(master_ctx->urma_ctx, master_ctx->jfce, &master_ctx->trs_jfs, (void *)(uintptr_t)sq_id);
    if (ret != 0) {
        trs_err("Failed to create master jfs. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

static inline void trs_master_urma_ctx_jfs_uninit(struct trs_urma_ctx *master_ctx)
{
    trs_dev_ctx_urma_mutex_lock(master_ctx->devid);
    trs_destroy_jfs(&master_ctx->trs_jfs);
    trs_dev_ctx_urma_mutex_unlock(master_ctx->devid);
}

static int trs_sqcq_remote_alloc_with_urma(uint32_t dev_id, struct halSqCqInputInfo *in, struct trs_urma_ctx *urma_ctx,
                                           struct trs_remote_sync_info *sync_info)
{
    struct event_reply reply;
    void *sync_msg = NULL;
    UINT64 sync_msg_len;
    int ret, result;

    reply.buf_len = sizeof(struct trs_remote_sync_info) + sizeof(int);
    reply.buf = (char *)malloc(reply.buf_len);
    if (reply.buf == NULL) {
        trs_err("Malloc reply buffer failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    };

    sync_msg_len = sizeof(struct halSqCqInputInfo) + sizeof(urma_jfr_id_t) + sizeof(urma_token_t) + in->ext_info_len;
    sync_msg = malloc(sync_msg_len);
    if (sync_msg == NULL) {
        free(reply.buf);
        trs_err("Malloc sync_msg failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    ret = memcpy_s(sync_msg, sync_msg_len, in, sizeof(struct halSqCqInputInfo));
    ret |= memcpy_s((void *)((uintptr_t)sync_msg + sizeof(struct halSqCqInputInfo)),
                    sync_msg_len - sizeof(struct halSqCqInputInfo), &urma_ctx->trs_jfr.jfr->jfr_id,
                    sizeof(urma_jfr_id_t));
    ret |= memcpy_s((void *)((uintptr_t)sync_msg + sizeof(struct halSqCqInputInfo) + sizeof(urma_jfr_id_t)),
                    sync_msg_len - sizeof(struct halSqCqInputInfo) - sizeof(urma_jfr_id_t), &urma_ctx->token,
                    sizeof(urma_token_t));
    if ((in->ext_info != NULL) && (in->ext_info_len != 0)) {
        ret |= memcpy_s((void *)((uintptr_t)sync_msg + sizeof(struct halSqCqInputInfo) + sizeof(urma_jfr_id_t) +
                                 sizeof(urma_token_t)),
                        sync_msg_len - sizeof(struct halSqCqInputInfo) - sizeof(urma_jfr_id_t) - sizeof(urma_token_t),
                        in->ext_info, in->ext_info_len);
    }
    if (ret != 0) {
        free(reply.buf);
        free(sync_msg);
        trs_err("Failed to memcpy_s. (ret=%d)\n", ret);
        return DRV_ERROR_PARA_ERROR;
    }
    ret = trs_svm_mem_event_sync(dev_id, sync_msg, sync_msg_len, DRV_SUBEVENT_TRS_ALLOC_SQCQ_WITH_URMA_MSG, &reply);
    result = DRV_EVENT_REPLY_BUFFER_RET(reply.buf);
    if ((ret != DRV_ERROR_NONE) || (result != 0)) {
        trs_err_if(((ret != DRV_ERROR_NONE) || (result != DRV_ERROR_NO_RESOURCES)),
                   "Sync event. (dev_id=%u; ret=%d; result=%d)\n", dev_id, ret, result);
        ret = (ret != DRV_ERROR_NONE) ? ret : result;
        free(reply.buf);
        free(sync_msg);
        return ret;
    }

    (void)memcpy_s(sync_info, sizeof(struct trs_remote_sync_info), DRV_EVENT_REPLY_BUFFER_DATA_PTR(reply.buf),
                   sizeof(struct trs_remote_sync_info));

    free(reply.buf);
    free(sync_msg);

    return 0;
}

static int trs_sqcq_remote_free_with_urma(uint32_t dev_id, struct halSqCqFreeInfo *in)
{
    struct event_reply reply;
    int result, ret;

    reply.buf_len = sizeof(result);
    reply.buf = (char *)&result;
    ret = trs_local_mem_event_sync(dev_id, in, sizeof(struct halSqCqFreeInfo), DRV_SUBEVENT_TRS_FREE_SQCQ_WITH_URMA_MSG,
                                   &reply);
    if ((ret != 0) || (result != 0)) {
        trs_err("Failed to sync sqcq free event. (ret=%d; result=%d)\n", ret, result);
        return (ret != 0) ? ret : result;
    }

    return 0;
}

static drvError_t trs_remote_sqcq_alloc_with_urma(uint32_t dev_id, struct trs_urma_ctx *master_ctx,
                                                  struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
{
    struct trs_remote_sync_info sync_info = {0};
    struct halSqCqFreeInfo free_in;
    drvError_t ret;

    ret = trs_sqcq_remote_alloc_with_urma(dev_id, in, master_ctx, &sync_info);
    if (ret != 0) {
        trs_err_if((ret != DRV_ERROR_NO_RESOURCES), "Alloc sqcq with urma ret. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = trs_remote_urma_info_init(master_ctx, in, &sync_info);
    if (ret != 0) {
#ifndef EMU_ST
        free_in.type = in->type;
        free_in.tsId = in->tsId;
        free_in.flag = in->flag;
        free_in.sqId = sync_info.out.sqId;
        free_in.cqId = sync_info.out.cqId;
        trs_sqcq_remote_free_with_urma(dev_id, &free_in);
        trs_err("Failed to init remote urma info. (ret=%d; dev_id=%u)\n", ret, dev_id);
        return ret;
#endif
    }

    (void)memcpy_s(out, sizeof(struct halSqCqOutputInfo), &sync_info.out, sizeof(struct halSqCqOutputInfo));
    trs_debug("Remote alloc sqcq success. (sqid=%u; cqid=%u)\n", sync_info.out.sqId, sync_info.out.cqId);
    return DRV_ERROR_NONE;
}

drvError_t trs_remote_sq_cq_free_with_urma(uint32_t dev_id, struct halSqCqFreeInfo *in, struct trs_urma_ctx *urma_ctx,
                                           bool remote_free_flag)
{
    drvError_t ret;

    trs_remote_urma_info_uninit(urma_ctx);
    if (remote_free_flag) {
        ret = trs_sqcq_remote_free_with_urma(dev_id, in);
        if (ret != 0) {
            trs_err("Failed to free sqcq with urma. (ret=%d; dev_id=%u; sqid=%u; cqid=%u)\n", ret, dev_id, in->sqId,
                    in->cqId);
            return ret;
        }
    }

    trs_debug("Remote free success. (dev_id=%u; type=%d; sqid=%u; cqid=%u)\n", dev_id, in->type, in->sqId, in->cqId);

    return DRV_ERROR_NONE;
}

static drvError_t trs_local_sq_cq_alloc_with_urma(uint32_t dev_id, struct trs_urma_ctx *master_ctx,
                                                  struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
{
    struct sqcq_usr_info *sq_info = NULL;
    struct halSqCqFreeInfo free_in;
    uint32_t flag = in->flag;
    drvError_t ret;

    in->flag |= TSDRV_FLAG_SPECIFIED_SQ_ID | TSDRV_FLAG_SPECIFIED_CQ_ID;
    in->flag &= (uint32_t)(~TSDRV_FLAG_RANGE_ID);
    ret = trs_local_sqcq_alloc(dev_id, in, out);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Failed to alloc sqcq with specified id. (dev_id=%u; sq_id=%u; cq_id=%u)\n", dev_id, in->sqId,
                in->cqId);
        return ret;
    }

    sq_info = trs_get_sq_info(dev_id, in->tsId, DRV_NORMAL_TYPE, out->sqId);
    sq_info->urma_ctx = (void *)master_ctx;

    ret = trs_master_register_segment(dev_id, sq_info, out->sqId);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        trs_err("Failed to register master sq queue. (ret=%d; dev_id=%u)\n", ret, dev_id);
        free_in.type = in->type;
        free_in.tsId = in->tsId;
        free_in.flag = in->flag;
        free_in.sqId = in->sqId;
        free_in.cqId = in->cqId;
        (void)trs_local_sqcq_free(dev_id, &free_in);
        return ret;
#endif
    }

    if (!trs_is_sq_init_without_sq_mem(in->flag) && (sq_info->sq_ctrl.shr_info != NULL)) {
        sq_info->sq_ctrl.shr_info->jfs_id = master_ctx->trs_jfs.jfs->jfs_id.id;
        sq_info->sq_ctrl.shr_info->jfc_id = master_ctx->trs_jfs.trs_jfc.jfc->jfc_id.id;
    }
    in->flag = flag;
    trs_debug("Local alloc sqcq success. (sqid=%u; cqid=%u)\n", in->sqId, in->cqId);
    return ret;
}

static drvError_t trs_local_sq_cq_free_with_urma(uint32_t dev_id, struct halSqCqFreeInfo *info,
                                                 struct trs_urma_ctx *urma_ctx)
{
    uint32_t flag = info->flag;
    drvError_t ret;

    trs_master_unregister_segment(urma_ctx);

    info->flag |= TSDRV_FLAG_SPECIFIED_SQ_ID | TSDRV_FLAG_SPECIFIED_CQ_ID;
#ifndef EMU_ST /* Don't delete because in UB UT scene sqcq has been free in device */
    ret = trs_local_sqcq_free(dev_id, info);
    if (ret != 0) {
        struct sqcq_usr_info *sq_info = trs_get_sq_info(dev_id, info->tsId, DRV_NORMAL_TYPE, info->sqId);
        if (sq_info != NULL) {
            trs_master_register_segment(dev_id, sq_info, info->sqId);
        }
        trs_err("Failed to free local sqcq. (dev_id=%d)\n", dev_id);
        info->flag = flag;
        return ret;
    }
#endif
    info->flag = flag;
    return DRV_ERROR_NONE;
}

drvError_t trs_sqcq_urma_alloc(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
{
    struct halSqCqOutputInfo remote_out = {0};
    struct halSqCqFreeInfo free_in;
    struct trs_urma_ctx *master_ctx = NULL;
    drvError_t ret;

    if ((in->flag & TSDRV_FLAG_ONLY_SQCQ_ID) != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    master_ctx = trs_master_urma_ctx_init(dev_id);
    if (master_ctx == NULL) {
        trs_err("Failed to init master urma ctx.\n");
        return DRV_ERROR_INNER_ERR;
    }

    ret = trs_remote_sqcq_alloc_with_urma(dev_id, master_ctx, in, &remote_out);
    if (ret != 0) {
        trs_err_if((ret != DRV_ERROR_NO_RESOURCES), "Alloc remote sqcq ret. (ret=%d; dev_id=%u)\n", ret, dev_id);
        goto remote_sqcq_alloc_fail;
    }

    ret = trs_master_urma_ctx_jfs_init(master_ctx, remote_out.sqId);
    if (ret != 0) {
        goto urma_jfs_init_fail;
    }

    in->sqId = remote_out.sqId;
    in->cqId = remote_out.cqId;
    ret = trs_local_sq_cq_alloc_with_urma(dev_id, master_ctx, in, out);
    if (ret != 0) {
        trs_err("Failed to alloc sqcq with specified id. (ret=%d; dev_id=%u; sq_id=%u; cq_id=%u)\n", ret, dev_id,
                remote_out.sqId, remote_out.cqId);
        goto local_sqcq_alloc_fail;
    }

    trs_debug("Alloc sqcq success. (dev_id=%u; sqid=%u; cqid=%u; jfs_id=%u; jfc_id=%u)\n", dev_id, in->sqId, in->cqId,
              master_ctx->trs_jfs.jfs->jfs_id.id, master_ctx->trs_jfs.trs_jfc.jfc->jfc_id.id);
    return DRV_ERROR_NONE;

local_sqcq_alloc_fail:
    trs_master_urma_ctx_jfs_uninit(master_ctx);
urma_jfs_init_fail:
    free_in.type = in->type;
    free_in.tsId = in->tsId;
    free_in.sqId = remote_out.sqId;
    free_in.cqId = remote_out.cqId;
    free_in.flag = in->flag;
    (void)trs_remote_sq_cq_free_with_urma(dev_id, &free_in, master_ctx, true);
remote_sqcq_alloc_fail:
    trs_master_urma_ctx_uninit(master_ctx);
    return ret;
}

drvError_t trs_sqcq_urma_free(uint32_t dev_id, struct halSqCqFreeInfo *info, bool remote_free_flag)
{
    struct sqcq_usr_info *sq_info = NULL;
    struct trs_urma_ctx *urma_ctx = NULL;
    drvError_t ret;

    sq_info = trs_get_sq_info(dev_id, info->tsId, info->type, info->sqId);
    if (sq_info == NULL) {
        return DRV_ERROR_NOT_EXIST;
    }
    urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;

    ret = trs_local_sq_cq_free_with_urma(dev_id, info, urma_ctx);
    if (ret != 0) {
        trs_err("Failed to free local sqcq. (dev_id=%d; sqid=%u; cqid=%u)\n", dev_id, info->sqId, info->cqId);
        return ret;
    }

    trs_master_urma_ctx_jfs_uninit(urma_ctx);
    ret = trs_remote_sq_cq_free_with_urma(dev_id, info, urma_ctx, remote_free_flag);
    if (ret != 0) {
        trs_err("Failed to free sqcq with urma. (dev_id=%d)\n", dev_id);
        return ret;
    }

    (void)trs_async_uninit_async_ctx(dev_id, info->sqId, urma_ctx);

    trs_master_urma_ctx_uninit(urma_ctx);

    return DRV_ERROR_NONE;
}

int trs_recycle_sq_cq_with_urma(uint32_t dev_id, uint32_t ts_id, uint32_t sq_id, uint32_t cq_id, bool remote_free_flag)
{
    struct halSqCqFreeInfo info = {0};

    info.type = DRV_NORMAL_TYPE;
    info.tsId = ts_id;
    info.sqId = sq_id;
    info.cqId = cq_id;
    return trs_sqcq_urma_free(dev_id, &info, remote_free_flag);
}

static void trs_urma_get_jfc_opt(urma_jfc_t *jfc)
{
    char jetty_buf[TRS_URMA_JFC_CTX_SIZE] = {0};
    int ret;

    ret = urma_get_jfc_opt(jfc, URMA_JFC_FULL_CTX, jetty_buf, TRS_URMA_JFC_CTX_SIZE);
    if (ret == 0) {
        trs_err_hex_dump(jetty_buf, TRS_URMA_JFC_CTX_SIZE, "Jetty context dump for err CQE");
    } else {
        trs_err("Failed to get jetty context. (ret=%d; jfc_id=%u)\n", ret, jfc->jfc_id.id);
    }
}

static int trs_jfs_wqe_credit_poll(uint32_t dev_id, struct trs_urma_jfs *trs_jfs)
{
    int cnt, i;
    bool poll_failed = false;

    if (g_poll_jfc_cr == NULL) {
        g_poll_jfc_cr = (urma_cr_t *)malloc(sizeof(urma_cr_t) * trs_get_jfc_depth());
        if (g_poll_jfc_cr == NULL) {
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    cnt = urma_poll_jfc(trs_jfs->trs_jfc.jfc, (int)trs_get_jfc_depth(), g_poll_jfc_cr);
    if (cnt < 0) {
        trs_err("Urma poll jfc failed. (jfs_id=%u; cnt=%u)\n", trs_jfs->jfs->jfs_id.id, cnt);
        trs_urma_get_jfc_opt(trs_jfs->trs_jfc.jfc);
        return DRV_ERROR_INNER_ERR;
    }

    for (i = 0; i < cnt; i++) {
        if (g_poll_jfc_cr[i].status != URMA_CR_SUCCESS) {
            poll_failed = true;
            trs_err("Cr status abnormal. (index=%d, cr_status=%d; cnt=%u; jfs_id=%u; jfc_id=%u; sq_id=%u; dev_id=%u)",
                    i, g_poll_jfc_cr[i].status, cnt, trs_jfs->jfs->jfs_id.id, trs_jfs->trs_jfc.jfc->jfc_id.id,
                    trs_jfs->trs_jfc.jfc->jfc_cfg.user_ctx, dev_id);
        }
    }

    if (poll_failed) {
        trs_urma_get_jfc_opt(trs_jfs->trs_jfc.jfc);
    }

    trs_debug("Polled jfc cqe. (jfs_id=%u; jfc_id=%u; sq_id=%u; dev_id=%u; cnt=%d)\n", trs_jfs->jfs->jfs_id.id,
              trs_jfs->trs_jfc.jfc->jfc_id.id, trs_jfs->trs_jfc.jfc->jfc_cfg.user_ctx, dev_id, cnt);
    return DRV_ERROR_NONE;
}

static inline void trs_urma_jfc_ack(urma_jfc_t *jfc)
{
    uint32_t ack_cnt = 1;
    urma_ack_jfc((urma_jfc_t **)&jfc, &ack_cnt, 1);
}

static inline int trs_urma_jfc_rearm(urma_jfc_t *jfc)
{
    urma_status_t urma_ret;
    urma_ret = urma_rearm_jfc(jfc, false);
    if (urma_ret != URMA_SUCCESS) {
        trs_err("Urma rearm jfc failed. (ret=%d; jfc_id=%u)\n", urma_ret, jfc->jfc_id.id);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

static inline int trs_urma_jfc_ack_rearm(urma_jfc_t *jfc)
{
    trs_urma_jfc_ack(jfc);
    return trs_urma_jfc_rearm(jfc);
}

static void trs_urma_poll_waited_jfc(uint32_t dev_id, urma_jfc_t *wait_jfc)
{
    struct sqcq_usr_info *sq_info = NULL;
    struct trs_urma_jfs *trs_jfs = NULL;
    int ret;

    if (wait_jfc == NULL) {
        trs_warn_limit("Invalid null jfc.\n");
        return;
    }

    sq_info = _trs_get_sq_info(dev_id, 0, DRV_NORMAL_TYPE, (uint32_t)wait_jfc->jfc_cfg.user_ctx);
    if (sq_info == NULL) {
        trs_warn_limit("Get sq info failed. (dev_id=%u; sq_id=%u)\n", dev_id, (uint32_t)wait_jfc->jfc_cfg.user_ctx);
        trs_urma_jfc_ack(wait_jfc);
        return;
    }

    (void)pthread_rwlock_rdlock(&sq_info->mutex);

    if (sq_info->valid == 0) {
        trs_warn_limit("Sq has been freed. (dev_id=%u; sq_id=%u)\n", dev_id, (uint32_t)wait_jfc->jfc_cfg.user_ctx);
        trs_urma_jfc_ack(wait_jfc);
        goto unlock_sq_rwlock;
    }

    trs_jfs = &(((struct trs_urma_ctx *)sq_info->urma_ctx)->trs_jfs);
    if ((trs_jfs == NULL) || (trs_jfs->trs_jfc.jfc != wait_jfc)) {
        trs_warn_limit("Skip invalid jfc. (jfc_id=%u; jfs_is_null=%d)\n", wait_jfc->jfc_id.id, (trs_jfs == NULL));
        trs_urma_jfc_ack(wait_jfc);
        goto unlock_sq_rwlock;
    }

    trs_debug("Start to poll. (dev_id=%u; sq_id=%u)\n", dev_id, (uint32_t)wait_jfc->jfc_cfg.user_ctx);
    ret = trs_jfs_wqe_credit_poll(dev_id, trs_jfs);
    if (ret != 0) {
        trs_warn_limit("Thread poll jfc credit update failed. (ret=%d; jfc_id=%u)\n", ret, wait_jfc->jfc_id.id);
    }

    (void)trs_urma_jfc_ack_rearm(wait_jfc);
unlock_sq_rwlock:
    (void)pthread_rwlock_unlock(&sq_info->mutex);
}

static void trs_urma_poll_proc_jfce(uint32_t dev_id)
{
    struct trs_urma_proc_ctx *proc_ctx = trs_get_urma_proc_ctx(dev_id);
    int cnt;
    int i;

    /* malloc jfc buffer */
    if (g_wait_jfc_buffer_num < trs_get_sq_num(dev_id, 0, DRV_NORMAL_TYPE)) {
        if (g_wait_jfc_buffer != NULL) {
            free(g_wait_jfc_buffer);
        }

        g_wait_jfc_buffer_num = trs_get_sq_num(dev_id, 0, DRV_NORMAL_TYPE);
        g_wait_jfc_buffer = (urma_jfc_t **)malloc(sizeof(urma_jfc_t *) * g_wait_jfc_buffer_num);
        if (g_wait_jfc_buffer == NULL) {
            trs_warn("Malloc g_wait_jfc_buffer failed. (num=%u)\n", g_wait_jfc_buffer_num);
            g_wait_jfc_buffer_num = 0;
            return;
        }
    }

    if ((proc_ctx == NULL) || (proc_ctx->jfce == NULL) || (g_wait_jfc_buffer_num == 0)) {
        return;
    }

    trs_dev_ctx_urma_mutex_lock(dev_id);
    cnt = urma_wait_jfc(proc_ctx->jfce, g_wait_jfc_buffer_num, TRS_URMA_JFC_WAIT_TIMEOUT_MS, g_wait_jfc_buffer);
    if (cnt <= 0) {
        trs_dev_ctx_urma_mutex_unlock(dev_id);
        return;
    }

    trs_debug("Waited jfc. (devid=%u; cnt=%d)\n", dev_id, cnt);
    for (i = 0; i < cnt; i++) {
        trs_urma_poll_waited_jfc(dev_id, g_wait_jfc_buffer[i]);
    }
    trs_dev_ctx_urma_mutex_unlock(dev_id);
}

static void trs_jfs_write_src_notify_sge_init(struct sqcq_usr_info *sq_info, urma_sge_t *src_sge_buf, uint32_t tail)
{
#ifndef EMU_ST
    struct trs_urma_ctx *urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;

    *((u64 *)((u64)(uintptr_t)urma_ctx->notify_buffer + tail * 8)) = tail;
    src_sge_buf->addr = (u64)(uintptr_t)urma_ctx->notify_buffer + tail * 8;
    src_sge_buf->len = 8; /* notify len must be 8 bytes */
    src_sge_buf->tseg = &urma_ctx->local_notify_tseg;
#endif
}

static void trs_jfs_write_src_sge_init(struct sqcq_usr_info *sq_info, urma_sge_t *src_sge_buf, uint32_t tail,
                                       uint32_t sqe_num)
{
#ifndef EMU_ST
    struct trs_urma_ctx *urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;
    src_sge_buf->addr = urma_ctx->local_tseg->seg.ubva.va + tail * sq_info->e_size;
    src_sge_buf->len = sqe_num * sq_info->e_size;
    src_sge_buf->tseg = urma_ctx->local_tseg;
#endif
}

static void trs_jfs_write_dst_sge_init(struct sqcq_usr_info *sq_info, urma_sge_t *dst_sge_sqe, uint32_t tail,
                                       uint32_t sqe_num)
{
#ifndef EMU_ST
    struct trs_urma_ctx *urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;
    dst_sge_sqe->addr = urma_ctx->sq_que_tseg->seg.ubva.va + tail * sq_info->e_size;
    dst_sge_sqe->len = sqe_num * sq_info->e_size;
    dst_sge_sqe->tseg = urma_ctx->sq_que_tseg;
#endif
}

static void trs_jfs_write_with_notify_dst_sge_init(struct sqcq_usr_info *sq_info, urma_sge_t *dst_sge_sqe,
                                                   urma_sge_t *dst_sge_notify, uint32_t tail, uint32_t sqe_num)
{
#ifndef EMU_ST
    struct trs_urma_ctx *urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;
    dst_sge_sqe->addr = urma_ctx->sq_que_tseg->seg.ubva.va + tail * sq_info->e_size;
    dst_sge_sqe->len = sqe_num * sq_info->e_size;
    dst_sge_sqe->tseg = urma_ctx->sq_que_tseg;

    dst_sge_notify->addr = urma_ctx->sq_tail_tseg->seg.ubva.va;
    dst_sge_notify->len = 8; /* notify len must be 8 bytes */
    dst_sge_notify->tseg = urma_ctx->sq_tail_tseg;
#endif
}

static void trs_fill_sq_task_wq(struct trs_urma_ctx *urma_ctx, urma_opcode_t opcode, urma_jfs_wr_t *wr)
{
#ifndef EMU_ST
    wr->rw.src.num_sge = 1;
    wr->rw.dst.num_sge = 1;
    wr->opcode = opcode;
    wr->tjetty = urma_ctx->tjetty;
    wr->flag.bs.place_order = 2; /* 2: strong order */
    wr->flag.bs.comp_order = 1;  /* 1: Completion order with previous WR */
#endif
}

static void trs_set_urma_resp_cqe(urma_jfs_wr_t *wr)
{
    wr->flag.bs.complete_enable = 0; /* cqe will report even cqe_enable not set when wqe fail */
}

static int trs_post_send_wr(struct sqcq_usr_info *sq_info, uint32_t wqe_num, urma_jfs_wr_t *wr, bool is_db_en)
{
    struct trs_urma_ctx *urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;
    urma_jfs_wr_t *bad_wr = NULL;
    urma_status_t urma_ret;

    urma_ret = urma_post_jfs_wr(urma_ctx->trs_jfs.jfs, wr, &bad_wr);
    if (urma_ret == URMA_ENOMEM) {
        trs_warn_limit("Jfs full.\n");
        return DRV_ERROR_NO_RESOURCES;
    } else if (urma_ret != URMA_SUCCESS) {
        trs_err("Failed to post wr. (ret=%d)\n", urma_ret);
        return DRV_ERROR_INNER_ERR;
    }

    urma_ctx->trs_jfs.pi = (urma_ctx->trs_jfs.pi + wqe_num) % TRS_UB_PI_CI_DEPTH;

    if (is_db_en == true) {
        trs_smp_wmb;
        *(urma_ctx->trs_jfs.jfs_db_addr) = urma_ctx->trs_jfs.pi;
    }

    return 0;
}

drvError_t trs_sq_task_send_with_urma(uint32_t dev_id, uint32_t sqe_num, struct sqcq_usr_info *sq_info)
{
    struct trs_urma_ctx *urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;
    urma_jfs_wr_t wr = {0};
    urma_jfs_wr_t wr_next = {0};
    urma_jfs_wr_t wr_notify = {0};
    urma_sge_t src_sge_buf = {0};
    urma_sge_t dst_sge_buf = {0};
    urma_sge_t src_sge_buf_next = {0};
    urma_sge_t dst_sge_buf_next = {0};
    urma_sge_t src_sge_buf_notify = {0};
    urma_sge_t dst_sge_buf_notify = {0};
    uint32_t wqe_num;
    int ret;

    if (sq_info->tail + sqe_num <= sq_info->depth) {
        trs_fill_sq_task_wq(urma_ctx, URMA_OPC_WRITE, &wr);
        trs_set_urma_resp_cqe(&wr);
        trs_jfs_write_src_sge_init(sq_info, &src_sge_buf, sq_info->tail, sqe_num);
        trs_jfs_write_with_notify_dst_sge_init(sq_info, &dst_sge_buf, &dst_sge_buf_notify, sq_info->tail, sqe_num);
        wr.rw.src.sge = &src_sge_buf;
        wr.rw.dst.sge = &dst_sge_buf;

        trs_fill_sq_task_wq(urma_ctx, URMA_OPC_WRITE, &wr_notify);
        trs_set_urma_resp_cqe(&wr_notify);
        trs_jfs_write_src_notify_sge_init(sq_info, &src_sge_buf_notify, (sq_info->tail + sqe_num) % sq_info->depth);
        wr_notify.rw.src.sge = &src_sge_buf_notify;
        wr_notify.rw.dst.sge = &dst_sge_buf_notify;
        wr.next = &wr_notify;

        wqe_num = 2;
    } else {
#ifndef EMU_ST
        uint32_t cur_sqe_num, sq_tail;
        /* first, make the sqes at the tail of queue */
        trs_fill_sq_task_wq(urma_ctx, URMA_OPC_WRITE, &wr);
        trs_set_urma_resp_cqe(&wr);
        cur_sqe_num = sq_info->depth - sq_info->tail;
        trs_jfs_write_src_sge_init(sq_info, &src_sge_buf, sq_info->tail, cur_sqe_num);
        trs_jfs_write_dst_sge_init(sq_info, &dst_sge_buf, sq_info->tail, cur_sqe_num);
        wr.rw.src.sge = &src_sge_buf;
        wr.rw.dst.sge = &dst_sge_buf;

        /* second, make the sqes at the head of queue */
        trs_fill_sq_task_wq(urma_ctx, URMA_OPC_WRITE, &wr_next);
        trs_set_urma_resp_cqe(&wr_next);
        sq_tail = (sq_info->tail + sqe_num) % sq_info->depth;
        cur_sqe_num = sq_tail;
        trs_jfs_write_src_sge_init(sq_info, &src_sge_buf_next, 0, cur_sqe_num);
        trs_jfs_write_with_notify_dst_sge_init(sq_info, &dst_sge_buf_next, &dst_sge_buf_notify, 0, cur_sqe_num);
        wr_next.rw.src.sge = &src_sge_buf_next;
        wr_next.rw.dst.sge = &dst_sge_buf_next;
        wr.next = &wr_next;

        trs_fill_sq_task_wq(urma_ctx, URMA_OPC_WRITE, &wr_notify);
        trs_set_urma_resp_cqe(&wr_notify);
        trs_jfs_write_src_notify_sge_init(sq_info, &src_sge_buf_notify, sq_tail);
        wr_notify.rw.src.sge = &src_sge_buf_notify;
        wr_notify.rw.dst.sge = &dst_sge_buf_notify;
        wr_next.next = &wr_notify;

        wqe_num = 3;
#endif
    }

    ret = trs_post_send_wr(sq_info, wqe_num, &wr, true);
    if (ret != 0) {
        trs_warn_limit("Send sq task warn. (ret=%d; dev_id=%u)\n", ret, dev_id);
    }

    return ret;
}

drvError_t trs_sq_task_send_urma(uint32_t dev_id, struct halTaskSendInfo *info, struct sqcq_usr_info *sq_info)
{
    struct trs_urma_ctx *urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;
    int ret;

    if (sq_info->status == 0) {
        trs_err("Invalid status. (dev_id=%u; sq_id=%u; status=%u)\n", dev_id, info->sqId, sq_info->status);
        return DRV_ERROR_STATUS_FAIL;
    }

    ret = trs_sq_task_send_check(dev_id, info, sq_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (trs_get_sq_ctrl_flag(sq_info, info) == TRS_SQ_CTRL_BY_TRS_FLAG) {
        trs_sq_task_fill(info, sq_info);
    }

    info->pos = sq_info->tail;

    ret = trs_sq_task_send_with_urma(dev_id, info->sqe_num, sq_info);
    if (ret != 0) {
        return ret;
    }

    sq_info->tail = (sq_info->tail + info->sqe_num) % sq_info->depth;
    trs_sq_send_ok_stat(sq_info, info->sqe_num);
    trs_debug("Send sqe success. (dev_id=%u; sq_id=%u; jfs=%u; jfc=%u; sqe_num=%u; sq_tail=%u)\n", dev_id, info->sqId,
              urma_ctx->trs_jfs.jfs->jfs_id.id, urma_ctx->trs_jfs.trs_jfc.jfc->jfc_id.id, info->sqe_num, sq_info->tail);
    return ret;
}

static void trs_jfs_write_sge_init(uint64_t va, uint64_t len, urma_target_seg_t *tseg, urma_sge_t *src_sge_buf)
{
#ifndef EMU_ST
    src_sge_buf->addr = va;
    src_sge_buf->len = (uint32_t)len;
    src_sge_buf->tseg = tseg;
#endif
}

static int trs_args_async_copy_info_check(struct halSqTaskArgsInfo *info)
{
    struct halTsegInfo *srcTsegInfo = info->srcTsegInfo;
    struct halTsegInfo *dstTsegInfo = info->dstTsegInfo;

    if ((srcTsegInfo == NULL) || (dstTsegInfo == NULL)) {
        trs_err("Invalid NULL TsegInfo. (srcTsegInfo=%p; dstTsegInfo=%p)\n", srcTsegInfo, dstTsegInfo);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((srcTsegInfo->tseg_ptr == NULL) || (dstTsegInfo->tseg_ptr == NULL)) {
        trs_err("Invalid NULL TsegInfo tseg_ptr. (src_tseg_ptr=%p; dst_tseg_ptr=%p)\n", srcTsegInfo->tseg_ptr,
                dstTsegInfo->tseg_ptr);
        return DRV_ERROR_INVALID_VALUE;
    }

    return 0;
}

drvError_t trs_sq_task_args_async_copy(uint32_t dev_id, struct halSqTaskArgsInfo *info, struct sqcq_usr_info *sq_info)
{
#ifndef EMU_ST
    struct trs_urma_ctx *urma_ctx = (struct trs_urma_ctx *)sq_info->urma_ctx;
    u32 wr_num = (u32)(trs_align_up_power2((uint64_t)info->size, TRS_URMA_WR_CPY_MAX_SIZE) / TRS_URMA_WR_CPY_MAX_SIZE);
    u32 i;
    int ret;

    if (wr_num > TRS_URMA_ARGS_CPY_WR_MAX_NUM) {
        trs_err("Size exceeds depth. (depth=%u; size=%u; num=%u)\n", TRS_URMA_ARGS_CPY_WR_MAX_NUM, info->size, wr_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = trs_args_async_copy_info_check(info);
    if (ret != 0) {
        return ret;
    }

    for (i = 0; i < wr_num; i++) {
        urma_jfs_wr_t *wr = &urma_ctx->trs_jfs.wr_sge_array[i].wr;
        u32 size = (i == (wr_num - 1)) ? (info->size - i * (u32)TRS_URMA_WR_CPY_MAX_SIZE) :
                                         (u32)TRS_URMA_WR_CPY_MAX_SIZE;

        (void)memset_s(wr, sizeof(urma_jfs_wr_t), 0, sizeof(urma_jfs_wr_t));
        wr->opcode = URMA_OPC_WRITE;
        wr->tjetty = urma_ctx->tjetty;
        wr->flag.bs.place_order = TRS_URMA_WR_PLACE_ORDER; /* 2: strong order */
        wr->flag.bs.comp_order = 1;                        /* 1: Completion order with previous WR */
        wr->rw.src.sge = &urma_ctx->trs_jfs.wr_sge_array[i].src_sge;
        wr->rw.src.num_sge = 1;
        wr->rw.dst.sge = &urma_ctx->trs_jfs.wr_sge_array[i].dst_sge;
        wr->rw.dst.num_sge = 1;
        trs_set_urma_resp_cqe(wr);
        trs_jfs_write_sge_init(info->src + i * TRS_URMA_WR_CPY_MAX_SIZE, (uint64_t)size, info->srcTsegInfo->tseg_ptr,
                               wr->rw.src.sge);
        trs_jfs_write_sge_init(info->dst + i * TRS_URMA_WR_CPY_MAX_SIZE, (uint64_t)size, info->dstTsegInfo->tseg_ptr,
                               wr->rw.dst.sge);
        wr->next = (i == (wr_num - 1)) ? NULL : &urma_ctx->trs_jfs.wr_sge_array[i + 1].wr;
    }

    ret = trs_post_send_wr(sq_info, wr_num, &urma_ctx->trs_jfs.wr_sge_array[0].wr, false);
    if (ret != 0) {
        trs_warn_limit("Send sq task warn. (ret=%d; dev_id=%u; sq_id=%u)\n", ret, dev_id, info->sqId);
        return ret;
    }

    trs_debug("Sq args copy success. (dev_id=%u; sq_id=%u)\n", dev_id, info->sqId);

    return DRV_ERROR_NONE;
#endif
}

drvError_t trs_get_urma_tseg_info_by_va(uint32_t devid, uint64_t va, uint64_t size, uint32_t flag,
                                        struct halTsegInfo *tsegInfo)
{
    urma_target_seg_t *tseg = NULL;
    urma_seg_t seg;
    urma_token_t token;
    int ret = 0;

    ret = halMemGetSeg(devid, va, size, &seg, &token);
    if (ret != 0) {
        trs_err("Failed to get va segment. (devid=%u; va=0x%llx; size=%u; ret=%d)\n", devid, va, size, ret);
        return ret;
    }

    if (flag == 1) { /* local tseg */
        tseg = calloc(1, sizeof(urma_target_seg_t));
        if (tseg != NULL) {
            tseg->seg = seg;
        }
    } else {
        struct trs_urma_proc_ctx *urma_proc_ctx = trs_get_urma_proc_ctx(devid);
        if (urma_proc_ctx == NULL) {
            trs_err("Not opened. (devid=%u)\n", devid);
            return DRV_ERROR_INVALID_DEVICE;
        }

        tseg = urma_import_seg(urma_proc_ctx->urma_ctx, &seg, &token, 0, trs_get_seg_import_flag());
    }

    if (tseg == NULL) {
        trs_err("Failed to calloc or import tseg va. (devid=%u)\n", devid);
        return DRV_ERROR_INNER_ERR;
    }

    tsegInfo->tseg_ptr = tseg;
    tsegInfo->tseg_local_flag = flag;
    return 0;
}

drvError_t trs_put_urma_tseg_info(uint32_t devid, struct halTsegInfo *tsegInfo)
{
    (void)devid;

    if (tsegInfo->tseg_local_flag == 1) { /* local tseg */
        free(tsegInfo->tseg_ptr);
    } else { /* remote tseg */
        urma_unimport_seg(tsegInfo->tseg_ptr);
    }

    return 0;
}

static void *trs_urma_poll_thread_func(void *arg)
{
    uint32_t dev_id;
    int event_num;
    int i = 0;

    (void)arg;
    g_trs_urma_poll_thread_status = TRS_URMA_POLL_THREAD_STATUS_RUNNING;
    while (g_trs_urma_poll_thread_status == TRS_URMA_POLL_THREAD_STATUS_RUNNING) {
        do {
            event_num = epoll_wait(g_trs_urma_epoll.epfd, g_trs_urma_epoll.epoll_events, TRS_DEV_NUM, 5000); /* 5s */
        } while ((event_num == -1) && (errno == EINTR));

        if ((event_num < 0) || (event_num > TRS_DEV_NUM)) {
            trs_warn("Call epoll_wait error. (epfd=%d; strerror=\"%s\"; errno=%d)\n", g_trs_urma_epoll.epfd,
                     strerror(errno), errno);
            continue;
        }

        for (i = 0; i < event_num; i++) {
            dev_id = (uint32_t)(uintptr_t)g_trs_urma_epoll.epoll_events[i].data.ptr;
            if (dev_id >= TRS_DEV_NUM) {
                trs_warn("Invalid devid. (dev_id=%u)\n", dev_id);
                continue;
            }

            trs_dev_mutex_lock();
            if (trs_is_dev_open(dev_id) == false) {
                trs_dev_mutex_unlock();
                continue;
            }

            trs_urma_poll_proc_jfce(dev_id);
            trs_dev_mutex_unlock();
        }

        usleep(TRS_URMA_POLL_THREAD_IDLE_US);
    }

    return NULL;
}

static int trs_urma_cqe_poll_init(void)
{
    if (g_trs_urma_epoll.init_flag == false) {
        g_trs_urma_epoll.epoll_events = (struct epoll_event *)calloc(TRS_DEV_NUM, sizeof(struct epoll_event));
        if (g_trs_urma_epoll.epoll_events == NULL) {
            trs_err("Call malloc ep_event failed.\n");
            return DRV_ERROR_MALLOC_FAIL;
        }

        g_trs_urma_epoll.epfd = epoll_create(TRS_DEV_NUM);
        if (g_trs_urma_epoll.epfd < 0) {
            free(g_trs_urma_epoll.epoll_events);
            g_trs_urma_epoll.epoll_events = NULL;
            trs_err("Epoll create error. (strerror=\"%s\"; errno=%d)\n", strerror(errno), errno);
            return DRV_ERROR_INNER_ERR;
        }

        g_trs_urma_epoll.init_flag = true;
    }

    if (g_trs_urma_poll_thread_inited == false) {
        pthread_attr_t attr;
        int ret;

        (void)pthread_attr_init(&attr);
        (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        ret = pthread_create(&g_trs_urma_poll_thread, &attr, trs_urma_poll_thread_func, NULL);
        (void)pthread_attr_destroy(&attr);
        if (ret != 0) {
            trs_warn("Create urma poll thread warn. (ret=%d)\n", ret);
            return DRV_ERROR_INNER_ERR;
        }

        g_trs_urma_poll_thread_inited = true;
    }

    return 0;
}

int trs_urma_epoll_add_ctl(uint32_t dev_id)
{
    struct trs_urma_proc_ctx *urma_proc_ctx = trs_get_urma_proc_ctx(dev_id);
    struct epoll_event ep_event;
    int ret;

    ep_event.data.ptr = (void *)(uintptr_t)dev_id;
    ep_event.events = EPOLLIN;
    ret = epoll_ctl((signed int)g_trs_urma_epoll.epfd, EPOLL_CTL_ADD, urma_proc_ctx->jfce->fd, &ep_event);
    if (ret != 0) {
#ifndef EMU_ST
        trs_err("Call epoll_ctl for data_in failed. (ret=%d; dev_id=%d)\n", ret, dev_id);
        return DRV_ERROR_INNER_ERR;
#endif
    }

    return 0;
}

void trs_urma_epoll_del_ctl(uint32_t dev_id)
{
    struct trs_urma_proc_ctx *urma_proc_ctx = trs_get_urma_proc_ctx(dev_id);
    struct epoll_event ep_event;

    ep_event.data.ptr = (void *)(uintptr_t)dev_id;
    (void)epoll_ctl((signed int)g_trs_urma_epoll.epfd, EPOLL_CTL_DEL, urma_proc_ctx->jfce->fd, &ep_event);
}

int trs_urma_cqe_poll_init_dev(uint32_t dev_id)
{
    int ret = 0;

    ret = trs_urma_cqe_poll_init();
    if (ret != 0) {
        return ret;
    }

    return trs_urma_epoll_add_ctl(dev_id);
}

void trs_urma_cqe_poll_uninit_dev(uint32_t dev_id)
{
    trs_urma_epoll_del_ctl(dev_id);
}
