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
#include "urma_api.h"
#include "ascend_hal_error.h"
#include "uref.h"
#include "queue.h"
#include "que_compiler.h"
#include "dms_user_interface.h"
#include "queue_interface.h"
#include "que_urma.h"
#include "comm_user_interface.h"
#include "que_platform_ub.h"

#define QUE_URMA_MAX_DEVNUM MAX_DEVICE

static struct que_urma_ctx *g_urma_ctx[QUE_URMA_MAX_DEVNUM] = {NULL};
static pthread_mutex_t g_urma_ctx_lock = PTHREAD_MUTEX_INITIALIZER;

static inline void que_urma_ctx_lock(void)
{
    (void)pthread_mutex_lock(&g_urma_ctx_lock);
}

static inline void que_urma_ctx_unlock(void)
{
    (void)pthread_mutex_unlock(&g_urma_ctx_lock);
}

static bool que_urma_eid_equal(const urma_eid_t *lhs, const urma_eid_t *rhs)
{
    return memcmp(lhs->raw, rhs->raw, sizeof(lhs->raw)) == 0;
}

static inline void que_urma_tp_list_lock(struct que_urma_ctx *urma_ctx)
{
    (void)pthread_mutex_lock(&urma_ctx->tp_list_lock);
}

static inline void que_urma_tp_list_unlock(struct que_urma_ctx *urma_ctx)
{
    (void)pthread_mutex_unlock(&urma_ctx->tp_list_lock);
}

static unsigned int que_devid_to_urma_devid(unsigned int devid)
{
    return devid;
}

static struct que_urma_ctx *_que_urma_ctx_create(unsigned int urma_devid)
{
    struct que_urma_ctx *urma_ctx = NULL;

    urma_ctx = calloc(1, sizeof(struct que_urma_ctx));
    if (que_unlikely(urma_ctx == NULL)) {
        return NULL;
    }
    urma_ctx->page_size = (size_t)getpagesize();
    urma_ctx->urma_devid = urma_devid;
    uref_init(&urma_ctx->ref);
    if (pthread_mutex_init(&urma_ctx->tp_list_lock, NULL) != 0) {
        free(urma_ctx);
        return NULL;
    }

    return urma_ctx;
}

static inline void _que_urma_ctx_destroy(struct que_urma_ctx *urma_ctx)
{
    (void)pthread_mutex_destroy(&urma_ctx->tp_list_lock);
    free(urma_ctx);
}

static int que_get_trans_num(void)
{
    return que_get_trans_num_platform();
}

static void que_urma_dev_eid_uninit(struct que_urma_ctx *urma_ctx)
{
    int trans_type, trans_num;
    trans_num = que_get_trans_num();
    for (trans_type = TRANS_D2H_H2D; trans_type < trans_num; trans_type++) {
        urma_ctx->urma_dev[trans_type] = NULL;
    }
}

static int que_urma_dev_eid_init(struct que_urma_ctx *urma_ctx)
{
    int num, trans_num, ret = 0;
    int trans_idx;
    struct dms_ub_dev_info dev_info[TRANS_TYPE_MAX];
#ifndef EMU_ST
    trans_num = que_get_trans_num();
    ret = dms_get_ub_dev_info(urma_ctx->urma_devid, &dev_info[TRANS_D2H_H2D], &num);
    if (que_unlikely((ret != 0) || (num == 0))) {
        QUEUE_LOG_ERR("que dms get ub dev info failed.(dev_id=%u; ret=%d; num=%d)\n", urma_ctx->urma_devid, ret, num);
        return ret;
    }

    ret = que_get_ub_d2d_dev_info_platform(urma_ctx->urma_devid, dev_info, ret);
    if (que_unlikely(ret != 0)) {
        QUEUE_LOG_ERR("que dms get ub d2d dev info failed.(dev_id=%u; ret=%d)\n", urma_ctx->urma_devid, ret);
        return ret;
    }
    for (trans_idx = TRANS_D2H_H2D; trans_idx < trans_num; trans_idx++) {
        urma_ctx->urma_dev[trans_idx] = dev_info[trans_idx].urma_dev[0];
        urma_ctx->eid_index[trans_idx] = dev_info[trans_idx].eid_index[0];
    }
#endif

    return DRV_ERROR_NONE;
}

static int que_urma_token_init(struct que_urma_ctx *urma_ctx)
{
    int ret, trans_idx, trans_num;
    unsigned int token_val;
    urma_token_id_t *token_id[TRANS_TYPE_MAX] = {0};
    trans_num = que_get_trans_num();
    for (trans_idx = TRANS_D2H_H2D; trans_idx < trans_num; trans_idx++) {
        token_id[trans_idx] = urma_alloc_token_id(urma_ctx->context[trans_idx]);
        if (que_unlikely(token_id[trans_idx] == NULL)) {
            goto cleanup;
        }
    }

#ifndef EMU_ST
    ret = dms_get_token_val(urma_ctx->urma_devid, SHARED_TOKEN_VAL, &token_val);
    if (ret != 0) {
        QUEUE_LOG_ERR("Failed to get token_val. (ret=%d, devid=%u)\n", ret, urma_ctx->urma_devid);
        goto cleanup;
    }
#endif
    for (trans_idx = TRANS_D2H_H2D; trans_idx < trans_num; trans_idx++) {
        urma_ctx->token_info[trans_idx].token_id = token_id[trans_idx];
        urma_ctx->token_info[trans_idx].token.token = token_val;
#ifndef EMU_ST
        ret = que_register_share_urma_seg(urma_ctx, trans_idx);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que register share urma seg failed. (ret=%d, devid=%u, trans_idx=%d)\n", ret,
                          urma_ctx->urma_devid, trans_idx);
            goto cleanup;
        }
#endif
    }

    return DRV_ERROR_NONE;

cleanup:
    for (trans_idx = TRANS_D2H_H2D; trans_idx < trans_num; trans_idx++) {
        if (token_id[trans_idx] != NULL) {
            urma_free_token_id(token_id[trans_idx]);
        }
    }
    return DRV_ERROR_INNER_ERR;
}

static void que_urma_token_uninit(struct que_urma_ctx *urma_ctx)
{
    int trans_idx, trans_num;
    trans_num = que_get_trans_num();
    for (trans_idx = TRANS_D2H_H2D; trans_idx < trans_num; trans_idx++) {
#ifndef EMU_ST
        que_unregister_share_urma_seg(urma_ctx->tseg[trans_idx]);
#endif
        if (urma_ctx->token_info[trans_idx].token_id != NULL) {
            urma_status_t status = urma_free_token_id(urma_ctx->token_info[trans_idx].token_id);
            if (que_unlikely(status != URMA_SUCCESS)) {
                QUEUE_LOG_WARN("urma free token id abnormal. (urma_devid=%u; trans_idx=%d; status=%d)\n",
                               urma_ctx->urma_devid, trans_idx, status);
            }
            urma_ctx->token_info[trans_idx].token_id = NULL;
        }
        urma_ctx->token_info[trans_idx].token.token = 0;
    }
    return;
}

static int que_urma_context_init(struct que_urma_ctx *urma_ctx)
{
    urma_context_t *context = NULL;
    int trans_type, trans_num, ret;
    trans_num = que_get_trans_num();
    for (trans_type = TRANS_D2H_H2D; trans_type < trans_num; trans_type++) {
        context = ascend_urma_create_context(urma_ctx->urma_dev[trans_type], urma_ctx->eid_index[trans_type]);
        if (que_unlikely(context == NULL)) {
            QUEUE_LOG_ERR("urma context create fail. (urma_devid=%u; eid_index=%u)\n", urma_ctx->urma_devid,
                          urma_ctx->eid_index[trans_type]);
            ret = DRV_ERROR_INNER_ERR;
            goto context_clean;
        }
        urma_ctx->context[trans_type] = context;
    }

    return DRV_ERROR_NONE;

context_clean:
    for (trans_type = TRANS_D2H_H2D; trans_type < trans_num; trans_type++) {
        if (urma_ctx->context[trans_type] != NULL) {
            (void)ascend_urma_delete_context(urma_ctx->context[trans_type]);
            urma_ctx->context[trans_type] = NULL;
        }
    }
    return ret;
}

static void que_urma_context_uninit(struct que_urma_ctx *urma_ctx)
{
    int trans_type, trans_num;
    trans_num = que_get_trans_num();
    for (trans_type = TRANS_D2H_H2D; trans_type < trans_num; trans_type++) {
        if (urma_ctx->context[trans_type] != NULL) {
            urma_status_t status = urma_delete_context(urma_ctx->context[trans_type]);
            if (que_unlikely(status != URMA_SUCCESS)) {
                QUEUE_LOG_WARN("urma context delete abnormal. (urma_devid=%u; status=%d)\n", urma_ctx->urma_devid,
                               status);
            }
            urma_ctx->context[trans_type] = NULL;
        }
    }
}

static int que_urma_ctx_init(struct que_urma_ctx *urma_ctx)
{
    int ret;
    ret = que_urma_dev_eid_init(urma_ctx);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("urma dev and eid init fail. (ret=%d; urma_devid=%u)\n", ret, urma_ctx->urma_devid);
        return ret;
    }

    ret = que_urma_context_init(urma_ctx);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("urma context init fail. (ret=%d; urma_devid=%u)\n", ret, urma_ctx->urma_devid);
        goto err_context_init;
    }

    ret = que_urma_token_init(urma_ctx);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("urma token init fail. (ret=%d; urma_devid=%u)\n", ret, urma_ctx->urma_devid);
        goto err_token_init;
    }

    return DRV_ERROR_NONE;
err_token_init:
    que_urma_context_uninit(urma_ctx);
err_context_init:
    que_urma_dev_eid_uninit(urma_ctx);
    return ret;
}

static void que_urma_ctx_uninit(struct que_urma_ctx *urma_ctx)
{
    que_urma_token_uninit(urma_ctx);
    que_urma_context_uninit(urma_ctx);
    que_urma_dev_eid_uninit(urma_ctx);
}

static int _que_urma_ctx_add(struct que_urma_ctx *urma_ctx)
{
    if (que_unlikely(g_urma_ctx[urma_ctx->urma_devid] != NULL)) {
        return DRV_ERROR_REPEATED_INIT;
    }
    g_urma_ctx[urma_ctx->urma_devid] = urma_ctx;
    return DRV_ERROR_NONE;
}

static void _que_urma_ctx_del(struct que_urma_ctx *urma_ctx)
{
    if (que_likely(g_urma_ctx[urma_ctx->urma_devid] == urma_ctx)) {
        g_urma_ctx[urma_ctx->urma_devid] = NULL;
    }
}

struct que_urma_ctx *que_urma_ctx_create(unsigned int urma_devid)
{
    struct que_urma_ctx *urma_ctx = NULL;
    int ret;

    urma_ctx = _que_urma_ctx_create(urma_devid);
    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("urma ctx create fail. (urma_devid=%u)\n", urma_devid);
        return NULL;
    }

    ret = que_urma_ctx_init(urma_ctx);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        _que_urma_ctx_destroy(urma_ctx);
        QUEUE_LOG_ERR("urma ctx init fail. (urma_devid=%u)\n", urma_devid);
        return NULL;
    }

    ret = _que_urma_ctx_add(urma_ctx);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        que_urma_ctx_uninit(urma_ctx);
        _que_urma_ctx_destroy(urma_ctx);
        return NULL;
    }

    return urma_ctx;
}

void que_urma_ctx_destroy(struct que_urma_ctx *urma_ctx)
{
    _que_urma_ctx_del(urma_ctx);
    que_urma_ctx_uninit(urma_ctx);
    _que_urma_ctx_destroy(urma_ctx);
}

static inline struct que_urma_ctx *que_urma_ctx_find(unsigned int urma_devid)
{
    return g_urma_ctx[urma_devid];
}

static void que_urma_ctx_release(struct uref *uref)
{
    struct que_urma_ctx *urma_ctx = container_of(uref, struct que_urma_ctx, ref);
    que_urma_ctx_destroy(urma_ctx);
}

static struct que_urma_ctx *que_urma_ctx_get(unsigned int devid)
{
    struct que_urma_ctx *urma_ctx = NULL;
    unsigned int urma_devid;

    urma_devid = que_devid_to_urma_devid(devid);
    if (que_unlikely(urma_devid >= QUE_URMA_MAX_DEVNUM)) {
        QUEUE_LOG_ERR("devid to udmaid fail. (devid=%u; urma_devid=%u)\n", devid, urma_devid);
        return NULL;
    }

    que_urma_ctx_lock();
    urma_ctx = que_urma_ctx_find(urma_devid);
    if (urma_ctx != NULL) {
        if (uref_get_unless_zero(&urma_ctx->ref) == 0) {
            que_urma_ctx_unlock();
            return NULL;
        }
    } else {
        urma_ctx = que_urma_ctx_find(urma_devid);
        if (urma_ctx == NULL) {
            urma_ctx = que_urma_ctx_create(urma_devid);
            if (urma_ctx != NULL) {
                if (uref_get_unless_zero(&urma_ctx->ref) == 0) {
                    que_urma_ctx_unlock();
                    return NULL;
                }
            }
        } else {
            if (uref_get_unless_zero(&urma_ctx->ref) == 0) {
                que_urma_ctx_unlock();
                return NULL;
            }
        }
    }
    que_urma_ctx_unlock();

    return urma_ctx;
}

static void que_urma_ctx_put(struct que_urma_ctx *urma_ctx)
{
    uref_put(&urma_ctx->ref, que_urma_ctx_release);
}

urma_target_seg_t *que_get_urma_ctx_tseg(unsigned int devid, unsigned int d2d_flag)
{
    urma_target_seg_t *tseg = NULL;
    struct que_urma_ctx *urma_ctx = que_urma_ctx_get(devid);

    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("get urma ctx fail. (devid=%u)\n", devid);
        return NULL;
    }
    tseg = urma_ctx->tseg[d2d_flag];
    que_urma_ctx_put(urma_ctx);

    return tseg;
}

int que_fill_ctx_token(unsigned int devid, urma_token_t *token, unsigned int d2d_flag)
{
    struct que_urma_ctx *urma_ctx = NULL;
    urma_ctx = que_urma_ctx_get(devid);
    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("get urma ctx fail. (devid=%u)\n", devid);
        return DRV_ERROR_INNER_ERR;
    }
    token->token = urma_ctx->token_info[d2d_flag].token.token;
    que_urma_ctx_put(urma_ctx);
    return DRV_ERROR_NONE;
}

int que_urma_token_alloc(unsigned int devid, struct que_urma_token *token_info, unsigned int d2d_flag)
{
    struct que_urma_ctx *urma_ctx = que_urma_ctx_get(devid);
    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("get urma ctx fail. (devid=%u)\n", devid);
        return DRV_ERROR_UNINIT;
    }

    urma_token_id_t *token_id = urma_alloc_token_id(urma_ctx->context[d2d_flag]);
    if (que_unlikely(token_id == NULL)) {
        que_urma_ctx_put(urma_ctx);
        return DRV_ERROR_INNER_ERR;
    }
    token_info->token_id = token_id;
    token_info->token.token = urma_ctx->token_info[d2d_flag].token.token;
    que_urma_ctx_put(urma_ctx);
    return DRV_ERROR_NONE;
}

void que_urma_token_free(unsigned int devid, struct que_urma_token *token_info)
{
    if (token_info->token_id == NULL) {
        token_info->token.token = 0;
        return;
    }
    urma_status_t status = urma_free_token_id(token_info->token_id);
    if (que_unlikely(status != URMA_SUCCESS)) {
        QUEUE_LOG_WARN("urma free token id abnormal. (devid=%u; status=%d)\n", devid, status);
    }
    token_info->token_id = NULL;
    token_info->token.token = 0;
    return;
}

#ifndef EMU_ST
void que_urma_ctx_put_ex(unsigned int devid)
{
    struct que_urma_ctx *urma_ctx = NULL;
    unsigned int urma_devid;

    urma_devid = que_devid_to_urma_devid(devid);
    if (que_unlikely(urma_devid >= QUE_URMA_MAX_DEVNUM)) {
        QUEUE_LOG_ERR("devid to udmaid fail. (devid=%u; urma_devid=%u)\n", devid, urma_devid);
        return;
    }
    que_urma_ctx_lock();
    urma_ctx = que_urma_ctx_find(urma_devid);
    if (urma_ctx != NULL) {
        que_urma_ctx_put(urma_ctx);
    }
    que_urma_ctx_unlock();
}
#endif

urma_jfce_t *que_urma_jfce_create(unsigned int devid, unsigned int d2d_flag)
{
    struct que_urma_ctx *urma_ctx = NULL;
    urma_jfce_t *jfce = NULL;

    urma_ctx = que_urma_ctx_get(devid);
    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("get urma ctx fail. (devid=%u)\n", devid);
        return NULL;
    }
    jfce = urma_create_jfce(urma_ctx->context[d2d_flag]);
    que_urma_ctx_put(urma_ctx);

    return jfce;
}

void que_urma_jfce_destroy(urma_jfce_t *jfce)
{
    (void)urma_delete_jfce(jfce);
}

urma_jfc_t *que_urma_jfc_create(unsigned int devid, urma_jfc_cfg_t *cfg, unsigned int d2d_flag)
{
    struct que_urma_ctx *urma_ctx = NULL;
    urma_jfc_t *jfc = NULL;
    urma_status_t status;

    urma_ctx = que_urma_ctx_get(devid);
    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("get urma ctx fail. (devid=%u)\n", devid);
        return NULL;
    }
    jfc = urma_create_jfc(urma_ctx->context[d2d_flag], cfg);
    if (que_unlikely(jfc == NULL)) {
        QUEUE_LOG_ERR("create jfc fail. (devid=%u)\n", devid);
        goto err_create_jfc;
    }
    status = urma_rearm_jfc(jfc, false);
    if (que_unlikely(status != URMA_SUCCESS)) {
        QUEUE_LOG_ERR("rearm jfc fail. (status=%d; devid=%u)\n", status, devid);
        goto err_rearm_jfc;
    }
    que_urma_ctx_put(urma_ctx);
    return jfc;

err_rearm_jfc:
    urma_delete_jfc(jfc);
err_create_jfc:
    que_urma_ctx_put(urma_ctx);
    return NULL;
}

void que_urma_jfc_destroy(urma_jfc_t *jfc)
{
    (void)urma_delete_jfc(jfc);
}

urma_jfs_t *que_urma_jfs_create(unsigned int devid, urma_jfs_cfg_t *cfg, unsigned int d2d_flag)
{
    struct que_urma_ctx *urma_ctx = NULL;
    urma_jfs_t *jfs = NULL;

    urma_ctx = que_urma_ctx_get(devid);
    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("get urma ctx fail. (devid=%u)\n", devid);
        return NULL;
    }

    jfs = urma_create_jfs(urma_ctx->context[d2d_flag], cfg);
    que_urma_ctx_put(urma_ctx);
    if (que_unlikely(jfs == NULL)) {
        QUEUE_LOG_ERR("urma create jfs fail. (devid=%u)\n", devid);
    }

    return jfs;
}

void que_urma_jfs_destroy(urma_jfs_t *jfs)
{
    urma_status_t status = urma_delete_jfs(jfs);
    if (que_unlikely(status != URMA_SUCCESS)) {
        QUEUE_LOG_WARN("delete jfs abnormal. (status=%d)\n", status);
    }
}

urma_jfr_t *que_urma_jfr_create(unsigned int devid, urma_jfr_cfg_t *cfg, unsigned int d2d_flag)
{
    struct que_urma_ctx *urma_ctx = NULL;
    urma_jfr_t *jfr = NULL;

    urma_ctx = que_urma_ctx_get(devid);
    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("get urma ctx fail. (devid=%u)\n", devid);
        return NULL;
    }

    cfg->token_value = urma_ctx->token_info[d2d_flag].token;
    jfr = urma_create_jfr(urma_ctx->context[d2d_flag], cfg);
    que_urma_ctx_put(urma_ctx);
    if (que_unlikely(jfr == NULL)) {
        QUEUE_LOG_ERR("urma create jfr fail. (devid=%u)\n", devid);
    }

    return jfr;
}

void que_urma_jfr_destroy(urma_jfr_t *jfr)
{
    urma_status_t status = urma_delete_jfr(jfr);
    if (que_unlikely(status != URMA_SUCCESS)) {
        QUEUE_LOG_WARN("delete jfr abnormal. (status=%d)\n", status);
    }
}

urma_target_jetty_t *que_jfr_import(unsigned int devid, urma_jfr_id_t *jfr_id, urma_token_t *token,
                                    unsigned int d2d_flag)
{
#ifdef SSAPI_USE_MAMI
    uint32_t tp_cnt = 1;
    urma_tp_info_t tpid_info = {0};
    urma_status_t status;
    urma_active_tp_cfg_t active_tp_cfg = {0};
#endif
    urma_rjfr_t rjfr = {.jfr_id = *jfr_id, .trans_mode = URMA_TM_RM, .flag.bs.token_policy = URMA_TOKEN_PLAIN_TEXT};
    struct que_urma_ctx *urma_ctx = NULL;
    urma_target_jetty_t *tjetty = NULL;

    urma_ctx = que_urma_ctx_get(devid);
    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("get urma ctx fail. (devid=%u)\n", devid);
        return NULL;
    }

#ifdef SSAPI_USE_MAMI
    urma_get_tp_cfg_t tpid_cfg = {
        .trans_mode = URMA_TM_RM,
        .local_eid = urma_ctx->context[d2d_flag]->eid, // local_eid
        .peer_eid = jfr_id->eid,                       // remote_eid
        .flag.bs.ctp = true,
    };

    que_urma_tp_list_lock(urma_ctx);
    if (urma_ctx->tp_list_fetched[d2d_flag] && que_urma_eid_equal(&urma_ctx->tp_peer_eid[d2d_flag], &jfr_id->eid)) {
        tpid_info = urma_ctx->stored_tpid_info[d2d_flag];
    } else {
        status = ascend_urma_get_tp_list(urma_ctx->context[d2d_flag], &tpid_cfg, &tp_cnt, &tpid_info);
        if ((status != 0) || (tp_cnt == 0)) {
            que_urma_tp_list_unlock(urma_ctx);
            QUEUE_LOG_ERR("urma get tp list failed (status=%d; tp_cnt=%u)\n", status, tp_cnt);
            que_urma_ctx_put(urma_ctx);
            return tjetty;
        }
        urma_ctx->tp_list_fetched[d2d_flag] = true;
        urma_ctx->tp_peer_eid[d2d_flag] = jfr_id->eid;
        urma_ctx->stored_tpid_info[d2d_flag] = tpid_info;
    }
    que_urma_tp_list_unlock(urma_ctx);

    active_tp_cfg.tp_handle = tpid_info.tp_handle;
    rjfr.tp_type = URMA_CTP;
    tjetty = ascend_urma_import_jfr_ex(urma_ctx->context[d2d_flag], &rjfr, token, &active_tp_cfg);
#else
    tjetty = urma_import_jfr(urma_ctx->context[d2d_flag], &rjfr, token);
#endif
    que_urma_ctx_put(urma_ctx);
    return tjetty;
}

void que_jfr_unimport(urma_target_jetty_t *tjetty)
{
    urma_status_t status = urma_unimport_jfr(tjetty);
    if (que_unlikely(status != URMA_SUCCESS)) {
        QUEUE_LOG_WARN("unimport jfr abnormal. (status=%d)\n", status);
    }
}

static void que_seg_cfg_pack_template(struct que_urma_ctx *urma_ctx, unsigned long long va, unsigned long long size,
                                      unsigned int access, unsigned int pin_flag, urma_seg_cfg_t *seg_cfg,
                                      unsigned int d2d_flag)
{
    static urma_reg_seg_flag_t flag = {0};

    flag.bs.cacheable = (uint32_t)URMA_NON_CACHEABLE & 1u;
    flag.bs.access = (unsigned int)access & 0x3Fu;
    flag.bs.token_id_valid = (uint32_t)URMA_TOKEN_ID_VALID & 1u;
    flag.bs.token_policy = (uint32_t)URMA_TOKEN_PLAIN_TEXT & 7u;
    flag.bs.non_pin = ((pin_flag == QUE_PIN) ? 0u : 1u) & 1u; /* segment pages: 0-pinned; 1-non-pinned */
    seg_cfg->va = que_align_down(va, urma_ctx->page_size);
    seg_cfg->len = que_align_up(va + size, urma_ctx->page_size) - seg_cfg->va;
    seg_cfg->token_value = urma_ctx->token_info[d2d_flag].token;
    seg_cfg->token_id = urma_ctx->token_info[d2d_flag].token_id;
    seg_cfg->flag = flag;
}

static void que_update_seg_cfg_token(struct que_urma_token *token, urma_seg_cfg_t *seg_cfg)
{
    if (que_unlikely(token == NULL)) {
        return;
    }
    if (que_unlikely(token->token_id == NULL)) {
        return;
    }
    seg_cfg->token_id = token->token_id;
    seg_cfg->token_value = token->token;
}

urma_target_seg_t *que_pin_seg_create(unsigned int devid, unsigned long long va, unsigned long long size,
                                      unsigned int access, struct que_urma_token *token, unsigned int d2d_flag)
{
    struct que_urma_ctx *urma_ctx = NULL;
    urma_target_seg_t *tseg = NULL;
    urma_seg_cfg_t seg_cfg = {0};

    urma_ctx = que_urma_ctx_get(devid);
    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("get urma ctx fail. (devid=%u)\n", devid);
        return NULL;
    }
    que_seg_cfg_pack_template(urma_ctx, va, size, access, QUE_PIN, &seg_cfg, d2d_flag);
    que_update_seg_cfg_token(token, &seg_cfg);
    tseg = urma_register_seg(urma_ctx->context[d2d_flag], &seg_cfg);
    que_urma_ctx_put(urma_ctx);

    return tseg;
}

urma_target_seg_t *que_nonpin_seg_create(unsigned int devid, unsigned long long va, unsigned long long size,
                                         unsigned int access, struct que_urma_token *token, unsigned int d2d_flag)
{
    struct que_urma_ctx *urma_ctx = NULL;
    urma_target_seg_t *tseg = NULL;
    urma_seg_cfg_t seg_cfg = {0};

    urma_ctx = que_urma_ctx_get(devid);
    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("get urma ctx fail. (devid=%u)\n", devid);
        return NULL;
    }
    que_seg_cfg_pack_template(urma_ctx, va, size, access, QUE_NON_PIN, &seg_cfg, d2d_flag);
    que_update_seg_cfg_token(token, &seg_cfg);
    tseg = urma_register_seg(urma_ctx->context[d2d_flag], &seg_cfg);
    que_urma_ctx_put(urma_ctx);

    return tseg;
}

void que_seg_destroy(urma_target_seg_t *tseg)
{
    (void)urma_unregister_seg(tseg);
}

urma_target_seg_t *que_seg_import(unsigned int devid, unsigned int d2d_flag, struct urma_seg *seg, unsigned int access,
                                  urma_token_t *token)
{
    struct que_urma_ctx *urma_ctx = NULL;
    urma_import_seg_flag_t flag = {0};

    flag.bs.cacheable = (uint32_t)URMA_NON_CACHEABLE & 1u;
    flag.bs.access = (unsigned int)access & 0x3Fu;
    flag.bs.mapping = (uint32_t)URMA_SEG_NOMAP & 1u;
    flag.bs.reserved = 0u;
    urma_target_seg_t *tseg = NULL;

    urma_ctx = que_urma_ctx_get(devid);
    if (que_unlikely(urma_ctx == NULL)) {
        QUEUE_LOG_ERR("get urma ctx fail. (devid=%u)\n", devid);
        return NULL;
    }

    tseg = urma_import_seg(urma_ctx->context[d2d_flag], seg, token, 0, flag);
    que_urma_ctx_put(urma_ctx);

    return tseg;
}

void que_seg_unimport(urma_target_seg_t *tseg)
{
    urma_status_t status = urma_unimport_seg(tseg);
    if (que_unlikely(status != URMA_SUCCESS)) {
        QUEUE_LOG_WARN("unimport seg abnormal. (status=%d)\n", status);
    }
}

struct que_jfs_rw_wr *que_jfs_rw_wr_create(struct que_jfs_rw_wr_attr *attr)
{
    urma_sge_t *src_sge = NULL, *dst_sge = NULL;
    struct que_jfs_rw_wr *rw_wr = NULL;
    urma_jfs_wr_t *wr = NULL;
    unsigned int i;

    rw_wr = (struct que_jfs_rw_wr *)malloc(sizeof(struct que_jfs_rw_wr));
    if (que_unlikely(rw_wr == NULL)) {
        return NULL;
    }

    wr = calloc(attr->wr_num, sizeof(urma_jfs_wr_t));
    if (que_unlikely(wr == NULL)) {
        free(rw_wr);
        return NULL;
    }

    src_sge = calloc(attr->wr_num, sizeof(urma_sge_t));
    if (que_unlikely(src_sge == NULL)) {
        free(wr);
        free(rw_wr);
        return NULL;
    }

    dst_sge = calloc(attr->wr_num, sizeof(urma_sge_t));
    if (que_unlikely(dst_sge == NULL)) {
        free(src_sge);
        free(wr);
        free(rw_wr);
        return NULL;
    }

    for (i = 0; i < attr->wr_num; i++) {
        wr[i].flag.bs.complete_enable = URMA_COMPLETE_ENABLE;
        wr[i].opcode = attr->opcode;
        wr[i].rw.src.sge = &src_sge[i];
        wr[i].rw.src.num_sge = 1;
        wr[i].rw.dst.sge = &dst_sge[i];
        wr[i].rw.dst.num_sge = 1;
        wr[i].next = NULL;
    }
    rw_wr->attr = *attr;
    rw_wr->wr = wr;
    return rw_wr;
}

void que_jfs_rw_wr_destroy(struct que_jfs_rw_wr *rw_wr)
{
    if (rw_wr == NULL) {
        return;
    }

    if (rw_wr->wr != NULL) {
        if (rw_wr->wr[0].rw.src.sge != NULL) {
            free(rw_wr->wr[0].rw.src.sge);
            rw_wr->wr[0].rw.src.sge = NULL;
        }

        if (rw_wr->wr[0].rw.dst.sge != NULL) {
            free(rw_wr->wr[0].rw.dst.sge);
            rw_wr->wr[0].rw.dst.sge = NULL;
        }

        free(rw_wr->wr);
        rw_wr->wr = NULL;
    }
    free(rw_wr);
}
