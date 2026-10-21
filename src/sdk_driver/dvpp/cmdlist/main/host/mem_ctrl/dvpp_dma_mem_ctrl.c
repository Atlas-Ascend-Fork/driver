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
#include "dvpp_dma_mem_ctrl.h"

#include "comm_kernel_interface.h"
#include "vmng_kernel_interface.h"
#include "svm_mem_query.h"

#include "dvpp_cmdlist_log.h"

void *dvpp_dma_alloc(uint32_t dev_id, size_t size, dma_addr_t *dma_addr, ka_gfp_t gfp)
{
    void *dma_vir_addr = NULL;
    struct device *pcie_dev = NULL;
    if (size == 0) {
        DVPP_CMDLIST_LOG_ERROR("input para is invalid, size=%lu\n", size);
        return NULL;
    }

    if (dma_addr == NULL) {
        DVPP_CMDLIST_LOG_ERROR("dma addr is null, please check.\n");
        return NULL;
    }

    pcie_dev = hal_kernel_devdrv_get_pci_dev_by_devid(dev_id);
    if (pcie_dev == NULL) {
        DVPP_CMDLIST_LOG_ERROR("try to get pcie dev fail.\n");
        return NULL;
    }

    dma_vir_addr = hal_kernel_devdrv_dma_zalloc_coherent(pcie_dev, size, dma_addr, gfp);
    if (dma_vir_addr == NULL) {
        DVPP_CMDLIST_LOG_ERROR("alloc dma memory fail in phy scene.\n");
        return NULL;
    }

    return dma_vir_addr;
}

void dvpp_dma_free(uint32_t dev_id, size_t size, void *addr, dma_addr_t dma_addr)
{
    struct device *pcie_dev = NULL;
    if ((size == 0) || (addr == NULL)) {
        DVPP_CMDLIST_LOG_ERROR("input para is invalid, size=%lu\n", size);
        return;
    }

    if (dma_addr == 0ULL) {
        DVPP_CMDLIST_LOG_ERROR("dma addr is null, please check.\n");
        return;
    }

    pcie_dev = hal_kernel_devdrv_get_pci_dev_by_devid(dev_id);
    if (pcie_dev == NULL) {
        DVPP_CMDLIST_LOG_ERROR("try to get pcie dev fail.\n");
        return;
    }

    hal_kernel_devdrv_dma_free_coherent(pcie_dev, size, addr, dma_addr);

    return;
}

int32_t dvpp_dma_sync_link_copy(uint32_t dev_id, dvpp_dma_mem *dma_mem, dvpp_svm_pa_info *pa_info)
{
    struct devdrv_dma_node dma_node = {0};
    uint32_t i;
    int32_t ret;
    if ((dma_mem == NULL) || (pa_info == NULL) || (pa_info->pa_list == NULL) || (pa_info->size_list == NULL)) {
        DVPP_CMDLIST_LOG_ERROR("input arg is NULL.\n");
        return -1;
    }

    if (pa_info->num == 0) {
        DVPP_CMDLIST_LOG_ERROR("need copy block num is 0.\n");
        return -1;
    }

    for (i = 0; i < pa_info->num; i++) {
        // 虚拟地址连续
        dma_node.src_addr = dma_mem->phy_addr + (i == 0 ? 0 : pa_info->size_list[i]);
        // 物理地址不一定连续
        dma_node.dst_addr = pa_info->pa_list[i];
        dma_node.size = pa_info->size_list[i];
        dma_node.direction = DEVDRV_DMA_HOST_TO_DEVICE;
        // TD 这样会报多次中断，待优化
        ret = hal_kernel_devdrv_dma_sync_link_copy_extend(dev_id, DEVDRV_DMA_DATA_TRAFFIC, DEVDRV_DMA_WAIT_INTR,
                                                          &dma_node, 1);
        if (ret != 0) {
            DVPP_CMDLIST_LOG_ERROR("copy host dma memory to device fail. ret = %d, size = %u\n", ret, dma_node.size);
            return ret;
        }
    }

    return 0;
}
