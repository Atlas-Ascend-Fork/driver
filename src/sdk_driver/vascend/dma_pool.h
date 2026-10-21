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

#ifndef _DVT_DMA_POOL_H_
#define _DVT_DMA_POOL_H_

#include "domain_manage.h"

/* VDAVINCI_PAGES_OF_SCHEDULE = 1G
 * CONFIG_PREEMPT is not set and the time of
 * dealing pin/map pages MEM is less than watchdog_thresh(10s) */
#define VDAVINCI_PAGES_OF_SCHEDULE (((unsigned long)1 * (1 << 30)) / KA_MM_PAGE_SIZE)
#define VDAVINCI_PIN_PAGES_OF_SCHEDULE (VDAVINCI_PAGES_OF_SCHEDULE * 4)
#define VDAVINCI_MAP_PAGES_OF_SCHEDULE (VDAVINCI_PAGES_OF_SCHEDULE * 16)

#define VDAVINCI_TIME_OF_SCHEDULE 1 /* ms */
#define VDAVINCI_PIN_TIME_OF_SCHEDULE (VDAVINCI_TIME_OF_SCHEDULE * 100)
#define VDAVINCI_MAP_TIME_OF_SCHEDULE (VDAVINCI_TIME_OF_SCHEDULE * 200)

struct vdavinci_iova_info {
    /* sg table */
    unsigned long gfn;
    ka_sg_table_t **dma_sgt;
    /* dma array */
    unsigned long *gfn_array;
    unsigned long *dma_addr;
    unsigned long size;
    unsigned int sg_len;
    struct dev_dma_sgt **sgt_array;
};

void hw_vdavinci_unpin_pages(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom);
void hw_vdavinci_dma_pool_uninit(struct hw_vdavinci *vdavinci);
int hw_vdavinci_dma_pool_init(struct hw_vdavinci *vdavinci);
void hw_vdavinci_unplug_ram(struct hw_vdavinci *vdavinci, unsigned long start_gfn, unsigned long size);
void hw_vdavinci_put_iova(ka_sg_table_t *dma_sgt);
int hw_vdavinci_get_iova(struct hw_vdavinci *vdavinci, unsigned long gfn, unsigned long size, ka_sg_table_t **dma_sgt);
int hw_vdavinci_get_iova_batch(struct hw_vdavinci *vdavinci, unsigned long *gfn, unsigned long *dma_addr,
                               unsigned long count);
int hw_vdavinci_get_iova_sg(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom, unsigned long gfn,
                            unsigned long size, ka_sg_table_t **dma_sgt);
int hw_vdavinci_get_iova_array(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom, unsigned long *gfn,
                               unsigned long *dma_addr, unsigned long count);
int vf_get_iova_sg(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom, unsigned long gfn, unsigned long size,
                   ka_sg_table_t **dma_sgt);
int vf_get_iova_array(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom, unsigned long *gfn,
                      unsigned long *dma_addr, unsigned long count);
void hw_vdavinci_iommu_detach_group(struct hw_vdavinci *vdavinci);
int hw_vdavinci_iommu_attach_group(struct hw_vdavinci *vdavinci);
bool hw_vdavinci_scheduled(struct hw_vdavinci *vdavinci, unsigned long current_pages, unsigned long max_pages,
                           unsigned int timeout, ka_page_t *page);
bool is_vm_pfn_valid(ka_device_t *dev, unsigned long pfn, unsigned long size);
struct ram_range_info *get_ram_range_by_gfn(struct vm_dom_info *vm_dom, unsigned long gfn, unsigned long size);
#endif
