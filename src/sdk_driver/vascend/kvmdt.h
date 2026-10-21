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

#ifndef _KVMDT_H_
#define _KVMDT_H_

#include "ka_compiler_pub.h"
#include "ka_memory_pub.h"
#include "dvt.h"
#include "domain_manage.h"

#define KVMDT_ERROR_PFN (~0UL)

/*
 * Specific MPT modules function collections.
 */
struct hw_kvmdt_ops {
    int (*register_mdev)(ka_device_t *dev, struct hw_dvt *dvt);
    void (*unregister_mdev)(ka_device_t *dev, struct hw_dvt *dvt);
    int (*inject_msix)(struct hw_vdavinci *vdavinci, u32 vector, int irq);
    int (*read_gpa)(struct hw_vdavinci *vdavinci, unsigned long gpa, void *buf, unsigned long len);
    int (*write_gpa)(struct hw_vdavinci *vdavinci, unsigned long gpa, void *buf, unsigned long len);
    unsigned long (*gfn_to_mfn)(struct hw_vdavinci *vdavinci, unsigned long gfn);
    bool (*is_valid_gfn)(struct hw_vdavinci *vdavinci, unsigned long gfn);
    int (*mmio_get)(void **dst, int *size, void *_vdavinci, int bar);
    int (*dma_pool_init)(struct hw_vdavinci *vdavinci);
    void (*dma_pool_uninit)(struct hw_vdavinci *vdavinci);
    int (*dma_get_iova)(struct hw_vdavinci *vdavinci, unsigned long gfn, unsigned long size, ka_sg_table_t **dma_sgt);
    void (*dma_put_iova)(ka_sg_table_t *dma_sgt);
    int (*dma_get_iova_batch)(struct hw_vdavinci *vdavinci, unsigned long *gfn, unsigned long *dma_addr,
                              unsigned long count);
    bool (*is_pfn_valid)(ka_device_t *dev, unsigned long pfn, unsigned long size);
};

ka_vdev *hw_vdavinci_create(struct kobject *kobj, ka_mdev_device_t *mdev);
int hw_vdavinci_remove(ka_mdev_device_t *mdev);
int hw_vdavinci_open(ka_mdev_device_t *mdev);
void hw_vdavinci_release(ka_mdev_device_t *mdev);
ssize_t hw_vdavinci_read(ka_mdev_device_t *mdev, char __ka_user *buf, size_t count, loff_t *ppos);
size_t hw_vdavinci_write(ka_mdev_device_t *mdev, const char __ka_user *buf, size_t count, loff_t *ppos);
int hw_vdavinci_mmap(ka_mdev_device_t *mdev, ka_vm_area_struct_t *vma);
long hw_vdavinci_ioctl(ka_mdev_device_t *mdev, unsigned int cmd, unsigned long arg);
void hw_vdavinci_release_vm(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom);
static inline bool hw_vdavinci_is_support_normal_nc(void)
{
#ifdef VM_ALLOW_ANY_UNCACHED
    if (VM_ALLOW_ANY_UNCACHED != VM_NONE) {
        return true;
    }
#endif
    return false;
}

unsigned long kvmdt_gfn_to_hva(struct hw_vdavinci *vdavinci, unsigned long gfn);
#endif
