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

#include "dvpp_cmdlist.h"
#include "dvpp_cmdlist_log.h"

static dvpp_cmdlist_engine_ops g_cmdlist_engine_items[DVPP_CMDLIST_ENGINE_TYPE_BUTT] = {0};

dvpp_cmdlist_engine_ops *dvpp_get_cmdlist_engine_ops(void)
{
    return g_cmdlist_engine_items;
}

void dvpp_seq_set_cmdnode_val(struct CmdNode *node, uint32_t val)
{
    if (((uint64_t)node->posAddr + sizeof(uint32_t)) > ((uint64_t)node->endAddr)) {
        DVPP_CMDLIST_LOG_ERROR("Cmdbuf size is insufficient. Current node dataSize is %u\n", node->dataSize);
        return;
    }
    *(node->posAddr++) = val;
    node->dataSize += sizeof(uint32_t);
}

void dvpp_ext_set_cmdnode_val(struct CmdNode *node, uint32_t val)
{
    if (((uint64_t)node->extAddr + sizeof(uint32_t)) > ((uint64_t)node->endAddr)) {
        DVPP_CMDLIST_LOG_ERROR("Cmdbuf size is insufficient. Current node extDataSize is %u\n", node->extDataSize);
        return;
    }
    *(node->extAddr++) = val;
    node->extDataSize += sizeof(uint32_t);
}

static void dvpp_init_splited_cmdbuf(struct CmdBuf *cmd_buf, uint32_t addr_offset, uint16_t batch_idx)
{
    cmd_buf->posAddr = (uint32_t *)((uint64_t)(uintptr_t)(cmd_buf->startAddr) + batch_idx * addr_offset);
    cmd_buf->cmdHeadNode = NULL;
    cmd_buf->cmdTailNode = NULL;
}

struct CmdNode *dvpp_alloc_one_cmdnode(struct CmdBuf *cmd_buf)
{
    uint32_t node_size = DVPP_ALIGN_UP(sizeof(struct CmdNode), ADDR_ALIGN_SIZE);
    uint64_t tmp_addr = DVPP_ALIGN_UP((uint64_t)(uintptr_t)(cmd_buf->posAddr), ADDR_ALIGN_SIZE);
    uint32_t *posAddr = (uint32_t *)(uintptr_t)tmp_addr;
    struct CmdNode *node = (struct CmdNode *)posAddr;

    if (((uint64_t)posAddr + node_size) > (uint64_t)(cmd_buf->endAddr)) {
        DVPP_CMDLIST_LOG_ERROR("alloc one cmdnode failed, node_size %u\n", node_size);
        return NULL;
    }
    posAddr += node_size / sizeof(uint32_t);
    node->startAddr = posAddr;
    node->posAddr = posAddr;
    node->endAddr = cmd_buf->endAddr;
    node->extAddr = node->startAddr;
    node->extDataSize = 0;
    node->dataSize = 0;
    node->len1 = 0;
    node->isNeedWrBack = 0;
    node->next = NULL;
    node->prev = NULL;
    if (cmd_buf->cmdHeadNode == NULL) {
        cmd_buf->cmdHeadNode = node;
        cmd_buf->cmdTailNode = cmd_buf->cmdHeadNode;
    } else {
        node->prev = cmd_buf->cmdTailNode;
        cmd_buf->cmdTailNode->next = node;
        cmd_buf->cmdTailNode = node;
    }
    cmd_buf->posAddr = cmd_buf->cmdTailNode->posAddr;

    return cmd_buf->cmdTailNode;
}

static void dvpp_update_cmdbuf_pos(struct CmdBuf *cmd_buf, struct CmdNode *node)
{
    cmd_buf->posAddr = node->posAddr;
}

static int32_t dvpp_fill_one_cmdnode(dvpp_decoder *decoder, struct CmdNode *node, uint32_t node_idx)
{
    uint32_t mod = dvpp_get_module_type(decoder);
    if (g_cmdlist_engine_items[mod].dvpp_fill_one_cmdnode != NULL) {
        g_cmdlist_engine_items[mod].dvpp_fill_one_cmdnode(decoder, node, node_idx);
        return 0;
    }

    return -1;
}

static int32_t dvpp_fill_sqelist(dvpp_decoder *decoder, struct CmdBuf *cmd_buf, uint32_t sqe_idx, uint32_t blkdim)
{
    uint32_t mod = dvpp_get_module_type(decoder);
    if (g_cmdlist_engine_items[mod].dvpp_fill_sqelist != NULL) {
        g_cmdlist_engine_items[mod].dvpp_fill_sqelist(cmd_buf, sqe_idx, blkdim);
        return 0;
    }

    return -1;
}

static int32_t dvpp_connect_all_cmdnode(dvpp_decoder *decoder, struct CmdBuf *cmd_buf)
{
    uint32_t mod = dvpp_get_module_type(decoder);
    if (g_cmdlist_engine_items[mod].dvpp_connect_all_cmdnode != NULL) {
        g_cmdlist_engine_items[mod].dvpp_connect_all_cmdnode(cmd_buf);
        return 0;
    }

    return -1;
}

static int32_t dvpp_check_param(dvpp_decoder *decoder, uint32_t node_idx)
{
    uint32_t mod = dvpp_get_module_type(decoder);
    if (g_cmdlist_engine_items[mod].dvpp_check_param != NULL) {
        return g_cmdlist_engine_items[mod].dvpp_check_param(decoder, node_idx);
    }

    return -1;
}

void dvpp_init_cmdbuf(uint64_t host_addr, uint64_t dev_addr, uint32_t len, struct CmdBuf *cmd_buf)
{
    cmd_buf->bufAddr = host_addr;
    cmd_buf->ioAddr = dev_addr;
    cmd_buf->bufSize = len;
    cmd_buf->startAddr = (uint32_t *)(DVPP_ALIGN_UP(cmd_buf->bufAddr, ADDR_ALIGN_SIZE) + SQE_LIST_LEN);
    cmd_buf->ioStartAddr = DVPP_ALIGN_UP(cmd_buf->ioAddr, ADDR_ALIGN_SIZE) + SQE_LIST_LEN;
    cmd_buf->posAddr = cmd_buf->startAddr;
    cmd_buf->endAddr = (uint32_t *)(host_addr + len);
    cmd_buf->cmdHeadNode = NULL;
    cmd_buf->cmdTailNode = NULL;
}

int32_t dvpp_gen_cmdlist(dvpp_decoder *decoder, struct CmdBuf *cmd_buf)
{
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t node_idx = 0;
    struct CmdNode *node = NULL;
    uint32_t batch_cnt = dvpp_get_batch_cnt(decoder);
    uint32_t split_size = (cmd_buf->bufSize - SQE_LIST_LEN) / batch_cnt;
    uint32_t addr_offset = DVPP_ALIGN_DOWN(split_size, ADDR_ALIGN_SIZE);
    uint32_t mod = dvpp_get_module_type(decoder);

    for (i = 0; i < batch_cnt; i++) {
        dvpp_init_splited_cmdbuf(cmd_buf, addr_offset, i);
        for (j = 0; j < dvpp_get_node_cnt_of_batch(decoder, i); j++) {
            if (dvpp_check_param(decoder, node_idx) != 0) {
                DVPP_CMDLIST_LOG_ERROR("the current param is insufficient to build the cmdlist for module %u\n", mod);
                return -1;
            }
            node = dvpp_alloc_one_cmdnode(cmd_buf);
            if (node == NULL) {
                DVPP_CMDLIST_LOG_ERROR("alloc cmdnode failed, node is NULL.\n");
                return -1;
            }
            if (dvpp_fill_one_cmdnode(decoder, node, node_idx) != 0) {
                DVPP_CMDLIST_LOG_ERROR("module %u fill one cmdnode fail.\n", mod);
                return -1;
            }
            dvpp_update_cmdbuf_pos(cmd_buf, node);
            node_idx++;
        }

        if (dvpp_connect_all_cmdnode(decoder, cmd_buf) != 0) {
            DVPP_CMDLIST_LOG_ERROR("module %u connect all cmdnodes fail.\n", mod);
            return -1;
        }
        if (dvpp_fill_sqelist(decoder, cmd_buf, i, batch_cnt) != 0) {
            DVPP_CMDLIST_LOG_ERROR("module %u fill sqelist fail.\n", mod);
            return -1;
        }
    }

    return 0;
}

uint64_t dvpp_get_cmdbuf_real_size(struct CmdBuf *cmd_buf)
{
    // 实际使用cmdbuf大小为，最后一个batch写完之后的位置posAddr减去cmdbuf的开始地址bufAddr
    return (uint64_t)(uintptr_t)(cmd_buf->posAddr) - (uint64_t)(uintptr_t)(cmd_buf->bufAddr);
}

void dvpp_gen_sqe(dvpp_decoder *decoder, struct CmdBuf *cmd_buf, struct dvpp_sqe *sqe)
{
    uint32_t mod = dvpp_get_module_type(decoder);
    uint32_t batch_cnt = dvpp_get_batch_cnt(decoder);
    if (batch_cnt == 1) {
        g_cmdlist_engine_items[mod].dvpp_gen_sqe(cmd_buf, DVPP_SQE_FIRST_PTR_MODE, batch_cnt, sqe);
    } else {
        g_cmdlist_engine_items[mod].dvpp_gen_sqe(cmd_buf, DVPP_SQE_SECOND_PTR_MODE, batch_cnt, sqe);
    }
}

void add_padding_data(struct CmdNode *node)
{
    uint32_t paddingSize;
    uint32_t i;
    if ((node->dataSize) % ADDR_ALIGN_SIZE) {
        paddingSize = ADDR_ALIGN_SIZE - (node->dataSize) % ADDR_ALIGN_SIZE;
        for (i = 0; i < paddingSize / ALIGN_LEN; ++i) {
            dvpp_seq_set_cmdnode_val(node, CMDLST_PADDING_DATA);
        }
    }
}

#ifdef BUILD_DEBUG
void dvpp_dump_cmdbuf(struct CmdBuf *cmd_buf)
{
    struct CmdNode *node = cmd_buf->cmdHeadNode;
    uint32_t *pos_addr = NULL;
    while (node != NULL) {
        pos_addr = node->startAddr;
        while (pos_addr < node->posAddr) {
            DVPP_CMDLIST_LOG_DEBUG("%016llx: %08x\n", (uint64_t)(pos_addr), *pos_addr);
            pos_addr++;
        }
        node = node->next;
    }
}

void dvpp_dump_sqe(struct dvpp_sqe *sqe)
{
    uint32_t i;
    for (i = 0; i < SQE_LEN / sizeof(uint32_t); i++) {
        DVPP_CMDLIST_LOG_DEBUG("sqe[%u]=%08x\n", i, sqe->filed[i]);
    }
}

void dvpp_dump_sqe_list(struct CmdBuf *cmd_buf)
{
    uint32_t i = 0;
    struct dvpp_sqe *sqe = (struct dvpp_sqe *)(cmd_buf->bufAddr);
    for (i = 0; i < DVPP_MAX_BATCH_NUM; i++) {
        DVPP_CMDLIST_LOG_DEBUG("sqe_list[%u]:\n", i);
        dvpp_dump_sqe(sqe + i);
    }
}
#endif
