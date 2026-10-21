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

#include "ka_compiler_pub.h"
#include "ka_task_pub.h"
#include "ka_pci_pub.h"
#include "dvt.h"
#include "kvmdt.h"
#include "vfio_ops.h"

/**
 * hw_dvt_hypervisor_inject_msix - inject a MSIX interrupt into vdavinci
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_inject_msix(void *__vdavinci, u32 vector, int irq)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;
    u16 command, control;
    int ret = 0;

    if (ka_unlikely(vdavinci == NULL)) {
        return -EINVAL;
    }
    if (ka_unlikely(!vdavinci->msix_injection_allowed)) {
        command = *(u16 *)(&vdavinci_cfg_space(vdavinci)[KA_PCI_COMMAND]);
        control = *(u16 *)(&vdavinci_cfg_space(vdavinci)[DAVINCI_PCI_MSIX_FLAGS]);

        if (!(command & KA_PCI_COMMAND_INTX_DISABLE) || !(control & KA_PCI_MSIX_FLAGS_ENABLE) ||
            (control & KA_PCI_MSIX_FLAGS_MASKALL)) {
            vascend_info(vdavinci->dvt->vdavinci_priv->dev, "msix cap flag doesn't support\n");
            return -EPERM;
        }
        vdavinci->msix_injection_allowed = true;
    }

    ka_task_rcu_read_lock();
    ret = g_hw_kvmdt_ops.inject_msix(vdavinci, vector, irq);
    ka_task_rcu_read_unlock();

    return ret;
}

/**
 * hw_dvt_hypervisor_read_gpa - copy data from GPA to host data buffer
 * @vdavinci: a vdavinci
 * @gpa: guest physical addr
 * @buf: host data buffer
 * @len: data length
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_read_gpa(void *__vdavinci, unsigned long gpa, void *buf, unsigned long len)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL || buf == NULL) {
        return -EINVAL;
    }
    return g_hw_kvmdt_ops.read_gpa(vdavinci, gpa, buf, len);
}

/**
 * hw_dvt_hypervisor_write_gpa - copy data from host data buffer to GPA
 * @vdavinci: a vdavinci
 * @gpa: guest physical addr
 * @buf: host data buffer
 * @len: data length
 *
 * Returns:
 * Zero on success, negative error code if failed.
 */
int hw_dvt_hypervisor_write_gpa(void *__vdavinci, unsigned long gpa, void *buf, unsigned long len)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL || buf == NULL) {
        return -EINVAL;
    }
    return g_hw_kvmdt_ops.write_gpa(vdavinci, gpa, buf, len);
}

/**
 * hw_dvt_hypervisor_gfn_to_mfn - translate a GFN to MFN
 * @vdavinci: a vdavinci
 * @gpfn: guest pfn
 *
 * Returns:
 * MFN on success, KVMDT_ERROR_PFN if failed.
 *
 * The lookup walks vm_dom->ram_info_list, which is mutated under
 * vm_dom->sem by the unplug/unpin paths: take the read side so a
 * concurrent unplug cannot free the ram_info being read. Callers must
 * be in a sleepable context (the kvmdt fallback already calls
 * might_sleep). The in-module pin path calls g_hw_kvmdt_ops.gfn_to_mfn
 * directly instead: it runs under the sem write side and must not take
 * the read side here (rwsem is not recursive).
 */
unsigned long hw_dvt_hypervisor_gfn_to_mfn(void *__vdavinci, unsigned long gfn)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;
    struct vm_dom_info *vm_dom = NULL;
    unsigned long mfn = KVMDT_ERROR_PFN;

    if (vdavinci == NULL) {
        return KVMDT_ERROR_PFN;
    }

    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    if (vdavinci->vdev.domain == NULL) {
        ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
        return KVMDT_ERROR_PFN;
    }
    vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;
    ka_task_down_read(&vm_dom->sem);
    /* unlock to support concurrent read */
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);

    mfn = g_hw_kvmdt_ops.gfn_to_mfn(vdavinci, gfn);

    ka_task_up_read(&vm_dom->sem);
    return mfn;
}

/**
 * hw_dvt_hypervisor_dma_pool_init - dma pool init
 * @vdavinci: a vdavinci
 *
 * Returns:
 * 0 on success, negative error code if failed.
 */
int hw_dvt_hypervisor_dma_pool_init(void *__vdavinci)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL) {
        return -EINVAL;
    }

    return g_hw_kvmdt_ops.dma_pool_init(vdavinci);
}

/**
 * hw_dvt_hypervisor_dma_pool_uninit - dma pool uninit
 * @vdavinci: a vdavinci
 */
void hw_dvt_hypervisor_dma_pool_uninit(void *__vdavinci)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL) {
        return;
    }
    g_hw_kvmdt_ops.dma_pool_uninit(vdavinci);
}

/**
 * hw_dvt_hypervisor_dma_map_guest_page - setup dma map for guest page
 * @vdavinci: a vdavinci
 * @gfn: guest pfn
 * @size: page size
 * @dma_sgt: retrieve allocated dma addr list(sg_table)
 *
 * Returns:
 * 0 on success, negative error code if failed.
 */
int hw_dvt_hypervisor_dma_map_guest_page(void *__vdavinci, unsigned long gfn, unsigned long size,
                                         ka_sg_table_t **dma_sgt)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL || dma_sgt == NULL) {
        return -EINVAL;
    }

    return g_hw_kvmdt_ops.dma_get_iova(vdavinci, gfn, size, dma_sgt);
}

/**
 * hw_dvt_hypervisor_dma_unmap_guest_page - cancel dma map for guest page
 * @vdavinci: a vdavinci
 * @dma_sgt: the dma addr list(sg_table)
 */
void hw_dvt_hypervisor_dma_unmap_guest_page(void *__vdavinci, ka_sg_table_t *dma_sgt)
{
    if (__vdavinci == NULL || dma_sgt == NULL) {
        ka_dfx_pr_err("vdavinci or dma_sgt is null\n");
        return;
    }

    return g_hw_kvmdt_ops.dma_put_iova(dma_sgt);
}

/**
 * hw_dvt_hypervisor_dma_pool_active - dma pool active or not
 * @__vdavinci: a vdavinci
 *
 * Returns:
 * true when this vdavinci's dev_dom is mapped and ready, false otherwise.
 *
 * The check mirrors hw_vdavinci_get_iova: take cache_lock to read vdev.domain
 * safely, then the vm_dom->sem read side so a concurrent unplug/unmap cannot
 * free the dev_dom being checked. Caller must be in a sleepable context.
 */
bool hw_dvt_hypervisor_dma_pool_active(void *__vdavinci)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;
    struct vm_dom_info *vm_dom = NULL;
    struct dev_dom_info *dev_dom = NULL;
    bool active = false;

    if (__vdavinci == NULL) {
        return false;
    }

    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    if (vdavinci->vdev.domain == NULL) {
        ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
        return false;
    }
    vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;
    ka_task_down_read(&vm_dom->sem);
    /* unlock to support concurrent read */
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);

    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    if (dev_dom != NULL && dev_dom->status == DOMAIN_MAP_STATUS_READY) {
        active = true;
    }

    ka_task_up_read(&vm_dom->sem);
    return active;
}

/**
 * hw_dvt_hypervisor_dma_map_guest_page_batch
 * - setup dma map for guest page when dma pool is active
 * @__vdavinci: a vdavinci
 * @gfn: guest pfn array
 * @dma_addr: retrieve dma addr array
 * @count: array size
 *
 * Returns:
 * 0 on success, negative error code if failed.
 */
int hw_dvt_hypervisor_dma_map_guest_page_batch(void *__vdavinci, unsigned long *gfn, unsigned long *dma_addr,
                                               unsigned long count)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;

    if (vdavinci == NULL || gfn == NULL || dma_addr == NULL || count == 0) {
        return -EINVAL;
    }

    return g_hw_kvmdt_ops.dma_get_iova_batch(vdavinci, gfn, dma_addr, count);
}

/**
 * hw_dvt_hypervisor_dma_unmap_guest_page_batch
 * - cancel dma map for guest page when dma pool is active
 * @__vdavinci: a vdavinci
 * @gfn: guest pfn array
 * @dma_addr: retrieve dma addr array
 * @count: array size
 */
void hw_dvt_hypervisor_dma_unmap_guest_page_batch(void *__vdavinci, unsigned long *gfn, unsigned long *dma_addr,
                                                  unsigned long count)
{}

/**
 * hw_dvt_hypervisor_is_valid_gfn - check if a visible gfn
 * @vdavinci: a vdavinci
 * @gfn: guest PFN
 *
 * Returns:
 * true on valid gfn, false on not.
 */
bool hw_dvt_hypervisor_is_valid_gfn(void *__vdavinci, unsigned long gfn)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)__vdavinci;
    bool ret = false;

    if (vdavinci == NULL) {
        return false;
    }
    if (g_hw_kvmdt_ops.is_valid_gfn == NULL) {
        return true;
    }
    ka_task_rcu_read_lock();
    ret = g_hw_kvmdt_ops.is_valid_gfn(vdavinci, gfn);
    ka_task_rcu_read_unlock();
    return ret;
}

int hw_dvt_hypervisor_mmio_get(void **dst, int *size, void *__vdavinci, int bar)
{
    if (g_hw_kvmdt_ops.mmio_get == NULL || __vdavinci == NULL || dst == NULL || size == NULL) {
        return -EINVAL;
    }

    return g_hw_kvmdt_ops.mmio_get(dst, size, __vdavinci, bar);
}

bool hw_dvt_hypervisor_is_vm_pfn_valid(ka_device_t *dev, unsigned long pfn, unsigned long size)
{
    if (ka_unlikely(dev == NULL || size == 0)) {
        return false;
    }

    return g_hw_kvmdt_ops.is_pfn_valid(dev, pfn, size);
}
