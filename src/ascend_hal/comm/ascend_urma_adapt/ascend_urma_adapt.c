/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdlib.h>
#include <string.h>
#include <securec.h>
#include <pthread.h>

#include "urma_api.h"
#include "urma_types.h"

#include "drv_user_common.h"
#include "ascend_urma_pub.h"
#include "ascend_urma_log.h"
#include "ascend_urma_adapt.h"

/* ======================== urma_ctx management ======================== */
static pthread_mutex_t g_urma_ctx_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct list_head g_urma_ctx_list = LIST_HEAD_INIT(g_urma_ctx_list);

static bool ascend_urma_cxt_info_match(const struct ascend_urma_cxt_info *info, const char *dev_name, u32 eid_index)
{
    if (strncmp(info->urma_dev->name, dev_name, URMA_MAX_NAME) != 0) {
        return false;
    }
    return (info->eid_index == eid_index);
}

/* Create or reuse a urma context for the given device and eid index */
urma_context_t *ascend_urma_create_context(urma_device_t *dev, uint32_t eid_index)
{
    struct ascend_urma_cxt_info *info = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    char urma_dev_name[URMA_MAX_NAME];

    if (dev == NULL) {
        ascend_urma_err("Urma dev is NULL.\n");
        return NULL;
    }

    (void)pthread_mutex_lock(&g_urma_ctx_mutex);
    list_for_each_safe(pos, n, &g_urma_ctx_list)
    {
        info = list_entry(pos, struct ascend_urma_cxt_info, node);
        if (ascend_urma_cxt_info_match(info, dev->name, eid_index)) {
            info->cnt++;
            (void)pthread_mutex_unlock(&g_urma_ctx_mutex);
            return info->urma_ctx;
        }
    }

    info = calloc(1, sizeof(struct ascend_urma_cxt_info));
    if (info == NULL) {
        ascend_urma_err("Calloc failed. (size=%llu)\n", (u64)sizeof(struct ascend_urma_cxt_info));
        (void)pthread_mutex_unlock(&g_urma_ctx_mutex);
        return NULL;
    }

    INIT_LIST_HEAD(&info->node);
    info->urma_dev = dev;
    info->eid_index = eid_index;
    info->urma_ctx = urma_create_context(dev, eid_index);
    if (info->urma_ctx == NULL) {
        (void)memcpy_s(urma_dev_name, URMA_MAX_NAME, dev->name, URMA_MAX_NAME);
        urma_dev_name[URMA_MAX_NAME - 1] = '\0';
        ascend_urma_err("Urma create context failed. (dev=%s; eid_index=%u)\n", urma_dev_name, eid_index);
        free(info);
        (void)pthread_mutex_unlock(&g_urma_ctx_mutex);
        return NULL;
    }
    info->cnt = 1;

    drv_user_list_add_tail(&info->node, &g_urma_ctx_list);
    (void)pthread_mutex_unlock(&g_urma_ctx_mutex);

    return info->urma_ctx;
}

/* Decrease the reference count of the urma context */
urma_status_t ascend_urma_delete_context(urma_context_t *urma_ctx)
{
    struct ascend_urma_cxt_info *info = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;

    if (urma_ctx == NULL) {
        ascend_urma_err("Urma ctx is NULL.\n");
        return URMA_EINVAL;
    }

    (void)pthread_mutex_lock(&g_urma_ctx_mutex);
    list_for_each_safe(pos, n, &g_urma_ctx_list)
    {
        info = list_entry(pos, struct ascend_urma_cxt_info, node);
        if (info->urma_ctx == urma_ctx) {
            if (info->cnt > 0) {
                info->cnt--;
            }
            (void)pthread_mutex_unlock(&g_urma_ctx_mutex);
            return URMA_SUCCESS;
        }
    }
    (void)pthread_mutex_unlock(&g_urma_ctx_mutex);

    return URMA_EINVAL;
}

/* ======================== tp info management ======================== */
static pthread_mutex_t g_tp_info_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct list_head g_tp_info_list = LIST_HEAD_INIT(g_tp_info_list);

static bool ascend_tp_info_eid_equal(const urma_eid_t *eid1, const urma_eid_t *eid2)
{
    return (memcmp(eid1->raw, eid2->raw, URMA_EID_SIZE) == 0);
}

static struct ascend_tp_info *ascend_tp_info_find_by_eid(const urma_eid_t *local_eid, const urma_eid_t *remote_eid)
{
    struct ascend_tp_info *info = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;

    list_for_each_safe(pos, n, &g_tp_info_list)
    {
        info = list_entry(pos, struct ascend_tp_info, node);
        if (ascend_tp_info_eid_equal(&info->local_eid, local_eid) &&
            ascend_tp_info_eid_equal(&info->remote_eid, remote_eid)) {
            return info;
        }
    }
    return NULL;
}

static struct ascend_tp_info *ascend_tp_info_create(const urma_eid_t *local_eid, const urma_eid_t *remote_eid,
                                                    const urma_tp_info_t *tpid_info)
{
    struct ascend_tp_info *info = NULL;

    info = calloc(1, sizeof(struct ascend_tp_info));
    if (info == NULL) {
        ascend_urma_err("Calloc failed. (size=%llu)\n", (u64)sizeof(struct ascend_tp_info));
        return NULL;
    }

    INIT_LIST_HEAD(&info->node);
    info->local_eid = *local_eid;
    info->remote_eid = *remote_eid;
    info->tpid_info = *tpid_info;
    info->ctp_used_flag = false;
    info->cnt = 0;

    drv_user_list_add_tail(&info->node, &g_tp_info_list);
    return info;
}

static int _ascend_urma_get_tp_list(urma_context_t *ctx, urma_get_tp_cfg_t *cfg, uint32_t *tp_cnt,
                                    urma_tp_info_t *tpid_info, struct ascend_tp_info **out_info)
{
    struct ascend_tp_info *info = NULL;
    urma_status_t status;

    info = ascend_tp_info_find_by_eid(&cfg->local_eid, &cfg->peer_eid);
    if (info != NULL) {
        *tp_cnt = 1;
        *tpid_info = info->tpid_info;
        *out_info = info;
        return DRV_ERROR_NONE;
    }

    status = urma_get_tp_list(ctx, cfg, tp_cnt, tpid_info);
    if ((status != URMA_SUCCESS) || (*tp_cnt == 0)) {
        ascend_urma_err("Urma get tp list failed. (status=%d; tp_cnt=%u)\n", status, *tp_cnt);
        return DRV_ERROR_URMA_NO_RES;
    }
    info = ascend_tp_info_create(&cfg->local_eid, &cfg->peer_eid, tpid_info);
    if (info == NULL) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    *out_info = info;
    return DRV_ERROR_NONE;
}

/* Get the tp list for the given configuration, with internal caching by EID pair */
urma_status_t ascend_urma_get_tp_list(urma_context_t *ctx, urma_get_tp_cfg_t *cfg, uint32_t *tp_cnt,
                                      urma_tp_info_t *tpid_info)
{
    struct ascend_tp_info *info = NULL;
    int ret;

    if ((ctx == NULL) || (cfg == NULL) || (tp_cnt == NULL) || (tpid_info == NULL)) {
        ascend_urma_err("Invalid param.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_mutex_lock(&g_tp_info_mutex);
    ret = _ascend_urma_get_tp_list(ctx, cfg, tp_cnt, tpid_info, &info);
    (void)pthread_mutex_unlock(&g_tp_info_mutex);

    return ret;
}

/* Maximum retry count when import fails with a first-used (never-succeeded) ctp */
#define ASCEND_URMA_IMPORT_MAX_RETRY 1

/* Common implementation for importing a remote resource (JFR or Jetty), with ctp retry on first-used failure */
static urma_target_jetty_t *ascend_urma_import_common(urma_context_t *ctx, urma_token_t *token_value,
                                                      urma_active_tp_cfg_t *active_tp_cfg, urma_get_tp_cfg_t *tp_cfg,
                                                      struct ascend_remote_res_info *res_info)
{
    struct ascend_tp_info *info = NULL;
    urma_target_jetty_t *tjetty = NULL;
    uint32_t tp_cnt = 1;
    urma_tp_info_t tpid_info = {0};
    uint32_t retry_cnt = 0;

    (void)pthread_mutex_lock(&g_tp_info_mutex);
    while (tjetty == NULL) {
        if (_ascend_urma_get_tp_list(ctx, tp_cfg, &tp_cnt, &tpid_info, &info) != DRV_ERROR_NONE) {
            break;
        }

        active_tp_cfg->tp_handle = info->tpid_info.tp_handle;

        if (res_info->remote_res_type == ASCEND_RES_JFR) {
            tjetty = urma_import_jfr_ex(ctx, (urma_rjfr_t *)res_info->remote_res, token_value, active_tp_cfg);
        } else {
            tjetty = urma_import_jetty_ex(ctx, (urma_rjetty_t *)res_info->remote_res, token_value, active_tp_cfg);
        }

        if (tjetty != NULL) {
            info->cnt++;
            info->ctp_used_flag = true;
            break;
        }

        /* Import failed; give up if ctp was already used or max retries reached */
        if (info->ctp_used_flag || (retry_cnt >= ASCEND_URMA_IMPORT_MAX_RETRY)) {
            ascend_urma_err("Import failed, give up retry. (ctp_used_flag=%u; retry_cnt=%u; max_retry=%u; "
                            "res_type=%u; info_cnt=%u)\n",
                            info->ctp_used_flag, retry_cnt, ASCEND_URMA_IMPORT_MAX_RETRY, res_info->remote_res_type,
                            info->cnt);
            /* if ctp is not used but first import is failed , urma will destroy ctp, so delete node in list here */
            if (info->ctp_used_flag == false) {
                drv_user_list_del(&info->node);
                free(info);
                info = NULL;
            }
            break;
        }

        /* Destroy the current tp node so _ascend_urma_get_tp_list queries a fresh ctp */
        drv_user_list_del(&info->node);
        free(info);
        info = NULL;
        (void)pthread_mutex_unlock(&g_tp_info_mutex);
        tp_cnt = 1;
        (void)memset_s(&tpid_info, sizeof(tpid_info), 0, sizeof(tpid_info));
        retry_cnt++;
        ascend_urma_warn("Retrying import with a new ctp. (retry_cnt=%u, res_type=%u)\n", retry_cnt,
                         res_info->remote_res_type);
        (void)pthread_mutex_lock(&g_tp_info_mutex);
    }

    (void)pthread_mutex_unlock(&g_tp_info_mutex);
    return tjetty;
}

/* Import a remote JFR resource via the urma adapt layer */
urma_target_jetty_t *ascend_urma_import_jfr_ex(urma_context_t *ctx, urma_rjfr_t *rjfr, urma_token_t *token_value,
                                               urma_active_tp_cfg_t *active_tp_cfg)
{
    urma_target_jetty_t *tjetty = NULL;
    urma_get_tp_cfg_t tp_cfg = {0};
    struct ascend_remote_res_info res_info = {0};

    if ((ctx == NULL) || (rjfr == NULL) || (token_value == NULL) || (active_tp_cfg == NULL)) {
        ascend_urma_err("Invalid param.\n");
        return NULL;
    }

    tp_cfg.trans_mode = rjfr->trans_mode;
    tp_cfg.local_eid = ctx->eid;
    tp_cfg.peer_eid = rjfr->jfr_id.eid;
    if (rjfr->tp_type == URMA_CTP) {
        tp_cfg.flag.bs.ctp = true;
    }

    res_info.remote_res_type = ASCEND_RES_JFR;
    res_info.remote_res = (void *)rjfr;

    tjetty = ascend_urma_import_common(ctx, token_value, active_tp_cfg, &tp_cfg, &res_info);
    if (tjetty == NULL) {
        ascend_urma_err("Import jfr failed.(local_eid=" EID_FMT "; remote_eid=" EID_FMT ")\n",
                        EID_ARGS(tp_cfg.local_eid), EID_ARGS(tp_cfg.peer_eid));
    }

    return tjetty;
}

/* Import a remote Jetty resource via the urma adapt layer */
urma_target_jetty_t *ascend_urma_import_jetty_ex(urma_context_t *ctx, urma_rjetty_t *rjetty, urma_token_t *token_value,
                                                 urma_active_tp_cfg_t *active_tp_cfg)
{
    urma_target_jetty_t *tjetty = NULL;
    urma_get_tp_cfg_t tp_cfg = {0};
    struct ascend_remote_res_info res_info = {0};

    if ((ctx == NULL) || (rjetty == NULL) || (token_value == NULL) || (active_tp_cfg == NULL)) {
        ascend_urma_err("Invalid param.\n");
        return NULL;
    }

    tp_cfg.trans_mode = rjetty->trans_mode;
    tp_cfg.local_eid = ctx->eid;
    tp_cfg.peer_eid = rjetty->jetty_id.eid;
    if (rjetty->tp_type == URMA_CTP) {
        tp_cfg.flag.bs.ctp = true;
    }

    res_info.remote_res_type = ASCEND_RES_JETTY;
    res_info.remote_res = (void *)rjetty;

    tjetty = ascend_urma_import_common(ctx, token_value, active_tp_cfg, &tp_cfg, &res_info);
    if (tjetty == NULL) {
        ascend_urma_err("Import jetty failed.(local_eid=" EID_FMT "; remote_eid=" EID_FMT ")\n",
                        EID_ARGS(tp_cfg.local_eid), EID_ARGS(tp_cfg.peer_eid));
    }

    return tjetty;
}

static int ascend_urma_unimport_common(urma_target_jetty_t *tjetty, u32 res_type)
{
    struct ascend_tp_info *info = NULL;
    urma_status_t status;
    urma_eid_t local_eid;
    urma_eid_t remote_eid;

    if ((tjetty == NULL) || (tjetty->urma_ctx == NULL)) {
        ascend_urma_err("Invalid tjetty param.\n");
        return URMA_SUCCESS;
    }

    local_eid = tjetty->urma_ctx->eid;
    remote_eid = tjetty->id.eid;

    (void)pthread_mutex_lock(&g_tp_info_mutex);
    info = ascend_tp_info_find_by_eid(&local_eid, &remote_eid);
    if (info == NULL) {
        ascend_urma_err("Can not find tp_info from eid pair.(local_eid=" EID_FMT "; remote_eid=" EID_FMT ")\n",
                        EID_ARGS(local_eid), EID_ARGS(remote_eid));
        (void)pthread_mutex_unlock(&g_tp_info_mutex);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (info->ctp_used_flag == false) {
        (void)pthread_mutex_unlock(&g_tp_info_mutex);
        return DRV_ERROR_NONE;
    }

    if (res_type == ASCEND_RES_JFR) {
        status = urma_unimport_jfr(tjetty);
    } else {
        status = urma_unimport_jetty(tjetty);
    }

    if (status != URMA_SUCCESS) {
        ascend_urma_warn("Urma unimport jfr has problem. (status=%d)\n", status);
    }

    info->cnt--;
    if ((info->cnt == 0) && (info->ctp_used_flag)) {
        drv_user_list_del(&info->node);
        free(info);
    }
    (void)pthread_mutex_unlock(&g_tp_info_mutex);

    return URMA_SUCCESS;
}

/* Unimport a previously imported remote JFR target jetty */
urma_status_t ascend_urma_unimport_jfr_ex(urma_target_jetty_t *tjetty)
{
    return ascend_urma_unimport_common(tjetty, ASCEND_RES_JFR);
}

/* Unimport a previously imported remote Jetty target jetty */
urma_status_t ascend_urma_unimport_jetty_ex(urma_target_jetty_t *tjetty)
{
    return ascend_urma_unimport_common(tjetty, ASCEND_RES_JETTY);
}

/* ======================== cleanup ======================== */

static void ascend_urma_cxt_list_cleanup(void)
{
    struct ascend_urma_cxt_info *info = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;

    (void)pthread_mutex_lock(&g_urma_ctx_mutex);
    list_for_each_safe(pos, n, &g_urma_ctx_list)
    {
        info = list_entry(pos, struct ascend_urma_cxt_info, node);
        drv_user_list_del(&info->node);
        if (info->urma_ctx != NULL) {
            (void)urma_delete_context(info->urma_ctx);
        }
        free(info);
    }
    (void)pthread_mutex_unlock(&g_urma_ctx_mutex);
}

static void ascend_tp_info_list_cleanup(void)
{
    struct ascend_tp_info *info = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;

    (void)pthread_mutex_lock(&g_tp_info_mutex);
    list_for_each_safe(pos, n, &g_tp_info_list)
    {
        info = list_entry(pos, struct ascend_tp_info, node);
        drv_user_list_del(&info->node);
        free(info);
    }
    (void)pthread_mutex_unlock(&g_tp_info_mutex);
}

/* Release all urma adapt layer resources */
void ascend_urma_adapt_release(void)
{
    ascend_urma_cxt_list_cleanup();
    ascend_tp_info_list_cleanup();
}
