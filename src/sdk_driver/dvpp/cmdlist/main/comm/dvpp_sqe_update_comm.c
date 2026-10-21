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

#include "dvpp_sqe_update_comm.h"

#include "ka_base_pub.h"

#include "dvpp_cmdlist_log.h"
#include "dvpp_cmdlist_context.h"
#include "dvpp_tlv_decoder.h"
#include "dvpp_mem_allocater_args.h"

uint64_t alloc_args_mem(uint32_t pool_id, uint64_t size, dvpp_sqe_args *sqe_args)
{
    dvpp_cmdlist_context *ctx = get_dvpp_cmdlist_ctx();
    uint64_t addr;

    BEGIN_TIME_STAMP(ctx, sqe_args->streamid, sqe_args->taskid, time_cost_vmalloc);

    addr = alloc_args_memory(pool_id, (uint32_t)size, sqe_args);
    if (addr == 0) {
        DVPP_CMDLIST_LOG_ERROR("malloc kernel args buf fail\n");
        return 0;
    }

    CALC_TIME_COST(ctx, sqe_args->streamid, sqe_args->taskid, time_cost_vmalloc);

    return addr;
}

int32_t copy_args_from_user(dvpp_save_para *save_para, dvpp_sqe_args *sqe_args)
{
    uint32_t n = 0;
    dvpp_cmdlist_context *ctx = get_dvpp_cmdlist_ctx();

    BEGIN_TIME_STAMP(ctx, sqe_args->streamid, sqe_args->taskid, time_cost_usr_copy_to_kern);

    n = ka_base_copy_from_user((void *)sqe_args->args_addr, (void *)save_para->args_uva, save_para->args_size);
    if (n != 0) {
        DVPP_CMDLIST_LOG_ERROR("copy args from user fail. need copy size is %llu, remain %u is not copied.\n",
                               save_para->args_size, n);
        return -1;
    }

    CALC_TIME_COST(ctx, sqe_args->streamid, sqe_args->taskid, time_cost_usr_copy_to_kern);

    return 0;
}

int32_t dvpp_comm_drv_ready(dvpp_sqe_args *sqe_args, uint64_t tlv_stru_addr, uint64_t *cmdbuf_size_used)
{
    int32_t ret = 0;
    dvpp_tlv_decoder tlv_dec;
    dvpp_decoder dec;
    struct CmdBuf cmd_buf;
    uint32_t tlv_node_space;
    tlv_dec.pbuf = (uint8_t *)(uintptr_t)(sqe_args->args_addr);
    tlv_dec.size = sqe_args->args_size;
    tlv_dec.tlv_type_struct = (dvpp_tlv_type_struct *)(tlv_stru_addr);
    tlv_dec.tlv_node_num = (sqe_args->is_use_mem_pool == 1) ? sqe_args->tlv_node_num : DVPP_MAX_NODE_NUM;
    tlv_node_space = (sqe_args->is_use_mem_pool == 1) ? (sqe_args->tlv_node_num * DVPP_MAX_TLV_TYPE) :
                                                        (DVPP_MAX_NODE_NUM * DVPP_MAX_TLV_TYPE);
    tlv_dec.tlv_type_struct_size = (tlv_node_space * sizeof(void *));
    dec.decoder = (void *)(&tlv_dec);
    dec.protocol = DVPP_PROTOCOL_TLV;

    ret = dvpp_init_decoder(&dec);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("init tlv decoder fail.\n");
        return -1;
    }

    ret = dvpp_decode_args(&dec);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("decode tlv args fail.\n");
        return -1;
    }

    dvpp_init_cmdbuf(sqe_args->cmdbuf_kva, sqe_args->cmdbuf_uva, sqe_args->cmdbuf_size, &cmd_buf);
    ret = dvpp_gen_cmdlist(&dec, &cmd_buf);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("dvpp generate cmdlist fail.\n");
        return -1;
    }
    *cmdbuf_size_used = dvpp_get_cmdbuf_real_size(&cmd_buf);

    (void)dvpp_gen_sqe(&dec, &cmd_buf, (struct dvpp_sqe *)sqe_args);

#ifdef BUILD_DEBUG
    dvpp_dump_cmdbuf(&cmd_buf);
    dvpp_dump_sqe((struct dvpp_sqe *)sqe_args);
    dvpp_dump_sqe_list(&cmd_buf);
#endif

    return 0;
}

uint64_t get_alloc_size(dvpp_sqe_args *sqe_args, uint64_t dma_info_size)
{
    uint64_t alloc_size = 0;
    uint32_t tlv_node_space = 0;
    tlv_node_space = (sqe_args->is_use_mem_pool == 1) ? sqe_args->tlv_node_num * DVPP_MAX_TLV_TYPE :
                                                        (DVPP_MAX_NODE_NUM * DVPP_MAX_TLV_TYPE);
    alloc_size = sqe_args->args_size + (tlv_node_space * sizeof(void *)) + dma_info_size;

    // 使用内存池场景。如果args所需内存，或者cmdbuf所需内存大于各自默认内存池中内存块大小，则不走内存池
    if ((sqe_args->is_use_mem_pool == 1) &&
        ((alloc_size > DVPP_DEFAULT_MEM_BLK_SIZE) || (sqe_args->cmdbuf_size > DVPP_DEFAULT_DMA_MEM_BLK_SIZE))) {
        // 如果内存大小过大，则不走内存池，需重新计算非内存池场景内存大小，并将标志位置为不走内存池
        alloc_size = sqe_args->args_size + (DVPP_MAX_NODE_NUM * DVPP_MAX_TLV_TYPE * sizeof(void *)) + dma_info_size;
        sqe_args->is_use_mem_pool = 0;
    }
    DVPP_CMDLIST_LOG_DEBUG("alloc_size %llu, is_use_mem_pool %u\n", alloc_size, sqe_args->is_use_mem_pool);

    return alloc_size;
}