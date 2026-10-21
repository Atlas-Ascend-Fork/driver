/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "drv_buff_common_mempool.h"
#include "ascend_hal.h"
#include "drv_buff_common.h"
#include "drv_buff_unibuff.h"
#include "drv_buff_mbuf.h"
#include "buff_mng.h"
#include "drv_buff_maintain.h"

static drvError_t buff_mbuf_info_para_check(struct buff_get_info_handle_arg *para_in, uint32 out_size)
{
    struct Mbuf *mbuf = NULL;

    if ((para_in->in_size != sizeof(struct Mbuf *)) || (*((struct Mbuf **)para_in->in) == NULL) ||
        (para_in->out_size != out_size)) {
        buff_err("para size invalid: in size:%u, %u. out size:%u, %u\n", para_in->in_size,
                 (uint32)sizeof(struct Mbuf *), para_in->out_size, out_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    mbuf = *((struct Mbuf **)para_in->in);
    if (mbuf_is_invalid(mbuf) != 0) {
        buff_err("Mbuf invalid\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t buff_get_mbuf_use_info(struct buff_get_info_handle_arg *para_in)
{
    struct uni_buff_head_t *head = NULL;
    struct uni_buff_head_t *data_head = NULL;
    struct uni_buff_ext_info *ext_info = NULL;
    struct MbufUseInfo *info = (struct MbufUseInfo *)para_in->out;
    struct Mbuf *mbuf = NULL;
    struct share_mbuf *s_mbuf = NULL;

    if (buff_mbuf_info_para_check(para_in, sizeof(struct MbufUseInfo)) != 0) {
        buff_err("mbuf info check fail\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    mbuf = *((struct Mbuf **)para_in->in);
    para_in->out_size = 0;
    info->allocPid = 0;
    info->usePid = 0;

    s_mbuf = get_share_mbuf_by_mbuf(mbuf);
    head = mbuf_get_head(s_mbuf);
    if (head->ext_flag == 1) {
        ext_info = buff_get_ext_info(head, s_mbuf->blk_id);
        if (ext_info == NULL) {
            buff_err("Can not find get buff ext info. (buff_type=%u; ext_flag=%u; mbuf=%pK)\n", head->buff_type,
                     head->ext_flag, (void *)s_mbuf);
            return DRV_ERROR_BAD_ADDRESS;
        }

        info->allocPid = ext_info->alloc_pid;
        info->usePid = ext_info->use_pid;
        buff_put_ext_info(ext_info, s_mbuf->blk_id);
    }

    info->timestamp = s_mbuf->timestamp;
    info->status = head->status;
    info->ref = 0;

    if (mbuf->buff_type != MBUF_BARE_BUFF) {
        data_head = buff_get_head(mbuf->datablock, mbuf->data_blk_id);
        if (data_head == NULL) {
            buff_err("get data head failed, data %pK\n", mbuf->datablock);
            return DRV_ERROR_BAD_ADDRESS;
        }
        info->ref = data_head->ref;
        buff_put_head(data_head, mbuf->data_blk_id);
    }

    para_in->out_size = (uint32)sizeof(struct MbufUseInfo);
    return DRV_ERROR_NONE;
}

drvError_t buff_get_mbuf_type_info(struct buff_get_info_handle_arg *para_in)
{
    struct MbufTypeInfo *info = (struct MbufTypeInfo *)para_in->out;
    struct Mbuf *mbuf = NULL;
    struct share_mbuf *s_mbuf = NULL;

    if (buff_mbuf_info_para_check(para_in, sizeof(struct MbufTypeInfo)) != 0) {
        buff_err("mbuf %p info check fail\n", (void *)mbuf);
        return DRV_ERROR_INVALID_VALUE;
    }

    mbuf = *((struct Mbuf **)para_in->in);
    s_mbuf = get_share_mbuf_by_mbuf(mbuf);
    info->type = (unsigned int)s_mbuf->buff_type;

    return DRV_ERROR_NONE;
}

drvError_t buff_get_buff_type_info(struct buff_get_info_handle_arg *para_in)
{
    struct BuffTypeInfo *buf_info = (struct BuffTypeInfo *)para_in->out;
    struct uni_buff_head_t *buff_head = NULL;
    unsigned long alloc_size;
    void *alloc_addr = NULL;
    void *buff = NULL;
    uint32 buff_blk_id;
    drvError_t ret;
    int pool_id;

    if ((para_in->in == NULL) || (para_in->in_size < sizeof(void *)) ||
        (para_in->out_size < sizeof(struct BuffTypeInfo))) {
        buff_err("para invalid. (in=%p; in_size=%u; out_size=%u)\n", para_in->in, para_in->in_size, para_in->out_size);
        return (int)DRV_ERROR_INVALID_VALUE;
    }
    buff = *((void **)para_in->in);
    ret = buff_blk_get(buff, &pool_id, &alloc_addr, &alloc_size, &buff_blk_id);
    if (ret != DRV_ERROR_NONE) {
        buff_err("Not alloc. (ret=%d)\n", ret);
        return (int)DRV_ERROR_INVALID_VALUE;
    }
    buff_blk_put(pool_id, alloc_addr);

    if (buff_verify_and_get_head(buff, &buff_head, buff_blk_id) != 0) {
        buff_err("Buff is invalid.\n");
        return (int)DRV_ERROR_INVALID_VALUE;
    }
    buf_info->type = (buff_head->mbuf_data_flag == UNI_MBUF_DATA_ENABLE) ? BUFF_TYPE_MBUF_DATA : BUFF_TYPE_NORMAL;
    buff_put_head(buff_head, buff_blk_id);
    para_in->out_size = sizeof(struct BuffTypeInfo);

    return DRV_ERROR_NONE;
}

static drvError_t buff_mempool_info_para_check(struct buff_get_info_handle_arg *para_in, uint32 out_size)
{
    if (para_in->in == NULL) {
        buff_err("Para invalid, input ptr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (*((struct mempool_t **)para_in->in) == NULL) {
        buff_err("Para invalid, para in ptr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((para_in->in_size != sizeof(struct mempool_t *)) || (para_in->out_size != out_size)) {
        buff_err("Para size invalid. (in_size=%u; input out_size=%u; out_size=%u)\n", para_in->in_size,
                 para_in->out_size, out_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t buff_get_mempool_info(struct buff_get_info_handle_arg *para_in)
{
    struct MemPoolInfo *mempool_info = (struct MemPoolInfo *)para_in->out;
    struct mempool_t *mp = NULL;

    if (buff_mempool_info_para_check(para_in, sizeof(struct MemPoolInfo)) != DRV_ERROR_NONE) {
        return DRV_ERROR_INVALID_VALUE;
    }

    mp = *((struct mempool_t **)para_in->in);
    mempool_info->blk_start = mp->blk_start;
    mempool_info->blk_total_len = mp->blk_total_len;
    para_in->out_size = (uint32)sizeof(struct MemPoolInfo);
    return DRV_ERROR_NONE;
}

drvError_t buff_get_mempool_blk_available(struct buff_get_info_handle_arg *para_in)
{
    struct MpBlkAvailable *mp_blk_available = (struct MpBlkAvailable *)para_in->out;
    struct mempool_t *mp = NULL;

    if (buff_mempool_info_para_check(para_in, sizeof(struct MpBlkAvailable)) != DRV_ERROR_NONE) {
        return DRV_ERROR_INVALID_VALUE;
    }

    mp = *((struct mempool_t **)para_in->in);
    mp_blk_available->blk_available = buff_api_atomic_read(&mp->blk_available);
    para_in->out_size = (uint32)sizeof(struct MpBlkAvailable);

    return DRV_ERROR_NONE;
}