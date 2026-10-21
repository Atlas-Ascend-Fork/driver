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
#include "dvpp_sqe_update_phy.h"

#include "comm_kernel_interface.h"
#include "dvpp_dma_mem_ctrl.h"
#include "dvpp_tlv_decoder.h"
#include "dvpp_cmdlist.h"
#include "dvpp_cmdlist_log.h"
#include "dvpp_cmdlist_context.h"
#include "dvpp_cmdlist_util.h"
#include "dvpp_sqe_update_comm.h"
#include "dvpp_mem_allocater_args.h"
#include "dvpp_mem_allocater_dma.h"

static dvpp_dma_mem *alloc_host_dma(dvpp_comm_para *para, dvpp_sqe_args *sqe_args)
{
    dvpp_dma_mem *dma_mem = NULL;
    uint32_t tlv_node_space = 0;
    uint64_t offset = 0;
    dvpp_cmdlist_context *ctx = get_dvpp_cmdlist_ctx();

    BEGIN_TIME_STAMP(ctx, sqe_args->streamid, sqe_args->taskid, time_cost_dma_alloc);
    tlv_node_space = (sqe_args->is_use_mem_pool == 1) ? (sqe_args->tlv_node_num * DVPP_MAX_TLV_TYPE) :
                                                        (DVPP_MAX_NODE_NUM * DVPP_MAX_TLV_TYPE);
    offset = tlv_node_space * sizeof(void *);

    dma_mem = (dvpp_dma_mem *)(sqe_args->args_addr + sqe_args->args_size + offset);

    if (alloc_dma_memory(para->devid, sqe_args, sqe_args->cmdbuf_size, dma_mem) != 0) {
        DVPP_CMDLIST_LOG_ERROR("alloc host dma memory fail, size:%d\n", sqe_args->cmdbuf_size);
        return NULL;
    }

    CALC_TIME_COST(ctx, sqe_args->streamid, sqe_args->taskid, time_cost_dma_alloc);

    return dma_mem;
}

static dvpp_svm_pa_info *get_svm_pa_list(dvpp_comm_para *para, dvpp_sqe_args *sqe_args, uint64_t page_size,
                                         uint64_t pa_num, uint64_t cmdbuf_size_used)
{
    int32_t ret = 0;
    uint32_t tlv_node_space;
    uint64_t offset = 0;
    dvpp_svm_pa_info *pa_info = NULL;
    uint64_t mem_list_size = sizeof(uint64_t) * pa_num;
    tlv_node_space = (sqe_args->is_use_mem_pool == 1) ? (sqe_args->tlv_node_num * DVPP_MAX_TLV_TYPE) :
                                                        (DVPP_MAX_NODE_NUM * DVPP_MAX_TLV_TYPE);
    offset = (tlv_node_space * sizeof(void *)) + sizeof(dvpp_dma_mem);
    pa_info = (dvpp_svm_pa_info *)(sqe_args->args_addr + sqe_args->args_size + offset);
    pa_info->pa_list = (uint64_t *)((uint64_t)(uintptr_t)(pa_info) + sizeof(dvpp_svm_pa_info));
    pa_info->size_list = (uint64_t *)((uint64_t)(uintptr_t)(pa_info->pa_list) + mem_list_size);
    pa_info->num = pa_num;
    pa_info->page_size = page_size;
    ret = dvpp_get_pa_list_from_svm_addr(para->pid, sqe_args->cmdbuf_uva, cmdbuf_size_used, pa_info);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("get phy addr list based on svm addr fail, uva = %pK, size = %u, ret = %d\n",
                               (void *)(sqe_args->cmdbuf_uva), sqe_args->cmdbuf_size, ret);
        return NULL;
    }

    return pa_info;
}

static int32_t pcie_copy(dvpp_comm_para *para, dvpp_sqe_args *sqe_args, dvpp_dma_mem *dma_mem,
                         dvpp_svm_pa_info *pa_info)
{
    int32_t ret = 0;
    dvpp_cmdlist_context *ctx = get_dvpp_cmdlist_ctx();
    BEGIN_TIME_STAMP(ctx, sqe_args->streamid, sqe_args->taskid, time_cost_pcie_copy);

    ret = dvpp_dma_sync_link_copy(para->devid, dma_mem, pa_info);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("copy host dma memory to device fail. ret:%d\n", ret);
        return -1;
    }

    CALC_TIME_COST(ctx, sqe_args->streamid, sqe_args->taskid, time_cost_pcie_copy);

    return 0;
}

static void resource_destroy(dvpp_comm_para *comm_para, dvpp_save_para *save_para, dvpp_dma_mem *dma_mem,
                             dvpp_svm_pa_info *pa_info, dvpp_sqe_args *sqe_args)
{
    dvpp_put_pa_list(comm_para->pid, save_para->cmdbuf_uva, save_para->cmdbuf_size, pa_info);
    free_dma_memory(comm_para->devid, sqe_args, dma_mem);
    dma_mem->vir_addr = 0;

    // args内存包含了dma_mem，所以要在后面释放
    free_args_memory(comm_para->devid, sqe_args, save_para->args_kva);
    save_para->args_kva = 0;
}

static int32_t dvpp_drv_ready(dvpp_sqe_args *sqe_args, dvpp_comm_para *para)
{
    uint64_t mem_ctrl_size = 0;
    uint64_t mem_list_size = 0;
    uint64_t alloc_size = 0;
    dvpp_dma_mem *dma_mem = NULL;
    dvpp_svm_pa_info *pa_info = NULL;
    dvpp_save_para save_para;
    dvpp_sqe_args sqe_args_tmp;
    uint64_t page_size = 0;
    uint64_t pa_num = 0;
    uint64_t tlv_stru_addr = 0;
    uint32_t pool_id;
    uint64_t cmdbuf_size_used = 0;
    uint64_t dma_info_size = 0;

    DVPP_CMDLIST_LOG_DEBUG("args_size %u, cmdbuf_size %u, is_use_mem_pool %u, tlv_node_num %u\n", sqe_args->args_size,
                           sqe_args->cmdbuf_size, sqe_args->is_use_mem_pool, sqe_args->tlv_node_num);
    page_size = dvpp_get_page_size(para->pid, sqe_args->cmdbuf_uva, sqe_args->cmdbuf_size);
    if (page_size == 0) {
        DVPP_CMDLIST_LOG_ERROR("get page size fail.\n");
        goto FAIL0;
    }

    pa_num = dvpp_get_pa_num(sqe_args->cmdbuf_uva, sqe_args->cmdbuf_size, page_size);
    if (pa_num == 0) {
        DVPP_CMDLIST_LOG_ERROR("get page num fail.\n");
        goto FAIL0;
    }

    mem_ctrl_size = sizeof(dvpp_dma_mem) + sizeof(dvpp_svm_pa_info);
    mem_list_size = ((sizeof(uint64_t) * pa_num) << 1);
    pool_id = para->devid;
    dma_info_size = mem_ctrl_size + mem_list_size + 8; // 多申请8个字节，存放从内存池获取的dma node的地址
    alloc_size = get_alloc_size(sqe_args, dma_info_size);

    // 这里保存用户态args内存，后续会使用
    save_para.args_uva = sqe_args->args_addr;
    save_para.args_kva = alloc_args_mem(pool_id, alloc_size, sqe_args);
    save_para.args_size = sqe_args->args_size;
    if (save_para.args_kva == 0) {
        DVPP_CMDLIST_LOG_ERROR("alloc args kva fail.\n");
        goto FAIL0;
    }
    sqe_args->args_addr = save_para.args_kva;

    if (copy_args_from_user(&save_para, sqe_args)) {
        DVPP_CMDLIST_LOG_ERROR("copy args from user fail.\n");
        goto FAIL1;
    }

    dma_mem = alloc_host_dma(para, sqe_args);
    if (dma_mem == NULL) {
        DVPP_CMDLIST_LOG_ERROR("alloc host dma memory fail.\n");
        goto FAIL1;
    }
    sqe_args->cmdbuf_kva = dma_mem->vir_addr;

    // 这里保存是因为生成sqe的时候可能会修改下面的值，后续释放内存就会出错
    sqe_args_tmp = *sqe_args;
    save_para.cmdbuf_uva = sqe_args->cmdbuf_uva;
    save_para.cmdbuf_size = sqe_args->cmdbuf_size;
    save_para.is_use_mem_pool = sqe_args->is_use_mem_pool;
    tlv_stru_addr = sqe_args->args_addr + sqe_args->args_size;
    cmdbuf_size_used = sqe_args->cmdbuf_size; // 默认初始化为整个cmdbuf大小
    if (dvpp_comm_drv_ready(sqe_args, tlv_stru_addr, &cmdbuf_size_used) != 0) {
        DVPP_CMDLIST_LOG_ERROR("dvpp drv comm ready fail.\n");
        goto FAIL2;
    }

    pa_info = get_svm_pa_list(para, &sqe_args_tmp, page_size, pa_num, cmdbuf_size_used);
    if (pa_info == NULL) {
        DVPP_CMDLIST_LOG_ERROR("get svm pa list fail.\n");
        goto FAIL2;
    }

    if (pcie_copy(para, sqe_args, dma_mem, pa_info) != 0) {
        DVPP_CMDLIST_LOG_ERROR("pcie copy fail.\n");
        goto FAIL3;
    }

    sqe_args->is_use_mem_pool = save_para.is_use_mem_pool; // 生成sqe的时候被修改
    resource_destroy(para, &save_para, dma_mem, pa_info, sqe_args);
    return 0;

FAIL3:
    dvpp_put_pa_list(para->pid, save_para.cmdbuf_uva, save_para.cmdbuf_size, pa_info);
FAIL2:
    sqe_args->is_use_mem_pool = save_para.is_use_mem_pool; // 生成sqe的时候被修改
    free_dma_memory(pool_id, sqe_args, dma_mem);
    dma_mem->vir_addr = 0;
FAIL1:
    // args内存包含了dma_mem，所以要在后面释放
    free_args_memory(pool_id, sqe_args, save_para.args_kva);
    save_para.args_kva = 0;
FAIL0:
    return -1;
}

int32_t dvpp_sqe_update_phy_mach(uint32_t devid, int pid, void *sqe)
{
    dvpp_sqe_args *sqe_args = NULL;
    time_cost_node *tmp_node = NULL;
    dvpp_comm_para comm_para;
    dvpp_cmdlist_context *ctx = get_dvpp_cmdlist_ctx();

    sqe_args = (dvpp_sqe_args *)sqe;

    if (ctx->time_cost_stat.enable == 1) {
        tmp_node = alloc_time_cost_node_from_ctx();
        tmp_node->taskid = sqe_args->taskid;
        tmp_node->streamid = sqe_args->streamid;
        add_time_cost_node_to_ctx(tmp_node);
    }

    comm_para.devid = devid;
    comm_para.pid = pid;
    if (dvpp_drv_ready(sqe_args, &comm_para) != 0) {
        DVPP_CMDLIST_LOG_ERROR("dvpp drv ready fail.\n");
        return -1;
    }

    return 0;
}
