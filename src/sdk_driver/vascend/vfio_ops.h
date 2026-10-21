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

#ifndef _VFIO_OPS_H_
#define _VFIO_OPS_H_

#include "dvt.h"
#include "kvmdt.h"
#include "ka_memory_pub.h"

void vdavinci_iommu_unmap(ka_device_t *dev, unsigned long iova, size_t size);
int vdavinci_iommu_map(ka_device_t *dev, unsigned long iova, phys_addr_t paddr, size_t size, int prot);
void vdavinci_unpin_pages(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info);

/**
 * vdavinci_pin_pages - pin guest pages for DMA
 * @vdavinci: a vdavinci
 * @pin_info: pages to pin
 *
 * Context: the vpmem path calls g_hw_kvmdt_ops.gfn_to_mfn directly, which
 * walks vm_dom->ram_info_list without the read-side protection the interface
 * layer provides. Callers must already hold the vm_dom->sem write side; use
 * hw_dvt_hypervisor_gfn_to_mfn in lock-free contexts instead.
 */
int vdavinci_pin_pages(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info);
int vdavinci_register_device(ka_device_t *dev, ka_dvt_dev *dvt, const char *name);
void vdavinci_unregister_device(ka_device_t *dev, ka_dvt_dev *dvt);
int vdavinci_register_vfio_group(struct hw_vdavinci *vdavinci);
void vdavinci_unregister_vfio_group(struct hw_vdavinci *vdavinci);
int vdavinci_rw_gpa(struct hw_vdavinci *vdavinci, unsigned long gpa, void *buf, unsigned long len, bool write);
ka_dentry_t *vdavinci_debugfs_create_dir(const char *name, ka_dentry_t *parent);
void vdavinci_debugfs_remove(ka_dentry_t *dentry);
void vdavinci_put_vfio_device(struct hw_vdavinci *vdavinci);
int vdavinci_get_vfio_device(struct hw_vdavinci *vdavinci);
int vdavinci_iommu_notify(void *data, unsigned long iova, unsigned long size);
int vdavinci_group_notify(void *data, void *group_data);
void vdavinci_vfio_dma_unmap(void *data, u64 iova, u64 length);
struct hw_vdavinci *get_vdavinci_from_dev(ka_device_t *dev);
struct hw_vdavinci *get_vdavinci_from_mdev(ka_mdev_device_t *mdev);
void vdavinci_check(struct hw_vdavinci *vdavinci);
bool vdavinci_pfn_is_vpmem(unsigned long pfn);

/**
 * vdavinci_gfn_is_vpmem - check whether a gfn maps to vpmem
 * @vdavinci: a vdavinci
 * @gfn: guest pfn
 *
 * Context: same as vdavinci_pin_pages, calls g_hw_kvmdt_ops.gfn_to_mfn
 * directly and relies on the caller holding the vm_dom->sem write side.
 */
bool vdavinci_gfn_is_vpmem(struct hw_vdavinci *vdavinci, unsigned long gfn);
void vfio_ops_module_exit(void);
int vfio_ops_module_init(void);
#endif
