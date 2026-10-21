/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"
#include "kernel_cgroup_mem_adapt.h"
#include "pbl/pbl_runenv_config.h"

#include "comm_kernel_interface.h"
#include "svm_kernel_interface.h"

#include "soc_adapt.h"
#include "trs_chan.h"
#include "trs_chan_mem.h"
#include "trs_chan_mem_pool.h"
#include "trs_pm_adapt.h"
#include "trs_rsv_mem.h"
#include "trs_host_msg.h"
#include "trs_chan_near_ops_rsv_mem.h"
#include "trs_chan_near_ops_mem.h"
#include "trs_host_comm.h"
#include "trs_host_mode_config.h"

/* stub for david ub scene start */
#ifndef EMU_ST
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
ka_device_t *hal_kernel_devdrv_get_pci_dev_by_devid(u32 udevid)
{
    return NULL;
}

ka_dma_addr_t hal_kernel_devdrv_dma_map_single(ka_device_t *dev, void *ptr, size_t size, ka_dma_data_direction_t dir)
{
    return (ka_dma_addr_t)NULL;
}

void hal_kernel_devdrv_dma_unmap_single(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir)
{}

void *hal_kernel_devdrv_dma_alloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr, ka_gfp_t gfp)
{
    return NULL;
}

void hal_kernel_devdrv_dma_free_coherent(ka_device_t *dev, size_t size, void *addr, ka_dma_addr_t dma_addr)
{}
#endif
#endif
/* stub for david ub scene end */

static bool trs_chan_use_coherent_mem(struct trs_id_inst *inst)
{
    /* Avoid streaming DMA mappings and their bounce buffers for CC PCIe shared buffers. */
    return (devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_PCIE) && run_in_confidential_virtual_mach();
}

static void *trs_chan_mem_alloc_dma(struct trs_id_inst *inst, ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr,
                                    ka_dma_data_direction_t dir)
{
    void *vaddr = NULL;

    if (dev != NULL) {
        if (trs_chan_use_coherent_mem(inst)) {
            return hal_kernel_devdrv_dma_alloc_coherent(dev, size, dma_addr, KA_GFP_KERNEL);
        } else {
            vaddr = ka_alloc_pages_exact_ex(size,
                                            KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_ACCOUNT | __KA_GFP_RETRY_MAYFAIL);
            if (vaddr == NULL) {
                return NULL;
            }

            *dma_addr = hal_kernel_devdrv_dma_map_single(dev, vaddr, size, KA_DMA_BIDIRECTIONAL);
            if (ka_mm_dma_mapping_error(dev, *dma_addr)) {
                ka_free_pages_exact_ex(vaddr, size);
                return NULL;
            }
        }
    }

    return vaddr;
}

static void *trs_chan_mem_alloc_ddr_dma(struct trs_id_inst *inst, size_t size, phys_addr_t *paddr)
{
    return trs_chan_mem_alloc_dma(inst, hal_kernel_devdrv_get_pci_dev_by_devid(inst->devid), size,
                                  (ka_dma_addr_t *)paddr, KA_DMA_BIDIRECTIONAL);
}

static void trs_chan_mem_free_dma(struct trs_id_inst *inst, ka_device_t *dev, void *vaddr, ka_dma_addr_t dma_addr,
                                  size_t size)
{
    if ((dev != NULL) && trs_chan_use_coherent_mem(inst)) {
        hal_kernel_devdrv_dma_free_coherent(dev, size, vaddr, dma_addr);
        return;
    }

    if (dev != NULL) {
        hal_kernel_devdrv_dma_unmap_single(dev, dma_addr, size, KA_DMA_BIDIRECTIONAL);
    }

    ka_free_pages_exact_ex(vaddr, size);
}

static void trs_chan_mem_free_ddr_dma(struct trs_id_inst *inst, void *vaddr, size_t size, phys_addr_t paddr)
{
    trs_chan_mem_free_dma(inst, hal_kernel_devdrv_get_pci_dev_by_devid(inst->devid), vaddr, (ka_dma_addr_t)paddr, size);
}

static void *trs_chan_mem_alloc_ddr_phy(struct trs_id_inst *inst, size_t size, phys_addr_t *paddr)
{
    void *vaddr = NULL;

    vaddr = ka_alloc_pages_exact_ex(size, KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_ACCOUNT | __KA_GFP_RETRY_MAYFAIL);
    if (vaddr == NULL) {
        return NULL;
    }
    *paddr = ka_mm_virt_to_phys(vaddr);

    return vaddr;
}

static inline void trs_chan_mem_free_ddr_phy(struct trs_id_inst *inst, void *vaddr, size_t size, phys_addr_t paddr)
{
    ka_free_pages_exact_ex(vaddr, size);
}

static void *trs_chan_mem_get_ddr_dma(struct trs_id_inst *inst, size_t size, phys_addr_t *phy_addr)
{
    void *vaddr = NULL;

    vaddr = trs_chan_mem_get_from_mem_list(inst, size, phy_addr);
    if (vaddr == NULL) {
        vaddr = trs_chan_mem_alloc_ddr_dma(inst, size, phy_addr);
    }
    return vaddr;
}

static void trs_chan_mem_put_ddr_dma(struct trs_id_inst *inst, void *vaddr, size_t size, phys_addr_t phy_addr)
{
    int ret;
    struct trs_chan_mem_node_attr attr = {.inst = *inst,
                                          .vaddr = vaddr,
                                          .phy_addr = phy_addr,
                                          .size = size,
                                          .is_dma_addr = true,
                                          .ops.alloc_ddr = trs_chan_mem_alloc_ddr_dma,
                                          .ops.free_ddr = trs_chan_mem_free_ddr_dma};

    ret = trs_chan_mem_put_to_mem_list(&attr);
    if (ret != 0) {
        trs_chan_mem_free_ddr_dma(inst, vaddr, size, phy_addr);
    }
}

static void *trs_chan_mem_get_ddr_phy(struct trs_id_inst *inst, size_t size, phys_addr_t *phy_addr)
{
    void *vaddr = NULL;

    vaddr = trs_chan_mem_get_from_mem_list(inst, size, phy_addr);
    if (vaddr == NULL) {
        vaddr = trs_chan_mem_alloc_ddr_phy(inst, size, phy_addr);
    }
    return vaddr;
}

static void trs_chan_mem_put_ddr_phy(struct trs_id_inst *inst, void *vaddr, size_t size, phys_addr_t phy_addr)
{
    int ret;
    struct trs_chan_mem_node_attr attr = {.inst = *inst,
                                          .vaddr = vaddr,
                                          .phy_addr = phy_addr,
                                          .size = size,
                                          .is_dma_addr = false,
                                          .ops.alloc_ddr = trs_chan_mem_alloc_ddr_phy,
                                          .ops.free_ddr = trs_chan_mem_free_ddr_phy};

    ret = trs_chan_mem_put_to_mem_list(&attr);
    if (ret != 0) {
        trs_chan_mem_free_ddr_phy(inst, vaddr, size, phy_addr);
    }
}

static void *trs_chan_mem_alloc_pm_inst_rsv(struct trs_id_inst *inst, int type, size_t size, phys_addr_t *paddr,
                                            u32 flag)
{
    struct uda_mia_dev_para dev_para;
    struct trs_id_inst pm_inst;

    dev_para.phy_devid = inst->devid;
    if (!uda_is_phy_dev(inst->devid)) {
        int ret = uda_udevid_to_mia_devid(inst->devid, &dev_para);
        if (ret != 0) {
            trs_err("Failed to get devid. (devid=%u; ret=%d)\n", inst->devid, ret);
            return NULL;
        }
    }

    /* In container computing group, devid is 100+. devid need translated to phy_devid to alloc rsv mem */
    trs_id_inst_pack(&pm_inst, dev_para.phy_devid, inst->tsid);

    return trs_chan_mem_alloc_rsv(&pm_inst, type, size, paddr, 0);
}

static void trs_chan_mem_free_pm_inst_rsv(struct trs_id_inst *inst, int type, phys_addr_t *paddr, size_t size)
{
    struct uda_mia_dev_para dev_para;
    struct trs_id_inst pm_inst;

    dev_para.phy_devid = inst->devid;
    if (!uda_is_phy_dev(inst->devid)) {
#ifndef EMU_ST
        int ret = uda_udevid_to_mia_devid(inst->devid, &dev_para);
        if (ret != 0) {
            trs_err("Failed to get devid. (devid=%u; ret=%d)\n", inst->devid, ret);
            return;
        }
#endif
    }

    trs_id_inst_pack(&pm_inst, dev_para.phy_devid, inst->tsid);

    return trs_chan_mem_free_rsv(&pm_inst, type, paddr, size);
}

static void *trs_chan_mem_get_svm_mem(struct trs_id_inst *inst, size_t size, void *specified_uva,
                                      struct trs_chan_mem_attr *mem_attr)
{
#ifdef CFG_FEATURE_SQ_SUPPORT_SVM_MEM
    struct trs_sq_mem_handle handle = {0};
    int ret;

    if (specified_uva == NULL) {
        return NULL;
    }

    handle.mem_attr.addr = (u64)(uintptr_t)specified_uva;
    handle.mem_attr.size = size;
    if (trs_get_sq_send_mode(inst->devid) != TRS_MODE_TYPE_SQ_SEND_HIGH_PERFORMANCE) {
        handle.mem_attr.cp_only_flag = true;
    }
    ret = trs_host_get_sq_mem_handle(inst, ka_task_get_current_tgid(), &handle);
    if (ret != 0) {
        trs_err("Get sq mem handle failed. (devid=%u; size=%ld; ret=%d)\n", inst->devid, size, ret);
        return NULL;
    }

    mem_attr->pa_num = handle.pa_num;
    mem_attr->ka_attr = handle.mem_attr;
    mem_attr->pa_wraper_list = handle.pa_wraper_list;
    mem_attr->vmap_pfn_handle = handle.vmap_pfn_handle;
    mem_attr->phy_addr = handle.pa_wraper_list[0].pa;
    mem_attr->specified_uva = specified_uva;
    return ka_mm_vmap_pfn_kva(mem_attr->vmap_pfn_handle);
#else
    return NULL;
#endif
}

static void trs_chan_mem_put_svm_mem(struct trs_id_inst *inst, void *vaddr, struct trs_chan_mem_attr *mem_attr)
{
#ifdef CFG_FEATURE_SQ_SUPPORT_SVM_MEM
    struct trs_sq_mem_handle handle = {0};

    handle.mem_attr = mem_attr->ka_attr;
    handle.pa_num = mem_attr->pa_num;
    handle.vmap_pfn_handle = mem_attr->vmap_pfn_handle;
    handle.pa_wraper_list = mem_attr->pa_wraper_list;

    trs_host_put_sq_mem_handle(inst, mem_attr->tgid, &handle);

    mem_attr->pa_wraper_list = NULL;
    mem_attr->vmap_pfn_handle = NULL;
    mem_attr->pa_num = 0;
#endif
}

static inline bool is_host_mem(u32 mem_side)
{
    return ((mem_side == TRS_CHAN_HOST_MEM) || (mem_side == TRS_CHAN_HOST_PHY_MEM));
}

static inline bool is_size_out_of_range(size_t size)
{
    return ((size > 4096UL) || (size == 0)); /* 4096: sqcq mem size */
}

static int trs_chan_check_sqcq_mem_size(struct trs_id_inst *inst, u32 mem_side, size_t size)
{
    u32 host_flag;

    if (devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_HCCS) {
        /* host_flag == 0 means in virtual machine */
        if (devdrv_get_host_phy_mach_flag(inst->devid, &host_flag) != 0) {
            trs_err("Get host flag not support. (devid=%u;)\n", inst->devid);
            return -EINVAL;
        }
        /* In HCCS connection, virtual machine and using host mem, sqcq size should be less or equal 4K */
        if ((host_flag == 0) && is_host_mem(mem_side) && is_size_out_of_range(size)) {
            trs_err("Check sqcq mem size failed. (devid=%u; host_flag=0x%x; mem_side=%u; size=0x%lx)\n", inst->devid,
                    host_flag, mem_side, size);
            return -EINVAL;
        }
        trs_debug("Check sqcq mem size. (devid=%u; host_flag=0x%x; mem_side=%u; size=0x%lx)\n", inst->devid, host_flag,
                  mem_side, size);
    }
    return 0;
}

void *trs_chan_ops_sq_mem_alloc(struct trs_id_inst *inst, struct trs_chan_type *types, struct trs_chan_sq_para *sq_para,
                                struct trs_chan_mem_attr *mem_attr)
{
    size_t size = sq_para->sqe_size * sq_para->sq_depth;
    void *vaddr = NULL;
    u32 sq_mem_side;

    sq_mem_side = trs_soc_get_sq_mem_side(inst, types);
    if (trs_chan_check_sqcq_mem_size(inst, sq_mem_side, size) != 0) {
        return NULL;
    }

    mem_attr->mem_type = sq_mem_side;
    if (sq_mem_side == TRS_CHAN_DEV_RSV_MEM) {
        vaddr = trs_chan_mem_alloc_pm_inst_rsv(inst, RSV_MEM_HW_SQCQ, size, (phys_addr_t *)(&mem_attr->phy_addr), 0);
    } else if (sq_mem_side == TRS_CHAN_HOST_MEM) {
        vaddr = trs_chan_mem_alloc_ddr_dma(inst, size, &mem_attr->phy_addr);
        mem_attr->mem_type |= TRS_CHAN_MEM_LOCAL;
    } else if (sq_mem_side == TRS_CHAN_HOST_PHY_MEM) {
        vaddr = trs_chan_mem_alloc_ddr_phy(inst, size, &mem_attr->phy_addr);
        mem_attr->mem_type |= TRS_CHAN_MEM_LOCAL;
    } else if (sq_mem_side == TRS_CHAN_DEV_MEM_PRI) {
        vaddr = trs_chan_mem_alloc_pm_inst_rsv(inst, RSV_MEM_HW_SQCQ, size, (phys_addr_t *)(&mem_attr->phy_addr), 0);
        if (vaddr == NULL) {
            if (trs_chan_check_sqcq_mem_size(inst, TRS_CHAN_HOST_MEM, size) != 0) {
                return NULL;
            }
            vaddr = trs_chan_mem_alloc_ddr_dma(inst, size, &mem_attr->phy_addr);
            mem_attr->mem_type = TRS_CHAN_HOST_MEM | TRS_CHAN_MEM_LOCAL;
        } else {
            mem_attr->mem_type = TRS_CHAN_DEV_RSV_MEM;
        }
    } else if (sq_mem_side == TRS_CHAN_DEV_SVM_MEM) {
        vaddr = trs_chan_mem_get_svm_mem(inst, size, sq_para->sq_que_uva, mem_attr);
    }

    mem_attr->size = size;
    mem_attr->tgid = ka_task_get_current_tgid();
    if (sq_mem_side != TRS_CHAN_DEV_SVM_MEM) {
        mem_attr->pa_num = 1;
    }
    return vaddr;
}

void trs_chan_ops_sq_mem_free(struct trs_id_inst *inst, struct trs_chan_type *types, void *sq_addr,
                              struct trs_chan_mem_attr *mem_attr)
{
    u32 mem_side;

    mem_side = trs_chan_get_sqcq_mem_side(mem_attr->mem_type);
    if (mem_side == TRS_CHAN_DEV_RSV_MEM) {
        trs_chan_mem_free_pm_inst_rsv(inst, RSV_MEM_HW_SQCQ, sq_addr, mem_attr->size);
    } else if (mem_side == TRS_CHAN_HOST_MEM) {
        trs_chan_mem_free_ddr_dma(inst, sq_addr, mem_attr->size, mem_attr->phy_addr);
    } else if (mem_side == TRS_CHAN_HOST_PHY_MEM) {
        trs_chan_mem_free_ddr_phy(inst, sq_addr, mem_attr->size, mem_attr->phy_addr);
    } else if (mem_side == TRS_CHAN_DEV_SVM_MEM) {
        trs_chan_mem_put_svm_mem(inst, sq_addr, mem_attr);
    }
}

void *trs_chan_sq_mem_alloc(struct trs_id_inst *inst, u32 sqe_size, u32 sq_depth, struct trs_chan_mem_attr *mem_attr)
{
    struct trs_chan_sq_para sq_para;
    struct trs_chan_type types = {.type = CHAN_TYPE_HW, .sub_type = CHAN_SUB_TYPE_HW_RTS};

    sq_para.sqe_size = sqe_size;
    sq_para.sq_depth = sq_depth;

    return trs_chan_ops_sq_mem_alloc(inst, &types, &sq_para, mem_attr);
}
KA_EXPORT_SYMBOL_GPL(trs_chan_sq_mem_alloc);

void trs_chan_sq_mem_free(struct trs_id_inst *inst, void *sq_addr, struct trs_chan_mem_attr *mem_attr)
{
    struct trs_chan_type types = {.type = CHAN_TYPE_HW, .sub_type = CHAN_SUB_TYPE_HW_RTS};

    return trs_chan_ops_sq_mem_free(inst, &types, sq_addr, mem_attr);
}
KA_EXPORT_SYMBOL_GPL(trs_chan_sq_mem_free);

void *trs_chan_ops_cq_mem_alloc(struct trs_id_inst *inst, struct trs_chan_type *types, struct trs_chan_cq_para *cq_para,
                                struct trs_chan_mem_attr *mem_attr)
{
    size_t size = cq_para->cqe_size * cq_para->cq_depth;
    void *vaddr = NULL;
    u32 cq_mem_side;

    cq_mem_side = trs_soc_get_cq_mem_side(inst);
    if (trs_chan_check_sqcq_mem_size(inst, cq_mem_side, size) != 0) {
        return NULL;
    }
    mem_attr->mem_type = cq_mem_side;
    if (cq_mem_side == TRS_CHAN_DEV_RSV_MEM) {
        /* To reduce time consumption, drv not clear rsv type cq mem. Tsfw clear when sqcq creat mbox operation */
        vaddr = trs_chan_mem_alloc_rsv(inst, RSV_MEM_HW_SQCQ, size, (phys_addr_t *)(&mem_attr->phy_addr), 0);
    } else if (cq_mem_side == TRS_CHAN_HOST_MEM) {
        vaddr = trs_chan_mem_get_ddr_dma(inst, size, &mem_attr->phy_addr);
        mem_attr->mem_type |= TRS_CHAN_MEM_LOCAL;
    } else if (cq_mem_side == TRS_CHAN_HOST_PHY_MEM) {
        vaddr = trs_chan_mem_get_ddr_phy(inst, size, &mem_attr->phy_addr);
        mem_attr->mem_type |= TRS_CHAN_MEM_LOCAL;
    }

    mem_attr->size = size;
    mem_attr->pa_num = 1;
    return vaddr;
}

void trs_chan_ops_cq_mem_free(struct trs_id_inst *inst, struct trs_chan_type *types, void *cq_addr,
                              struct trs_chan_mem_attr *mem_attr)
{
    u32 mem_side;

    mem_side = trs_chan_get_sqcq_mem_side(mem_attr->mem_type);
    if (mem_side == TRS_CHAN_DEV_RSV_MEM) {
        trs_chan_mem_free_rsv(inst, RSV_MEM_HW_SQCQ, cq_addr, mem_attr->size);
    } else if (mem_side == TRS_CHAN_HOST_MEM) {
        trs_chan_mem_put_ddr_dma(inst, cq_addr, mem_attr->size, mem_attr->phy_addr);
    } else if (mem_side == TRS_CHAN_HOST_PHY_MEM) {
        trs_chan_mem_put_ddr_phy(inst, cq_addr, mem_attr->size, mem_attr->phy_addr);
    }
}

void trs_chan_ops_flush_sqe_cache(struct trs_id_inst *inst, u32 mem_type, void *addr, u64 pa, u32 len)
{
    ka_device_t *dev = NULL;

    if (devdrv_get_connect_protocol(inst->devid) != CONNECT_PROTOCOL_UB) {
        u32 sq_mem_side = trs_chan_get_sqcq_mem_side(mem_type);
        if (!trs_chan_mem_is_dev_mem(sq_mem_side) && !trs_chan_use_coherent_mem(inst)) {
            ka_dma_addr_t dma_addr = (ka_dma_addr_t)pa;
            dev = hal_kernel_devdrv_get_pci_dev_by_devid(inst->devid);
            if (dev == NULL) {
                return;
            }
            ka_mm_dma_sync_single_for_device(dev, dma_addr, len, KA_DMA_TO_DEVICE);
        }
    }
}

void trs_chan_flush_sqe_cache(struct trs_id_inst *inst, u32 mem_type, u64 pa, u32 len)
{
#ifndef EMU_ST
    if (devdrv_get_connect_protocol(inst->devid) != CONNECT_PROTOCOL_UB) {
        trs_chan_ops_flush_sqe_cache(inst, mem_type, NULL, pa, len);
    }
#endif
}
KA_EXPORT_SYMBOL_GPL(trs_chan_flush_sqe_cache);

void trs_chan_ops_invalid_cqe_cache(struct trs_id_inst *inst, u32 mem_type, void *addr, u64 pa, u32 len)
{
    ka_device_t *dev = NULL;

    if (devdrv_get_connect_protocol(inst->devid) != CONNECT_PROTOCOL_UB) {
        if (!trs_chan_mem_is_dev_mem(trs_soc_get_cq_mem_side(inst)) && !trs_chan_use_coherent_mem(inst)) {
            ka_dma_addr_t dma_addr = (ka_dma_addr_t)pa;
            dev = hal_kernel_devdrv_get_pci_dev_by_devid(inst->devid);
            if (dev == NULL) {
                return;
            }
            ka_mm_dma_sync_single_for_cpu(dev, dma_addr, len, KA_DMA_FROM_DEVICE);
        }
    }
}

static int trs_chan_devmem_addr_h2d(u32 devid, u64 host_addr, u64 *dev_addr)
{
    struct uda_mia_dev_para mia_para;
    u32 phy_devid;
    int ret;

    /* Todo: tmp for vf not config bar4 */
    if (uda_is_phy_dev(devid)) {
        phy_devid = devid;
    } else {
        ret = uda_udevid_to_mia_devid(devid, &mia_para);
        if (ret != 0) {
            trs_err("uda_udevid_to_mia_devid failed. (ret=%d; udevid=%u)\n", ret, devid);
            return ret;
        }
        phy_devid = mia_para.phy_devid;
    }

    return devdrv_devmem_addr_h2d(phy_devid, host_addr, dev_addr);
}

int trs_chan_near_sqcq_mem_h2d(struct trs_id_inst *inst, u64 host_addr, u64 *dev_addr, u32 mem_side)
{
    int ret = 0;

    if (mem_side == TRS_CHAN_DEV_RSV_MEM) {
        ret = trs_chan_near_sqcq_rsv_mem_h2d(inst, host_addr, dev_addr);
    } else { // TRS_CHAN_DEV_SVM_MEM
        ret = trs_chan_devmem_addr_h2d(inst->devid, host_addr, dev_addr);
    }

    if (ret != 0) {
        trs_err("Mem h2d failed. (devid=%u; tsid=%u; mem_side=%u; ret=%d)\n", inst->devid, inst->tsid, mem_side, ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_near_sqcq_mem_h2d);

static int _trs_chan_sq_rsvmem_map(struct trs_id_inst *inst, u64 host_paddr, u32 host_pid,
                                   struct trs_chan_sq_para *sq_param, u64 *dev_vaddr)
{
#ifndef EMU_ST
    struct trs_msg_map_sq_mem *msg_map_sq_mem = NULL;
    struct trs_msg_data msg = {0};
    struct uda_mia_dev_para mia_para;
    u64 dev_paddr;
    int ret;

    ret = trs_chan_near_sqcq_rsv_mem_h2d(inst, host_paddr, &dev_paddr);
    if (ret != 0) {
        trs_err("Rsv mem h2d failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return ret;
    }

    msg.header.cmdtype = TRS_MSG_SQ_RSVMEM_MAP;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.tsid = inst->tsid;
    msg.header.result = 0;
    msg_map_sq_mem = (struct trs_msg_map_sq_mem *)msg.payload;
    msg_map_sq_mem->remote_tgid = (int)host_pid;
    msg_map_sq_mem->dev_paddr = dev_paddr;
    msg_map_sq_mem->size = KA_MM_PAGE_ALIGN(sq_param->sqe_size * sq_param->sq_depth);
    msg_map_sq_mem->phy_devid = inst->devid;
    msg_map_sq_mem->vfid = -1;
    if (!uda_is_phy_dev(inst->devid)) {
        int ret = uda_udevid_to_mia_devid(inst->devid, &mia_para);
        if (ret != 0) {
            trs_err("Failed to get devid. (devid=%u; ret=%d)\n", inst->devid, ret);
            return ret;
        }
        msg_map_sq_mem->phy_devid = mia_para.phy_devid;
        msg_map_sq_mem->vfid = mia_para.sub_devid;
    }
    trs_debug("devid info. (devid=%u; phy_devid=%u; vfid=%u)\n", inst->devid, msg_map_sq_mem->phy_devid,
              msg_map_sq_mem->vfid);
    ret = trs_host_msg_send(msg_map_sq_mem->phy_devid, &msg, sizeof(struct trs_msg_data));
    if ((ret != 0) && (ret != -ESRCH)) {
        trs_err("Msg send fail. (devid=%u; phy_devid=%u; vfid=%d, ret=%d; header_result=%d)\n", inst->devid,
                msg_map_sq_mem->phy_devid, msg_map_sq_mem->vfid, ret, msg.header.result);
        return ret;
    }

    *dev_vaddr = msg_map_sq_mem->sq_map.va;
    return 0;
#endif
}

int trs_chan_ops_sq_rsvmem_map(struct trs_id_inst *inst, struct trs_sq_mem_map_para *para, void **sq_dev_vaddr)
{
#ifndef EMU_ST
    u64 dev_vaddr;
    int ret;

    if (!trs_soc_support_sq_rsvmem_map(inst)) {
        *sq_dev_vaddr = NULL;
        return 0;
    }

    if ((para->chan_types.type != CHAN_TYPE_HW) ||
        (trs_chan_get_sqcq_mem_side(para->mem_type) != TRS_CHAN_DEV_RSV_MEM)) {
        *sq_dev_vaddr = NULL;
        return 0;
    }

    ret = _trs_chan_sq_rsvmem_map(inst, para->sq_phy_addr, (u32)para->host_pid, &para->sq_para, &dev_vaddr);
    if (ret != 0) {
        *sq_dev_vaddr = NULL;
        return ret;
    }

    *sq_dev_vaddr = (void *)(uintptr_t)dev_vaddr;
#endif
    return 0;
}

int trs_chan_sq_rsvmem_map(struct trs_id_inst *inst, struct trs_sq_mem_map_para *para, void **sq_dev_vaddr)
{
    return trs_chan_ops_sq_rsvmem_map(inst, para, sq_dev_vaddr);
}
KA_EXPORT_SYMBOL_GPL(trs_chan_sq_rsvmem_map);

static int _trs_chan_sq_rsvmem_unmap(struct trs_id_inst *inst, struct trs_chan_sq_para *sq_para, u32 pid,
                                     void *dev_vaddr)
{
#ifndef EMU_ST
    struct trs_msg_map_sq_mem *msg_map_sq_mem = NULL;
    struct uda_mia_dev_para mia_para;
    struct trs_msg_data msg;
    int ret;

    msg.header.cmdtype = TRS_MSG_SQ_RSVMEM_UNMAP;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.tsid = inst->tsid;
    msg_map_sq_mem = (struct trs_msg_map_sq_mem *)msg.payload;
    msg_map_sq_mem->remote_tgid = (int)pid;
    msg_map_sq_mem->size = KA_MM_PAGE_ALIGN(sq_para->sqe_size * sq_para->sq_depth);
    msg_map_sq_mem->sq_map.va = (u64)dev_vaddr;
    msg_map_sq_mem->sq_map.size = KA_MM_PAGE_ALIGN(sq_para->sqe_size * sq_para->sq_depth);
    msg_map_sq_mem->phy_devid = inst->devid;
    msg_map_sq_mem->vfid = -1;
    if (!uda_is_phy_dev(inst->devid)) {
        int ret = uda_udevid_to_mia_devid(inst->devid, &mia_para);
        if (ret != 0) {
            trs_err("Failed to get devid. (devid=%u; ret=%d)\n", inst->devid, ret);
            return ret;
        }
        msg_map_sq_mem->phy_devid = mia_para.phy_devid;
        msg_map_sq_mem->vfid = mia_para.sub_devid;
    }
    ret = trs_host_msg_send(msg_map_sq_mem->phy_devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        trs_err("Msg send fail. (devid=%u; phy_devid=%d; ret=%d; header_result=%d)\n", inst->devid,
                msg_map_sq_mem->phy_devid, ret, msg.header.result);
        return ret;
    }
    return 0;
#endif
}

int trs_chan_ops_sq_rsvmem_unmap(struct trs_id_inst *inst, struct trs_sq_mem_map_para *para, void *sq_dev_vaddr)
{
#ifndef EMU_ST
    if (sq_dev_vaddr == NULL) {
        return 0;
    }

    if (!trs_soc_support_sq_rsvmem_map(inst)) {
        return 0;
    }
    if ((para->chan_types.type != CHAN_TYPE_HW) ||
        (trs_chan_get_sqcq_mem_side(para->mem_type) != TRS_CHAN_DEV_RSV_MEM)) {
        return 0;
    }

    return _trs_chan_sq_rsvmem_unmap(inst, &para->sq_para, (u32)para->host_pid, sq_dev_vaddr);
#else
    return 0;
#endif
}

void trs_chan_sq_rsvmem_unmap(struct trs_id_inst *inst, struct trs_sq_mem_map_para *para, void *sq_dev_vaddr)
{
    trs_chan_ops_sq_rsvmem_unmap(inst, para, sq_dev_vaddr);
}
KA_EXPORT_SYMBOL_GPL(trs_chan_sq_rsvmem_unmap);
