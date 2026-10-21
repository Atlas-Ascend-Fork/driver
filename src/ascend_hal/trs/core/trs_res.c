/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

#include "ascend_hal.h"
#include "drv_user_common.h"

#include "trs_ioctl.h"
#include "trs_user_pub_def.h"
#include "trs_master_urma.h"
#include "trs_dev_drv.h"
#include "trs_res.h"

struct trs_res_id_ctx {
    uint32_t res_id_num;
    struct res_id_usr_info *res_id_info;
};

static struct trs_res_id_ctx res_id_ctx[TRS_DEV_NUM][TRS_TS_NUM][DRV_INVALID_ID] = {0};
static int trs_dev_single_res_id_init(uint32_t dev_id, drvIdType_t id_type);

static struct trs_res_id_ctx *trs_get_res_id_ctx(uint32_t dev_id, uint32_t ts_id, drvIdType_t type)
{
    return &res_id_ctx[dev_id][ts_id][type];
}

uint32_t trs_get_res_id_num(uint32_t dev_id, uint32_t ts_id, drvIdType_t type)
{
    return res_id_ctx[dev_id][ts_id][type].res_id_num;
}

static int trs_res_type_trans[DRV_INVALID_ID] = {
    [DRV_STREAM_ID] = TRS_STREAM, [DRV_EVENT_ID] = TRS_EVENT, [DRV_MODEL_ID] = TRS_MODEL,
    [DRV_NOTIFY_ID] = TRS_NOTIFY, [DRV_CMO_ID] = TRS_CMO,     [DRV_CNT_NOTIFY_ID] = TRS_CNT_NOTIFY,
    [DRV_SQ_ID] = TRS_HW_SQ,      [DRV_CQ_ID] = TRS_HW_CQ,
};

static drvIdType_t trs_res_query_type_trans[DRV_RESOURCE_INVALID_ID] = {
    [DRV_RESOURCE_STREAM_ID] = (int)TRS_STREAM, [DRV_RESOURCE_EVENT_ID] = (int)TRS_EVENT,
    [DRV_RESOURCE_MODEL_ID] = (int)TRS_MODEL,   [DRV_RESOURCE_NOTIFY_ID] = (int)TRS_NOTIFY,
    [DRV_RESOURCE_CMO_ID] = (int)TRS_CMO,       [DRV_RESOURCE_SQ_ID] = (int)TRS_HW_SQ,
    [DRV_RESOURCE_CQ_ID] = (int)TRS_HW_CQ,      [DRV_RESOURCE_CNT_NOTIFY_ID] = (int)TRS_CNT_NOTIFY,
};

static int trs_get_res_id_type(drvIdType_t type)
{
    return ((type >= DRV_STREAM_ID) && (type < DRV_INVALID_ID)) ? trs_res_type_trans[type] : TRS_MAX_ID_TYPE;
}

static struct res_id_usr_info *_trs_get_res_id_info(uint32_t dev_id, uint32_t ts_id, drvIdType_t type, uint32_t id)
{
    struct trs_res_id_ctx *ctx = trs_get_res_id_ctx(dev_id, ts_id, type);

    if (ctx->res_id_info == NULL) {
        return NULL;
    }

    if (id >= ctx->res_id_num) {
        trs_err("Invalid id. (dev_id=%u; ts_id=%u; type=%d; id=%u)\n", dev_id, ts_id, type, id);
        return NULL;
    }

    return &ctx->res_id_info[id];
}

struct res_id_usr_info *trs_get_res_id_info(uint32_t dev_id, uint32_t ts_id, drvIdType_t type, uint32_t id)
{
    struct res_id_usr_info *res_id_info = NULL;

    if (trs_get_res_id_type(type) >= TRS_MAX_ID_TYPE) {
        trs_err("Invalid value. (dev_id=%u; ts_id=%u; type=%d)\n", dev_id, ts_id, type);
        return NULL;
    }

    res_id_info = _trs_get_res_id_info(dev_id, ts_id, type, id);
    if (res_id_info != NULL) {
        if (res_id_info->valid == 0) {
            return NULL;
        }
    }

    return res_id_info;
}

drvError_t __attribute__((weak)) trs_register_reg(uint32_t dev_id, uint64_t va, uint32_t size)
{
    (void)dev_id;
    (void)va;
    (void)size;
    return DRV_ERROR_NONE;
}

void __attribute__((weak)) trs_unregister_reg(uint32_t dev_id, uint64_t va, uint32_t size)
{
    (void)dev_id;
    (void)va;
    (void)size;
}

drvError_t __attribute__((weak)) halResAddrMap(unsigned int devId, struct res_addr_info *res_info, unsigned long *va,
                                               unsigned int *len)
{
    (void)devId;
    (void)res_info;
    (void)va;
    (void)len;
    return DRV_ERROR_NONE;
}

drvError_t __attribute__((weak)) halResAddrUnmap(unsigned int devId, struct res_addr_info *res_info)
{
    (void)devId;
    (void)res_info;
    return DRV_ERROR_NONE;
}

int trs_res_map_reg_init(uint32_t dev_id, uint32_t ts_id, uint32_t res_id, drvIdType_t type,
                         struct res_id_usr_info *id_info)
{
    struct res_addr_info res_info = {0};
    unsigned long notify_va;
    uint32_t notify_len;
    int ret;

    if (id_info == NULL) {
        return DRV_ERROR_UNINIT;
    }
    res_info.id = ts_id;
    res_info.res_id = res_id;
    res_info.target_proc_type = PROCESS_CP1;
    res_info.res_type = (type == DRV_NOTIFY_ID) ? RES_ADDR_TYPE_STARS_NOTIFY_RECORD :
                                                  RES_ADDR_TYPE_STARS_CNT_NOTIFY_RECORD;
    ret = halResAddrMap(dev_id, &res_info, &notify_va, &notify_len);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST /* Don't delete for UT compile */
        trs_err("Failed to map notify. (dev_id=%u; tsid=%u; res_id=%u)\n", dev_id, ts_id, res_id);
        return ret;
#endif
    }

    id_info->res_addr = (uint64_t)notify_va;
    id_info->res_len = notify_len;
    ret = trs_register_reg(dev_id, id_info->res_addr, id_info->res_len);
    if (ret != 0) {
        trs_err("Register addr failed. (dev_id=%u; type=%d; addr=0x%llx; len=%u; id=%u; ret=%d)\n", dev_id, type,
                (unsigned long long)id_info->res_addr, id_info->res_len, res_id, ret);
        return ret;
    }
    trs_debug("Id map reg init. (dev_id=%u; type=%d; id=%u; addr=0x%llx; len=%u)\n", dev_id, type, res_id,
              (unsigned long long)id_info->res_addr, id_info->res_len);
    return DRV_ERROR_NONE;
}

void trs_res_map_reg_un_init(uint32_t dev_id, uint32_t tsid, uint32_t res_id, drvIdType_t type,
                             struct res_id_usr_info *id_info)
{
    struct res_addr_info res_info = {0};
    int ret;

    if ((type != DRV_NOTIFY_ID) && (type != DRV_CNT_NOTIFY_ID)) {
        return;
    }

    trs_unregister_reg(dev_id, (uint64_t)id_info->res_addr, id_info->res_len);

    res_info.id = tsid;
    res_info.target_proc_type = PROCESS_CP1;
    res_info.res_id = res_id;
    res_info.res_type = (type == DRV_NOTIFY_ID) ? RES_ADDR_TYPE_STARS_NOTIFY_RECORD :
                                                  RES_ADDR_TYPE_STARS_CNT_NOTIFY_RECORD;
    ret = halResAddrUnmap(dev_id, &res_info);
    if (ret != DRV_ERROR_NONE) {
        trs_warn("Unmap failed. (dev_id=%u; ts_id=%u; notify_id=%u; ret=%d)\n", dev_id, tsid, res_id, ret);
    }
}

static void trs_res_id_info_un_init(uint32_t dev_id, uint32_t ts_id, uint32_t res_id, drvIdType_t type)
{
    struct res_id_usr_info *id_info = NULL;

    id_info = _trs_get_res_id_info(dev_id, ts_id, type, res_id);
    if (id_info == NULL) {
        return;
    }

    if (id_info->res_addr != 0) {
        trs_res_map_reg_un_init(dev_id, ts_id, res_id, type, id_info);
    }

    id_info->valid = 0;
    id_info->res_addr = 0;
    id_info->res_len = 0;
    id_info->priv = 0;
}

static int _trs_res_id_info_init(uint32_t dev_id, uint32_t ts_id, uint32_t res_id, drvIdType_t type,
                                 struct res_id_info_val *id_info_val)
{
    struct res_id_usr_info *id_info = NULL;

    id_info = _trs_get_res_id_info(dev_id, ts_id, type, res_id);
    if (id_info == NULL) {
        return DRV_ERROR_UNINIT;
    }

    id_info->res_len = id_info_val->res_len;
    id_info->res_addr = id_info_val->res_addr;
    id_info->valid = 1;
    return DRV_ERROR_NONE;
}

int trs_res_id_info_init(uint32_t dev_id, uint32_t ts_id, uint32_t res_id, drvIdType_t type,
                         struct res_id_info_val *id_info_val)
{
    struct trs_res_id_ctx *ctx = NULL;
    int ret = 0;

    ctx = trs_get_res_id_ctx(dev_id, ts_id, type);
    if (ctx->res_id_info == NULL) {
        ret = trs_dev_single_res_id_init(dev_id, type);
        if (ret != 0) {
            return ret;
        }
    }

    return _trs_res_id_info_init(dev_id, ts_id, res_id, type, id_info_val);
}

static int trs_res_type_init(uint32_t dev_id, uint32_t ts_id, int id_type, struct res_id_usr_info **id_info,
                             uint32_t *id_num)
{
    struct trs_res_id_para para = {.tsid = ts_id, .res_type = id_type};
    struct res_id_usr_info *info = NULL;
    uint32_t num;
    int ret;

    if (id_type >= TRS_MAX_ID_TYPE) {
        *id_info = NULL;
        *id_num = 0;
        return 0;
    }

    ret = trs_id_query(dev_id, TRS_RES_ID_MAX_QUERY, &para, &num);
    if ((ret != DRV_ERROR_NONE) || (num == 0)) {
        return ret;
    }

    info = calloc(num, sizeof(struct res_id_usr_info));
    if (info == NULL) {
        trs_err("Calloc failed. (dev_id=%u; ts_id=%u; id_type=%d; num=%u)\n", dev_id, ts_id, id_type, num);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    *id_info = info;
    *id_num = num;

    trs_info("Res type init success. (dev_id=%u; ts_id=%u; id_type=%d; num=%u)\n", dev_id, ts_id, id_type, num);
    return DRV_ERROR_NONE;
}

static void trs_res_type_un_init(uint32_t dev_id, uint32_t ts_id, drvIdType_t id_type, struct res_id_usr_info **id_info,
                                 uint32_t *id_num)
{
    struct res_id_usr_info *info = *id_info;
    uint32_t i = 0;

    for (i = 0; i < *id_num; i++) {
        if (info[i].valid == 0) {
            continue;
        }

        trs_res_id_info_un_init(dev_id, ts_id, i, id_type);
    }

    free(info);
    *id_info = NULL;
    *id_num = 0;
}

static int trs_ts_res_id_init(uint32_t dev_id, uint32_t ts_id)
{
    struct trs_res_id_ctx *ctx = NULL;
    drvIdType_t i, j;
    int ret;

    for (i = 0; i < DRV_INVALID_ID; i++) {
        ctx = trs_get_res_id_ctx(dev_id, ts_id, i);
        ret = trs_res_type_init(dev_id, ts_id, trs_get_res_id_type(i), &ctx->res_id_info, &ctx->res_id_num);
        if (ret != DRV_ERROR_NONE) {
            goto error;
        }
    }
    return 0;

error:
    for (j = 0; j < i; j++) {
        ctx = trs_get_res_id_ctx(dev_id, ts_id, j);
        trs_res_type_un_init(dev_id, ts_id, j, &ctx->res_id_info, &ctx->res_id_num);
    }
    return ret;
}

static void trs_ts_res_id_un_init(uint32_t dev_id, uint32_t ts_id)
{
    struct trs_res_id_ctx *ctx = NULL;
    drvIdType_t type;

    for (type = 0; type < DRV_INVALID_ID; type++) {
        ctx = trs_get_res_id_ctx(dev_id, ts_id, type);
        if (ctx->res_id_info != NULL) {
            trs_res_type_un_init(dev_id, ts_id, type, &ctx->res_id_info, &ctx->res_id_num);
        }
    }
}

static int trs_dev_single_res_id_init(uint32_t dev_id, drvIdType_t id_type)
{
    uint32_t ts_num = trs_get_ts_num(dev_id);
    uint32_t i, j;
    int ret;

    for (i = 0; i < ts_num; i++) {
        struct trs_res_id_ctx *ctx = trs_get_res_id_ctx(dev_id, i, id_type);
        ret = trs_res_type_init(dev_id, i, trs_get_res_id_type(id_type), &ctx->res_id_info, &ctx->res_id_num);
        if (ret != DRV_ERROR_NONE) {
            for (j = 0; j < i; j++) {
                ctx = trs_get_res_id_ctx(dev_id, j, id_type);
                trs_res_type_un_init(dev_id, j, id_type, &ctx->res_id_info, &ctx->res_id_num);
            }
            return ret;
        }
    }

    trs_info("Res id init. (devid=%u; id_type=%d)\n", dev_id, id_type);
    return 0;
}

int trs_dev_res_id_init(uint32_t dev_id)
{
    uint32_t ts_num = trs_get_ts_num(dev_id);
    uint32_t i, j;
    int ret;

    if (trs_get_connection_type(dev_id) != TRS_CONNECT_PROTOCOL_UB) {
        return 0;
    }

    trs_info("Res id init. (devid=%u)\n", dev_id);
    for (i = 0; i < ts_num; i++) {
        ret = trs_ts_res_id_init(dev_id, i);
        if (ret != DRV_ERROR_NONE) {
            for (j = 0; j < i; j++) {
                trs_ts_res_id_un_init(dev_id, j);
            }
            return ret;
        }
    }
    return 0;
}

void trs_dev_res_id_uninit(uint32_t dev_id)
{
    uint32_t ts_num = trs_get_ts_num(dev_id);
    uint32_t ts_id;

    for (ts_id = 0; ts_id < ts_num; ts_id++) {
        trs_ts_res_id_un_init(dev_id, ts_id);
    }
}

struct trs_res_remote_ops g_res_remote_ops;
void trs_register_res_remote_ops(struct trs_res_remote_ops *ops)
{
    g_res_remote_ops = *ops;
}

static struct trs_res_remote_ops *trs_get_res_remote_ops(void)
{
    return &g_res_remote_ops;
}

static void trs_res_fill_ioctrl_para(struct halResourceIdInputInfo *in, struct trs_res_id_para *para)
{
    para->tsid = in->tsId;
    para->res_type = trs_res_type_trans[in->type];
    para->id = in->resourceId;
    para->flag = in->res[RESOURCEID_RESV_FLAG];
    if (in->res[RESOURCEID_RESV_FLAG] == TSDRV_RES_RANGE_ID) {
        para->para = in->res[RESOURCEID_RESV_RANGE];
    } else {
        para->para = in->res[RESOURCEID_RESV_STREAM_PRIORITY];
    }
}

static drvError_t trs_res_alloc_para_check(uint32_t dev_id, struct halResourceIdInputInfo *in,
                                           struct halResourceIdOutputInfo *out)
{
    if ((in == NULL) || (out == NULL)) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((dev_id >= TRS_DEV_NUM) || (in->type >= DRV_INVALID_ID) || (in->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; res_type=%d; ts_id=%u)\n", dev_id, in->type, in->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

static bool trs_is_remote_res_ops(uint32_t flag)
{
    if (drvGetRuntimeApiVer() >= TRS_MC2_FEATURE) {
        if ((flag & TSDRV_FLAG_REMOTE_ID) != 0) {
            return true;
        }
    } else {
        trs_debug("Not support. (runtime_ver=0x%x; drv_ver=0x%x)\n", drvGetRuntimeApiVer(), __HAL_API_VERSION);
    }
    return false;
}

static drvError_t trs_remote_res_alloc(uint32_t dev_id, struct halResourceIdInputInfo *in,
                                       struct halResourceIdOutputInfo *out)
{
    if (trs_get_res_remote_ops()->resid_alloc != NULL) {
        return trs_get_res_remote_ops()->resid_alloc(dev_id, in, out);
    } else {
        trs_warn("Not support. (dev_id=%u; type=%d; flag=%u)\n", dev_id, in->type, in->res[RESOURCEID_RESV_FLAG]);
        return DRV_ERROR_NOT_SUPPORT;
    }
}

static drvError_t trs_local_res_alloc(uint32_t dev_id, struct halResourceIdInputInfo *in,
                                      struct halResourceIdOutputInfo *out)
{
    static int trs_res_errcode[DRV_INVALID_ID] = {
        [DRV_STREAM_ID] = DRV_ERROR_NO_STREAM_RESOURCES, [DRV_EVENT_ID] = DRV_ERROR_NO_EVENT_RESOURCES,
        [DRV_MODEL_ID] = DRV_ERROR_NO_MODEL_RESOURCES,   [DRV_NOTIFY_ID] = DRV_ERROR_NO_NOTIFY_RESOURCES,
        [DRV_CMO_ID] = DRV_ERROR_NO_RESOURCES,           [DRV_CNT_NOTIFY_ID] = DRV_ERROR_NO_RESOURCES,
    };
    struct trs_res_id_para para = {0};
    int ret;

    trs_res_fill_ioctrl_para(in, &para);

    ret = trs_dev_io_ctrl(dev_id, TRS_RES_ID_ALLOC, &para);
    if (ret == DRV_ERROR_NONE) {
        if ((trs_get_connection_type(dev_id) == TRS_CONNECT_PROTOCOL_UB) &&
            ((in->type == DRV_NOTIFY_ID) || (in->type == DRV_CNT_NOTIFY_ID))) {
            struct res_id_info_val id_info_val = {0};
            ret = _trs_res_id_info_init(dev_id, para.tsid, para.id, in->type, &id_info_val);
            if (ret != 0) {
                trs_err("Failed to init res id info. (dev_id=%u; type=%d; id=%u)\n", dev_id, in->type, para.id);
                return ret;
            }
        }
        out->resourceId = para.id;
        out->res[0] = para.value[0];
        out->res[1] = para.value[1];
    } else if (ret == DRV_ERROR_NO_RESOURCES) {
        ret = trs_res_errcode[in->type];
    } else {
        /* do nothing */
    }
    return ret;
}

#ifdef CFG_FEATURE_SUPPORT_STREAM_TASK
#define TRS_MAX_STREAM_NUM 2048U
#define TRS_MAX_MODEL_NUM 2048U
#endif
static drvError_t trs_res_alloc_post_proc(uint32_t devId, struct halResourceIdInputInfo *in, uint32_t id)
{
#ifdef CFG_FEATURE_SUPPORT_STREAM_TASK
    /*
     * stream extend feature: stream num 2K -> 32K; model num 2K -> 32K, just for cloudv2/cloudv3
     * Before runtime with old version still uses 2K to judge, here adapts for version compatibility.
     */
    if (drvGetRuntimeApiVer() < TRS_STREAM_EXPEND_FEATURE) {
        if ((in->type == DRV_STREAM_ID) && (id >= TRS_MAX_STREAM_NUM)) {
            in->resourceId = id;
            (void)halResourceIdFree(devId, in);
            return DRV_ERROR_NO_STREAM_RESOURCES;
        }
        if ((in->type == DRV_MODEL_ID) && (id >= TRS_MAX_MODEL_NUM)) {
            in->resourceId = id;
            (void)halResourceIdFree(devId, in);
            return DRV_ERROR_NO_MODEL_RESOURCES;
        }
    }
#else
    (void)devId;
    (void)in;
    (void)id;
#endif
    return DRV_ERROR_NONE;
}

drvError_t halResourceIdAlloc(uint32_t devId, struct halResourceIdInputInfo *in, struct halResourceIdOutputInfo *out)
{
    drvError_t ret;

    ret = trs_res_alloc_para_check(devId, in, out);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (trs_is_remote_res_ops(in->res[RESOURCEID_RESV_FLAG])) {
        ret = trs_remote_res_alloc(devId, in, out);
    } else {
        ret = trs_local_res_alloc(devId, in, out);
    }
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return trs_res_alloc_post_proc(devId, in, out->resourceId);
}

/* internal interface, running in dev cp proc */
drvError_t _halResourceIdAlloc(uint32_t dev_id, struct halResourceIdInputInfo *in, struct halResourceIdOutputInfo *out)
{
    drvError_t ret;

    ret = trs_res_alloc_para_check(dev_id, in, out);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    return trs_local_res_alloc(dev_id, in, out);
}

static drvError_t trs_res_free_para_check(uint32_t dev_id, struct halResourceIdInputInfo *in)
{
    if (in == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((dev_id >= TRS_DEV_NUM) || (in->type >= DRV_INVALID_ID) || (in->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; res_type=%d; ts_id=%u)\n", dev_id, in->type, in->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

static drvError_t trs_remote_res_free(uint32_t dev_id, struct halResourceIdInputInfo *in)
{
    if (trs_get_res_remote_ops()->resid_free != NULL) {
        return trs_get_res_remote_ops()->resid_free(dev_id, in);
    } else {
        trs_warn("Not support. (dev_id=%u; type=%d; flag=%u)\n", dev_id, in->type, in->res[RESOURCEID_RESV_FLAG]);
        return DRV_ERROR_NOT_SUPPORT;
    }
}

static drvError_t trs_local_res_free(uint32_t dev_id, struct halResourceIdInputInfo *in)
{
    struct trs_res_id_para para;
    int ret;

    trs_res_fill_ioctrl_para(in, &para);

    if (((trs_get_connection_type(dev_id) == TRS_CONNECT_PROTOCOL_UB) &&
         ((in->type == DRV_NOTIFY_ID) || (in->type == DRV_CNT_NOTIFY_ID))) ||
        (in->type == DRV_STREAM_ID)) {
        trs_res_id_info_un_init(dev_id, in->tsId, in->resourceId, in->type);
    }

    ret = trs_dev_io_ctrl(dev_id, TRS_RES_ID_FREE, &para);
    if (ret != 0) {
        trs_err("Ioctl failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t halResourceIdComm(uint32_t dev_id, uint32_t cmd, struct halResourceIdInputInfo *in)
{
    struct trs_res_id_para para;
    drvError_t ret;

    ret = trs_res_free_para_check(dev_id, in);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    trs_res_fill_ioctrl_para(in, &para);

    return trs_dev_io_ctrl(dev_id, cmd, &para);
}

drvError_t halResourceIdFree(uint32_t devId, struct halResourceIdInputInfo *in)
{
    drvError_t ret;

    ret = trs_res_free_para_check(devId, in);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (trs_is_remote_res_ops(in->res[RESOURCEID_RESV_FLAG])) {
        return trs_remote_res_free(devId, in);
    }

    return trs_local_res_free(devId, in);
}

/* internal interface, running in dev cp proc */
drvError_t _halResourceIdFree(uint32_t dev_id, struct halResourceIdInputInfo *in)
{
    drvError_t ret;

    ret = trs_res_free_para_check(dev_id, in);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return trs_local_res_free(dev_id, in);
}

drvError_t halResourceEnable(uint32_t devId, struct halResourceIdInputInfo *in)
{
    return halResourceIdComm(devId, TRS_RES_ID_ENABLE, in);
}

drvError_t halResourceDisable(uint32_t devId, struct halResourceIdInputInfo *in)
{
    return halResourceIdComm(devId, TRS_RES_ID_DISABLE, in);
}

drvError_t trs_res_config_para_check(uint32_t dev_id, struct halResourceIdInputInfo *in,
                                     struct halResourceConfigInfo *para)
{
    if ((in == NULL) || (para == NULL)) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((dev_id >= TRS_DEV_NUM) || (in->type >= DRV_INVALID_ID) || (in->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; res_type=%d; ts_id=%u)\n", dev_id, in->type, in->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

drvError_t trs_remote_res_config(uint32_t dev_id, struct halResourceIdInputInfo *in, struct halResourceConfigInfo *para)
{
    if (trs_get_res_remote_ops()->resid_config != NULL) {
        return trs_get_res_remote_ops()->resid_config(dev_id, in, para);
    } else {
        trs_warn("Not support. (dev_id=%u; type=%d; flag=%u)\n", dev_id, in->type, in->res[RESOURCEID_RESV_FLAG]);
        return DRV_ERROR_NOT_SUPPORT;
    }
}

drvError_t trs_local_res_config(uint32_t dev_id, struct halResourceIdInputInfo *in, struct halResourceConfigInfo *para)
{
    struct trs_res_id_para cfg;

    trs_res_fill_ioctrl_para(in, &cfg);
    cfg.prop = para->prop;
    cfg.para = para->value[0];
    return trs_dev_io_ctrl(dev_id, TRS_RES_ID_CFG, &cfg);
}

drvError_t _halResourceConfig(uint32_t dev_id, struct halResourceIdInputInfo *in, struct halResourceConfigInfo *para)
{
    drvError_t ret;

    ret = trs_res_config_para_check(dev_id, in, para);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return trs_local_res_config(dev_id, in, para);
}

int trs_id_query(uint32_t dev_id, uint32_t cmd, struct trs_res_id_para *para, uint32_t *value)
{
    int ret;

    ret = trs_dev_io_ctrl(dev_id, cmd, para);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Query failed. (dev_id=%u; ts_id=%u; res_type=%d)\n", dev_id, para->tsid, para->res_type);
        return ret;
    }

    *value = para->para;

    return DRV_ERROR_NONE;
}

int trs_res_id_query(uint32_t dev_id, uint32_t cmd, struct trs_res_id_para *para, uint32_t *value)
{
    if (value == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((dev_id >= TRS_DEV_NUM) || (para->res_type >= TRS_CORE_MAX_ID_TYPE)) {
        trs_err("Invalid para. (dev_id=%u; res_type=%d)\n", dev_id, para->res_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return trs_id_query(dev_id, cmd, para, value);
}

int trs_res_reg_offset_query(uint32_t dev_id, uint32_t ts_id, drvIdType_t type, uint32_t id, uint32_t *value)
{
    struct trs_res_id_para para = {.tsid = ts_id, .id = id};
    para.res_type = (type < DRV_INVALID_ID) ? trs_res_type_trans[type] : TRS_CORE_MAX_ID_TYPE;
    return trs_res_id_query(dev_id, TRS_RES_ID_REG_OFFSET_QUERY, &para, value);
}

int trs_res_reg_total_size_query(uint32_t dev_id, uint32_t ts_id, drvIdType_t type, uint32_t *value)
{
    struct trs_res_id_para para = {.tsid = ts_id};
    para.res_type = (type < DRV_INVALID_ID) ? trs_res_type_trans[type] : TRS_CORE_MAX_ID_TYPE;
    return trs_res_id_query(dev_id, TRS_RES_ID_REG_SIZE_QUERY, &para, value);
}

int trs_res_num_query(uint32_t dev_id, uint32_t ts_id, int res_type, uint32_t *value)
{
    struct trs_res_id_para para = {.tsid = ts_id, .res_type = res_type};
    int ret = trs_res_id_query(dev_id, TRS_RES_ID_NUM_QUERY, &para, value);
#ifdef CFG_FEATURE_SUPPORT_STREAM_TASK
    /*
     * stream extend feature: stream num 2K -> 32K; model num 2K -> 32K, just for cloudv2/cloudv3
     * Because runtime with old version still uses 2K to judge, here adapts for version compatibility.
     * If num < 2048, it means in vf, vf not support stream extend, just return num.
     */
    if (drvGetRuntimeApiVer() < TRS_STREAM_EXPEND_FEATURE) {
        if (res_type == TRS_STREAM) {
            *value = (*value < TRS_MAX_STREAM_NUM) ? *value : TRS_MAX_STREAM_NUM;
        }
        if (res_type == TRS_MODEL) {
            *value = (*value < TRS_MAX_MODEL_NUM) ? *value : TRS_MAX_MODEL_NUM;
        }
    }
#endif
    return ret;
}

int trs_res_used_num_query(uint32_t dev_id, uint32_t ts_id, int res_type, uint32_t *value)
{
    struct trs_res_id_para para = {.tsid = ts_id, .res_type = res_type};
    return trs_res_id_query(dev_id, TRS_RES_ID_USED_NUM_QUERY, &para, value);
}

drvError_t halResourceInfoQuery(uint32_t devId, uint32_t tsId, drvResourceType_t type, struct halResourceInfo *info)
{
    drvError_t ret;

    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (type >= DRV_RESOURCE_INVALID_ID)) {
        trs_err("Invalid para. (dev_id=%u; type=%d)\n", devId, type);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = trs_res_num_query(devId, tsId, trs_res_query_type_trans[type], &info->capacity);
    ret |= (drvError_t)trs_res_used_num_query(devId, tsId, trs_res_query_type_trans[type], &info->usedNum);
    return ret;
}

static int trs_get_res_id_by_stream(uint32_t dev_id, struct halResourceIdInputInfo *in,
                                    struct halResourceDetailInfo *info)
{
    struct halSqCqInputInfo input_info = {0};
    int ret;

    if (in->type != DRV_STREAM_ID) {
        trs_err("Not support. (type=%u)\n", in->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    input_info.type = DRV_INVALID_TYPE;
    if ((info->type == DRV_RES_QUERY_SQID) || (info->type == DRV_RES_QUERY_CQID)) {
        input_info.type = DRV_NORMAL_TYPE;
    } else {
        input_info.type = DRV_LOGIC_TYPE;
    }
    input_info.tsId = in->tsId;
    input_info.info[0] = in->resourceId;
    ret = trs_dev_io_ctrl(dev_id, TRS_ID_SQCQ_GET, &input_info);
    ;
    if (ret != 0) {
        return ret;
    }

    switch (info->type) {
        case DRV_RES_QUERY_SQID:
            info->value0 = input_info.sqId;
            return DRV_ERROR_NONE;
        case DRV_RES_QUERY_CQID:
        case DRV_RES_QUERY_LOGIC_CQID:
            info->value0 = input_info.cqId;
            return DRV_ERROR_NONE;
        default:
            trs_err("Not support. (devid=%u; type=%u)\n", dev_id, info->type);
            return DRV_ERROR_INVALID_VALUE;
    }
}

drvError_t halResourceDetailQuery(uint32_t devId, struct halResourceIdInputInfo *in, struct halResourceDetailInfo *info)
{
    drvError_t ret;

    if ((in == NULL) || (info == NULL)) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (in->type >= DRV_INVALID_ID)) {
        trs_err("Invalid para. (dev_id=%u; type=%d)\n", devId, in->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (info->type) {
        case DRV_RES_QUERY_OFFSET:
            ret = trs_res_reg_offset_query(devId, in->tsId, in->type, in->resourceId, &info->value0);
            return ret;
        case DRV_RES_QUERY_SQID:
        case DRV_RES_QUERY_CQID:
        case DRV_RES_QUERY_LOGIC_CQID:
            return trs_get_res_id_by_stream(devId, in, info);
        case DRV_RES_INFO_QUERY: {
            ret = trs_res_num_query(devId, in->tsId, trs_res_type_trans[in->type], &info->value0);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
            return trs_res_used_num_query(devId, in->tsId, trs_res_type_trans[in->type], &info->value1);
        }
        case DRV_RES_NOTIFY_TYPE_TOTAL_SIZE: {
            return halNotifyGetInfo(devId, in->tsId, DRV_NOTIFY_TYPE_TOTAL_SIZE, &(info->value0));
        }
        case DRV_RES_NOTIFY_TYPE_NUM: {
            return halNotifyGetInfo(devId, in->tsId, DRV_NOTIFY_TYPE_NUM, &(info->value0));
        }
        default:
            trs_err("Invalid para. (devid=%u; tsid=%u; id_type=%d; query_type=%d)\n", devId, in->tsId, in->type,
                    info->type);
            return DRV_ERROR_INVALID_VALUE;
    }
}
