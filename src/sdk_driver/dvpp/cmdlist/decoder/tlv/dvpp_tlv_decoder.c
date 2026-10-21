/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "dvpp_tlv_decoder.h"

#include "securec.h"
#include "dvpp_cmdlist_log.h"
#include "dvpp_cmdlist.h"
#include "dvpp_vpu_decoder.h"

#define CHECK_ARGS_PPOS_AND_FAIL_RET(ppos, end) \
    do {                                        \
        if ((ppos) >= (end)) {                  \
            return -1;                          \
        }                                       \
    } while (0)

static int32_t dvpp_tlv_init_decoder(void *decoder)
{
    dvpp_tlv_decoder *dec = (dvpp_tlv_decoder *)decoder;
    errno_t ret = EOK;
    dec->mod = 0;
    dec->ppos = dec->pbuf;
    dec->version = NULL;
    dec->head = NULL;

    ret = memset_s(dec->tlv_type_struct, dec->tlv_type_struct_size,
                   (int32_t)(uintptr_t)(DVPP_TLV_STRUCT_PTR_DEFAULT_VAL), dec->tlv_type_struct_size);
    if (ret != EOK) {
        DVPP_CMDLIST_LOG_ERROR("memset tlv type struct ptr array fail, size:%u", dec->tlv_type_struct_size);
        return -1;
    }

    return 0;
}

static uint16_t dvpp_tlv_get_node_cnt(void *decoder)
{
    return ((dvpp_tlv_decoder *)decoder)->head->node_cnt;
}

static uint16_t dvpp_tlv_get_batch_cnt(void *decoder)
{
    return ((dvpp_tlv_decoder *)decoder)->head->batch_cnt;
}

static uint16_t dvpp_tlv_get_node_cnt_of_batch(void *decoder, uint32_t index)
{
    return ((dvpp_tlv_decoder *)decoder)->head->node_cnt_of_batch[index];
}

static uint32_t dvpp_tlv_get_module_type(void *decoder)
{
    return ((dvpp_tlv_decoder *)decoder)->head->mod;
}

static void *dvpp_tlv_get_vpu_mod_val(void *decoder, uint32_t node_idx, uint32_t type)
{
    uint32_t stru_len = 0;
    uint64_t data_addr = 0;
    dvpp_tlv_decoder *dec = (dvpp_tlv_decoder *)decoder;
    if (dec->tlv_type_struct->val[node_idx][type] == DVPP_TLV_STRUCT_PTR_DEFAULT_VAL) {
        return NULL;
    }

    if (dec->head->mod == DVPP_MOD_TYPE_VPC) {
        stru_len = g_vpu_mod_vpc_stru_len[type];
    } else if (dec->head->mod == DVPP_MOD_TYPE_JPEGD) {
        stru_len = g_vpu_mod_jpegd_stru_len[type];
    } else {
        stru_len = g_vpu_mod_jpege_stru_len[type];
    }

    data_addr = (uint64_t)((uintptr_t)(dec->tlv_type_struct->val[node_idx][type]) + sizeof(tlv_data_len));
    if (data_addr + stru_len - 1 >= (uint64_t)(uintptr_t)(dec->pbuf + dec->size)) {
        return NULL;
    }

    return (tlv_data_val)data_addr;
}

static uint16_t dvpp_tlv_get_vpu_mod_len(void *decoder, uint32_t node_idx, uint32_t type)
{
    dvpp_tlv_decoder *dec = (dvpp_tlv_decoder *)decoder;
    if (dec->tlv_type_struct->val[node_idx][type] != DVPP_TLV_STRUCT_PTR_DEFAULT_VAL) {
        return *(tlv_data_len *)(dec->tlv_type_struct->val[node_idx][type]);
    }

    return 0;
}

static int32_t dvpp_tlv_handle(dvpp_tlv_decoder *decoder, dvpp_tlv_data *data, uint32_t node_idx)
{
    if (node_idx >= DVPP_MAX_NODE_NUM) {
        DVPP_CMDLIST_LOG_ERROR("node_idx(%u) must be between [%u, %u]\n", node_idx, 0, DVPP_MAX_NODE_NUM - 1);
        return -1;
    }
    if (data->type >= DVPP_MAX_TLV_TYPE) {
        DVPP_CMDLIST_LOG_ERROR("tlv type(%u) must be between [%u, %u]\n", data->type, 0, DVPP_MAX_TLV_TYPE - 1);
        return -1;
    }
    decoder->tlv_type_struct->val[node_idx][data->type] = data->val;
    return 0;
}

static int32_t tlv_pop_data(dvpp_tlv_decoder *decoder, dvpp_tlv_data *data)
{
    CHECK_ARGS_PPOS_AND_FAIL_RET(decoder->ppos + sizeof(tlv_data_type) - 1, decoder->pbuf + decoder->size);
    data->type = *((tlv_data_type *)(uintptr_t)(decoder->ppos));
    decoder->ppos += sizeof(tlv_data_type);
    CHECK_ARGS_PPOS_AND_FAIL_RET(decoder->ppos + sizeof(tlv_data_len) - 1, decoder->pbuf + decoder->size);
    // val把len也包含进去，方便后续保持兼容性
    data->val = decoder->ppos;
    data->len = *((tlv_data_len *)(uintptr_t)(decoder->ppos));
    decoder->ppos += sizeof(tlv_data_len);
    decoder->ppos += data->len;
    CHECK_ARGS_PPOS_AND_FAIL_RET(decoder->ppos, decoder->pbuf + decoder->size);

    return 0;
}

static int32_t dvpp_tlv_decode_args(void *decoder)
{
    uint32_t i = 0;
    uint32_t total_cnt = 0;
    int32_t ret = 0;
    dvpp_tlv_data data;
    dvpp_tlv_decoder *dec = (dvpp_tlv_decoder *)decoder;
    // 解析version
    ret = tlv_pop_data(dec, &data);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("parse tlv version data fail.\n");
        return ret;
    }
    if (data.type != DVPP_TLV_TYPE_ARGS_VERSION) {
        DVPP_CMDLIST_LOG_ERROR("parse tlv version fail.\n");
        return -1;
    }
    dec->version = (dvpp_tlv_version_param *)((uint64_t)(uintptr_t)data.val + sizeof(tlv_data_len));
    CHECK_ARGS_PPOS_AND_FAIL_RET((uint64_t)(uintptr_t)dec->version + sizeof(dvpp_tlv_version_param) - 1,
                                 (uint64_t)(uintptr_t)(dec->pbuf + dec->size));
    // 打印版本号
#ifdef BUILD_DEBUG
    DVPP_CMDLIST_LOG_DEBUG("major_num=%u\n", dec->version->major_num);
    DVPP_CMDLIST_LOG_DEBUG("minor_num=%u\n", dec->version->minor_num);
    DVPP_CMDLIST_LOG_DEBUG("revision_num=%u\n", dec->version->revision_num);
#endif
    // 解析头
    ret = tlv_pop_data(dec, &data);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("parse tlv head data fail.\n");
        return ret;
    }
    if (data.type != DVPP_TLV_TYPE_ARGS_HEAD) {
        DVPP_CMDLIST_LOG_ERROR("parse tlv head fail.\n");
        return -1;
    }
    dec->head = (dvpp_tlv_head_param *)((uintptr_t)data.val + sizeof(tlv_data_len));
    CHECK_ARGS_PPOS_AND_FAIL_RET((uint64_t)(uintptr_t)dec->head + sizeof(dvpp_tlv_head_param) - 1,
                                 (uint64_t)(uintptr_t)(dec->pbuf + dec->size));
    if (dec->head->mod >= DVPP_CMDLIST_ENGINE_TYPE_BUTT) {
        DVPP_CMDLIST_LOG_ERROR("mod(%u) must be between [0, %d)\n", dec->head->mod,
                               (int32_t)DVPP_CMDLIST_ENGINE_TYPE_BUTT);
        return -1;
    }

    if (dec->head->batch_cnt < DVPP_MIN_BATCH_NUM || dec->head->batch_cnt > DVPP_MAX_BATCH_NUM) {
        DVPP_CMDLIST_LOG_ERROR("batch cnt(%u) must be between [%u, %u]\n", dec->head->batch_cnt, DVPP_MIN_BATCH_NUM,
                               DVPP_MAX_BATCH_NUM);
        return -1;
    }

    if (dec->head->node_cnt < DVPP_MIN_NODE_NUM || dec->head->node_cnt > DVPP_MAX_NODE_NUM) {
        DVPP_CMDLIST_LOG_ERROR("node cnt(%u) must be between [%u, %u]\n", dec->head->node_cnt, DVPP_MIN_NODE_NUM,
                               DVPP_MAX_NODE_NUM);
        return -1;
    }

    if (dec->head->node_cnt > dec->tlv_node_num) {
        DVPP_CMDLIST_LOG_ERROR("node cnt(%u) must be less than %u\n", dec->head->node_cnt, dec->tlv_node_num);
        return -1;
    }
    for (i = 0; i < dec->head->batch_cnt; i++) {
        if (dec->head->node_cnt_of_batch[i] > DVPP_MAX_NODE_NUM) {
            DVPP_CMDLIST_LOG_ERROR("node cnt of batch[%u] (%u) must <= %u\n", i, dec->head->node_cnt_of_batch[i],
                                   DVPP_MAX_NODE_NUM);
            return -1;
        }
        total_cnt += dec->head->node_cnt_of_batch[i];
    }

    if (total_cnt > DVPP_MAX_NODE_NUM) {
        DVPP_CMDLIST_LOG_ERROR("total node cnt %u must <= %d\n", total_cnt, DVPP_MAX_NODE_NUM);
        return -1;
    }

#ifdef BUILD_DEBUG
    DVPP_CMDLIST_LOG_DEBUG("batch_cnt=%u\n", dec->head->batch_cnt);
    DVPP_CMDLIST_LOG_DEBUG("node_cnt=%u\n", dec->head->node_cnt);
#endif

    for (i = 0; i < dec->head->node_cnt; i++) {
        do {
            ret = tlv_pop_data(dec, &data);
            if (ret != 0) {
                DVPP_CMDLIST_LOG_ERROR("parse tlv node data fail.\n");
                return ret;
            }
            ret = dvpp_tlv_handle(dec, &data, i);
            if (ret != 0) {
                DVPP_CMDLIST_LOG_ERROR("fill tlv value fail.\n");
                return ret;
            }
        } while (data.type != DVPP_TLV_TYPE_ARGS_END);
    }
    return 0;
}

void dvpp_tlv_decoder_register(void)
{
    struct dvpp_decoder_ops *decoder_ops = dvpp_get_decoder_ops();
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_init_decoder = dvpp_tlv_init_decoder;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_decode_args = dvpp_tlv_decode_args;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_node_cnt = dvpp_tlv_get_node_cnt;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_batch_cnt = dvpp_tlv_get_batch_cnt;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_node_cnt_of_batch = dvpp_tlv_get_node_cnt_of_batch;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_module_type = dvpp_tlv_get_module_type;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_vpu_mod_val = dvpp_tlv_get_vpu_mod_val;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_vpu_mod_len = dvpp_tlv_get_vpu_mod_len;
}

void dvpp_tlv_decoder_unregister(void)
{
    struct dvpp_decoder_ops *decoder_ops = dvpp_get_decoder_ops();
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_init_decoder = NULL;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_decode_args = NULL;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_node_cnt = NULL;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_batch_cnt = NULL;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_node_cnt_of_batch = NULL;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_module_type = NULL;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_vpu_mod_val = NULL;
    decoder_ops[DVPP_PROTOCOL_TLV].dvpp_get_vpu_mod_len = NULL;
}
