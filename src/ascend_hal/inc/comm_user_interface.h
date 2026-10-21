/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COMM_USER_INTERFACE_H
#define COMM_USER_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include "urma_types.h"
#include "drv_user_common.h"

#define ASCEND_URMA_MAX_DEV_NUM 65U

/* ================================ Ascend Urma Raw ==============================*/
int ascend_urma_get_token_val(uint32_t devid, uint32_t *token_val);
urma_target_jetty_t *ascend_urma_import_jfr(urma_context_t *urma_ctx, urma_rjfr_t *rjfr, urma_token_t *token_value);
int ascend_urma_wait_jfc(urma_jfce_t *jfce, uint32_t jfc_cnt, int time_out, urma_jfc_t *jfc[]);

/* ================================ Ascend Urma Qos ==============================*/
#define ASCEND_URMA_PRIORITY_HIGH 10
#define ASCEND_URMA_PRIORITY_MIDDLE 11
#define ASCEND_URMA_PRIORITY_LOW 12

/* ================================ Ascend Urma Dev ==============================*/
bool ascend_urma_dev_is_exist(uint32_t devid);

/* ================================ Ascend Urma Ctx ==============================*/
void *ascend_urma_ctx_get(uint32_t devid);
void ascend_urma_ctx_put(void *ctx);

urma_context_t *ascend_to_urma_ctx(void *ctx);

/* ================================ Ascend Urma Token ==============================*/
struct ascend_urma_token {
    struct list_head node;

    void *pool;

    urma_token_id_t *token_id;
    urma_token_t token_val;
    uint64_t acquired_num;
};

urma_token_id_t *ascend_urma_alloc_token_id(void *ctx);
void ascend_urma_free_token_id(urma_token_id_t *token_id);

/* ================================ Ascend Urma Seg ==============================*/
#define ASCEND_URMA_SEG_FLAG_ACCESS_WRITE (1U << 0U)
#define ASCEND_URMA_SEG_FLAG_PIN (1U << 1U)
#define ASCEND_URMA_SEG_FLAG_WITHOUT_TOKEN_VAL (1U << 2U)

/* Callback to acquire a token for the given VA range.
 * Returns a pointer to an existing token if one is found within the same reserve range,
 * or NULL if a new token should be allocated. */
typedef void *(*ascend_urma_acquire_token_cb)(void *seg_mng, uint64_t start, uint64_t size);

struct ascend_urma_seg_mng_attr {
    uint32_t token_num_default;
    uint32_t token_num_cache_up_thres;
    uint32_t max_seg_num_per_token;
    ascend_urma_acquire_token_cb acquire_token;
};

struct ascend_urma_seg_info {
    uint64_t start;
    uint64_t size;
    uint32_t token_id;
    uint32_t token_val;
    uint32_t seg_flag;
    urma_target_seg_t *tseg;
};

struct ascend_urma_seg_info_ex {
    uint64_t start;
    uint64_t size;
    uint32_t seg_flag;

    struct ascend_urma_token *token;
    urma_target_seg_t *tseg;
};

void *ascend_urma_seg_mng_create(uint32_t devid, struct ascend_urma_seg_mng_attr *attr);
void ascend_urma_seg_mng_destroy(void *seg_mng);

/* If already register, will return DRV_ERROR_BUSY */
int ascend_urma_register_seg(void *seg_mng, uint64_t start, uint64_t size, uint32_t seg_flag);
int ascend_urma_unregister_seg(void *seg_mng, uint64_t start, uint64_t size);
urma_target_seg_t *ascend_urma_register_segment(uint32_t devid, uint64_t start, uint64_t size, uint32_t seg_flag,
                                                void *token);
void ascend_urma_unregister_segment(urma_target_seg_t *tseg);

int ascend_urma_get_seg_info(void *seg_mng, uint64_t va, struct ascend_urma_seg_info *info);

/* Get the previous and next segment relative to the given va, caller should ensure acquire seg_mng lock */
int ascend_urma_seg_get_prev(void *seg_mng, uint64_t va, struct ascend_urma_seg_info_ex *info);
int ascend_urma_seg_get_next(void *seg_mng, uint64_t va, struct ascend_urma_seg_info_ex *info);

int ascend_urma_get_urma_dev_name(uint32_t devid, char *name, uint32_t *size);

urma_context_t *ascend_urma_create_context(urma_device_t *dev, uint32_t eid_index);
urma_status_t ascend_urma_delete_context(urma_context_t *urma_ctx);
urma_status_t ascend_urma_get_tp_list(urma_context_t *ctx, urma_get_tp_cfg_t *cfg, uint32_t *tp_cnt,
                                      urma_tp_info_t *tpid_info);
urma_target_jetty_t *ascend_urma_import_jfr_ex(urma_context_t *ctx, urma_rjfr_t *rjfr, urma_token_t *token_value,
                                               urma_active_tp_cfg_t *active_tp_cfg);
urma_target_jetty_t *ascend_urma_import_jetty_ex(urma_context_t *ctx, urma_rjetty_t *rjetty, urma_token_t *token_value,
                                                 urma_active_tp_cfg_t *active_tp_cfg);
urma_status_t ascend_urma_unimport_jfr_ex(urma_target_jetty_t *tjetty);
urma_status_t ascend_urma_unimport_jetty_ex(urma_target_jetty_t *tjetty);
#endif