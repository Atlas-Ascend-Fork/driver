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

#include "pbl_feature_loader.h"
#include "pbl_uda.h"

#include "svm_kern_log.h"
#include "svm_addr_desc.h"
#include "kmc_h2d.h"
#include "pcie_th_msg.h"
#include "dma_map_kernel.h"
#include "pcie_th_agent.h"

static int pcie_th_agent_dma_addr_get(u32 udevid, void *msg, u32 *reply_len)
{
    struct pcie_th_dma_addr_get_msg *msg_info = (struct pcie_th_dma_addr_get_msg *)msg;
    struct svm_dma_addr_info dma_info;
    struct svm_global_va dst_va;
    u64 i;
    int ret;

    if (msg_info->head.extend_num == 0) {
        svm_err("Invalid extend num. (udevid=%u; tgid=%u; va=0x%llx; size=0x%llx)\n", udevid, msg_info->tgid,
                msg_info->va, msg_info->size);
        return -EINVAL;
    }

    svm_global_va_pack(uda_get_host_id(), msg_info->tgid, msg_info->va, msg_info->size, &dst_va);
    ret = svm_dma_addr_get(udevid, msg_info->tgid, &dst_va, &dma_info, NULL);
    if (ret != 0) {
        svm_err("Get dma addr failed. (udevid=%u; tgid=%u; va=0x%llx; size=0x%llx)\n", udevid, msg_info->tgid,
                msg_info->va, msg_info->size);
        return ret;
    }

    if (msg_info->head.extend_num > dma_info.dma_addr_seg_num) {
        msg_info->head.extend_num = dma_info.dma_addr_seg_num;
    }

    for (i = 0; i < msg_info->head.extend_num; i++) {
        msg_info->seg[i] = dma_info.seg[i];
    }

    msg_info->seg[0].dma_addr = dma_info.seg[0].dma_addr + dma_info.first_seg_offset;
    msg_info->seg[0].size = dma_info.seg[0].size - dma_info.first_seg_offset;
    if (msg_info->head.extend_num >= dma_info.dma_addr_seg_num) {
        msg_info->seg[msg_info->head.extend_num - 1].size = dma_info.last_seg_len;
    }

    *reply_len = sizeof(struct pcie_th_dma_addr_get_msg) + sizeof(struct svm_dma_addr_seg) * msg_info->head.extend_num;

    if (msg_info->pin_flag == 0) {
        svm_dma_addr_put(udevid, msg_info->tgid, &dst_va);
    }

    return 0;
}

static int pcie_th_agent_dma_addr_put(u32 udevid, void *msg, u32 *reply_len)
{
    struct pcie_th_dma_addr_put_msg *msg_info = (struct pcie_th_dma_addr_put_msg *)msg;
    struct svm_global_va dst_va;

    svm_global_va_pack(uda_get_host_id(), msg_info->tgid, msg_info->va, msg_info->size, &dst_va);
    svm_dma_addr_put(udevid, msg_info->tgid, &dst_va);

    *reply_len = sizeof(struct pcie_th_dma_addr_put_msg);
    return 0;
}

static struct svm_kmc_d2h_recv_handle g_d2h_pcie_th_dma_addr_get_handle = {
    .func = pcie_th_agent_dma_addr_get,
    .raw_msg_size = sizeof(struct pcie_th_dma_addr_get_msg),
    .extend_gran_size = sizeof(struct svm_dma_addr_seg)};

static struct svm_kmc_d2h_recv_handle g_d2h_pcie_th_dma_addr_put_handle = {
    .func = pcie_th_agent_dma_addr_put, .raw_msg_size = sizeof(struct pcie_th_dma_addr_put_msg), .extend_gran_size = 0};

int svm_pcie_th_agent_init(void)
{
    svm_kmc_d2h_recv_handle_register(SVM_KMC_MSG_DMA_ADDR_GET, &g_d2h_pcie_th_dma_addr_get_handle);
    svm_kmc_d2h_recv_handle_register(SVM_KMC_MSG_DMA_ADDR_PUT, &g_d2h_pcie_th_dma_addr_put_handle);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_pcie_th_agent_init, FEATURE_LOADER_STAGE_3);
